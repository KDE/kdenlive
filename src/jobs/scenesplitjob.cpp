/***************************************************************************
 *   Copyright (C) 2011 by Jean-Baptiste Mardelle (jb@kdenlive.org)        *
 *   Copyright (C) 2017 by Nicolas Carion                                  *
 *                                                                         *
 *   This file is part of Kdenlive. See www.kdenlive.org.                  *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) version 3 or any later version accepted by the       *
 *   membership of KDE e.V. (or its successor approved  by the membership  *
 *   of KDE e.V.), which shall act as a proxy defined in Section 14 of     *
 *   version 3 of the license.                                             *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>. *
 ***************************************************************************/

#include "scenesplitjob.hpp"
#include "bin/clipcreator.hpp"
#include "bin/model/markerlistmodel.hpp"
#include "bin/projectclip.h"
#include "bin/projectfolder.h"
#include "bin/projectitemmodel.h"
#include "core.h"
#include "jobmanager.h"
#include "kdenlivesettings.h"
#include "project/clipstabilize.h"
#include "ui_scenecutdialog_ui.h"

#include <QScopedPointer>

#include <mlt++/Mlt.h>

SceneSplitJob::SceneSplitJob(const QString &binId, bool subClips, int markersType, int minInterval)
    : MeltJob(binId, STABILIZEJOB, true, -1, -1)
    , m_subClips(subClips)
    , m_markersType(markersType)
    , m_minInterval(minInterval)
{
}

const QString SceneSplitJob::getDescription() const
{
    return i18n("Scene split");
}
void SceneSplitJob::configureConsumer()
{
    m_consumer = std::make_unique<Mlt::Consumer>(*m_profile.get(), "null");
    m_consumer->set("all", 1);
    m_consumer->set("terminate_on_pause", 1);
    m_consumer->set("real_time", -1);
    // We just want to find scene change, set all methods to the fastests
    m_consumer->set("rescale", "nearest");
    m_consumer->set("deinterlace_method", "onefield");
    m_consumer->set("top_field_first", -1);
}

void SceneSplitJob::configureFilter()
{
    m_filter = std::make_unique<Mlt::Filter>(*m_profile.get(), "motion_est");
    if ((m_filter == nullptr) || !m_filter->is_valid()) {
        m_errorMessage.append(i18n("Cannot create filter motion_est. Cannot split scenes"));
        return;
    }

    m_filter->set("shot_change_list", 0);
    m_filter->set("denoise", 0);
    m_filter->set_in_and_out(0, length - 1);
}

void SceneSplitJob::configureProfile()
{
    m_profile->set_height(160);
    m_profile->set_width(m_profile->height() * m_profile->sar());
}

// static
int SceneSplitJob::prepareJob(const std::shared_ptr<JobManager> &ptr, const std::vector<QString> &binIds, int parentId, QString undoString)
{
    // Show config dialog
    QScopedPointer<QDialog> d(new QDialog(QApplication::activeWindow()));
    Ui::SceneCutDialog_UI ui;
    ui.setupUi(d.data());
    // Set  up categories
    for (size_t i = 0; i < MarkerListModel::markerTypes.size(); ++i) {
        ui.marker_type->insertItem((int)i, i18n("Category %1", i));
        ui.marker_type->setItemData((int)i, MarkerListModel::markerTypes[i], Qt::DecorationRole);
    }
    ui.marker_type->setCurrentIndex(KdenliveSettings::default_marker_type());
    ui.zone_only->setEnabled(false);  // not implemented
    ui.store_data->setEnabled(false); // not implemented
    if (d->exec() != QDialog::Accepted) {
        return -1;
    }
    int markersType = ui.add_markers->isChecked() ? ui.marker_type->currentIndex() : -1;
    bool subclips = ui.cut_scenes->isChecked();
    int minInterval = ui.minDuration->value();

    return emit ptr->startJob_noprepare<SceneSplitJob>(binIds, parentId, std::move(undoString), subclips, markersType, minInterval);
}

bool SceneSplitJob::commitResult(Fun &undo, Fun &redo)
{
    Q_UNUSED(undo)
    Q_UNUSED(redo)
    Q_ASSERT(!m_resultConsumed);
    if (!m_done) {
        qDebug() << "ERROR: Trying to consume invalid results";
        return false;
    }
    m_resultConsumed = true;
    if (!m_successful) {
        return false;
    }
    QString result = QString::fromLatin1(m_filter->get("shot_change_list"));
    if (result.isEmpty()) {
        m_errorMessage.append(i18n("No data returned from clip analysis"));
        return false;
    }

    auto binClip = pCore->projectItemModel()->getClipByBinID(m_clipId);
    QStringList markerData = result.split(QLatin1Char(';'));
    if (m_markersType >= 0) {
        // Build json data for markers
        QJsonArray list;
        int ix = 1;
        int lastCut = 0;
        for (const QString &marker : qAsConst(markerData)) {
            int pos = marker.section(QLatin1Char('='), 0, 0).toInt();
            if (m_minInterval > 0 && ix > 1 && pos - lastCut < m_minInterval) {
                continue;
            }
            lastCut = pos;
            QJsonObject currentMarker;
            currentMarker.insert(QLatin1String("pos"), QJsonValue(pos));
            currentMarker.insert(QLatin1String("comment"), QJsonValue(i18n("Scene %1", ix)));
            currentMarker.insert(QLatin1String("type"), QJsonValue(m_markersType));
            list.push_back(currentMarker);
            ix++;
        }
        QJsonDocument json(list);
        binClip->getMarkerModel()->importFromJson(QString(json.toJson()), true, undo, redo);
    }
    if (m_subClips) {
        // Create zones
        int ix = 1;
        int lastCut = 0;
        QJsonArray list;
        QJsonDocument json(list);
        for (const QString &marker : qAsConst(markerData)) {
            int pos = marker.section(QLatin1Char('='), 0, 0).toInt();
            if (pos <= lastCut + 1 || pos - lastCut < m_minInterval) {
                continue;
            }
            QJsonObject currentZone;
            currentZone.insert(QLatin1String("name"), QJsonValue(i18n("Scene %1", ix)));
            currentZone.insert(QLatin1String("in"), QJsonValue(lastCut));
            currentZone.insert(QLatin1String("out"), QJsonValue(pos - 1));
            list.push_back(currentZone);
            lastCut = pos;
            ix++;
        }
        if (!json.isEmpty()) {
            pCore->projectItemModel()->loadSubClips(m_clipId, QString(json.toJson()), undo, redo);
        }
    }
    qDebug() << "RESULT of the SCENESPLIT filter:" << result;

    // TODO refac: reimplement add markers and subclips
    return true;
}
