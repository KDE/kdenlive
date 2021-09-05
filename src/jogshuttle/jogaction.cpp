/*
 *   SPDX-FileCopyrightText: 2010 Pascal Fleury (fleury@users.sourceforge.net)    *
 *                                                                         *
SPDX-License-Identifier: LicenseRef-KDE-Accepted-GPL
 ***************************************************************************/

#include "jogaction.h"
#include "core.h"
#include "monitor/monitormanager.h"

#include "kdenlive_debug.h"
#include <cstdio>
#include <cstdlib>
#include <klocalizedstring.h>
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
            emit rewind(-SPEEDS[magnitude]);
        } else if (shuttle_pos == 0) {
            ////qCDebug(KDENLIVE_LOG) << "Shuttle pos0 action: " << m_actionMap[0];
            emit action(m_actionMap[0]);
        } else if (shuttle_pos > 0) {
            emit forward(SPEEDS[magnitude]);
        }
    }
}

void JogShuttleAction::slotButton(int button_id)
{
    if (button_id >= m_actionMap.size() || m_actionMap[button_id].isEmpty()) {
        // TODO(fleury): Should this go to the status bar to inform the user ?
        // qCDebug(KDENLIVE_LOG) << "No action applied for button: " << button_id;
        return;
    }
    ////qCDebug(KDENLIVE_LOG) << "Shuttle button =" << button_id << ": action=" << m_actionMap[button_id];
    emit action(m_actionMap[button_id]);
}
