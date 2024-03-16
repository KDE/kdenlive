/*
    SPDX-FileCopyrightText: 2007 Jean-Baptiste Mardelle <jb@kdenlive.org>

SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "monitormanager.h"
#include "core.h"
#include "doc/kdenlivedoc.h"
#include "kdenlivesettings.h"
#include "mainwindow.h"
#include "timeline2/view/timelinewidget.h"

#include <mlt++/Mlt.h>

#include "klocalizedstring.h"
#include <KDualAction>
#include <kwidgetsaddons_version.h>

#include "kdenlive_debug.h"
#include <QObject>
#include <dialogs/timeremap.h>
#include <timeline2/view/timelinecontroller.h>

const double MonitorManager::speedArray[6] = {1., 1.5, 2., 3., 5.5, 10.};

MonitorManager::MonitorManager(QObject *parent)
    : QObject(parent)
    , m_activeMultiTrack(-1)

{
    setupActions();
    refreshTimer.setSingleShot(true);
    refreshTimer.setInterval(200);
    connect(&refreshTimer, &QTimer::timeout, this, &MonitorManager::forceProjectMonitorRefresh);
    connect(pCore.get(), &Core::monitorProfileUpdated, this, [&]() {
        QAction *prog = pCore->window()->actionCollection()->action(QStringLiteral("mlt_progressive"));
        if (prog) {
            prog->setEnabled(!pCore->getProjectProfile().progressive());
            slotProgressivePlay(prog->isChecked());
        }
    });
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
    if (!m_projectMonitor->isActive()) {
        activateMonitor(Kdenlive::ProjectMonitor);
    } else {
        // Force raise
        m_projectMonitor->parentWidget()->raise();
    }
}

void MonitorManager::refreshProjectRange(QPair<int, int> range, bool forceRefresh)
{
    if (m_projectMonitor->position() >= range.first && m_projectMonitor->position() <= range.second) {
        if (forceRefresh) {
            m_projectMonitor->refreshMonitor(false);
        } else {
            m_projectMonitor->refreshMonitorIfActive();
        }
    }
}

void MonitorManager::refreshProjectMonitor(bool directUpdate)
{
    m_projectMonitor->refreshMonitor(directUpdate);
}

void MonitorManager::refreshClipMonitor(bool directUpdate)
{
    m_clipMonitor->refreshMonitor(directUpdate);
}

void MonitorManager::forceProjectMonitorRefresh()
{
    m_projectMonitor->forceMonitorRefresh();
}

bool MonitorManager::projectMonitorVisible() const
{
    return (m_projectMonitor->monitorIsFullScreen() || (m_projectMonitor->isVisible() && !m_projectMonitor->visibleRegion().isEmpty()));
}

bool MonitorManager::clipMonitorVisible() const
{
    return (m_clipMonitor->monitorIsFullScreen() || (m_clipMonitor->isVisible() && !m_clipMonitor->visibleRegion().isEmpty()));
}

void MonitorManager::refreshMonitors()
{
    if (m_activeMonitor) {
        if (m_activeMonitor == m_clipMonitor) {
            activateMonitor(Kdenlive::ProjectMonitor);
            refreshProjectMonitor(true);
            activateMonitor(Kdenlive::ClipMonitor);
            refreshClipMonitor(true);
        } else {
            bool playing = m_projectMonitor->isPlaying();
            if (playing) {
                m_projectMonitor->switchPlay(false);
            }
            activateMonitor(Kdenlive::ClipMonitor);
            refreshClipMonitor(true);
            activateMonitor(Kdenlive::ProjectMonitor);
            refreshProjectMonitor(true);
            if (playing) {
                m_projectMonitor->switchPlay(true);
            }
        }
    }
}

bool MonitorManager::activateMonitor(Kdenlive::MonitorId name, bool raiseMonitor)
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
                if (raiseMonitor) {
                    m_clipMonitor->parentWidget()->raise();
                }
            } else {
                m_clipMonitor->fixFocus();
            }
            // Set guides list to show guides
            m_clipMonitor->updateGuidesList();
            if (!m_clipMonitor->isVisible()) {
                pCore->displayMessage(i18n("Do you want to <a href=\"#clipmonitor\">show the clip monitor</a> to view timeline?"),
                                      MessageType::InformationMessage);
                m_activeMonitor = m_projectMonitor;
                return false;
            }
            Q_EMIT updateOverlayInfos(name, KdenliveSettings::displayClipMonitorInfo());
            m_projectMonitor->displayAudioMonitor(false);
            m_clipMonitor->displayAudioMonitor(true);
        } else if (name == Kdenlive::ProjectMonitor) {
            // Set guides list to show guides
            m_projectMonitor->updateGuidesList();
            if (!m_projectMonitor->monitorIsFullScreen()) {
                if (raiseMonitor) {
                    m_projectMonitor->parentWidget()->raise();
                }
            } else {
                m_projectMonitor->fixFocus();
            }
            if (!m_projectMonitor->isVisible()) {
                pCore->displayMessage(i18n("Do you want to <a href=\"#projectmonitor\">show the project monitor</a> to view timeline?"),
                                      MessageType::InformationMessage);
                m_activeMonitor = m_clipMonitor;
                return false;
            }
            Q_EMIT updateOverlayInfos(name, KdenliveSettings::displayProjectMonitorInfo());
            m_clipMonitor->displayAudioMonitor(false);
            m_projectMonitor->displayAudioMonitor(true);
        }
    }
    Q_EMIT checkColorScopes();
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
    if (pCore->activeTool() == ToolType::SlipTool) {
        m_projectMonitor->slotTrimmingPos(-1);
        pCore->window()->getCurrentTimeline()->model()->requestSlipSelection(-1, true);
    } else if (isTrimming()) {
        return;
    } else {
        if (m_activeMonitor == m_clipMonitor) {
            m_clipMonitor->slotRewindOneFrame();
        } else if (m_activeMonitor == m_projectMonitor) {
            m_projectMonitor->slotRewindOneFrame();
        }
    }
}

void MonitorManager::slotForwardOneFrame()
{
    if (pCore->activeTool() == ToolType::SlipTool) {
        m_projectMonitor->slotTrimmingPos(1);
        pCore->window()->getCurrentTimeline()->model()->requestSlipSelection(1, true);
    } else if (isTrimming()) {
        return;
    } else {
        if (m_activeMonitor == m_clipMonitor) {
            m_clipMonitor->slotForwardOneFrame();
        } else if (m_activeMonitor == m_projectMonitor) {
            m_projectMonitor->slotForwardOneFrame();
        }
    }
}

void MonitorManager::slotRewindOneSecond()
{
    if (pCore->activeTool() == ToolType::SlipTool) {
        m_projectMonitor->slotTrimmingPos(-qRound(pCore->getCurrentFps()));
        pCore->window()->getCurrentTimeline()->model()->requestSlipSelection(-qRound(pCore->getCurrentFps()), true);
    } else if (isTrimming()) {
        return;
    } else {
        if (m_activeMonitor == m_clipMonitor) {
            m_clipMonitor->slotRewindOneFrame(qRound(pCore->getCurrentFps()));
        } else if (m_activeMonitor == m_projectMonitor) {
            m_projectMonitor->slotRewindOneFrame(qRound(pCore->getCurrentFps()));
        }
    }
}

void MonitorManager::slotForwardOneSecond()
{
    if (pCore->activeTool() == ToolType::SlipTool) {
        m_projectMonitor->slotTrimmingPos(qRound(pCore->getCurrentFps()));
        pCore->window()->getCurrentTimeline()->model()->requestSlipSelection(qRound(pCore->getCurrentFps()), true);
    } else if (isTrimming()) {
        return;
    } else {
        if (m_activeMonitor == m_clipMonitor) {
            m_clipMonitor->slotForwardOneFrame(qRound(pCore->getCurrentFps()));
        } else if (m_activeMonitor == m_projectMonitor) {
            m_projectMonitor->slotForwardOneFrame(qRound(pCore->getCurrentFps()));
        }
    }
}

void MonitorManager::slotStartMultiTrackMode()
{
    getAction(QStringLiteral("monitor_multitrack"))->setEnabled(false);
    m_activeMultiTrack = pCore->window()->getCurrentTimeline()->controller()->activeTrack();
    pCore->window()->getCurrentTimeline()->controller()->setMulticamIn(m_projectMonitor->position());
}

void MonitorManager::slotStopMultiTrackMode()
{
    if (m_activeMultiTrack == -1) {
        return;
    }
    getAction(QStringLiteral("monitor_multitrack"))->setEnabled(true);
    pCore->window()->getCurrentTimeline()->controller()->setMulticamIn(-1);
    m_activeMultiTrack = -1;
}

void MonitorManager::slotPerformMultiTrackMode()
{
    if (m_activeMultiTrack == -1) {
        return;
    }
    pCore->window()->getCurrentTimeline()->controller()->processMultitrackOperation(m_activeMultiTrack,
                                                                                    pCore->window()->getCurrentTimeline()->controller()->multicamIn);
    m_activeMultiTrack = pCore->window()->getCurrentTimeline()->controller()->activeTrack();
    pCore->window()->getCurrentTimeline()->controller()->setMulticamIn(m_projectMonitor->position());
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
    if (m_clipMonitor) {
        m_clipMonitor->resetProfile();
    }
    if (m_projectMonitor) {
        m_projectMonitor->resetProfile();
    }
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

void MonitorManager::slotToggleEffectScene(bool enable)
{
    if (m_activeMonitor) {
        static_cast<Monitor *>(m_activeMonitor)->enableEffectScene(enable);
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
    Q_EMIT clearScopes();
}

void MonitorManager::updateScopeSource()
{
    Q_EMIT checkColorScopes();
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
        QMetaObject::invokeMethod(m_activeMonitor, "slotSwitchFullScreen", Qt::QueuedConnection);
    }
}

void MonitorManager::setupActions()
{
    KDualAction *playAction = new KDualAction(i18n("Play"), i18n("Pause"), this);
    playAction->setInactiveIcon(QIcon::fromTheme(QStringLiteral("media-playback-start")));
    playAction->setActiveIcon(QIcon::fromTheme(QStringLiteral("media-playback-pause")));
    connect(playAction, &KDualAction::activeChangedByUser, this, &MonitorManager::slotPlay);
    pCore->window()->addAction(QStringLiteral("monitor_play"), playAction, Qt::Key_Space, QStringLiteral("navandplayback"));

    QAction *monitorPause = new QAction(QIcon::fromTheme(QStringLiteral("media-playback-stop")), i18n("Pause"), this);
    connect(monitorPause, &QAction::triggered, this, &MonitorManager::slotPause);
    pCore->window()->addAction(QStringLiteral("monitor_pause"), monitorPause, Qt::Key_K, QStringLiteral("navandplayback"));

    QAction *fullMonitor = new QAction(i18n("Switch Monitor Fullscreen"), this);
    fullMonitor->setIcon(QIcon::fromTheme(QStringLiteral("view-fullscreen")));
    connect(fullMonitor, &QAction::triggered, this, &MonitorManager::slotSwitchFullscreen);
    pCore->window()->addAction(QStringLiteral("monitor_fullscreen"), fullMonitor, Qt::Key_F11, QStringLiteral("monitor"));

    QAction *monitorZoomIn = new QAction(i18n("Zoom In Monitor"), this);
    monitorZoomIn->setIcon(QIcon::fromTheme(QStringLiteral("zoom-in")));
    connect(monitorZoomIn, &QAction::triggered, this, &MonitorManager::slotZoomIn);
    pCore->window()->addAction(QStringLiteral("monitor_zoomin"), monitorZoomIn, {}, QStringLiteral("monitor"));

    QAction *monitorZoomOut = new QAction(i18n("Zoom Out Monitor"), this);
    monitorZoomOut->setIcon(QIcon::fromTheme(QStringLiteral("zoom-out")));
    connect(monitorZoomOut, &QAction::triggered, this, &MonitorManager::slotZoomOut);
    pCore->window()->addAction(QStringLiteral("monitor_zoomout"), monitorZoomOut, {}, QStringLiteral("monitor"));

    QAction *monitorSeekBackward = new QAction(QIcon::fromTheme(QStringLiteral("media-seek-backward")), i18n("Rewind"), this);
    connect(monitorSeekBackward, &QAction::triggered, this, [&](bool) { MonitorManager::slotRewind(); });
    pCore->window()->addAction(QStringLiteral("monitor_seek_backward"), monitorSeekBackward, Qt::Key_J, QStringLiteral("navandplayback"));

    QAction *monitorSeekBackwardOneFrame = new QAction(QIcon::fromTheme(QStringLiteral("media-skip-backward")), i18n("Rewind 1 Frame"), this);
    connect(monitorSeekBackwardOneFrame, &QAction::triggered, this, &MonitorManager::slotRewindOneFrame);
    pCore->window()->addAction(QStringLiteral("monitor_seek_backward-one-frame"), monitorSeekBackwardOneFrame, Qt::Key_Left, QStringLiteral("navandplayback"));

    QAction *monitorSeekBackwardOneSecond = new QAction(QIcon::fromTheme(QStringLiteral("media-skip-backward")), i18n("Rewind 1 Second"), this);
    connect(monitorSeekBackwardOneSecond, &QAction::triggered, this, &MonitorManager::slotRewindOneSecond);
    pCore->window()->addAction(QStringLiteral("monitor_seek_backward-one-second"), monitorSeekBackwardOneSecond, Qt::SHIFT | Qt::Key_Left,
                               QStringLiteral("navandplayback"));

    QAction *monitorSeekForward = new QAction(QIcon::fromTheme(QStringLiteral("media-seek-forward")), i18n("Forward"), this);
    connect(monitorSeekForward, &QAction::triggered, this, [&](bool) { MonitorManager::slotForward(); });
    pCore->window()->addAction(QStringLiteral("monitor_seek_forward"), monitorSeekForward, Qt::Key_L, QStringLiteral("navandplayback"));

    QAction *projectStart = new QAction(QIcon::fromTheme(QStringLiteral("go-first")), i18n("Go to Project Start"), this);
    connect(projectStart, &QAction::triggered, this, &MonitorManager::slotStart);
    pCore->window()->addAction(QStringLiteral("seek_start"), projectStart, Qt::CTRL | Qt::Key_Home, QStringLiteral("navandplayback"));

    QAction *projectEnd = new QAction(QIcon::fromTheme(QStringLiteral("go-last")), i18n("Go to Project End"), this);
    connect(projectEnd, &QAction::triggered, this, &MonitorManager::slotEnd);
    pCore->window()->addAction(QStringLiteral("seek_end"), projectEnd, Qt::CTRL | Qt::Key_End, QStringLiteral("navandplayback"));

    QAction *monitorSeekForwardOneFrame = new QAction(QIcon::fromTheme(QStringLiteral("media-skip-forward")), i18n("Forward 1 Frame"), this);
    connect(monitorSeekForwardOneFrame, &QAction::triggered, this, &MonitorManager::slotForwardOneFrame);
    pCore->window()->addAction(QStringLiteral("monitor_seek_forward-one-frame"), monitorSeekForwardOneFrame, Qt::Key_Right, QStringLiteral("navandplayback"));

    QAction *monitorSeekForwardOneSecond = new QAction(QIcon::fromTheme(QStringLiteral("media-skip-forward")), i18n("Forward 1 Second"), this);
    connect(monitorSeekForwardOneSecond, &QAction::triggered, this, &MonitorManager::slotForwardOneSecond);
    pCore->window()->addAction(QStringLiteral("monitor_seek_forward-one-second"), monitorSeekForwardOneSecond, Qt::SHIFT | Qt::Key_Right,
                               QStringLiteral("navandplayback"));

    m_multiTrack = new QAction(QIcon::fromTheme(QStringLiteral("view-split-left-right")), i18n("Multitrack View"), this);
    m_multiTrack->setCheckable(true);
    connect(m_multiTrack, &QAction::triggered, this, [&](bool checked) {
        if (m_projectMonitor) {
            Q_EMIT m_projectMonitor->multitrackView(checked, true);
        }
    });
    pCore->window()->addAction(QStringLiteral("monitor_multitrack"), m_multiTrack, Qt::Key_F12, QStringLiteral("monitor"));

    QAction *performMultiTrackOperation = new QAction(QIcon::fromTheme(QStringLiteral("media-playback-pause")), i18n("Perform Multitrack Operation"), this);
    connect(performMultiTrackOperation, &QAction::triggered, this, &MonitorManager::slotPerformMultiTrackMode);
    pCore->window()->addAction(QStringLiteral("perform_multitrack_mode"), performMultiTrackOperation);

    QAction *enableEditmode = new QAction(QIcon::fromTheme(QStringLiteral("transform-crop")), i18n("Show/Hide edit mode"), this);
    enableEditmode->setWhatsThis(xi18nc("@info:whatsthis", "Toggles edit mode (and the display of the object handles)."));
    enableEditmode->setCheckable(true);
    enableEditmode->setChecked(KdenliveSettings::showOnMonitorScene());
    connect(enableEditmode, &QAction::triggered, this, &MonitorManager::slotToggleEffectScene);
    pCore->window()->addAction(QStringLiteral("monitor_editmode"), enableEditmode, {}, QStringLiteral("monitor"));

    KSelectAction *interlace = new KSelectAction(i18n("Deinterlacer"), this);
    interlace->addAction(i18n("One Field (fast)"));
    interlace->addAction(i18n("Linear Blend (fast)"));
    interlace->addAction(i18n("YADIF - temporal only (good)"));
    interlace->addAction(i18n("YADIF - temporal + spacial (best)"));
    if (KdenliveSettings::mltdeinterlacer() == QLatin1String("linearblend")) {
        interlace->setCurrentItem(1);
    } else if (KdenliveSettings::mltdeinterlacer() == QLatin1String("yadif-nospatial")) {
        interlace->setCurrentItem(2);
    } else if (KdenliveSettings::mltdeinterlacer() == QLatin1String("yadif")) {
        interlace->setCurrentItem(3);
    } else {
        interlace->setCurrentItem(0);
    }
    connect(interlace, &KSelectAction::indexTriggered, this, &MonitorManager::slotSetDeinterlacer);
    pCore->window()->addAction(QStringLiteral("mlt_interlace"), interlace);
    pCore->window()->actionCollection()->setShortcutsConfigurable(interlace, false);

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
    connect(interpol, &KSelectAction::indexTriggered, this, &MonitorManager::slotSetInterpolation);
    pCore->window()->addAction(QStringLiteral("mlt_interpolation"), interpol);
    pCore->window()->actionCollection()->setShortcutsConfigurable(interpol, false);

    QAction *progressive = new QAction(i18n("Progressive playback"), this);
    connect(progressive, &QAction::triggered, this, &MonitorManager::slotProgressivePlay);
    pCore->window()->addAction(QStringLiteral("mlt_progressive"), progressive);
    progressive->setCheckable(true);
    progressive->setChecked(KdenliveSettings::monitor_progressive());

    QAction *audioScrub = new QAction(i18n("Audio Scrubbing"), this);
    connect(audioScrub, &QAction::triggered, this, [&](bool enable) { KdenliveSettings::setAudio_scrub(enable); });
    pCore->window()->addAction(QStringLiteral("mlt_scrub"), audioScrub);
    audioScrub->setCheckable(true);
    audioScrub->setChecked(KdenliveSettings::audio_scrub());

    m_muteAction = new KDualAction(i18n("Mute Monitor"), i18n("Unmute Monitor"), this);
    m_muteAction->setActiveIcon(QIcon::fromTheme(QStringLiteral("audio-volume-medium")));
    m_muteAction->setInactiveIcon(QIcon::fromTheme(QStringLiteral("audio-volume-muted")));
    connect(m_muteAction, &KDualAction::activeChangedByUser, this, &MonitorManager::slotMuteCurrentMonitor);
    pCore->window()->addAction(QStringLiteral("mlt_mute"), m_muteAction, {}, QStringLiteral("monitor"));

    QAction *zoneStart = new QAction(QIcon::fromTheme(QStringLiteral("media-seek-backward")), i18n("Go to Zone Start"), this);
    connect(zoneStart, &QAction::triggered, this, &MonitorManager::slotZoneStart);
    pCore->window()->addAction(QStringLiteral("seek_zone_start"), zoneStart, Qt::SHIFT | Qt::Key_I, QStringLiteral("navandplayback"));

    QAction *zoneEnd = new QAction(QIcon::fromTheme(QStringLiteral("media-seek-forward")), i18n("Go to Zone End"), this);
    connect(zoneEnd, &QAction::triggered, this, &MonitorManager::slotZoneEnd);
    pCore->window()->addAction(QStringLiteral("seek_zone_end"), zoneEnd, Qt::SHIFT | Qt::Key_O, QStringLiteral("navandplayback"));

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
    setConsumerProperty(QStringLiteral("deinterlacer"), value);
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

void MonitorManager::slotProgressivePlay(bool active)
{
    if (pCore->getProjectProfile().progressive()) {
        // nothing to do
        return;
    }
    KdenliveSettings::setMonitor_progressive(active);
    if (m_clipMonitor) {
        m_clipMonitor->resetConsumer(true);
        m_clipMonitor->refreshMonitor(true);
    }
    if (m_projectMonitor) {
        m_projectMonitor->resetConsumer(true);
        m_projectMonitor->refreshMonitor(true);
    }
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
        Q_EMIT m_projectMonitor->zoneUpdatedWithUndo(sourceZone, destZone);
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
        Q_EMIT m_projectMonitor->zoneUpdatedWithUndo(sourceZone, destZone);
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

void MonitorManager::switchMultiTrackView(bool enable)
{
    if (isMultiTrack()) {
        if (!enable && m_projectMonitor) {
            m_multiTrack->setChecked(false);
            Q_EMIT m_projectMonitor->multitrackView(false, true);
        }
    } else if (enable) {
        m_multiTrack->trigger();
    }
}

bool MonitorManager::isTrimming() const
{
    if (m_projectMonitor && m_projectMonitor->m_trimmingbar) {
        return m_projectMonitor->m_trimmingbar->isVisible();
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
