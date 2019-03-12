/*
Copyright (C) 2018  Jean-Baptiste Mardelle <jb@kdenlive.org>
This file is part of Kdenlive. See www.kdenlive.org.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of
the License or (at your option) version 3 or any later version
accepted by the membership of KDE e.V. (or its successor approved
by the membership of KDE e.V.), which shall act as a proxy
defined in Section 14 of version 3 of the license.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "keyframemonitorhelper.hpp"
#include "assets/model/assetparametermodel.hpp"
#include "monitor/monitor.h"

#include <QSize>
#include <utility>
KeyframeMonitorHelper::KeyframeMonitorHelper(Monitor *monitor, std::shared_ptr<AssetParameterModel> model, const QPersistentModelIndex &index, QObject *parent)
    : QObject(parent)
    , m_monitor(monitor)
    , m_model(std::move(model))
    , m_active(false)
{
    m_indexes << index;
}

bool KeyframeMonitorHelper::connectMonitor(bool activate)
{
    if (activate == m_active) {
        return false;
    }
    m_active = activate;
    if (activate) {
        connect(m_monitor, &Monitor::effectPointsChanged, this, &KeyframeMonitorHelper::slotUpdateFromMonitorData, Qt::UniqueConnection);
    } else {
        disconnect(m_monitor, &Monitor::effectPointsChanged, this, &KeyframeMonitorHelper::slotUpdateFromMonitorData);
    }
    return m_active;
}

void KeyframeMonitorHelper::addIndex(const QPersistentModelIndex &index)
{
    m_indexes << index;
}
