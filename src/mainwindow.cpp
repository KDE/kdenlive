/*
    SPDX-FileCopyrightText: 2007 Jean-Baptiste Mardelle <jb@kdenlive.org>

SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "mainwindow.h"
#include "assets/assetpanel.hpp"
#include "audiomixer/mixermanager.hpp"
#include "bin/clipcreator.hpp"
#include "bin/generators/generators.h"
#include "bin/mediabrowser.h"
#include "bin/model/subtitlemodel.hpp"
#include "bin/projectclip.h"
#include "bin/projectfolder.h"
#include "bin/projectitemmodel.h"
#include "core.h"
#include "dialogs/clipcreationdialog.h"
#include "dialogs/clipjobmanager.h"
#include "dialogs/renderwidget.h"
#include "dialogs/settings/kdenlivesettingsdialog.h"
#include "dialogs/subtitleedit.h"
#include "dialogs/wizard.h"
#include "doc/docundostack.hpp"
#include "doc/kdenlivedoc.h"
#include "effects/effectbasket.h"
#include "effects/effectlist/view/effectlistwidget.hpp"
#include "effects/effectstack/model/effectstackmodel.hpp"
#include "jobs/customjobtask.h"
#include "jobs/scenesplittask.h"
#include "jobs/speedtask.h"
#include "jobs/stabilizetask.h"
#include "jobs/transcodetask.h"
#include "kddocksetup.h"
#include "kdenlivesettings.h"
#include "keysequencehandler.h"
#include "layouts/layoutmanagement.h"
#include "library/librarywidget.h"
#include "render/renderserver.h"

#ifndef NODBUS
#include <QDBusConnectionInterface>
#include <QDBusInterface>
#endif

// #include "kdenlive_debug.h"
// #include "kdenlivecore_export.h"

#include "dialogs/markerdialog.h"
#include "dialogs/textbasededit.h"
#include "dialogs/timeremap.h"
#include "filefilter.h"
#include "lib/localeHandling.h"
#include "mltcontroller/clipcontroller.h"
#include "monitor/monitor.h"
#include "monitor/monitormanager.h"
#include "monitor/scopes/audiographspectrum.h"
#include "onlineresources/resourcewidget.hpp"
#include "profiles/profilemodel.hpp"
#include "profiles/profilerepository.hpp"
#include "project/cliptranscode.h"
#include "project/dialogs/archivewidget.h"
#include "project/dialogs/guideslist.h"
#include "project/dialogs/projectsettings.h"
#include "project/dialogs/temporarydata.h"
#include "project/projectmanager.h"
#include "scopes/scopemanager.h"
#include "timeline2/view/timelinecontroller.h"
#include "timeline2/view/timelinetabs.hpp"
#include "timeline2/view/timelinewidget.h"
#include "titler/titlewidget.h"
#include "transitions/transitionlist/view/transitionlistwidget.hpp"
#include "transitions/transitionsrepository.hpp"
#include "widgets/progressbutton.h"
#include <config-kdenlive.h>

#ifdef USE_JOGSHUTTLE
#include "jogshuttle/jogmanager.h"
#endif

#include <kddockwidgets/core/DockRegistry.h>
#include <kddockwidgets/core/FloatingWindow.h>
#include <kddockwidgets/core/MainWindow.h>

#include <KAboutData>
#include <KActionCollection>
#include <KActionMenu>
#include <KColorScheme>
#include <KColorSchemeMenu>
#include <KConfigDialog>
#include <KCoreAddons>
#include <KDualAction>
#include <KEditToolBar>
#include <KIconEffect>
#include <KIconTheme>
#include <KLocalizedString>
#include <KMessageBox>
#include <KNSWidgets/Dialog>
#include <KNotifyConfigWidget>
#include <KRecentDirs>
#include <KShortcutsDialog>
#include <KStandardAction>
#include <KStyleManager>
#include <KToggleFullScreenAction>
#include <KToolBar>
#include <KXMLGUIFactory>

#include <KConfigGroup>
#include <QAction>
#include <QClipboard>
#include <QCollator>
#include <QDesktopServices>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QMenu>
#include <QMenuBar>
#include <QProxyStyle>
#include <QPushButton>
#include <QScreen>
#include <QStandardPaths>
#include <QStatusBar>
#include <QStyleFactory>
#include <QUndoGroup>
#include <QVBoxLayout>
#include <QtConcurrent/QtConcurrentRun>

static const char version[] = KDENLIVE_VERSION;
namespace Mlt {
class Producer;
}

QMap<QString, QImage> MainWindow::m_lumacache;
QMap<QString, QStringList> MainWindow::m_lumaFiles;
KIO::filesize_t m_totalCacheSize = 0;
int m_totalCacheJobs = 0;

MainWindow::MainWindow(QWidget *parent)
    : KXmlGuiWindow(parent)
    , m_activeTool(ToolType::SelectTool)
    , m_mousePosition(0)
    , m_effectBasket(nullptr)
{
    // Init all action categories that are used by other parts of the software
    // before we call MainWindow::init and therefore can't be initialized there
    KActionCategory *category = new KActionCategory(i18n("Monitor"), actionCollection());
    kdenliveCategoryMap.insert(QStringLiteral("monitor"), category);
    category = new KActionCategory(i18n("Add Clip"), actionCollection());
    kdenliveCategoryMap.insert(QStringLiteral("addclip"), category);
    category = new KActionCategory(i18n("Add Marker by Category Number"), actionCollection());
    kdenliveCategoryMap.insert(QStringLiteral("guidecategorynumber"), category);
    category = new KActionCategory(i18n("Navigation and Playback"), actionCollection());
    kdenliveCategoryMap.insert(QStringLiteral("navandplayback"), category);
    category = new KActionCategory(i18n("Bin Tags"), actionCollection());
    kdenliveCategoryMap.insert(QStringLiteral("bintags"), category);
    auto flags = KDDockWidgets::Config::self().flags();
    flags |= KDDockWidgets::Config::Flag_HideTitleBarWhenTabsVisible;
    flags |= KDDockWidgets::Config::Flag_AllowReorderTabs;
    flags |= KDDockWidgets::Config::Flag_TitleBarShowAutoHide;

    KDDockWidgets::Config::self().setFlags(flags);
    KDDockWidgets::Core::FloatingWindow::s_windowFlagsOverride = Qt::Tool;

    // Increase the separator size, just for demo
    KDDockWidgets::Config::self().setViewFactory(new CustomWidgetFactory());
    KDDockWidgets::Config::self().setLayoutSpacing(0); // SeparatorThickness(3);
    if (KdenliveSettings::tabposition() == 1) {
        KDDockWidgets::Config::self().setTabsAtBottom(true);
    }
    mainDockWindow = new KDDockWidgets::QtWidgets::MainWindow(QStringLiteral("KdenliveKDDock"));
    mainDockWindow->setCenterWidgetMargins(QMargins(1, 1, 1, 1));
}

void MainWindow::init()
{
    // Handle communication with the renderer app
    new RenderServer(this);
    QString defaultProfile = KdenliveSettings::default_profile();

    pCore->setCurrentProfile(defaultProfile.isEmpty() ? ProjectManager::getDefaultProjectFormat() : defaultProfile);
    m_commandStack = new QUndoGroup();

    // If using a custom profile, make sure the file exists or fallback to default
    QString currentProfilePath = pCore->getCurrentProfile()->path();
    if (currentProfilePath.startsWith(QLatin1Char('/')) && !QFile::exists(currentProfilePath)) {
        KMessageBox::error(this, i18n("Cannot find your default profile, switching to ATSC 1080p 25"));
        pCore->setCurrentProfile(QStringLiteral("atsc_1080p_25"));
        KdenliveSettings::setDefault_profile(QStringLiteral("atsc_1080p_25"));
    }

    // Disable movit until it's stable
    m_gpuAllowed = false;
    KdenliveSettings::setGpu_accel(false);

    // m_gpuAllowed = EffectsRepository::get()->hasInternalEffect(QStringLiteral("glsl.manager"));
    setCentralWidget(mainDockWindow);
    mainDockWindow->show();

    m_shortcutRemoveFocus = new QShortcut(QKeySequence(QStringLiteral("Esc")), this);
    connect(m_shortcutRemoveFocus, &QShortcut::activated, this, &MainWindow::slotRemoveFocus);

    /// Add Widgets
    m_timelineToolBar = toolBar(QStringLiteral("timelineToolBar"));
    m_timelineToolBarContainer = new TimelineContainer(this);
    auto *ctnLay = new QVBoxLayout;
    ctnLay->setSpacing(0);
    ctnLay->setContentsMargins(0, 0, 0, 0);
    m_timelineToolBarContainer->setLayout(ctnLay);
    ctnLay->addWidget(m_timelineToolBar);
    KSharedConfigPtr config = KSharedConfig::openConfig();
    KConfigGroup mainConfig(config, QStringLiteral("MainWindow"));
    KConfigGroup tbGroup(&mainConfig, QStringLiteral("Toolbar timelineToolBar"));
    m_timelineToolBar->applySettings(tbGroup);
    QFrame *fr = new QFrame(this);
    fr->setFrameShape(QFrame::HLine);
    fr->setMaximumHeight(1);
    fr->setLineWidth(1);
    ctnLay->addWidget(fr);
    setupActions();
    auto *layoutManager = new LayoutManagement(this);
    connect(pCore.get(), &Core::loadLayoutById, layoutManager, &LayoutManagement::slotLoadLayoutById, Qt::DirectConnection);
    connect(pCore.get(), &Core::loadLayoutFromData, layoutManager, &LayoutManagement::slotLoadLayoutFromData, Qt::DirectConnection);
    connect(pCore.get(), &Core::adjustLayoutToDar, layoutManager, &LayoutManagement::adjustLayoutToDar, Qt::DirectConnection);
    pCore->buildDocks();

    m_clipMonitor = new Monitor(Kdenlive::ClipMonitor, pCore->monitorManager(), this);
    connect(m_clipMonitor, &Monitor::addMarker, this, &MainWindow::slotAddMarkerGuideQuickly);
    connect(m_clipMonitor, &Monitor::addMarker, this, &MainWindow::slotAddMarkerWithCategory);
    connect(m_clipMonitor, &Monitor::deleteMarker, this, &MainWindow::slotDeleteClipMarker);
    connect(m_clipMonitor, &Monitor::seekToPreviousSnap, this, &MainWindow::slotSnapRewind);
    connect(m_clipMonitor, &Monitor::seekToNextSnap, this, &MainWindow::slotSnapForward);
    connect(m_clipMonitor, &Monitor::passKeyPress, this, &MainWindow::triggerKey);

    m_projectMonitor = new Monitor(Kdenlive::ProjectMonitor, pCore->monitorManager(), this);
    connect(m_projectMonitor, &Monitor::passKeyPress, this, &MainWindow::triggerKey);
    connect(m_projectMonitor, &Monitor::addMarker, this, &MainWindow::slotAddMarkerGuideQuickly);
    connect(m_projectMonitor, &Monitor::addMarker, this, &MainWindow::slotAddMarkerWithCategory);
    connect(m_projectMonitor, &Monitor::seekToPreviousSnap, this, &MainWindow::slotSnapRewind);
    connect(m_projectMonitor, &Monitor::seekToNextSnap, this, &MainWindow::slotSnapForward);
    auto resetDualActions = [&]() {
        if (m_playZone) m_playZone->setActive(false);
        if (m_playZoneFromCursor) m_playZoneFromCursor->setActive(false);
        if (m_loopZone) m_loopZone->setActive(false);
        if (m_loopClip) m_loopClip->setActive(false);
    };
    connect(m_clipMonitor, &Monitor::playbackChanged, this, [resetDualActions](bool playing) {
        if (!playing) {
            resetDualActions();
        }
    });
    connect(m_projectMonitor, &Monitor::playbackChanged, this, [resetDualActions](bool playing) {
        if (!playing) {
            resetDualActions();
        }
    });
    connect(m_loopClip, &KDualAction::activeChangedByUser, this, [&]() {
        std::pair<int, int> inOut = getCurrentTimeline()->controller()->selectionInOut();
        m_projectMonitor->slotLoopClip(inOut);
    });
    installEventFilter(this);
    pCore->monitorManager()->initMonitors(m_clipMonitor, m_projectMonitor);

    m_timelineTabs = new TimelineTabs();
    connect(m_timelineTabs, &TimelineTabs::timelineCreated, this, &MainWindow::connectTimelineApplication);
    connect(m_timelineTabs, &TimelineTabs::selectionStateChanged, this, &MainWindow::updateTimelineSelectionActions);
    ctnLay->addWidget(m_timelineTabs);

    // Timeline dock
    const QSize firstWindowSize = mainDockWindow->size();
    m_timelineDock = addDock(i18n("Timeline"), QStringLiteral("timeline"), m_timelineToolBarContainer, KDDockWidgets::Location_OnBottom, nullptr);

    // Project bin
    m_projectBinDock = addDock(i18n("Project Bin"), QStringLiteral("project_bin"), pCore->bin(), KDDockWidgets::Location_OnTop, m_timelineDock);

    auto dockLib = addDock(i18n("Library"), QStringLiteral("library"), pCore->library(), KDDockWidgets::Location_OnRight);
    dockLib->close();

    auto dockSubs = addDock(i18n("Subtitles"), QStringLiteral("subtitles"), pCore->subtitleWidget(), KDDockWidgets::Location_OnRight);
    dockSubs->close();

    auto dockText = addDock(i18n("Speech Editor"), QStringLiteral("textedit"), pCore->textEditWidget(), KDDockWidgets::Location_OnRight);
    dockText->close();

    auto dockRemap = addDock(i18n("Time Remapping"), QStringLiteral("timeremap"), pCore->timeRemapWidget(), KDDockWidgets::Location_OnRight);
    dockRemap->close();

    connect(pCore.get(), &Core::remapClip, this, [&, dockRemap](int id) {
        if (id > -1) {
            dockRemap->open();
            dockRemap->setAsCurrentTab();
        }
        pCore->timeRemapWidget()->selectedClip(id, pCore->currentTimelineId());
    });

    auto dockGuides = addDock(i18n("Markers"), QStringLiteral("markers"), pCore->guidesList(), KDDockWidgets::Location_OnRight);
    dockGuides->close();

    // Screen grab widget
    QWidget *grabWidget = new QWidget(this);
    auto *grabLayout = new QVBoxLayout;
    grabWidget->setLayout(grabLayout);
    auto *recToolbar = new QToolBar(grabWidget);
    grabLayout->addWidget(recToolbar);
    grabLayout->addStretch(10);
    // Check number of monitors for FFmpeg screen capture
    int screens = QApplication::screens().count();
    if (screens > 1) {
        auto *screenCombo = new QComboBox(recToolbar);
        for (int ix = 0; ix < screens; ix++) {
            screenCombo->addItem(i18n("Monitor %1", ix));
        }
        connect(screenCombo, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), m_clipMonitor, &Monitor::slotSetScreen);
        recToolbar->addWidget(screenCombo);
        // Update screen grab monitor choice in case we changed from fullscreen
        screenCombo->setEnabled(KdenliveSettings::grab_capture_type() == 0);
    }
    // Screengrab record action
    QAction *recAction = m_clipMonitor->recAction();
    addAction(QStringLiteral("screengrab_record"), recAction);
    recToolbar->addAction(recAction);
    QAction *recConfig = actionCollection()->action(QStringLiteral("configure_recording"));
    recToolbar->addAction(recConfig);
    auto screenGrabDock = addDock(i18n("Screen Grab"), QStringLiteral("screengrab"), grabWidget);
    screenGrabDock->close();

    // Audio spectrum scope
    m_audioSpectrum = new AudioGraphSpectrum(pCore->monitorManager());
    auto spectrumDock = addDock(i18n("Audio Spectrum"), QStringLiteral("audiospectrum"), m_audioSpectrum);
    connect(spectrumDock, &KDDockWidgets::QtWidgets::DockWidget::isOpenChanged, this, [&](bool visible) { m_audioSpectrum->dockVisible(visible); });
    spectrumDock->close();

    // Add monitors here to keep them at the right of the window
    m_clipMonitorDock = addDock(i18n("Clip Monitor"), QStringLiteral("clipmonitor"), m_clipMonitor, KDDockWidgets::Location_OnRight, m_projectBinDock);
    m_projectMonitorDock =
        addDock(i18n("Project Monitor"), QStringLiteral("projectmonitor"), m_projectMonitor, KDDockWidgets::Location_OnRight, m_clipMonitorDock);

    m_clipMonitorDock->addDockWidgetAsTab(dockLib);
    pCore->bin()->buildPropertiesDock(m_clipMonitorDock);
    m_projectMonitorDock->addDockWidgetAsTab(dockText);
    // Notes widget
    pCore->projectManager()->buildNotesWidget(m_projectMonitorDock);
    // Raise monitors
    m_clipMonitorDock->setAsCurrentTab();
    m_projectMonitorDock->setAsCurrentTab();

    // Media browser widget
    auto clipDockWidget = addDock(i18n("Media Browser"), QStringLiteral("media_browser"), pCore->mediaBrowser());
    clipDockWidget->close();

    // Online resources widget
    auto *onlineResources = new ResourceWidget(this);
    m_onlineResourcesDock = addDock(i18n("Online Resources"), QStringLiteral("onlineresources"), onlineResources);
    m_onlineResourcesDock->close();
    connect(onlineResources, &ResourceWidget::previewClip, this, [&](const QString &path, const QString &title) {
        m_clipMonitor->slotPreviewResource(path, title);
        m_clipMonitorDock->open();
        m_clipMonitorDock->setAsCurrentTab();
    });

    connect(onlineResources, &ResourceWidget::addClip, this, &MainWindow::slotAddProjectClip);
    connect(onlineResources, &ResourceWidget::addLicenseInfo, this, &MainWindow::slotAddTextNote);

    const QSize stackSize(firstWindowSize.width() * 0.3, 0);
    m_effectStackDock =
        addDock(i18n("Effect/Composition Stack"), QStringLiteral("effect_stack"), m_assetPanel, KDDockWidgets::Location_OnRight, m_timelineDock, stackSize);
    connect(pCore.get(), &Core::requestShowBinEffectStack, m_assetPanel, &AssetPanel::showEffectStack, Qt::QueuedConnection);
    connect(m_assetPanel, &AssetPanel::doSplitEffect, m_projectMonitor, &Monitor::slotSwitchCompare);
    connect(m_assetPanel, &AssetPanel::doSplitBinEffect, m_clipMonitor, &Monitor::slotSwitchCompare);
    connect(m_assetPanel, &AssetPanel::switchCurrentComposition, this,
            [&](int cid, const QString &compositionId) { getCurrentTimeline()->model()->switchComposition(cid, compositionId); });
    connect(pCore->bin(), &Bin::updateTabName, m_timelineTabs, &TimelineTabs::renameTab);
    connect(m_timelineTabs, &TimelineTabs::showMixModel, this, [&](int cid, std::shared_ptr<AssetParameterModel> model, bool refreshOnly) {
        m_assetPanel->showMix(cid, model, refreshOnly);
        if (m_effectStackDock->asDockWidgetController()->isTabbed() && m_effectStackDock->parent() == m_timelineDock->parent()) {
            // Don't raise if tabbed with timeline
            return;
        }
        if (KdenliveSettings::raisepropsmixes()) {
            m_effectStackDock->setAsCurrentTab();
        }
    });
    connect(m_timelineTabs, &TimelineTabs::showTransitionModel, this, [&](int tid, std::shared_ptr<AssetParameterModel> model) {
        m_assetPanel->showTransition(tid, model);
        if (m_effectStackDock->asDockWidgetController()->isTabbed() && m_effectStackDock->parent() == m_timelineDock->parent()) {
            // Don't raise if tabbed with timeline
            return;
        }
        if (KdenliveSettings::raisepropscompositions()) {
            m_effectStackDock->setAsCurrentTab();
        }
    });
    connect(m_timelineTabs, &TimelineTabs::showItemEffectStack, this,
            [&](const QString &clipName, std::shared_ptr<EffectStackModel> model, QSize size, bool showKeyframes) {
                if (model == nullptr && m_assetPanel->effectStackOwner().type == KdenliveObjectType::BinClip) {
                    // Effect stask is currently displaying a bin clip, do nothing
                    return;
                }
                m_assetPanel->showEffectStack(clipName, model, size, showKeyframes);
                if (m_effectStackDock->asDockWidgetController()->isTabbed() && m_effectStackDock->parent() == m_timelineDock->parent()) {
                    // Don't raise if tabbed with timeline
                    return;
                }
                bool isClip = model && model->getOwnerId().type == KdenliveObjectType::TimelineClip;
                bool isTrack = model && model->getOwnerId().type == KdenliveObjectType::TimelineTrack;
                if ((isClip && KdenliveSettings::raisepropsclips()) || (isTrack && KdenliveSettings::raisepropstracks())) {
                    m_effectStackDock->setAsCurrentTab();
                }
            });

    connect(m_timelineTabs, &TimelineTabs::updateAssetPosition, m_assetPanel, &AssetPanel::updateAssetPosition);

    connect(m_timelineTabs, &TimelineTabs::showSubtitle, this, [&, dockSubs](int id) {
        if (id > -1) {
            dockSubs->open();
            dockSubs->setAsCurrentTab();
        }
        pCore->subtitleWidget()->setActiveSubtitle(id);
    });

    connect(pCore.get(), &Core::finalizeRecording, this, [this](const QUuid uuid, const QString &filename) {
        auto timeline = getTimeline(uuid);
        if (timeline) {
            timeline->controller()->finishRecording(filename);
        }
    });

    connect(m_timelineTabs, &TimelineTabs::updateZoom, this, &MainWindow::updateZoomSlider);
    connect(this, &MainWindow::clearAssetPanel, m_assetPanel, &AssetPanel::clearAssetPanel, Qt::DirectConnection);
    connect(this, &MainWindow::assetPanelWarning, m_assetPanel, &AssetPanel::assetPanelWarning);
    connect(m_assetPanel, &AssetPanel::seekToPos, this, [this](int pos) {
        ObjectId oId = m_assetPanel->effectStackOwner();
        switch (oId.type) {
        case KdenliveObjectType::TimelineTrack:
        case KdenliveObjectType::TimelineClip:
        case KdenliveObjectType::TimelineComposition:
        case KdenliveObjectType::Master:
        case KdenliveObjectType::TimelineMix:
            m_projectMonitor->requestSeek(pos);
            break;
        case KdenliveObjectType::BinClip:
            m_clipMonitor->requestSeek(pos);
            break;
        default:
            qDebug() << "ERROR unhandled object type";
            break;
        }
    });

    // Assets filter options
    QAction *includeList = actionCollection()->action(QStringLiteral("assets_reviewed_only"));
    // 10 bit support
    QAction *tenBit = actionCollection()->action(QStringLiteral("assets_tenbit_only"));

    m_effectList2 = new EffectListWidget(includeList, tenBit, this);
    connect(m_effectList2, &EffectListWidget::activateAsset, pCore->projectManager(), &ProjectManager::activateAsset);
    connect(m_assetPanel, &AssetPanel::reloadEffect, m_effectList2, &EffectListWidget::reloadCustomEffect);
    m_effectListDock = addDock(i18n("Effects"), QStringLiteral("effect_list"), m_effectList2, KDDockWidgets::Location_None, m_projectBinDock);
    connect(m_effectListDock, &KDDockWidgets::QtWidgets::DockWidget::isOpenChanged, this, [&](bool visible) {
        if (visible) {
            m_effectList2->searchLine()->setFocus();
        }
    });
    connect(m_effectListDock, &KDDockWidgets::QtWidgets::DockWidget::isCurrentTabChanged, this, [&](bool focused) {
        if (focused) {
            m_effectList2->searchLine()->setFocus();
        }
    });

    m_compositionList = new TransitionListWidget(includeList, tenBit, this);
    m_compositionListDock = addDock(i18n("Compositions"), QStringLiteral("transition_list"), m_compositionList, KDDockWidgets::Location_None, m_projectBinDock);
    connect(m_compositionListDock, &KDDockWidgets::QtWidgets::DockWidget::isOpenChanged, this, [&](bool visible) {
        if (visible) {
            m_compositionList->searchLine()->setFocus();
        }
    });
    connect(m_compositionListDock, &KDDockWidgets::QtWidgets::DockWidget::isCurrentTabChanged, this, [&](bool focused) {
        if (focused) {
            m_compositionList->searchLine()->setFocus();
        }
    });

    // Clear history action
    QAction *cleanHistory = actionCollection()->action(QStringLiteral("clear_undo_history"));

    m_undoView = new QUndoView();
    m_undoView->setCleanIcon(QIcon::fromTheme(QStringLiteral("edit-clear")));
    m_undoView->setEmptyLabel(i18n("Clean"));
    m_undoView->setGroup(m_commandStack);
    m_undoView->addAction(cleanHistory);

    m_undoView->setContextMenuPolicy(Qt::ActionsContextMenu);
    m_undoViewDock = addDock(i18n("Undo History"), QStringLiteral("undo_history"), m_undoView, KDDockWidgets::Location_None, m_projectBinDock);

    connect(m_commandStack, &QUndoGroup::cleanChanged, [this, cleanHistory](bool isClean) {
        m_saveAction->setDisabled(isClean);
        cleanHistory->setDisabled(isClean);
    });

    m_mixerDock = addDock(i18n("Audio Mixer"), QStringLiteral("mixer"), pCore->mixer(), KDDockWidgets::Location_None, m_effectStackDock);
    m_mixerDock->setWhatsThis(xi18nc("@info:whatsthis", "Toggles the audio mixer panel/widget."));
    QAction *showMixer = actionCollection()->action(QStringLiteral("audiomixer_button"));
    connect(m_mixerDock, &KDDockWidgets::QtWidgets::DockWidget::isOpenChanged, this, [&, showMixer](bool visible) {
        pCore->mixer()->connectMixer(visible);
        pCore->audioMixerVisible = visible;
        m_projectMonitor->displayAudioMonitor(m_projectMonitor->isActive());
        showMixer->setChecked(visible);
    });
    connect(showMixer, &QAction::triggered, this, [&]() {
        if (m_mixerDock->isVisible() && !m_mixerDock->visibleRegion().isEmpty()) {
            m_mixerDock->close();
        } else {
            m_mixerDock->open();
            m_mixerDock->setAsCurrentTab();
        }
    });

    // Close non-general docks for the initial layout
    // only show important ones
    m_mixerDock->close();
    m_projectBinDock->setAsCurrentTab();

    bool firstRun = readOptions();
    if (KdenliveSettings::lastCacheCheck().isNull()) {
        // Define a date for first check
        KdenliveSettings::setLastCacheCheck(QDateTime::currentDateTime());
    }

    // Build effects menu
    m_effectsMenu = new QMenu(i18n("Add Effect"), this);
    m_effectActions = new KActionCategory(i18n("Effects"), actionCollection());
    m_effectList2->reloadEffectMenu(m_effectsMenu, m_effectActions);

    m_transitionsMenu = new QMenu(i18n("Add Transition"), this);
    m_transitionActions = new KActionCategory(i18n("Transitions"), actionCollection());

    m_scopesManager = new ScopeManager(this);

    m_extraFactory = new KXMLGUIClient(this);
    buildDynamicActions();

    // Create Effect Basket (dropdown list of favorites)
    m_effectBasket = new EffectBasket(this);
    connect(m_effectBasket, &EffectBasket::activateAsset, pCore->projectManager(), &ProjectManager::activateAsset);
    connect(m_effectList2, &EffectListWidget::reloadFavorites, m_effectBasket, &EffectBasket::slotReloadBasket);
    auto *widgetlist = new QWidgetAction(this);
    widgetlist->setDefaultWidget(m_effectBasket);
    // widgetlist->setText(i18n("Favorite Effects"));
    widgetlist->setToolTip(i18n("Favorite Effects"));
    widgetlist->setWhatsThis(xi18nc("@info:whatsthis", "Click to show a list of favorite effects. Double-click on an effect to add it to the selected clip."));
    widgetlist->setIcon(QIcon::fromTheme(QStringLiteral("favorite")));
    auto *menu = new QMenu(this);
    menu->addAction(widgetlist);

    auto *basketButton = new QToolButton(this);
    basketButton->setMenu(menu);
    basketButton->setToolButtonStyle(toolBar()->toolButtonStyle());
    basketButton->setDefaultAction(widgetlist);
    basketButton->setPopupMode(QToolButton::InstantPopup);
    // basketButton->setText(i18n("Favorite Effects"));
    basketButton->setToolTip(i18n("Favorite Effects"));
    basketButton->setWhatsThis(
        xi18nc("@info:whatsthis", "Click to show a list of favorite effects. Double-click on an effect to add it to the selected clip."));

    basketButton->setIcon(QIcon::fromTheme(QStringLiteral("favorite")));

    auto *toolButtonAction = qobject_cast<QWidgetAction *>(actionCollection()->action(QStringLiteral("favorite_effects")));
    toolButtonAction->setDefaultWidget(basketButton);
    connect(toolButtonAction, &QAction::triggered, basketButton, &QToolButton::showMenu);
    connect(m_effectBasket, &EffectBasket::activateAsset, menu, &QMenu::close);

    // Render button
    ProgressButton *timelineRender = new ProgressButton(i18n("Render…"), 100, this);
    auto *tlrMenu = new QMenu(this);
    timelineRender->setMenu(tlrMenu);
    connect(this, &MainWindow::setRenderProgress, timelineRender, &ProgressButton::setProgress);
    auto *renderButtonAction = qobject_cast<QWidgetAction *>(actionCollection()->action(QStringLiteral("project_render_button")));
    renderButtonAction->setDefaultWidget(timelineRender);

    // Timeline preview button
    ProgressButton *timelinePreview = new ProgressButton(i18n("Rendering preview"), 1000, this);
    auto *tlMenu = new QMenu(this);
    timelinePreview->setMenu(tlMenu);
    connect(this, &MainWindow::setPreviewProgress, timelinePreview, &ProgressButton::setProgress);
    auto *previewButtonAction = qobject_cast<QWidgetAction *>(actionCollection()->action(QStringLiteral("timeline_preview_button")));
    previewButtonAction->setDefaultWidget(timelinePreview);

    // Since not all widgets are added yet, don't use the Save flag now
    setupGUI(KXmlGuiWindow::ToolBar | KXmlGuiWindow::StatusBar | KXmlGuiWindow::Create);

    // Remove secondary cut shortcut conflicting with extract action
    QAction *officialCut = actionCollection()->action(KStandardAction::name(KStandardAction::Cut));
    QList<QKeySequence> cutShortcuts = officialCut->shortcuts();
    if (cutShortcuts.size() > 1) {
        if (cutShortcuts.at(1) == QKeySequence(Qt::SHIFT | Qt::Key_Delete)) {
            cutShortcuts.takeLast();
            officialCut->setShortcuts(cutShortcuts);
        }
    }

    // Only start saving config once all GUI setup is done.
    connect(pCore.get(), &Core::GUISetupDone, this, &MainWindow::finishUiSetup, Qt::DirectConnection);

    LocaleHandling::resetLocale();

    m_timelineToolBar->setToolButtonStyle(Qt::ToolButtonFollowStyle);
    m_timelineToolBar->setProperty("otherToolbar", true);
    timelinePreview->setToolButtonStyle(m_timelineToolBar->toolButtonStyle());
    connect(m_timelineToolBar, &QToolBar::toolButtonStyleChanged, timelinePreview, &ProgressButton::setToolButtonStyle);

    timelineRender->setToolButtonStyle(toolBar()->toolButtonStyle());
    /*ScriptingPart* sp = new ScriptingPart(this, QStringList());
    guiFactory()->addClient(sp);*/

    loadDockActions();
    loadClipActions();
    loadContainerActions();

    KEditToolBar::setGlobalDefaultToolBar(QStringLiteral("timelineToolBar"));

    for (auto &bin : m_binWidgets) {
        bin->setupGeneratorMenu();
    }

    connect(pCore->monitorManager(), &MonitorManager::updateOverlayInfos, this, &MainWindow::slotUpdateMonitorOverlays);

    // Setup and fill effects and transitions menus.
    connect(m_effectsMenu, &QMenu::triggered, this, &MainWindow::slotAddEffect);
    connect(m_transitionsMenu, &QMenu::triggered, this, &MainWindow::slotAddTransition);

    m_timelineContextMenu = new QMenu(this);

    m_timelineContextMenu->addAction(actionCollection()->action(QStringLiteral("insert_space")));
    m_timelineContextMenu->addAction(actionCollection()->action(QStringLiteral("delete_space")));
    m_timelineContextMenu->addAction(actionCollection()->action(QStringLiteral("delete_space_all_tracks")));
    m_timelineContextMenu->addAction(actionCollection()->action(KStandardAction::name(KStandardAction::Paste)));
    slotConnectMonitors();

    m_timelineToolBar->setToolButtonStyle(Qt::ToolButtonIconOnly);
    // TODO: let user select timeline toolbar toolbutton style
    // connect(toolBar(), &QToolBar::iconSizeChanged, m_timelineToolBar, &QToolBar::setToolButtonStyle);
    m_timelineToolBar->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_timelineToolBar, &QWidget::customContextMenuRequested, this, &MainWindow::showTimelineToolbarMenu);

    QAction *prevRender = actionCollection()->action(QStringLiteral("prerender_timeline_zone"));
    QAction *stopPrevRender = actionCollection()->action(QStringLiteral("stop_prerender_timeline"));
    tlMenu->addAction(stopPrevRender);
    tlMenu->addAction(actionCollection()->action(QStringLiteral("set_render_timeline_zone")));
    tlMenu->addAction(actionCollection()->action(QStringLiteral("unset_render_timeline_zone")));
    tlMenu->addAction(actionCollection()->action(QStringLiteral("clear_render_timeline_zone")));

    tlMenu->addAction(actionCollection()->action(QStringLiteral("preview_using_proxy")));
    tlMenu->addAction(actionCollection()->action(QStringLiteral("preview_auto_render")));
    tlMenu->addSeparator();
    tlMenu->addAction(actionCollection()->action(QStringLiteral("disable_preview")));
    tlMenu->addAction(actionCollection()->action(QStringLiteral("manage_cache")));
    timelinePreview->defineDefaultAction(prevRender, stopPrevRender);
    timelinePreview->setAutoRaise(true);

    QAction *showRender = actionCollection()->action(QStringLiteral("project_render"));
    tlrMenu->addAction(showRender);
    tlrMenu->addAction(actionCollection()->action(QStringLiteral("stop_project_render")));
    timelineRender->defineDefaultAction(showRender, showRender);
    timelineRender->setAutoRaise(true);

    // Populate encoding profiles
    KConfig conf(QStringLiteral("encodingprofiles.rc"), KConfig::CascadeConfig, QStandardPaths::AppDataLocation);
    if (KdenliveSettings::grab_parameters().isEmpty() || KdenliveSettings::grab_extension().isEmpty()) {
        KConfigGroup group(&conf, "screengrab");
        QMap<QString, QString> values = group.entryMap();
        QMapIterator<QString, QString> i(values);
        if (i.hasNext()) {
            i.next();
            QString grabstring = i.value();
            KdenliveSettings::setGrab_parameters(grabstring.section(QLatin1Char(';'), 0, 0));
            KdenliveSettings::setGrab_extension(grabstring.section(QLatin1Char(';'), 1, 1));
        }
    }
    if (KdenliveSettings::decklink_parameters().isEmpty() || KdenliveSettings::decklink_extension().isEmpty()) {
        KConfigGroup group(&conf, "decklink");
        QMap<QString, QString> values = group.entryMap();
        QMapIterator<QString, QString> i(values);
        if (i.hasNext()) {
            i.next();
            QString decklinkstring = i.value();
            KdenliveSettings::setDecklink_parameters(decklinkstring.section(QLatin1Char(';'), 0, 0));
            KdenliveSettings::setDecklink_extension(decklinkstring.section(QLatin1Char(';'), 1, 1));
        }
    }
    if (!QDir(KdenliveSettings::currenttmpfolder()).isReadable())
        KdenliveSettings::setCurrenttmpfolder(QStandardPaths::writableLocation(QStandardPaths::TempLocation));

