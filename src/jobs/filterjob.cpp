/***************************************************************************
 *                                                                         *
 *   Copyright (C) 2015 by Jean-Baptiste Mardelle (jb@kdenlive.org)        *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA          *
 ***************************************************************************/

#include "filterjob.h"
#include "bin/model/markerlistmodel.hpp"
#include "bin/projectclip.h"
#include "bin/projectitemmodel.h"
#include "doc/kdenlivedoc.h"
#include "kdenlivesettings.h"
#include "meltjob.h"
#include "project/clipstabilize.h"
#include "project/dialogs/clipspeed.h"
#include "ui_scenecutdialog_ui.h"

#include <KMessageBox>
#include <QUrl>
#include <klocalizedstring.h>
#include <mlt++/Mlt.h>

// static
QList<ProjectClip *> FilterJob::filterClips(const QList<ProjectClip *> &clips, const QStringList &params)
{
    QString condition;
    if (params.count() > 3) {
        // there is a condition for this job, for example operate only on vcodec=mpeg1video
        condition = params.at(3);
    }
    QList<ProjectClip *> result;
    for (int i = 0; i < clips.count(); i++) {
        ProjectClip *clip = clips.at(i);
        ClipType type = clip->clipType();
        if (type != AV && type != Audio && type != Video) {
            // Clip will not be processed by this job
            continue;
        }
        if (!condition.isEmpty() && !clip->matches(condition)) {
            // Clip does not match requested condition, do not process
            continue;
        }
        result << clip;
    }
    return result;
}

