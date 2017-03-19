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
#include "renderer.h"
#include "kdenlivesettings.h"
#include "mainwindow.h"
#include "doc/kdenlivedoc.h"
#include "utils/KoIconUtils.h"
#include "mltcontroller/bincontroller.h"

#include <mlt++/Mlt.h>

#include "klocalizedstring.h"
#include <KDualAction>

#include <QObject>
#include "kdenlive_debug.h"

MonitorManager::MonitorManager(QObject *parent) :
    QObject(parent),
    m_document(nullptr),
    m_clipMonitor(nullptr),
    m_projectMonitor(nullptr),
    m_activeMonitor(nullptr)
{
    setupActions();
}

Timecode MonitorManager::timecode() const
{
    return m_timecode;
}

void MonitorManager::setDocument(KdenliveDoc *doc)
{
    m_document = doc;
}

QAction *MonitorManager::getAction(const QString &name)
{
    return pCore->window()->action(name.toLatin1());
}

void MonitorManager::initMonitors(Monitor *clipMonitor, Monitor *projectMonitor, RecMonitor *recMonitor)
{
    m_clipMonitor = clipMonitor;
    m_projectMonitor = projectMonitor;
    connect(m_clipMonitor->render, SIGNAL(activateMonitor(Kdenlive::MonitorId)), this, SLOT(activateMonitor(Kdenlive::MonitorId)));
    connect(m_projectMonitor->render, SIGNAL(activateMonitor(Kdenlive::MonitorId)), this, SLOT(activateMonitor(Kdenlive::MonitorId)));
    m_monitorsList.append(clipMonitor);
    m_monitorsList.append(projectMonitor);
    if (recMonitor) {
        m_monitorsList.append(recMonitor);
    }
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
    for (int i = 0; i < m_monitorsList.size(); ++i) {
        if (m_monitorsList[i]->id() == monitorName) {
            monitor = m_monitorsList.at(i);
        }
    }
    return monitor;
}