#ifdef USE_JOGSHUTTLE
    new JogManager(this);
#endif
    m_timelineTabs->populateActions(actionCollection());
    connect(qApp, &QApplication::focusChanged, this, &MainWindow::updateDeleteAction);
    updateDeleteAction();
    m_scopesManager->slotCheckActiveScopes();
    connect(qApp, &QGuiApplication::applicationStateChanged, this, [&](Qt::ApplicationState state) {
        if (state == Qt::ApplicationActive && getCurrentTimeline()) {
            getCurrentTimeline()->regainFocus();
        }
    });
    connect(this, &MainWindow::removeBinDock, this, &MainWindow::slotRemoveBinDock);
    // m_messageLabel->setMessage(QStringLiteral("This is a beta version. Always backup your data"), MltError);

    QAction *const showMenuBarAction = actionCollection()->action(KStandardAction::name(KStandardAction::ShowMenubar));
    // FIXME: workaround for BUG 171080
    showMenuBarAction->setChecked(!menuBar()->isHidden());

    // after the QMenuBar has been initialised
    m_hamburgerMenu->setMenuBar(menuBar());
    m_hamburgerMenu->setShowMenuBarAction(showMenuBarAction);

    // Detect shortcut conflicts bewtween mainwindow and media browser
    pCore->mediaBrowser()->detectShortcutConflicts();

    connect(toolBar(), &KToolBar::visibilityChanged, this, [&](bool visible) {
        if (visible && !toolBar()->actions().contains(m_hamburgerMenu)) {
            // hack to be able to insert the hamburger menu at the first position
            QAction *const firstChild = toolBar()->actionAt(toolBar()->height() / 2, toolBar()->height() / 2);
            QAction *const separator = toolBar()->insertSeparator(firstChild);
            toolBar()->insertAction(separator, m_hamburgerMenu);
            m_hamburgerMenu->hideActionsOf(toolBar());
        }
    });

    m_loadingDialog = new QProgressDialog(this);
    m_loadingDialog->setWindowFlags((m_loadingDialog->windowFlags() | Qt::CustomizeWindowHint) & ~Qt::WindowCloseButtonHint & ~Qt::WindowSystemMenuHint);
    m_loadingDialog->setMinimumDuration(0);
    m_loadingDialog->setMaximum(0);
    m_loadingDialog->setWindowTitle(i18nc("@title:window", "Loading Project"));
    m_loadingDialog->setCancelButton(nullptr);
    m_loadingDialog->setAutoClose(false);
    m_loadingDialog->setAutoReset(false);
    m_loadingDialog->setModal(true);
    m_loadingDialog->close();
    if (!KdenliveSettings::showtitlebars()) {
        Q_EMIT pCore->hideBars(true);
    }

    // fix for Bug 376053: intercept "Configure Toolbars" to prevent shortcut reset
    QAction *tbAction = actionCollection()->action(KStandardAction::name(KStandardAction::ConfigureToolbars));
    if (tbAction) {
        // disconnect EVERYTHING currently connected to it
        disconnect(tbAction, nullptr, nullptr, nullptr);

        // connect to a custom slot
        connect(tbAction, &QAction::triggered, this, &MainWindow::slotEditToolbars);
    }
}

void MainWindow::finishUiSetup()
{
    pCore->restoreLayout();
    Q_EMIT pCore->closeSplash();
    setAutoSaveSettings();
    QObject::disconnect(pCore.get(), &Core::GUISetupDone, this, nullptr);
    // This should connect only after splash is done
    connect(pCore.get(), &Core::loadingMessageNewStage, this, [&](const QString &message, int max = -1) {
        if (max > -1) {
            m_loadingDialog->reset();
            m_loadingDialog->setMaximum(max);
        }
        if (!message.isEmpty()) {
            m_loadingDialog->setLabelText(message);
        }
        m_loadingDialog->show();
    });
    connect(pCore.get(), &Core::loadingMessageIncrease, this, [&]() { m_loadingDialog->setValue(m_loadingDialog->value() + 1); });
    connect(pCore.get(), &Core::loadingMessageHide, this, [&]() {
        m_loadingDialog->reset();
        m_loadingDialog->setMaximum(0);
        m_loadingDialog->close();
    });
}

void MainWindow::loadBins(QStringList binInfo)
{
    // Delete all secondary bins
    while (m_binWidgets.size() > 1) {
        int ix = 0;
        auto bin = m_binWidgets.at(ix);
        if (bin->isMainBin()) {
            ix = 1;
            bin = m_binWidgets.at(ix);
        }
        auto toDelete = bin->parentWidget();
        m_binWidgets.takeAt(ix);
        delete toDelete;
    }

    // Create missing bins
    Bin *mainBin = nullptr;
    if (!m_binWidgets.isEmpty()) {
        // Main bin still there
        mainBin = m_binWidgets.first();
    }

    QStringList existingNames;
    for (const QString &info : binInfo) {
        if (mainBin && info.startsWith("project_bin:")) {
            // Main Bin, don't recreate
            continue;
        }
        auto bin = new Bin(pCore->projectItemModel(), this, false);
        addBin(bin, QString(), false, info.section(QLatin1Char(':'), 0, 0));
    }
}

void MainWindow::loadContainerActions()
{
    // Build all actions using the static_cast<QMenu *>(factory()->container method
    // That need to be rebuild when updating the ui.rc file

    // Ensure the "Monitor Config" menu doesn't replace the settings dialog on Mac because of text heuristics
    auto *monitorConfig = qobject_cast<QMenu *>(factory()->container(QStringLiteral("monitor_config"), this));
    if (monitorConfig) {
        monitorConfig->menuAction()->setMenuRole(QAction::NoRole);
    }

    // Connect monitor overlay info menu.
    QMenu *monitorOverlay = static_cast<QMenu *>(factory()->container(QStringLiteral("monitor_config_overlay"), this));
    if (monitorOverlay) {
        connect(monitorOverlay, &QMenu::triggered, this, &MainWindow::slotSwitchMonitorOverlay);

        m_projectMonitor->setupMenu(static_cast<QMenu *>(factory()->container(QStringLiteral("monitor_go"), this)), monitorOverlay, m_playZone,
                                    m_playZoneFromCursor, m_loopZone, m_loopClip);
        m_clipMonitor->setupMenu(static_cast<QMenu *>(factory()->container(QStringLiteral("monitor_go"), this)), monitorOverlay, m_playZone,
                                 m_playZoneFromCursor, m_loopZone, nullptr);
    }

    QMenu *clipInTimeline = static_cast<QMenu *>(factory()->container(QStringLiteral("clip_in_timeline"), this));
    clipInTimeline->setIcon(QIcon::fromTheme(QStringLiteral("go-jump")));

    QMenu *m = static_cast<QMenu *>(factory()->container(QStringLiteral("video_effects_menu"), this));
    connect(m, &QMenu::triggered, this, &MainWindow::slotAddEffect);

    QMenu *addMenu = static_cast<QMenu *>(factory()->container(QStringLiteral("generators"), this));
    Generators::getGenerators(KdenliveSettings::producerslist(), addMenu);
    connect(addMenu, &QMenu::triggered, this, &MainWindow::buildGenerator);
}

MainWindow::~MainWindow()
{
    disconnect(qApp, &QApplication::focusChanged, this, &MainWindow::updateDeleteAction);
    QObject::disconnect(m_deleteActionStateConnection);
    pCore->prepareShutdown();
    delete m_timelineTabs;
    if (m_audioSpectrum) {
        delete m_audioSpectrum;
    }
    if (m_projectMonitor) {
        m_projectMonitor->stop();
    }
    if (m_clipMonitor) {
        m_clipMonitor->stop();
    }
    // Core::mediaUnavailable.reset();
    delete m_projectMonitor;
    delete m_clipMonitor;
    delete m_shortcutRemoveFocus;
    delete m_effectList2;
    delete m_compositionList;
    delete m_loadingDialog;
    pCore->finishShutdown();
    qDeleteAll(m_transitions);
    Mlt::Factory::close();
}

// virtual
bool MainWindow::queryClose()
{
    if (m_renderWidget) {
        int waitingJobs = m_renderWidget->waitingJobsCount();
        int runningJobs = m_renderWidget->runningJobsCount();
        if (runningJobs > 0) {
            switch (KMessageBox::warningTwoActionsCancel(this,
                                                         i18np("You have 1 rendering job running.\nWhat do you want to do with this job?",
                                                               "You have %1 rendering jobs running.\nWhat do you want to do with these jobs?", runningJobs),
                                                         QString(), KGuiItem(i18n("Continue rendering in the background")), KGuiItem(i18n("Abort rendering")),
                                                         KStandardGuiItem::cancel(), QStringLiteral("warnOnRenderExit"))) {
            case KMessageBox::PrimaryAction:
                // create script with waiting jobs and start it
                if (waitingJobs > 0 && !m_renderWidget->startWaitingRenderJobs()) {
                    return false;
                }
                break;
            case KMessageBox::SecondaryAction:
                // Abort the jobs
                Q_EMIT abortAllRenderJobs();
                break;
            default:
                return false;
            }
        } else if (waitingJobs > 0) {
            switch (KMessageBox::warningTwoActionsCancel(this,
                                                         i18np("You have 1 rendering job waiting in the queue.\nWhat do you want to do with this job?",
                                                               "You have %1 rendering jobs waiting in the queue.\nWhat do you want to do with these jobs?",
                                                               waitingJobs),
                                                         QString(), KGuiItem(i18n("Start them now")), KGuiItem(i18n("Delete them")))) {
            case KMessageBox::PrimaryAction:
                // create script with waiting jobs and start it
                if (!m_renderWidget->startWaitingRenderJobs()) {
                    return false;
                }
                break;
            case KMessageBox::SecondaryAction:
                // Don't do anything, jobs will be deleted
                break;
            default:
                return false;
            }
        }
    }
    // WARNING: According to KMainWindow::queryClose documentation we are not supposed to close the document here?
    KDDockWidgets::LayoutSaver dockLayout(KDDockWidgets::RestoreOption_AbsoluteFloatingDockWindows);
    KdenliveSettings::setKdockLayout(QString(dockLayout.serializeLayout()));
    // setAutoSaveSettings(QStringLiteral("MainWindow"), false);
    if (!pCore->projectManager()->closeCurrentDocument(true, true)) {
        return false;
    }
    saveOptions();
    m_windowClosing = true;
    return true;
}

void MainWindow::buildGenerator(QAction *action)
{
    Generators gen(action->data().toString(), this);
    if (gen.exec() == QDialog::Accepted) {
        pCore->activeBin()->slotAddClipToProject(gen.getSavedClip());
    }
}

