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

#include "monitormanager.h"
#include "core.h"
#include "doc/kdenlivedoc.h"
#include "kdenlivesettings.h"
#include "mainwindow.h"

#include <mlt++/Mlt.h>

#include "klocalizedstring.h"
#include <KDualAction>

#include "kdenlive_debug.h"
#include <QObject>

const double MonitorManager::speedArray[5] = {1.1, 1.5, 3., 5.5, 10.};

MonitorManager::MonitorManager(QObject *parent)
    : QObject(parent)

{
    setupActions();
    refreshTimer.setSingleShot(true);
    refreshTimer.setInterval(200);
    connect(&refreshTimer, &QTimer::timeout, this, &MonitorManager::forceProjectMonitorRefresh);
}

QAction *MonitorManager::getAction(const QString &name)
{
    return pCore->window()->action(name.toUtf8().constData());
}

void MonitorManager::initMonitors(Monitor *clipMonitor, Monitor *projectMonitor)
{
    m_clipMonitor = clipMonitor;
    m_projectMonitor = projectMonitor;
    m_monitorsList.append(clipMonitor);
    m_monitorsList.append(projectMonitor);
}

void MonitorManager::appendMonitor(AbstractMonitor *monitor)
{
    if (!m_monitorsList.contains(monitor)) {
        m_monitorsList.append(monitor);
    }
}

void MonitorManager::removeMonitor(AbstractMonitor *monitor)
{
    m_monitorsList.removeAll(monitor);
}

AbstractMonitor *MonitorManager::monitor(Kdenlive::MonitorId monitorName)
{
    AbstractMonitor *monitor = nullptr;
    for (auto &i : m_monitorsList) {
        if (i->id() == monitorName) {
            monitor = i;
        }
    }
    return monitor;
}

void MonitorManager::setConsumerProperty(const QString &name, const QString &value)
{
    if (m_clipMonitor) {
        m_clipMonitor->setConsumerProperty(name, value);
    }
    if (m_projectMonitor) {
        m_projectMonitor->setConsumerProperty(name, value);
    }
}

void MonitorManager::lockMonitor(Kdenlive::MonitorId name, bool lock)
{
    Q_UNUSED(name)
    if (lock) {
        m_refreshMutex.lock();
    } else {
        m_refreshMutex.unlock();
    }
}

void MonitorManager::focusProjectMonitor()
{
    activateMonitor(Kdenlive::ProjectMonitor);
}

void MonitorManager::refreshProjectRange(QSize range)
{
    if (m_projectMonitor->position() >= range.width() && m_projectMonitor->position() <= range.height()) {
        m_projectMonitor->refreshMonitorIfActive();
    }
}

void MonitorManager::refreshProjectMonitor()
{
    m_projectMonitor->refreshMonitorIfActive();
}

void MonitorManager::refreshClipMonitor()
{
    m_clipMonitor->refreshMonitorIfActive();
}

void MonitorManager::forceProjectMonitorRefresh()
{
    m_projectMonitor->forceMonitorRefresh();
}

bool MonitorManager::projectMonitorVisible() const
{
    return (m_projectMonitor->monitorIsFullScreen() || (m_projectMonitor->isVisible() && !m_projectMonitor->visibleRegion().isEmpty()));
}

