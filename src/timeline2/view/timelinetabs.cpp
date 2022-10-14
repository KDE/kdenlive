/*
    SPDX-FileCopyrightText: 2017 Nicolas Carion
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "timelinetabs.hpp"
#include "assets/model/assetparametermodel.hpp"
#include "core.h"
#include "mainwindow.h"
#include "monitor/monitor.h"
#include "monitor/monitormanager.h"
#include "project/projectmanager.h"
#include "timelinecontroller.h"
#include "timelinewidget.h"

#include <QMenu>

TimelineContainer::TimelineContainer(QWidget *parent)
    : QWidget(parent)
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
    connect(this, &TimelineTabs::tabCloseRequested, this, &TimelineTabs::closeTimelineByIndex);
}

bool TimelineTabs::raiseTimeline(const QUuid &uuid)
{
    for (int i = 0; i < count(); i++) {
        TimelineWidget *timeline = static_cast<TimelineWidget *>(widget(i));
        if (timeline->uuid == uuid) {
            setCurrentIndex(i);
            return true;
        }
    }
    return false;
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
    newTimeline->setTimelineMenu(m_timelineClipMenu, m_timelineCompositionMenu, m_timelineMenu, m_guideMenu, m_timelineRulerMenu, m_editGuideAction,
                                 m_headerMenu, m_thumbsMenu, m_timelineSubtitleClipMenu);
    newTimeline->setModel(timelineModel, proxy);
    addTab(newTimeline, tabName);
    return newTimeline;
}

void TimelineTabs::connectCurrent(int ix)
{
    qDebug() << "==== SWITCHING CURRENT TIMELINE TO: " << ix;
    QUuid previousTab = QUuid();
    int duration = 0;
    if (m_activeTimeline && m_activeTimeline->model()) {
        previousTab = m_activeTimeline->uuid;
        m_activeTimeline->model()->updateDuration();
        duration = m_activeTimeline->model()->duration();
        pCore->window()->disconnectTimeline(m_activeTimeline);
        disconnectTimeline(m_activeTimeline);
    } else {
        qDebug() << "==== NO PRECIOUS TIMELINE";
    }
    if (ix < 0 || ix >= count()) {
        m_activeTimeline = nullptr;
        return;
    }
    m_activeTimeline = static_cast<TimelineWidget *>(widget(ix));
    if (m_activeTimeline->model() == nullptr || m_activeTimeline->model()->m_closing) {
        // Closing app
        qDebug() << "++++++++++++\n\nCLOSING APP\n\n+++++++++++++";
        return;
    }
    qDebug() << "==== CONNECT NEW TIMELINE, MODEL:" << m_activeTimeline->model()->getTracksCount();
    connectTimeline(m_activeTimeline);
    pCore->window()->connectTimeline();
    if (previousTab != QUuid()) {
        pCore->bin()->updatePlaylistClip(previousTab, duration, m_activeTimeline->uuid);
    }
}

void TimelineTabs::renameTab(const QUuid &uuid, const QString &name)
{
    qDebug() << "==== READY TO RENAME!!!!!!!!!";
    for (int i = 0; i < count(); i++) {
        if (static_cast<TimelineWidget *>(widget(i))->uuid == uuid) {
            tabBar()->setTabText(i, name);
            pCore->projectManager()->setTimelinePropery(uuid, QStringLiteral("kdenlive:clipname"), name);
            break;
        }
    }
}

void TimelineTabs::closeTimelineByIndex(int ix)
{
    TimelineWidget *timeline = static_cast<TimelineWidget *>(widget(ix));
    timeline->setSource(QUrl());
    const QUuid uuid = timeline->uuid;
    timeline->blockSignals(true);
    pCore->window()->disconnectTimeline(timeline);
    disconnectTimeline(timeline);
    pCore->projectManager()->closeTimeline(uuid);
    if (m_activeTimeline == timeline) {
        m_activeTimeline = nullptr;
    }
    delete timeline;
    removeTab(ix);
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

void TimelineTabs::closeTimeline(const QUuid &uuid)
{
    for (int i = 0; i < count(); i++) {
        TimelineWidget *timeline = static_cast<TimelineWidget *>(widget(i));
        if (uuid == timeline->uuid) {
            if (m_activeTimeline == timeline) {
                pCore->window()->disconnectTimeline(m_activeTimeline);
                disconnectTimeline(m_activeTimeline);
                m_activeTimeline = nullptr;
            }
            timeline->unsetModel();
            removeTab(i);
            delete timeline;
            break;
        }
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
    connect(timeline->controller(), &TimelineController::updateAssetPosition, this, &TimelineTabs::updateAssetPosition);
    connect(timeline->controller(), &TimelineController::centerView, timeline, &TimelineWidget::slotCenterView);

    /*connect(pCore->monitorManager()->projectMonitor(), &Monitor::zoneUpdated, m_activeTimeline, &TimelineWidget::zoneUpdated);
    connect(pCore->monitorManager()->projectMonitor(), &Monitor::zoneUpdatedWithUndo, m_activeTimeline, &TimelineWidget::zoneUpdatedWithUndo);
    connect(m_activeTimeline, &TimelineWidget::zoneMoved, pCore->monitorManager()->projectMonitor(), &Monitor::slotLoadClipZone);
    connect(pCore->monitorManager()->projectMonitor(), &Monitor::addEffect, m_activeTimeline->controller(), &TimelineController::addEffectToCurrentClip);*/
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
    disconnect(timeline->controller(), &TimelineController::updateAssetPosition, this, &TimelineTabs::updateAssetPosition);
    // delete timeline;
}

void TimelineTabs::setTimelineMenu(QMenu *clipMenu, QMenu *compositionMenu, QMenu *timelineMenu, QMenu *guideMenu, QMenu *timelineRulerMenu,
                                   QAction *editGuideAction, QMenu *headerMenu, QMenu *thumbsMenu, QMenu *subtitleClipMenu)
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