void MainWindow::saveProperties(KConfigGroup &config)
{
    // save properties here
    KXmlGuiWindow::saveProperties(config);
    // TODO: fix session management
    if (qApp->isSavingSession() && pCore->projectManager()) {
        if (pCore->currentDoc() && !pCore->currentDoc()->url().isEmpty()) {
            config.writeEntry("kdenlive_lastUrl", pCore->currentDoc()->url().toLocalFile());
        }
    }
}

void MainWindow::saveNewToolbarConfig()
{
    // Sync current changes
    KXmlGuiWindow::saveNewToolbarConfig();
    // All dynamically inserted actions are removed by the save toolbar
    // So re-add them manually....
    loadDockActions();
    loadClipActions();
    loadContainerActions();
    for (auto &bin : m_binWidgets) {
        bin->setupGeneratorMenu();
    }

    // hack to be able to insert the hamburger menu at the first position
    QAction *const firstChild = toolBar()->actionAt(toolBar()->height() / 2, toolBar()->height() / 2);
    QAction *const separator = toolBar()->insertSeparator(firstChild);
    toolBar()->insertAction(separator, m_hamburgerMenu);
    m_hamburgerMenu->hideActionsOf(toolBar());
}

void MainWindow::slotReloadEffects(const QStringList &paths)
{
    for (const QString &p : paths) {
        EffectsRepository::get()->reloadCustom(p);
    }
    m_effectList2->reloadEffectMenu(m_effectsMenu, m_effectActions);
}

void MainWindow::configureNotifications()
{
    KNotifyConfigWidget::configure(this);
}

void MainWindow::slotFullScreen()
{
    KToggleFullScreenAction::setFullScreen(this, actionCollection()->action(QStringLiteral("fullscreen"))->isChecked());
}

void MainWindow::slotConnectMonitors()
{
    // connect(m_projectList, SIGNAL(deleteProjectClips(QStringList,QMap<QString,QString>)), this,
    // SLOT(slotDeleteProjectClips(QStringList,QMap<QString,QString>)));
    connect(m_projectMonitor, &Monitor::createSplitOverlay, this, &MainWindow::createSplitOverlay, Qt::DirectConnection);
    connect(m_projectMonitor, &Monitor::removeSplitOverlay, this, &MainWindow::removeSplitOverlay, Qt::DirectConnection);
}

void MainWindow::createSplitOverlay(std::shared_ptr<Mlt::Filter> filter)
{
    if (m_assetPanel->effectStackOwner().type == KdenliveObjectType::TimelineClip) {
        getCurrentTimeline()->controller()->createSplitOverlay(m_assetPanel->effectStackOwner().itemId, filter);
        m_projectMonitor->activateSplit();
    } else {
        pCore->displayMessage(i18n("Select a clip to compare effect"), ErrorMessage);
    }
}

void MainWindow::removeSplitOverlay()
{
    getCurrentTimeline()->controller()->removeSplitOverlay();
}



void MainWindow::saveOptions()
{
    KdenliveSettings::self()->save();
    pCore->projectManager()->saveRecentFiles();
}

bool MainWindow::readOptions()
{
    KSharedConfigPtr config = KSharedConfig::openConfig();
    pCore->projectManager()->recentFilesAction()->loadEntries(KConfigGroup(config, "Recent Files"));

    if (KdenliveSettings::defaultprojectfolder().isEmpty()) {
        QDir dir(QStandardPaths::writableLocation(QStandardPaths::MoviesLocation));
        dir.mkpath(QStringLiteral("."));
        KdenliveSettings::setDefaultprojectfolder(dir.absolutePath());
    }

    if (KdenliveSettings::trackheight() == 0) {
        QFont ft = QFontDatabase::systemFont(QFontDatabase::SmallestReadableFont);
        // Height of the icon row
        int baseUnit = qMax(28, qCeil(QFontInfo(ft).pixelSize() * 1.8));
        int trackHeight = baseUnit + qMax(22, qCeil(QFontInfo(ft).pixelSize() * 2.5) + 6);
        KdenliveSettings::setTrackheight(trackHeight);
    }
    bool firstRun = false;
    KConfigGroup initialGroup(config, "version");
    if (!initialGroup.exists() || KdenliveSettings::sdlAudioBackend().isEmpty() || KdenliveSettings::kdenliverendererpath().isEmpty()) {
        // First run, check if user is on a KDE Desktop
        firstRun = true;
        // Define default video location for first run
        KRecentDirs::add(QStringLiteral(":KdenliveClipFolder"), QStandardPaths::writableLocation(QStandardPaths::MoviesLocation));
        KRecentDirs::add(QStringLiteral(":KdenliveProjectsFolder"), QStandardPaths::writableLocation(QStandardPaths::MoviesLocation));

        // this is our first run, show Wizard
        QPointer<Wizard> w = new Wizard(true);
        if (w->exec() == QDialog::Accepted && w->isOk()) {
            w->adjustSettings();
            delete w;
        } else {
            delete w;
            ::exit(1);
        }
    } else if (!KdenliveSettings::ffmpegpath().isEmpty() && !QFile::exists(KdenliveSettings::ffmpegpath())) {
        // Invalid entry for FFmpeg, check system
        QPointer<Wizard> w = new Wizard(true);
        if (w->exec() == QDialog::Accepted && w->isOk()) {
            w->adjustSettings();
        }
        delete w;
    }
    if (firstRun) {
        if (TransitionsRepository::get()->exists(QStringLiteral("qtblend")) && TransitionsRepository::get()->getVersion(QStringLiteral("qtblend")) > 200) {
            KdenliveSettings::setPreferredcomposite(QStringLiteral("qtblend"));
        }
    }
    initialGroup.writeEntry("version", version);
    if (KdenliveSettings::guidesCategories().isEmpty()) {
        KdenliveSettings::setGuidesCategories(KdenliveDoc::getDefaultGuideCategories());
    }
    return firstRun;
}

void MainWindow::slotRunWizard()
{
    QPointer<Wizard> w = new Wizard(false, this);
    if (w->exec() == QDialog::Accepted && w->isOk()) {
        w->adjustSettings();
    }
    delete w;
}

void MainWindow::slotRefreshProfiles()
{
    KdenliveSettingsDialog *d = static_cast<KdenliveSettingsDialog *>(KConfigDialog::exists(QStringLiteral("settings")));
    if (d) {
        d->checkProfile();
    }
}

void MainWindow::slotEditProjectSettings(int ix)
{
    KdenliveDoc *project = pCore->currentDoc();
    QPair<int, int> p = getCurrentTimeline()->getAvTracksCount();
    int channels = project->getDocumentProperty(QStringLiteral("audioChannels"), QStringLiteral("2")).toInt();
    std::unique_ptr<ProjectSettings> w(new ProjectSettings(project, project->metadata(), p.second, p.first, channels, true, !project->isModified(), this));
    if (ix > 0) {
        w->tabWidget->setCurrentIndex(ix);
    }
    connect(w.get(), &ProjectSettings::disableProxies, this, &MainWindow::slotDisableProxies);
    // connect(w, SIGNAL(disablePreview()), pCore->projectManager()->currentTimeline(), SLOT(invalidateRange()));
    connect(w.get(), &ProjectSettings::refreshProfiles, this, &MainWindow::slotRefreshProfiles);

    if (w->exec() == QDialog::Accepted) {
        QString profile = w->selectedProfile();
        bool modified = false;
        if (m_renderWidget) {
            m_renderWidget->updateDocumentPath();
        }
        const QStringList guidesCat = w->guidesCategories();
        if (guidesCat != project->guidesCategories()) {
            project->updateGuideCategories(guidesCat, w->remapGuidesCategories());
        }
        if (KdenliveSettings::videothumbnails() != w->enableVideoThumbs()) {
            slotSwitchVideoThumbs();
        }
        if (KdenliveSettings::audiothumbnails() != w->enableAudioThumbs()) {
            slotSwitchAudioThumbs();
        }
        if (project->getDocumentProperty(QStringLiteral("previewparameters")) != w->previewParams() ||
            project->getDocumentProperty(QStringLiteral("previewextension")) != w->previewExtension()) {
            modified = true;
            project->setDocumentProperty(QStringLiteral("previewparameters"), w->previewParams());
            project->setDocumentProperty(QStringLiteral("previewextension"), w->previewExtension());
            getCurrentTimeline()->controller()->clearPreviewRange(false);
        }

        bool proxiesChanged = false;
        if (project->getDocumentProperty(QStringLiteral("proxyparams")) != w->proxyParams() ||
            project->getDocumentProperty(QStringLiteral("proxyextension")) != w->proxyExtension()) {
            modified = true;
            proxiesChanged = true;
            project->setDocumentProperty(QStringLiteral("proxyparams"), w->proxyParams());
            project->setDocumentProperty(QStringLiteral("proxyextension"), w->proxyExtension());
        }
        if (project->getDocumentProperty(QStringLiteral("externalproxyparams")) != w->externalProxyParams()) {
            modified = true;
            proxiesChanged = true;
            project->setDocumentProperty(QStringLiteral("externalproxyparams"), w->externalProxyParams());
        }
        if (proxiesChanged && pCore->projectItemModel()->hasProxies() &&
            KMessageBox::questionTwoActions(this, i18n("You have changed the proxy parameters. Do you want to recreate all proxy clips for this project?"), {},
                                            KGuiItem(i18nc("@action:button", "Recreate")),
                                            KGuiItem(i18nc("@action:button", "Continue without"))) == KMessageBox::PrimaryAction) {
            pCore->bin()->rebuildProxies();
        }

        if (project->getDocumentProperty(QStringLiteral("generateproxy")) != QString::number(int(w->generateProxy()))) {
            modified = true;
            project->setDocumentProperty(QStringLiteral("generateproxy"), QString::number(int(w->generateProxy())));
        }
        if (project->getDocumentProperty(QStringLiteral("proxyminsize")) != QString::number(w->proxyMinSize())) {
            modified = true;
            project->setDocumentProperty(QStringLiteral("proxyminsize"), QString::number(w->proxyMinSize()));
        }
        if (project->getDocumentProperty(QStringLiteral("generateimageproxy")) != QString::number(int(w->generateImageProxy()))) {
            modified = true;
            project->setDocumentProperty(QStringLiteral("generateimageproxy"), QString::number(int(w->generateImageProxy())));
        }
        if (project->getDocumentProperty(QStringLiteral("proxyimageminsize")) != QString::number(w->proxyImageMinSize())) {
            modified = true;
            project->setDocumentProperty(QStringLiteral("proxyimageminsize"), QString::number(w->proxyImageMinSize()));
        }
        if (project->getDocumentProperty(QStringLiteral("proxyimagesize")) != QString::number(w->proxyImageSize())) {
            modified = true;
            project->setDocumentProperty(QStringLiteral("proxyimagesize"), QString::number(w->proxyImageSize()));
        }
        if (project->getDocumentProperty(QStringLiteral("proxyresize")) != QString::number(w->proxyResize())) {
            modified = true;
            project->setDocumentProperty(QStringLiteral("proxyresize"), QString::number(w->proxyResize()));
        }
        if (QString::number(int(w->useProxy())) != project->getDocumentProperty(QStringLiteral("enableproxy"))) {
            project->setDocumentProperty(QStringLiteral("enableproxy"), QString::number(int(w->useProxy())));
            modified = true;
            slotUpdateProxySettings();
        }
        if (QString::number(int(w->useExternalProxy())) != project->getDocumentProperty(QStringLiteral("enableexternalproxy"))) {
            project->setDocumentProperty(QStringLiteral("enableexternalproxy"), QString::number(int(w->useExternalProxy())));
            modified = true;
        }
        if (w->metadata() != project->metadata()) {
            project->setMetadata(w->metadata());
            if (m_renderWidget) {
                m_renderWidget->updateMetadataToolTip();
            }
        }

        // Check storage location for project files
        auto storageType = w->storageType();
        auto currentStorageInfo = project->projectTempFolder();
        ProjectStorageType previousStorageType = currentStorageInfo.second;
        const QString currentStorage = currentStorageInfo.first;
        QString pathToMove;
        KMessageBox::ButtonCode answer = KMessageBox::Cancel;
        if (previousStorageType == storageType) {
            if (storageType == StoreInCustomFolder) {
                // Check if custom folder changed
                if (QFileInfo(w->storageFolder()).absoluteFilePath() != QFileInfo(currentStorage).absoluteFilePath()) {
                    pathToMove = w->storageFolder();
                    answer = KMessageBox::warningContinueCancel(
                        this, i18n("This will move all temporary files from<br/><b>%1</b> to <b>%2</b>,<br/>the project file will then be reloaded",
                                   currentStorage, pathToMove));
                }
            }
        } else {
            qDebug() << "5555555555555555555555555555555555555\n\nDETECTING PROJECT STORAGE_: " << storageType << "\n\n5555555555555555555555";
            if (project->url().isEmpty() && storageType == StoreWithProjectFile) {
                // New project, first save then move
                answer = KMessageBox::warningContinueCancel(this, i18n("Project has not been saved.<br/>This will first save the project, then move "
                                                                       "all temporary files from <br/><b>%1</b> to the project file location and reload",
                                                                       currentStorage));
                if (answer == KMessageBox::Continue) {
                    project->setDocumentProperty(QStringLiteral("storagetype"), QString::number(int(storageType)));
                    pCore->projectManager()->saveFile();
                    /*QDir oldDir(currentStorage);
                    QString documentId = QDir::cleanPath(project->getDocumentProperty(QStringLiteral("documentid")));
                    bool ok;
                    documentId.toLongLong(&ok, 10);
                    if (!ok || documentId.isEmpty()) {
                        KMessageBox::error(this, i18n("Cannot perform operation, invalid document id: %1", documentId));
                        return;
                    }

                    const QString savePath = pCore->projectManager()->getSavePath();
                    if (savePath.isEmpty()) {
                        return;
                    }
                    pathToMove = QFileInfo(savePath).absolutePath() + QStringLiteral("/cachefiles");
                    QDir newDir(pathToMove);
                    // Update project's url, project will be saved after files are copied
                    project->setUrl(QUrl::fromLocalFile(savePath));
                    pCore->projectManager()->moveProjectData(storageType, oldDir.absoluteFilePath(documentId), newDir.absolutePath());*/
                    return;
                    // answer = KMessageBox::Cancel;
                }
            } else {
                qDebug() << "5555555555555555555555555555555555555\n\nDETECTING ALREADY SAVED PROJECT : " << storageType << "\n\n5555555555555555555555";
                answer = KMessageBox::Continue;
                if (storageType == StoreWithProjectFile) {
                    pathToMove = QFileInfo(project->url().toLocalFile()).absolutePath() + QStringLiteral("/cachefiles");
                } else if (storageType == StoreInCustomFolder) {
                    pathToMove = w->storageFolder();
                } else {
                    pathToMove = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
                }
                if (project->isModified()) {
                    // Project folder changed, moving data
                    answer = KMessageBox::warningContinueCancel(
                        this, i18n("The current project has not been saved.<br/>This will first save the project, then move "
                                   "all temporary files from <br/><b>%1</b> to <b>%2</b>,<br>and the project file will be reloaded",
                                   currentStorage, pathToMove));
                    if (answer == KMessageBox::Continue) {
                        pCore->projectManager()->saveFile();
                    }
                } else {
                    answer = KMessageBox::warningContinueCancel(
                        this, i18n("This will move all temporary files from<br/><b>%1</b> to <b>%2</b>,<br/>the project file will then be reloaded",
                                   currentStorage, pathToMove));
                }
            }
            if (answer == KMessageBox::Continue) {
                // Proceed with move
                QString documentId = QDir::cleanPath(project->getDocumentProperty(QStringLiteral("documentid")));
                bool ok;
                documentId.toLongLong(&ok, 10);
                if (!ok || documentId.isEmpty()) {
                    KMessageBox::error(this, i18n("Cannot perform operation, invalid document id: %1", documentId));
                } else {
                    QDir newDir(pathToMove);
                    QDir oldDir(currentStorage);
                    if (newDir.exists(documentId)) {
                        KMessageBox::error(this, i18n("Cannot perform operation, target directory already exists: %1", newDir.absoluteFilePath(documentId)));
                    } else {
                        // Proceed with the move
                        pCore->projectManager()->moveProjectData(storageType, oldDir.absoluteFilePath(documentId), newDir.absolutePath());
                    }
                }
            }
        }
        if (pCore->getCurrentProfile()->path() != profile || project->profileChanged(profile)) {
            if (!qFuzzyCompare(pCore->getCurrentProfile()->fps() - ProfileRepository::get()->getProfile(profile)->fps(), 0.)) {
                // Fps was changed, we save the project to an xml file with updated profile and reload project
                // Check if blank project
                if ((project->url().fileName().isEmpty() && !project->isModified()) || !pCore->bin()->hasUserClip()) {
                    // Trying to switch project profile from an empty project
                    pCore->setCurrentProfile(profile);
                    pCore->projectManager()->newFile(profile, false);
                    return;
                }
                pCore->projectManager()->saveWithUpdatedProfile(profile);
            } else {
                bool darChanged = !qFuzzyCompare(pCore->getCurrentProfile()->dar(), ProfileRepository::get()->getProfile(profile)->dar());
                pCore->setCurrentProfile(profile);
                pCore->projectManager()->slotResetProfiles(darChanged);
                slotUpdateDocumentState(true);
            }
        } else if (modified) {
            project->setModified();
        }
    }
}

void MainWindow::slotDisableProxies()
{
    pCore->currentDoc()->setDocumentProperty(QStringLiteral("enableproxy"), QString::number(false));
    pCore->currentDoc()->setModified();
    slotUpdateProxySettings();
}

void MainWindow::slotStopRenderProject()
{
    if (m_renderWidget) {
        m_renderWidget->slotAbortCurrentJob();
    }
}

void MainWindow::updateProjectPath(const QString &path)
{
    if (m_renderWidget) {
        m_renderWidget->resetRenderPath(path);
    } else {
        // Clear render name as project url changed
        QMap<QString, QString> renderProps;
        renderProps.insert(QStringLiteral("renderurl"), QString());
        slotSetDocumentRenderProfile(renderProps);
    }
}

void MainWindow::slotRenderProject()
{
    KdenliveDoc *project = pCore->currentDoc();

    if (!m_renderWidget && project) {
        m_renderWidget = new RenderWidget(project->useProxy(), this);
        connect(m_renderWidget, &RenderWidget::shutdown, this, &MainWindow::slotShutdown);
        connect(m_renderWidget, &RenderWidget::selectedRenderProfile, this, &MainWindow::slotSetDocumentRenderProfile);
        connect(m_renderWidget, &RenderWidget::abortProcess, this, &MainWindow::abortRenderJob);
        connect(pCore.get(), &Core::gotMissingClipsCount, m_renderWidget, &RenderWidget::updateMissingClipsCount);
        connect(pCore.get(), &Core::updateRenderOffset, m_renderWidget, &RenderWidget::updateRenderOffset);
        connect(this, &MainWindow::updateRenderWidgetProfile, m_renderWidget, &RenderWidget::adjustViewToProfile);
        m_renderWidget->setGuides(project->getGuideModel(getCurrentTimeline()->getUuid()));
        m_renderWidget->updateDocumentPath();
        m_renderWidget->setRenderProfile(project->getRenderProperties());
    }

    slotCheckRenderStatus();
    if (m_renderWidget) {
        m_renderWidget->showNormal();
        m_renderWidget->activateWindow();
        m_renderWidget->raise();
    }

    // What are the following lines supposed to do?
    // m_renderWidget->enableAudio(false);
    // m_renderWidget->export_audio;
}

void MainWindow::slotCheckRenderStatus()
{
    // Make sure there are no missing clips
    // TODO
    /*if (m_renderWidget)
        m_renderWidget->missingClips(pCore->bin()->hasMissingClips());*/
}

void MainWindow::setRenderingProgress(const QString &url, int progress, int frame)
{
    Q_EMIT setRenderProgress(progress);
    if (m_renderWidget) {
        m_renderWidget->setRenderProgress(url, progress, frame);
    }
}

void MainWindow::setRenderingFinished(const QString &url, int status, const QString &error)
{
    Q_EMIT setRenderProgress(100);
    if (m_renderWidget) {
        m_renderWidget->setRenderStatus(url, status, error);
    }
}

void MainWindow::addProjectClip(const QString &url, const QString &folder)
{
    if (pCore->currentDoc()) {
        QStringList ids = pCore->projectItemModel()->getClipByUrl(QFileInfo(url));
        if (!ids.isEmpty()) {
            // Clip is already in project bin, abort
            return;
        }
        ClipCreator::createClipFromFile(url, folder, pCore->projectItemModel());
    }
}

void MainWindow::addTimelineClip(const QString &url)
{
    if (pCore->currentDoc()) {
        QStringList ids = pCore->projectItemModel()->getClipByUrl(QFileInfo(url));
        if (!ids.isEmpty()) {
            pCore->selectBinClip(ids.constFirst());
            slotInsertClipInsert();
        }
    }
}

void MainWindow::scriptRender(const QString &url)
{
    Q_UNUSED(url)
    slotRenderProject();
    m_renderWidget->slotPrepareExport(true);
}

#ifndef NODBUS
void MainWindow::exitApp()
{
    QApplication::exit(0);
}
#endif

void MainWindow::slotCleanProject()
{
    if (KMessageBox::warningContinueCancel(this, i18n("This will remove all unused clips from your project."), i18n("Clean up project")) ==
        KMessageBox::Cancel) {
        return;
    }
    pCore->projectItemModel()->requestCleanupUnused();
}

void MainWindow::slotUpdateMousePosition(int pos, int duration)
{
    if (!pCore->currentDoc()) {
        return;
    }
    if (duration < 0) {
        duration = getCurrentTimeline()->controller()->duration();
    }
    if (pos >= 0) {
        m_mousePosition = pos;
    }
    switch (m_timeFormatButton->currentItem()) {
    case 0:
        m_timeFormatButton->setText(pCore->currentDoc()->timecode().getTimecodeFromFrames(m_mousePosition) + QStringLiteral(" / ") +
                                    pCore->currentDoc()->timecode().getTimecodeFromFrames(duration));
        break;
    default:
        m_timeFormatButton->setText(QStringLiteral("%1 / %2").arg(m_mousePosition, 6, 10, QLatin1Char('0')).arg(duration, 6, 10, QLatin1Char('0')));
    }
}

void MainWindow::slotUpdateProjectDuration(int duration)
{
    if (pCore->currentDoc()) {
        slotUpdateMousePosition(-1, duration);
    }
    if (m_renderWidget) {
        m_renderWidget->projectDurationChanged(duration);
    }
}

void MainWindow::slotUpdateZoneDuration()
{
    if (m_renderWidget) {
        m_renderWidget->zoneDurationChanged();
    }
}

void MainWindow::slotUpdateDocumentState(bool modified)
{
    m_timelineTabs->updateWindowTitle();
    setWindowModified(modified);
    m_saveAction->setEnabled(modified);
}

