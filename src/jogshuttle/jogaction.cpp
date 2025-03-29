/*
    SPDX-FileCopyrightText: 2010 Pascal Fleury <fleury@users.sourceforge.net>

SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "jogaction.h"
#include "core.h"
#include "monitor/monitormanager.h"

#include "kdenlive_debug.h"
#include <KLocalizedString>
#include <cstdio>
#include <cstdlib>
#include <utility>
// TODO(fleury): this should probably be a user configuration parameter (at least the max speed).
// const double SPEEDS[] = {0.0, 0.5, 1.0, 2.0, 4.0, 8.0, 16.0, 32.0};
const double SPEEDS[] = {0.0, 1.0, 2.0, 4.0, 5.0, 8.0, 16.0, 60.0};
const size_t SPEEDS_SIZE = sizeof(SPEEDS) / sizeof(double);

JogShuttleAction::JogShuttleAction(const JogShuttle *jogShuttle, QStringList actionMap, QObject *parent)
    : QObject(parent)
    , m_jogShuttle(jogShuttle)
    , m_actionMap(std::move(actionMap))
{
    // Add action map 0 used for stopping the monitor when the shuttle is in neutral position.
    if (m_actionMap.isEmpty()) {
        m_actionMap.append(QStringLiteral("monitor_pause"));
    }

    connect(m_jogShuttle, &JogShuttle::jogBack, pCore->monitorManager(), &MonitorManager::slotRewindOneFrame);
    connect(m_jogShuttle, &JogShuttle::jogForward, pCore->monitorManager(), &MonitorManager::slotForwardOneFrame);
    connect(m_jogShuttle, &JogShuttle::shuttlePos, this, &JogShuttleAction::slotShuttlePos);
    connect(m_jogShuttle, &JogShuttle::button, this, &JogShuttleAction::slotButton);

    connect(this, &JogShuttleAction::rewind, pCore->monitorManager(), &MonitorManager::slotRewind);
    connect(this, &JogShuttleAction::forward, pCore->monitorManager(), &MonitorManager::slotForward);
}

JogShuttleAction::~JogShuttleAction() = default;

void JogShuttleAction::slotShuttlePos(int shuttle_pos)
{
    size_t magnitude = size_t(abs(shuttle_pos));
    if (magnitude < SPEEDS_SIZE) {
        if (shuttle_pos < 0) {
            Q_EMIT rewind(-SPEEDS[magnitude]);
        } else if (shuttle_pos == 0) {
            Q_EMIT action(m_actionMap[0]);
        } else if (shuttle_pos > 0) {
            Q_EMIT forward(SPEEDS[magnitude]);
        }
    }
}

void JogShuttleAction::slotButton(int button_id)
{
    if (button_id >= m_actionMap.size() || m_actionMap[button_id].isEmpty()) {
        pCore->displayMessage(i18nc("%1 button id", "No action applied for jog shutte button %1", button_id), InformationMessage);
        qCInfo(KDENLIVE_LOG) << "No action applied for jog shutte button: " << button_id;
        return;
    }
    Q_EMIT action(m_actionMap[button_id]);
}