QHash<ProjectClip *, AbstractClipJob *> FilterJob::prepareJob(const QList<ProjectClip *> &clips, const QStringList &parameters)
{
    QHash<ProjectClip *, AbstractClipJob *> jobs;
    QStringList sources;
    sources.reserve(clips.count());
    for (int i = 0; i < clips.count(); i++) {
        sources << clips.at(i)->url();
    }
    const QString filterName = parameters.constFirst();
    if (filterName == QLatin1String("timewarp")) {
        QMap<QString, QString> producerParams = QMap<QString, QString>();
        QMap<QString, QString> filterParams = QMap<QString, QString>();
        QMap<QString, QString> consumerParams = QMap<QString, QString>();
        QMap<QString, QString> extraParams = QMap<QString, QString>();
        producerParams.insert(QStringLiteral("in"), QStringLiteral("0"));
        producerParams.insert(QStringLiteral("out"), QStringLiteral("-1"));
        extraParams.insert(QStringLiteral("projecttreefilter"), QStringLiteral("1"));
        // Reverse clip using project profile since playlists can only be included with same fps
        // extraParams.insert(QStringLiteral("producer_profile"), QStringLiteral("1"));
        bool multipleSelection = clips.count() > 1;
        QPointer<ClipSpeed> d = new ClipSpeed(clips.count() == 1 ? QUrl::fromLocalFile(sources.constFirst() + QStringLiteral(".mlt"))
                                                                 : QUrl::fromLocalFile(sources.constFirst()).adjusted(QUrl::RemoveFilename),
                                              multipleSelection, QApplication::activeWindow());
        if (d->exec() == QDialog::Accepted) {
            QString speedString = QStringLiteral("timewarp:%1:").arg(QString::number(d->speed() / 100, 'f'));
            QDir destFolder;
            if (multipleSelection) {
                destFolder = QDir(d->selectedUrl().toLocalFile());
            }
            for (int i = 0; i < clips.count(); i++) {
                QString prodstring = speedString + sources.at(i);
                producerParams.insert(QStringLiteral("producer"), prodstring);
                QString destination;
                if (multipleSelection) {
                    destination = destFolder.absoluteFilePath(QUrl::fromLocalFile(sources.at(i)).fileName() + QStringLiteral(".mlt"));
                } else {
                    destination = d->selectedUrl().toLocalFile();
                }
                if (QFile::exists(destination)) {
                    if (KMessageBox::questionYesNo(QApplication::activeWindow(), i18n("File %1 already exists.\nDo you want to overwrite it?", destination)) !=
                        KMessageBox::Yes) {
                        continue;
                    }
                }
                consumerParams.insert(QStringLiteral("consumer"), QStringLiteral("xml:") + destination);
                ProjectClip *clip = clips.at(i);
                auto *job = new MeltJob(clip->clipType(), clip->AbstractProjectItem::clipId(), producerParams, filterParams, consumerParams, extraParams);
                job->description = i18n("Reverse clip");
                job->setAddClipToProject(1);
                jobs.insert(clip, job);
            }
        }
        delete d;
        return jobs;
    }
    if (filterName == QLatin1String("motion_est")) {
        // Show config dialog
        QPointer<QDialog> d = new QDialog(QApplication::activeWindow());
        Ui::SceneCutDialog_UI ui;
        ui.setupUi(d);
        // Set  up categories
        for (size_t i = 0; i < MarkerListModel::markerTypes.size(); ++i) {
            ui.marker_type->insertItem((int)i, i18n("Category %1", i));
            ui.marker_type->setItemData((int)i, MarkerListModel::markerTypes[i], Qt::DecorationRole);
        }
        ui.marker_type->setCurrentIndex(KdenliveSettings::default_marker_type());
        if (d->exec() != QDialog::Accepted) {
            delete d;
            return jobs;
        }
        // Autosplit filter
        QMap<QString, QString> producerParams = QMap<QString, QString>();
        QMap<QString, QString> filterParams = QMap<QString, QString>();
        QMap<QString, QString> consumerParams = QMap<QString, QString>();

        // Producer params
        // None

        // Filter params, use a smaller region of the image to speed up operation
        // In fact, it's faster to rescale whole image than using part of it (bounding=\"25%x25%:15%x15\")
        filterParams.insert(QStringLiteral("filter"), filterName);
        filterParams.insert(QStringLiteral("shot_change_list"), QStringLiteral("0"));
        filterParams.insert(QStringLiteral("denoise"), QStringLiteral("0"));

        // Consumer
        consumerParams.insert(QStringLiteral("consumer"), QStringLiteral("null"));
        consumerParams.insert(QStringLiteral("all"), QStringLiteral("1"));
        consumerParams.insert(QStringLiteral("terminate_on_pause"), QStringLiteral("1"));
        consumerParams.insert(QStringLiteral("real_time"), QStringLiteral("-1"));
        // We just want to find scene change, set all methods to the fastest
        consumerParams.insert(QStringLiteral("rescale"), QStringLiteral("nearest"));
        consumerParams.insert(QStringLiteral("deinterlace_method"), QStringLiteral("onefield"));
        consumerParams.insert(QStringLiteral("top_field_first"), QStringLiteral("-1"));

        // Extra
        QMap<QString, QString> extraParams;
        extraParams.insert(QStringLiteral("key"), QStringLiteral("shot_change_list"));
        extraParams.insert(QStringLiteral("projecttreefilter"), QStringLiteral("1"));
        QString keyword(QStringLiteral("%count"));
        extraParams.insert(QStringLiteral("resultmessage"), i18n("Found %1 scenes.", keyword));
        extraParams.insert(QStringLiteral("resize_profile"), QStringLiteral("160"));
        if (ui.store_data->isChecked()) {
            // We want to save result as clip metadata
            extraParams.insert(QStringLiteral("storedata"), QStringLiteral("1"));
        }
        if (ui.zone_only->isChecked()) {
            // We want to analyze only clip zone
            extraParams.insert(QStringLiteral("zoneonly"), QStringLiteral("1"));
        }
        if (ui.add_markers->isChecked()) {
            // We want to create markers
            extraParams.insert(QStringLiteral("addmarkers"), QString::number(ui.marker_type->currentIndex()));
            extraParams.insert(QStringLiteral("label"), i18n("Scene "));
        }
        if (ui.cut_scenes->isChecked()) {
            // We want to cut scenes
            extraParams.insert(QStringLiteral("cutscenes"), QStringLiteral("1"));
        }
        delete d;

        for (int i = 0; i < clips.count(); i++) {
            // Set clip specific infos

            // in and out
            int in = 0;
            int out = -1;
            ProjectClip *clip = clips.at(i);
            if (extraParams.contains(QStringLiteral("zoneonly"))) {
                // Analyse clip zone only, remove in / out and replace with zone
                QPoint zone = clip->zone();
                in = zone.x();
                out = zone.y();
            }
            producerParams.insert(QStringLiteral("in"), QString::number(in));
            producerParams.insert(QStringLiteral("out"), QString::number(out));
            producerParams.insert(QStringLiteral("producer"), sources.at(i));

            // Destination
            // Since this job is only doing analysis, we have a null consumer and no destination
            auto *job = new MeltJob(clip->clipType(), clip->AbstractProjectItem::clipId(), producerParams, filterParams, consumerParams, extraParams);
            job->description = i18n("Auto split");
            jobs.insert(clip, job);
        }
        return jobs;
    }
    if (filterName == QLatin1String("vidstab") || filterName == QLatin1String("videostab2") || filterName == QLatin1String("videostab")) {
        // vidstab
        int out = 100000;
        if (clips.count() == 1) {
            out = clips.constFirst()->duration().frames(KdenliveSettings::project_fps());
        }
        QPointer<ClipStabilize> d = new ClipStabilize(sources, filterName);
        if (d->exec() == QDialog::Accepted) {
            QMap<QString, QString> producerParams = d->producerParams();
            QMap<QString, QString> filterParams = d->filterParams();
            QMap<QString, QString> consumerParams = d->consumerParams();
            QMap<QString, QString> extraParams;
            extraParams.insert(QStringLiteral("producer_profile"), QStringLiteral("1"));
            QString destination = d->destination();
            QUrl trffile;
            for (int i = 0; i < clips.count(); i++) {
                // Set clip specific infos

                // in and out
                int clip_in = 0;
                int clip_out = -1;
                ProjectClip *clip = clips.at(i);
                if (extraParams.contains(QStringLiteral("zoneonly"))) {
                    // Analyse clip zone only, remove in / out and replace with zone
                    QPoint zone = clip->zone();
                    clip_in = zone.x();
                    clip_out = zone.y();
                }
                producerParams.insert(QStringLiteral("in"), QString::number(clip_in));
                producerParams.insert(QStringLiteral("out"), QString::number(clip_out));
                producerParams.insert(QStringLiteral("producer"), sources.at(i));

                // Consumer
                QString consumerName = d->consumerParams().value(QStringLiteral("consumer"));
                if (clips.count() == 1) {
                    // We only have one clip, destination points to the final url
                    consumerParams.insert(QStringLiteral("consumer"), consumerName + QLatin1Char(':') + destination);
                    trffile = QUrl::fromLocalFile(destination + QStringLiteral(".trf"));
                } else {
                    // Filter several clips, destination points to a folder
                    QString mltfile = destination + QFileInfo(clip->url()).fileName() + QStringLiteral(".mlt");
                    consumerParams.insert(QStringLiteral("consumer"), consumerName + QLatin1Char(':') + mltfile);
                    trffile = QUrl::fromLocalFile(mltfile + QStringLiteral(".trf"));
                }
                consumerParams.insert(QStringLiteral("real_time"), QStringLiteral("-1"));
                // Append a 'filename' parameter for saving vidstab data
                filterParams.insert(QStringLiteral("filename"), trffile.toLocalFile());
                auto *job = new MeltJob(clip->clipType(), clip->AbstractProjectItem::clipId(), producerParams, filterParams, consumerParams, extraParams);
                job->setAddClipToProject(d->autoAddClip() ? clip->parent()->AbstractProjectItem::clipId().toInt() : -100);
                job->description = d->desc();
                jobs.insert(clip, job);
            }
        }
        delete d;
        return jobs;
    }
    return jobs;
}