void MainWindow::connectDocument()
{
    KdenliveDoc *project = pCore->currentDoc();
    connect(project, &KdenliveDoc::startAutoSave, pCore->projectManager(), &ProjectManager::slotStartAutoSave);
    connect(project, &KdenliveDoc::reloadEffects, this, &MainWindow::slotReloadEffects);
    KdenliveSettings::setProject_fps(pCore->getCurrentFps());
    slotSwitchTimelineZone(project->getDocumentProperty(QStringLiteral("enableTimelineZone")).toInt() == 1);
    slotUpdateProjectDuration(getCurrentTimeline()->model()->duration() - 1);
    const QUuid uuid = getCurrentTimeline()->getUuid();
    m_clipMonitor->updateDocumentUuid();
    pCore->guidesList()->refreshDar();
    connect(m_projectMonitor, &Monitor::multitrackView, getCurrentTimeline()->controller(), &TimelineController::slotMultitrackView, Qt::UniqueConnection);
    connect(m_projectMonitor, &Monitor::activateTrack, getCurrentTimeline()->controller(), &TimelineController::activateTrackAndSelect, Qt::UniqueConnection);
    connect(getCurrentTimeline()->controller(), &TimelineController::timelineClipSelected, this, [&](bool selected) {
        m_loopClip->setEnabled(selected);
        Q_EMIT pCore->library()->enableAddSelection(selected);
    });
    connect(pCore->library(), &LibraryWidget::saveTimelineSelection, getCurrentTimeline()->controller(), &TimelineController::saveTimelineSelection,
            Qt::UniqueConnection);
    connect(pCore->mixer(), &MixerManager::purgeCache, m_projectMonitor, &Monitor::purgeCache);
    connect(m_projectMonitor, &Monitor::zoneUpdated, project, [project](const QPoint &) { project->setModified(); });
    connect(m_clipMonitor, &Monitor::zoneUpdated, project, [project](const QPoint &) { project->setModified(); });
    connect(project, &KdenliveDoc::docModified, this, &MainWindow::slotUpdateDocumentState);

    if (m_renderWidget) {
        slotCheckRenderStatus();
        m_renderWidget->setGuides(pCore->currentDoc()->getGuideModel(uuid));
        m_renderWidget->updateDocumentPath();
        m_renderWidget->setRenderProfile(project->getRenderProperties());
        m_renderWidget->updateMetadataToolTip();
    }

    m_commandStack->setActiveStack(project->commandStack().get());
    m_timelineTabs->updateWindowTitle();
    setWindowModified(project->isModified());
    m_saveAction->setEnabled(project->isModified());
    m_normalEditTool->setChecked(true);
    connect(m_projectMonitor, &Monitor::durationChanged, this, &MainWindow::slotUpdateProjectDuration);
    connect(m_projectMonitor, &Monitor::zoneDurationChanged, this, &MainWindow::slotUpdateZoneDuration);
    connect(m_effectList2, &EffectListWidget::reloadFavorites, getCurrentTimeline(), &TimelineWidget::updateEffectFavorites);
    connect(m_compositionList, &TransitionListWidget::reloadFavorites, getCurrentTimeline(), &TimelineWidget::updateTransitionFavorites);
    connect(pCore.get(), &Core::processDragEnd, getCurrentTimeline(), &TimelineWidget::endDrag);

    // Load master effect zones
    getCurrentTimeline()->controller()->updateMasterZones(getCurrentTimeline()->model()->getMasterEffectZones());

    // Set bin effects status
    bool binEffectsDisabled = project->getDocumentProperty(QStringLiteral("disablebineffects")).toInt() == 1;
    if (binEffectsDisabled) {
        pCore->projectItemModel()->setBinEffectsEnabled(!binEffectsDisabled);
    }

    // Set timeline effects status
    bool disabled = project->getDocumentProperty(QStringLiteral("disabletimelineeffects")).toInt() == 1;
    QAction *disableEffects = actionCollection()->action(QStringLiteral("disable_timeline_effects"));
    if (disableEffects) {
        if (disabled != disableEffects->isChecked()) {
            disableEffects->blockSignals(true);
            disableEffects->setChecked(disabled);
            disableEffects->blockSignals(false);
        }
    }

    m_buttonSelectTool->setChecked(true);
    connect(m_projectMonitorDock, &KDDockWidgets::QtWidgets::DockWidget::isOpenChanged, m_projectMonitor, &Monitor::slotRefreshMonitor, Qt::UniqueConnection);
    connect(m_clipMonitorDock, &KDDockWidgets::QtWidgets::DockWidget::isOpenChanged, m_clipMonitor, &Monitor::slotRefreshMonitor, Qt::UniqueConnection);
    // Initialize audio mixer
    getCurrentTimeline()->model()->rebuildMixer();
    getCurrentTimeline()->focusTimeline();
}

void MainWindow::slotEditKeys()
{
    KShortcutsDialog *dialog = new KShortcutsDialog(KShortcutsEditor::AllActions, KShortcutsEditor::LetterShortcutsAllowed, this);

    KNSWidgets::Action *downloadKeybordSchemes =
        new KNSWidgets::Action(i18n("Download New Keyboard Schemes…"), QStringLiteral(":data/kdenlive_keyboardschemes.knsrc"), this);
    connect(downloadKeybordSchemes, &KNSWidgets::Action::dialogFinished, this, [&, dialog](const QList<KNSCore::Entry> &changedEntries) {
        if (changedEntries.count() > 0) {
            for (auto &entry : changedEntries) {
                const QStringList files = entry.installedFiles();
                for (auto &file : files) {
                    KConfig conf(file);
                    KConfigGroup group(&conf, "Shortcuts");
                    if (group.exists()) {
                        QMap<QString, QString> values = group.entryMap();
                        // This is an ui.rc file, convert to xml
                        QDomDocument doc;
                        auto node = doc.createElement(QStringLiteral("gui"));
                        node.setAttribute(QStringLiteral("name"), QStringLiteral("kdenlive"));
                        node.setAttribute(QStringLiteral("version"), QStringLiteral("1"));
                        doc.appendChild(node);
                        auto list = doc.createElement(QStringLiteral("ActionProperties"));
                        node.appendChild(list);
                        for (auto i = values.cbegin(), end = values.cend(); i != end; ++i) {
                            if (i.value() == QLatin1String("none")) {
                                continue;
                            }
                            auto e = doc.createElement(QStringLiteral("Action"));
                            e.setAttribute(QStringLiteral("name"), i.key());
                            e.setAttribute(QStringLiteral("shortcut"), i.value());
                            list.appendChild(e);
                        }
                        QFile f(file);
                        if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) {
                            qDebug() << "/// COULD NOT open shortcut file : " << file;
                            continue;
                        }
                        QTextStream stream(&f);
                        stream << doc.toString();
                        f.close();
                    }
                }
            }
            dialog->refreshSchemes();
        }
    });

    dialog->addActionToSchemesMoreButton(downloadKeybordSchemes);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->addCollection(actionCollection(), i18nc("general keyboard shortcuts", "General"));

    // Update the shortcut conflicts list bewtween mainwindow and media browser
    connect(dialog, &KShortcutsDialog::saved, this, [this] {
        pCore->mediaBrowser()->detectShortcutConflicts();
        factory()->refreshActionProperties();
    });
    dialog->configure(true);
}

void MainWindow::slotPreferences()
{
    slotShowPreferencePage(m_lastConfigPage);
}

void MainWindow::slotShowPreferencePage(Kdenlive::ConfigPage page, int option)
{
    /*
     * An instance of your dialog could be already created and could be
     * cached, in which case you want to display the cached dialog
     * instead of creating another one
     */
    if (KConfigDialog::showDialog(QStringLiteral("settings"))) {
        if (page != Kdenlive::NoPage) {
            KdenliveSettingsDialog *d = static_cast<KdenliveSettingsDialog *>(KConfigDialog::exists(QStringLiteral("settings")));
            d->showPage(page, option);
        }
        return;
    }

    // KConfigDialog didn't find an instance of this dialog, so lets
    // create it :

    // Get the mappable actions in localized form
    QMap<QString, QString> actions;
    KActionCollection *collection = actionCollection();
    for (const QString &action_name : std::as_const(m_actionNames)) {
        const QString action_text = KLocalizedString::removeAcceleratorMarker(collection->action(action_name)->text());
        actions[action_text] = action_name;
    }

    auto *dialog = new KdenliveSettingsDialog(actions, m_gpuAllowed, this);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    connect(dialog, &KConfigDialog::settingsChanged, this, &MainWindow::updateConfiguration);
    connect(dialog, &KConfigDialog::settingsChanged, this, &MainWindow::configurationChanged);
    connect(dialog, &KdenliveSettingsDialog::doResetConsumer, this, [this](bool fullReset) {
        m_scaleGroup->setEnabled(!KdenliveSettings::external_display());
        pCore->projectManager()->slotResetConsumers(fullReset);
    });
    connect(dialog, &KdenliveSettingsDialog::checkTabPosition, this, &MainWindow::slotCheckTabPosition);
    connect(dialog, &KdenliveSettingsDialog::restartKdenlive, this, &MainWindow::slotRestart);
    connect(dialog, &KdenliveSettingsDialog::updateLibraryFolder, pCore.get(), &Core::updateLibraryPath);
    connect(dialog, &KdenliveSettingsDialog::resetView, this, &MainWindow::resetTimelineTracks);
    connect(KdenliveSettings::self(), &KdenliveSettings::window_backgroundChanged, pCore->monitorManager(), &MonitorManager::updateBgColor);

    dialog->show();
    if (page != Kdenlive::NoPage) {
        dialog->showPage(page, option);
    }
}

void MainWindow::slotCheckTabPosition()
{
    int pos = KDDockWidgets::Config::self().tabsAtBottom() ? 1 : 0;
    if (KdenliveSettings::tabposition() != pos) {
        KDDockWidgets::Config::self().setTabsAtBottom(pos == 1);
    }
}

void MainWindow::slotRestart(bool clean)
{
    if (clean) {
        if (KMessageBox::warningContinueCancel(this,
                                               i18n("This will delete Kdenlive's configuration file and restart the application. Do you want to proceed?"),
                                               i18nc("@title:window", "Reset Configuration")) != KMessageBox::Continue) {
            return;
        }
    }
    cleanRestart(clean, false);
}

void MainWindow::cleanRestart(bool clean, bool forceQuit)
{
    m_exitCode = clean ? EXIT_CLEAN_RESTART : EXIT_RESTART;
    QApplication::closeAllWindows();
    if (forceQuit) {
        // qApp->quit();
        qApp->exit(m_exitCode);
    }
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    qDebug() << ":::: CLOSE EVENT REQUESTED....";
    KXmlGuiWindow::closeEvent(event);
    if (event->isAccepted()) {
        QApplication::exit(m_exitCode);
        return;
    }
}

void MainWindow::updateConfiguration()
{
    // TODO: we should apply settings to all projects, not only the current one
    m_buttonAudioThumbs->setChecked(KdenliveSettings::audiothumbnails());
    m_buttonVideoThumbs->setChecked(KdenliveSettings::videothumbnails());
    m_buttonShowMarkers->setChecked(KdenliveSettings::showmarkers());

    // Update list of transcoding profiles
    buildDynamicActions();
    loadClipActions();
}

void MainWindow::slotSwitchVideoThumbs()
{
    KdenliveSettings::setVideothumbnails(!KdenliveSettings::videothumbnails());
    m_buttonVideoThumbs->setChecked(KdenliveSettings::videothumbnails());
}

void MainWindow::slotSwitchAudioThumbs()
{
    KdenliveSettings::setAudiothumbnails(!KdenliveSettings::audiothumbnails());
    pCore->bin()->checkAudioThumbs();
    m_buttonAudioThumbs->setChecked(KdenliveSettings::audiothumbnails());
    // Preserve icon colorization based on zoom level after toggle
    updateAudioWaveformActionIcon();
}

void MainWindow::slotSwitchMarkersComments()
{
    KdenliveSettings::setShowmarkers(!KdenliveSettings::showmarkers());
    m_buttonShowMarkers->setChecked(KdenliveSettings::showmarkers());
}

void MainWindow::slotSwitchSnap()
{
    KdenliveSettings::setSnaptopoints(!KdenliveSettings::snaptopoints());
    m_buttonSnap->setChecked(KdenliveSettings::snaptopoints());
}

void MainWindow::slotSwitchClipOverlays()
{
    KdenliveSettings::setShowClipOverlays(!KdenliveSettings::showClipOverlays());
    m_buttonHideClipOverlays->setChecked(KdenliveSettings::showClipOverlays());
}

void MainWindow::slotShowTimelineTags()
{
    KdenliveSettings::setTagsintimeline(!KdenliveSettings::tagsintimeline());
    m_buttonTimelineTags->setChecked(KdenliveSettings::tagsintimeline());
    // Reset view to update timeline colors
    getCurrentTimeline()->model()->_resetView();
}

void MainWindow::slotMouseZoomOnPlayhead()
{
    KdenliveSettings::setTimelinemousezoomonplayhead(!KdenliveSettings::timelinemousezoomonplayhead());
    m_buttonMouseZoomOnPlayhead->setChecked(KdenliveSettings::timelinemousezoomonplayhead());
}

QAction *MainWindow::focusedDeleteAction() const
{
    QWidget *focus = QApplication::focusWidget();
    if (focus != nullptr) {
        for (auto *bin : m_binWidgets) {
            if (bin->isAncestorOf(focus)) {
                auto *action = bin->findChild<QAction *>(QStringLiteral("delete_clip"));
                Q_ASSERT(action);
                return action;
            }
        }
        if (pCore->textEditWidget()->isAncestorOf(focus)) {
            auto *editor = pCore->textEditWidget()->findChild<VideoTextEdit *>();
            Q_ASSERT(editor);
            return editor->deleteAction;
        }
        for (QWidget *widget = focus; widget != nullptr && widget != this; widget = widget->parentWidget()) {
            if (widget == m_effectStackDock) {
                // This command has no shared action presentation.
                return nullptr;
            }
            if (widget == pCore->guidesList()) {
                return nullptr;
            }
        }
    }
    return actionCollection()->action(QStringLiteral("delete_timeline_selection"));
}

void MainWindow::updateDeleteAction()
{
    QObject::disconnect(m_deleteActionStateConnection);
    QAction *target = focusedDeleteAction();
    QAction *generic = actionCollection()->action(QStringLiteral("delete_selected_item"));
    // The effect stack handles availability in its direct deletion command.
    generic->setEnabled(!target || target->isEnabled());
    if (target) {
        m_deleteActionStateConnection = connect(target, &QAction::changed, this, [generic, target]() { generic->setEnabled(target->isEnabled()); });
    }
}

void MainWindow::slotDeleteItem()
{
    QWidget *focus = QApplication::focusWidget();
    if (focus && (focus == pCore->guidesList() || pCore->guidesList()->isAncestorOf(focus))) {
        pCore->guidesList()->removeGuide();
        return;
    }
    QAction *target = focusedDeleteAction();
    if (!target) {
        m_assetPanel->deleteCurrentEffect();
    } else if (target->isEnabled()) {
        target->trigger();
    }
}

void MainWindow::slotAddClipMarker()
{
    std::shared_ptr<ProjectClip> clip(nullptr);
    GenTime pos;
    if (m_projectMonitor->isActive()) {
        getCurrentTimeline()->controller()->addMarker();
        return;
    } else {
        clip = m_clipMonitor->currentController();
        pos = GenTime(m_clipMonitor->position(), pCore->getCurrentFps());
    }
    if (!clip) {
        m_messageLabel->setMessage(i18n("Cannot find clip to add marker"), ErrorMessage);
        return;
    }
    clip->getMarkerModel()->editMarkerGui(pos, this, true, clip.get());
}

void MainWindow::slotDeleteClipMarker(bool allowGuideDeletion)
{
    std::shared_ptr<ProjectClip> clip(nullptr);
    GenTime pos;
    if (m_projectMonitor->isActive()) {
        getCurrentTimeline()->controller()->deleteMarker();
        return;
    } else {
        clip = m_clipMonitor->currentController();
        pos = GenTime(m_clipMonitor->position(), pCore->getCurrentFps());
    }
    if (!clip) {
        m_messageLabel->setMessage(i18n("Cannot find clip to remove marker"), ErrorMessage);
        return;
    }

    bool markerFound = false;
    clip->getMarkerModel()->getMarker(pos, &markerFound);
    if (!markerFound) {
        if (allowGuideDeletion && m_projectMonitor->isActive()) {
            actionCollection()->action(QStringLiteral("delete_sequence_marker"))->trigger();
        } else {
            m_messageLabel->setMessage(i18n("No marker found at cursor time"), ErrorMessage);
        }
        return;
    }
    clip->getMarkerModel()->removeMarker(pos);
}

void MainWindow::slotDeleteAllClipMarkers()
{
    std::shared_ptr<ProjectClip> clip(nullptr);
    if (m_projectMonitor->isActive()) {
        getCurrentTimeline()->controller()->deleteAllMarkers();
        return;
    } else {
        clip = m_clipMonitor->currentController();
    }
    if (!clip) {
        m_messageLabel->setMessage(i18n("Cannot find clip to remove marker"), ErrorMessage);
        return;
    }
    bool ok = clip->getMarkerModel()->removeAllMarkers();
    if (!ok) {
        m_messageLabel->setMessage(i18n("An error occurred while deleting markers"), ErrorMessage);
        return;
    }
}

void MainWindow::slotDeleteAllSequenceMarkers()
{
    auto model = pCore->currentDoc()->getGuideModel(pCore->currentTimelineId());
    bool ok = model->removeAllMarkers();
    if (!ok) {
        m_messageLabel->setMessage(i18n("An error occurred while deleting markers"), ErrorMessage);
        return;
    }
}

void MainWindow::slotEditClipMarker()
{
    std::shared_ptr<ProjectClip> clip(nullptr);
    GenTime pos;
    if (m_projectMonitor->isActive()) {
        getCurrentTimeline()->controller()->editMarker();
        return;
    } else {
        clip = m_clipMonitor->currentController();
        pos = GenTime(m_clipMonitor->position(), pCore->getCurrentFps());
    }
    if (!clip) {
        m_messageLabel->setMessage(i18n("Cannot find clip to edit marker"), ErrorMessage);
        return;
    }

    bool markerFound = false;
    clip->getMarkerModel()->getMarker(pos, &markerFound);
    if (!markerFound) {
        m_messageLabel->setMessage(i18n("No marker found at cursor time"), ErrorMessage);
        return;
    }

    clip->getMarkerModel()->editMarkerGui(pos, this, false, clip.get());
    // Focus back clip monitor
    m_clipMonitor->setFocus();
}

void MainWindow::slotAddMarkerGuideQuickly()
{
    if (!getCurrentTimeline() || !pCore->currentDoc()) {
        return;
    }
    if (m_clipMonitor->isActive()) {
        QMap<int, QString> marker;
        marker.insert(m_clipMonitor->position(), QString());
        pCore->bin()->addClipMarker(m_clipMonitor->activeClipId(), marker);
    } else {
        int selectedClip = getCurrentTimeline()->controller()->getMainSelectedItem();
        if (selectedClip == -1) {
            // Add timeline guide
            getCurrentTimeline()->controller()->switchGuide();
        } else {
            // Add marker to main clip
            getCurrentTimeline()->controller()->addQuickMarker(selectedClip);
        }
    }
}

void MainWindow::slotAddMarkerWithCategory()
{
    if (!getCurrentTimeline() || !pCore->currentDoc()) {
        return;
    }

    auto *caller = qobject_cast<QAction *>(QObject::sender());
    if (!caller) {
        return;
    }
    // subtract 1 due to default category descriptions being 1-indexed
    int category = caller->data().toInt() - 1;

    // check if category exists
    if (!pCore->markerTypes.contains(category)) {
        pCore->displayMessage(i18n("Marker category does not exist"), ErrorMessage);
        return;
    }
    int currentCategory = KdenliveSettings::default_marker_type();

    KdenliveSettings::setDefault_marker_type(category);
    if (m_clipMonitor->isActive()) {
        QMap<int, QString> marker;
        marker.insert(m_clipMonitor->position(), QString());
        pCore->bin()->addClipMarker(m_clipMonitor->activeClipId(), marker);
    } else {
        int selectedClip = getCurrentTimeline()->controller()->getMainSelectedItem();
        if (selectedClip == -1) {
            // Add timeline guide
            getCurrentTimeline()->controller()->switchGuide();
        } else {
            // Add marker to main clip
            getCurrentTimeline()->controller()->addQuickMarker(selectedClip);
        }
    }
    // return to previously-chosen default category
    KdenliveSettings::setDefault_marker_type(currentCategory);
}

void MainWindow::slotSeparateAudioChannel()
{
    KdenliveSettings::setDisplayallchannels(!KdenliveSettings::displayallchannels());
}

void MainWindow::slotAutoTrackHeight(bool enable)
{
    KdenliveSettings::setAutotrackheight(enable);
    Q_EMIT pCore->autoTrackHeight(enable);
}

void MainWindow::slotNormalizeAudioChannel(bool normalize)
{
    KdenliveSettings::setNormalizechannels(normalize);
}

void MainWindow::slotInsertTrack()
{
    pCore->monitorManager()->activateMonitor(Kdenlive::ProjectMonitor);
    getCurrentTimeline()->controller()->beginAddTrack(-1);
}

void MainWindow::slotDeleteTrack()
{
    pCore->monitorManager()->activateMonitor(Kdenlive::ProjectMonitor);
    getCurrentTimeline()->controller()->deleteMultipleTracks(-1);
}

void MainWindow::slotMoveTrackUp()
{
    pCore->monitorManager()->activateMonitor(Kdenlive::ProjectMonitor);
    getCurrentTimeline()->controller()->moveTrackUp();
}

void MainWindow::slotMoveTrackDown()
{
    pCore->monitorManager()->activateMonitor(Kdenlive::ProjectMonitor);
    getCurrentTimeline()->controller()->moveTrackDown();
}

void MainWindow::slotShowTrackRec(bool checked)
{
    if (checked) {
        pCore->mixer()->monitorAudio(getCurrentTimeline()->controller()->activeTrack(), checked);
    } else {
        pCore->mixer()->monitorAudio(pCore->mixer()->recordTrack(), false);
    }
}

void MainWindow::slotSelectAllTracks()
{
    if (QApplication::focusWidget() != nullptr) {
        if (QApplication::focusWidget()->parentWidget() != nullptr) {
            for (auto &bin : m_binWidgets) {
                if (bin->isAncestorOf(QApplication::focusWidget())) {
                    bin->selectAll();
                    return;
                }
            }
        }
        if (QApplication::focusWidget()->objectName() == QLatin1String("guides_list")) {
            pCore->guidesList()->selectAll();
            return;
        }
    }
    getCurrentTimeline()->controller()->selectAll();
}

void MainWindow::slotSearchGuide()
{
    KDDockWidgets::QtWidgets::DockWidget *dock = qobject_cast<KDDockWidgets::QtWidgets::DockWidget *>(pCore->guidesList()->parentWidget());
    if (dock) {
        dock->open();
        dock->setAsCurrentTab();
    }
    pCore->guidesList()->filter_line->setFocus();
}

void MainWindow::slotSearchBin()
{
    pCore->activeBin()->searchLine()->setFocus();
}

void MainWindow::slotExportGuides()
{
    pCore->currentDoc()
        ->getGuideModel(getCurrentTimeline()->getUuid())
        ->exportGuidesGui(this, GenTime(getCurrentTimeline()->controller()->duration() - 1, pCore->getCurrentFps()));
}

void MainWindow::slotLockGuides(bool lock)
{
    KdenliveSettings::setLockedGuides(lock);
}

void MainWindow::slotDeleteAllGuides()
{
    pCore->currentDoc()->getGuideModel(getCurrentTimeline()->getUuid())->removeAllMarkers();
}