bool MonitorManager::activateMonitor(Kdenlive::MonitorId name)
{
    if ((m_activeMonitor != nullptr) && m_activeMonitor->id() == name) {
        return true;
    }
    if (m_clipMonitor == nullptr || m_projectMonitor == nullptr) {
        return false;
    }
    QMutexLocker locker(&m_switchMutex);
    bool stopCurrent = m_activeMonitor != nullptr;
    for (int i = 0; i < m_monitorsList.count(); ++i) {
        if (m_monitorsList.at(i)->id() == name) {
            m_activeMonitor = m_monitorsList.at(i);
        } else if (stopCurrent) {
            m_monitorsList.at(i)->stop();
        }
    }
    if (m_activeMonitor) {
        if (name == Kdenlive::ClipMonitor) {
            if (!m_clipMonitor->monitorIsFullScreen()) {
                m_clipMonitor->parentWidget()->raise();
            }
            if (!m_clipMonitor->isVisible()) {
                pCore->displayMessage(i18n("Do you want to <a href=\"#clipmonitor\">show the clip monitor</a> to view timeline?"), MessageType::InformationMessage);
                m_activeMonitor = m_projectMonitor;
                return false;
            }
            emit updateOverlayInfos(name, KdenliveSettings::displayClipMonitorInfo());
            m_projectMonitor->displayAudioMonitor(false);
            m_clipMonitor->displayAudioMonitor(true);
        } else if (name == Kdenlive::ProjectMonitor) {
            if (!m_projectMonitor->monitorIsFullScreen()) {
                m_projectMonitor->parentWidget()->raise();
            }
            if (!m_projectMonitor->isVisible()) {
                pCore->displayMessage(i18n("Do you want to <a href=\"#projectmonitor\">show the project monitor</a> to view timeline?"), MessageType::InformationMessage);
                m_activeMonitor = m_clipMonitor;
                return false;
            }
            emit updateOverlayInfos(name, KdenliveSettings::displayProjectMonitorInfo());
            m_clipMonitor->displayAudioMonitor(false);
            m_projectMonitor->displayAudioMonitor(true);
        }
    }
    emit checkColorScopes();
    return (m_activeMonitor != nullptr);
}

void MonitorManager::resetDisplay()
{
    m_projectMonitor->clearDisplay();
    m_clipMonitor->clearDisplay();
}

bool MonitorManager::isActive(Kdenlive::MonitorId id) const
{
    return m_activeMonitor ? m_activeMonitor->id() == id : false;
}

void MonitorManager::slotSwitchMonitors(bool activateClip)
{
    if (activateClip) {
        activateMonitor(Kdenlive::ClipMonitor);
    } else {
        activateMonitor(Kdenlive::ProjectMonitor);
    }
}

void MonitorManager::stopActiveMonitor()
{
    if (m_activeMonitor) {
        m_activeMonitor->stop();
    }
}

void MonitorManager::pauseActiveMonitor()
{
    if (m_activeMonitor == m_clipMonitor) {
        m_clipMonitor->pause();
    } else if (m_activeMonitor == m_projectMonitor) {
        m_projectMonitor->pause();
    }
}

void MonitorManager::slotPlay()
{
    if (m_activeMonitor) {
        m_activeMonitor->slotPlay();
    }
}

void MonitorManager::slotPause()
{
    pauseActiveMonitor();
}

void MonitorManager::slotPlayZone()
{
    if (m_activeMonitor == m_clipMonitor) {
        m_clipMonitor->slotPlayZone();
    } else if (m_activeMonitor == m_projectMonitor) {
        m_projectMonitor->slotPlayZone();
    }
}

void MonitorManager::slotLoopZone()
{
    if (m_activeMonitor == m_clipMonitor) {
        m_clipMonitor->slotLoopZone();
    } else {
        m_projectMonitor->slotLoopZone();
    }
}

void MonitorManager::slotRewind(double speed)
{
    if (m_activeMonitor) {
        m_activeMonitor->slotRewind(speed);
    }
}

void MonitorManager::slotForward(double speed)
{
    if (m_activeMonitor) {
        m_activeMonitor->slotForward(speed, true);
    }
}

void MonitorManager::slotRewindOneFrame()
{
    if (m_activeMonitor == m_clipMonitor) {
        m_clipMonitor->slotRewindOneFrame();
    } else if (m_activeMonitor == m_projectMonitor) {
        m_projectMonitor->slotRewindOneFrame();
    }
}

void MonitorManager::slotForwardOneFrame()
{
    if (m_activeMonitor == m_clipMonitor) {
        m_clipMonitor->slotForwardOneFrame();
    } else if (m_activeMonitor == m_projectMonitor) {
        m_projectMonitor->slotForwardOneFrame();
    }
}

void MonitorManager::slotRewindOneSecond()
{
    if (m_activeMonitor == m_clipMonitor) {
        m_clipMonitor->slotRewindOneFrame(qRound(pCore->getCurrentFps()));
    } else if (m_activeMonitor == m_projectMonitor) {
        m_projectMonitor->slotRewindOneFrame(qRound(pCore->getCurrentFps()));
    }
}

