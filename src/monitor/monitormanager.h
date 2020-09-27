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

#include <QDir>
#include <QMutex>
#include <QTimer>

class KdenliveDoc;
class KDualAction;

namespace Mlt {
class Profile;
}

class MonitorManager : public QObject
{
    Q_OBJECT

public:
    explicit MonitorManager(QObject *parent = nullptr);
    void initMonitors(Monitor *clipMonitor, Monitor *projectMonitor);
    void appendMonitor(AbstractMonitor *monitor);
    void removeMonitor(AbstractMonitor *monitor);
    Timecode timecode() const;
    void resetProfiles();
    /** @brief delete and rebuild consumer, for example when external display is switched */
    void resetConsumers(bool fullReset);
    void stopActiveMonitor();
    void pauseActiveMonitor();
    AbstractMonitor *activeMonitor();
    /** Searches for a monitor with the given name.
    @return nullptr, if no monitor could be found, or the monitor otherwise.
    */
    AbstractMonitor *monitor(Kdenlive::MonitorId monitorName);
    bool isActive(Kdenlive::MonitorId id) const;
    void updateScopeSource();
    void clearScopeSource();
    /** @brief Change an MLT consumer property for both monitors. */
    void setConsumerProperty(const QString &name, const QString &value);
    /** @brief Return a mainwindow action from its id name. */
    QAction *getAction(const QString &name);
    Monitor *clipMonitor();
    Monitor *projectMonitor();
    void lockMonitor(Kdenlive::MonitorId name, bool);
    void refreshIcons();
    void resetDisplay();
    QDir getCacheFolder(CacheType type);
    /** @brief Returns true if multitrack view is enabled in project monitor. */
    bool isMultiTrack() const;
    /** @brief Returns true if the project monitor is visible (and not tabbed under another dock. */
    bool projectMonitorVisible() const;
    QTimer refreshTimer;
    static const double speedArray[5];

public slots:

    /** @brief Activates a monitor.
     * @param name name of the monitor to activate */
    bool activateMonitor(Kdenlive::MonitorId);
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
    void focusProjectMonitor();
    void refreshProjectMonitor();
    /** @brief Refresh project monitor if the timeline cursor is inside the range. */
    void refreshProjectRange(QSize range);
    void refreshClipMonitor();

    /** @brief Switch current monitor to fullscreen. */
    void slotSwitchFullscreen();

    /** @brief Switches between project and clip monitor.
     * @ref activateMonitor
     * @param activateClip whether to activate the clip monitor */
    void slotSwitchMonitors(bool activateClip);
    void slotUpdateAudioMonitoring();
    /** @brief Export the current monitor's frame to image file. */
    void slotExtractCurrentFrame();
    /** @brief Export the current monitor's frame to image file and add it to the current project */
    void slotExtractCurrentFrameToProject();
    /** @brief Refresh monitor background color */
    void updateBgColor();

private slots:
    /** @brief Set MLT's consumer deinterlace method */
    void slotSetDeinterlacer(int ix);
    /** @brief Set MLT's consumer interpolation method */
    void slotSetInterpolation(int ix);
    /** @brief Switch muting on/off */
    void slotMuteCurrentMonitor(bool active);
    /** @brief Zoom in active monitor */
    void slotZoomIn();
    /** @brief Zoom out active monitor */
    void slotZoomOut();
    /** @brief Trigger refresh of both monitors */
    void forceProjectMonitorRefresh();

private:
    /** @brief Make sure 2 monitors cannot be activated simultaneously*/
    QMutex m_refreshMutex;
    QMutex m_switchMutex;
    /** @brief Sets up all the actions and attaches them to the collection of MainWindow. */
    void setupActions();
    Monitor *m_clipMonitor{nullptr};
    Monitor *m_projectMonitor{nullptr};
    AbstractMonitor *m_activeMonitor{nullptr};
    QList<AbstractMonitor *> m_monitorsList;
    KDualAction *m_muteAction;
    QAction *m_multiTrack{nullptr};

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
    /** @brief info is available for audio spectrum widget */
    void frameDisplayed(const SharedFrame &);
    /** @brief Triggered when the project monitor is paused (used to reset stored audiomixer data */
    void cleanMixer();
    /** @brief Update monitor preview resolution */
    void updatePreviewScaling();
    /** @brief monitor scaling was changed, update select action */
    void scalingChanged();
};

#endif
