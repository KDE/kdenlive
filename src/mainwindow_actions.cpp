/*
    SPDX-FileCopyrightText: 2007 Jean-Baptiste Mardelle <jb@kdenlive.org>
    SPDX-FileCopyrightText: 2026 Nicolas Carion
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "assets/assetpanel.hpp"
#include "core.h"
#include "kdenlivesettings.h"
#include "mainwindow.h"
#include "monitor/monitor.h"
#include "monitor/monitormanager.h"
#include "project/projectmanager.h"
#include "timeline2/view/timelinecontroller.h"
#include "timeline2/view/timelinewidget.h"
#include "transitions/transitionsrepository.hpp"

#include <KActionCollection>
#include <KActionMenu>
#include <KColorSchemeMenu>
#include <KDualAction>
#include <KLocalizedString>
#include <KMessageBox>
#include <KStandardAction>
#include <KStyleManager>
#include <KToolBar>
#include <QActionGroup>
#include <QFontDatabase>
#include <QLabel>
#include <QMenu>
#include <QPushButton>
#include <QSlider>
#include <QStatusBar>
#include <QStyleFactory>
#include <QToolButton>
#include <QUndoGroup>
#include <QVBoxLayout>
#include <QWidgetAction>

void MainWindow::addAction(const QString &name, QAction *action, const QKeySequence &shortcut, KActionCategory *category)
{
    m_actionNames.append(name);
    if (category) {
        category->addAction(name, action);
    } else {
        actionCollection()->addAction(name, action);
    }
    actionCollection()->setDefaultShortcut(action, shortcut);
}

void MainWindow::addAction(const QString &name, QAction *action, const QKeySequence &shortcut, const QString &category)
{
    addAction(name, action, shortcut, kdenliveCategoryMap.value(category, nullptr));
}

QAction *MainWindow::addAction(const QString &name, const QString &text, const QIcon &icon, const QKeySequence &shortcut, KActionCategory *category)
{
    auto *action = new QAction(icon, text, this);
    addAction(name, action, shortcut, category);
    return action;
}

QAction *MainWindow::addAction(const QString &name, const QString &text, const QIcon &icon, const QKeySequence &shortcut, const QString &category)
{
    return addAction(name, text, icon, shortcut, kdenliveCategoryMap.value(category, nullptr));
}

QAction *MainWindow::addAction(const QString &name, const QString &text, const QObject *receiver, const char *member, const QIcon &icon,
                               const QKeySequence &shortcut, KActionCategory *category)
{
    auto *action = addAction(name, text, icon, shortcut, category);
    connect(action, SIGNAL(triggered(bool)), receiver, member);

    return action;
}

QAction *MainWindow::addAction(const QString &name, const QString &text, const QObject *receiver, const char *member, const QIcon &icon,
                               const QKeySequence &shortcut, const QString &category)
{
    return addAction(name, text, receiver, member, icon, shortcut, kdenliveCategoryMap.value(category, nullptr));
}

void MainWindow::setupActions()
{
    // Window actions are created before child widgets consume the collection.
    addAction(QStringLiteral("themes_menu"), KColorSchemeMenu::createMenu(KColorSchemeManager::instance(), this));
    QAction *stylesAction = KStyleManager::createConfigureAction(this);
    if (stylesAction->menu()) {
        const QStringList stylesToHide = {
            QStringLiteral("windowsvista"), // recoloring does not work well
            QStringLiteral("Windows"),
        };
        for (QAction *child : stylesAction->menu()->actions()) {
            if (stylesToHide.contains(child->data().toString(), Qt::CaseInsensitive)) {
                child->setVisible(false);
            }
        }
    }
    addAction(QStringLiteral("styles_menu"), stylesAction);

    QAction *cleanHistory = addAction(QStringLiteral("clear_undo_history"), i18n("Clear Undo History"), QIcon::fromTheme(QStringLiteral("edit-clear-history")));
    connect(cleanHistory, &QAction::triggered, this, [this]() {
        if (KMessageBox::warningContinueCancel(this, i18n("This will clear all undo history, you will not be able to undo any previous action.")) !=
            KMessageBox::Continue) {
            return;
        }
        const QList<QUndoStack *> stacks = m_commandStack->stacks();
        for (auto *stack : stacks) {
            stack->clear();
        }
    });

    QAction *switchAction = addAction(QStringLiteral("show_titlebars"), i18n("Show Title Bars"));
    switchAction->setCheckable(true);
    switchAction->setChecked(KdenliveSettings::showtitlebars());
    connect(switchAction, &QAction::triggered, this, [](bool checked) {
        KdenliveSettings::setShowtitlebars(checked);
        Q_EMIT pCore->hideBars(!checked);
    });
    connect(pCore.get(), &Core::switchTitleBars, this, [switchAction]() { switchAction->trigger(); });

    QAction *showMixer = addAction(QStringLiteral("audiomixer_button"), i18n("Audio Mixer"), QIcon::fromTheme(QStringLiteral("view-media-equalizer")));
    showMixer->setCheckable(true);

    auto *favoriteEffects = new QWidgetAction(this);
    favoriteEffects->setText(i18n("Favorite Effects"));
    favoriteEffects->setIcon(QIcon::fromTheme(QStringLiteral("favorite")));
    addAction(QStringLiteral("favorite_effects"), favoriteEffects);
    auto *renderButton = new QWidgetAction(this);
    renderButton->setText(i18nc("@intoolbar the name of the action to place an export/render button in a toolbar", "Render Button"));
    renderButton->setIcon(QIcon::fromTheme(QStringLiteral("media-record")));
    addAction(QStringLiteral("project_render_button"), renderButton);
    auto *previewButton = new QWidgetAction(this);
    previewButton->setText(i18n("Timeline Preview"));
    previewButton->setIcon(QIcon::fromTheme(QStringLiteral("preview-render-on")));
    addAction(QStringLiteral("timeline_preview_button"), previewButton);

    // Stable active-timeline commands; TimelineTabs binds them after initialization.
    addAction(QStringLiteral("delete_subtitle_clip"), i18n("Delete Timeline Selection"), QIcon::fromTheme(QStringLiteral("edit-delete")))->setEnabled(false);
    addAction(QStringLiteral("audio_record"), i18n("Record"), QIcon::fromTheme(QStringLiteral("media-record")), Qt::Key_R);
    addAction(QStringLiteral("sequence_next"), i18n("Switch to next Sequence"), QIcon::fromTheme(QStringLiteral("go-next")), QKeySequence::NextChild);
    addAction(QStringLiteral("sequence_previous"), i18n("Switch to previous Sequence"), QIcon::fromTheme(QStringLiteral("go-previous")),
              QKeySequence::PreviousChild);

    addAction(QStringLiteral("create_marker_from_zone"), i18n("Create Marker from Zone"), this, SLOT(slotCreateRangeMarkerFromZone()),
              QIcon::fromTheme(QStringLiteral("bookmark-new")));
    addAction(QStringLiteral("create_marker_from_zone_quick"), i18n("Create Marker from Zone Quickly"), this, SLOT(slotCreateRangeMarkerFromZoneQuick()),
              QIcon::fromTheme(QStringLiteral("bookmark-new")));
    addAction(QStringLiteral("identify_gaps_all_tracks"), i18n("All Tracks"));
    addAction(QStringLiteral("add_markers_at_gaps_on_track"), i18n("Selected Track"));
    QAction *separateChannels = addAction(QStringLiteral("separate_audio_channels"), i18n("Separate Channels"), this, SLOT(slotSeparateAudioChannel()));
    separateChannels->setCheckable(true);
    separateChannels->setChecked(KdenliveSettings::displayallchannels());
    separateChannels->setData(QStringLiteral("separate_channels"));
    QAction *normalizeChannels =
        addAction(QStringLiteral("normalize_audio_thumbnails"), i18n("Normalize Audio Thumbnails"), this, SLOT(slotNormalizeAudioChannel(bool)));
    normalizeChannels->setCheckable(true);
    normalizeChannels->setChecked(KdenliveSettings::normalizechannels());
    normalizeChannels->setData(QStringLiteral("normalize_channels"));
    QAction *proxyRender = addAction(QStringLiteral("preview_using_proxy"), i18n("Preview Using Proxy Clips"));
    proxyRender->setCheckable(true);
    proxyRender->setChecked(KdenliveSettings::proxypreview());
    connect(proxyRender, &QAction::triggered, this, [](bool checked) { KdenliveSettings::setProxypreview(checked); });
    QAction *autoRender = addAction(QStringLiteral("preview_auto_render"), i18n("Automatic Preview"), this, SLOT(slotToggleAutoPreview(bool)),
                                    QIcon::fromTheme(QStringLiteral("view-refresh")));
    autoRender->setCheckable(true);
    autoRender->setChecked(KdenliveSettings::autopreview());

    QAction *recConfig = addAction(QStringLiteral("configure_recording"), i18n("Configure Recording"), QIcon::fromTheme(QStringLiteral("configure")));
    connect(recConfig, &QAction::triggered, this, []() { Q_EMIT pCore->showConfigDialog(Kdenlive::PageCapture, 0); });
    QAction *includeList = addAction(QStringLiteral("assets_reviewed_only"), i18n("Only show reviewed items"), QIcon::fromTheme(QStringLiteral("games-solve")));
    includeList->setCheckable(true);
    includeList->setChecked(KdenliveSettings::enableAssetsIncludeList());
    QAction *tenBit =
        addAction(QStringLiteral("assets_tenbit_only"), i18n("Only show 10 bit compatible items"), QIcon::fromTheme(QStringLiteral("colormanagement")));
    tenBit->setCheckable(true);
    tenBit->setChecked(KdenliveSettings::tenbitpipeline());

    // These menu presentations did not previously offer configurable shortcuts.
    for (const QString &name :
         {QStringLiteral("create_marker_from_zone"), QStringLiteral("create_marker_from_zone_quick"), QStringLiteral("identify_gaps_all_tracks"),
          QStringLiteral("add_markers_at_gaps_on_track"), QStringLiteral("separate_audio_channels"), QStringLiteral("normalize_audio_thumbnails"),
          QStringLiteral("preview_using_proxy"), QStringLiteral("preview_auto_render"), QStringLiteral("configure_recording"),
          QStringLiteral("assets_reviewed_only"), QStringLiteral("assets_tenbit_only")}) {
        actionCollection()->setShortcutsConfigurable(actionCollection()->action(name), false);
    }

    // create edit mode buttons
    // TODO: remove icon check ones we require KF > 6.1
    QString normalEditIconName =
        QIcon::hasThemeIcon(QStringLiteral("timeline-mode-normal")) ? QStringLiteral("timeline-mode-normal") : QStringLiteral("kdenlive-normal-edit");
    m_normalEditTool = new QAction(QIcon::fromTheme(normalEditIconName), i18n("Normal Mode"), this);
    m_normalEditTool->setCheckable(true);
    m_normalEditTool->setChecked(true);

    // TODO: remove icon check ones we require KF > 6.1
    QString overwriteEditIconName =
        QIcon::hasThemeIcon(QStringLiteral("timeline-mode-overwrite")) ? QStringLiteral("timeline-mode-overwrite") : QStringLiteral("kdenlive-overwrite-edit");
    m_overwriteEditTool = new QAction(QIcon::fromTheme(overwriteEditIconName), i18n("Overwrite Mode"), this);
    m_overwriteEditTool->setCheckable(true);
    m_overwriteEditTool->setChecked(false);

    // TODO: remove icon check ones we require KF > 6.1
    QString insertEditIconName =
        QIcon::hasThemeIcon(QStringLiteral("timeline-mode-insert")) ? QStringLiteral("timeline-mode-insert") : QStringLiteral("kdenlive-insert-edit");
    m_insertEditTool = new QAction(QIcon::fromTheme(insertEditIconName), i18n("Insert Mode"), this);
    m_insertEditTool->setCheckable(true);
    m_insertEditTool->setChecked(false);

    KSelectAction *sceneMode = new KSelectAction(i18n("Timeline Edit Mode"), this);
    sceneMode->setWhatsThis(
        xi18nc("@info:whatsthis", "Switches between Normal, Overwrite and Insert Mode. Determines the default action when handling clips in the timeline."));
    sceneMode->addAction(m_normalEditTool);
    sceneMode->addAction(m_overwriteEditTool);
    sceneMode->addAction(m_insertEditTool);
    sceneMode->setCurrentItem(0);
    connect(sceneMode, &KSelectAction::actionTriggered, this, &MainWindow::slotChangeEdit);
    addAction(QStringLiteral("timeline_mode"), sceneMode);
    actionCollection()->setShortcutsConfigurable(sceneMode, false);

    m_useTimelineZone = new KDualAction(i18n("Do not Use Timeline Zone for Insert"), i18n("Use Timeline Zone for Insert"), this);
    m_useTimelineZone->setWhatsThis(xi18nc("@info:whatsthis", "Toggles between using the timeline zone for inserting (on) or not (off)."));
    m_useTimelineZone->setActiveIcon(QIcon::fromTheme(QStringLiteral("timeline-use-zone-on")));
    m_useTimelineZone->setInactiveIcon(QIcon::fromTheme(QStringLiteral("timeline-use-zone-off")));
    m_useTimelineZone->setAutoToggle(true);
    connect(m_useTimelineZone, &KDualAction::activeChangedByUser, this, &MainWindow::slotSwitchTimelineZone);
    addAction(QStringLiteral("use_timeline_zone_in_edit"), m_useTimelineZone);

    m_compositeAction = new QAction(i18n("Enable Track Compositing"), this);
    m_compositeAction->setCheckable(true);
    connect(m_compositeAction, &QAction::triggered, this, &MainWindow::slotUpdateCompositing);
    addAction(QStringLiteral("timeline_compositing"), m_compositeAction);
    actionCollection()->setShortcutsConfigurable(m_compositeAction, false);

    QAction *splitView = new QAction(QIcon::fromTheme(QStringLiteral("view-split-top-bottom")), i18n("Split Audio Tracks"), this);
    addAction(QStringLiteral("timeline_view_split"), splitView);
    splitView->setData(QVariant::fromValue(1));
    splitView->setCheckable(true);
    splitView->setChecked(KdenliveSettings::audiotracksbelow() == 1);

    QAction *splitView2 = new QAction(QIcon::fromTheme(QStringLiteral("view-split-top-bottom")), i18n("Split Audio Tracks (reverse)"), this);
    addAction(QStringLiteral("timeline_view_split_reverse"), splitView2);
    splitView2->setData(QVariant::fromValue(2));
    splitView2->setCheckable(true);
    splitView2->setChecked(KdenliveSettings::audiotracksbelow() == 2);

    QAction *mixedView = new QAction(QIcon::fromTheme(QStringLiteral("document-new")), i18n("Mixed Audio tracks"), this);
    addAction(QStringLiteral("timeline_mixed_view"), mixedView);
    mixedView->setData(QVariant::fromValue(0));
    mixedView->setCheckable(true);
    mixedView->setChecked(KdenliveSettings::audiotracksbelow() == 0);

    auto *clipTypeGroup = new QActionGroup(this);
    clipTypeGroup->addAction(mixedView);
    clipTypeGroup->addAction(splitView);
    clipTypeGroup->addAction(splitView2);
    connect(clipTypeGroup, &QActionGroup::triggered, this, &MainWindow::slotUpdateTimelineView);

    // Create audio waveform zoom actions
    m_audioZoomIn = new QAction(QIcon::fromTheme(QStringLiteral("zoom-in")), i18n("Zoom In Audio Waveforms"), this);
    m_audioZoomOut = new QAction(QIcon::fromTheme(QStringLiteral("zoom-out")), i18n("Zoom Out Audio Waveforms"), this);
    m_audioZoomReset = new QAction(QIcon::fromTheme(QStringLiteral("zoom-original")), i18n("Reset Audio Waveform Zoom"), this);
    m_audioZoomCycle = new QAction(QIcon::fromTheme(QStringLiteral("zoom-1-to-2")), i18n("Cycle Audio Waveform Zoom Levels"), this);
    connect(m_audioZoomIn, &QAction::triggered, this, &MainWindow::slotAudioZoomIn);
    connect(m_audioZoomOut, &QAction::triggered, this, &MainWindow::slotAudioZoomOut);
    connect(m_audioZoomReset, &QAction::triggered, this, &MainWindow::slotAudioZoomReset);
    connect(m_audioZoomCycle, &QAction::triggered, this, &MainWindow::slotAudioZoomCycle);
    addAction(QStringLiteral("zoom_audio_in"), m_audioZoomIn);
    addAction(QStringLiteral("zoom_audio_out"), m_audioZoomOut);
    addAction(QStringLiteral("zoom_audio_reset"), m_audioZoomReset);
    addAction(QStringLiteral("zoom_audio_thumbs"),
              m_audioZoomCycle); // kept action name zoom_audio_thumbs for backwards compatibility before in/out/reset were introduced

    m_timeFormatButton = new KSelectAction(QStringLiteral("00:00:00:00 / 00:00:00:00"), this);
    m_timeFormatButton->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    m_timeFormatButton->addAction(i18n("hh:mm:ss:ff"));
    m_timeFormatButton->addAction(i18n("Frames"));
    if (KdenliveSettings::frametimecode()) {
        m_timeFormatButton->setCurrentItem(1);
    } else {
        m_timeFormatButton->setCurrentItem(0);
    }
    connect(m_timeFormatButton, &KSelectAction::indexTriggered, this, &MainWindow::slotUpdateTimecodeFormat);
    m_timeFormatButton->setToolBarMode(KSelectAction::MenuMode);
    m_timeFormatButton->setToolButtonPopupMode(QToolButton::InstantPopup);
    addAction(QStringLiteral("timeline_timecode"), m_timeFormatButton);
    actionCollection()->setShortcutsConfigurable(m_timeFormatButton, false);

    m_buttonSubtitleEditTool = new QAction(QIcon::fromTheme(QStringLiteral("add-subtitle")), i18n("Edit Subtitle Tool"), this);
    m_buttonSubtitleEditTool->setWhatsThis(xi18nc("@info:whatsthis", "Toggles the subtitle track in the timeline."));
    m_buttonSubtitleEditTool->setCheckable(true);
    m_buttonSubtitleEditTool->setChecked(false);
    addAction(QStringLiteral("subtitle_tool"), m_buttonSubtitleEditTool);
    connect(m_buttonSubtitleEditTool, &QAction::triggered, this, &MainWindow::slotShowSubtitles);

    // create tools buttons
    m_buttonSelectTool = new QAction(QIcon::fromTheme(QStringLiteral("cursor-arrow")), i18n("Selection Tool"), this);
    // toolbar->addAction(m_buttonSelectTool);
    m_buttonSelectTool->setCheckable(true);
    m_buttonSelectTool->setChecked(true);

    m_buttonRazorTool = new QAction(QIcon::fromTheme(QStringLiteral("edit-cut")), i18n("Razor Tool"), this);
    // toolbar->addAction(m_buttonRazorTool);
    m_buttonRazorTool->setCheckable(true);
    m_buttonRazorTool->setChecked(false);

    m_buttonSpacerTool = new QAction(QIcon::fromTheme(QStringLiteral("distribute-horizontal-x")), i18n("Spacer Tool"), this);
    m_buttonSpacerTool->setWhatsThis(
        xi18nc("@info:whatsthis",
               "When selected, clicking and dragging the mouse in the timeline temporarily groups separate clips and creates or removes space between clips."));
    // toolbar->addAction(m_buttonSpacerTool);
    m_buttonSpacerTool->setCheckable(true);
    m_buttonSpacerTool->setChecked(false);

    m_buttonRippleTool = new QAction(QIcon::fromTheme(QStringLiteral("kdenlive-ripple")), i18n("Ripple Tool"), this);
    m_buttonRippleTool->setWhatsThis(
        xi18nc("@info:whatsthis",
               "When selected, dragging the edges of a clip lengthens or shortens the clip and moves adjacent clips back and forth while doing that."));
    m_buttonRippleTool->setCheckable(true);
    m_buttonRippleTool->setChecked(false);

    /* TODO Implement Roll
    m_buttonRollTool = new QAction(QIcon::fromTheme(QStringLiteral("kdenlive-rolling")), i18n("Roll Tool"), this);

    m_buttonRollTool->setCheckable(true);
    m_buttonRollTool->setChecked(false);*/

    m_buttonSlipTool = new QAction(QIcon::fromTheme(QStringLiteral("kdenlive-slip")), i18n("Slip Tool"), this);
    m_buttonSlipTool->setWhatsThis(xi18nc("@info:whatsthis", "When selected, dragging a clip slips the clip beneath the given window back and forth."));
    m_buttonSlipTool->setCheckable(true);
    m_buttonSlipTool->setChecked(false);

    m_buttonMulticamTool = new QAction(QIcon::fromTheme(QStringLiteral("view-split-left-right")), i18n("Multicam Tool"), this);
    m_buttonMulticamTool->setCheckable(true);
    m_buttonMulticamTool->setChecked(false);

    /* TODO Implement Slide
    m_buttonSlideTool = new QAction(QIcon::fromTheme(QStringLiteral("kdenlive-slide")), i18n("Slide Tool"), this);
    m_buttonSlideTool->setCheckable(true);
    m_buttonSlideTool->setChecked(false);*/

    auto *toolGroup = new QActionGroup(this);
    toolGroup->addAction(m_buttonSelectTool);
    toolGroup->addAction(m_buttonRazorTool);
    toolGroup->addAction(m_buttonSpacerTool);
    toolGroup->addAction(m_buttonRippleTool);
    // toolGroup->addAction(m_buttonRollTool);
    toolGroup->addAction(m_buttonSlipTool);
    // toolGroup->addAction(m_buttonSlideTool);
    toolGroup->addAction(m_buttonMulticamTool);

    toolGroup->setExclusive(true);

    QAction *collapseItem = new QAction(QIcon::fromTheme(QStringLiteral("collapse-all")), i18n("Collapse/Expand Item"), this);
    addAction(QStringLiteral("collapse_expand"), collapseItem, Qt::Key_Less);
    connect(collapseItem, &QAction::triggered, this, &MainWindow::slotCollapse);

    QAction *collapseAllItems = new QAction(QIcon::fromTheme(QStringLiteral("collapse-all")), i18n("Collapse/Expand All Items"), this);
    addAction(QStringLiteral("collapse_expand_all"), collapseAllItems, QKeySequence(Qt::SHIFT | Qt::Key_Less));
    connect(collapseAllItems, &QAction::triggered, this, &MainWindow::slotCollapseAll);

    QAction *sameTrack = new QAction(QIcon::fromTheme(QStringLiteral("composite-track-preview")), i18n("Mix Clips"), this);
    sameTrack->setWhatsThis(
        xi18nc("@info:whatsthis", "Creates a same-track transition between the selected clip and the adjacent one closest to the playhead."));
    addAction(QStringLiteral("mix_clip"), sameTrack, Qt::Key_U);

    // toolbar->setToolButtonStyle(Qt::ToolButtonIconOnly);

    connect(toolGroup, &QActionGroup::triggered, this, &MainWindow::slotChangeTool);

    m_buttonVideoThumbs = new QAction(QIcon::fromTheme(QStringLiteral("kdenlive-show-video")), i18n("Show Video Thumbnails"), this);
    m_buttonVideoThumbs->setWhatsThis(xi18nc("@info:whatsthis", "Toggles the display of video thumbnails for the clips in the timeline (default is On)."));

    m_buttonVideoThumbs->setCheckable(true);
    m_buttonVideoThumbs->setChecked(KdenliveSettings::videothumbnails());
    connect(m_buttonVideoThumbs, &QAction::triggered, this, &MainWindow::slotSwitchVideoThumbs);

    // TODO: remove icon check ones we require KF > 6.1
    QString waveformIconName = QIcon::hasThemeIcon(QStringLiteral("waveform")) ? QStringLiteral("waveform") : QStringLiteral("kdenlive-show-audiothumb");
    m_buttonAudioThumbs = new QAction(QIcon::fromTheme(waveformIconName), i18n("Show Audio Thumbnails"), this);
    m_buttonAudioThumbs->setWhatsThis(xi18nc("@info:whatsthis", "Toggles the display of audio thumbnails for the clips in the timeline (default is On)."));

    m_buttonAudioThumbs->setCheckable(true);
    m_buttonAudioThumbs->setChecked(KdenliveSettings::audiothumbnails());
    connect(m_buttonAudioThumbs, &QAction::triggered, this, &MainWindow::slotSwitchAudioThumbs);

    // Create split button for audio thumbnails with zoom controls
    auto *audioThumbsButton = new QToolButton(this);
    audioThumbsButton->setDefaultAction(m_buttonAudioThumbs);
    audioThumbsButton->setPopupMode(QToolButton::MenuButtonPopup);
    audioThumbsButton->setToolButtonStyle(Qt::ToolButtonIconOnly);

    // Set initial icon colorization based on current zoom level
    updateAudioWaveformActionIcon();

    // Create menu for zoom controls
    m_audioThumbsMenu = new QMenu(this);
    QWidget *statusAudioZoomWidget = new QWidget();
    QVBoxLayout *statusAudioZoomLayout = new QVBoxLayout(statusAudioZoomWidget);
    statusAudioZoomLayout->setContentsMargins(2, 2, 2, 2);
    statusAudioZoomLayout->setSpacing(2);

    // Current zoom level (top)
    m_statusZoomLevelButton = new QPushButton(i18n("%1×", KdenliveSettings::waveformScaler()));
    QFont statusBoldFont = m_statusZoomLevelButton->font();
    statusBoldFont.setBold(true);
    m_statusZoomLevelButton->setFont(statusBoldFont);
    applyZoomLevelButtonStyling();
    statusAudioZoomLayout->addWidget(m_statusZoomLevelButton);

    // Separator between label and buttons
    QFrame *statusZoomSeparator = new QFrame();
    statusZoomSeparator->setFrameShape(QFrame::HLine);
    statusZoomSeparator->setFixedHeight(1);
    statusZoomSeparator->setLineWidth(1);
    statusAudioZoomLayout->addWidget(statusZoomSeparator);

    // Zoom in button
    QToolButton *statusZoomInButton = new QToolButton();
    statusZoomInButton->setIcon(QIcon::fromTheme(QStringLiteral("zoom-in")));
    statusZoomInButton->setToolTip(i18n("Zoom In Audio Waveforms"));
    statusZoomInButton->setAutoRaise(true);
    connect(statusZoomInButton, &QToolButton::clicked, this, &MainWindow::slotAudioZoomIn);
    statusAudioZoomLayout->addWidget(statusZoomInButton);

    // Zoom out button (bottom)
    QToolButton *statusZoomOutButton = new QToolButton();
    statusZoomOutButton->setIcon(QIcon::fromTheme(QStringLiteral("zoom-out")));
    statusZoomOutButton->setToolTip(i18n("Zoom Out Audio Waveforms"));
    statusZoomOutButton->setAutoRaise(true);
    connect(statusZoomOutButton, &QToolButton::clicked, this, &MainWindow::slotAudioZoomOut);
    statusAudioZoomLayout->addWidget(statusZoomOutButton);

    // Set initial button states
    statusZoomInButton->setEnabled(KdenliveSettings::waveformScaler() < 8);
    statusZoomOutButton->setEnabled(KdenliveSettings::waveformScaler() > 1);
    // Helper function to update button state based on zoom level (click to reset zoom)
    auto updateZoomButtonState = [this]() {
        if (KdenliveSettings::waveformScaler() > 1) {
            m_statusZoomLevelButton->setToolTip(i18n("Click to reset Audio Waveform Zoom"));
            m_statusZoomLevelButton->setEnabled(true);
            // Ensure the connection exists
            disconnect(m_statusZoomLevelButton, &QPushButton::clicked, this, &MainWindow::slotAudioZoomReset);
            connect(m_statusZoomLevelButton, &QPushButton::clicked, this, &MainWindow::slotAudioZoomReset);
        } else {
            m_statusZoomLevelButton->setToolTip(i18n("Current Audio Waveform Zoom Level"));
            m_statusZoomLevelButton->setEnabled(false);
            disconnect(m_statusZoomLevelButton, &QPushButton::clicked, this, &MainWindow::slotAudioZoomReset);
        }
    };
    updateZoomButtonState();

    auto *statusAudioZoomAction = new QWidgetAction(this);
    statusAudioZoomAction->setDefaultWidget(statusAudioZoomWidget);
    m_audioThumbsMenu->addAction(statusAudioZoomAction);

    // Connect to update zoom level display when it changes
    connect(KdenliveSettings::self(), &KdenliveSettings::waveformScalerChanged, this, [this, statusZoomInButton, statusZoomOutButton, updateZoomButtonState]() {
        m_statusZoomLevelButton->setText(i18n("%1×", KdenliveSettings::waveformScaler()));
        // Update button states based on zoom level
        statusZoomInButton->setEnabled(KdenliveSettings::waveformScaler() < 8);
        statusZoomOutButton->setEnabled(KdenliveSettings::waveformScaler() > 1);
        // Enable or disable the reset zoom button
        updateZoomButtonState();
        updateAudioWaveformActionIcon();
    });

    audioThumbsButton->setMenu(m_audioThumbsMenu);

    // Create widget action for the split button
    auto *audioThumbsButtonAction = new QWidgetAction(this);
    audioThumbsButtonAction->setDefaultWidget(audioThumbsButton);
    audioThumbsButtonAction->setText(i18n("Audio Thumbnails"));
    audioThumbsButtonAction->setIcon(QIcon::fromTheme(waveformIconName));

    m_buttonShowMarkers = new QAction(QIcon::fromTheme(QStringLiteral("kdenlive-show-markers")), i18n("Show Markers Comments"), this);

    m_buttonShowMarkers->setCheckable(true);
    m_buttonShowMarkers->setChecked(KdenliveSettings::showmarkers());
    connect(m_buttonShowMarkers, &QAction::triggered, this, &MainWindow::slotSwitchMarkersComments);

    m_buttonSnap = new QAction(QIcon::fromTheme(QStringLiteral("snap")), i18n("Snap"), this);
    m_buttonSnap->setWhatsThis(xi18nc("@info:whatsthis", "Toggles the snap function (clips snap to playhead, edges, markers, guides and others)."));
    m_buttonSnap->setCheckable(true);
    m_buttonSnap->setChecked(KdenliveSettings::snaptopoints());
    connect(m_buttonSnap, &QAction::triggered, this, &MainWindow::slotSwitchSnap);

    m_buttonHideClipOverlays = new QAction(QIcon::fromTheme(QStringLiteral("draw-text")), i18n("Show Clip Names"), this);
    m_buttonHideClipOverlays->setWhatsThis(xi18nc("@info:whatsthis", "Toggles the display of clip names and info in timeline."));
    m_buttonHideClipOverlays->setCheckable(true);
    m_buttonHideClipOverlays->setChecked(KdenliveSettings::showClipOverlays());
    connect(m_buttonHideClipOverlays, &QAction::triggered, this, &MainWindow::slotSwitchClipOverlays);

    m_buttonMouseZoomOnPlayhead = new QAction(QIcon::fromTheme(QStringLiteral("zoom-original")), i18n("Mouse Zoom on Playhead"), this);
    m_buttonMouseZoomOnPlayhead->setWhatsThis(xi18nc("@info:whatsthis", "Toggles the mouse zoom on playhead feature (default is Off)."));

    m_buttonMouseZoomOnPlayhead->setCheckable(true);
    m_buttonMouseZoomOnPlayhead->setChecked(KdenliveSettings::timelinemousezoomonplayhead());
    connect(m_buttonMouseZoomOnPlayhead, &QAction::triggered, this, &MainWindow::slotMouseZoomOnPlayhead);

    m_buttonTimelineTags = new QAction(QIcon::fromTheme(QStringLiteral("tag")), i18n("Show Color Tags in Timeline"), this);
    m_buttonTimelineTags->setWhatsThis(xi18nc("@info:whatsthis", "Toggles the display of clip tags in the timeline (default is On)."));

    m_buttonTimelineTags->setCheckable(true);
    m_buttonTimelineTags->setChecked(KdenliveSettings::tagsintimeline());
    connect(m_buttonTimelineTags, &QAction::triggered, this, &MainWindow::slotShowTimelineTags);

    m_buttonFitZoom = new QAction(QIcon::fromTheme(QStringLiteral("zoom-fit-best")), i18n("Fit Zoom to Project"), this);
    m_buttonFitZoom->setWhatsThis(xi18nc("@info:whatsthis", "Adjusts the zoom level to fit the entire project into the timeline windows."));

    m_buttonFitZoom->setCheckable(false);

    m_zoomSlider = new QSlider(Qt::Horizontal, this);
    m_zoomSlider->setRange(0, 20);
    m_zoomSlider->setPageStep(1);
    m_zoomSlider->setInvertedAppearance(true);
    m_zoomSlider->setInvertedControls(true);

    m_zoomSlider->setMaximumWidth(150);
    m_zoomSlider->setMinimumWidth(100);

    m_zoomIn = KStandardAction::zoomIn(this, SLOT(slotZoomIn()), actionCollection());
    m_zoomOut = KStandardAction::zoomOut(this, SLOT(slotZoomOut()), actionCollection());

    connect(m_zoomSlider, &QSlider::valueChanged, this, [&](int value) { slotSetZoom(value); });
    connect(m_zoomSlider, &QAbstractSlider::sliderMoved, this, &MainWindow::slotShowZoomSliderToolTip);
    connect(m_buttonFitZoom, &QAction::triggered, this, &MainWindow::slotFitZoom);

    KToolBar *toolbar = new KToolBar(QStringLiteral("statusToolBar"), this, Qt::BottomToolBarArea);
    toolbar->setMovable(false);
    toolbar->setToolButtonStyle(Qt::ToolButtonIconOnly);

    if (KdenliveSettings::gpu_accel()) {
        QLabel *warnLabel = new QLabel(i18n("Experimental GPU processing enabled - not for production"), this);
        warnLabel->setFont(QFontDatabase::systemFont(QFontDatabase::SmallestReadableFont));
        warnLabel->setAlignment(Qt::AlignHCenter);
        warnLabel->setStyleSheet(QStringLiteral("QLabel { background-color :red; color:black;padding-left:2px;padding-right:2px}"));
        toolbar->addWidget(warnLabel);
    }

    m_trimLabel = new QLabel(QString(), this);
    m_trimLabel->setFont(QFontDatabase::systemFont(QFontDatabase::SmallestReadableFont));
    m_trimLabel->setAlignment(Qt::AlignHCenter);
    m_trimLabel->setMinimumWidth(m_trimLabel->fontMetrics().boundingRect(i18n("Multicam")).width() + 8);
    m_trimLabel->setToolTip(i18n("Active tool and editing mode"));

    toolbar->addWidget(m_trimLabel);
    toolbar->addSeparator();
    toolbar->addAction(m_buttonMouseZoomOnPlayhead);
    toolbar->addAction(m_buttonTimelineTags);
    toolbar->addAction(m_buttonVideoThumbs);
    toolbar->addAction(audioThumbsButtonAction);
    toolbar->addAction(m_buttonShowMarkers);
    toolbar->addAction(m_buttonHideClipOverlays);
    toolbar->addAction(m_buttonSnap);
    toolbar->addSeparator();
    toolbar->addAction(m_buttonFitZoom);
    toolbar->addAction(m_zoomOut);
    toolbar->addWidget(m_zoomSlider);
    toolbar->addAction(m_zoomIn);

    int small = style()->pixelMetric(QStyle::PM_SmallIconSize);
    statusBar()->setMaximumHeight(2 * small);
    m_messageLabel = new StatusBarMessageLabel(this);
    m_messageLabel->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding);
    connect(this, &MainWindow::displayMessage, m_messageLabel, &StatusBarMessageLabel::setMessage);
    connect(this, &MainWindow::displaySelectionMessage, m_messageLabel, &StatusBarMessageLabel::setSelectionMessage);
    connect(this, &MainWindow::displayProgressMessage, m_messageLabel, &StatusBarMessageLabel::setProgressMessage);
    statusBar()->addWidget(m_messageLabel, 10);
    statusBar()->addPermanentWidget(toolbar);
    toolbar->setIconSize(QSize(small, small));
    toolbar->layout()->setContentsMargins(0, 0, 0, 0);
    statusBar()->setContentsMargins(0, 0, 0, 0);

    addAction(QStringLiteral("normal_mode"), m_normalEditTool);
    addAction(QStringLiteral("overwrite_mode"), m_overwriteEditTool);
    addAction(QStringLiteral("insert_mode"), m_insertEditTool);

    KActionCategory *toolsActionCategory = new KActionCategory(i18n("Tools"), actionCollection());
    addAction(QStringLiteral("select_tool"), m_buttonSelectTool, Qt::Key_S, toolsActionCategory);
    addAction(QStringLiteral("razor_tool"), m_buttonRazorTool, Qt::Key_X, toolsActionCategory);
    addAction(QStringLiteral("spacer_tool"), m_buttonSpacerTool, Qt::Key_M, toolsActionCategory);
    addAction(QStringLiteral("ripple_tool"), m_buttonRippleTool, {}, toolsActionCategory);
    // addAction(QStringLiteral("roll_tool"), m_buttonRollTool, QKeySequence(), toolsActionCategory);
    addAction(QStringLiteral("slip_tool"), m_buttonSlipTool, {}, toolsActionCategory);
    addAction(QStringLiteral("multicam_tool"), m_buttonMulticamTool, {}, toolsActionCategory);
    // addAction(QStringLiteral("slide_tool"), m_buttonSlideTool);

    addAction(QStringLiteral("automatic_transition"), m_buttonTimelineTags);
    addAction(QStringLiteral("show_video_thumbs"), m_buttonVideoThumbs);
    addAction(QStringLiteral("show_audio_thumbs"), m_buttonAudioThumbs);
    addAction(QStringLiteral("show_markers"), m_buttonShowMarkers);
    addAction(QStringLiteral("snap"), m_buttonSnap);
    addAction(QStringLiteral("hide_overlay"), m_buttonHideClipOverlays);
    addAction(QStringLiteral("zoom_fit"), m_buttonFitZoom);

