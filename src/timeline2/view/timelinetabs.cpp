/*
    SPDX-FileCopyrightText: 2017 Nicolas Carion
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "timelinetabs.hpp"
#include "assets/model/assetparametermodel.hpp"
#include "audiomixer/mixermanager.hpp"
#include "bin/projectclip.h"
#include "bin/projectitemmodel.h"
#include "core.h"
#include "doc/kdenlivedoc.h"
#include "mainwindow.h"
#include "monitor/monitor.h"
#include "monitor/monitormanager.h"
#include "monitor/monitorproxy.h"
#include "project/projectmanager.h"
#include "timelinecontroller.h"
#include "timelinewidget.h"

#include <KActionCollection>
#include <KMessageBox>
#include <QAction>
#include <QInputDialog>
#include <QPainter>
#include <QSignalBlocker>

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
    setCornerWidget(pb);
    connect(this, &TimelineTabs::currentChanged, this, &TimelineTabs::connectCurrent);
    connect(this, &TimelineTabs::tabCloseRequested, this, &TimelineTabs::closeTimelineByIndex);
    connect(tabBar(), &QTabBar::tabBarDoubleClicked, this, &TimelineTabs::onTabBarDoubleClicked);
    connect(pCore.get(), &Core::saveTimelinePreview, this, &TimelineTabs::saveTimelinePreview);
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
    if (m_activeTimeline == nullptr || pCore->currentDoc()->closing) {
        return;
    }
    if (count() == 1 && pCore->projectItemModel()->sequenceCount() > 1) {
        pCore->window()->setWindowTitle(pCore->currentDoc()->description(KLocalizedString::removeAcceleratorMarker(tabText(0))));
        m_activeTimeline->model()->updateVisibleSequenceName(tabText(0));
    } else {
        pCore->window()->setWindowTitle(pCore->currentDoc()->description());
        m_activeTimeline->model()->updateVisibleSequenceName(QString());
    }
}

bool TimelineTabs::raiseTimeline(const QUuid &uuid)
{
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

int TimelineTabs::getTimelineIndex(const QUuid &uuid)
{
    for (int i = 0; i < count(); i++) {
        TimelineWidget *timeline = static_cast<TimelineWidget *>(widget(i));
        if (timeline->getUuid() == uuid) {
            return i;
        }
    }
    return -1;
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

TimelineWidget *TimelineTabs::addTimeline(const QUuid uuid, int ix, const QString &tabName, std::shared_ptr<TimelineItemModel> timelineModel,
                                          MonitorProxy *proxy, bool openInMonitor, bool previewEnabled)
{
    QMutexLocker lk(&m_lock);
    if (count() == 1 && m_activeTimeline) {
        m_activeTimeline->model()->updateVisibleSequenceName(QString());
    }
    disconnect(this, &TimelineTabs::currentChanged, this, &TimelineTabs::connectCurrent);
    TimelineWidget *newTimeline = new TimelineWidget(uuid, this);
    auto *controller = newTimeline->controller();
    connect(controller, &TimelineController::selectionStateChanged, this, [this, controller](const TimelineController::SelectionState &state) {
        if (controller == activeController()) {
            Q_EMIT selectionStateChanged(state);
        }
    });
    Q_EMIT timelineCreated(newTimeline);
    if (m_actions) {
        newTimeline->populateActions(m_actions);
    }
    newTimeline->setModel(timelineModel, proxy, previewEnabled);
    int newIndex = 0;
    if (ix == -1 || ix >= count()) {
        newIndex = addTab(newTimeline, tabName);
    } else {
        newIndex = insertTab(ix, newTimeline, tabName);
    }
    setCurrentIndex(newIndex);
    setTabsClosable(count() > 1);
    lk.unlock();
    doConnectCurrent(newIndex, openInMonitor);
    connect(this, &TimelineTabs::currentChanged, this, &TimelineTabs::connectCurrent);
    return newTimeline;
}

void TimelineTabs::connectCurrent(int ix)
{
    doConnectCurrent(ix, true);
}

void TimelineTabs::doConnectCurrent(int ix, bool openInMonitor)
{
    QMutexLocker lk(&m_lock);
    Q_EMIT selectionStateChanged({});
    QUuid previousTab = QUuid();
    if (m_activeTimeline && m_activeTimeline->model()) {
        previousTab = m_activeTimeline->getUuid();
        pCore->window()->disableMulticam();
        if (openInMonitor && !pCore->currentDoc()->loading) {
            if (pCore->isMediaCapturing()) {
                pCore->switchCapture();
            } else if (pCore->isMediaMonitoring()) {
                pCore->setAudioMonitoring(false);
            }
            int pos = pCore->getMonitorPosition();
            m_activeTimeline->model()->updateDuration();
            std::pair<int, int> durations = m_activeTimeline->model()->durations();
            qDebug() << "::::: GOT SEQUENCES DURATIONS: " << durations;
            if (durations.second > 0) {
                int previousIndex = getTimelineIndex(previousTab);
                const QString seqName = KLocalizedString::removeAcceleratorMarker(tabText(previousIndex));
                // A sequence was made shorter, this will resize its instance in other sequences. Warn user
                if (KMessageBox::questionTwoActions(this,
                                                    i18n("The timeline sequence <b>%1</b> was shortened.<br/>Resize all instances in other timelines ?<br>Not "
                                                         "resizing will temporarily keep the current duration in all other sequences.",
                                                         seqName),
                                                    {}, KGuiItem(i18nc("@action:button", "Resize")),
                                                    KGuiItem(i18nc("@action:button", "Don't Resize"))) == KMessageBox::PrimaryAction) {
                    durations.second = 0;
                }
            }
            pCore->bin()->updateSequenceClip(previousTab, durations, pos);
        }
        pCore->window()->disconnectTimeline(m_activeTimeline);
        disconnectTimeline(m_activeTimeline);
    } else {
        qDebug() << "==== NO PREVIOUS TIMELINE";
    }
    if (ix < 0 || ix >= count() || pCore->currentDoc()->closing) {
        m_activeTimeline = nullptr;
        updatePreviewAction();
        qDebug() << "==== ABORTING NO TIMELINE AVAILABLE";
        return;
    }
    m_activeTimeline = static_cast<TimelineWidget *>(widget(ix));
    if (m_activeTimeline->model() == nullptr || m_activeTimeline->model()->m_closing) {
        // Closing app
        qDebug() << "++++++++++++\n\nCLOSING APP\n\n+++++++++++++";
        return;
    }
    if (openInMonitor) {
        pCore->window()->connectTimeline();
        connectTimeline(m_activeTimeline);
        updateWindowTitle();
        if (!m_activeTimeline->model()->isLoading) {
            pCore->bin()->sequenceActivated();
            pCore->projectManager()->polishTimelines({m_activeTimeline->getUuid()});
        }
        // Wait a few milliseconds to allow for the qml view to display
        pCore->monitorManager()->projectMonitor()->refreshMonitorTimer.start();
    } else {
        connectTimeline(m_activeTimeline);
    }
    publishSelectionState();
}

void TimelineTabs::renameTab(const QUuid &uuid, const QString &name)
{
    qDebug() << "==== READY TO RENAME!!!!!!!!!";
    for (int i = 0; i < count(); i++) {
        if (static_cast<TimelineWidget *>(widget(i))->getUuid() == uuid) {
            tabBar()->setTabText(i, name);
            pCore->projectManager()->setTimelineProperty(uuid, QStringLiteral("kdenlive:clipname"), name);
            updateWindowTitle();
            break;
        }
    }
}

void TimelineTabs::closeTimelineByIndex(int ix)
{
    TimelineWidget *timeline = static_cast<TimelineWidget *>(widget(ix));
    if (timeline == m_activeTimeline) {
        Q_EMIT selectionStateChanged({});
        Q_EMIT timeline->model()->requestClearAssetView(-1);
        pCore->clearTimeRemap();
        pCore->mixer()->unsetModel();
        pCore->window()->disableMulticam();
        m_activeTimeline->model()->updateDuration();
        // timeline->controller()->saveSequenceProperties();
    }
    const QString seqName = KLocalizedString::removeAcceleratorMarker(tabText(ix));
    std::shared_ptr<TimelineItemModel> model = timeline->model();
    const QUuid uuid = timeline->getUuid();
    const QString id = pCore->projectItemModel()->getSequenceId(uuid);
    Fun undo = [uuid, id, model]() { return pCore->projectManager()->openTimeline(id, -1, uuid, -1, false, model); };
    Fun redo = [this, ix, uuid]() {
        TimelineWidget *timeline = static_cast<TimelineWidget *>(widget(ix));
        if (timeline == m_activeTimeline) {
            Q_EMIT selectionStateChanged({});
        }
        pCore->projectManager()->closeTimeline(uuid, false, false);
        removeTab(ix);
        timeline->blockSignals(true);
        if (timeline == m_activeTimeline) {
            pCore->window()->disconnectTimeline(timeline);
            disconnectTimeline(timeline);
        }
        if (m_activeTimeline == timeline) {
            m_activeTimeline = nullptr;
        }
        delete timeline;
        publishSelectionState();
        updatePreviewAction();
        updateWindowTitle();
        return true;
    };
    redo();
    pCore->pushUndo(undo, redo, i18n("Close %1", seqName));
}

TimelineWidget *TimelineTabs::getCurrentTimeline() const
{
    return m_activeTimeline;
}

void TimelineTabs::closeTimelineTab(const QUuid uuid, bool checkActiveClosed)
{
    QMutexLocker lk(&m_lock);
    int currentCount = count();
    bool activeTimelineClosed = false;
    disconnect(this, &TimelineTabs::currentChanged, this, &TimelineTabs::connectCurrent);
    bool closing = pCore->currentDoc()->closing;
    for (int i = 0; i < currentCount; i++) {
        TimelineWidget *timeline = static_cast<TimelineWidget *>(widget(i));
        if (uuid == timeline->getUuid()) {
            removeTab(i);
            timeline->blockSignals(true);
            if (timeline == m_activeTimeline) {
                activeTimelineClosed = true;
                Q_EMIT showSubtitle(-1);
                pCore->window()->disconnectTimeline(timeline, closing);
                disconnectTimeline(timeline);
                m_activeTimeline = nullptr;
                publishSelectionState();
                updatePreviewAction();
            }
            delete timeline;
            // pCore->projectManager()->closeTimeline(uuid);
            setTabsClosable(count() > 1);
            if (currentCount == 2) {
                updateWindowTitle();
            }
            break;
        }
    }
    lk.unlock();
    if (closing) {
        // We are closing document, no need to reconnect
        return;
    }
    connect(this, &TimelineTabs::currentChanged, this, &TimelineTabs::connectCurrent);
    // if the tab is being closed by an undo action,
    // we need to trigger the connection of the remaining tab, as the undo stack won't trigger a currentChanged signal
    if (checkActiveClosed && activeTimelineClosed && count() > 0) {
        connectCurrent(currentIndex());
    }
}

void TimelineTabs::publishSelectionState()
{
    const auto *controller = activeController();
    Q_EMIT selectionStateChanged(controller ? controller->selectionState() : TimelineController::SelectionState{});
}

void TimelineTabs::updatePreviewAction()
{
    if (!m_actions) {
        return;
    }
    TimelineController *controller = activeController();
    QAction *action = m_actions->action(QStringLiteral("disable_preview"));
    const QSignalBlocker blocker(action);
    action->setEnabled(controller && m_activeTimeline->model()->hasTimelinePreview());
    action->setChecked(controller && controller->previewDisabled());
}

void TimelineTabs::connectTimeline(TimelineWidget *timeline)
{
    int position = pCore->currentDoc()->getSequenceProperty(timeline->getUuid(), QStringLiteral("position"), QString::number(0)).toInt();
    pCore->monitorManager()->projectMonitor()->getControllerProxy()->setCursorPosition(position);
    connect(timeline->controller(), &TimelineController::previewDisabledStateChanged, this, &TimelineTabs::updatePreviewAction, Qt::UniqueConnection);
    updatePreviewAction();
    connect(timeline, &TimelineWidget::focusProjectMonitor, pCore->monitorManager(), &MonitorManager::focusProjectMonitor, Qt::DirectConnection);
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
    QQmlEngine::setObjectOwnership(pCore->monitorManager()->projectMonitor()->getControllerProxy(), QQmlEngine::CppOwnership);
    Q_EMIT timeline->controller()->selectionChanged();
    timeline->setEnabled(true);
    timeline->setMouseTracking(true);
    timeline->focusTimeline();
}

void TimelineTabs::disconnectTimeline(TimelineWidget *timeline)
{
    timeline->setEnabled(false);
    timeline->setMouseTracking(false);
    disconnect(timeline->controller(), &TimelineController::previewDisabledStateChanged, this, &TimelineTabs::updatePreviewAction);
    disconnect(timeline, &TimelineWidget::focusProjectMonitor, pCore->monitorManager(), &MonitorManager::focusProjectMonitor);
    disconnect(this, &TimelineTabs::changeZoom, timeline, &TimelineWidget::slotChangeZoom);
    disconnect(this, &TimelineTabs::fitZoom, timeline, &TimelineWidget::slotFitZoom);
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

TimelineController *TimelineTabs::activeController() const
{
    if (m_activeTimeline == nullptr || pCore->currentDoc() == nullptr || pCore->currentDoc()->closing) {
        return nullptr;
    }
    const auto model = m_activeTimeline->model();
    if (model == nullptr || model->m_closing) {
        return nullptr;
    }
    return m_activeTimeline->controller();
}

void TimelineTabs::populateActions(KActionCollection *actions)
{
    Q_ASSERT(actions);
    Q_ASSERT(!m_actions);
    if (m_actions) {
        return;
    }
    m_actions = actions;

    // Shared commands are connected once; their target is resolved at execution, never captured from a tab.
    const auto bind = [this, actions](const QString &id, auto command) {
        QAction *action = actions->action(id);
        Q_ASSERT(action);
        connect(action, &QAction::triggered, this, [this, command]() {
            if (TimelineController *controller = activeController()) {
                command(controller);
            }
        });
    };
    bind(QStringLiteral("delete_timeline_selection"), [](TimelineController *controller) { controller->deleteSelectedClips(); });
    bind(QStringLiteral("audio_record"), [](TimelineController *controller) {
        if (pCore->isMediaMonitoring() || pCore->isMediaCapturing()) {
            controller->switchRecording();
        } else {
            pCore->displayMessage(i18n("Enable audio monitoring from the Mixer to record"), ErrorMessage);
        }
    });
    bind(QStringLiteral("mix_clip"), [](TimelineController *controller) { controller->mixClip(); });
    bind(QStringLiteral("delete_effects"), [](TimelineController *controller) { controller->deleteEffects(); });
    bind(QStringLiteral("expand_timeline_clip"), [](TimelineController *controller) { controller->expandActiveClip(); });
    bind(QStringLiteral("duplicate_timeline_clip"), [](TimelineController *controller) { controller->duplicateClip(); });
    bind(QStringLiteral("cut_timeline_clip"), [](TimelineController *controller) { controller->cutClipUnderCursor(); });
    bind(QStringLiteral("replace_timeline_clip"), [](TimelineController *controller) { controller->replaceClip(); });
    bind(QStringLiteral("cut_timeline_all_clips"), [](TimelineController *controller) { controller->cutAllClipsUnderCursor(); });
    bind(QStringLiteral("clip_split"), [](TimelineController *controller) { controller->splitAV(); });
    bind(QStringLiteral("clip_enable_all"), [](TimelineController *controller) { controller->setClipsEnabled(true); });
    bind(QStringLiteral("clip_disable_all"), [](TimelineController *controller) { controller->setClipsEnabled(false); });
    bind(QStringLiteral("extract_clip"), [](TimelineController *controller) { controller->extract(); });
    bind(QStringLiteral("save_to_bin"), [](TimelineController *controller) { controller->saveZone(); });
    bind(QStringLiteral("set_audio_align_ref"), [](TimelineController *controller) { controller->setAudioRef(); });
    bind(QStringLiteral("align_audio"), [](TimelineController *controller) { controller->alignAudio(); });
    bind(QStringLiteral("set_timecode_ref"), [](TimelineController *controller) { controller->setTimecodeRef(); });
    bind(QStringLiteral("align_timecode"), [](TimelineController *controller) { controller->alignTimecode(); });
    bind(QStringLiteral("edit_item_duration"), [](TimelineController *controller) { controller->editItemDuration(); });
    bind(QStringLiteral("edit_item_speed"), [](TimelineController *controller) { controller->changeItemSpeed(-1, -1); });
    bind(QStringLiteral("edit_item_remap"), [](TimelineController *controller) { controller->remapItemTime(-1); });
    bind(QStringLiteral("resize_timeline_clip_start"),
         [](TimelineController *controller) { controller->setInPoint(pCore->activeTool() == ToolType::RippleTool); });
    bind(QStringLiteral("resize_timeline_clip_end"),
         [](TimelineController *controller) { controller->setOutPoint(pCore->activeTool() == ToolType::RippleTool); });
    bind(QStringLiteral("paste_effects"), [](TimelineController *controller) { controller->pasteEffects(); });
    bind(QStringLiteral("group_clip"), [](TimelineController *controller) { controller->groupSelection(); });
    bind(QStringLiteral("ungroup_clip"), [](TimelineController *controller) { controller->unGroupSelection(); });

    bind(QStringLiteral("select_timeline_zone"), [](TimelineController *controller) { controller->setZoneToSelection(); });
    bind(QStringLiteral("select_timeline_clip"), [](TimelineController *controller) { controller->selectCurrentItem(KdenliveObjectType::TimelineClip, true); });
    bind(QStringLiteral("deselect_timeline_clip"),
         [](TimelineController *controller) { controller->selectCurrentItem(KdenliveObjectType::TimelineClip, false); });
    bind(QStringLiteral("select_add_timeline_clip"),
         [](TimelineController *controller) { controller->selectCurrentItem(KdenliveObjectType::TimelineClip, true, true); });
    bind(QStringLiteral("select_timeline_transition"), [](TimelineController *controller) {
        if (!controller->selectCurrentItem(KdenliveObjectType::TimelineComposition, true, false, false)) {
            controller->selectCurrentItem(KdenliveObjectType::TimelineMix, true);
        }
    });
    bind(QStringLiteral("deselect_timeline_transition"), [](TimelineController *controller) {
        if (!controller->selectCurrentItem(KdenliveObjectType::TimelineComposition, false, false, false)) {
            controller->selectCurrentItem(KdenliveObjectType::TimelineMix, false);
        }
    });
    bind(QStringLiteral("select_add_timeline_transition"),
         [](TimelineController *controller) { controller->selectCurrentItem(KdenliveObjectType::TimelineComposition, true, true); });
    bind(QStringLiteral("select_track"), [](TimelineController *controller) { controller->selectCurrentTrack(); });
    bind(QStringLiteral("unselect_all_tracks"), [this](TimelineController *) { m_activeTimeline->model()->requestClearSelection(); });
    bind(QStringLiteral("switch_target_stream"), [this](TimelineController *) { m_activeTimeline->showTargetMenu(); });
    for (int i = 1; i < 10; ++i) {
        const QString audioId = QStringLiteral("activate_audio_%1").arg(i);
        QAction *audioAction = actions->action(audioId);
        bind(audioId, [this, audioAction](TimelineController *controller) {
            const QList<int> trackIds = m_activeTimeline->model()->getTracksIds(true);
            if (!trackIds.isEmpty()) {
                const int trackPos = qBound(0, audioAction->data().toInt(), int(trackIds.count()) - 1);
                controller->setActiveTrack(trackIds.at(trackPos));
            }
        });
        const QString targetId = QStringLiteral("activate_target_%1").arg(i);
        QAction *targetAction = actions->action(targetId);
        bind(targetId, [targetAction](TimelineController *controller) { controller->assignCurrentTarget(targetAction->data().toInt()); });
    }

    bind(QStringLiteral("insert_space"), [](TimelineController *controller) { controller->insertSpace(); });
    bind(QStringLiteral("delete_space"), [](TimelineController *controller) { controller->removeSpace(-1, -1, false); });
    bind(QStringLiteral("delete_space_all_tracks"), [](TimelineController *controller) { controller->removeSpace(-1, -1, true); });
    bind(QStringLiteral("delete_all_spaces"), [](TimelineController *controller) { controller->removeTrackSpaces(-1, -1); });
    bind(QStringLiteral("delete_all_clips"), [](TimelineController *controller) { controller->removeTrackClips(-1, -1); });
    bind(QStringLiteral("switch_track_solo"), [](TimelineController *controller) { controller->switchSoloTrack(); });
    bind(QStringLiteral("add_sequence_marker"), [](TimelineController *controller) { controller->switchGuide(-1, false, true); });
    bind(QStringLiteral("edit_sequence_marker"), [](TimelineController *controller) { controller->editGuide(); });
    bind(QStringLiteral("delete_sequence_marker"), [](TimelineController *controller) { controller->switchGuide(-1, true); });
    bind(QStringLiteral("add_markers_at_gaps"), [](TimelineController *controller) { controller->addMarkersAtGaps(); });
    bind(QStringLiteral("identify_gaps_all_tracks"), [](TimelineController *controller) { controller->addMarkersAtGaps(); });
    bind(QStringLiteral("add_markers_at_gaps_on_track"), [](TimelineController *controller) { controller->addMarkersAtGapsOnTrack(); });

    bind(QStringLiteral("prerender_timeline_zone"), [](TimelineController *controller) { controller->startPreviewRender(); });
    bind(QStringLiteral("stop_prerender_timeline"), [](TimelineController *controller) { controller->stopPreviewRender(); });
    bind(QStringLiteral("set_render_timeline_zone"), [](TimelineController *controller) { controller->addPreviewRange(true); });
    bind(QStringLiteral("unset_render_timeline_zone"), [](TimelineController *controller) { controller->addPreviewRange(false); });
    bind(QStringLiteral("clear_render_timeline_zone"), [](TimelineController *controller) { controller->clearPreviewRange(true); });
    bind(QStringLiteral("disable_subtitle"), [](TimelineController *controller) { controller->switchSubtitleDisable(); });
    bind(QStringLiteral("lock_subtitle"), [](TimelineController *controller) { controller->switchSubtitleLock(); });
    bind(QStringLiteral("export_subtitle"), [](TimelineController *controller) { controller->exportSubtitle(); });
    connect(actions->action(QStringLiteral("disable_preview")), &QAction::triggered, this, [this](bool disabled) {
        if (TimelineController *controller = activeController()) {
            controller->setPreviewEnabled(!disabled);
        }
    });

    connect(pCore->monitorManager()->projectMonitor(), &Monitor::deleteMarker, this, [this]() {
        if (TimelineController *controller = activeController()) {
            controller->switchGuide(-1, true);
        }
    });
    connect(actions->action(QStringLiteral("sequence_next")), &QAction::triggered, this, &TimelineTabs::slotNextSequence);
    connect(actions->action(QStringLiteral("sequence_previous")), &QAction::triggered, this, &TimelineTabs::slotPreviousSequence);
    connect(static_cast<QToolButton *>(cornerWidget()), &QToolButton::clicked, actions->action(QStringLiteral("add_playlist_clip")), &QAction::trigger);

    for (int i = 0; i < count(); ++i) {
        static_cast<TimelineWidget *>(widget(i))->populateActions(actions);
    }
    publishSelectionState();
    updatePreviewAction();
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

void TimelineTabs::slotNextSequence()
{
    if (!activeController()) {
        return;
    }
    int max = count();
    int focus = currentIndex() + 1;
    if (focus >= max) {
        focus = 0;
    }
    setCurrentIndex(focus);
}

void TimelineTabs::slotPreviousSequence()
{
    if (!activeController()) {
        return;
    }
    int focus = currentIndex() - 1;
    if (focus < 0) {
        focus = count() - 1;
    }
    setCurrentIndex(focus);
}

void TimelineTabs::onTabBarDoubleClicked(int index)
{
    if (index == -1) {
        // No action when double clicking in empty space
        return;
    }
    const QString currentTabName = KLocalizedString::removeAcceleratorMarker(tabText(index));
    bool ok = false;
    const QString newName = QInputDialog::getText(this, i18n("Rename Sequence"), i18n("Rename Sequence"), QLineEdit::Normal, currentTabName, &ok);
    if (ok && !newName.isEmpty()) {
        TimelineWidget *timeline = static_cast<TimelineWidget *>(widget(index));
        if (timeline) {
            const QString id = pCore->projectItemModel()->getSequenceId(timeline->getUuid());
            std::shared_ptr<ProjectClip> clip = pCore->projectItemModel()->getClipByBinID(id);
            if (clip) {
                clip->rename(newName, 0);
            }
        }
    }
}

void TimelineTabs::saveTimelinePreview(const QString &path)
{
    int imageWidth = 600;
    int imageHeight = size().height() * imageWidth / size().width();
    QPixmap bitmap(size());
    QPainter painter(&bitmap);
    render(&painter);
    painter.end();
    QPixmap scaled = bitmap.scaled(imageWidth, imageHeight);
    scaled.save(path);
}