void MonitorManager::slotForwardOneSecond()
{
    if (m_activeMonitor == m_clipMonitor) {
        m_clipMonitor->slotForwardOneFrame(qRound(pCore->getCurrentFps()));
    } else if (m_activeMonitor == m_projectMonitor) {
        m_projectMonitor->slotForwardOneFrame(qRound(pCore->getCurrentFps()));
    }
}

void MonitorManager::slotStart()
{
    if (m_activeMonitor == m_clipMonitor) {
        m_clipMonitor->slotStart();
    } else if (m_activeMonitor == m_projectMonitor) {
        m_projectMonitor->slotStart();
    }
}

void MonitorManager::slotEnd()
{
    if (m_activeMonitor == m_clipMonitor) {
        m_clipMonitor->slotEnd();
    } else if (m_activeMonitor == m_projectMonitor) {
        m_projectMonitor->slotEnd();
    }
}

void MonitorManager::resetProfiles()
{
    m_clipMonitor->resetProfile();
    m_projectMonitor->resetProfile();
}

void MonitorManager::resetConsumers(bool fullReset)
{
    bool clipMonitorActive = m_clipMonitor->isActive();
    m_clipMonitor->resetConsumer(fullReset);
    m_projectMonitor->resetConsumer(fullReset);
    if (clipMonitorActive) {
        refreshClipMonitor();
    } else {
        refreshProjectMonitor();
    }
}

void MonitorManager::slotUpdateAudioMonitoring()
{
    if (m_clipMonitor) {
        m_clipMonitor->updateAudioForAnalysis();
    }
    if (m_projectMonitor) {
        m_projectMonitor->updateAudioForAnalysis();
    }
}

void MonitorManager::clearScopeSource()
{
    emit clearScopes();
}

void MonitorManager::updateScopeSource()
{
    emit checkColorScopes();
}

AbstractMonitor *MonitorManager::activeMonitor()
{
    if (m_activeMonitor) {
        return m_activeMonitor;
    }
    return nullptr;
}

void MonitorManager::slotSwitchFullscreen()
{
    if (m_activeMonitor) {
        m_activeMonitor->slotSwitchFullScreen();
    }
}