void MainWindow::slotInsertClipOverwrite()
{
    const QString &binId = m_clipMonitor->activeClipId();
    if (binId.isEmpty()) {
        // No clip in monitor
        return;
    }
    std::function<bool(void)> undo = []() { return true; };
    std::function<bool(void)> redo = []() { return true; };
    bool res = getCurrentTimeline()->controller()->insertZone(binId, m_clipMonitor->getZoneInfo(), true, undo, redo);
    if (res) {
        pCore->pushUndo(undo, redo, i18n("Overwrite zone"));
    } else {
        pCore->displayMessage(i18n("Could not insert zone"), ErrorMessage);
        undo();
    }
}

void MainWindow::slotInsertClipInsert()
{
    const QString &binId = m_clipMonitor->activeClipId();
    if (binId.isEmpty()) {
        // No clip in monitor
        pCore->displayMessage(i18n("No clip selected in project bin"), ErrorMessage);
        return;
    }
    std::function<bool(void)> undo = []() { return true; };
    std::function<bool(void)> redo = []() { return true; };
    bool res = getCurrentTimeline()->controller()->insertZone(binId, m_clipMonitor->getZoneInfo(), false, undo, redo);
    if (res) {
        pCore->pushUndo(undo, redo, i18n("Insert zone"));
    } else {
        pCore->displayMessage(i18n("Could not insert zone"), ErrorMessage);
        undo();
    }
}

void MainWindow::slotExtractZone()
{
    getCurrentTimeline()->controller()->extractZone(m_clipMonitor->getZoneInfo());
}

void MainWindow::slotLiftZone()
{
    getCurrentTimeline()->controller()->extractZone(m_clipMonitor->getZoneInfo(), true);
}

void MainWindow::slotAddProjectClip(const QUrl &url, const QString &folderInfo)
{
    pCore->activeBin()->droppedUrls(QList<QUrl>() << url, folderInfo);
}

void MainWindow::slotAddTextNote(const QString &text)
{
    pCore->projectManager()->slotAddTextNote(text);
}

void MainWindow::slotAddProjectClipList(const QList<QUrl> &urls)
{
    pCore->activeBin()->droppedUrls(urls);
}

void MainWindow::slotAddTransition(QAction *result)
{
    if (!result) {
        return;
    }
    // TODO refac
    /*
    QStringList info = result->data().toStringList();
    if (info.isEmpty() || info.count() < 2) {
        return;
    }
    QDomElement transition = transitions.getEffectByTag(info.at(0), info.at(1));
    if (pCore->projectManager()->currentTimeline() && !transition.isNull()) {
        pCore->projectManager()->currentTimeline()->projectView()->slotAddTransitionToSelectedClips(transition.cloneNode().toElement());
    }
    */
}

void MainWindow::slotAddEffect(QAction *result)
{
    if (!result) {
        return;
    }
    QString effectId = result->data().toString();
    addEffect(effectId);
}

void MainWindow::addEffect(const QString &effectId)
{
    if (m_assetPanel->effectStackOwner().type == KdenliveObjectType::BinClip || m_clipMonitor->isActive()) {
        // Pass the command to bin
        pCore->activeBin()->slotAddEffect({}, {effectId});
    } else if (m_assetPanel->effectStackOwner().type == KdenliveObjectType::TimelineTrack ||
               m_assetPanel->effectStackOwner().type == KdenliveObjectType::Master) {
        if (!m_assetPanel->addEffect(effectId)) {
            pCore->displayMessage(i18n("Cannot add effect to active item"), ErrorMessage);
        }
    } else {
        // Add effect to the current timeline selection
        QVariantMap effectData;
        effectData.insert(QStringLiteral("kdenlive/effect"), effectId);
        getCurrentTimeline()->controller()->addAsset(effectData);
    }
}

void MainWindow::slotZoomIn(bool zoomOnMouse)
{
    slotSetZoom(m_zoomSlider->value() - 1, KdenliveSettings::timelinemousezoomonplayhead() ? false : zoomOnMouse);
    slotShowZoomSliderToolTip();
}

void MainWindow::slotZoomOut(bool zoomOnMouse)
{
    slotSetZoom(m_zoomSlider->value() + 1, KdenliveSettings::timelinemousezoomonplayhead() ? false : zoomOnMouse);
    slotShowZoomSliderToolTip();
}

void MainWindow::slotFitZoom()
{
    Q_EMIT m_timelineTabs->fitZoom();
}

void MainWindow::slotSetZoom(int value, bool zoomOnMouse)
{
    value = qBound(m_zoomSlider->minimum(), value, m_zoomSlider->maximum());
    Q_EMIT m_timelineTabs->changeZoom(value, zoomOnMouse);
    updateZoomSlider(value);
}

void MainWindow::updateZoomSlider(int value)
{
    slotUpdateZoomSliderToolTip(value);
    KdenliveDoc *project = pCore->currentDoc();
    if (project) {
        project->setZoom(pCore->currentTimelineId(), value);
    }
    m_zoomOut->setEnabled(value < m_zoomSlider->maximum());
    m_zoomIn->setEnabled(value > m_zoomSlider->minimum());
    QSignalBlocker blocker(m_zoomSlider);
    m_zoomSlider->setValue(value);
}

void MainWindow::slotShowZoomSliderToolTip(int zoomlevel)
{
    if (zoomlevel != -1) {
        slotUpdateZoomSliderToolTip(zoomlevel);
    }

    QPoint global = m_zoomSlider->rect().topLeft();
    global.ry() += m_zoomSlider->height() / 2;
    QHelpEvent toolTipEvent(QEvent::ToolTip, QPoint(0, 0), m_zoomSlider->mapToGlobal(global));
    QApplication::sendEvent(m_zoomSlider, &toolTipEvent);
}

void MainWindow::slotUpdateZoomSliderToolTip(int zoomlevel)
{
    int max = m_zoomSlider->maximum() + 1;
    m_zoomSlider->setToolTip(i18n("Zoom Level: %1/%2", max - zoomlevel, max));
}

void MainWindow::customEvent(QEvent *e)
{
    if (e->type() == QEvent::User) {
        m_messageLabel->setMessage(static_cast<MltErrorEvent *>(e)->message(), MltError);
    }
}

void MainWindow::slotSnapRewind()
{
    if (m_projectMonitor->isActive()) {
        getCurrentTimeline()->controller()->gotoPreviousSnap();
    } else {
        m_clipMonitor->slotSeekToPreviousSnap();
    }
}

void MainWindow::slotSnapForward()
{
    if (m_projectMonitor->isActive()) {
        getCurrentTimeline()->controller()->gotoNextSnap();
    } else {
        m_clipMonitor->slotSeekToNextSnap();
    }
}

void MainWindow::slotGuideRewind()
{
    if (m_projectMonitor->isActive()) {
        getCurrentTimeline()->controller()->gotoPreviousGuide();
    } else {
        m_clipMonitor->slotSeekToPreviousSnap();
    }
}

void MainWindow::slotGuideForward()
{
    if (m_projectMonitor->isActive()) {
        getCurrentTimeline()->controller()->gotoNextGuide();
    } else {
        m_clipMonitor->slotSeekToNextSnap();
    }
}

void MainWindow::slotClipStart()
{
    if (m_projectMonitor->isActive()) {
        getCurrentTimeline()->controller()->seekCurrentClip(false);
    } else {
        m_clipMonitor->slotStart();
    }
}

void MainWindow::slotClipEnd()
{
    if (m_projectMonitor->isActive()) {
        getCurrentTimeline()->controller()->seekCurrentClip(true);
    } else {
        m_clipMonitor->slotEnd();
    }
}

void MainWindow::slotChangeTool(QAction *action)
{
    ToolType::ProjectTool activeTool = ToolType::SelectTool;

    // if(action == m_buttonSelectTool) covered by default value
    if (action == m_buttonRazorTool) {
        activeTool = ToolType::RazorTool;
    } else if (action == m_buttonSpacerTool) {
        activeTool = ToolType::SpacerTool;
    }
    if (action == m_buttonRippleTool) {
        activeTool = ToolType::RippleTool;
    }
    if (action == m_buttonRollTool) {
        activeTool = ToolType::RollTool;
    }
    if (action == m_buttonSlipTool) {
        activeTool = ToolType::SlipTool;
    }
    if (action == m_buttonSlideTool) {
        activeTool = ToolType::SlideTool;
    }
    if (action == m_buttonMulticamTool) {
        activeTool = ToolType::MulticamTool;
    };
    slotSetTool(activeTool);
}

void MainWindow::slotChangeEdit(QAction *action)
{
    TimelineMode::EditMode mode = TimelineMode::NormalEdit;
    if (action == m_overwriteEditTool) {
        mode = TimelineMode::OverwriteEdit;
    } else if (action == m_insertEditTool) {
        mode = TimelineMode::InsertEdit;
    }
    getCurrentTimeline()->model()->setEditMode(mode);
    showToolMessage();
    if (mode == TimelineMode::InsertEdit) {
        // Disable spacer tool in insert mode
        if (m_buttonSpacerTool->isChecked()) {
            m_buttonSelectTool->setChecked(true);
            slotSetTool(ToolType::SelectTool);
        }
        m_buttonSpacerTool->setEnabled(false);
    } else {
        m_buttonSpacerTool->setEnabled(true);
    }
}

void MainWindow::disableMulticam()
{
    if (m_activeTool == ToolType::MulticamTool) {
        m_buttonSelectTool->setChecked(true);
        slotSetTool(ToolType::SelectTool);
    }
}

ToolType::ProjectTool MainWindow::activeTool()
{
    return m_activeTool;
}

void MainWindow::slotSetTool(ToolType::ProjectTool tool)
{
    if (m_activeTool == ToolType::MulticamTool) {
        // End multicam operation
        pCore->monitorManager()->switchMultiTrackView(false);
        pCore->monitorManager()->slotStopMultiTrackMode();
    }
    m_activeTool = tool;
    Q_EMIT pCore->activeToolChanged();
    if (pCore->currentDoc()) {
        showToolMessage();
        getCurrentTimeline()->controller()->updateTrimmingMode();
    }
    if (m_activeTool == ToolType::MulticamTool) {
        // Start multicam operation
        pCore->monitorManager()->switchMultiTrackView(true);
        pCore->monitorManager()->slotStartMultiTrackMode();
    }
}

void MainWindow::showToolMessage()
{
    QString message;
    QString toolLabel;
    if (m_buttonSelectTool->isChecked()) {
#ifdef Q_OS_WIN
        message = xi18nc("@info:whatsthis",
                         "<shortcut>Shift drag</shortcut> for rubber-band selection, <shortcut>Shift click</shortcut> for multiple "
                         "selection, <shortcut>Meta drag</shortcut> to move a grouped clip to another track, <shortcut>Ctrl drag</shortcut> to pan");
#else
        message = xi18nc("@info:whatsthis",
                         "<shortcut>Shift drag</shortcut> for rubber-band selection, <shortcut>Shift click</shortcut> for multiple "
                         "selection, <shortcut>Meta + Alt drag</shortcut> to move a grouped clip to another track, <shortcut>Ctrl drag</shortcut> to pan");
#endif
        toolLabel = i18n("Select");
    } else if (m_buttonRazorTool->isChecked()) {
        message = xi18nc("@info:whatsthis", "<shortcut>Shift</shortcut> to preview cut frame");
        toolLabel = i18n("Razor");
    } else if (m_buttonSpacerTool->isChecked()) {
        message =
            xi18nc("@info:whatsthis",
                   "<shortcut>Ctrl</shortcut> to apply on current track only, <shortcut>Shift</shortcut> to also move guides. You can combine both modifiers.");
        toolLabel = i18n("Spacer");
    } else if (m_buttonSlipTool->isChecked()) {
        message = xi18nc("@info:whatsthis", "<shortcut>Click</shortcut> on an item to slip, <shortcut>Shift click</shortcut> for multiple selection");
        toolLabel = i18nc("Timeline Tool", "Slip");
    } /*else if (m_buttonSlideTool->isChecked()) { // TODO implement Slide
        toolLabel = i18nc("Timeline Tool", "Slide");
    }*/
    else if (m_buttonRippleTool->isChecked()) {
        message = xi18nc("@info:whatsthis", "<shortcut>Shift drag</shortcut> for rubber-band selection, <shortcut>Shift click</shortcut> for multiple "
                                            "selection, <shortcut>Alt click</shortcut> to select an item in a group, <shortcut>Ctrl drag</shortcut> to pan");
        toolLabel = i18nc("Timeline Tool", "Ripple");
    } /*else if (m_buttonRollTool->isChecked()) { // TODO implement Slide
        toolLabel = i18nc("Timeline Tool", "Roll");
    }*/
    else if (m_buttonMulticamTool->isChecked()) {
        message =
            xi18nc("@info:whatsthis", "<shortcut>Click</shortcut> on a track view in the project monitor to perform a lift of all tracks except active one");
        toolLabel = i18n("Multicam");
    }
    TimelineMode::EditMode mode = TimelineMode::NormalEdit;
    if (getCurrentTimeline() && getCurrentTimeline()->model()) {
        mode = getCurrentTimeline()->model()->editMode();
    }

    // Store the current edit mode for palette updates
    m_currentEditMode = mode;

    if (mode != TimelineMode::NormalEdit) {
        if (!toolLabel.isEmpty()) {
            toolLabel.append(QStringLiteral(" | "));
        }
        if (mode == TimelineMode::InsertEdit) {
            toolLabel.append(i18n("Insert"));
        } else if (mode == TimelineMode::OverwriteEdit) {
            toolLabel.append(i18n("Overwrite"));
        }
    }

    applyToolMessageStyling();

    m_trimLabel->setText(toolLabel);
    m_messageLabel->setKeyMap(message);
}

void MainWindow::setWidgetKeyBinding(const QString &mess)
{
    m_messageLabel->setKeyMap(mess);
}

void MainWindow::showKeyBinding(const QString &text)
{
    m_messageLabel->setTmpKeyMap(text);
}

void MainWindow::slotCopy()
{
    QWidget *widget = QApplication::focusWidget();
    while ((widget != nullptr) && widget != this) {
        if (widget == m_effectStackDock) {
            m_assetPanel->sendStandardCommand(KStandardAction::Copy);
            return;
        } else if (widget == m_effectList2 && m_effectList2->infoPanelIsFocused()) {
            m_effectList2->processCopy();
            return;
        } else if (widget == m_compositionList && m_compositionList->infoPanelIsFocused()) {
            m_compositionList->processCopy();
            return;
        }
        widget = widget->parentWidget();
    }
    getCurrentTimeline()->controller()->copyItem();
}

void MainWindow::slotCut()
{
    QWidget *widget = QApplication::focusWidget();
    while ((widget != nullptr) && widget != this) {
        if (widget == m_effectStackDock) {
            // Todo: cut effect?
            // m_assetPanel->sendStandardCommand(KStandardAction::Copy);
            return;
        }
        widget = widget->parentWidget();
    }
    getCurrentTimeline()->controller()->cutItem();
}

void MainWindow::slotPaste()
{
    QWidget *widget = QApplication::focusWidget();
    while ((widget != nullptr) && widget != this) {
        if (widget == m_effectStackDock) {
            m_assetPanel->sendStandardCommand(KStandardAction::Paste);
            return;
        }
        widget = widget->parentWidget();
    }
    getCurrentTimeline()->controller()->pasteItem();
}

void MainWindow::slotClipInTimeline(const QString &clipId, const QList<int> &ids)
{
    Q_UNUSED(clipId)
    QMenu *inTimelineMenu = static_cast<QMenu *>(factory()->container(QStringLiteral("clip_in_timeline"), this));
    QList<QAction *> actionList;
    for (int i = 0; i < ids.count(); ++i) {
        ObjectId oid(KdenliveObjectType::TimelineClip, ids.at(i), pCore->currentTimelineId());
        QString track = getCurrentTimeline()->controller()->getTrackNameFromIndex(pCore->getItemTrack(oid));
        QString start = pCore->currentDoc()->timecode().getTimecodeFromFrames(pCore->getItemPosition(oid));
        int j = 0;
        QAction *a = new QAction(track + QStringLiteral(": ") + start, inTimelineMenu);
        a->setData(ids.at(i));
        connect(a, &QAction::triggered, this, &MainWindow::slotSelectClipInTimeline);
        while (j < actionList.count()) {
            if (actionList.at(j)->text() > a->text()) {
                break;
            }
            j++;
        }
        actionList.insert(j, a);
    }
    QList<QAction *> list = inTimelineMenu->actions();
    unplugActionList(QStringLiteral("timeline_occurences"));
    qDeleteAll(list);
    plugActionList(QStringLiteral("timeline_occurences"), actionList);

    if (actionList.isEmpty()) {
        inTimelineMenu->setEnabled(false);
    } else {
        inTimelineMenu->setEnabled(true);
    }
}

void MainWindow::raiseBin(bool unconditionally)
{
    Bin *bin = activeBin();
    if (bin) {
        KDDockWidgets::QtWidgets::DockWidget *dock = qobject_cast<KDDockWidgets::QtWidgets::DockWidget *>(bin->parentWidget());
        if (!unconditionally) {
            if (dock && dock->asDockWidgetController()->isTabbed()) {
                if (dock->asGroupController()->containsDockWidget(m_clipMonitorDock->asDockWidgetController())) {
                    return;
                }
            }
        }
        bin->focusBinView();
        dock->open();
        dock->setAsCurrentTab();
    }
}

void MainWindow::focusTimeline()
{
    auto tl = getCurrentTimeline();
    if (tl) {
        tl->focusTimeline();
        Q_EMIT tl->controller()->selectionChanged();
    }
}

void MainWindow::slotClipInProjectTree(ObjectId ownerId, bool seekToStart)
{
    QString binId;
    if (ownerId.type != KdenliveObjectType::TimelineClip) {
        int cid = getCurrentTimeline()->controller()->getMainSelectedClip();
        if (cid == -1) {
            return;
        }
        ownerId = ObjectId(KdenliveObjectType::TimelineClip, cid, pCore->currentTimelineId());
        binId = getCurrentTimeline()->controller()->getClipBinId(cid);
    } else {
        binId = pCore->currentDoc()->getTimeline(ownerId.uuid)->getClipBinId(ownerId.itemId);
    }
    // If we have multiple bins, check first if a visible bin contains it
    raiseBin();
    int start = pCore->getItemIn(ownerId);
    int duration = pCore->getItemDuration(ownerId);
    int pos = m_projectMonitor->position();
    int itemPos = pCore->getItemPosition(ownerId);
    bool containsPos = (pos >= itemPos && pos < itemPos + duration);
    double speed = pCore->getClipSpeed(ownerId);
    if (containsPos) {
        pos -= itemPos - start;
    }
    if (!qFuzzyCompare(speed, 1.)) {
        if (speed > 0.) {
            // clip has a speed effect, adjust zone
            start = qRound(start * speed);
            duration = qRound(duration * speed);
            if (containsPos) {
                pos = qRound(pos * speed);
            }
        } else if (speed < 0.) {
            int max = getCurrentTimeline()->controller()->clipMaxDuration(ownerId.itemId);
            if (max > 0) {
                int invertedPos = itemPos + duration - m_projectMonitor->position();
                start = qRound((max - (start + duration)) * -speed);
                duration = qRound(duration * -speed);
                if (containsPos) {
                    pos = start + qRound(invertedPos * -speed);
                }
            }
        }
    }
    QPoint zone(start, start + duration - 1);
    if (!containsPos || seekToStart) {
        pos = start;
    }
    activeBin()->selectClipById(binId, pos, zone, true);
}

void MainWindow::slotSelectClipInTimeline()
{
    pCore->monitorManager()->activateMonitor(Kdenlive::ProjectMonitor);
    auto *action = qobject_cast<QAction *>(sender());
    int clipId = action->data().toInt();
    getCurrentTimeline()->controller()->focusItem(clipId);
}

/** Gets called when the window gets hidden */
void MainWindow::hideEvent(QHideEvent * /*event*/)
{
    if (isMinimized() && pCore->monitorManager()) {
        pCore->monitorManager()->pauseActiveMonitor();
    }
}

void MainWindow::slotUpdateTimelineView(QAction *action)
{
    int viewMode = action->data().toInt();
    KdenliveSettings::setAudiotracksbelow(viewMode);
    getCurrentTimeline()->model()->_resetView();
}

void MainWindow::loadClipActions()
{
    unplugActionList(QStringLiteral("add_effect"));
    plugActionList(QStringLiteral("add_effect"), m_effectsMenu->actions());

    QList<QAction *> clipJobActions = getExtraActions(QStringLiteral("clipjobs"));
    unplugActionList(QStringLiteral("clip_jobs"));
    plugActionList(QStringLiteral("clip_jobs"), clipJobActions);

    QList<QAction *> atcActions = getExtraActions(QStringLiteral("audiotranscoderslist"));
    unplugActionList(QStringLiteral("audio_transcoders_list"));
    plugActionList(QStringLiteral("audio_transcoders_list"), atcActions);

    QList<QAction *> tcActions = getExtraActions(QStringLiteral("transcoderslist"));
    unplugActionList(QStringLiteral("transcoders_list"));
    plugActionList(QStringLiteral("transcoders_list"), tcActions);
}

void MainWindow::loadDockActions()
{
    QList<QAction *> list = kdenliveCategoryMap.value(QStringLiteral("interface"))->actions();
    // Sort actions
    QMap<QString, QAction *> sorted;
    QMap<QString, QAction *> bins;
    QMap<QString, QAction *> scopes;
    QStringList sortedList;
    QStringList binsList;
    QStringList scopesList;
    delete m_binsListMenu;
    delete m_scopesListMenu;
    m_scopesListMenu = new QMenu(i18n("Scopes"));
    QAction *a = m_scopesListMenu->menuAction();
    a->setData(i18n("Scopes"));
    list << a;

    // Group bins
    if (m_binWidgets.size() > 1) {
        m_binsListMenu = new QMenu(i18n("Project Bins"));
        a = m_binsListMenu->menuAction();
        a->setData(m_binsListMenu->title());
        list << a;
    } else {
        m_binsListMenu = nullptr;
    }

    // Group scopes
    QStringList scopesNames = m_scopesManager->getScopesNames();
    // TODO: move audiospectrum's creation to ScopeManager
    scopesNames << QStringLiteral("audiospectrum");

    for (QAction *a : std::as_const(list)) {
        if (a->objectName().startsWith(QStringLiteral("raise_"))) {
            continue;
        }
        const QString actionName = a->data().toString();
        if (m_binsListMenu && actionName.contains(QLatin1String("project_bin"))) {
            bins.insert(actionName, a);
            binsList << actionName;
            continue;
        }
        if (scopesNames.contains(actionName.section(QLatin1Char('#'), 1))) {
            scopes.insert(actionName, a);
            scopesList << actionName;
            continue;
        }
        sorted.insert(actionName, a);
        sortedList << actionName;
    }
    QList<QAction *> orderedList;
    QCollator order;
    std::sort(sortedList.begin(), sortedList.end(), order);
    for (const QString &text : std::as_const(sortedList)) {
        orderedList << sorted.value(text);
    }
    QList<QAction *> orderedBinList;
    binsList.sort(Qt::CaseInsensitive);
    for (const QString &text : std::as_const(binsList)) {
        orderedBinList << bins.value(text);
    }
    if (m_binsListMenu) {
        m_binsListMenu->addActions(orderedBinList);
    }
    QList<QAction *> orderedScopesList;
    scopesList.sort(Qt::CaseInsensitive);
    for (const QString &text : std::as_const(scopesList)) {
        orderedScopesList << scopes.value(text);
    }
    m_scopesListMenu->addActions(orderedScopesList);

    unplugActionList(QStringLiteral("dock_actions"));
    plugActionList(QStringLiteral("dock_actions"), orderedList);
}

