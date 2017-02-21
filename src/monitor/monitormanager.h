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
#include "recmonitor.h"

#include "timecode.h"
#include <QDir>

class KdenliveDoc;
class BinController;
class KDualAction;

namespace Mlt
{
class Profile;
}

class MonitorManager : public QObject
{
    Q_OBJECT

public:
    explicit MonitorManager(QObject *parent = nullptr);
    void initMonitors(Monitor *clipMonitor, Monitor *projectMonitor, RecMonitor *recMonitor);
    void appendMonitor(AbstractMonitor *monitor);
    void removeMonitor(AbstractMonitor *monitor);
    Timecode timecode() const;
    void resetProfiles(const MltVideoProfile &profile, const Timecode &tc);
    void stopActiveMonitor();
    void pauseActiveMonitor();
    AbstractRender *activeRenderer();
    /** Searches for a monitor with the given name.
    @return nullptr, if no monitor could be found, or the monitor otherwise.
    */
    AbstractMonitor *monitor(Kdenlive::MonitorId monitorName);
    void updateScopeSource();
    void clearScopeSource();
    /** @brief Returns current project's folder. */
    QString getProjectFolder() const;
    /** @brief Sets current document for later reference. */
    void setDocument(KdenliveDoc *doc);
    /** @brief Change an MLT consumer property for both monitors. */
    void setConsumerProperty(const QString &name, const QString &value);
    BinController *binController();
    Mlt::Profile *profile();
    /** @brief Return a mainwindow action from its id name. */
    QAction *getAction(const QString &name);
    Monitor *clipMonitor();
    Monitor *projectMonitor();
    void lockMonitor(Kdenlive::MonitorId name, bool);
    void refreshIcons();
    void resetDisplay();
    QDir getCacheFolder(CacheType type);

public slots:

    /** @brief Activates a monitor.
     * @param name name of the monitor to activate */
    bool activateMonitor(Kdenlive::MonitorId, bool forceRefresh = false);
    bool isActive(Kdenlive::MonitorId id) const;
    void slotPlay();
    void slotPause();
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
    void slotZoneStart();
    void slotZoneEnd();
    void slotSetInPoint();
    void slotSetOutPoint();

    /** @brief Switch current monitor to fullscreen. */
    void slotSwitchFullscreen();

    /** @brief Switches between project and clip monitor.
     * @ref activateMonitor
     * @param activateClip whether to activate the clip monitor */
    void slotSwitchMonitors(bool activateClip);
    void slotUpdateAudioMonitoring();

private slots:
    /** @brief Set MLT's consumer deinterlace method */
    void slotSetDeinterlacer(int ix);
    /** @brief Set MLT's consumer interpolation method */
    void slotSetInterpolation(int ix);
    /** @brief Switch muting on/off */
    void slotMuteCurrentMonitor(bool active);

private:
    /** @brief Make sure 2 monitors cannot be activated simultaneously*/
    QMutex m_refreshMutex;
    QMutex m_switchMutex;
    /** @brief Sets up all the actions and attaches them to the collection of MainWindow. */
    void setupActions();
    KdenliveDoc *m_document;
    Monitor *m_clipMonitor;
    Monitor *m_projectMonitor;
    Timecode m_timecode;
    AbstractMonitor *m_activeMonitor;
    QList<AbstractMonitor *>m_monitorsList;
    KDualAction *m_muteAction;

signals:
    /** @brief When the monitor changed, update the visible color scopes */
    void checkColorScopes();
    /** @brief When the active monitor renderer was deleted, reset color scopes */
    void clearScopes();
    /** @brief Check if we still need to send frame for scopes */
    void checkScopes();
    void addEffect(const QDomElement &);
    /** @brief Monitor activated, refresh overlay options actions */
    void updateOverlayInfos(int, int);
    /** @brief info is available for audio spectum widget */
    void frameDisplayed(const SharedFrame &);
};

#endif