void MonitorManager::setupActions()
{
    KDualAction *playAction = new KDualAction(i18n("Play"), i18n("Pause"), this);
    playAction->setInactiveIcon(QIcon::fromTheme(QStringLiteral("media-playback-start")));
    playAction->setActiveIcon(QIcon::fromTheme(QStringLiteral("media-playback-pause")));
    connect(playAction, &KDualAction::activeChangedByUser, this, &MonitorManager::slotPlay);
    pCore->window()->addAction(QStringLiteral("monitor_play"), playAction, Qt::Key_Space);

    QAction *monitorPause = new QAction(QIcon::fromTheme(QStringLiteral("media-playback-stop")), i18n("Pause"), this);
    connect(monitorPause, &QAction::triggered, this, &MonitorManager::slotPause);
    pCore->window()->addAction(QStringLiteral("monitor_pause"), monitorPause, Qt::Key_K);

    QAction *fullMonitor = new QAction(i18n("Switch monitor fullscreen"), this);
    fullMonitor->setIcon(QIcon::fromTheme(QStringLiteral("view-fullscreen")));
    connect(fullMonitor, &QAction::triggered, this, &MonitorManager::slotSwitchFullscreen);
    pCore->window()->addAction(QStringLiteral("monitor_fullscreen"), fullMonitor);

    QAction *monitorZoomIn = new QAction(i18n("Zoom in monitor"), this);
    monitorZoomIn->setIcon(QIcon::fromTheme(QStringLiteral("zoom-in")));
    connect(monitorZoomIn, &QAction::triggered, this, &MonitorManager::slotZoomIn);
    pCore->window()->addAction(QStringLiteral("monitor_zoomin"), monitorZoomIn);

    QAction *monitorZoomOut = new QAction(i18n("Zoom out monitor"), this);
    monitorZoomOut->setIcon(QIcon::fromTheme(QStringLiteral("zoom-out")));
    connect(monitorZoomOut, &QAction::triggered, this, &MonitorManager::slotZoomOut);
    pCore->window()->addAction(QStringLiteral("monitor_zoomout"), monitorZoomOut);

    QAction *monitorSeekBackward = new QAction(QIcon::fromTheme(QStringLiteral("media-seek-backward")), i18n("Rewind"), this);
    connect(monitorSeekBackward, &QAction::triggered, this, &MonitorManager::slotRewind);
    pCore->window()->addAction(QStringLiteral("monitor_seek_backward"), monitorSeekBackward, Qt::Key_J);

    QAction *monitorSeekBackwardOneFrame = new QAction(QIcon::fromTheme(QStringLiteral("media-skip-backward")), i18n("Rewind 1 Frame"), this);
    connect(monitorSeekBackwardOneFrame, &QAction::triggered, this, &MonitorManager::slotRewindOneFrame);
    pCore->window()->addAction(QStringLiteral("monitor_seek_backward-one-frame"), monitorSeekBackwardOneFrame, Qt::Key_Left);

    QAction *monitorSeekBackwardOneSecond = new QAction(QIcon::fromTheme(QStringLiteral("media-skip-backward")), i18n("Rewind 1 Second"), this);
    connect(monitorSeekBackwardOneSecond, &QAction::triggered, this, &MonitorManager::slotRewindOneSecond);
    pCore->window()->addAction(QStringLiteral("monitor_seek_backward-one-second"), monitorSeekBackwardOneSecond, Qt::SHIFT + Qt::Key_Left);

    QAction *monitorSeekForward = new QAction(QIcon::fromTheme(QStringLiteral("media-seek-forward")), i18n("Forward"), this);
    connect(monitorSeekForward,  &QAction::triggered, this, &MonitorManager::slotForward);
    pCore->window()->addAction(QStringLiteral("monitor_seek_forward"), monitorSeekForward, Qt::Key_L);

    QAction *projectStart = new QAction(QIcon::fromTheme(QStringLiteral("go-first")), i18n("Go to Project Start"), this);
    connect(projectStart, &QAction::triggered, this, &MonitorManager::slotStart);
    pCore->window()->addAction(QStringLiteral("seek_start"), projectStart, Qt::CTRL + Qt::Key_Home);

    m_multiTrack = new QAction(QIcon::fromTheme(QStringLiteral("view-split-left-right")), i18n("Multitrack view"), this);
    m_multiTrack->setCheckable(true);
    connect(m_multiTrack, &QAction::triggered, this, [&](bool checked) {
        if (m_projectMonitor) {
            emit m_projectMonitor->multitrackView(checked, true);
        }
    });
    pCore->window()->addAction(QStringLiteral("monitor_multitrack"), m_multiTrack);

    QAction *projectEnd = new QAction(QIcon::fromTheme(QStringLiteral("go-last")), i18n("Go to Project End"), this);
    connect(projectEnd, &QAction::triggered, this, &MonitorManager::slotEnd);
    pCore->window()->addAction(QStringLiteral("seek_end"), projectEnd, Qt::CTRL + Qt::Key_End);

    QAction *monitorSeekForwardOneFrame = new QAction(QIcon::fromTheme(QStringLiteral("media-skip-forward")), i18n("Forward 1 Frame"), this);
    connect(monitorSeekForwardOneFrame, &QAction::triggered, this, &MonitorManager::slotForwardOneFrame);
    pCore->window()->addAction(QStringLiteral("monitor_seek_forward-one-frame"), monitorSeekForwardOneFrame, Qt::Key_Right);

    QAction *monitorSeekForwardOneSecond = new QAction(QIcon::fromTheme(QStringLiteral("media-skip-forward")), i18n("Forward 1 Second"), this);
    connect(monitorSeekForwardOneSecond, &QAction::triggered, this, &MonitorManager::slotForwardOneSecond);
    pCore->window()->addAction(QStringLiteral("monitor_seek_forward-one-second"), monitorSeekForwardOneSecond, Qt::SHIFT + Qt::Key_Right);

    KSelectAction *interlace = new KSelectAction(i18n("Deinterlacer"), this);
    interlace->addAction(i18n("One Field (fast)"));
    interlace->addAction(i18n("Linear Blend (fast)"));
    interlace->addAction(i18n("YADIF - temporal only (good)"));
    interlace->addAction(i18n("YADIF - temporal + spacial (best)"));
    if (KdenliveSettings::mltdeinterlacer() == QLatin1String("linearblend")) {
        interlace->setCurrentItem(1);
    } else if (KdenliveSettings::mltdeinterlacer() == QLatin1String("yadif-temporal")) {
        interlace->setCurrentItem(2);
    } else if (KdenliveSettings::mltdeinterlacer() == QLatin1String("yadif")) {
        interlace->setCurrentItem(3);
    } else {
        interlace->setCurrentItem(0);
    }
    connect(interlace, static_cast<void (KSelectAction::*)(int)>(&KSelectAction::triggered), this, &MonitorManager::slotSetDeinterlacer);
    pCore->window()->addAction(QStringLiteral("mlt_interlace"), interlace);

    KSelectAction *interpol = new KSelectAction(i18n("Interpolation"), this);
    interpol->addAction(i18n("Nearest Neighbor (fast)"));
    interpol->addAction(i18n("Bilinear (good)"));
    interpol->addAction(i18n("Bicubic (better)"));
    interpol->addAction(i18n("Hyper/Lanczos (best)"));
    if (KdenliveSettings::mltinterpolation() == QLatin1String("bilinear")) {
        interpol->setCurrentItem(1);
    } else if (KdenliveSettings::mltinterpolation() == QLatin1String("bicubic")) {
        interpol->setCurrentItem(2);
    } else if (KdenliveSettings::mltinterpolation() == QLatin1String("hyper")) {
        interpol->setCurrentItem(3);
    } else {
        interpol->setCurrentItem(0);
    }
    connect(interpol, static_cast<void (KSelectAction::*)(int)>(&KSelectAction::triggered), this, &MonitorManager::slotSetInterpolation);
    pCore->window()->addAction(QStringLiteral("mlt_interpolation"), interpol);

    QAction *zoneStart = new QAction(QIcon::fromTheme(QStringLiteral("media-seek-backward")), i18n("Go to Zone Start"), this);
    connect(zoneStart, &QAction::triggered, this, &MonitorManager::slotZoneStart);
    pCore->window()->addAction(QStringLiteral("seek_zone_start"), zoneStart, Qt::SHIFT + Qt::Key_I);

    m_muteAction = new KDualAction(i18n("Mute monitor"), i18n("Unmute monitor"), this);
    m_muteAction->setActiveIcon(QIcon::fromTheme(QStringLiteral("audio-volume-medium")));
    m_muteAction->setInactiveIcon(QIcon::fromTheme(QStringLiteral("audio-volume-muted")));
    connect(m_muteAction, &KDualAction::activeChangedByUser, this, &MonitorManager::slotMuteCurrentMonitor);
    pCore->window()->addAction(QStringLiteral("mlt_mute"), m_muteAction);

    QAction *zoneEnd = new QAction(QIcon::fromTheme(QStringLiteral("media-seek-forward")), i18n("Go to Zone End"), this);
    connect(zoneEnd, &QAction::triggered, this, &MonitorManager::slotZoneEnd);
    pCore->window()->addAction(QStringLiteral("seek_zone_end"), zoneEnd, Qt::SHIFT + Qt::Key_O);

    QAction *markIn = new QAction(QIcon::fromTheme(QStringLiteral("zone-in")), i18n("Set Zone In"), this);
    connect(markIn, &QAction::triggered, this, &MonitorManager::slotSetInPoint);
    pCore->window()->addAction(QStringLiteral("mark_in"), markIn, Qt::Key_I);

    QAction *markOut = new QAction(QIcon::fromTheme(QStringLiteral("zone-out")), i18n("Set Zone Out"), this);
    connect(markOut, &QAction::triggered, this, &MonitorManager::slotSetOutPoint);
    pCore->window()->addAction(QStringLiteral("mark_out"), markOut, Qt::Key_O);
}