#if defined(Q_OS_WIN)
    int glBackend = KdenliveSettings::opengl_backend();
    QAction *openGLAuto = new QAction(i18n("Auto"), this);
    openGLAuto->setData(0);
    openGLAuto->setCheckable(true);
    openGLAuto->setChecked(glBackend == 0);

    QAction *openGLDesktop = new QAction(i18n("OpenGL"), this);
    openGLDesktop->setData(Qt::AA_UseDesktopOpenGL);
    openGLDesktop->setCheckable(true);
    openGLDesktop->setChecked(glBackend == Qt::AA_UseDesktopOpenGL);

    QAction *openGLES = new QAction(i18n("DirectX (ANGLE)"), this);
    openGLES->setData(Qt::AA_UseOpenGLES);
    openGLES->setCheckable(true);
    openGLES->setChecked(glBackend == Qt::AA_UseOpenGLES);

    QAction *openGLSoftware = new QAction(i18n("Software OpenGL"), this);
    openGLSoftware->setData(Qt::AA_UseSoftwareOpenGL);
    openGLSoftware->setCheckable(true);
    openGLSoftware->setChecked(glBackend == Qt::AA_UseSoftwareOpenGL);
    addAction(QStringLiteral("opengl_auto"), openGLAuto);
    addAction(QStringLiteral("opengl_desktop"), openGLDesktop);
    addAction(QStringLiteral("opengl_es"), openGLES);
    addAction(QStringLiteral("opengl_software"), openGLSoftware);
