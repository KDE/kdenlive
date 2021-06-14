/***************************************************************************
 *   Copyright (C) 2017 by Nicolas Carion                                  *
 *   This file is part of Kdenlive. See www.kdenlive.org.                  *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) version 3 or any later version accepted by the       *
 *   membership of KDE e.V. (or its successor approved  by the membership  *
 *   of KDE e.V.), which shall act as a proxy defined in Section 14 of     *
 *   version 3 of the license.                                             *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>. *
 ***************************************************************************/

#include "timelinetabs.hpp"
#include "assets/model/assetparametermodel.hpp"
#include "core.h"
#include "mainwindow.h"
#include "monitor/monitor.h"
#include "monitor/monitormanager.h"
#include "project/projectmanager.h"
#include "timelinecontroller.h"
#include "timelinewidget.h"

TimelineContainer::TimelineContainer(QWidget *parent)
    :QWidget(parent)
{
}

QSize TimelineContainer::sizeHint() const
{
    return QSize(800, pCore->window()->height() / 2);
}

TimelineTabs::TimelineTabs(QWidget *parent)
    : QTabWidget(parent)
    , m_activeTimeline(nullptr)
{
    setTabBarAutoHide(true);
    setTabsClosable(true);

    // Resize to 0 the size of the close button of the main timeline, so that the user cannot close it.
    /*if (tabBar()->tabButton(0, QTabBar::RightSide) != nullptr) {
        tabBar()->tabButton(0, QTabBar::RightSide)->resize(0, 0);
    }*/
    connect(this, &TimelineTabs::currentChanged, this, &TimelineTabs::connectCurrent);
    connect(this, &TimelineTabs::tabCloseRequested, this, &TimelineTabs::closeTimeline);
}

void TimelineTabs::raiseTimeline(const QUuid &uuid)
{
    for (int i = 0; i < count(); i++) {
        TimelineWidget *timeline = static_cast<TimelineWidget *>(widget(i));
        if (timeline->uuid == uuid) {
            setCurrentIndex(i);
            break;
        }
    }
}

void TimelineTabs::setModified(const QUuid &uuid, bool modified)
{
    for (int i = 0; i < count(); i++) {
        TimelineWidget *timeline = static_cast<TimelineWidget *>(widget(i));
        if (timeline->uuid == uuid) {
            setTabIcon(i, modified ? QIcon::fromTheme(QStringLiteral("document-save")) : QIcon());
            break;
        }
    }
}

TimelineWidget *TimelineTabs::addTimeline(const QUuid &uuid, const QString &tabName, std::shared_ptr<TimelineItemModel> timelineModel, MonitorProxy *proxy)
{
    TimelineWidget *newTimeline = new TimelineWidget(uuid, this);
    newTimeline->setTimelineMenu(m_timelineClipMenu, m_timelineCompositionMenu, m_timelineMenu, m_guideMenu, m_timelineRulerMenu, m_editGuideAction, m_headerMenu, m_thumbsMenu, m_timelineSubtitleClipMenu);
    newTimeline->setModel(timelineModel, proxy);
    addTab(newTimeline, tabName);
    return newTimeline;
}

void TimelineTabs::connectCurrent(int ix)
{
    qDebug()<<"==== SWITCHING CURRENT TIMELINE TO: "<<ix;
    QUuid previousTab = QUuid();
    if (m_activeTimeline) {
        previousTab = m_activeTimeline->uuid;
        pCore->window()->disconnectTimeline(m_activeTimeline);
        disconnectTimeline(m_activeTimeline);
    } else {
        qDebug()<<"==== NO PRECIOUS TIMELINE";
    }
    m_activeTimeline = static_cast<TimelineWidget *>(widget(ix));
    connectTimeline(m_activeTimeline);
    pCore->window()->connectTimeline();
    if (previousTab != QUuid()) {
        pCore->bin()->updatePlaylistClip(previousTab, m_activeTimeline->uuid);
    }
}

void TimelineTabs::closeTimeline(int ix)
{
    TimelineWidget *timeline = static_cast<TimelineWidget *>(widget(ix));
    const QUuid uuid = timeline->uuid;
    bool lastTab = count() == 1;
    if (pCore->projectManager()->closeDocument(uuid, lastTab)) {
        if (!lastTab) {
            removeTab(ix);
            delete timeline;
        }
    }
}

TimelineTabs::~TimelineTabs()
{
    // clear source
    for (int i = 0; i < count(); i++) {
        TimelineWidget *timeline = static_cast<TimelineWidget *>(widget(i));
        timeline->setSource(QUrl());
    };
}

TimelineWidget *TimelineTabs::getCurrentTimeline() const
{
    return m_activeTimeline;
}