void MainWindow::buildDynamicActions()
{
    if (kdenliveCategoryMap.contains(QStringLiteral("clipjobs"))) {
        auto ts = kdenliveCategoryMap.take(QStringLiteral("clipjobs"));
        QList<QAction *> acs = ts->actions();
        qDeleteAll(acs);
        delete ts;
    }
    if (kdenliveCategoryMap.contains(QStringLiteral("transcoderslist"))) {
        auto ts = kdenliveCategoryMap.take(QStringLiteral("transcoderslist"));
        QList<QAction *> acs = ts->actions();
        qDeleteAll(acs);
        delete ts;
    }
    if (kdenliveCategoryMap.contains(QStringLiteral("audiotranscoderslist"))) {
        auto ts = kdenliveCategoryMap.take(QStringLiteral("audiotranscoderslist"));
        QList<QAction *> acs = ts->actions();
        qDeleteAll(acs);
        delete ts;
    }

    auto cjobs = new KActionCategory(i18n("Clip Jobs"), m_extraFactory->actionCollection());
    QAction *action;
    QMap<QString, QString> jobValues = ClipJobManager::getClipJobNames();
    QMapIterator<QString, QString> k(jobValues);
    while (k.hasNext()) {
        k.next();
        action = new QAction(k.value(), m_extraFactory->actionCollection());
        action->setData(k.key());
        if (k.key() == QLatin1String("stabilize;v")) {
            connect(action, &QAction::triggered, this, [this]() { StabilizeTask::start(this); });
        } else if (k.key() == QLatin1String("scenesplit;v")) {
            connect(action, &QAction::triggered, this, [&]() { SceneSplitTask::start(this); });
        } else if (k.key() == QLatin1String("timewarp;av")) {
            connect(action, &QAction::triggered, this, [&]() { SpeedTask::start(this); });
        } else {
            connect(action, &QAction::triggered, this, [&, jobId = k.key().section(QLatin1Char(';'), 0, 0)]() { CustomJobTask::start(this, jobId); });
        }
        cjobs->addAction(action->text(), action);
    }
    action = new QAction(QIcon::fromTheme(QStringLiteral("configure")), i18n("Configure Clip Jobs…"), m_extraFactory->actionCollection());
    cjobs->addAction(action->text(), action);
    connect(action, &QAction::triggered, this, [this]() { manageClipJobs(); });
    kdenliveCategoryMap.insert(QStringLiteral("clipjobs"), cjobs);

    // transcoders
    auto ts = new KActionCategory(i18n("Transcoders"), m_extraFactory->actionCollection());
    auto ats = new KActionCategory(i18n("Extract Audio"), m_extraFactory->actionCollection());
    KSharedConfigPtr config = KSharedConfig::openConfig(QStringLiteral("kdenlivetranscodingrc"), KConfig::CascadeConfig, QStandardPaths::AppDataLocation);
    KConfigGroup transConfig(config, "Transcoding");
    // read the entries
    QMap<QString, QString> profiles = transConfig.entryMap();
    QMapIterator<QString, QString> i(profiles);
    while (i.hasNext()) {
        i.next();
        QStringList transList;
        transList << i.value().split(QLatin1Char(';'));
        auto *a = new QAction(i.key(), m_extraFactory->actionCollection());
        a->setData(transList);
        if (transList.count() > 1) {
            a->setToolTip(transList.at(1));
        }
        connect(a, &QAction::triggered, [&, a]() {
            QStringList transcodeData = a->data().toStringList();
            std::vector<QString> ids = pCore->activeBin()->selectedClipsIds(true);
            QMap<QString, QVector<int>> clipStreamSelection;
            QString clipId;
            if (transcodeData.count() > 2 && transcodeData.at(2) == QLatin1String("audio")) {
                // Audio extract, check if we have multi stream clips
                QMap<QString, int> clipStreamCount;
                for (const QString &id : ids) {
                    if (id.contains(QLatin1Char('/'))) {
                        clipId = id.section(QLatin1Char('/'), 0, 0);
                    } else {
                        clipId = id;
                    }
                    std::shared_ptr<ProjectClip> clip = pCore->projectItemModel()->getClipByBinID(clipId);
                    if (clip->audioStreamsCount() > 1) {
                        clipStreamSelection.insert(clipId, clip->activeFfmpegStreams());
                    }
                }
            }
            const QString tData = transcodeData.first();
            int clipIn = -1;
            int clipOut = -1;
            for (const QString &id : ids) {
                if (id.contains(QLatin1Char('/'))) {
                    clipId = id.section(QLatin1Char('/'), 0, 0);
                    clipIn = id.section(QLatin1Char('/'), 1, 1).toInt();
                    clipOut = id.section(QLatin1Char('/'), 2, 2).toInt();
                } else {
                    clipId = id;
                    clipIn = -1;
                    clipOut = -1;
                }
                std::shared_ptr<ProjectClip> clip = pCore->projectItemModel()->getClipByBinID(clipId);
                TranscodeSeek::TranscodeInfo info;
                info.url = clip->clipUrl();
                info.type = clip->clipType();
                if (clip->statusReady()) {
                    info.vCodec = clip->videoCodecProperty(QStringLiteral("pix_fmt"));
                }
                info.fps_info = clip->fpsInfo();
                if (clipStreamSelection.contains(clipId)) {
                    // Extract selected audio streams only, create one task per stream
                    QVector<int> selectedStreams = clipStreamSelection.value(clipId);
                    for (auto &ix : selectedStreams) {
                        QString args;
                        QString suffix;
                        if (ix == -1) {
                            // Merge all audio streams
                            args = QStringLiteral("-filter_complex amerge=inputs=%1 ").arg(clip->audioStreamsCount());
                            args.append(QStringLiteral("-ac %1 ").arg(clip->audioChannels()));
                            suffix = i18n("-merged");
                        } else {
                            args = QStringLiteral("-map 0:a:%1 ").arg(ix);
                            suffix = i18n("-stream-%1", ix);
                        }
                        args.append(tData);
                        TranscodeTask::start(ObjectId(KdenliveObjectType::BinClip, clipId.toInt(), QUuid()), suffix, QString(), args, info, clipIn, clipOut,
                                             false, clip.get());
                    }
                } else {
                    TranscodeTask::start(ObjectId(KdenliveObjectType::BinClip, clipId.toInt(), QUuid()), QString(), QString(), tData, info, clipIn, clipOut,
                                         false, clip.get());
                }
            }
        });
        if (transList.count() > 2 && transList.at(2) == QLatin1String("audio")) {
            // This is an audio transcoding action
            ats->addAction(i.key(), a);
        } else {
            ts->addAction(i.key(), a);
        }
    }
    kdenliveCategoryMap.insert(QStringLiteral("transcoderslist"), ts);
    kdenliveCategoryMap.insert(QStringLiteral("audiotranscoderslist"), ats);
    updateDockMenu();
}

void MainWindow::updateDockMenu()
{
    // Populate View menu with show / hide actions for dock widgets
    KActionCategory *guiActions = nullptr;
    QList<QAction *> existing;
    if (kdenliveCategoryMap.contains(QStringLiteral("interface"))) {
        guiActions = kdenliveCategoryMap.value(QStringLiteral("interface"));
        existing = guiActions->actions();
    } else {
        guiActions = new KActionCategory(i18n("Interface"), actionCollection());
    }

    QList<KDDockWidgets::QtWidgets::DockWidget *> docks = findChildren<KDDockWidgets::QtWidgets::DockWidget *>();
    for (auto dock : std::as_const(docks)) {
        QAction *dockInfo = dock->toggleAction();
        if (!dockInfo || existing.contains(dockInfo)) {
            continue;
        }
        guiActions->addAction(dock->objectName(), dockInfo);
    }
    kdenliveCategoryMap.insert(QStringLiteral("interface"), guiActions);
    loadDockActions();
}

QList<QAction *> MainWindow::getExtraActions(const QString &name)
{
    if (!kdenliveCategoryMap.contains(name)) {
        return QList<QAction *>();
    }
    return kdenliveCategoryMap.value(name)->actions();
}

void MainWindow::slotTranscode(const QStringList &urls)
{
    Q_ASSERT(!urls.isEmpty());
    QString params;
    QString desc;
    ClipTranscode *d = new ClipTranscode(urls, params, QStringList(), desc, pCore->activeBin()->getCurrentFolder());
    connect(d, &ClipTranscode::addClip, this, &MainWindow::slotAddProjectClip);
    d->show();
}

void MainWindow::slotFriendlyTranscode(const QString &binId, bool checkProfile)
{
    QString params;
    QString desc;
    std::shared_ptr<ProjectClip> clip = pCore->projectItemModel()->getClipByBinID(binId);
    if (clip == nullptr) {
        qDebug() << "// NO CLIP FOUND FOR BIN ID: " << binId;
        return;
    }
    QStringList urls = {clip->url()};
    // Prepare clip properties
    QMap<QString, QString> sourceProps;
    sourceProps.insert(QStringLiteral("resource"), clip->url());
    sourceProps.insert(QStringLiteral("kdenlive:originalurl"), clip->url());
    sourceProps.insert(QStringLiteral("kdenlive:clipname"), clip->clipName());
    sourceProps.insert(QStringLiteral("kdenlive:proxy"), clip->getProducerProperty(QStringLiteral("kdenlive:proxy")));
    sourceProps.insert(QStringLiteral("_fullreload"), QStringLiteral("1"));
    ClipTranscode *d = new ClipTranscode(urls, params, QStringList(), desc, pCore->activeBin()->getCurrentFolder());
    connect(d, &ClipTranscode::addClip, [&, binId, sourceProps](const QUrl &url, const QString & /*folderInfo*/) {
        QMap<QString, QString> newProps;
        newProps.insert(QStringLiteral("resource"), QDir::cleanPath(url.toLocalFile()));
        newProps.insert(QStringLiteral("kdenlive:originalurl"), url.toLocalFile());
        newProps.insert(QStringLiteral("kdenlive:clipname"), url.fileName());
        newProps.insert(QStringLiteral("kdenlive:proxy"), QStringLiteral("-"));
        newProps.insert(QStringLiteral("_fullreload"), QStringLiteral("1"));
        QMetaObject::invokeMethod(pCore->activeBin(), "slotEditClipCommand", Qt::QueuedConnection, Q_ARG(QString, binId), Q_ARG(stringMap, sourceProps),
                                  Q_ARG(stringMap, newProps));
    });
    d->exec();
    if (checkProfile) {
        pCore->bin()->slotCheckProfile(binId);
    }
}

void MainWindow::slotTranscodeClip()
{
    const QString dialogFilter = FileFilter::Builder().defaultCategories().toQFilter();
    QString clipFolder = KRecentDirs::dir(QStringLiteral(":KdenliveClipFolder"));
    QStringList urls = QFileDialog::getOpenFileNames(this, i18nc("@title:window", "Files to Transcode"), clipFolder, dialogFilter);
    if (urls.isEmpty()) {
        return;
    }
    slotTranscode(urls);
}

void MainWindow::slotSetDocumentRenderProfile(const QMap<QString, QString> &props)
{
    KdenliveDoc *project = pCore->currentDoc();
    bool modified = false;
    QMapIterator<QString, QString> i(props);
    while (i.hasNext()) {
        i.next();
        if (project->getDocumentProperty(i.key()) == i.value()) {
            continue;
        }
        project->setDocumentProperty(i.key(), i.value());
        modified = true;
    }
    if (modified) {
        project->setModified();
    }
}

void MainWindow::slotUpdateTimecodeFormat(int ix)
{
    KdenliveSettings::setFrametimecode(ix == 1);
    Q_EMIT pCore->updateProjectTimecode();
    m_clipMonitor->updateTimecodeFormat();
    m_projectMonitor->updateTimecodeFormat();
    Q_EMIT getCurrentTimeline()->controller()->frameFormatChanged();
    m_timeFormatButton->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
}

void MainWindow::applyToolMessageStyling()
{
    if (!m_trimLabel) {
        return;
    }

    KColorScheme scheme(QApplication::palette().currentColorGroup());

    switch (m_currentEditMode) {
    case TimelineMode::InsertEdit:
        // Use a red color from the palette for insert mode
        m_trimLabel->setStyleSheet(QStringLiteral("QLabel { padding-left: 2; padding-right: 2; background-color :%1; }")
                                       .arg(scheme.foreground(KColorScheme::NegativeText).color().name()));
        break;
    case TimelineMode::OverwriteEdit:
        // Use a green color from the palette for overwrite mode
        m_trimLabel->setStyleSheet(QStringLiteral("QLabel { padding-left: 2; padding-right: 2; background-color :%1; }")
                                       .arg(scheme.foreground(KColorScheme::PositiveText).color().name()));
        break;
    default:
        // Use normal window background color for normal edit mode
        m_trimLabel->setStyleSheet(
            QStringLiteral("QLabel { padding-left: 2; padding-right: 2; background-color :%1; }").arg(QApplication::palette().window().color().name()));
        break;
    }
}

void MainWindow::applyZoomLevelButtonStyling()
{
    if (!m_statusZoomLevelButton) {
        return;
    }

    m_statusZoomLevelButton->setStyleSheet(
        QStringLiteral("QPushButton { padding: 2px; background-color: rgba(255, 0, 0, 0.25); border: none; border-radius: 4px; } "
                       "QPushButton:hover { border: 1px solid palette(highlight); } "
                       "QPushButton:disabled { color: palette(text); background-color: transparent; }"));
}

void MainWindow::slotRemoveFocus()
{
    getCurrentTimeline()->setFocus();
}

void MainWindow::slotShutdown()
{
    pCore->currentDoc()->setModified(false);
    // Call shutdown
#ifndef NODBUS
    QDBusConnectionInterface *interface = QDBusConnection::sessionBus().interface();
    // org.kde.Shutdown is DBus activatable, so we can't query for it running
    if (qgetenv("XDG_CURRENT_DESKTOP") == QLatin1String("KDE")) {
        QDBusInterface kdeShutdown(QStringLiteral("org.kde.LogoutPrompt"), QStringLiteral("/LogoutPrompt"), QStringLiteral("org.kde.LogoutPrompt"));
        kdeShutdown.call(QStringLiteral("promptShutDown"));
    } else if ((interface != nullptr) && interface->isServiceRegistered(QStringLiteral("org.gnome.SessionManager"))) {
        QDBusInterface smserver(QStringLiteral("org.gnome.SessionManager"), QStringLiteral("/org/gnome/SessionManager"),
                                QStringLiteral("org.gnome.SessionManager"));
        smserver.call(QStringLiteral("Shutdown"));
    }
#endif
}

void MainWindow::slotSwitchMonitors()
{
    pCore->monitorManager()->slotSwitchMonitors(!m_clipMonitor->isActive());
    if (m_projectMonitor->isActive()) {
        focusTimeline();
    } else {
        Bin *bin = activeBin();
        if (bin) {
            bin->focusBinView();
        }
    }
}

void MainWindow::slotFocusTimecode()
{
    if (m_clipMonitor->isActive()) {
        m_clipMonitor->focusTimecode();
    } else if (m_projectMonitor) {
        m_projectMonitor->focusTimecode();
    }
}

void MainWindow::slotSwitchMonitorOverlay(QAction *action)
{
    if (pCore->monitorManager()->isActive(Kdenlive::ClipMonitor)) {
        m_clipMonitor->switchMonitorInfo(action->data().toInt());
    } else {
        m_projectMonitor->switchMonitorInfo(action->data().toInt());
    }
}

void MainWindow::slotSwitchDropFrames(bool drop)
{
    KdenliveSettings::setMonitor_dropframes(drop);
    m_clipMonitor->restart();
    m_projectMonitor->restart();
}

void MainWindow::slotInsertZoneToTree()
{
    if (!m_clipMonitor->isActive() || m_clipMonitor->currentController() == nullptr) {
        return;
    }
    QPoint info = m_clipMonitor->getZoneInfo();
    QString id;
    // clip monitor counts the frame after the out point as the zone out, so we
    // need to subtract 1 to get the actual last frame
    pCore->projectItemModel()->requestAddBinSubClip(id, info.x(), info.y() - 1, {}, m_clipMonitor->activeClipId());
}

void MainWindow::slotUpdateProxySettings()
{
    KdenliveDoc *project = pCore->currentDoc();
    if (m_renderWidget) {
        m_renderWidget->updateProxyConfig(project->useProxy());
    }
    for (auto &b : m_binWidgets) {
        b->refreshProxySettings();
    }
}

void MainWindow::slotArchiveProject()
{
    KdenliveDoc *doc = pCore->currentDoc();
    pCore->projectManager()->prepareSave();
    QString sceneData = pCore->projectManager()->projectSceneList(doc->url().adjusted(QUrl::RemoveFilename | QUrl::StripTrailingSlash).toLocalFile()).first;
    if (sceneData.isEmpty()) {
        KMessageBox::error(this, i18n("Project file could not be saved for archiving."));
        return;
    }
    QStringList compositionLumas = doc->extractCompositionLumas();
    QStringList externalEffectFiles = doc->extractExternalEffectFiles();

    QPointer<ArchiveWidget> d(new ArchiveWidget(doc->url().fileName(), sceneData, compositionLumas, externalEffectFiles, this));
    if (d->exec() != 0) {
        m_messageLabel->setMessage(i18n("Archiving project"), OperationCompletedMessage);
    }
}

void MainWindow::slotDownloadResources()
{
    m_onlineResourcesDock->open();
    m_onlineResourcesDock->setAsCurrentTab();
}

void MainWindow::slotProcessImportKeyframes(GraphicsRectItem type, const QString &tag, const QString &keyframes)
{
    Q_UNUSED(keyframes)
    Q_UNUSED(tag)
    if (type == AVWidget) {
        // This data should be sent to the effect stack
        // TODO REFAC reimplement
        // m_effectStack->setKeyframes(tag, data);
    } else if (type == TransitionWidget) {
        // This data should be sent to the transition stack
        // TODO REFAC reimplement
        // m_effectStack->transitionConfig()->setKeyframes(tag, data);
    } else {
        // Error
    }
}

void MainWindow::slotAlignPlayheadToMousePos()
{
    pCore->monitorManager()->activateMonitor(Kdenlive::ProjectMonitor);
    getCurrentTimeline()->controller()->seekToMouse();
}

void MainWindow::triggerKey(QKeyEvent *ev)
{
    // Hack: The QQuickWindow that displays fullscreen monitor does not integrate with QActions.
    // So on keypress events we parse keys and check for shortcuts in all existing actions
    QKeySequence seq;
    // Remove the Num modifier or some shortcuts like "*" will not work
    auto mods = ev->modifiers() & ~Qt::KeypadModifier;
    // Some shortcuts are translated, for example Shift+5 is equal to '%' in a swiss keyboard
    // So we need to remove the Shift Modifier to match. Logic copied from KKeySequenceRecorder
    if (KdenliveKeySequence::isShiftAsModifierAllowed(ev->key())) {
        seq = QKeySequence(static_cast<int>(mods) + ev->key());
    } else {
        seq = QKeySequence(static_cast<int>(mods & ~Qt::ShiftModifier) + ev->key());
    }
    QList<KActionCollection *> collections = KActionCollection::allCollections();
    for (int i = 0; i < collections.count(); ++i) {
        KActionCollection *coll = collections.at(i);
        for (QAction *tempAction : coll->actions()) {
            if (tempAction->shortcuts().contains(seq)) {
                // Trigger action
                tempAction->trigger();
                ev->accept();
                return;
            }
        }
    }
    QWidget::keyPressEvent(ev);
}

KDDockWidgets::QtWidgets::DockWidget *MainWindow::addDock(const QString &title, const QString &objectName, QWidget *widget, KDDockWidgets::Location area,
                                                          KDDockWidgets::QtWidgets::DockWidget *otherDockWidget, const QSize preferredSize)
{
    auto dock = new KDDockWidgets::QtWidgets::DockWidget(objectName);
    qDebug() << "Building dock: " << objectName;
    Q_ASSERT(widget != nullptr);
    widget->setProperty("_breeze_force_frame", false);
    dock->setTitle(title);
    dock->setWidget(widget);
    dock->setObjectName(objectName);
    dock->toggleAction()->setData(QStringLiteral("%1#%2").arg(title, objectName));
    if (area == KDDockWidgets::Location_None) {
        // Add widget as tab
        qDebug() << "Creating dock: " << objectName;
        Q_ASSERT(otherDockWidget != nullptr);
        otherDockWidget->addDockWidgetAsTab(dock, preferredSize);
    } else {
        mainDockWindow->addDockWidget(dock, area, otherDockWidget, preferredSize);
    }
    KActionCategory *guiActions = nullptr;
    if (kdenliveCategoryMap.contains(QStringLiteral("interface"))) {
        guiActions = kdenliveCategoryMap.take(QStringLiteral("interface"));
    } else {
        guiActions = new KActionCategory(i18n("Interface"), actionCollection());
    }
    auto dockAction = dock->toggleAction();
    connect(dockAction, &QAction::triggered, this, [dock](bool dockVisible) {
        if (!dockVisible && !KdenliveSettings::showtitlebars()) {
            // Hack: when titlebar is hidden and a standalone widget is hidden through its
            // menu action, empty space is not automatically reused. So we hack around by
            // showing widget again, then hiding
            dock->open();
            dock->close();
        }
    });
    guiActions->addAction(objectName, dockAction);
    const QString actionText = KLocalizedString::removeAcceleratorMarker(dockAction->text());
    QAction *action = new QAction(i18n("Raise %1", actionText), this);
    action->setData(QStringLiteral("_raise"));
    connect(action, &QAction::triggered, this, [dock]() {
        dock->open();
        dock->setAsCurrentTab();
        dock->setFocus(Qt::OtherFocusReason);
    });
    guiActions->addAction("raise_" + dock->objectName(), action);
    kdenliveCategoryMap.insert(QStringLiteral("interface"), guiActions);
    return dock;
}

bool MainWindow::isMixedTabbed() const
{
    return m_mixerDock->asDockWidgetController()->isTabbed();
    return false;
}

void MainWindow::slotUpdateMonitorOverlays(int id, int code)
{
    QMenu *monitorOverlay = static_cast<QMenu *>(factory()->container(QStringLiteral("monitor_config_overlay"), this));
    if (!monitorOverlay) {
        return;
    }
    QList<QAction *> actions = monitorOverlay->actions();
    for (QAction *ac : std::as_const(actions)) {
        int mid = ac->data().toInt();
        if (mid == Monitor::InfoOverlay || mid == Monitor::ClipJobsOverlay) {
            ac->setVisible(id == Kdenlive::ClipMonitor);
        }
        ac->setChecked(code & mid);
    }
}

void MainWindow::raiseMonitor(bool clipMonitor, bool raise)
{
    if (clipMonitor) {
        if (raise) {
            m_clipMonitorDock->open();
        }
        m_clipMonitorDock->setAsCurrentTab();
    } else {
        if (raise) {
            m_projectMonitorDock->open();
        }
        m_projectMonitorDock->setAsCurrentTab();
    }
}

void MainWindow::raiseMixer(bool raise)
{
    if (m_mixerDock) {
        if (raise) {
            m_mixerDock->open();
        }
        m_mixerDock->setAsCurrentTab();
    }
}

void MainWindow::slotToggleAutoPreview(bool enable)
{
    KdenliveSettings::setAutopreview(enable);
    if (enable && getCurrentTimeline()) {
        getCurrentTimeline()->controller()->startPreviewRender();
    }
}

