/*
    SPDX-FileCopyrightText: 2017 Nicolas Carion
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "timelinetabs.hpp"
#include "assets/model/assetparametermodel.hpp"
#include "audiomixer/mixermanager.hpp"
#include "bin/projectitemmodel.h"
#include "core.h"
#include "doc/docundostack.hpp"
#include "doc/kdenlivedoc.h"
#include "mainwindow.h"
#include "monitor/monitor.h"
#include "monitor/monitormanager.h"
#include "monitor/monitorproxy.h"
#include "project/projectmanager.h"
#include "timelinecontroller.h"
#include "timelinewidget.h"

#include <KMessageBox>
#include <QMenu>
#include <QQmlContext>

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
    setTabsClosable(false);
    setDocumentMode(true);
    setMovable(true);
    QToolButton *pb = new QToolButton(this);
    pb->setIcon(QIcon::fromTheme(QStringLiteral("list-add")));
    pb->setAutoRaise(true);
    pb->setToolTip(i18n("Add Timeline Sequence"));
    pb->setWhatsThis(
        i18n("Add Timeline Sequence. This will create a new timeline for editing. Each timeline corresponds to a Sequence Clip in the Project Bin"));
    connect(pb, &QToolButton::clicked, [=]() { pCore->triggerAction(QStringLiteral("add_playlist_clip")); });
    setCornerWidget(pb);
    connect(this, &TimelineTabs::currentChanged, this, &TimelineTabs::connectCurrent);
    connect(this, &TimelineTabs::tabCloseRequested, this, &TimelineTabs::closeTimelineByIndex);
}

TimelineTabs::~TimelineTabs()
{
    // clear source
    for (int i = 0; i < count(); i++) {
        TimelineWidget *timeline = static_cast<TimelineWidget *>(widget(i));
        timeline->setSource(QUrl());
    };
}

void TimelineTabs::updateWindowTitle()
{
    // Show current timeline name in Window title if we have multiple sequences but only one opened
    if (count() == 1 && pCore->projectItemModel()->sequenceCount() > 1) {
        pCore->window()->setWindowTitle(pCore->currentDoc()->description(KLocalizedString::removeAcceleratorMarker(tabBar()->tabText(currentIndex()))));
    } else {
        pCore->window()->setWindowTitle(pCore->currentDoc()->description());
    }
}

bool TimelineTabs::raiseTimeline(const QUuid &uuid)
{
    QMutexLocker lk(&m_lock);
    for (int i = 0; i < count(); i++) {
        TimelineWidget *timeline = static_cast<TimelineWidget *>(widget(i));
        if (timeline->getUuid() == uuid) {
            if (i != currentIndex()) {
                setCurrentIndex(i);
            }
            return true;
        }
    }
    return false;
}

void TimelineTabs::setModified(const QUuid &uuid, bool modified)
{
    for (int i = 0; i < count(); i++) {
        TimelineWidget *timeline = static_cast<TimelineWidget *>(widget(i));
        if (timeline->getUuid() == uuid) {
            setTabIcon(i, modified ? QIcon::fromTheme(QStringLiteral("document-save")) : QIcon());
            break;
        }
    }
}

TimelineWidget *TimelineTabs::addTimeline(const QUuid uuid, const QString &tabName, std::shared_ptr<TimelineItemModel> timelineModel, MonitorProxy *proxy)
{
    QMutexLocker lk(&m_lock);
    disconnect(this, &TimelineTabs::currentChanged, this, &TimelineTabs::connectCurrent);
    TimelineWidget *newTimeline = new TimelineWidget(uuid, this);
    newTimeline->setTimelineMenu(m_timelineClipMenu, m_timelineCompositionMenu, m_timelineMenu, m_guideMenu, m_timelineRulerMenu, m_editGuideAction,
                                 m_headerMenu, m_thumbsMenu, m_timelineSubtitleClipMenu);
    newTimeline->setModel(timelineModel, proxy);
    int newIndex = addTab(newTimeline, tabName);
    setCurrentIndex(newIndex);
    connectCurrent(newIndex);
    setTabsClosable(count() > 1);
    if (count() == 2) {
        updateWindowTitle();
    }
    connect(this, &TimelineTabs::currentChanged, this, &TimelineTabs::connectCurrent);
    return newTimeline;
}

void TimelineTabs::connectCurrent(int ix)
{
    QUuid previousTab = QUuid();
    if (m_activeTimeline && m_activeTimeline->model()) {
        previousTab = m_activeTimeline->getUuid();
        qDebug() << "===== DISCONNECTING PREVIOUS: " << previousTab;
        pCore->window()->disableMulticam();
        int pos = pCore->getMonitorPosition();
        m_activeTimeline->model()->updateDuration();
        int duration = m_activeTimeline->model()->duration();
        m_activeTimeline->controller()->saveSequenceProperties();
        pCore->bin()->updateSequenceClip(previousTab, duration, pos);
        pCore->window()->disconnectTimeline(m_activeTimeline);
        disconnectTimeline(m_activeTimeline);
    } else {
        qDebug() << "==== NO PREVIOUS TIMELINE";
    }
    if (ix < 0 || ix >= count()) {
        m_activeTimeline = nullptr;
        qDebug() << "==== ABORTING NO TIMELINE AVAILABLE";
        return;
    }
    m_activeTimeline = static_cast<TimelineWidget *>(widget(ix));
    if (m_activeTimeline->model() == nullptr || m_activeTimeline->model()->m_closing) {
        // Closing app
        qDebug() << "++++++++++++\n\nCLOSING APP\n\n+++++++++++++";
        return;
    }
    qDebug() << "==== CONNECT NEW TIMELINE, MODEL:" << m_activeTimeline->model()->getTracksCount();
    pCore->window()->connectTimeline();
    connectTimeline(m_activeTimeline);
    if (!m_activeTimeline->model()->isLoading) {
        pCore->bin()->updateTargets();
    }
}

void TimelineTabs::renameTab(const QUuid &uuid, const QString &name)
{
    qDebug() << "==== READY TO RENAME!!!!!!!!!";
    for (int i = 0; i < count(); i++) {
        if (static_cast<TimelineWidget *>(widget(i))->getUuid() == uuid) {
            tabBar()->setTabText(i, name);
            pCore->projectManager()->setTimelinePropery(uuid, QStringLiteral("kdenlive:clipname"), name);
            if (count() == 1) {
                updateWindowTitle();
            }
            break;
        }
    }
}

void TimelineTabs::closeTimelineByIndex(int ix)
{
    if (KMessageBox::warningContinueCancel(this, i18n("Closing a sequence will clear the undo history"), QString(), KStandardGuiItem::cont(),
                                           KStandardGuiItem::cancel(), QStringLiteral("clearHistoryOnSequenceClose")) != KMessageBox::Continue) {
        return;
    }
    // Closing a sequence currently causes many undo crashes, so disable for now
    QMutexLocker lk(&m_lock);
    bool closingCurrent = ix == currentIndex();
    const QString seqName = tabText(ix);
    TimelineWidget *timeline = static_cast<TimelineWidget *>(widget(ix));
    const QUuid uuid = timeline->getUuid();
    Fun redo = [this, ix, uuid]() {
        TimelineWidget *timeline = static_cast<TimelineWidget *>(widget(ix));
        if (timeline == m_activeTimeline) {
            Q_EMIT timeline->model()->requestClearAssetView(-1);
            pCore->clearTimeRemap();
            pCore->mixer()->unsetModel();
        }
        timeline->blockSignals(true);
        timeline->setSource(QUrl());
        if (timeline == m_activeTimeline) {
            pCore->window()->disconnectTimeline(timeline);
            disconnectTimeline(timeline);
        }
        timeline->unsetModel();
        if (m_activeTimeline == timeline) {
            m_activeTimeline = nullptr;
        }
        delete timeline;
        pCore->projectManager()->closeTimeline(uuid);
        setTabsClosable(count() > 1);
        if (count() == 1) {
            updateWindowTitle();
        }
        return true;
    };
    /*Fun undo = [this, uuid]() {
        const QString binId = pCore->bin()->sequenceBinId(uuid);
        if (!binId.isEmpty()) {
            return pCore->projectManager()->openTimeline(binId, uuid);
        }
        return false;
    };*/
    if (closingCurrent) {
        pCore->window()->disableMulticam();
        int pos = pCore->getMonitorPosition();
        m_activeTimeline->model()->updateDuration();
        int duration = m_activeTimeline->model()->duration();
        timeline->controller()->saveSequenceProperties();
    }
    redo();
    // pCore->pushUndo(undo, redo, i18n("Close %1", seqName));
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
    QMutexLocker lk(&m_lock);
    for (int i = 0; i < count(); i++) {
        TimelineWidget *timeline = static_cast<TimelineWidget *>(widget(i));
        if (uuid == timeline->getUuid()) {
            timeline->blockSignals(true);
            timeline->setSource(QUrl());
            timeline->blockSignals(true);
            if (timeline == m_activeTimeline) {
                Q_EMIT showSubtitle(-1);
                pCore->window()->disconnectTimeline(timeline);
                disconnectTimeline(timeline);
            }
            timeline->unsetModel();
            if (m_activeTimeline == timeline) {
                m_activeTimeline = nullptr;
            }
            delete timeline;
            pCore->projectManager()->closeTimeline(uuid);
            setTabsClosable(count() > 1);
            if (count() == 1) {
                updateWindowTitle();
            }
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
    connect(timeline->controller(), &TimelineController::updateZoom, this, [&](double value) { Q_EMIT updateZoom(getCurrentTimeline()->zoomForScale(value)); });
    connect(timeline->controller(), &TimelineController::showItemEffectStack, this, &TimelineTabs::showItemEffectStack);
    connect(timeline->controller(), &TimelineController::showSubtitle, this, &TimelineTabs::showSubtitle);
    connect(timeline->controller(), &TimelineController::updateAssetPosition, this, &TimelineTabs::updateAssetPosition);
    connect(timeline->controller(), &TimelineController::centerView, timeline, &TimelineWidget::slotCenterView);

    connect(pCore->monitorManager()->projectMonitor(), &Monitor::zoneUpdated, m_activeTimeline, &TimelineWidget::zoneUpdated);
    connect(pCore->monitorManager()->projectMonitor(), &Monitor::zoneUpdatedWithUndo, m_activeTimeline, &TimelineWidget::zoneUpdatedWithUndo);
    connect(m_activeTimeline, &TimelineWidget::zoneMoved, pCore->monitorManager()->projectMonitor(), &Monitor::slotLoadClipZone);
    connect(pCore->monitorManager()->projectMonitor(), &Monitor::addTimelineEffect, m_activeTimeline->controller(),
            &TimelineController::addEffectToCurrentClip);
    timeline->rootContext()->setContextProperty("proxy", pCore->monitorManager()->projectMonitor()->getControllerProxy());
}

void TimelineTabs::disconnectTimeline(TimelineWidget *timeline)
{
    timeline->rootContext()->setContextProperty("proxy", nullptr);
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

    disconnect(pCore->monitorManager()->projectMonitor(), &Monitor::zoneUpdated, timeline, &TimelineWidget::zoneUpdated);
    disconnect(pCore->monitorManager()->projectMonitor(), &Monitor::zoneUpdatedWithUndo, timeline, &TimelineWidget::zoneUpdatedWithUndo);
    disconnect(timeline, &TimelineWidget::zoneMoved, pCore->monitorManager()->projectMonitor(), &Monitor::slotLoadClipZone);
    disconnect(pCore->monitorManager()->projectMonitor(), &Monitor::addTimelineEffect, timeline->controller(), &TimelineController::addEffectToCurrentClip);
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

const QStringList TimelineTabs::openedSequences()
{
    QStringList result;
    for (int i = 0; i < count(); i++) {
        result << static_cast<TimelineWidget *>(widget(i))->getUuid().toString();
    }
    return result;
}

TimelineWidget *TimelineTabs::getTimeline(const QUuid uuid) const
{
    for (int i = 0; i < count(); i++) {
        TimelineWidget *tl = static_cast<TimelineWidget *>(widget(i));
        if (tl->getUuid() == uuid) {
            return tl;
        }
    }
    return nullptr;
}
