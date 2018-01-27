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
#include <klocalizedstring.h>

#include <mlt++/Mlt.h>

SceneSplitJob::SceneSplitJob(const QString &binId, bool subClips, int markersType)
    : MeltJob(binId, STABILIZEJOB, true, -1, -1)
    , m_subClips(subClips)
    , m_markersType(markersType)
{
}

const QString SceneSplitJob::getDescription() const
{
    return i18n("Scene split");
}
void SceneSplitJob::configureConsumer()
{
    m_consumer.reset(new Mlt::Consumer(m_profile, "null"));
    m_consumer->set("all", 1);
    m_consumer->set("terminate_on_pause", 1);
    m_consumer->set("real_time", -KdenliveSettings::mltthreads());
    // We just want to find scene change, set all mathods to the fastests
    m_consumer->set("rescale", "nearest");
    m_consumer->set("deinterlace_method", "onefield");
    m_consumer->set("top_field_first", -1);
}

void SceneSplitJob::configureFilter()
{

    m_filter.reset(new Mlt::Filter(m_profile, "motion_est"));
    if ((m_filter == nullptr) || !m_filter->is_valid()) {
        m_errorMessage.append(i18n("Cannot create filter motion_est. Cannot split scenes"));
        return;
    }

    m_filter->set("shot_change_list", 0);
    m_filter->set("denoise", 0);
}

void SceneSplitJob::configureProfile()
{

    m_profile.set_height(160);
    m_profile.set_width(m_profile.height() * m_profile.sar());
}

// static
int SceneSplitJob::prepareJob(std::shared_ptr<JobManager> ptr, const std::vector<QString> &binIds, int parentId, QString undoString)
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

    return ptr->startJob_noprepare<SceneSplitJob>(binIds, parentId, std::move(undoString), subclips, markersType);
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
    qDebug() << "RESULT of the SCENESPLIT filter:" << result;

    // TODO refac: reimplement add markers and subclips
    return true;
}