void MainWindow::showTimelineToolbarMenu(const QPoint &pos)
{
    QMenu menu;
    menu.addAction(actionCollection()->action(KStandardAction::name(KStandardAction::ConfigureToolbars)));
    QMenu *contextSize = new QMenu(i18n("Icon Size"));
    menu.addMenu(contextSize);
    auto *sizeGroup = new QActionGroup(contextSize);
    int currentSize = m_timelineToolBar->iconSize().width();
    QAction *a = new QAction(i18nc("@item:inmenu Icon size", "Default"), contextSize);
    a->setData(m_timelineToolBar->iconSizeDefault());
    a->setCheckable(true);
    if (m_timelineToolBar->iconSizeDefault() == currentSize) {
        a->setChecked(true);
    }
    a->setActionGroup(sizeGroup);
    contextSize->addAction(a);
    KIconTheme *theme = KIconLoader::global()->theme();
    QList<int> avSizes;
    if (theme) {
        avSizes = theme->querySizes(KIconLoader::Toolbar);
    }

    std::sort(avSizes.begin(), avSizes.end());

    if (avSizes.count() < 10) {
        // Fixed or threshold type icons
        for (int it : avSizes) {
            QString text;
            if (it < 19) {
                text = i18n("Small (%1x%2)", it, it);
            } else if (it < 25) {
                text = i18n("Medium (%1x%2)", it, it);
            } else if (it < 35) {
                text = i18n("Large (%1x%2)", it, it);
            } else {
                text = i18n("Huge (%1x%2)", it, it);
            }

            // save the size in the contextIconSizes map
            auto *sizeAction = new QAction(text, contextSize);
            sizeAction->setData(it);
            sizeAction->setCheckable(true);
            sizeAction->setActionGroup(sizeGroup);
            if (it == currentSize) {
                sizeAction->setChecked(true);
            }
            contextSize->addAction(sizeAction);
        }
    } else {
        // Scalable icons.
        const int progression[] = {16, 22, 32, 48, 64, 96, 128, 192, 256};

        for (int i : progression) {
            for (int it : avSizes) {
                if (it >= i) {
                    QString text;
                    if (it < 19) {
                        text = i18n("Small (%1x%2)", it, it);
                    } else if (it < 25) {
                        text = i18n("Medium (%1x%2)", it, it);
                    } else if (it < 35) {
                        text = i18n("Large (%1x%2)", it, it);
                    } else {
                        text = i18n("Huge (%1x%2)", it, it);
                    }

                    // save the size in the contextIconSizes map
                    auto *sizeAction = new QAction(text, contextSize);
                    sizeAction->setData(it);
                    sizeAction->setCheckable(true);
                    sizeAction->setActionGroup(sizeGroup);
                    if (it == currentSize) {
                        sizeAction->setChecked(true);
                    }
                    contextSize->addAction(sizeAction);
                    break;
                }
            }
        }
    }
    connect(contextSize, &QMenu::triggered, this, &MainWindow::setTimelineToolbarIconSize);
    menu.exec(m_timelineToolBar->mapToGlobal(pos));
    contextSize->deleteLater();
}

void MainWindow::setTimelineToolbarIconSize(QAction *a)
{
    if (!a) {
        return;
    }
    int size = a->data().toInt();
    m_timelineToolBar->setIconDimensions(size);
    KSharedConfigPtr config = KSharedConfig::openConfig();
    KConfigGroup mainConfig(config, QStringLiteral("MainWindow"));
    KConfigGroup tbGroup(&mainConfig, QStringLiteral("Toolbar timelineToolBar"));
    m_timelineToolBar->saveSettings(tbGroup);
}

void MainWindow::slotManageCache()
{
    QPointer<TemporaryData> d(new TemporaryData(pCore->currentDoc(), false, this));
    connect(d, &TemporaryData::disableProxies, this, &MainWindow::slotDisableProxies);
    d->exec();
}

void MainWindow::slotUpdateCompositing(bool checked)
{
    getCurrentTimeline()->controller()->switchCompositing(checked);
    pCore->currentDoc()->setModified();
}

void MainWindow::slotUpdateCompositeAction(bool enable)
{
    m_compositeAction->setChecked(enable);
}

void MainWindow::showMenuBar(bool show)
{
    if (!show && toolBar()->isHidden()) {
        KMessageBox::information(this, i18n("This will hide the menu bar completely. You can show it again by typing Ctrl+M."), i18n("Hide menu bar"),
                                 QStringLiteral("show-menubar-warning"));
    }
    menuBar()->setVisible(show);
}

TimelineWidget *MainWindow::getCurrentTimeline() const
{
    return m_timelineTabs->getCurrentTimeline();
}

TimelineWidget *MainWindow::getTimeline(const QUuid uuid) const
{
    return m_timelineTabs->getTimeline(uuid);
}

void MainWindow::getSequenceProperties(const QUuid &uuid, QMap<QString, QString> &props)
{
    TimelineWidget *w = getTimeline(uuid);
    if (w) {
        w->controller()->getSequenceProperties(props);
    }
}

bool MainWindow::hasTimeline() const
{
    return m_timelineTabs != nullptr;
}

void MainWindow::closeTimelineTab(const QUuid uuid, bool onDeletion, bool checkActiveClosed)
{
    m_timelineTabs->closeTimelineTab(uuid, checkActiveClosed);
    if (onDeletion) {
        resetSubtitles(uuid);
    }
}

const QStringList MainWindow::openedSequences() const
{
    if (m_timelineTabs) {
        return m_timelineTabs->openedSequences();
    }
    return QStringList();
}

void MainWindow::resetTimelineTracks()
{
    TimelineWidget *current = getCurrentTimeline();
    if (current) {
        current->controller()->resetTrackHeight();
    }
}

void MainWindow::slotSwitchTimelineZone(bool active)
{
    pCore->currentDoc()->setDocumentProperty(QStringLiteral("enableTimelineZone"), active ? QStringLiteral("1") : QStringLiteral("0"));
    Q_EMIT getCurrentTimeline()->controller()->useRulerChanged();
    QSignalBlocker blocker(m_useTimelineZone);
    m_useTimelineZone->setActive(active);
}

void MainWindow::slotGrabItem()
{
    getCurrentTimeline()->focusTimeline();
    getCurrentTimeline()->controller()->grabCurrent();
}

void MainWindow::slotAudioZoomIn()
{
    if (KdenliveSettings::normalizechannels()) {
        KdenliveSettings::setNormalizechannels(false);
    }
    if (KdenliveSettings::waveformScaler() < 5) {
        KdenliveSettings::setWaveformScaler(KdenliveSettings::waveformScaler() * 2);
    }
    slotNormalizeAudioChannel(true);
}

void MainWindow::slotAudioZoomOut()
{
    if (KdenliveSettings::normalizechannels()) {
        KdenliveSettings::setNormalizechannels(false);
    }
    if (KdenliveSettings::waveformScaler() > 1) {
        KdenliveSettings::setWaveformScaler(KdenliveSettings::waveformScaler() / 2);
    }
    slotNormalizeAudioChannel(true);
}

void MainWindow::slotAudioZoomReset()
{
    if (KdenliveSettings::normalizechannels()) {
        KdenliveSettings::setNormalizechannels(false);
    }
    KdenliveSettings::setWaveformScaler(1);
    slotNormalizeAudioChannel(true);

    // Close the audio thumbs menu when reset is clicked
    if (m_audioThumbsMenu && m_audioThumbsMenu->isVisible()) {
        m_audioThumbsMenu->close();
    }
}

void MainWindow::slotAudioZoomCycle()
{
    if (KdenliveSettings::normalizechannels()) {
        KdenliveSettings::setNormalizechannels(false);
    }
    if (KdenliveSettings::waveformScaler() < 5) {
        KdenliveSettings::setWaveformScaler(KdenliveSettings::waveformScaler() * 2);
    } else {
        KdenliveSettings::setWaveformScaler(1);
    }
    slotNormalizeAudioChannel(true);
}

void MainWindow::updateAudioWaveformActionIcon()
{
    // TODO: remove icon check once we require KF > 6.1
    QString waveformIconName = QIcon::hasThemeIcon(QStringLiteral("waveform")) ? QStringLiteral("waveform") : QStringLiteral("kdenlive-show-audiothumb");
    if (KdenliveSettings::waveformScaler() > 1 && KdenliveSettings::audiothumbnails()) {
        int iconSize = style()->pixelMetric(QStyle::PM_SmallIconSize);
        QImage img = QIcon::fromTheme(waveformIconName).pixmap(iconSize, iconSize).toImage();
        KIconEffect::toMonochrome(img, Qt::red, Qt::red, 1.0);
        m_buttonAudioThumbs->setIcon(QIcon(QPixmap::fromImage(img)));
    } else {
        m_buttonAudioThumbs->setIcon(QIcon::fromTheme(waveformIconName));
    }
}

void MainWindow::slotCollapse()
{
    if ((QApplication::focusWidget() != nullptr) && (QApplication::focusWidget()->parentWidget() != nullptr) &&
        QApplication::focusWidget()->parentWidget() == pCore->bin()) {
        // Bin expand/collapse
        pCore->bin()->expandCurrent();

    } else {
        QWidget *widget = QApplication::focusWidget();
        while ((widget != nullptr) && widget != this) {
            if (widget == m_effectStackDock) {
                m_assetPanel->collapseCurrentEffect();
                return;
            }
            widget = widget->parentWidget();
        }
        // Collapse / expand track
        getCurrentTimeline()->controller()->collapseActiveTrack();
    }
}

void MainWindow::slotCollapseAll()
{
    if ((QApplication::focusWidget() != nullptr) && (QApplication::focusWidget()->parentWidget() != nullptr) &&
        QApplication::focusWidget()->parentWidget() == pCore->bin()) {
        // Bin expand/collapse
        pCore->bin()->expandAll();

    } else {
        QWidget *widget = QApplication::focusWidget();
        while ((widget != nullptr) && widget != this) {
            if (widget == m_effectStackDock) {
                Q_EMIT m_assetPanel->slotSwitchCollapseAll();
                return;
            }
            widget = widget->parentWidget();
        }
        // Collapse / expand track
        getCurrentTimeline()->controller()->collapseAllTracks();
    }
}

bool MainWindow::timelineVisible() const
{
    return !centralWidget()->isHidden();
}

void MainWindow::slotActivateVideoTrackSequence()
{
    auto *action = qobject_cast<QAction *>(sender());
    const QList<int> trackIds = getCurrentTimeline()->model()->getTracksIds(false);
    int trackPos = qBound(0, action->data().toInt(), trackIds.count() - 1);
    int tid = trackIds.at(trackIds.count() - 1 - trackPos);
    getCurrentTimeline()->controller()->setActiveTrack(tid);
    if (m_activeTool == ToolType::MulticamTool) {
        pCore->monitorManager()->slotPerformMultiTrackMode();
    }
}

void MainWindow::resetSubtitles(const QUuid &uuid)
{
    // Hide subtitle track
    m_buttonSubtitleEditTool->setChecked(false);
    KdenliveSettings::setShowSubtitles(false);
    pCore->subtitleWidget()->setModel(nullptr);
    if (pCore->currentDoc()) {
        std::shared_ptr<TimelineItemModel> timeline = pCore->currentDoc()->getTimeline(uuid);
        if (timeline && timeline->hasSubtitleModel()) {
            auto subModel = timeline->getSubtitleModel();
            QMap<std::pair<int, QString>, QString> currentSubs = subModel->getSubtitlesList();
            QMapIterator<std::pair<int, QString>, QString> i(currentSubs);
            while (i.hasNext()) {
                i.next();
                const QString workPath = pCore->currentDoc()->subTitlePath(uuid, i.key().first, false);
                QFile workFile(workPath);
                if (workFile.exists()) {
                    workFile.remove();
                }
            }
        }
    }
}

void MainWindow::slotShowSubtitles(bool show)
{
    const QUuid uuid = getCurrentTimeline()->model()->uuid();
    KdenliveSettings::setShowSubtitles(show);
    if (getCurrentTimeline()->model()->hasSubtitleModel()) {
        getCurrentTimeline()->connectSubtitleModel(false);
    } else {
        QMap<QString, QString> props = QMap<QString, QString>();
        slotEditSubtitle(props);
    }
    pCore->currentDoc()->setSequenceProperty(uuid, QStringLiteral("hidesubtitle"), show ? 0 : 1);
}

void MainWindow::slotInitSubtitle(const QMap<QString, QString> &subProperties, const QUuid &uuid)
{
    std::shared_ptr<TimelineItemModel> timeline = pCore->currentDoc()->getTimeline(uuid);
    Q_ASSERT(!timeline->hasSubtitleModel());
    std::shared_ptr<SubtitleModel> subtitleModel = timeline->createSubtitleModel();
    // Starting a new subtitle for this project
    pCore->subtitleWidget()->setModel(subtitleModel);
    subtitleModel->loadProperties(subProperties);
    if (uuid == pCore->currentTimelineId() && pCore->currentDoc()->getSequenceProperty(uuid, QStringLiteral("hidesubtitle")).toInt() == 0) {
        KdenliveSettings::setShowSubtitles(true);
        m_buttonSubtitleEditTool->setChecked(true);
        getCurrentTimeline()->connectSubtitleModel(true);
    }
}

void MainWindow::slotEditSubtitle(const QMap<QString, QString> &subProperties)
{
    bool hasSubtitleModel = getCurrentTimeline()->hasSubtitles();
    if (!hasSubtitleModel) {
        std::shared_ptr<SubtitleModel> subtitleModel = getCurrentTimeline()->model()->createSubtitleModel();
        // Starting a new subtitle for this project
        pCore->subtitleWidget()->setModel(subtitleModel);
        m_buttonSubtitleEditTool->setChecked(true);
        KdenliveSettings::setShowSubtitles(true);
        if (!subProperties.isEmpty()) {
            subtitleModel->loadProperties(subProperties);
            // Load the disabled / locked state of the subtitle
            Q_EMIT getCurrentTimeline()->controller()->subtitlesLockedChanged();
            Q_EMIT getCurrentTimeline()->controller()->subtitlesDisabledChanged();
        }
        getCurrentTimeline()->connectSubtitleModel(true);
        // Update subtitle track combo list
        Q_EMIT getCurrentTimeline()->controller()->subtitlesListChanged();
    } else {
        KdenliveSettings::setShowSubtitles(m_buttonSubtitleEditTool->isChecked());
        getCurrentTimeline()->connectSubtitleModel(false);
    }
}

void MainWindow::slotAddSubtitle(const QString &text)
{
    showSubtitleTrack();
    getCurrentTimeline()->model()->getSubtitleModel()->addSubtitle(-1, 0, text);
}

void MainWindow::showSubtitleTrack()
{
    if (!getCurrentTimeline()->hasSubtitles() || !m_buttonSubtitleEditTool->isChecked()) {
        m_buttonSubtitleEditTool->setChecked(true);
        slotEditSubtitle();
    }
}

void MainWindow::slotImportSubtitle()
{
    showSubtitleTrack();
    getCurrentTimeline()->controller()->importSubtitle();
}

void MainWindow::slotManageSubtitle()
{
    showSubtitleTrack();
    getCurrentTimeline()->controller()->subtitlesMenuActivated(-1);
}

void MainWindow::slotSpeechRecognition()
{
    if (!getCurrentTimeline()->hasSubtitles()) {
        slotEditSubtitle();
    }
    getCurrentTimeline()->controller()->subtitleSpeechRecognition();
}

void MainWindow::slotCopyDebugInfo()
{
    // General note for this function: since the information targets developers, we don't want it to be translated

    QString debuginfo;
    QString packageType;
    switch (pCore->packageType()) {
    case LinuxPackageType::AppImage:
        packageType = QStringLiteral("AppImage");
        break;
    case LinuxPackageType::Flatpak:
        packageType = QStringLiteral("Flatpak");
        break;
    case LinuxPackageType::Snap:
        packageType = QStringLiteral("Snap");
        break;
    default:
        packageType = QStringLiteral("Unknown/Default");
        break;
    }
    QList<KAboutComponent> components = KAboutData::applicationData().components();
    for (auto &c : components) {
        debuginfo.append(QStringLiteral("%1: %2\n").arg(c.name(), c.version()));
    }
    debuginfo.append(QStringLiteral("Package Type: %1\n").arg(packageType));
    debuginfo.append(QStringLiteral("Qt: %1 (built against %2 %3)\n").arg(QString::fromLocal8Bit(qVersion()), QT_VERSION_STR, QSysInfo::buildAbi()));
    debuginfo.append(QStringLiteral("Frameworks: %2\n").arg(KCoreAddons::versionString()));
    debuginfo.append(QStringLiteral("System: %1\n").arg(QSysInfo::prettyProductName()));
    debuginfo.append(QStringLiteral("Kernel: %1 %2\n").arg(QSysInfo::kernelType(), QSysInfo::kernelVersion()));
    debuginfo.append(QStringLiteral("CPU: %1\n").arg(QSysInfo::currentCpuArchitecture()));
    debuginfo.append(QStringLiteral("Windowing System: %1\n").arg(QGuiApplication::platformName()));
    if (m_clipMonitor) {
        debuginfo.append(QStringLiteral("GPU: %1\n").arg(m_clipMonitor->getGPUInfo().join(QLatin1Char('/'))));
    }
    debuginfo.append(QStringLiteral("Movit (GPU): %1\n").arg(KdenliveSettings::gpu_accel() ? QStringLiteral("enabled") : QStringLiteral("disabled")));
    debuginfo.append(QStringLiteral("Track Compositing: %1\n").arg(TransitionsRepository::get()->getCompositingTransition()));
    QClipboard *clipboard = QApplication::clipboard();
    clipboard->setText(debuginfo);
}

bool MainWindow::eventFilter(QObject *object, QEvent *event)
{
    switch (event->type()) {
    case QEvent::ShortcutOverride:
        if (static_cast<QKeyEvent *>(event)->key() == Qt::Key_Escape) {
            if (pCore->isMediaCapturing()) {
                pCore->switchCapture();
                return true;
            }
            if (pCore->isMediaMonitoring()) {
                slotShowTrackRec(false);
                return true;
            }
            if (m_commandStack && m_commandStack->activeStack()) {
                if (m_commandStack->activeStack()->canUndo()) {
                    if (m_activeTool != ToolType::SelectTool) {
                        m_buttonSelectTool->trigger();
                        return true;
                    } else {
                        // Don't call selection clear if a drag operation is in progress
                        getCurrentTimeline()->model()->requestClearSelection();
                    }
                    return true;
                }
            }
        }
        break;
    case QEvent::ApplicationPaletteChange: {
        if (m_assetPanel) {
            m_assetPanel->clear();
        }
        if (m_clipMonitor) {
            m_clipMonitor->setPalette(qApp->palette());
        }
        if (m_projectMonitor) {
            m_projectMonitor->setPalette(qApp->palette());
        }
        if (m_timelineTabs) {
            m_timelineTabs->setPalette(qApp->palette());
            if (getCurrentTimeline() && getCurrentTimeline()->controller()) {
                getCurrentTimeline()->controller()->resetView();
            }
        }
        applyToolMessageStyling();
        applyZoomLevelButtonStyling();

        for (KDDockWidgets::Core::Group *group : KDDockWidgets::DockRegistry::self()->groups()) {
            auto tab_bar = static_cast<KDDockWidgets::QtWidgets::TabBar *>(group->tabBar()->view());
            if (QProxyStyle *style = qobject_cast<QProxyStyle *>(tab_bar->style())) {
                style->setBaseStyle(QStyleFactory::create(qApp->style()->name()));
                tab_bar->setPalette(qApp->palette());
            }
        }

        Q_EMIT pCore->updatePalette();
        break;
    }
    default:
        break;
    }
    return QObject::eventFilter(object, event);
}

void MainWindow::slotRemoveBinDock(const QString &name)
{
    QWidget *toDelete = nullptr;
    int ix = 0;
    for (auto &b : m_binWidgets) {
        if (b->parentWidget()->objectName() == name) {
            toDelete = b->parentWidget();
            m_binWidgets.takeAt(ix);
            break;
        }
        ix++;
    }
    if (toDelete) {
        toDelete->deleteLater();
    }
    if (!m_windowClosing) {
        KdenliveSettings::setBinsCount(m_binWidgets.size());
        updateDockMenu();
    }
}

void MainWindow::addBin(Bin *bin, const QString &binName, bool updateCount, const QString &objectName)
{
    connect(bin, &Bin::findInTimeline, this, &MainWindow::slotClipInTimeline, Qt::DirectConnection);
    connect(bin, &Bin::setupTargets, this, [&](bool hasVideo, QMap<int, QString> audioStreams) {
        if (getCurrentTimeline() && getCurrentTimeline()->controller()) {
            getCurrentTimeline()->controller()->setTargetTracks(hasVideo, audioStreams);
        }
    });
    connect(bin, &Bin::requestBinCloseForFolder, this, [this](const QString &folderId) {
        for (auto &b : m_binWidgets) {
            if (b->rootFolderId() == folderId) {
                Q_EMIT removeBinDock(b->parentWidget()->objectName());
            }
        }
    });
    m_binWidgets << bin;
    if (m_binWidgets.size() > 1) {
        // This is a secondary bin widget
        int ix = 2;
        // Ensure we have a unique id
        QStringList objectNames;
        for (auto &b : m_binWidgets) {
            QWidget *p = b->parentWidget();
            if (p) {
                objectNames << p->objectName();
            }
        }
        QString newBinName = objectName.isEmpty() ? QStringLiteral("project_bin_%1").arg(ix) : objectName;
        while (objectNames.contains(newBinName)) {
            ix++;
            newBinName = QStringLiteral("project_bin_%1").arg(ix);
        }
        KDDockWidgets::QtWidgets::DockWidget *binDock =
            addDock(binName.isEmpty() ? i18n("Project Bin %1", ix) : binName, newBinName, bin, KDDockWidgets::Location_None, m_projectBinDock);
        if (pCore->guiReady()) {
            bin->setupGeneratorMenu();
        }
        connect(bin, &Bin::requestShowClipProperties, getBin(), &Bin::showClipProperties, Qt::QueuedConnection);
        connect(bin, &Bin::requestBinClose, this, [this, binDock]() { Q_EMIT removeBinDock(binDock->objectName()); });
        binDock->open();
        binDock->setAsCurrentTab();
    }
    if (updateCount) {
        KdenliveSettings::setBinsCount(m_binWidgets.size());
    }
    if (pCore->guiReady()) {
        updateDockMenu();
    }
}

void MainWindow::cleanBins()
{
    // Close secondary bins
    QWidget *wid = QApplication::focusWidget();
    bool binHasFocus = false;
    while (m_binWidgets.size() > 1) {
        int ix = 0;
        auto bin = m_binWidgets.at(ix);
        if (!binHasFocus && (bin == wid || bin->isAncestorOf(wid))) {
            binHasFocus = true;
        }
        if (bin->isMainBin()) {
            ix = 1;
            bin = m_binWidgets.at(ix);
            if (!binHasFocus && (bin == wid || bin->isAncestorOf(wid))) {
                binHasFocus = true;
            }
        }
        auto toDelete = bin->parentWidget();
        m_binWidgets.takeAt(ix);
        delete toDelete;
    }

    // Clean main bins last
    for (auto &bin : m_binWidgets) {
        if (!bin->isMainBin()) {
            continue;
        }
        bin->cleanDocument();
    }
    // Ensure monitor is cleared
    if (!binHasFocus) {
        pCore->getMonitor(Kdenlive::ClipMonitor)->refreshMonitor();
    }
}

