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
#include "meltjob.h"
#include "kdenlivesettings.h"
#include "doc/kdenlivedoc.h"
#include "bin/projectclip.h"
#include "project/clipstabilize.h"
#include "ui_scenecutdialog_ui.h"

#include <QDebug>
#include <QUrl>
#include <klocalizedstring.h>

#include <mlt++/Mlt.h>

// static 
QList <ProjectClip *> FilterJob::filterClips(QList <ProjectClip *>clips, const QStringList &params)
{
    QString condition;
    if (params.count() > 3) {
        // there is a condition for this job, for example operate only on vcodec=mpeg1video
        condition = params.at(3);
    }
    QList <ProjectClip *> result;
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

QMap <ProjectClip *, AbstractClipJob *> FilterJob::prepareJob(QList <ProjectClip*> clips, QStringList parameters)
{
    QMap <ProjectClip *, AbstractClipJob *> jobs;
    QStringList sources;
    for (int i = 0; i < clips.count(); i++) {
        sources << clips.at(i)->url().toLocalFile();
    }
    QString filterName = parameters.first();
    if (filterName == "framebuffer") {
        QMap <QString, QString> producerParams = QMap <QString, QString> ();
        QMap <QString, QString> filterParams = QMap <QString, QString> ();
        QMap <QString, QString> consumerParams = QMap <QString, QString> ();
        QMap <QString, QString> extraParams = QMap <QString, QString> ();
        producerParams.insert("in", "0");
        producerParams.insert("out", "-1");
        extraParams.insert("projecttreefilter", "1");
        extraParams.insert("producer_profile", "1");
        for (int i = 0; i < clips.count(); i++) {
            QString prodstring = QString("framebuffer:" + sources.at(i) + "?-1");
            producerParams.insert("producer", prodstring);
            consumerParams.insert("consumer", "xml:" + sources.at(i) + ".mlt");
            ProjectClip *clip = clips.at(i);
            MeltJob *job = new MeltJob(clip->clipType(), clip->clipId(), producerParams, filterParams, consumerParams, extraParams);
            job->description = i18n("Reverse clip");
            job->setAddClipToProject(true);
            jobs.insert(clip, job);
        }
        return jobs;
    }
    if (filterName == "motion_est") {
        // Show config dialog
        QPointer<QDialog> d = new QDialog(QApplication::activeWindow());
        Ui::SceneCutDialog_UI ui;
        ui.setupUi(d);
        // Set  up categories
        for (int i = 0; i < 5; ++i) {
            ui.marker_type->insertItem(i, i18n("Category %1", i));
            ui.marker_type->setItemData(i, CommentedTime::markerColor(i), Qt::DecorationRole);
        }
        ui.marker_type->setCurrentIndex(KdenliveSettings::default_marker_type());
        if (d->exec() != QDialog::Accepted) {
            delete d;
            return jobs;
        }
        // Autosplit filter
        QMap <QString, QString> producerParams = QMap <QString, QString> ();
        QMap <QString, QString> filterParams = QMap <QString, QString> ();
        QMap <QString, QString> consumerParams = QMap <QString, QString> ();
        
        // Producer params
        // None

        // Filter params, use a smaller region of the image to speed up operation
        // In fact, it's faster to rescale whole image than using part of it (bounding=\"25%x25%:15%x15\")
        filterParams.insert("filter", filterName);
        filterParams.insert("shot_change_list", "0");
        filterParams.insert("denoise", "0");
        
        // Consumer
        consumerParams.insert("consumer", "null");
        consumerParams.insert("all", "1");
        consumerParams.insert("terminate_on_pause", "1");
        consumerParams.insert("real_time", "-1");
        // We just want to find scene change, set all mathods to the fastests
        consumerParams.insert("rescale", "nearest");
        consumerParams.insert("deinterlace_method", "onefield");
        consumerParams.insert("top_field_first", "-1");
        
        // Extra
        QMap <QString, QString> extraParams;
        extraParams.insert("key", "shot_change_list");
        extraParams.insert("projecttreefilter", "1");
        QString keyword("%count");
        extraParams.insert("resultmessage", i18n("Found %1 scenes.", keyword));
        extraParams.insert("resize_profile", "160");
        if (ui.store_data->isChecked()) {
            // We want to save result as clip metadata
            extraParams.insert("storedata", "1");
        }
        if (ui.zone_only->isChecked()) {
            // We want to analyze only clip zone
            extraParams.insert("zoneonly", "1");
        }
        if (ui.add_markers->isChecked()) {
            // We want to create markers
            extraParams.insert("addmarkers", QString::number(ui.marker_type->currentIndex()));
        }
        if (ui.cut_scenes->isChecked()) {
            // We want to cut scenes
            extraParams.insert("cutscenes", "1");
        }
        delete d;
        
        for (int i = 0; i < clips.count(); i++) {
            // Set clip specific infos

            // in and out
            int in = 0;
            int out = -1;
            ProjectClip *clip = clips.at(i);
            if (extraParams.contains("zoneonly")) {
                // Analyse clip zone only, remove in / out and replace with zone
                QPoint zone = clip->zone();
                in = zone.x();
                out = zone.y();
            }
            producerParams.insert("in", QString::number(in));
            producerParams.insert("out", QString::number(out));
            producerParams.insert("producer", sources.at(i));
            
            // Destination
            // Since this job is only doing analysis, we have a null consumer and no destination
            MeltJob *job = new MeltJob(clip->clipType(), clip->clipId(), producerParams, filterParams, consumerParams, extraParams);
            job->description = i18n("Auto split");
            jobs.insert(clip, job);
        }
        return jobs;
    }
    if (filterName == "vidstab") {
        // vidstab 
        QPointer<ClipStabilize> d = new ClipStabilize(sources, filterName);
        if (d->exec() == QDialog::Accepted) {
            QMap <QString, QString> producerParams = d->producerParams();
            QMap <QString, QString> filterParams = d->filterParams();
            QMap <QString, QString> consumerParams = d->consumerParams();
            QMap <QString, QString> extraParams;
            extraParams.insert("producer_profile", "1");
            QString destination = d->destination();
            QUrl trffile;
            for (int i = 0; i < clips.count(); i++) {
                // Set clip specific infos

                // in and out
                int in = 0;
                int out = -1;
                ProjectClip *clip = clips.at(i);
                if (extraParams.contains("zoneonly")) {
                    // Analyse clip zone only, remove in / out and replace with zone
                    QPoint zone = clip->zone();
                    in = zone.x();
                    out = zone.y();
                }
                producerParams.insert("in", QString::number(in));
                producerParams.insert("out", QString::number(out));
                producerParams.insert("producer", sources.at(i));

                // Consumer
                QString consumerName = consumerParams.value("consumer");
                if (clips.count() == 1) {
                    // We only have one clip, destination points to the final url
                    consumerParams.insert("consumer", consumerName + ':' + destination);
                    trffile = QUrl::fromLocalFile(destination + ".trf");
                } else {
                    // Filter several clips, destination points to a folder
                    QString mltfile = destination + clip->url().fileName() + ".mlt";
                    consumerParams.insert("consumer", consumerName + ':' + mltfile);
                    trffile = QUrl::fromLocalFile(mltfile + ".trf");
                }
                // Append a 'filename' parameter for saving vidstab data
                filterParams.insert("filename", trffile.path());
                MeltJob *job = new MeltJob(clip->clipType(), clip->clipId(), producerParams, filterParams, consumerParams, extraParams);
                job->setAddClipToProject(d->autoAddClip());
                job->description = d->desc();
                jobs.insert(clip, job);
            }
        }
        delete d;
        return jobs;
    }
    return jobs;
}