void MonitorManager::setConsumerProperty(const QString &name, const QString &value)
{
    if (m_clipMonitor) {
        m_clipMonitor->render->setConsumerProperty(name, value);
    }
    if (m_projectMonitor) {
        m_projectMonitor->render->setConsumerProperty(name, value);
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

bool MonitorManager::activateMonitor(Kdenlive::MonitorId name, bool forceRefresh)
{
    if (m_clipMonitor == nullptr || m_projectMonitor == nullptr) {
        return false;
    }
    if (m_activeMonitor && m_activeMonitor->id() == name) {
        if (forceRefresh) {
            m_activeMonitor->start();
        }
        return false;
    }
    QMutexLocker locker(&m_switchMutex);
    m_activeMonitor = nullptr;
    for (int i = 0; i < m_monitorsList.count(); ++i) {
        if (m_monitorsList.at(i)->id() == name) {
            m_activeMonitor = m_monitorsList.at(i);
        } else {
            m_monitorsList.at(i)->stop();
        }
    }
    if (m_activeMonitor) {
        m_activeMonitor->blockSignals(true);
        m_activeMonitor->parentWidget()->raise();
        if (name == Kdenlive::ClipMonitor) {
            emit updateOverlayInfos(name, KdenliveSettings::displayClipMonitorInfo());
            m_projectMonitor->displayAudioMonitor(false);
            m_clipMonitor->displayAudioMonitor(true);
        } else if (name == Kdenlive::ProjectMonitor) {
            emit updateOverlayInfos(name, KdenliveSettings::displayProjectMonitorInfo());
            m_clipMonitor->displayAudioMonitor(false);
            m_projectMonitor->displayAudioMonitor(true);
        }
        m_activeMonitor->blockSignals(false);
        m_activeMonitor->start();
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
    if (m_activeMonitor == m_clipMonitor) {
        m_clipMonitor->slotRewind(speed);
    } else if (m_activeMonitor == m_projectMonitor) {
        m_projectMonitor->slotRewind(speed);
    }
}

void MonitorManager::slotForward(double speed)
{
    if (m_activeMonitor == m_clipMonitor) {
        m_clipMonitor->slotForward(speed);
    } else if (m_activeMonitor == m_projectMonitor) {
        m_projectMonitor->slotForward(speed);
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
        m_clipMonitor->slotRewindOneFrame(m_timecode.fps());
    } else if (m_activeMonitor == m_projectMonitor) {
        m_projectMonitor->slotRewindOneFrame(m_timecode.fps());
    }
}

void MonitorManager::slotForwardOneSecond()
{
    if (m_activeMonitor == m_clipMonitor) {
        m_clipMonitor->slotForwardOneFrame(m_timecode.fps());
    } else if (m_activeMonitor == m_projectMonitor) {
        m_projectMonitor->slotForwardOneFrame(m_timecode.fps());
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

void MonitorManager::resetProfiles(const MltVideoProfile &profile, const Timecode &tc)
{
    m_timecode = tc;
    m_clipMonitor->resetProfile(profile);
    m_projectMonitor->resetProfile(profile);
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

AbstractRender *MonitorManager::activeRenderer()
{
    if (m_activeMonitor) {
        return m_activeMonitor->abstractRender();
    }
    return nullptr;
}

void MonitorManager::slotSwitchFullscreen()
{
    if (m_activeMonitor) {
        m_activeMonitor->slotSwitchFullScreen();
    }
}

QString MonitorManager::getProjectFolder() const
{
    if (m_document == nullptr) {
        //qCDebug(KDENLIVE_LOG)<<" + + +nullptr DOC!!";
        return QString();
    }
    return m_document->projectDataFolder() + QDir::separator();
}

BinController *MonitorManager::binController()
{
    return pCore->binController();
}

Mlt::Profile *MonitorManager::profile()
{
    return pCore->binController()->profile();
}

void MonitorManager::setupActions()
{
    KDualAction *playAction = new KDualAction(i18n("Play"), i18n("Pause"), this);
    playAction->setInactiveIcon(KoIconUtils::themedIcon(QStringLiteral("media-playback-start")));
    playAction->setActiveIcon(KoIconUtils::themedIcon(QStringLiteral("media-playback-pause")));
    playAction->setShortcut(Qt::Key_Space);
    pCore->window()->addAction(QStringLiteral("monitor_play"), playAction);
    connect(playAction, &QAction::triggered, this, &MonitorManager::slotPlay);

    QAction *monitorPause = new QAction(KoIconUtils::themedIcon(QStringLiteral("media-playback-stop")), i18n("Pause"), this);
    monitorPause->setShortcut(Qt::Key_K);
    pCore->window()->addAction(QStringLiteral("monitor_pause"), monitorPause);
    connect(monitorPause, &QAction::triggered, this, &MonitorManager::slotPause);

    QAction *fullMonitor = new QAction(i18n("Switch monitor fullscreen"), this);
    fullMonitor->setIcon(KoIconUtils::themedIcon(QStringLiteral("view-fullscreen")));
    pCore->window()->addAction(QStringLiteral("monitor_fullscreen"), fullMonitor);
    connect(fullMonitor, &QAction::triggered, this, &MonitorManager::slotSwitchFullscreen);

    QAction *monitorSeekBackward = new QAction(KoIconUtils::themedIcon(QStringLiteral("media-seek-backward")), i18n("Rewind"), this);
    monitorSeekBackward->setShortcut(Qt::Key_J);
    pCore->window()->addAction(QStringLiteral("monitor_seek_backward"), monitorSeekBackward);
    connect(monitorSeekBackward, SIGNAL(triggered(bool)), SLOT(slotRewind()));

    QAction *monitorSeekBackwardOneFrame = new QAction(KoIconUtils::themedIcon(QStringLiteral("media-skip-backward")), i18n("Rewind 1 Frame"), this);
    monitorSeekBackwardOneFrame->setShortcut(Qt::Key_Left);
    pCore->window()->addAction(QStringLiteral("monitor_seek_backward-one-frame"), monitorSeekBackwardOneFrame);
    connect(monitorSeekBackwardOneFrame, &QAction::triggered, this, &MonitorManager::slotRewindOneFrame);

    QAction *monitorSeekBackwardOneSecond = new QAction(KoIconUtils::themedIcon(QStringLiteral("media-skip-backward")), i18n("Rewind 1 Second"), this);
    monitorSeekBackwardOneSecond->setShortcut(Qt::SHIFT + Qt::Key_Left);
    pCore->window()->addAction(QStringLiteral("monitor_seek_backward-one-second"), monitorSeekBackwardOneSecond);
    connect(monitorSeekBackwardOneSecond, &QAction::triggered, this, &MonitorManager::slotRewindOneSecond);

    QAction *monitorSeekForward = new QAction(KoIconUtils::themedIcon(QStringLiteral("media-seek-forward")), i18n("Forward"), this);
    monitorSeekForward->setShortcut(Qt::Key_L);
    pCore->window()->addAction(QStringLiteral("monitor_seek_forward"), monitorSeekForward);
    connect(monitorSeekForward, SIGNAL(triggered(bool)), SLOT(slotForward()));

    QAction *projectStart = new QAction(KoIconUtils::themedIcon(QStringLiteral("go-first")), i18n("Go to Project Start"), this);
    projectStart->setShortcut(Qt::CTRL + Qt::Key_Home);
    pCore->window()->addAction(QStringLiteral("seek_start"), projectStart);
    connect(projectStart, &QAction::triggered, this, &MonitorManager::slotStart);

    QAction *projectEnd = new QAction(KoIconUtils::themedIcon(QStringLiteral("go-last")), i18n("Go to Project End"), this);
    projectEnd->setShortcut(Qt::CTRL + Qt::Key_End);
    pCore->window()->addAction(QStringLiteral("seek_end"), projectEnd);
    connect(projectEnd, &QAction::triggered, this, &MonitorManager::slotEnd);

    QAction *monitorSeekForwardOneFrame = new QAction(KoIconUtils::themedIcon(QStringLiteral("media-skip-forward")), i18n("Forward 1 Frame"), this);
    monitorSeekForwardOneFrame->setShortcut(Qt::Key_Right);
    pCore->window()->addAction(QStringLiteral("monitor_seek_forward-one-frame"), monitorSeekForwardOneFrame);
    connect(monitorSeekForwardOneFrame, &QAction::triggered, this, &MonitorManager::slotForwardOneFrame);

    QAction *monitorSeekForwardOneSecond = new QAction(KoIconUtils::themedIcon(QStringLiteral("media-skip-forward")), i18n("Forward 1 Second"), this);
    monitorSeekForwardOneSecond->setShortcut(Qt::SHIFT + Qt::Key_Right);
    pCore->window()->addAction(QStringLiteral("monitor_seek_forward-one-second"), monitorSeekForwardOneSecond);
    connect(monitorSeekForwardOneSecond, &QAction::triggered, this, &MonitorManager::slotForwardOneSecond);

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
    pCore->window()->addAction(QStringLiteral("mlt_interlace"), interlace);
    connect(interlace, static_cast<void (KSelectAction::*)(int)>(&KSelectAction::triggered), this, &MonitorManager::slotSetDeinterlacer);

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
    pCore->window()->addAction(QStringLiteral("mlt_interpolation"), interpol);
    connect(interpol, static_cast<void (KSelectAction::*)(int)>(&KSelectAction::triggered), this, &MonitorManager::slotSetInterpolation);

    QAction *zoneStart = new QAction(KoIconUtils::themedIcon(QStringLiteral("media-seek-backward")), i18n("Go to Zone Start"), this);
    zoneStart->setShortcut(Qt::SHIFT + Qt::Key_I);
    pCore->window()->addAction(QStringLiteral("seek_zone_start"), zoneStart);
    connect(zoneStart, &QAction::triggered, this, &MonitorManager::slotZoneStart);

    m_muteAction = new KDualAction(i18n("Mute monitor"), i18n("Unmute monitor"), this);
    m_muteAction->setActiveIcon(KoIconUtils::themedIcon(QStringLiteral("audio-volume-medium")));
    m_muteAction->setInactiveIcon(KoIconUtils::themedIcon(QStringLiteral("audio-volume-muted")));
    pCore->window()->addAction(QStringLiteral("mlt_mute"), m_muteAction);
    connect(m_muteAction, &KDualAction::activeChanged, this, &MonitorManager::slotMuteCurrentMonitor);

    QAction *zoneEnd = new QAction(KoIconUtils::themedIcon(QStringLiteral("media-seek-forward")), i18n("Go to Zone End"), this);
    zoneEnd->setShortcut(Qt::SHIFT + Qt::Key_O);
    pCore->window()->addAction(QStringLiteral("seek_zone_end"), zoneEnd);
    connect(zoneEnd, &QAction::triggered, this, &MonitorManager::slotZoneEnd);

    QAction *markIn = new QAction(KoIconUtils::themedIcon(QStringLiteral("zone-in")), i18n("Set Zone In"), this);
    markIn->setShortcut(Qt::Key_I);
    pCore->window()->addAction(QStringLiteral("mark_in"), markIn);
    connect(markIn, &QAction::triggered, this, &MonitorManager::slotSetInPoint);

    QAction *markOut = new QAction(KoIconUtils::themedIcon(QStringLiteral("zone-out")), i18n("Set Zone Out"), this);
    markOut->setShortcut(Qt::Key_O);
    pCore->window()-> addAction(QStringLiteral("mark_out"), markOut);
    connect(markOut, &QAction::triggered, this, &MonitorManager::slotSetOutPoint);
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
        QIcon newIcon = KoIconUtils::themedIcon(ic.name());
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
        m_projectMonitor->slotSetZoneStart();
    }
}

void MonitorManager::slotSetOutPoint()
{
    if (m_activeMonitor == m_clipMonitor) {
        m_clipMonitor->slotSetZoneEnd();
    } else if (m_activeMonitor == m_projectMonitor) {
        // Zone end behaves slightly differently in clip monitor and timeline monitor.
        // in timeline, set zone end selects the frame before current cursor, but in clip monitor
        // it selects frame at current cursor position.
        m_projectMonitor->slotSetZoneEnd(true);
    }
}

QDir MonitorManager::getCacheFolder(CacheType type)
{
    bool ok = false;
    if (m_document) {
        return m_document->getCacheDir(type, &ok);
    }
    return QDir();
}