#endif

    addAction(QStringLiteral("run_wizard"), i18n("Run Config Wizard…"), this, SLOT(slotRunWizard()), QIcon::fromTheme(QStringLiteral("tools-wizard")));
    addAction(QStringLiteral("project_settings"), i18n("Project Settings…"), this, SLOT(slotEditProjectSettings()),
              QIcon::fromTheme(QStringLiteral("configure")));

    addAction(QStringLiteral("project_render"), i18n("Render…"), this, SLOT(slotRenderProject()), QIcon::fromTheme(QStringLiteral("media-record")),
              Qt::CTRL | Qt::Key_Return);

    addAction(QStringLiteral("stop_project_render"), i18n("Stop Render"), this, SLOT(slotStopRenderProject()),
              QIcon::fromTheme(QStringLiteral("media-record")));

    addAction(QStringLiteral("project_clean"), i18n("Remove Unused Media"), this, SLOT(slotCleanProject()), QIcon::fromTheme(QStringLiteral("edit-clear-all")));

    QAction *resetAction = new QAction(QIcon::fromTheme(QStringLiteral("view-refresh")), i18n("Reset Configuration…"), this);
    addAction(QStringLiteral("reset_config"), resetAction);
    connect(resetAction, &QAction::triggered, this, [&]() { slotRestart(true); });

    m_playZoneFromCursor = new KDualAction(i18n("Play Zone From Cursor"), i18n("Pause"), this);
    m_playZoneFromCursor->setInactiveIcon(QIcon::fromTheme(QStringLiteral("media-playback-start")));
    m_playZoneFromCursor->setActiveIcon(QIcon::fromTheme(QStringLiteral("media-playback-pause")));
    connect(m_playZoneFromCursor, &KDualAction::activeChangedByUser, pCore->monitorManager(), &MonitorManager::slotPlayZoneFromCursor);
    addAction(QStringLiteral("monitor_play_zone_cursor"), m_playZoneFromCursor, QKeySequence(), QStringLiteral("navandplayback"));

    m_playZone = new KDualAction(i18n("Play Zone"), i18n("Pause"), this);
    m_playZone->setInactiveIcon(QIcon::fromTheme(QStringLiteral("media-playback-start")));
    m_playZone->setActiveIcon(QIcon::fromTheme(QStringLiteral("media-playback-pause")));
    connect(m_playZone, &KDualAction::activeChangedByUser, pCore->monitorManager(), &MonitorManager::slotPlayZone);
    addAction(QStringLiteral("monitor_play_zone"), m_playZone, Qt::CTRL | Qt::Key_Space, QStringLiteral("navandplayback"));

    m_loopZone = new KDualAction(i18n("Loop Zone"), i18n("Pause"), this);
    m_loopZone->setInactiveIcon(QIcon::fromTheme(QStringLiteral("media-playback-start")));
    m_loopZone->setActiveIcon(QIcon::fromTheme(QStringLiteral("media-playback-pause")));
    connect(m_loopZone, &KDualAction::activeChangedByUser, pCore->monitorManager(), &MonitorManager::slotLoopZone);
    addAction(QStringLiteral("monitor_loop_zone"), m_loopZone, Qt::CTRL | Qt::SHIFT | Qt::Key_Space, QStringLiteral("navandplayback"));

    m_loopClip = new KDualAction(i18n("Loop Selected Clip"), i18n("Pause"), this);
    m_loopClip->setInactiveIcon(QIcon::fromTheme(QStringLiteral("media-playback-start")));
    m_loopClip->setActiveIcon(QIcon::fromTheme(QStringLiteral("media-playback-pause")));
    m_loopClip->setEnabled(false);
    addAction(QStringLiteral("monitor_loop_clip"), m_loopClip, QKeySequence(), QStringLiteral("navandplayback"));

    addAction(QStringLiteral("transcode_clip"), i18n("Transcode Clips…"), this, SLOT(slotTranscodeClip()), QIcon::fromTheme(QStringLiteral("edit-copy")));
    QAction *exportAction = new QAction(QIcon::fromTheme(QStringLiteral("document-export")), i18n("OpenTimelineIO E&xport…"), this);
    m_otioExport = new OtioExport(this);
    connect(exportAction, &QAction::triggered, m_otioExport, &OtioExport::slotExport);
    addAction(QStringLiteral("export_project"), exportAction);
    QAction *importAction = new QAction(QIcon::fromTheme(QStringLiteral("document-import")), i18n("OpenTimelineIO &Import…"), this);
    m_otioImport = new OtioImport(this);
    connect(importAction, &QAction::triggered, m_otioImport, &OtioImport::slotImport);
    addAction(QStringLiteral("import_project"), importAction);

    addAction(QStringLiteral("archive_project"), i18n("Archive Project…"), this, SLOT(slotArchiveProject()),
              QIcon::fromTheme(QStringLiteral("document-save-all")));
    addAction(QStringLiteral("switch_monitor"), i18n("Switch Monitor"), this, SLOT(slotSwitchMonitors()), QIcon(), Qt::Key_T);
    addAction(QStringLiteral("focus_timecode"), i18n("Focus Timecode"), this, SLOT(slotFocusTimecode()), QIcon(), Qt::Key_Equal);
    addAction(QStringLiteral("expand_timeline_clip"), i18n("Expand Clip"), QIcon::fromTheme(QStringLiteral("document-open")));

    QAction *overlayInfo = new QAction(QIcon::fromTheme(QStringLiteral("help-hint")), i18n("Monitor Info Overlay"), this);
    addAction(QStringLiteral("monitor_overlay"), overlayInfo, {}, QStringLiteral("monitor"));
    overlayInfo->setCheckable(true);
    overlayInfo->setData(Monitor::InfoOverlay);

    QAction *overlayTCInfo = new QAction(QIcon::fromTheme(QStringLiteral("help-hint")), i18n("Monitor Overlay Timecode"), this);
    addAction(QStringLiteral("monitor_overlay_tc"), overlayTCInfo, {}, QStringLiteral("monitor"));
    overlayTCInfo->setCheckable(true);
    overlayTCInfo->setData(Monitor::TimecodeOverlay);

    QAction *overlayFpsInfo = new QAction(QIcon::fromTheme(QStringLiteral("help-hint")), i18n("Monitor Overlay Playback Fps"), this);
    addAction(QStringLiteral("monitor_overlay_fps"), overlayFpsInfo, {}, QStringLiteral("monitor"));
    overlayFpsInfo->setCheckable(true);
    overlayFpsInfo->setData(Monitor::PlaybackFpsOverlay);

    QAction *overlayMarkerInfo = new QAction(QIcon::fromTheme(QStringLiteral("help-hint")), i18n("Monitor Overlay Markers"), this);
    addAction(QStringLiteral("monitor_overlay_markers"), overlayMarkerInfo, {}, QStringLiteral("monitor"));
    overlayMarkerInfo->setCheckable(true);
    overlayMarkerInfo->setData(Monitor::MarkersOverlay);

    QAction *overlayAudioInfo = new QAction(QIcon::fromTheme(QStringLiteral("help-hint")), i18n("Monitor Overlay Audio Waveform"), this);
    addAction(QStringLiteral("monitor_overlay_audiothumb"), overlayAudioInfo, {}, QStringLiteral("monitor"));
    overlayAudioInfo->setCheckable(true);
    overlayAudioInfo->setData(Monitor::AudioWaveformOverlay);

    QAction *overlayClipJobs = new QAction(QIcon::fromTheme(QStringLiteral("help-hint")), i18n("Monitor Overlay Clip Jobs"), this);
    addAction(QStringLiteral("monitor_overlay_clipjobs"), overlayClipJobs, {}, QStringLiteral("monitor"));
    overlayClipJobs->setCheckable(true);
    overlayClipJobs->setData(Monitor::ClipJobsOverlay);

    connect(overlayInfo, &QAction::toggled, this, [&, overlayTCInfo, overlayFpsInfo, overlayMarkerInfo, overlayAudioInfo, overlayClipJobs](bool toggled) {
        overlayTCInfo->setEnabled(toggled);
        overlayFpsInfo->setEnabled(toggled);
        overlayMarkerInfo->setEnabled(toggled);
        overlayAudioInfo->setEnabled(toggled);
        overlayClipJobs->setEnabled(toggled);
    });

    // Monitor resolution scaling
    KActionCategory *resolutionActionCategory = new KActionCategory(i18n("Preview Resolution"), actionCollection());
    m_scaleGroup = new QActionGroup(this);
    m_scaleGroup->setExclusive(true);
    m_scaleGroup->setEnabled(!KdenliveSettings::external_display());
    QAction *scale_no = new QAction(i18n("Full Resolution (1:1)"), m_scaleGroup);
    addAction(QStringLiteral("scale_no_preview"), scale_no, QKeySequence(), resolutionActionCategory);
    scale_no->setCheckable(true);
    scale_no->setData(0);
    QAction *scale_1 = new QAction(i18n("1080p"), m_scaleGroup);
    addAction(QStringLiteral("scale_1_preview"), scale_1, QKeySequence(), resolutionActionCategory);
    scale_1->setCheckable(true);
    scale_1->setData(1);
    QAction *scale_2 = new QAction(i18n("720p"), m_scaleGroup);
    addAction(QStringLiteral("scale_2_preview"), scale_2, QKeySequence(), resolutionActionCategory);
    scale_2->setCheckable(true);
    scale_2->setData(2);
    QAction *scale_4 = new QAction(i18n("540p"), m_scaleGroup);
    addAction(QStringLiteral("scale_4_preview"), scale_4, QKeySequence(), resolutionActionCategory);
    scale_4->setCheckable(true);
    scale_4->setData(4);
    QAction *scale_8 = new QAction(i18n("360p"), m_scaleGroup);
    addAction(QStringLiteral("scale_8_preview"), scale_8, QKeySequence(), resolutionActionCategory);
    scale_8->setCheckable(true);
    scale_8->setData(8);
    QAction *scale_16 = new QAction(i18n("270p"), m_scaleGroup);
    addAction(QStringLiteral("scale_16_preview"), scale_16, QKeySequence(), resolutionActionCategory);
    scale_16->setCheckable(true);
    scale_16->setData(16);
    connect(pCore->monitorManager(), &MonitorManager::scalingChanged, this, [scale_1, scale_2, scale_4, scale_8, scale_16, scale_no]() {
        switch (KdenliveSettings::previewScaling()) {
        case 1:
            scale_1->setChecked(true);
            break;
        case 2:
            scale_2->setChecked(true);
            break;
        case 4:
            scale_4->setChecked(true);
            break;
        case 8:
            scale_8->setChecked(true);
            break;
        case 16:
            scale_16->setChecked(true);
            break;
        default:
            scale_no->setChecked(true);
            break;
        }
    });
    Q_EMIT pCore->monitorManager()->scalingChanged();
    connect(m_scaleGroup, &QActionGroup::triggered, this, [](QAction *ac) {
        int scaling = ac->data().toInt();
        KdenliveSettings::setPreviewScaling(scaling);
        // Clear timeline selection so that any qml monitor scene is reset
        Q_EMIT pCore->monitorManager()->updatePreviewScaling();
    });

    QAction *dropFrames = new QAction(QIcon(), i18n("Real Time (drop frames)"), this);
    dropFrames->setCheckable(true);
    dropFrames->setChecked(KdenliveSettings::monitor_dropframes());
    addAction(QStringLiteral("mlt_realtime"), dropFrames);
    connect(dropFrames, &QAction::toggled, this, &MainWindow::slotSwitchDropFrames);

    QAction *insertBinZone = addAction(QStringLiteral("insert_project_tree"), i18n("Insert Zone in Project Bin"), this, SLOT(slotInsertZoneToTree()),
                                       QIcon::fromTheme(QStringLiteral("kdenlive-add-clip")), Qt::CTRL | Qt::Key_I);
    insertBinZone->setWhatsThis(xi18nc("@info:whatsthis", "Creates a new clip in the project bin from the defined zone."));

    // TODO: Make these 2 shortcuts context aware to avoid conflict with media browser
    addAction(QStringLiteral("monitor_seek_snap_backward"), i18n("Go to Previous Snap Point"), this, SLOT(slotSnapRewind()),
              QIcon::fromTheme(QStringLiteral("media-seek-backward")), Qt::ALT | Qt::Key_Left, QStringLiteral("navandplayback"));
    addAction(QStringLiteral("monitor_seek_snap_forward"), i18n("Go to Next Snap Point"), this, SLOT(slotSnapForward()),
              QIcon::fromTheme(QStringLiteral("media-seek-forward")), Qt::ALT | Qt::Key_Right, QStringLiteral("navandplayback"));

    addAction(QStringLiteral("seek_clip_start"), i18n("Go to Clip Start"), this, SLOT(slotClipStart()), QIcon::fromTheme(QStringLiteral("media-seek-backward")),
              Qt::Key_Home, QStringLiteral("navandplayback"));
    addAction(QStringLiteral("seek_clip_end"), i18n("Go to Clip End"), this, SLOT(slotClipEnd()), QIcon::fromTheme(QStringLiteral("media-seek-forward")),
              Qt::Key_End, QStringLiteral("navandplayback"));
    addAction(QStringLiteral("monitor_seek_guide_backward"), i18n("Go to Previous Marker"), this, SLOT(slotGuideRewind()),
              QIcon::fromTheme(QStringLiteral("media-seek-backward")), Qt::CTRL | Qt::Key_Left, QStringLiteral("navandplayback"));
    addAction(QStringLiteral("monitor_seek_guide_forward"), i18n("Go to Next Marker"), this, SLOT(slotGuideForward()),
              QIcon::fromTheme(QStringLiteral("media-seek-forward")), Qt::CTRL | Qt::Key_Right, QStringLiteral("navandplayback"));
    addAction(QStringLiteral("align_playhead"), i18n("Align Playhead to Mouse Position"), this, SLOT(slotAlignPlayheadToMousePos()), QIcon(), Qt::Key_P,
              QStringLiteral("navandplayback"));

    addAction(QStringLiteral("grab_item"), i18n("Grab Current Item"), this, SLOT(slotGrabItem()), QIcon::fromTheme(QStringLiteral("transform-move")),
              Qt::SHIFT | Qt::Key_G);

    QAction *overwriteZone = addAction(QStringLiteral("overwrite_to_in_point"), i18n("Overwrite Clip Zone in Timeline"), this, SLOT(slotInsertClipOverwrite()),
                                       QIcon::fromTheme(QStringLiteral("timeline-overwrite")), Qt::Key_B);
    overwriteZone->setWhatsThis(xi18nc("@info:whatsthis", "When clicked the zone of the clip currently selected in the project bin is inserted at the playhead "
                                                          "position in the active timeline. Clips at the insert position are cut and overwritten."));
    QAction *insertZone = addAction(QStringLiteral("insert_to_in_point"), i18n("Insert Clip Zone in Timeline"), this, SLOT(slotInsertClipInsert()),
                                    QIcon::fromTheme(QStringLiteral("timeline-insert")), Qt::Key_V);
    insertZone->setWhatsThis(xi18nc("@info:whatsthis", "When clicked the zone of the clip currently selected in the project bin is inserted at the playhead "
                                                       "position in the active timeline. Clips at the insert position are cut and shifted to the right."));
    QAction *extractZone = addAction(QStringLiteral("remove_extract"), i18n("Extract Timeline Zone"), this, SLOT(slotExtractZone()),
                                     QIcon::fromTheme(QStringLiteral("timeline-extract")), Qt::SHIFT | Qt::Key_X);
    extractZone->setWhatsThis(xi18nc("@info:whatsthis", "Click to delete the timeline zone from the timeline. All clips to the right are shifted left."));
    QAction *liftZone = addAction(QStringLiteral("remove_lift"), i18n("Lift Timeline Zone"), this, SLOT(slotLiftZone()),
                                  QIcon::fromTheme(QStringLiteral("timeline-lift")), Qt::Key_Z);
    liftZone->setWhatsThis(xi18nc("@info:whatsthis", "Click to delete the timeline zone from the timeline. All clips to the right stay in position."));
    QAction *addPreviewZone =
        addAction(QStringLiteral("set_render_timeline_zone"), i18n("Add Preview Zone"), QIcon::fromTheme(QStringLiteral("preview-add-zone")));
    addPreviewZone->setWhatsThis(xi18nc("@info:whatsthis", "Add the currently defined timeline/selection zone as a preview render zone"));
    QAction *removePreviewZone =
        addAction(QStringLiteral("unset_render_timeline_zone"), i18n("Remove Preview Zone"), QIcon::fromTheme(QStringLiteral("preview-remove-zone")));
    removePreviewZone->setWhatsThis(xi18nc(
        "@info:whatsthis",
        "Removes the currently defined timeline/selection zone from the preview render zone. Note that this can leave gaps in the preview render zones."));
    QAction *removeAllPreviewZone =
        addAction(QStringLiteral("clear_render_timeline_zone"), i18n("Remove All Preview Zones"), QIcon::fromTheme(QStringLiteral("preview-remove-all")));
    removeAllPreviewZone->setWhatsThis(xi18nc("@info:whatsthis", "Remove all preview render zones."));
    QAction *startPreviewRender = addAction(QStringLiteral("prerender_timeline_zone"), i18n("Start Preview Render"),
                                            QIcon::fromTheme(QStringLiteral("preview-render-on")), QKeySequence(Qt::SHIFT | Qt::Key_Return));
    startPreviewRender->setWhatsThis(xi18nc("@info:whatsthis",
                                            "Click to start the rendering of all preview zones (recommended for areas with complex and many effects).<nl/>"
                                            "Click on the down-arrow icon to get a list of options (for example: add preview render zone, remove all zones)."));
    addAction(QStringLiteral("stop_prerender_timeline"), i18n("Stop Preview Render"), QIcon::fromTheme(QStringLiteral("preview-render-off")));

    addAction(QStringLiteral("select_timeline_zone"), i18n("Adjust Timeline Zone to Selection"), QIcon::fromTheme(QStringLiteral("edit-select")),
              Qt::SHIFT | Qt::Key_Z);
    addAction(QStringLiteral("select_timeline_clip"), i18n("Select Clip"), QIcon::fromTheme(QStringLiteral("edit-select")), Qt::Key_Plus);
    addAction(QStringLiteral("deselect_timeline_clip"), i18n("Deselect Clip"), QIcon::fromTheme(QStringLiteral("edit-select")), Qt::Key_Minus);
    addAction(QStringLiteral("select_add_timeline_clip"), i18n("Add Clip to Selection"), QIcon::fromTheme(QStringLiteral("edit-select")),
              Qt::ALT | Qt::Key_Plus);
    addAction(QStringLiteral("select_timeline_transition"), i18n("Select Transition"), QIcon::fromTheme(QStringLiteral("edit-select")),
              Qt::SHIFT | Qt::Key_Plus);
    addAction(QStringLiteral("deselect_timeline_transition"), i18n("Deselect Transition"), QIcon::fromTheme(QStringLiteral("edit-select")),
              Qt::SHIFT | Qt::Key_Minus);
    addAction(QStringLiteral("select_add_timeline_transition"), i18n("Add Transition to Selection"), QIcon::fromTheme(QStringLiteral("edit-select")),
              Qt::ALT | Qt::SHIFT | Qt::Key_Plus);

    addAction(QStringLiteral("delete_all_clip_markers"), i18n("Delete All Markers"), this, SLOT(slotDeleteAllClipMarkers()),
              QIcon::fromTheme(QStringLiteral("edit-delete")));
    addAction(QStringLiteral("add_marker_guide_quickly"), i18n("Add Marker Quickly"), this, SLOT(slotAddMarkerGuideQuickly()),
              QIcon::fromTheme(QStringLiteral("bookmark-new")), QKeySequence(Qt::KeypadModifier | Qt::Key_Asterisk));

    QAction *addMarkerWithCategory1 =
        addAction(QStringLiteral("add_marker_guide_1"), i18n("Add Marker Category 1"), this, SLOT(slotAddMarkerWithCategory()),
                  QIcon::fromTheme(QStringLiteral("bookmark-new")), QKeySequence(Qt::KeypadModifier | Qt::Key_1), QStringLiteral("guidecategorynumber"));
    addMarkerWithCategory1->setData(1);
    QAction *addMarkerWithCategory2 =
        addAction(QStringLiteral("add_marker_guide_2"), i18n("Add Marker Category 2"), this, SLOT(slotAddMarkerWithCategory()),
                  QIcon::fromTheme(QStringLiteral("bookmark-new")), QKeySequence(Qt::KeypadModifier | Qt::Key_2), QStringLiteral("guidecategorynumber"));
    addMarkerWithCategory2->setData(2);
    QAction *addMarkerWithCategory3 =
        addAction(QStringLiteral("add_marker_guide_3"), i18n("Add Marker Category 3"), this, SLOT(slotAddMarkerWithCategory()),
                  QIcon::fromTheme(QStringLiteral("bookmark-new")), QKeySequence(Qt::KeypadModifier | Qt::Key_3), QStringLiteral("guidecategorynumber"));
    addMarkerWithCategory3->setData(3);
    QAction *addMarkerWithCategory4 =
        addAction(QStringLiteral("add_marker_guide_4"), i18n("Add Marker Category 4"), this, SLOT(slotAddMarkerWithCategory()),
                  QIcon::fromTheme(QStringLiteral("bookmark-new")), QKeySequence(Qt::KeypadModifier | Qt::Key_4), QStringLiteral("guidecategorynumber"));
    addMarkerWithCategory4->setData(4);
    QAction *addMarkerWithCategory5 =
        addAction(QStringLiteral("add_marker_guide_5"), i18n("Add Marker Category 5"), this, SLOT(slotAddMarkerWithCategory()),
                  QIcon::fromTheme(QStringLiteral("bookmark-new")), QKeySequence(Qt::KeypadModifier | Qt::Key_5), QStringLiteral("guidecategorynumber"));
    addMarkerWithCategory5->setData(5);
    QAction *addMarkerWithCategory6 =
        addAction(QStringLiteral("add_marker_guide_6"), i18n("Add Marker Category 6"), this, SLOT(slotAddMarkerWithCategory()),
                  QIcon::fromTheme(QStringLiteral("bookmark-new")), QKeySequence(Qt::KeypadModifier | Qt::Key_6), QStringLiteral("guidecategorynumber"));
    addMarkerWithCategory6->setData(6);
    QAction *addMarkerWithCategory7 =
        addAction(QStringLiteral("add_marker_guide_7"), i18n("Add Marker Category 7"), this, SLOT(slotAddMarkerWithCategory()),
                  QIcon::fromTheme(QStringLiteral("bookmark-new")), QKeySequence(Qt::KeypadModifier | Qt::Key_7), QStringLiteral("guidecategorynumber"));
    addMarkerWithCategory7->setData(7);
    QAction *addMarkerWithCategory8 =
        addAction(QStringLiteral("add_marker_guide_8"), i18n("Add Marker Category 8"), this, SLOT(slotAddMarkerWithCategory()),
                  QIcon::fromTheme(QStringLiteral("bookmark-new")), QKeySequence(Qt::KeypadModifier | Qt::Key_8), QStringLiteral("guidecategorynumber"));
    addMarkerWithCategory8->setData(8);
    QAction *addMarkerWithCategory9 =
        addAction(QStringLiteral("add_marker_guide_9"), i18n("Add Marker Category 9"), this, SLOT(slotAddMarkerWithCategory()),
                  QIcon::fromTheme(QStringLiteral("bookmark-new")), QKeySequence(Qt::KeypadModifier | Qt::Key_9), QStringLiteral("guidecategorynumber"));
    addMarkerWithCategory9->setData(9);
    QAction *addMarkerWithCategory10 =
        addAction(QStringLiteral("add_marker_guide_10"), i18n("Add Marker Category 10"), this, SLOT(slotAddMarkerWithCategory()),
                  QIcon::fromTheme(QStringLiteral("bookmark-new")), QKeySequence(Qt::KeypadModifier | Qt::Key_0), QStringLiteral("guidecategorynumber"));
    addMarkerWithCategory10->setData(10);

    KActionCategory *clipActionCategory = new KActionCategory(i18n("Current Selection"), actionCollection());

    addAction(QStringLiteral("add_clip_marker"), i18n("Add Marker"), this, SLOT(slotAddClipMarker()), QIcon::fromTheme(QStringLiteral("bookmark-new")),
              QKeySequence(), clipActionCategory);

    addAction(QStringLiteral("delete_clip_marker"), i18n("Delete Marker"), this, SLOT(slotDeleteClipMarker()),
              QIcon::fromTheme(QStringLiteral("bookmark-remove")), QKeySequence(), clipActionCategory);

    QAction *editClipMarker = addAction(QStringLiteral("edit_clip_marker"), i18n("Edit Marker…"), this, SLOT(slotEditClipMarker()),
                                        QIcon::fromTheme(QStringLiteral("bookmark-edit")), QKeySequence(), clipActionCategory);
    editClipMarker->setObjectName(QStringLiteral("edit_marker"));

    QAction *splitAudio =
        addAction(QStringLiteral("clip_split"), i18n("Restore Audio"), QIcon::fromTheme(QStringLiteral("document-new")), QKeySequence(), clipActionCategory);
    splitAudio->setEnabled(false);

    QAction *extractClip = addAction(QStringLiteral("extract_clip"), i18n("Extract Clip"), QIcon::fromTheme(QStringLiteral("timeline-extract")),
                                     QKeySequence(Qt::SHIFT | Qt::Key_Delete), clipActionCategory);
    extractClip->setEnabled(false);

    QAction *extractToBin = addAction(QStringLiteral("save_to_bin"), i18n("Save Clip Part to Bin"), QIcon(), QKeySequence(), clipActionCategory);
    extractToBin->setEnabled(false);

    QAction *enableClips = addAction(QStringLiteral("clip_enable_all"), i18n("Enable Selected Clips"), QIcon(), QKeySequence(), clipActionCategory);
    enableClips->setEnabled(false);
    QAction *disableClips = addAction(QStringLiteral("clip_disable_all"), i18n("Disable Selected Clips"), QIcon(), QKeySequence(), clipActionCategory);
    disableClips->setEnabled(false);

    // Audio reference
    QAction *setAudioAlignReference =
        addAction(QStringLiteral("set_audio_align_ref"), i18n("Set Audio Reference"), QIcon(), QKeySequence(), clipActionCategory);
    setAudioAlignReference->setEnabled(false);

    QAction *alignAudio = addAction(QStringLiteral("align_audio"), i18n("Align Audio to Reference"), QIcon(), QKeySequence(), clipActionCategory);
    alignAudio->setEnabled(false);

    // Timecode reference
    QAction *setTimecodeReference = addAction(QStringLiteral("set_timecode_ref"), i18n("Set Timecode Reference"), QIcon(), QKeySequence(), clipActionCategory);
    setTimecodeReference->setEnabled(false);
    QAction *alignTimecode = addAction(QStringLiteral("align_timecode"), i18n("Align Timecode to Reference"), QIcon(), QKeySequence(), clipActionCategory);
    alignTimecode->setEnabled(false);

    QAction *act =
        addAction(QStringLiteral("edit_item_duration"), i18n("Edit Duration"), QIcon::fromTheme(QStringLiteral("measure")), QKeySequence(), clipActionCategory);
    act->setEnabled(false);

    act =
        addAction(QStringLiteral("edit_item_speed"), i18n("Change Speed"), QIcon::fromTheme(QStringLiteral("speedometer")), QKeySequence(), clipActionCategory);
    act->setEnabled(false);

    act = addAction(QStringLiteral("edit_item_remap"), i18n("Time Remap"), QIcon::fromTheme(QStringLiteral("speedometer")), QKeySequence(), clipActionCategory);
    act->setCheckable(true);
    act->setEnabled(false);

    act = addAction(QStringLiteral("clip_in_project_tree"), i18n("Clip in Project Bin"), this, SLOT(slotClipInProjectTree()),
                    QIcon::fromTheme(QStringLiteral("find-location")), QKeySequence(), clipActionCategory);
    act->setEnabled(false);
    addAction(QStringLiteral("search_bin"), i18n("Search Bin Clip…"), this, SLOT(slotSearchBin()), QIcon::fromTheme(QStringLiteral("edit-find")));

    QAction *duplicateClip = addAction(QStringLiteral("duplicate_timeline_clip"), i18n("Duplicate Clip"), QIcon::fromTheme(QStringLiteral("edit-copy")),
                                       QKeySequence(Qt::CTRL | Qt::Key_D), clipActionCategory);
    duplicateClip->setEnabled(false);

    addAction(QStringLiteral("cut_timeline_clip"), i18n("Cut Clip"), QIcon::fromTheme(QStringLiteral("edit-cut")), Qt::SHIFT | Qt::Key_R);

    addAction(QStringLiteral("replace_timeline_clip"), i18n("Replace with Bin Selection"), QIcon::fromTheme(QStringLiteral("edit-find-replace")),
              QKeySequence());

    addAction(QStringLiteral("cut_timeline_all_clips"), i18n("Cut All Clips"), QIcon::fromTheme(QStringLiteral("edit-cut")), Qt::CTRL | Qt::SHIFT | Qt::Key_R);

    addAction(QStringLiteral("delete_timeline_clip"), i18n("Delete Selected Item"), this, SLOT(slotDeleteItem()),
              QIcon::fromTheme(QStringLiteral("edit-delete")), Qt::Key_Delete);

    QAction *resizeStart = new QAction(QIcon(), i18n("Resize Item Start"), this);
    addAction(QStringLiteral("resize_timeline_clip_start"), resizeStart, QKeySequence(Qt::Key_ParenLeft));

    QAction *resizeEnd = new QAction(QIcon(), i18n("Resize Item End"), this);
    addAction(QStringLiteral("resize_timeline_clip_end"), resizeEnd, QKeySequence(Qt::Key_ParenRight));

    QAction *pasteEffects =
        addAction(QStringLiteral("paste_effects"), i18n("Paste Effects"), QIcon::fromTheme(QStringLiteral("edit-paste")), QKeySequence(), clipActionCategory);
    pasteEffects->setEnabled(false);

    QAction *delEffects = new QAction(QIcon::fromTheme(QStringLiteral("edit-delete")), i18n("Remove Effects"), this);
    addAction(QStringLiteral("delete_effects"), delEffects, QKeySequence(), clipActionCategory);
    delEffects->setEnabled(false);

    QAction *groupClip = addAction(QStringLiteral("group_clip"), i18n("Group Clips"), QIcon::fromTheme(QStringLiteral("object-group")), Qt::CTRL | Qt::Key_G,
                                   clipActionCategory);
    groupClip->setEnabled(false);

    QAction *ungroupClip = addAction(QStringLiteral("ungroup_clip"), i18n("Ungroup Clips"), QIcon::fromTheme(QStringLiteral("object-ungroup")),
                                     QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_G), clipActionCategory);
    ungroupClip->setEnabled(false);

    QAction *sentToSequence =
        addAction(QStringLiteral("send_sequence"), i18n("Create Sequence from Selection"), pCore->projectManager(), SLOT(slotCreateSequenceFromSelection()),
                  QIcon::fromTheme(QStringLiteral("bookmark-new")), QKeySequence(), clipActionCategory);
    sentToSequence->setWhatsThis(
        xi18nc("@info:whatsthis", "Adds the clip(s) currently selected in the timeline to a new sequence clip that can be opened in another timeline tab."));
    sentToSequence->setEnabled(false);

    QAction *copyToSequence =
        addAction(QStringLiteral("copy_to_sequence"), i18n("Copy Selection to New Sequence"), pCore->projectManager(),
                  SLOT(slotCopyAndCreateSequenceFromSelection()), QIcon::fromTheme(QStringLiteral("bookmark-new")), QKeySequence(), clipActionCategory);
    copyToSequence->setWhatsThis(
        xi18nc("@info:whatsthis", "Copy the clip(s) currently selected in the timeline to a new sequence clip that can be opened in another timeline tab."));
    copyToSequence->setEnabled(false);

    act = clipActionCategory->addAction(KStandardActions::Cut, this, &MainWindow::slotCut);
    act->setEnabled(false);

    act = clipActionCategory->addAction(KStandardActions::Copy, this, &MainWindow::slotCopy);
    act->setEnabled(false);

    actionCollection()->addAction(KStandardActions::Paste, this, &MainWindow::slotPaste);

    // Keyframe actions
    m_assetPanel = new AssetPanel(this);
    KActionCategory *kfActions = new KActionCategory(i18n("Effect Keyframes"), actionCollection());
    addAction(QStringLiteral("keyframe_add"), i18n("Add/Remove Keyframe"), m_assetPanel, SLOT(slotAddRemoveKeyframe()),
              QIcon::fromTheme(QStringLiteral("keyframe-add")), QKeySequence(), kfActions);
    addAction(QStringLiteral("keyframe_next"), i18n("Go to next keyframe"), m_assetPanel, SLOT(slotNextKeyframe()),
              QIcon::fromTheme(QStringLiteral("keyframe-next")), QKeySequence(), kfActions);
    addAction(QStringLiteral("keyframe_previous"), i18n("Go to previous keyframe"), m_assetPanel, SLOT(slotPreviousKeyframe()),
              QIcon::fromTheme(QStringLiteral("keyframe-previous")), QKeySequence(), kfActions);

    kdenliveCategoryMap.insert(QStringLiteral("timelineselection"), clipActionCategory);

    addAction(QStringLiteral("insert_space"), i18n("Insert Space…"));
    addAction(QStringLiteral("delete_space"), i18n("Remove Space"));
    addAction(QStringLiteral("delete_all_spaces"), i18n("Remove All Spaces After Cursor"));
    addAction(QStringLiteral("delete_all_clips"), i18n("Remove All Clips After Cursor"));
    addAction(QStringLiteral("delete_space_all_tracks"), i18n("Remove Space in All Tracks"));
    addAction(QStringLiteral("switch_track_solo"), i18n("Switch Solo Audio Track"));

    KActionCategory *timelineActions = new KActionCategory(i18n("Tracks"), actionCollection());
    QAction *insertTrack = new QAction(QIcon(), i18nc("@action", "Insert Track…"), this);
    connect(insertTrack, &QAction::triggered, this, &MainWindow::slotInsertTrack);
    timelineActions->addAction(QStringLiteral("insert_track"), insertTrack);

    QAction *autoTrackHeight = new QAction(QIcon(), i18n("Fit all Tracks in View"), this);
    autoTrackHeight->setCheckable(true);
    autoTrackHeight->setChecked(KdenliveSettings::autotrackheight());
    connect(autoTrackHeight, &QAction::triggered, this, &MainWindow::slotAutoTrackHeight);
    timelineActions->addAction(QStringLiteral("fit_all_tracks"), autoTrackHeight);

    QAction *masterEffectStack = new QAction(QIcon::fromTheme(QStringLiteral("composite-track-on")), i18n("Sequence effects"), this);
    connect(masterEffectStack, &QAction::triggered, this, [&]() {
        pCore->monitorManager()->activateMonitor(Kdenlive::ProjectMonitor);
        getCurrentTimeline()->controller()->showMasterEffects();
    });
    timelineActions->addAction(QStringLiteral("master_effects"), masterEffectStack);

    QAction *switchTrackTarget = new QAction(QIcon(), i18n("Switch Track Target Audio Stream"), this);
    timelineActions->addAction(QStringLiteral("switch_target_stream"), switchTrackTarget);
    actionCollection()->setDefaultShortcut(switchTrackTarget, Qt::Key_Apostrophe);

    QAction *deleteTrack = new QAction(QIcon(), i18n("Delete Track…"), this);
    connect(deleteTrack, &QAction::triggered, this, &MainWindow::slotDeleteTrack);
    timelineActions->addAction(QStringLiteral("delete_track"), deleteTrack);
    deleteTrack->setData("delete_track");

    QAction *moveTrackUp = new QAction(QIcon::fromTheme(QStringLiteral("go-up")), i18n("Move Track Up"), this);
    connect(moveTrackUp, &QAction::triggered, this, &MainWindow::slotMoveTrackUp);
    timelineActions->addAction(QStringLiteral("move_track_up"), moveTrackUp);

    QAction *moveTrackDown = new QAction(QIcon::fromTheme(QStringLiteral("go-down")), i18n("Move Track Down"), this);
    connect(moveTrackDown, &QAction::triggered, this, &MainWindow::slotMoveTrackDown);
    timelineActions->addAction(QStringLiteral("move_track_down"), moveTrackDown);

    QAction *showAudio = new QAction(QIcon(), i18n("Show Record Controls"), this);
    connect(showAudio, &QAction::triggered, this, &MainWindow::slotShowTrackRec);
    timelineActions->addAction(QStringLiteral("show_track_record"), showAudio);
    showAudio->setCheckable(true);
    showAudio->setData("show_track_record");

    QAction *selectTrack = new QAction(QIcon(), i18n("Select All in Current Track"), this);
    timelineActions->addAction(QStringLiteral("select_track"), selectTrack);

    QAction *selectAll = KStandardAction::selectAll(this, SLOT(slotSelectAllTracks()), this);
    selectAll->setIcon(QIcon::fromTheme(QStringLiteral("edit-select-all")));
    selectAll->setShortcutContext(Qt::WidgetWithChildrenShortcut);
    timelineActions->addAction(QStringLiteral("select_all_tracks"), selectAll);

    QAction *unselectAll = KStandardAction::deselect(nullptr, nullptr, this);
    unselectAll->setIcon(QIcon::fromTheme(QStringLiteral("edit-select-none")));
    unselectAll->setShortcutContext(Qt::WidgetWithChildrenShortcut);
    timelineActions->addAction(QStringLiteral("unselect_all_tracks"), unselectAll);

    kdenliveCategoryMap.insert(QStringLiteral("timeline"), timelineActions);

    // Cached data management
    addAction(QStringLiteral("manage_cache"), i18n("Manage Cached Data…"), this, SLOT(slotManageCache()),
              QIcon::fromTheme(QStringLiteral("network-server-database")));

    QAction *disablePreview = new QAction(i18n("Disable Timeline Preview"), this);
    disablePreview->setCheckable(true);
    disablePreview->setEnabled(false);
    addAction(QStringLiteral("disable_preview"), disablePreview);

    addAction(QStringLiteral("add_sequence_marker"), i18n("Add/Remove Timeline Marker"), QIcon::fromTheme(QStringLiteral("bookmarks")), Qt::Key_G);
    addAction(QStringLiteral("delete_sequence_marker"), i18n("Delete Timeline Marker"), QIcon::fromTheme(QStringLiteral("bookmark-remove")));
    addAction(QStringLiteral("delete_all_sequence_markers"), i18n("Delete All Timeline Markers"), this, SLOT(slotDeleteAllSequenceMarkers()),
              QIcon::fromTheme(QStringLiteral("edit-delete")));
    addAction(QStringLiteral("add_markers_at_gaps"), i18n("Identify Gaps"), QIcon::fromTheme(QStringLiteral("bookmark-new")));
    addAction(QStringLiteral("edit_sequence_marker"), i18n("Edit Timeline Marker…"), QIcon::fromTheme(QStringLiteral("bookmark-edit")));

    addAction(QStringLiteral("export_guides"), i18n("Export Markers…"), this, SLOT(slotExportGuides()), QIcon::fromTheme(QStringLiteral("document-export")));
    addAction(QStringLiteral("search_guide"), i18n("Search Marker…"), this, SLOT(slotSearchGuide()), QIcon::fromTheme(QStringLiteral("edit-find")));

    QAction *lockGuides =
        addAction(QStringLiteral("lock_guides"), i18n("Lock Timeline Markers"), this, SLOT(slotLockGuides(bool)), QIcon::fromTheme(QStringLiteral("lock")));
    lockGuides->setCheckable(true);
    lockGuides->setChecked(KdenliveSettings::lockedGuides());
    lockGuides->setToolTip(i18n("Lock Timeline Markers"));
    lockGuides->setWhatsThis(xi18nc(
        "@info:whatsthis", "Lock Timeline Markers. When locked, the markers won't move when using the spacer tool or inserting/removing blank in tracks."));

    addAction(QStringLiteral("add_subtitle"), i18n("Add Subtitle"), this, SLOT(slotAddSubtitle()), QIcon::fromTheme(QStringLiteral("list-add")),
              Qt::SHIFT | Qt::Key_S);
    addAction(QStringLiteral("disable_subtitle"), i18n("Disable Subtitle"), QIcon::fromTheme(QStringLiteral("view-hidden")));
    addAction(QStringLiteral("lock_subtitle"), i18n("Lock Subtitle"), QIcon::fromTheme(QStringLiteral("lock")));

    addAction(QStringLiteral("manage_subtitle"), i18n("Manage Subtitles"), this, SLOT(slotManageSubtitle()),
              QIcon::fromTheme(QStringLiteral("settings-configure")));
    addAction(QStringLiteral("import_subtitle"), i18n("Import Subtitle File…"), this, SLOT(slotImportSubtitle()),
              QIcon::fromTheme(QStringLiteral("document-import")));
    addAction(QStringLiteral("export_subtitle"), i18n("Export Subtitle File…"), QIcon::fromTheme(QStringLiteral("document-export")));
    addAction(QStringLiteral("audio_recognition"), i18n("Speech Recognition…"), this, SLOT(slotSpeechRecognition()),
              QIcon::fromTheme(QStringLiteral("autocorrection")));

    m_saveAction = KStandardAction::save(pCore->projectManager(), SLOT(saveFile()), actionCollection());
    m_saveAction->setIcon(QIcon::fromTheme(QStringLiteral("document-save")));

    QAction *const showMenuBarAction = KStandardAction::showMenubar(this, &MainWindow::showMenuBar, actionCollection());
    showMenuBarAction->setWhatsThis(xi18nc("@info:whatsthis", "This switches between having a <emphasis>Menubar</emphasis> "
                                                              "and having a <interface>Hamburger Menu</interface> button in the main Toolbar."));

    m_hamburgerMenu = KStandardAction::hamburgerMenu(nullptr, nullptr, actionCollection());
    KStandardAction::helpContents(this, &MainWindow::appHelpActivated, actionCollection());
    KStandardAction::quit(this, &MainWindow::close, actionCollection());
    KStandardAction::keyBindings(this, &MainWindow::slotEditKeys, actionCollection());
    KStandardAction::preferences(this, &MainWindow::slotPreferences, actionCollection());
    KStandardAction::configureNotifications(this, &MainWindow::configureNotifications, actionCollection());
    KStandardAction::fullScreen(this, &MainWindow::slotFullScreen, this, actionCollection());

    // cppcheck-suppress legacyUninitvar
    QAction *undo = KStandardAction::undo(m_commandStack, SLOT(undo()), actionCollection());
    undo->setEnabled(false);
    connect(m_commandStack, &QUndoGroup::canUndoChanged, undo, &QAction::setEnabled);

    // cppcheck-suppress legacyUninitvar
    QAction *redo = KStandardAction::redo(m_commandStack, SLOT(redo()), actionCollection());
    redo->setEnabled(false);
    connect(m_commandStack, &QUndoGroup::canRedoChanged, redo, &QAction::setEnabled);
    connect(this, &MainWindow::enableUndo, this, [this, undo, redo](bool enable) {
        bool undoEnabled = enable;
        if (enable && m_commandStack->activeStack()) {
            enable = m_commandStack->activeStack()->canRedo();
            undoEnabled = m_commandStack->activeStack()->canUndo();
        }
        redo->setEnabled(enable);
        undo->setEnabled(undoEnabled);
    });

    addAction(QStringLiteral("copy_debuginfo"), i18n("Copy Debug Information"), this, SLOT(slotCopyDebugInfo()), QIcon::fromTheme(QStringLiteral("edit-copy")));

    QAction *disableEffects = addAction(QStringLiteral("disable_timeline_effects"), i18n("Disable Timeline Effects"), pCore->projectManager(),
                                        SLOT(slotDisableTimelineEffects(bool)), QIcon::fromTheme(QStringLiteral("tools-wizard")));
    disableEffects->setData("disable_timeline_effects");
    disableEffects->setCheckable(true);
    disableEffects->setChecked(false);

    // Timeline hamburger menu
    auto tlsettings = new QMenu(this);
    tlsettings->setIcon(QIcon::fromTheme(QStringLiteral("application-menu")));
    tlsettings->addAction(m_compositeAction);
    tlsettings->addAction(disableEffects);
    tlsettings->addSeparator();
    tlsettings->addAction(mixedView);
    tlsettings->addAction(splitView);
    tlsettings->addAction(splitView2);

    auto *timelineSett = new QToolButton(this);
    timelineSett->setPopupMode(QToolButton::InstantPopup);
    timelineSett->setMenu(tlsettings);
    timelineSett->setIcon(QIcon::fromTheme(QStringLiteral("application-menu")));
    auto *tlButtonAction = new QWidgetAction(this);
    tlButtonAction->setDefaultWidget(timelineSett);
    tlButtonAction->setText(i18n("Track menu"));
    addAction(QStringLiteral("timeline_settings"), tlButtonAction);
    // end of new place for the TL hmbg menu

    addAction(QStringLiteral("switch_track_disabled"), i18n("Toggle Track Disabled"), pCore->projectManager(), SLOT(slotSwitchTrackDisabled()), QIcon(),
              Qt::SHIFT | Qt::Key_H, timelineActions);
    addAction(QStringLiteral("switch_all_track_disabled"), i18n("Toggle All Tracks Disabled"), pCore->projectManager(), SLOT(slotSwitchAllTrackDisabled()),
              QIcon(), Qt::CTRL | Qt::SHIFT | Qt::Key_H, timelineActions);
    addAction(QStringLiteral("switch_track_lock"), i18n("Toggle Track Lock"), pCore->projectManager(), SLOT(slotSwitchTrackLock()), QIcon(),
              Qt::SHIFT | Qt::Key_L, timelineActions);
    addAction(QStringLiteral("switch_all_track_lock"), i18n("Toggle All Tracks Lock"), pCore->projectManager(), SLOT(slotSwitchAllTrackLock()), QIcon(),
              Qt::CTRL | Qt::SHIFT | Qt::Key_L, timelineActions);
    addAction(QStringLiteral("switch_track_target"), i18n("Toggle Track Target"), pCore->projectManager(), SLOT(slotSwitchTrackTarget()), QIcon(),
              Qt::SHIFT | Qt::Key_T, timelineActions);
    addAction(QStringLiteral("switch_active_target"), i18n("Toggle Track Active"), pCore->projectManager(), SLOT(slotSwitchTrackActive()), QIcon(), Qt::Key_A,
              timelineActions);
    addAction(QStringLiteral("switch_all_targets"), i18n("Toggle All Tracks Active"), pCore->projectManager(), SLOT(slotSwitchAllTrackActive()), QIcon(),
              Qt::SHIFT | Qt::Key_A, timelineActions);
    addAction(QStringLiteral("activate_all_targets"), i18n("Switch All Tracks Active"), pCore->projectManager(), SLOT(slotMakeAllTrackActive()), QIcon(),
              Qt::SHIFT | Qt::ALT | Qt::Key_A, timelineActions);
    addAction(QStringLiteral("restore_all_sources"), i18n("Restore Current Clip Target Tracks"), pCore->projectManager(), SLOT(slotRestoreTargetTracks()), {},
              {}, timelineActions);
    addAction(QStringLiteral("add_project_note"), i18n("Add Project Note"), pCore->projectManager(), SLOT(slotAddProjectNote()),
              QIcon::fromTheme(QStringLiteral("list-add")), {}, timelineActions);

    // Build activate track shortcut sequences
    QList<int> keysequence{Qt::Key_1, Qt::Key_2, Qt::Key_3, Qt::Key_4, Qt::Key_5, Qt::Key_6, Qt::Key_7, Qt::Key_8, Qt::Key_9};
    for (int i = 1; i < 10; i++) {
        QAction *ac = new QAction(QIcon(), i18n("Select Audio Track %1", i), this);
        ac->setData(i - 1);
        addAction(QStringLiteral("activate_audio_%1").arg(i), ac, QKeySequence(Qt::ALT | keysequence[i - 1]), timelineActions);
        QAction *ac2 = new QAction(QIcon(), i18n("Select Video Track %1", i), this);
        ac2->setData(i - 1);
        connect(ac2, &QAction::triggered, this, &MainWindow::slotActivateVideoTrackSequence);
        addAction(QStringLiteral("activate_video_%1").arg(i), ac2, QKeySequence(keysequence[i - 1]), timelineActions);
        QAction *ac3 = new QAction(QIcon(), i18n("Select Target %1", i), this);
        ac3->setData(i - 1);
        addAction(QStringLiteral("activate_target_%1").arg(i), ac3, QKeySequence(Qt::CTRL | keysequence[i - 1]), timelineActions);
    }

    // Setup effects and transitions actions.
    KActionCategory *transitionActions = new KActionCategory(i18n("Transitions"), actionCollection());
    auto allTransitions = TransitionsRepository::get()->getNames();
    for (const auto &transition : std::as_const(allTransitions)) {
        auto *transAction = new QAction(transition.first, this);
        transAction->setData(transition.second);
        transAction->setIconVisibleInMenu(false);
        transitionActions->addAction("transition_" + transition.second, transAction);
    }

    // monitor actions
    addAction(QStringLiteral("extract_frame"), i18n("Extract Frame…"), pCore->monitorManager(), SLOT(slotExtractCurrentFrame()),
              QIcon::fromTheme(QStringLiteral("insert-image")));

    addAction(QStringLiteral("extract_frame_to_project"), i18n("Extract Frame to Project…"), pCore->monitorManager(), SLOT(slotExtractCurrentFrameToProject()),
              QIcon::fromTheme(QStringLiteral("insert-image")));

    addAction(QStringLiteral("extract_frame_to_clipboard"), i18n("Extract Frame to Clipboard"), pCore->monitorManager(),
              SLOT(slotExtractCurrentFrameToClipboard()), QIcon::fromTheme(QStringLiteral("insert-image")));
}

