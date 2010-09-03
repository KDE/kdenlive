/***************************************************************************
 *   Copyright (C) 2007 by Jean-Baptiste Mardelle (jb@kdenlive.org)        *
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


#ifndef MONITORMANAGER_H
#define MONITORMANAGER_H

#include "monitor.h"
#include "timecode.h"

class Monitor;

class MonitorManager : public QObject
{
    Q_OBJECT

public:
    MonitorManager(QWidget *parent = 0);
    void initMonitors(Monitor *clipMonitor, Monitor *projectMonitor);
    Timecode timecode();
    void resetProfiles(Timecode tc);
    void stopActiveMonitor();

public slots:

    /** @brief Activates a monitor.
     * @param name name of the monitor to activate */
    void activateMonitor(QString name = QString());
    void slotPlay();
    void slotPlayZone();
    void slotLoopZone();
    void slotRewind(double speed = 0);
    void slotForward(double speed = 0);
    void slotRewindOneFrame();
    void slotForwardOneFrame();
    void slotRewindOneSecond();
    void slotForwardOneSecond();
    void slotStart();
    void slotEnd();
    void slotResetProfiles();

    /** @brief Switches between project and clip monitor.
     * @ref activateMonitor
     * @param activateClip whether to activate the clip monitor */
    void slotSwitchMonitors(bool activateClip);

private:
    Monitor *m_clipMonitor;
    Monitor *m_projectMonitor;
    QString m_activeMonitor;
    Timecode m_timecode;
    bool m_blocked;

signals:
    /** @brief Emitted when the active monitor changes */
    void raiseClipMonitor(bool);
    /** @brief When the monitor changed, update the visible color scopes */
    void checkColorScopes();

};

#endif