void MainWindow::loadExtraBins(const QStringList binInfo)
{
    QString folderName;
    QStringList existingNames;
    pCore->lastActiveBin.clear();

    for (auto &bin : m_binWidgets) {
        KDDockWidgets::QtWidgets::DockWidget *dock = qobject_cast<KDDockWidgets::QtWidgets::DockWidget *>(bin->parentWidget());
        if (!dock) {
            continue;
        }
        bool binFound = false;
        const QString dockName = dock->objectName() + QLatin1Char(':');
        for (const QString &info : binInfo) {
            if (info.startsWith(dockName)) {
                existingNames << bin->loadInfo(info.split(QLatin1Char(':')), existingNames);
                binFound = true;
                break;
            }
        }
        if (!binFound) {
            // Init bin with default settings
            existingNames << bin->loadInfo({}, existingNames);
        }
    }
}

const QStringList MainWindow::extraBinIds() const
{
    QStringList ids;
    for (auto &b : m_binWidgets) {
        ids << b->binInfoToString();
    }
    return ids;
}

void MainWindow::folderRenamed(const QString &binId, const QString &folderName)
{
    for (auto &b : m_binWidgets) {
        // Find out dock widget
        if (b->rootFolderId() == binId) {
            KDDockWidgets::QtWidgets::DockWidget *dock = qobject_cast<KDDockWidgets::QtWidgets::DockWidget *>(b->parentWidget());
            if (dock) {
                dock->setWindowTitle(folderName);
            }
            break;
        }
    }
}

void MainWindow::blockBins(bool block)
{
    for (auto &b : m_binWidgets) {
        b->blockBin(block);
    }
}

Bin *MainWindow::getBin()
{
    if (m_binWidgets.isEmpty()) {
        return nullptr;
    }
    return m_binWidgets.first();
}

Bin *MainWindow::activeBin()
{
    if (!pCore->lastActiveBin.isEmpty()) {
        for (auto &bin : m_binWidgets) {
            if (bin->parentWidget()->objectName() == pCore->lastActiveBin) {
                return bin;
            }
        }
    }
    QWidget *wid = QApplication::focusWidget();
    if (wid) {
        for (auto &bin : m_binWidgets) {
            if (bin == wid || bin->isAncestorOf(wid)) {
                return bin;
            }
        }
    }
    return m_binWidgets.first();
}

int MainWindow::binCount() const
{
    if (m_binWidgets.isEmpty()) {
        return 0;
    }
    return m_binWidgets.count();
}

void MainWindow::checkMaxCacheSize()
{
    if (KdenliveSettings::lastCacheCheck().daysTo(QDateTime::currentDateTime()) < 12) {
        return;
    }
    if (KdenliveSettings::checkForUpdate()) {
        // Check if the Kdenlive version is very old
        const QStringList kdenliveVersion = KAboutData::applicationData().version().split(QLatin1Char('.'));
        if (kdenliveVersion.size() > 2) {
            bool ok;
            int kdenliveYear = kdenliveVersion.at(0).toInt(&ok);
            if (ok) {
                int kdenliveMonth = kdenliveVersion.at(1).toInt(&ok);
                if (ok) {
                    if (kdenliveYear < 100) {
                        kdenliveYear += 2000;
                    }
                    QDate releaseDate = QDate(kdenliveYear, kdenliveMonth, 1);
                    if (releaseDate.isValid()) {
                        int days = releaseDate.daysTo(QDate::currentDate());
                        if (days > 180) {
                            // Propose update
                            QAction *updateAction = new QAction(i18n("Go to download page"), this);
                            connect(updateAction, &QAction::triggered, this, []() {
                                QDesktopServices::openUrl(
                                    QUrl(QStringLiteral("https://kdenlive.org/download?utm_campaign=kdenlive_inapp&utm_term=update_reminder&utm_content=%1")
                                             .arg(KAboutData::applicationData().version())));
                            });
                            QAction *abortAction = new QAction(i18n("Never check again"), this);
                            connect(abortAction, &QAction::triggered, this, []() { KdenliveSettings::setCheckForUpdate(false); });
                            if (days > 360) {
                                Q_EMIT pCore->displayBinMessage(i18n("Your Kdenlive version is older than 1 year, we strongly encourage you to upgrade"),
                                                                KMessageWidget::Warning, {updateAction, abortAction}, true,
                                                                BinMessage::BinCategory::UpdateMessage);
                            } else {
                                Q_EMIT pCore->displayBinMessage(i18n("Your Kdenlive version is older than 6 months, we encourage you to upgrade"),
                                                                KMessageWidget::Information, {updateAction, abortAction}, true,
                                                                BinMessage::BinCategory::UpdateMessage);
                            }
                        }
                    }
                }
            }
        }
    }

    KdenliveSettings::setLastCacheCheck(QDateTime::currentDateTime());
    // Check cached data size
    if (KdenliveSettings::maxcachesize() <= 0 || pCore->currentDoc() == nullptr) {
        return;
    }
    bool ok;
    QDir cacheDir = pCore->currentDoc()->getCacheDir(SystemCacheRoot, &ok);
    if (!ok) {
        return;
    }
    QDir backupFolder(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + QStringLiteral("/.backup"));
    QList<QDir> toAdd;
    QList<QDir> toRemove;
    if (cacheDir.exists()) {
        toAdd << cacheDir;
    }
    if (backupFolder.exists()) {
        toAdd << cacheDir;
    }
    if (cacheDir.cd(QStringLiteral("knewstuff"))) {
        toRemove << cacheDir;
        cacheDir.cdUp();
    }
    if (cacheDir.cd(QStringLiteral("attica"))) {
        toRemove << cacheDir;
        cacheDir.cdUp();
    }
    if (cacheDir.cd(QStringLiteral("proxy"))) {
        toRemove << cacheDir;
        cacheDir.cdUp();
    }
    pCore->displayMessage(i18n("Checking cached data size"), InformationMessage);
    while (!toAdd.isEmpty()) {
        QDir dir = toAdd.takeFirst();
        m_totalCacheJobs++;
        QFutureWatcher<KIO::filesize_t> *watcher = new QFutureWatcher<KIO::filesize_t>(this);
        connect(watcher, &QFutureWatcherBase::finished, this, [this, watcher] {
            KIO::filesize_t size = watcher->result();
            m_totalCacheSize += size;
            m_totalCacheJobs--;
            watcher->deleteLater();
            if (m_totalCacheJobs == 0 && m_totalCacheSize > KIO::filesize_t(1048576) * KdenliveSettings::maxcachesize()) {
                slotManageCache();
            }
        });
        QFuture<KIO::filesize_t> future = QtConcurrent::run(&MainWindow::fetchFolderSize, this, dir.absolutePath());
        watcher->setFuture(future);
    }
    while (!toRemove.isEmpty()) {
        QDir dir = toRemove.takeFirst();
        m_totalCacheJobs++;
        QFutureWatcher<KIO::filesize_t> *watcher = new QFutureWatcher<KIO::filesize_t>(this);
        connect(watcher, &QFutureWatcherBase::finished, this, [this, watcher] {
            KIO::filesize_t size = watcher->result();
            m_totalCacheSize -= size;
            m_totalCacheJobs--;
            watcher->deleteLater();
            if (m_totalCacheJobs == 0 && m_totalCacheSize > KIO::filesize_t(1048576) * KdenliveSettings::maxcachesize()) {
                slotManageCache();
            }
        });
        QFuture<KIO::filesize_t> future = QtConcurrent::run(&MainWindow::fetchFolderSize, this, dir.absolutePath());
        watcher->setFuture(future);
    }
}

void MainWindow::manageClipJobs(AbstractTask::JOBTYPE type, QWidget *parentWidget)
{
    QScopedPointer<ClipJobManager> dialog(new ClipJobManager(type, parentWidget ? parentWidget : this));
    dialog->exec();
    // Rebuild list of clip jobs
    buildDynamicActions();
    loadClipActions();
}

TimelineWidget *MainWindow::openTimeline(const QUuid &uuid, int ix, const QString &tabName, std::shared_ptr<TimelineItemModel> timelineModel,
                                         bool openInMonitor, bool previewEnabled)
{
    // Create a new timeline tab
    KdenliveDoc *project = pCore->currentDoc();
    TimelineWidget *timeline = m_timelineTabs->addTimeline(uuid, ix, tabName, timelineModel, pCore->monitorManager()->projectMonitor()->getControllerProxy(),
                                                           openInMonitor, previewEnabled);
    slotSetZoom(project->zoom(uuid).x(), false);
    if (openInMonitor) {
        m_projectMonitor->slotLoadClipZone(project->zone(uuid));
    }
    getTimeline(uuid)->controller()->setZone(project->zone(uuid), false);
    getTimeline(uuid)->controller()->setScrollPos(project->getSequenceProperty(uuid, QStringLiteral("scrollPos")).toInt());
    return timeline;
}

void MainWindow::seekIfCurrent(const QUuid uuid, int pos)
{
    if (getCurrentTimeline()->getUuid() == uuid) {
        pCore->monitorManager()->projectMonitor()->slotSeek(pos);
    }
}

bool MainWindow::raiseTimeline(const QUuid &uuid)
{
    return m_timelineTabs->raiseTimeline(uuid);
}

void MainWindow::connectTimelineApplication(TimelineWidget *timeline)
{
    connect(timeline, &TimelineWidget::zoomIn, this, &MainWindow::slotZoomIn);
    connect(timeline, &TimelineWidget::zoomOut, this, &MainWindow::slotZoomOut);
    connect(timeline, &TimelineWidget::processingDrag, this, &MainWindow::enableUndo);
    connect(timeline, &TimelineWidget::markerActivated, pCore->guidesList(), &GuidesList::markerActivated);
    connect(timeline, &TimelineWidget::updateTimelineMousePos, this, &MainWindow::slotUpdateMousePosition);
}

void MainWindow::connectTimeline()
{
    qDebug() << "::::::::::: connecting timeline: " << getCurrentTimeline()->getUuid() << ", DUR: " << getCurrentTimeline()->controller()->duration();
    if (!getCurrentTimeline()->model()) {
        qDebug() << "::::::::::: TIMELINE HAS NO MODEL";
    }
    // We just switched timeline, ensure a monitor effects view does not remain from previous
    m_projectMonitor->resetScene();
    const QUuid uuid = getCurrentTimeline()->getUuid();
    pCore->projectManager()->setActiveTimeline(uuid);
    connect(m_projectMonitor, &Monitor::multitrackView, getCurrentTimeline()->controller(), &TimelineController::slotMultitrackView, Qt::UniqueConnection);
    connect(m_projectMonitor, &Monitor::activateTrack, getCurrentTimeline()->controller(), &TimelineController::activateTrackAndSelect, Qt::UniqueConnection);
    connect(getCurrentTimeline()->controller(), &TimelineController::timelineClipSelected, this, [&](bool selected) {
        m_loopClip->setEnabled(selected);
        Q_EMIT pCore->library()->enableAddSelection(selected);
    });
    connect(pCore->library(), &LibraryWidget::saveTimelineSelection, getCurrentTimeline()->controller(), &TimelineController::saveTimelineSelection,
            Qt::UniqueConnection);
    connect(getCurrentTimeline()->controller(), &TimelineController::durationChanged, pCore->projectManager(), &ProjectManager::adjustProjectDuration);
    connect(pCore.get(), &Core::processDragEnd, getCurrentTimeline(), &TimelineWidget::endDrag);
    pCore->monitorManager()->activateMonitor(Kdenlive::ProjectMonitor);

    KdenliveDoc *project = pCore->currentDoc();
    QSignalBlocker blocker(m_zoomSlider);
    m_zoomSlider->setValue(project->zoom(uuid).x());
    pCore->monitorManager()->projectMonitor()->adjustRulerSize(getCurrentTimeline()->model()->duration() - 1, project->getFilteredGuideModel(uuid));
    pCore->monitorManager()->projectMonitor()->loadZone(getCurrentTimeline()->controller()->zoneIn(), getCurrentTimeline()->controller()->zoneOut());

    connect(project, &KdenliveDoc::docModified, this, &MainWindow::slotUpdateDocumentState);
    slotUpdateDocumentState(project->isModified());

    // Timeline preview
    QAction *previewRender = actionCollection()->action(QStringLiteral("prerender_timeline_zone"));
    if (previewRender) {
        previewRender->setEnabled(true);
    }
    connect(getCurrentTimeline()->controller(), &TimelineController::previewRefreshRequested, m_projectMonitor, &Monitor::refreshMonitor, Qt::UniqueConnection);

    // update track compositing
    bool compositing = project->getSequenceProperty(uuid, QStringLiteral("compositing"), QStringLiteral("1")).toInt() > 0;
    Q_EMIT project->updateCompositionMode(compositing);

    // Ensure the active timeline has an opaque black background for compositing
    getCurrentTimeline()->model()->makeTransparentBg(false);
    // Initialize audio mixer
    getCurrentTimeline()->model()->rebuildMixer();

    // Audio record actions
    connect(pCore.get(), &Core::recordAudio, getCurrentTimeline()->controller(), &TimelineController::switchRecording, Qt::DirectConnection);

    // switch to active subtitle model
    pCore->subtitleWidget()->setModel(getCurrentTimeline()->model()->getSubtitleModel());
    bool hasSubtitleModel = getCurrentTimeline()->hasSubtitles();
    Q_EMIT getCurrentTimeline()->controller()->subtitlesLockedChanged();
    Q_EMIT getCurrentTimeline()->controller()->subtitlesDisabledChanged();
    bool showSubs = project->getSequenceProperty(uuid, QStringLiteral("hidesubtitle")).toInt() == 0;
    KdenliveSettings::setShowSubtitles(showSubs && hasSubtitleModel);
    getCurrentTimeline()->connectSubtitleModel(hasSubtitleModel);
    m_buttonSubtitleEditTool->setChecked(showSubs && hasSubtitleModel);
    pCore->projectManager()->updateSequenceOffset(uuid);
    if (hasSubtitleModel) {
        // Restore style
        getCurrentTimeline()->model()->getSubtitleModel()->loadProperties({});
        slotShowSubtitles(showSubs);
    }
    // Display timeline guides in the guides list
    pCore->guidesList()->setModel(project->getGuideModel(uuid), project->getFilteredGuideModel(uuid));
    if (m_renderWidget) {
        slotCheckRenderStatus();
        m_renderWidget->setGuides(project->getGuideModel(uuid));
        m_renderWidget->showRenderDuration();
    }
}

void MainWindow::disconnectTimeline(TimelineWidget *timeline, bool onClose)
{
    // Save current tab timeline position
    if (pCore->currentDoc()) {
        // pCore->currentDoc()->position = pCore->getTimelinePosition();
        //  disconnect(pCore->currentDoc(), &KdenliveDoc::docModified, this, &MainWindow::slotUpdateDocumentState);
    }
    if (!onClose) {
        // Ensure the active timeline has an transparent black background for embedded compositing
        timeline->model()->makeTransparentBg(true);
        if (!pCore->currentDoc()->loading) {
            // Update audio thumb if necessary
            const QUuid uuid = timeline->getUuid();
            const QString binId = pCore->projectItemModel()->getSequenceId(uuid);
            getBin()->rebuildAudioThumb(binId);
        }
    }
    disconnect(timeline->controller(), &TimelineController::durationChanged, pCore->projectManager(), &ProjectManager::adjustProjectDuration);
    disconnect(timeline->controller(), &TimelineController::previewRefreshRequested, m_projectMonitor, &Monitor::refreshMonitor);
    disconnect(m_projectMonitor, &Monitor::multitrackView, timeline->controller(), &TimelineController::slotMultitrackView);
    disconnect(m_projectMonitor, &Monitor::activateTrack, timeline->controller(), &TimelineController::activateTrackAndSelect);
    disconnect(pCore->library(), &LibraryWidget::saveTimelineSelection, timeline->controller(), &TimelineController::saveTimelineSelection);
    disconnect(pCore.get(), &Core::processDragEnd, timeline, &TimelineWidget::endDrag);
    pCore->monitorManager()->projectMonitor()->setProducer(QUuid(), nullptr, -2);
    // Audio record actions
    disconnect(pCore.get(), &Core::recordAudio, timeline->controller(), &TimelineController::switchRecording);
}

// Static
QProcessEnvironment MainWindow::getCleanEnvironement()
{
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    qDebug() << "::: GOT ENV: " << env.value("LD_LIBRARY_PATH") << ", PATH: " << env.value("PATH") << "\n\nXDG:\n" << env.value("XDG_DATA_DIRS");
    QStringList libPath = env.value(QStringLiteral("LD_LIBRARY_PATH")).split(QLatin1Char(':'), Qt::SkipEmptyParts);
    QStringList updatedLDPath;
    for (auto &s : libPath) {
        if (!s.startsWith(QStringLiteral("/tmp/.mount_"))) {
            updatedLDPath << s;
        }
    }
    if (updatedLDPath.isEmpty()) {
        env.remove(QStringLiteral("LD_LIBRARY_PATH"));
    } else {
        env.insert(QStringLiteral("LD_LIBRARY_PATH"), updatedLDPath.join(QLatin1Char(':')));
    }
    // Path
    libPath = env.value(QStringLiteral("PATH")).split(QLatin1Char(':'), Qt::SkipEmptyParts);
    updatedLDPath.clear();
    for (auto &s : libPath) {
        if (!s.startsWith(QStringLiteral("/tmp/.mount_"))) {
            updatedLDPath << s;
        }
    }
    if (updatedLDPath.isEmpty()) {
        env.remove(QStringLiteral("PATH"));
    } else {
        env.insert(QStringLiteral("PATH"), updatedLDPath.join(QLatin1Char(':')));
    }
    // XDG
    libPath = env.value(QStringLiteral("XDG_DATA_DIRS")).split(QLatin1Char(':'), Qt::SkipEmptyParts);
    updatedLDPath.clear();
    for (auto &s : libPath) {
        if (!s.startsWith(QStringLiteral("/tmp/.mount_"))) {
            updatedLDPath << s;
        }
    }
    if (updatedLDPath.isEmpty()) {
        env.remove(QStringLiteral("XDG_DATA_DIRS"));
    } else {
        env.insert(QStringLiteral("XDG_DATA_DIRS"), updatedLDPath.join(QLatin1Char(':')));
    }
    env.remove(QStringLiteral("QT_QPA_PLATFORM"));
    return env;
}

void MainWindow::appHelpActivated()
{
    // Don't use default help, show our website
    // QDesktopServices::openUrl(QUrl(QStringLiteral("help:kdenlive")));
    const QString helpUrl = QStringLiteral("https://docs.kdenlive.org?utm_campaign=kdenlive_inapp&utm_term=help_action&utm_content=%1")
                                .arg(KAboutData::applicationData().version());
    if (pCore->packageType() == LinuxPackageType::AppImage) {
        qDebug() << "::::: LAUNCHING APPIMAGE BROWSER.........";
        QProcessEnvironment env = getCleanEnvironement();
        QProcess process;
        process.setProcessEnvironment(env);
        QString openPath = QStandardPaths::findExecutable(QStringLiteral("xdg-open"));
        qDebug() << "------------\nFOUND OPEN PATH: " << openPath;
        process.setProgram(openPath.isEmpty() ? QStringLiteral("xdg-open") : openPath);
        process.setArguments({helpUrl});
        process.startDetached();
    } else {
        QDesktopServices::openUrl(QUrl(helpUrl));
    }
}

ObjectId MainWindow::effectStackOwner()
{
    return m_assetPanel->effectStackOwner();
}

bool MainWindow::effectIsMasterOnly(const QString &assetId) const
{
    if (m_effectList2) {
        return m_effectList2->isMasterOnly(assetId);
    }
    return false;
}

void MainWindow::reloadAssetPanel()
{
    ObjectId owner = m_assetPanel->effectStackOwner();
    if (owner.type == KdenliveObjectType::NoItem) {
        return;
    }
    m_assetPanel->clearAssetPanel(-1);
    pCore->showEffectStackFromId(owner);
}

bool MainWindow::hasRunningTask() const
{
    return m_assetPanel->hasRunningTask();
}

bool MainWindow::hasRunningRenderTask() const
{
    if (m_renderWidget) {
        return m_renderWidget->isRendering();
    }
    return false;
}

KIO::filesize_t MainWindow::fetchFolderSize(const QString path)
{
    // KIO::DirectorySizeJob doesn't work on Windows, so use Qt only
    KIO::filesize_t totalSize = 0;
    const auto flags = QDirListing::IteratorFlag::FilesOnly | QDirListing::IteratorFlag::Recursive;
    for (const auto &dirEntry : QDirListing(path, flags)) {
        totalSize += dirEntry.size();
    }
    return totalSize;
}

#ifdef DEBUG_MAINW
#undef DEBUG_MAINW
#endif

void MainWindow::slotCreateRangeMarkerFromZone()
{
    if (!getCurrentTimeline() || !pCore->currentDoc()) {
        return;
    }

    if (pCore->monitorManager()->clipMonitor()->isActive()) {
        pCore->monitorManager()->clipMonitor()->slotCreateRangeMarkerFromZone();
    } else {
        pCore->monitorManager()->projectMonitor()->slotCreateRangeMarkerFromZone();
    }
}

void MainWindow::slotCreateRangeMarkerFromZoneQuick()
{
    if (!getCurrentTimeline() || !pCore->currentDoc()) {
        return;
    }

    if (pCore->monitorManager()->clipMonitor()->isActive()) {
        pCore->monitorManager()->clipMonitor()->slotCreateRangeMarkerFromZoneQuick();
    } else {
        pCore->monitorManager()->projectMonitor()->slotCreateRangeMarkerFromZoneQuick();
    }
}

QSize MainWindow::sizeHint() const
{
    const QSize desktopSize = QGuiApplication::primaryScreen()->availableSize();
    return KXmlGuiWindow::sizeHint().expandedTo(desktopSize * 0.8);
}

void MainWindow::slotEditToolbars()
{
    // backup all current shortcuts
    QMap<QString, QKeySequence> shortcutBackup;
    for (auto *action : actionCollection()->actions()) {
        if (!action->shortcut().isEmpty()) {
            shortcutBackup.insert(action->objectName(), action->shortcut());
        }
    }

    // open the standard KDE Toolbar Editor
    KEditToolBar dlg(guiFactory(), this);

    // connect dialog changes to Kdenlive's refresh/save function (Fixes visual toolbar updates)
    connect(&dlg, &KEditToolBar::newToolBarConfig, this, &MainWindow::saveNewToolbarConfig);

    dlg.exec();

    // restore shortcuts
    bool restorationHappened = false;

    for (auto i = shortcutBackup.begin(); i != shortcutBackup.end(); ++i) {
        QAction *action = actionCollection()->action(i.key());
        if (action && action->shortcut() != i.value()) {
            if (!i.value().isEmpty()) {
                for (auto *otherAction : actionCollection()->actions()) {
                    if (otherAction != action && otherAction->shortcut() == i.value()) {
                        otherAction->setShortcut(QKeySequence());
                    }
                }
            }
            action->setShortcut(i.value());
            actionCollection()->setShortcutsConfigurable(action, true);
            restorationHappened = true;
        }
    }

    // PERSISTENCE FIX: Force the action collection to write its current state (with our restored shortcuts)
    // to the standard XML file immediately.
    if (restorationHappened) {
        actionCollection()->writeSettings();
    }
}