void MainWindow::updateTimelineSelectionActions(const TimelineController::SelectionState &state)
{
    int clipCount = 0;
    for (int count : state.clipCounts) {
        clipCount += count;
    }
    const int itemCount = clipCount + state.compositionCount + state.subtitleCount;
    const bool hasClips = clipCount > 0;
    const bool singleClip = clipCount == 1 && itemCount == 1;
    const bool singleClipOrPair = singleClip || state.isAvSplitPair;
    const bool allItemsAreClips = hasClips && itemCount == clipCount;
    const bool canRemap = singleClipOrPair && state.clipCounts.value(ClipType::Color) == 0 && state.clipCounts.value(ClipType::Image) == 0 &&
                          !state.doesAnyClipHaveSpeedAdjustment;
    QAction *editRemap = actionCollection()->action(QStringLiteral("edit_item_remap"));
    editRemap->setEnabled(canRemap);
    editRemap->setChecked(canRemap && state.doesAnyClipHaveTimeRemap);
    actionCollection()
        ->action(QStringLiteral("edit_item_speed"))
        ->setEnabled(allItemsAreClips && state.clipCounts.value(ClipType::Color) == 0 && state.clipCounts.value(ClipType::Image) == 0 &&
                     !state.doesAnyClipHaveTimeRemap);

    QAction *splitClip = actionCollection()->action(QStringLiteral("clip_split"));
    const bool allClipsHaveAudioAndVideo = allItemsAreClips && state.audioAndVideoClipCount == clipCount;
    const bool restoreVideo = allClipsHaveAudioAndVideo && state.audioOnlyClipCount == clipCount;
    const bool restoreAudio = allClipsHaveAudioAndVideo && state.videoOnlyClipCount == clipCount;
    splitClip->setEnabled(restoreVideo || restoreAudio);
    splitClip->setText(restoreVideo ? i18n("Restore video") : i18n("Restore audio"));
    actionCollection()->action(QStringLiteral("extract_clip"))->setEnabled(hasClips);
    actionCollection()->action(QStringLiteral("save_to_bin"))->setEnabled(singleClipOrPair);
    actionCollection()->action(QStringLiteral("clip_enable_all"))->setEnabled(hasClips && !state.allEnabled);
    actionCollection()->action(QStringLiteral("clip_disable_all"))->setEnabled(hasClips && !state.allDisabled);
    actionCollection()->action(QStringLiteral("set_audio_align_ref"))->setEnabled((singleClip && state.audioOnlyClipCount == 1) || state.isAvSplitPair);
    actionCollection()->action(QStringLiteral("align_audio"))->setEnabled(itemCount > 0);
    actionCollection()->action(QStringLiteral("set_timecode_ref"))->setEnabled(itemCount > 0);
    actionCollection()->action(QStringLiteral("align_timecode"))->setEnabled(itemCount > 0);
    actionCollection()->action(QStringLiteral("edit_item_duration"))->setEnabled(itemCount > 0);
    actionCollection()->action(QStringLiteral("clip_in_project_tree"))->setEnabled(singleClipOrPair);
    actionCollection()->action(QStringLiteral("duplicate_timeline_clip"))->setEnabled(hasClips);
    actionCollection()->action(QStringLiteral("paste_effects"))->setEnabled(hasClips);
    actionCollection()->action(QStringLiteral("delete_effects"))->setEnabled(hasClips);
    actionCollection()->action(QStringLiteral("group_clip"))->setEnabled(itemCount > 1);
    actionCollection()->action(QStringLiteral("ungroup_clip"))->setEnabled(state.hasGroupedItems);
    actionCollection()->action(QStringLiteral("send_sequence"))->setEnabled(itemCount > 1);
    actionCollection()->action(QStringLiteral("copy_to_sequence"))->setEnabled(itemCount > 1);
    actionCollection()->action(QStringLiteral("edit_cut"))->setEnabled(itemCount > 0);
    actionCollection()->action(QStringLiteral("edit_copy"))->setEnabled(itemCount > 0);
}
