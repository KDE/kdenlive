/*
    SPDX-FileCopyrightText: 2011 Jean-Baptiste Mardelle (jb@kdenlive.org)

SPDX-License-Identifier: LicenseRef-KDE-Accepted-GPL
*/

#include "abstractmonitor.h"
#include "monitormanager.h"

#include "kdenlivesettings.h"

AbstractMonitor::AbstractMonitor(Kdenlive::MonitorId id, MonitorManager *manager, QWidget *parent)
    : QWidget(parent)
    , m_id(id)
    , m_monitorManager(manager)
{
}

AbstractMonitor::~AbstractMonitor() = default;

bool AbstractMonitor::isActive() const
{
    return m_monitorManager->isActive(m_id);
}

bool AbstractMonitor::slotActivateMonitor()
{
    return m_monitorManager->activateMonitor(m_id);
}