void TimelineTabs::closeTimelines()
{
    for (int i = 0; i < count(); i++) {
        static_cast<TimelineWidget *>(widget(i))->unsetModel();
    }
}

void TimelineTabs::connectTimeline(TimelineWidget *timeline)
{
    connect(timeline, &TimelineWidget::focusProjectMonitor, pCore->monitorManager(), &MonitorManager::focusProjectMonitor, Qt::DirectConnection);
    connect(this, &TimelineTabs::audioThumbFormatChanged, timeline->controller(), &TimelineController::audioThumbFormatChanged);
    connect(this, &TimelineTabs::showThumbnailsChanged, timeline->controller(), &TimelineController::showThumbnailsChanged);
    connect(this, &TimelineTabs::showAudioThumbnailsChanged, timeline->controller(), &TimelineController::showAudioThumbnailsChanged);
    connect(this, &TimelineTabs::changeZoom, timeline, &TimelineWidget::slotChangeZoom);
    connect(this, &TimelineTabs::fitZoom, timeline, &TimelineWidget::slotFitZoom);
    connect(timeline->controller(), &TimelineController::showTransitionModel, this, &TimelineTabs::showTransitionModel);
    connect(timeline->controller(), &TimelineController::showMixModel, this, &TimelineTabs::showMixModel);
    connect(timeline->controller(), &TimelineController::updateZoom, this, [&](double value) { emit updateZoom(getCurrentTimeline()->zoomForScale(value)); });
    connect(timeline->controller(), &TimelineController::showItemEffectStack, this, &TimelineTabs::showItemEffectStack);
    connect(timeline->controller(), &TimelineController::showSubtitle, this, &TimelineTabs::showSubtitle);
    connect(timeline->controller(), &TimelineController::centerView, timeline, &TimelineWidget::slotCenterView);

    connect(pCore->monitorManager()->projectMonitor(), &Monitor::zoneUpdated, m_activeTimeline, &TimelineWidget::zoneUpdated);
    connect(pCore->monitorManager()->projectMonitor(), &Monitor::zoneUpdatedWithUndo, m_activeTimeline, &TimelineWidget::zoneUpdatedWithUndo);
    connect(m_activeTimeline, &TimelineWidget::zoneMoved, pCore->monitorManager()->projectMonitor(), &Monitor::slotLoadClipZone);
    connect(pCore->monitorManager()->projectMonitor(), &Monitor::addEffect, m_activeTimeline->controller(), &TimelineController::addEffectToCurrentClip);
}

void TimelineTabs::disconnectTimeline(TimelineWidget *timeline)
{
    disconnect(timeline, &TimelineWidget::focusProjectMonitor, pCore->monitorManager(), &MonitorManager::focusProjectMonitor);
    disconnect(timeline->controller(), &TimelineController::durationChanged, pCore->projectManager(), &ProjectManager::adjustProjectDuration);
    disconnect(this, &TimelineTabs::audioThumbFormatChanged, timeline->controller(), &TimelineController::audioThumbFormatChanged);
    disconnect(this, &TimelineTabs::showThumbnailsChanged, timeline->controller(), &TimelineController::showThumbnailsChanged);
    disconnect(this, &TimelineTabs::showAudioThumbnailsChanged, timeline->controller(), &TimelineController::showAudioThumbnailsChanged);
    disconnect(this, &TimelineTabs::changeZoom, timeline, &TimelineWidget::slotChangeZoom);
    disconnect(timeline->controller(), &TimelineController::showTransitionModel, this, &TimelineTabs::showTransitionModel);
    disconnect(timeline->controller(), &TimelineController::showMixModel, this, &TimelineTabs::showMixModel);
    disconnect(timeline->controller(), &TimelineController::showItemEffectStack, this, &TimelineTabs::showItemEffectStack);
    disconnect(timeline->controller(), &TimelineController::showSubtitle, this, &TimelineTabs::showSubtitle);
}

void TimelineTabs::setTimelineMenu(QMenu *clipMenu, QMenu *compositionMenu, QMenu *timelineMenu, QMenu *guideMenu, QMenu *timelineRulerMenu, QAction *editGuideAction, QMenu *headerMenu, QMenu *thumbsMenu, QMenu *subtitleClipMenu)
{
    m_timelineClipMenu = clipMenu;
    m_timelineCompositionMenu = compositionMenu;
    m_timelineMenu = timelineMenu;
    m_timelineRulerMenu = timelineRulerMenu;
    m_guideMenu = guideMenu;
    m_headerMenu = headerMenu;
    m_thumbsMenu = thumbsMenu;
    m_headerMenu->addMenu(m_thumbsMenu);
    m_timelineSubtitleClipMenu = subtitleClipMenu;
    m_editGuideAction = editGuideAction;
}