void MonitorManager::refreshIcons()
{
    QList<QAction *> allMenus = this->findChildren<QAction *>();
    for (int i = 0; i < allMenus.count(); i++) {
        QAction *m = allMenus.at(i);
        QIcon ic = m->icon();
        if (ic.isNull() || ic.name().isEmpty()) {
            continue;
        }
        QIcon newIcon = QIcon::fromTheme(ic.name());
        m->setIcon(newIcon);
    }
}

void MonitorManager::slotSetDeinterlacer(int ix)
{
    QString value;
    switch (ix) {

    case 1:
        value = QStringLiteral("linearblend");
        break;
    case 2:
        value = QStringLiteral("yadif-nospatial");
        break;
    case 3:
        value = QStringLiteral("yadif");
        break;
    default:
        value = QStringLiteral("onefield");
    }
    KdenliveSettings::setMltdeinterlacer(value);
    setConsumerProperty(QStringLiteral("deinterlace_method"), value);
}

void MonitorManager::slotSetInterpolation(int ix)
{
    QString value;
    switch (ix) {
    case 1:
        value = QStringLiteral("bilinear");
        break;
    case 2:
        value = QStringLiteral("bicubic");
        break;
    case 3:
        value = QStringLiteral("hyper");
        break;
    default:
        value = QStringLiteral("nearest");
    }
    KdenliveSettings::setMltinterpolation(value);
    setConsumerProperty(QStringLiteral("rescale"), value);
}

