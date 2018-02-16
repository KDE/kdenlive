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

TimelineTabs::TimelineTabs(QWidget *parent)
    : QTabWidget(parent)
    , m_mainTimeline(new TimelineWidget(pCore->window()->actionCollection(), this))
{
    setTabBarAutoHide(true);
    setTabsClosable(true);
    addTab(m_mainTimeline, i18n("Main timeline"));
    connectTimeline(m_mainTimeline);

    // Resize to 0 the size of the close button of the main timeline, so that the user cannnot close it.
    if (tabBar()->tabButton(0, QTabBar::RightSide) != nullptr) {
        tabBar()->tabButton(0, QTabBar::RightSide)->resize(0, 0);
    }
    connect(pCore->monitorManager()->projectMonitor(), &Monitor::zoneUpdated, m_mainTimeline, &TimelineWidget::zoneUpdated);
    connect(m_mainTimeline, &TimelineWidget::zoneMoved, pCore->monitorManager()->projectMonitor(), &Monitor::slotLoadClipZone);
    connect(pCore->monitorManager()->projectMonitor(), &Monitor::addEffect, m_mainTimeline->controller(), &TimelineController::addEffectToCurrentClip);
}

TimelineWidget *TimelineTabs::getMainTimeline() const
{
    return m_mainTimeline;
}

TimelineWidget *TimelineTabs::getCurrentTimeline() const
{
    return static_cast<TimelineWidget *>(currentWidget());
}

void TimelineTabs::connectTimeline(TimelineWidget *timeline)
{
    connect(pCore->monitorManager()->projectMonitor(), &Monitor::seekTimeline, timeline->controller(), &TimelineController::setPosition, Qt::DirectConnection);
    connect(timeline->controller(), &TimelineController::seeked, pCore->monitorManager()->projectMonitor(), &Monitor::requestSeek, Qt::DirectConnection);
    connect(pCore->monitorManager()->projectMonitor(), &Monitor::seekPosition, timeline->controller(), &TimelineController::onSeeked, Qt::DirectConnection);
    connect(timeline, &TimelineWidget::focusProjectMonitor, pCore->monitorManager(), &MonitorManager::focusProjectMonitor);
    connect(timeline, &TimelineWidget::zoomIn, pCore->window(), &MainWindow::slotZoomIn);
    connect(timeline, &TimelineWidget::zoomOut, pCore->window(), &MainWindow::slotZoomOut);
    connect(timeline->controller(), &TimelineController::durationChanged, pCore->projectManager(), &ProjectManager::adjustProjectDuration);

    connect(this, &TimelineTabs::audioThumbFormatChanged, timeline->controller(), &TimelineController::audioThumbFormatChanged);
    connect(this, &TimelineTabs::showThumbnailsChanged, timeline->controller(), &TimelineController::showThumbnailsChanged);
    connect(this, &TimelineTabs::showAudioThumbnailsChanged, timeline->controller(), &TimelineController::showAudioThumbnailsChanged);
    connect(this, &TimelineTabs::changeZoom, timeline, &TimelineWidget::slotChangeZoom);
    connect(timeline->controller(), &TimelineController::showTransitionModel, this, &TimelineTabs::showTransitionModel);
    connect(timeline->controller(), &TimelineController::showItemEffectStack, this, &TimelineTabs::showItemEffectStack);
}

void TimelineTabs::disconnectTimeline(TimelineWidget *timeline)
{
    disconnect(pCore->monitorManager()->projectMonitor(), &Monitor::seekTimeline, timeline->controller(), &TimelineController::setPosition);
    disconnect(timeline->controller(), &TimelineController::seeked, pCore->monitorManager()->projectMonitor(), &Monitor::requestSeek);
    disconnect(pCore->monitorManager()->projectMonitor(), &Monitor::seekPosition, timeline->controller(), &TimelineController::onSeeked);
    disconnect(timeline, &TimelineWidget::focusProjectMonitor, pCore->monitorManager(), &MonitorManager::focusProjectMonitor);
    disconnect(timeline, &TimelineWidget::zoomIn, pCore->window(), &MainWindow::slotZoomIn);
    disconnect(timeline, &TimelineWidget::zoomOut, pCore->window(), &MainWindow::slotZoomOut);
    disconnect(timeline->controller(), &TimelineController::durationChanged, pCore->projectManager(), &ProjectManager::adjustProjectDuration);

    disconnect(this, &TimelineTabs::audioThumbFormatChanged, timeline->controller(), &TimelineController::audioThumbFormatChanged);
    disconnect(this, &TimelineTabs::showThumbnailsChanged, timeline->controller(), &TimelineController::showThumbnailsChanged);
    disconnect(this, &TimelineTabs::showAudioThumbnailsChanged, timeline->controller(), &TimelineController::showAudioThumbnailsChanged);
    disconnect(this, &TimelineTabs::changeZoom, timeline, &TimelineWidget::slotChangeZoom);
    disconnect(timeline->controller(), &TimelineController::showTransitionModel, this, &TimelineTabs::showTransitionModel);
    disconnect(timeline->controller(), &TimelineController::showItemEffectStack, this, &TimelineTabs::showItemEffectStack);
}
