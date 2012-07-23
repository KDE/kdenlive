/***************************************************************************
 *   Copyright (C) 2010 by Pascal Fleury (fleury@users.sourceforge.net)    *
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

#include "jogaction.h"

#include <stdio.h>
#include <stdlib.h>
#include <klocalizedstring.h>

// TODO(fleury): this should probably be a user configuration parameter (at least the max speed).
const double SPEEDS[] = {0.0, 0.5, 1.0, 2.0, 4.0, 8.0, 16.0, 32.0};
const size_t SPEEDS_SIZE = sizeof(SPEEDS) / sizeof(double);

JogShuttleAction::JogShuttleAction (const JogShuttle* jogShuttle, const QStringList& actionMap, QObject * parent)
        : QObject(parent), m_jogShuttle(jogShuttle), m_actionMap(actionMap)
{
    // Add action map 0 used for stopping the monitor when the shuttle is in neutral position.
    if (m_actionMap.size() == 0)
      m_actionMap.append("monitor_pause");
    
    connect(m_jogShuttle, SIGNAL( jogBack() ), this, SLOT( slotJogBack() ));
    connect(m_jogShuttle, SIGNAL( jogForward() ), this, SLOT( slotJogForward() ));
    connect(m_jogShuttle, SIGNAL( shuttlePos ( int ) ), this, SLOT( slotShuttlePos ( int ) ));
    connect(m_jogShuttle, SIGNAL( button ( int ) ), this, SLOT( slotButton ( int ) ));
    //for (int i = 0; i < actionMap.size(); i++) fprintf(stderr, "button #%d -> action '%s'\n", i, actionMap[i].toAscii().constData());  //DBG
}

JogShuttleAction::~JogShuttleAction()
{
    disconnect(m_jogShuttle, SIGNAL( jogBack() ), this, SLOT( slotJogBack() ));
    disconnect(m_jogShuttle, SIGNAL( jogForward() ), this, SLOT( slotJogForward() ));
    disconnect(m_jogShuttle, SIGNAL( shuttlePos ( int ) ), this, SLOT( slotShuttlePos ( int ) ));
    disconnect(m_jogShuttle, SIGNAL( button ( int ) ), this, SLOT( slotButton ( int ) ));
}

void JogShuttleAction::slotJogBack()
{
    emit rewindOneFrame();
}

void JogShuttleAction::slotJogForward()
{
    emit forwardOneFrame();
}

void JogShuttleAction::slotShuttlePos(int shuttle_pos)
{
    size_t magnitude = abs(shuttle_pos);
    if (magnitude < SPEEDS_SIZE) {
        if (shuttle_pos < 0)
            emit rewind(-SPEEDS[magnitude]);
        if (shuttle_pos == 0)
            emit action(m_actionMap[0]);
        if (shuttle_pos > 0)
            emit forward(SPEEDS[magnitude]);
    }
}

void JogShuttleAction::slotButton(int button_id)
{
    if (button_id >= m_actionMap.size() || m_actionMap[button_id].isEmpty()) {
        // TODO(fleury): Shoudl this go to the status bar to inform the user ?
        fprintf(stderr, "Button %d has no action\n", button_id);
        return;
    }
    //fprintf(stderr, "Button #%d maps to action '%s'\n", button_id, m_actionMap[button_id].toAscii().constData()); //DBG
    emit action(m_actionMap[button_id]);
}