void MonitorManager::slotMuteCurrentMonitor(bool active)
{
    m_activeMonitor->mute(active);
}

Monitor *MonitorManager::clipMonitor()
{
    return m_clipMonitor;
}

Monitor *MonitorManager::projectMonitor()
{
    return m_projectMonitor;
}

void MonitorManager::slotZoneStart()
{
    if (m_activeMonitor == m_clipMonitor) {
        m_clipMonitor->slotZoneStart();
    } else if (m_activeMonitor == m_projectMonitor) {
        m_projectMonitor->slotZoneStart();
    }
}

void MonitorManager::slotZoneEnd()
{
    if (m_activeMonitor == m_projectMonitor) {
        m_projectMonitor->slotZoneEnd();
    } else if (m_activeMonitor == m_clipMonitor) {
        m_clipMonitor->slotZoneEnd();
    }
}

void MonitorManager::slotSetInPoint()
{
    if (m_activeMonitor == m_clipMonitor) {
        m_clipMonitor->slotSetZoneStart();
    } else if (m_activeMonitor == m_projectMonitor) {
        QPoint sourceZone = m_projectMonitor->getZoneInfo();
        QPoint destZone = sourceZone;
        destZone.setX(m_projectMonitor->position());
        if (destZone.x() > destZone.y()) {
            destZone.setY(qMin(pCore->projectDuration(), destZone.x() + (sourceZone.y() - sourceZone.x())));
        }
        emit m_projectMonitor->zoneUpdatedWithUndo(sourceZone, destZone);
    }
}

void MonitorManager::slotSetOutPoint()
{
    if (m_activeMonitor == m_clipMonitor) {
        m_clipMonitor->slotSetZoneEnd();
    } else if (m_activeMonitor == m_projectMonitor) {
        QPoint sourceZone = m_projectMonitor->getZoneInfo();
        QPoint destZone = sourceZone;
        destZone.setY(m_projectMonitor->position() + 1);
        if (destZone.y() < destZone.x()) {
            destZone.setX(qMax(0, destZone.y() - (sourceZone.y() - sourceZone.x())));
        }
        emit m_projectMonitor->zoneUpdatedWithUndo(sourceZone, destZone);
    }
}

void MonitorManager::slotExtractCurrentFrame()
{
    if (m_activeMonitor) {
        static_cast<Monitor *>(m_activeMonitor)->slotExtractCurrentFrame();
    }
}

void MonitorManager::slotExtractCurrentFrameToProject()
{
    if (m_activeMonitor) {
        static_cast<Monitor *>(m_activeMonitor)->slotExtractCurrentFrame(QString(), true);
    }
}

void MonitorManager::slotZoomIn()
{
    if (m_activeMonitor) {
        static_cast<Monitor *>(m_activeMonitor)->slotZoomIn();
    }
}

void MonitorManager::slotZoomOut()
{
    if (m_activeMonitor) {
        static_cast<Monitor *>(m_activeMonitor)->slotZoomOut();
    }
}

bool MonitorManager::isMultiTrack() const
{
    if (m_multiTrack) {
        return m_multiTrack->isChecked();
    }
    return false;
}

void MonitorManager::updateBgColor()
{
    if (m_projectMonitor) {
        m_projectMonitor->updateBgColor();
        m_projectMonitor->forceMonitorRefresh();
    }
    if (m_clipMonitor) {
        m_clipMonitor->updateBgColor();
        m_clipMonitor->forceMonitorRefresh();
    }
}
