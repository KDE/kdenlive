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
#include "dialogs/kdenlivesettingsdialog.h"
#include "dialogs/renderwidget.h"
#include "dialogs/subtitleedit.h"
#include "dialogs/wizard.h"
#include "doc/docundostack.hpp"
#include "doc/kdenlivedoc.h"
#include "docktitlebarmanager.h"
#include "effects/effectbasket.h"
#include "effects/effectlist/view/effectlistwidget.hpp"
#include "jobs/audiolevelstask.h"
#include "jobs/customjobtask.h"
#include "jobs/scenesplittask.h"
#include "jobs/speedtask.h"
#include "jobs/stabilizetask.h"
#include "jobs/transcodetask.h"
#include "kdenlivesettings.h"
#include "layoutmanagement.h"
#include "library/librarywidget.h"
#ifdef NODBUS
#include "render/renderserver.h"
#else
#include "mainwindowadaptor.h"
#endif
#include "dialogs/textbasededit.h"
#include "dialogs/timeremap.h"
#include "lib/localeHandling.h"
#include "mltconnection.h"
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
#include "utils/thememanager.h"
#include "widgets/progressbutton.h"
#include <config-kdenlive.h>

#ifdef USE_JOGSHUTTLE
#include "jogshuttle/jogmanager.h"
#endif

#include "knewstuff_version.h"
#include "kwidgetsaddons_version.h"
#include "utils/KMessageBox_KdenliveCompat.h"
#include <KAboutData>
#include <KActionCollection>
#include <KActionMenu>
#include <KColorScheme>
#include <KConfigDialog>
#include <KCoreAddons>
#include <KDualAction>
#include <KEditToolBar>
#include <KIconTheme>
#include <KLocalizedString>
#include <KMessageBox>
#if KNEWSTUFF_VERSION >= QT_VERSION_CHECK(5, 240, 0)
#include <KNSWidgets/Dialog>
#else
#include <KNS3/QtQuickDialogWrapper>
#endif
#include <KNotifyConfigWidget>
#include <KRecentDirs>
#include <KShortcutsDialog>
#include <KStandardAction>
#include <KToggleFullScreenAction>
#include <KToolBar>
#include <KXMLGUIFactory>

#include <KConfigGroup>
#include <QAction>
#include <QClipboard>
#include <QDesktopServices>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QMenu>
#include <QMenuBar>
#include <QPushButton>
#include <QScreen>
#include <QStandardPaths>
#include <QStatusBar>
#include <QStyleFactory>
#include <QUndoGroup>
#include <QVBoxLayout>

static const char version[] = KDENLIVE_VERSION;
namespace Mlt {
class Producer;
}

QMap<QString, QImage> MainWindow::m_lumacache;
QMap<QString, QStringList> MainWindow::m_lumaFiles;

/*static bool sortByNames(const QPair<QString, QAction *> &a, const QPair<QString, QAction*> &b)
{
    return a.first < b.first;
}*/

// determine the default KDE style as defined BY THE USER
// (as opposed to whatever style KDE considers default)
static QString defaultStyle(const char *fallback = nullptr)
{
    KSharedConfigPtr kdeGlobals = KSharedConfig::openConfig(QStringLiteral("kdeglobals"), KConfig::NoGlobals);
    KConfigGroup cg(kdeGlobals, "KDE");
    return cg.readEntry("widgetStyle", fallback);
}

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
    category = new KActionCategory(i18n("Navigation and Playback"), actionCollection());
    kdenliveCategoryMap.insert(QStringLiteral("navandplayback"), category);
    category = new KActionCategory(i18n("Bin Tags"), actionCollection());
    kdenliveCategoryMap.insert(QStringLiteral("bintags"), category);
}

void MainWindow::init(const QString &mltPath)
{
    QString desktopStyle = QApplication::style()->objectName();
    // Load themes
    auto themeManager = new ThemeManager(actionCollection());
    actionCollection()->addAction(QStringLiteral("themes_menu"), themeManager->menu());
    connect(themeManager, &ThemeManager::themeChanged, this, &MainWindow::slotThemeChanged);
    Q_EMIT pCore->updatePalette();

    if (!KdenliveSettings::widgetstyle().isEmpty() && QString::compare(desktopStyle, KdenliveSettings::widgetstyle(), Qt::CaseInsensitive) != 0) {
        // User wants a custom widget style, init
        doChangeStyle();
    }

    // Widget themes for non KDE users
    KActionMenu *stylesAction = new KActionMenu(i18n("Style"), this);
    auto *stylesGroup = new QActionGroup(stylesAction);

    // GTK theme does not work well with Kdenlive, and does not support color theming, so avoid it
    QStringList availableStyles = QStyleFactory::keys();
    if (KdenliveSettings::widgetstyle().isEmpty()) {
        // First run
        QStringList incompatibleStyles = {QStringLiteral("GTK+"), QStringLiteral("windowsvista"), QStringLiteral("Windows"), QStringLiteral("macintosh")};

        if (incompatibleStyles.contains(desktopStyle, Qt::CaseInsensitive)) {
            if (availableStyles.contains(QStringLiteral("breeze"), Qt::CaseInsensitive)) {
                // Auto switch to Breeze theme
                KdenliveSettings::setWidgetstyle(QStringLiteral("Breeze"));
                QApplication::setStyle(QStyleFactory::create(QStringLiteral("Breeze")));
            } else if (availableStyles.contains(QStringLiteral("fusion"), Qt::CaseInsensitive)) {
                KdenliveSettings::setWidgetstyle(QStringLiteral("Fusion"));
                QApplication::setStyle(QStyleFactory::create(QStringLiteral("Fusion")));
            }
        } else {
            KdenliveSettings::setWidgetstyle(QStringLiteral("Default"));
        }
    }

    // Add default style action
    QAction *defaultStyle = new QAction(i18n("Default"), stylesGroup);
    defaultStyle->setData(QStringLiteral("Default"));
    defaultStyle->setCheckable(true);
    stylesAction->addAction(defaultStyle);
    if (KdenliveSettings::widgetstyle() == QLatin1String("Default") || KdenliveSettings::widgetstyle().isEmpty()) {
        defaultStyle->setChecked(true);
    }

    for (const QString &style : qAsConst(availableStyles)) {
        auto *a = new QAction(style, stylesGroup);
        a->setCheckable(true);
        a->setData(style);
        if (KdenliveSettings::widgetstyle() == style) {
            a->setChecked(true);
        }
        stylesAction->addAction(a);
    }
    connect(stylesGroup, &QActionGroup::triggered, this, &MainWindow::slotChangeStyle);
    // QIcon::setThemeSearchPaths(QStringList() <<QStringLiteral(":/icons/"));
#ifdef NODBUS
    new RenderServer(this);
#else
    new RenderingAdaptor(this);
#endif
    QString defaultProfile = KdenliveSettings::default_profile();

    // Initialise MLT connection
    MltConnection::construct(mltPath);
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

    m_shortcutRemoveFocus = new QShortcut(QKeySequence(QStringLiteral("Esc")), this);
    connect(m_shortcutRemoveFocus, &QShortcut::activated, this, &MainWindow::slotRemoveFocus);

    /// Add Widgets
    setDockOptions(dockOptions() | QMainWindow::AllowNestedDocks | QMainWindow::AllowTabbedDocks);
    setDockOptions(dockOptions() | QMainWindow::GroupedDragging);
    setTabPosition(Qt::AllDockWidgetAreas, QTabWidget::TabPosition(KdenliveSettings::tabposition()));
    m_timelineToolBar = toolBar(QStringLiteral("timelineToolBar"));
    m_timelineToolBarContainer = new TimelineContainer(this);
    auto *ctnLay = new QVBoxLayout;
    ctnLay->setSpacing(0);
    ctnLay->setContentsMargins(0, 0, 0, 0);
    m_timelineToolBarContainer->setLayout(ctnLay);
    QFrame *topFrame = new QFrame(this);
    topFrame->setFrameShape(QFrame::HLine);
    topFrame->setFixedHeight(1);
    topFrame->setLineWidth(1);
    connect(this, &MainWindow::showTimelineFocus, this, [topFrame](bool focus, bool highlight) {
        if (focus) {
            KColorScheme scheme(QApplication::palette().currentColorGroup(), KColorScheme::Tooltip);
            if (highlight) {
                QColor col = scheme.decoration(KColorScheme::HoverColor).color();
                topFrame->setStyleSheet(QString("QFrame {border: 1px solid rgba(%1,%2,%3,70)}").arg(col.red()).arg(col.green()).arg(col.blue()));
            } else {
                QColor col = scheme.decoration(KColorScheme::FocusColor).color();
                topFrame->setStyleSheet(QString("QFrame {border: 1px solid rgba(%1,%2,%3,100)}").arg(col.red()).arg(col.green()).arg(col.blue()));
            }
        } else {
            topFrame->setStyleSheet(QString());
        }
    });
    ctnLay->addWidget(topFrame);
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
    pCore->bin()->setupMenu();
    pCore->buildDocks();

    QDockWidget *libraryDock = addDock(i18n("Library"), QStringLiteral("library"), pCore->library());
    QDockWidget *subtitlesDock = addDock(i18n("Subtitles"), QStringLiteral("Subtitles"), pCore->subtitleWidget());
    QDockWidget *textEditingDock = addDock(i18n("Speech Editor"), QStringLiteral("textedit"), pCore->textEditWidget());
    QDockWidget *timeRemapDock = addDock(i18n("Time Remapping"), QStringLiteral("timeremap"), pCore->timeRemapWidget());
    QDockWidget *guidesDock = addDock(i18n("Guides"), QStringLiteral("guides"), pCore->guidesList());
    connect(pCore.get(), &Core::remapClip, this, [&, timeRemapDock](int id) {
        if (id > -1) {
            timeRemapDock->show();
            timeRemapDock->raise();
        }
        pCore->timeRemapWidget()->selectedClip(id, pCore->currentTimelineId());
    });
    m_clipMonitor = new Monitor(Kdenlive::ClipMonitor, pCore->monitorManager(), this);
    pCore->bin()->setMonitor(m_clipMonitor);
    connect(m_clipMonitor, &Monitor::addMarker, this, &MainWindow::slotAddMarkerGuideQuickly);
    connect(m_clipMonitor, &Monitor::deleteMarker, this, &MainWindow::slotDeleteClipMarker);
    connect(m_clipMonitor, &Monitor::seekToPreviousSnap, this, &MainWindow::slotSnapRewind);
    connect(m_clipMonitor, &Monitor::seekToNextSnap, this, &MainWindow::slotSnapForward);
    connect(m_clipMonitor, &Monitor::passKeyPress, this, &MainWindow::triggerKey);

    m_projectMonitor = new Monitor(Kdenlive::ProjectMonitor, pCore->monitorManager(), this);
    connect(m_projectMonitor, &Monitor::passKeyPress, this, &MainWindow::triggerKey);
    connect(m_projectMonitor, &Monitor::addMarker, this, &MainWindow::slotAddMarkerGuideQuickly);
    connect(m_projectMonitor, &Monitor::deleteMarker, this, &MainWindow::slotDeleteGuide);
    connect(m_projectMonitor, &Monitor::seekToPreviousSnap, this, &MainWindow::slotSnapRewind);
    connect(m_projectMonitor, &Monitor::seekToNextSnap, this, &MainWindow::slotSnapForward);
    connect(m_loopClip, &QAction::triggered, this, [&]() {
        QPoint inOut = getCurrentTimeline()->controller()->selectionInOut();
        m_projectMonitor->slotLoopClip(inOut);
    });
    installEventFilter(this);
    pCore->monitorManager()->initMonitors(m_clipMonitor, m_projectMonitor);

    m_timelineTabs = new TimelineTabs(this);
    ctnLay->addWidget(m_timelineTabs);
    setCentralWidget(m_timelineToolBarContainer);

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
    QAction *recAction = m_clipMonitor->recAction();
    addAction(QStringLiteral("screengrab_record"), recAction);
    recToolbar->addAction(recAction);
    QAction *recConfig = new QAction(QIcon::fromTheme(QStringLiteral("configure")), i18n("Configure Recording"), this);
    recToolbar->addAction(recConfig);
    connect(recConfig, &QAction::triggered, [&]() { Q_EMIT pCore->showConfigDialog(Kdenlive::PageCapture, 0); });
    QDockWidget *screenGrabDock = addDock(i18n("Screen Grab"), QStringLiteral("screengrab"), grabWidget);

    // Sequence actions
    QAction *nextSequence = new QAction(QIcon::fromTheme(QStringLiteral("go-next")), i18n("Switch to next Sequence"), this);
    addAction(QStringLiteral("sequence_next"), nextSequence, QKeySequence::NextChild);
    connect(nextSequence, &QAction::triggered, m_timelineTabs, &TimelineTabs::slotNextSequence);

    QAction *prevSequence = new QAction(QIcon::fromTheme(QStringLiteral("go-previous")), i18n("Switch to previous Sequence"), this);
    addAction(QStringLiteral("sequence_previous"), prevSequence, QKeySequence::PreviousChild);
    connect(prevSequence, &QAction::triggered, m_timelineTabs, &TimelineTabs::slotPreviousSequence);

    // Audio spectrum scope
    m_audioSpectrum = new AudioGraphSpectrum(pCore->monitorManager());
    QDockWidget *spectrumDock = addDock(i18n("Audio Spectrum"), QStringLiteral("audiospectrum"), m_audioSpectrum);
    connect(spectrumDock, &QDockWidget::visibilityChanged, this, [&](bool visible) { m_audioSpectrum->dockVisible(visible); });

    // Project bin
    m_projectBinDock = addDock(i18n("Project Bin"), QStringLiteral("project_bin"), pCore->bin());

    // Media browser widget
    QDockWidget *clipDockWidget = addDock(i18n("Media Browser"), QStringLiteral("bin_clip"), pCore->mediaBrowser());

    // Online resources widget
    auto *onlineResources = new ResourceWidget(this);
    m_onlineResourcesDock = addDock(i18n("Online Resources"), QStringLiteral("onlineresources"), onlineResources);
    connect(onlineResources, &ResourceWidget::previewClip, this, [&](const QString &path, const QString &title) {
        m_clipMonitor->slotPreviewResource(path, title);
        m_clipMonitorDock->show();
        m_clipMonitorDock->raise();
    });

    connect(onlineResources, &ResourceWidget::addClip, this, &MainWindow::slotAddProjectClip);
    connect(onlineResources, &ResourceWidget::addLicenseInfo, this, &MainWindow::slotAddTextNote);

    // Close library and audiospectrum and others on first run
    screenGrabDock->close();
    libraryDock->close();
    subtitlesDock->close();
    textEditingDock->close();
    timeRemapDock->close();
    spectrumDock->close();
    clipDockWidget->close();
    guidesDock->close();
    m_onlineResourcesDock->close();

    m_effectStackDock = addDock(i18n("Effect/Composition Stack"), QStringLiteral("effect_stack"), m_assetPanel);
    connect(m_assetPanel, &AssetPanel::doSplitEffect, m_projectMonitor, &Monitor::slotSwitchCompare);
    connect(m_assetPanel, &AssetPanel::doSplitBinEffect, m_clipMonitor, &Monitor::slotSwitchCompare);
    connect(m_assetPanel, &AssetPanel::switchCurrentComposition, this,
            [&](int cid, const QString &compositionId) { getCurrentTimeline()->model()->switchComposition(cid, compositionId); });
    connect(pCore->bin(), &Bin::updateTabName, m_timelineTabs, &TimelineTabs::renameTab);
    connect(m_timelineTabs, &TimelineTabs::showMixModel, m_assetPanel, &AssetPanel::showMix);
    connect(m_timelineTabs, &TimelineTabs::showTransitionModel, this, [&](int tid, std::shared_ptr<AssetParameterModel> model) {
        m_assetPanel->showTransition(tid, model);
        m_effectStackDock->raise();
    });
    connect(m_timelineTabs, &TimelineTabs::showItemEffectStack, this,
            [&](const QString &clipName, std::shared_ptr<EffectStackModel> model, QSize size, bool showKeyframes) {
                if (model == nullptr && m_assetPanel->effectStackOwner().type == KdenliveObjectType::BinClip) {
                    // Effect stask is currently displaying a bin clip, do nothing
                    return;
                }
                m_assetPanel->showEffectStack(clipName, model, size, showKeyframes);
                m_effectStackDock->raise();
            });

    connect(m_timelineTabs, &TimelineTabs::updateAssetPosition, m_assetPanel, &AssetPanel::updateAssetPosition);

    connect(m_timelineTabs, &TimelineTabs::showSubtitle, this, [&, subtitlesDock](int id) {
        if (id > -1) {
            subtitlesDock->show();
            subtitlesDock->raise();
        }
        pCore->subtitleWidget()->setActiveSubtitle(id);
    });

    connect(m_timelineTabs, &TimelineTabs::updateZoom, this, &MainWindow::updateZoomSlider);
    connect(pCore->bin(), &Bin::requestShowEffectStack, [&]() {
        // Don't raise effect stack on clip bin in case it is docked with bin or clip monitor
        // m_effectStackDock->raise();
    });
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

    m_effectList2 = new EffectListWidget(this);
    connect(m_effectList2, &EffectListWidget::activateAsset, pCore->projectManager(), &ProjectManager::activateAsset);
    connect(m_assetPanel, &AssetPanel::reloadEffect, m_effectList2, &EffectListWidget::reloadCustomEffect);
    m_effectListDock = addDock(i18n("Effects"), QStringLiteral("effect_list"), m_effectList2);

    m_compositionList = new TransitionListWidget(this);
    m_compositionListDock = addDock(i18n("Compositions"), QStringLiteral("transition_list"), m_compositionList);

    // Add monitors here to keep them at the right of the window
    m_clipMonitorDock = addDock(i18n("Clip Monitor"), QStringLiteral("clip_monitor"), m_clipMonitor);
    m_projectMonitorDock = addDock(i18n("Project Monitor"), QStringLiteral("project_monitor"), m_projectMonitor);

    m_undoView = new QUndoView();
    m_undoView->setCleanIcon(QIcon::fromTheme(QStringLiteral("edit-clear")));
    m_undoView->setEmptyLabel(i18n("Clean"));
    m_undoView->setGroup(m_commandStack);
    m_undoViewDock = addDock(i18n("Undo History"), QStringLiteral("undo_history"), m_undoView);

    // Color and icon theme stuff
    connect(m_commandStack, &QUndoGroup::cleanChanged, m_saveAction, &QAction::setDisabled);
    addAction(QStringLiteral("styles_menu"), stylesAction);

    QAction *iconAction = new QAction(i18n("Force Breeze Icon Theme"), this);
    iconAction->setCheckable(true);
    iconAction->setChecked(KdenliveSettings::force_breeze());
    addAction(QStringLiteral("force_icon_theme"), iconAction);
    connect(iconAction, &QAction::triggered, this, &MainWindow::forceIconSet);

    m_mixerDock = addDock(i18n("Audio Mixer"), QStringLiteral("mixer"), pCore->mixer());
    m_mixerDock->setWhatsThis(xi18nc("@info:whatsthis", "Toggles the audio mixer panel/widget."));
    QAction *showMixer = new QAction(QIcon::fromTheme(QStringLiteral("view-media-equalizer")), i18n("Audio Mixer"), this);
    showMixer->setCheckable(true);
    addAction(QStringLiteral("audiomixer_button"), showMixer);
    connect(m_mixerDock, &QDockWidget::visibilityChanged, this, [&, showMixer](bool visible) {
        pCore->mixer()->connectMixer(visible);
        pCore->audioMixerVisible = visible;
        m_projectMonitor->displayAudioMonitor(m_projectMonitor->isActive());
        showMixer->setChecked(visible);
    });
    connect(showMixer, &QAction::triggered, this, [&]() {
        if (m_mixerDock->isVisible() && !m_mixerDock->visibleRegion().isEmpty()) {
            m_mixerDock->close();
        } else {
            m_mixerDock->show();
            m_mixerDock->raise();
        }
    });

    // Close non-general docks for the initial layout
    // only show important ones
    m_undoViewDock->close();
    m_mixerDock->close();

    // Tabify Widgets
    tabifyDockWidget(m_clipMonitorDock, m_projectMonitorDock);
    tabifyDockWidget(m_compositionListDock, m_effectListDock);
    tabifyDockWidget(m_effectStackDock, pCore->bin()->clipPropertiesDock());
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

    auto *scmanager = new ScopeManager(this);

    auto *titleBars = new DockTitleBarManager(this);
    connect(layoutManager, &LayoutManagement::updateTitleBars, titleBars, [&]() { titleBars->slotUpdateTitleBars(); });
    connect(layoutManager, &LayoutManagement::connectDocks, titleBars, &DockTitleBarManager::connectDocks);
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

    auto *toolButtonAction = new QWidgetAction(this);
    toolButtonAction->setText(i18n("Favorite Effects"));
    toolButtonAction->setIcon(QIcon::fromTheme(QStringLiteral("favorite")));
    toolButtonAction->setDefaultWidget(basketButton);
    addAction(QStringLiteral("favorite_effects"), toolButtonAction);
    connect(toolButtonAction, &QAction::triggered, basketButton, &QToolButton::showMenu);
    connect(m_effectBasket, &EffectBasket::activateAsset, menu, &QMenu::close);

    // Render button
    ProgressButton *timelineRender = new ProgressButton(i18n("Render…"), 100, this);
    auto *tlrMenu = new QMenu(this);
    timelineRender->setMenu(tlrMenu);
    connect(this, &MainWindow::setRenderProgress, timelineRender, &ProgressButton::setProgress);
    auto *renderButtonAction = new QWidgetAction(this);
    renderButtonAction->setText(i18n("Render Button"));
    renderButtonAction->setIcon(QIcon::fromTheme(QStringLiteral("media-record")));
    renderButtonAction->setDefaultWidget(timelineRender);
    addAction(QStringLiteral("project_render_button"), renderButtonAction);

    // Timeline preview button
    ProgressButton *timelinePreview = new ProgressButton(i18n("Rendering preview"), 1000, this);
    auto *tlMenu = new QMenu(this);
    timelinePreview->setMenu(tlMenu);
    connect(this, &MainWindow::setPreviewProgress, timelinePreview, &ProgressButton::setProgress);
    auto *previewButtonAction = new QWidgetAction(this);
    previewButtonAction->setText(i18n("Timeline Preview"));
    previewButtonAction->setIcon(QIcon::fromTheme(QStringLiteral("preview-render-on")));
    previewButtonAction->setDefaultWidget(timelinePreview);
    addAction(QStringLiteral("timeline_preview_button"), previewButtonAction);
    setupGUI(KXmlGuiWindow::ToolBar | KXmlGuiWindow::StatusBar | KXmlGuiWindow::Save | KXmlGuiWindow::Create);

    LocaleHandling::resetLocale();
    if (firstRun) {
        if (QScreen *current = QApplication::primaryScreen()) {
            int screenHeight = current->availableSize().height();
            if (screenHeight < 1000) {
                resize(current->availableSize());
            } else if (screenHeight < 2000) {
                resize(current->availableSize() / 1.2);
            } else {
                resize(current->availableSize() / 1.6);
            }
        }
    }

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

    // Timeline composition menu
    auto *compositionMenu = new QMenu(this);
    compositionMenu->addAction(actionCollection()->action(QStringLiteral("edit_item_duration")));
    compositionMenu->addAction(actionCollection()->action(QStringLiteral("edit_copy")));
    compositionMenu->addAction(actionCollection()->action(QStringLiteral("delete_timeline_clip")));

    // Timeline main menu
    auto *timelineMenu = new QMenu(this);
    timelineMenu->addAction(actionCollection()->action(QStringLiteral("edit_paste")));
    timelineMenu->addAction(actionCollection()->action(QStringLiteral("insert_space")));
    timelineMenu->addAction(actionCollection()->action(QStringLiteral("delete_space")));
    timelineMenu->addAction(actionCollection()->action(QStringLiteral("delete_space_all_tracks")));
    timelineMenu->addAction(actionCollection()->action(QStringLiteral("add_guide")));
    timelineMenu->addAction(actionCollection()->action(QStringLiteral("edit_guide")));
    QMenu *guideMenu = new QMenu(i18n("Go to Guide…"), this);
    timelineMenu->addMenu(guideMenu);

    // Timeline ruler menu
    auto *timelineRulerMenu = new QMenu(this);
    timelineRulerMenu->addAction(actionCollection()->action(QStringLiteral("add_guide")));
    timelineRulerMenu->addAction(actionCollection()->action(QStringLiteral("edit_guide")));
    timelineRulerMenu->addAction(actionCollection()->action(QStringLiteral("lock_guides")));
    timelineRulerMenu->addAction(actionCollection()->action(QStringLiteral("export_guides")));
    timelineRulerMenu->addMenu(guideMenu);
    timelineRulerMenu->addAction(actionCollection()->action(QStringLiteral("mark_in")));
    timelineRulerMenu->addAction(actionCollection()->action(QStringLiteral("mark_out")));
    timelineRulerMenu->addAction(actionCollection()->action(QStringLiteral("select_timeline_zone")));
    timelineRulerMenu->addAction(actionCollection()->action(QStringLiteral("add_project_note")));
    timelineRulerMenu->addAction(actionCollection()->action(QStringLiteral("add_subtitle")));

    // Timeline subtitle menu
    auto *timelineSubtitleMenu = new QMenu(this);
    timelineSubtitleMenu->addAction(actionCollection()->action(QStringLiteral("edit_copy")));
    timelineSubtitleMenu->addAction(actionCollection()->action(QStringLiteral("delete_subtitle_clip")));

    // Timeline headers menu
    auto *timelineHeadersMenu = new QMenu(this);
    timelineHeadersMenu->addAction(actionCollection()->action(QStringLiteral("insert_track")));
    timelineHeadersMenu->addAction(actionCollection()->action(QStringLiteral("delete_track")));
    timelineHeadersMenu->addAction(actionCollection()->action(QStringLiteral("fit_all_tracks")));
    timelineHeadersMenu->addAction(actionCollection()->action(QStringLiteral("show_track_record")));

    QAction *separate_channels = new QAction(QIcon(), i18n("Separate Channels"), this);
    separate_channels->setCheckable(true);
    separate_channels->setChecked(KdenliveSettings::displayallchannels());
    separate_channels->setData("separate_channels");
    connect(separate_channels, &QAction::triggered, this, &MainWindow::slotSeparateAudioChannel);
    timelineHeadersMenu->addAction(separate_channels);

    QAction *normalize_channels = new QAction(QIcon(), i18n("Normalize Audio Thumbnails"), this);
    normalize_channels->setCheckable(true);
    normalize_channels->setChecked(KdenliveSettings::normalizechannels());
    normalize_channels->setData("normalize_channels");
    connect(normalize_channels, &QAction::triggered, this, &MainWindow::slotNormalizeAudioChannel);
    timelineHeadersMenu->addAction(normalize_channels);

    QMenu *thumbsMenu = new QMenu(i18n("Thumbnails"), this);
    auto *thumbGroup = new QActionGroup(this);
    QAction *inFrame = new QAction(i18n("In Frame"), thumbGroup);
    inFrame->setData(QStringLiteral("2"));
    inFrame->setCheckable(true);
    thumbsMenu->addAction(inFrame);
    QAction *inOutFrame = new QAction(i18n("In/Out Frames"), thumbGroup);
    inOutFrame->setData(QStringLiteral("0"));
    inOutFrame->setCheckable(true);
    thumbsMenu->addAction(inOutFrame);
    QAction *allFrame = new QAction(i18n("All Frames"), thumbGroup);
    allFrame->setData(QStringLiteral("1"));
    allFrame->setCheckable(true);
    thumbsMenu->addAction(allFrame);
    QAction *noFrame = new QAction(i18n("No Thumbnails"), thumbGroup);
    noFrame->setData(QStringLiteral("3"));
    noFrame->setCheckable(true);
    thumbsMenu->addAction(noFrame);
    pCore->bin()->setupGeneratorMenu();

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

    // Automatic timeline preview action
    QAction *proxyRender = new QAction(i18n("Preview Using Proxy Clips"), this);
    proxyRender->setCheckable(true);
    proxyRender->setChecked(KdenliveSettings::proxypreview());
    connect(proxyRender, &QAction::triggered, this, [&](bool checked) { KdenliveSettings::setProxypreview(checked); });
    tlMenu->addAction(proxyRender);

    // Automatic timeline preview action
    QAction *autoRender = new QAction(QIcon::fromTheme(QStringLiteral("view-refresh")), i18n("Automatic Preview"), this);
    autoRender->setCheckable(true);
    autoRender->setChecked(KdenliveSettings::autopreview());
    connect(autoRender, &QAction::triggered, this, &MainWindow::slotToggleAutoPreview);
    tlMenu->addAction(autoRender);
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
    /*KConfig conf(QStringLiteral("encodingprofiles.rc"), KConfig::CascadeConfig, QStandardPaths::AppDataLocation);
    if (KdenliveSettings::proxyparams().isEmpty() || KdenliveSettings::proxyextension().isEmpty()) {
        KConfigGroup group(&conf, "proxy");
        QMap<QString, QString> values = group.entryMap();
        QMapIterator<QString, QString> i(values);
        if (i.hasNext()) {
            i.next();
            QString proxystring = i.value();
            KdenliveSettings::setProxyparams(proxystring.section(QLatin1Char(';'), 0, 0));
            KdenliveSettings::setProxyextension(proxystring.section(QLatin1Char(';'), 1, 1));
        }
    }*/
    if (KdenliveSettings::v4l_parameters().isEmpty() || KdenliveSettings::v4l_extension().isEmpty()) {
        KConfigGroup group(&conf, "video4linux");
        QMap<QString, QString> values = group.entryMap();
        QMapIterator<QString, QString> i(values);
        if (i.hasNext()) {
            i.next();
            QString v4lstring = i.value();
            KdenliveSettings::setV4l_parameters(v4lstring.section(QLatin1Char(';'), 0, 0));
            KdenliveSettings::setV4l_extension(v4lstring.section(QLatin1Char(';'), 1, 1));
        }
    }
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

    if (firstRun) {
        // Load editing layout
        layoutManager->loadLayout(QStringLiteral("kdenlive_editing"), true);
    }

#ifdef USE_JOGSHUTTLE
    new JogManager(this);
#endif
    m_timelineTabs->setTimelineMenu(compositionMenu, timelineMenu, guideMenu, timelineRulerMenu, actionCollection()->action(QStringLiteral("edit_guide")),
                                    timelineHeadersMenu, thumbsMenu, timelineSubtitleMenu);
    scmanager->slotCheckActiveScopes();
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

    m_hamburgerMenu = KStandardAction::hamburgerMenu(nullptr, nullptr, actionCollection());
    // after the QMenuBar has been initialised
    m_hamburgerMenu->setMenuBar(menuBar());
    m_hamburgerMenu->setShowMenuBarAction(showMenuBarAction);

    // Detect shortcut conflicts bewtween mainwindow and media browser
    pCore->mediaBrowser()->detectShortcutConflicts();

    connect(toolBar(), &KToolBar::visibilityChanged, this, [&, showMenuBarAction](bool visible) {
        if (visible && !toolBar()->actions().contains(m_hamburgerMenu)) {
            // hack to be able to insert the hamburger menu at the first position
            QAction *const firstChild = toolBar()->actionAt(toolBar()->height() / 2, toolBar()->height() / 2);
            QAction *const seperator = toolBar()->insertSeparator(firstChild);
            toolBar()->insertAction(seperator, m_hamburgerMenu);
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

void MainWindow::loadContainerActions()
{
    // Build all actions using the static_cast<QMenu *>(factory()->container method
    // That need to be rebuild when updating the ui.rc file

    // Redirect help entry to our own function
    // First delete the default help action
    QAction *officialHelp = actionCollection()->action(KStandardAction::name(KStandardAction::HelpContents));
    actionCollection()->removeAction(officialHelp);
    // Now recreate our own
    KStandardAction::helpContents(this, &MainWindow::appHelpActivated, actionCollection());
    officialHelp = actionCollection()->action(KStandardAction::name(KStandardAction::HelpContents));
    // Replug it in the Help menu
    QMenu *helpMenu = static_cast<QMenu *>(factory()->container(QStringLiteral("help"), this));
    if (helpMenu) {
        QAction *whatsThis = actionCollection()->action(KStandardAction::name(KStandardAction::WhatsThis));
        helpMenu->insertAction(whatsThis, officialHelp);
    }

    // rebuild timeline clip menu
    m_timelineTabs->buildClipMenu();

    // Ensure the "Monitor Config" menu doesn't replace the settings dialog on Mac because of text heuristics
    auto *monitorConfig = qobject_cast<QMenu *>(factory()->container(QStringLiteral("monitor_config"), this));
    if (monitorConfig) {
        monitorConfig->menuAction()->setMenuRole(QAction::NoRole);
    }

    // Connect monitor overlay info menu.
    QMenu *monitorOverlay = static_cast<QMenu *>(factory()->container(QStringLiteral("monitor_config_overlay"), this));
    if (monitorOverlay) {
        connect(monitorOverlay, &QMenu::triggered, this, &MainWindow::slotSwitchMonitorOverlay);

        m_projectMonitor->setupMenu(static_cast<QMenu *>(factory()->container(QStringLiteral("monitor_go"), this)), monitorOverlay, m_playZone, m_loopZone,
                                    nullptr, m_loopClip);
        m_clipMonitor->setupMenu(static_cast<QMenu *>(factory()->container(QStringLiteral("monitor_go"), this)), monitorOverlay, m_playZone, m_loopZone,
                                 static_cast<QMenu *>(factory()->container(QStringLiteral("marker_menu"), this)), nullptr);
    }

    QMenu *clipInTimeline = static_cast<QMenu *>(factory()->container(QStringLiteral("clip_in_timeline"), this));
    clipInTimeline->setIcon(QIcon::fromTheme(QStringLiteral("go-jump")));

    QMenu *m = static_cast<QMenu *>(factory()->container(QStringLiteral("video_effects_menu"), this));
    connect(m, &QMenu::triggered, this, &MainWindow::slotAddEffect);

    QMenu *addMenu = static_cast<QMenu *>(factory()->container(QStringLiteral("generators"), this));
    Generators::getGenerators(KdenliveSettings::producerslist(), addMenu);
    connect(addMenu, &QMenu::triggered, this, &MainWindow::buildGenerator);
}

void MainWindow::slotThemeChanged(const QString &name)
{
    KSharedConfigPtr config = KSharedConfig::openConfig(name);
    QPalette plt = KColorScheme::createApplicationPalette(config);
    // qApp->setPalette(plt);
    // Required for qml palette change
    QGuiApplication::setPalette(plt);

    QColor background = plt.window().color();
    bool useDarkIcons = background.value() < 100;

    if (m_assetPanel) {
        m_assetPanel->updatePalette();
    }
    if (m_clipMonitor) {
        m_clipMonitor->setPalette(plt);
    }
    if (m_projectMonitor) {
        m_projectMonitor->setPalette(plt);
    }
    if (m_timelineTabs) {
        m_timelineTabs->setPalette(plt);
        getCurrentTimeline()->controller()->resetView();
    }
    if (m_audioSpectrum) {
        m_audioSpectrum->refreshPixmap();
    }
    Q_EMIT pCore->updatePalette();

    KSharedConfigPtr kconfig = KSharedConfig::openConfig();
    KConfigGroup initialGroup(kconfig, "version");
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    bool isAppimage = pCore->packageType() == QStringLiteral("appimage");
    bool isKDE = env.value(QStringLiteral("XDG_CURRENT_DESKTOP")).toLower() == QLatin1String("kde");
    bool forceBreeze = initialGroup.exists() && KdenliveSettings::force_breeze();
    if ((!isKDE || isAppimage || forceBreeze) &&
        ((useDarkIcons && QIcon::themeName() == QStringLiteral("breeze")) || (!useDarkIcons && QIcon::themeName() == QStringLiteral("breeze-dark")))) {
        // We need to reload icon theme, on KDE desktops this is not necessary, however for the Appimage it is even on KDE Desktop
        // See also https://kate-editor.org/post/2021/2021-03-07-cross-platform-light-dark-themes-and-icons/
        QIcon::setThemeName(useDarkIcons ? QStringLiteral("breeze-dark") : QStringLiteral("breeze"));
        KdenliveSettings::setUse_dark_breeze(useDarkIcons);
    }
}

MainWindow::~MainWindow()
{
    pCore->prepareShutdown();
    delete m_timelineTabs;
    delete m_audioSpectrum;
    if (m_projectMonitor) {
        m_projectMonitor->stop();
    }
    if (m_clipMonitor) {
        m_clipMonitor->stop();
    }
    ClipController::mediaUnavailable.reset();
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
        if (waitingJobs > 0) {
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
    saveOptions();

    // WARNING: According to KMainWindow::queryClose documentation we are not supposed to close the document here?
    return pCore->projectManager()->closeCurrentDocument(true, true);
}

void MainWindow::buildGenerator(QAction *action)
{
    Generators gen(action->data().toString(), this);
    if (gen.exec() == QDialog::Accepted) {
        pCore->bin()->slotAddClipToProject(gen.getSavedClip());
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
    KXmlGuiWindow::saveNewToolbarConfig();
    // TODO for some reason all dynamically inserted actions are removed by the save toolbar
    // So we currently re-add them manually....
    loadDockActions();
    loadClipActions();
    loadContainerActions();
    for (auto &bin : m_binWidgets) {
        bin->setupGeneratorMenu();
    }

    // hack to be able to insert the hamburger menu at the first position
    QAction *const firstChild = toolBar()->actionAt(toolBar()->height() / 2, toolBar()->height() / 2);
    QAction *const seperator = toolBar()->insertSeparator(firstChild);
    toolBar()->insertAction(seperator, m_hamburgerMenu);
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
    connect(m_clipMonitor, &Monitor::refreshClipThumbnail, pCore->bin(), &Bin::slotRefreshClipThumbnail);
    connect(m_projectMonitor, &Monitor::requestFrameForAnalysis, this, &MainWindow::slotMonitorRequestRenderFrame);
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

QAction *MainWindow::addAction(const QString &name, const QString &text, const QObject *receiver, const char *member, const QIcon &icon,
                               const QKeySequence &shortcut, KActionCategory *category)
{
    auto *action = new QAction(text, this);
    if (!icon.isNull()) {
        action->setIcon(icon);
    }
    addAction(name, action, shortcut, category);
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
    // create edit mode buttons
    m_normalEditTool = new QAction(QIcon::fromTheme(QStringLiteral("kdenlive-normal-edit")), i18n("Normal Mode"), this);
    m_normalEditTool->setCheckable(true);
    m_normalEditTool->setChecked(true);

    m_overwriteEditTool = new QAction(QIcon::fromTheme(QStringLiteral("kdenlive-overwrite-edit")), i18n("Overwrite Mode"), this);
    m_overwriteEditTool->setCheckable(true);
    m_overwriteEditTool->setChecked(false);

    m_insertEditTool = new QAction(QIcon::fromTheme(QStringLiteral("kdenlive-insert-edit")), i18n("Insert Mode"), this);
    m_insertEditTool->setCheckable(true);
    m_insertEditTool->setChecked(false);

    KSelectAction *sceneMode = new KSelectAction(i18n("Timeline Edit Mode"), this);
    sceneMode->setWhatsThis(
        xi18nc("@info:whatsthis", "Switches between Normal, Overwrite and Insert Mode. Determines the default action when handling clips in the timeline."));
    sceneMode->addAction(m_normalEditTool);
    sceneMode->addAction(m_overwriteEditTool);
    sceneMode->addAction(m_insertEditTool);
    sceneMode->setCurrentItem(0);
#if KWIDGETSADDONS_VERSION >= QT_VERSION_CHECK(5, 240, 0)
    connect(sceneMode, &KSelectAction::actionTriggered, this, &MainWindow::slotChangeEdit);
#else
    connect(sceneMode, static_cast<void (KSelectAction::*)(QAction *)>(&KSelectAction::triggered), this, &MainWindow::slotChangeEdit);
#endif
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

    auto tlsettings = new QMenu(this);
    tlsettings->setIcon(QIcon::fromTheme(QStringLiteral("configure")));
    tlsettings->addAction(m_compositeAction);
    tlsettings->addSeparator();
    tlsettings->addAction(mixedView);
    tlsettings->addAction(splitView);
    tlsettings->addAction(splitView2);

    auto *timelineSett = new QToolButton(this);
    timelineSett->setPopupMode(QToolButton::InstantPopup);
    timelineSett->setMenu(tlsettings);
    timelineSett->setIcon(QIcon::fromTheme(QStringLiteral("configure")));
    auto *tlButtonAction = new QWidgetAction(this);
    tlButtonAction->setDefaultWidget(timelineSett);
    tlButtonAction->setText(i18n("Track menu"));
    addAction(QStringLiteral("timeline_settings"), tlButtonAction);

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
    // TODO icon available (and properly working) in KF 5.86
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
    // TODO icon available (and properly working) in KF 5.86
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

    QAction *sameTrack = new QAction(QIcon::fromTheme(QStringLiteral("composite-track-preview")), i18n("Mix Clips"), this);
    sameTrack->setWhatsThis(
        xi18nc("@info:whatsthis", "Creates a same-track transition between the selected clip and the adjacent one closest to the playhead."));
    addAction(QStringLiteral("mix_clip"), sameTrack, Qt::Key_U);
    connect(sameTrack, &QAction::triggered, this, [this]() { getCurrentTimeline()->controller()->mixClip(); });

    // toolbar->setToolButtonStyle(Qt::ToolButtonIconOnly);

    connect(toolGroup, &QActionGroup::triggered, this, &MainWindow::slotChangeTool);

    m_buttonVideoThumbs = new QAction(QIcon::fromTheme(QStringLiteral("kdenlive-show-videothumb")), i18n("Show Video Thumbnails"), this);
    m_buttonVideoThumbs->setWhatsThis(xi18nc("@info:whatsthis", "Toggles the display of video thumbnails for the clips in the timeline (default is On)."));

    m_buttonVideoThumbs->setCheckable(true);
    m_buttonVideoThumbs->setChecked(KdenliveSettings::videothumbnails());
    connect(m_buttonVideoThumbs, &QAction::triggered, this, &MainWindow::slotSwitchVideoThumbs);

    m_buttonAudioThumbs = new QAction(QIcon::fromTheme(QStringLiteral("kdenlive-show-audiothumb")), i18n("Show Audio Thumbnails"), this);
    m_buttonAudioThumbs->setWhatsThis(xi18nc("@info:whatsthis", "Toggles the display of audio thumbnails for the clips in the timeline (default is On)."));

    m_buttonAudioThumbs->setCheckable(true);
    m_buttonAudioThumbs->setChecked(KdenliveSettings::audiothumbnails());
    connect(m_buttonAudioThumbs, &QAction::triggered, this, &MainWindow::slotSwitchAudioThumbs);

    m_buttonShowMarkers = new QAction(QIcon::fromTheme(QStringLiteral("kdenlive-show-markers")), i18n("Show Markers Comments"), this);

    m_buttonShowMarkers->setCheckable(true);
    m_buttonShowMarkers->setChecked(KdenliveSettings::showmarkers());
    connect(m_buttonShowMarkers, &QAction::triggered, this, &MainWindow::slotSwitchMarkersComments);

    m_buttonSnap = new QAction(QIcon::fromTheme(QStringLiteral("snap")), i18n("Snap"), this);
    m_buttonSnap->setWhatsThis(xi18nc("@info:whatsthis", "Toggles the snap function (clips snap to playhead, edges, markers, guides and others)."));

    m_buttonSnap->setCheckable(true);
    m_buttonSnap->setChecked(KdenliveSettings::snaptopoints());
    connect(m_buttonSnap, &QAction::triggered, this, &MainWindow::slotSwitchSnap);

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
    toolbar->addAction(m_buttonTimelineTags);
    toolbar->addAction(m_buttonVideoThumbs);
    toolbar->addAction(m_buttonAudioThumbs);
    toolbar->addAction(m_buttonShowMarkers);
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

    addAction(QStringLiteral("project_clean"), i18n("Clean Project"), this, SLOT(slotCleanProject()), QIcon::fromTheme(QStringLiteral("edit-clear")));

    QAction *resetAction = new QAction(QIcon::fromTheme(QStringLiteral("view-refresh")), i18n("Reset Configuration…"), this);
    addAction(QStringLiteral("reset_config"), resetAction);
    connect(resetAction, &QAction::triggered, this, [&]() { slotRestart(true); });

    m_playZone = addAction(QStringLiteral("monitor_play_zone"), i18n("Play Zone"), pCore->monitorManager(), SLOT(slotPlayZone()),
                           QIcon::fromTheme(QStringLiteral("media-playback-start")), Qt::CTRL | Qt::Key_Space, QStringLiteral("navandplayback"));
    m_loopZone = addAction(QStringLiteral("monitor_loop_zone"), i18n("Loop Zone"), pCore->monitorManager(), SLOT(slotLoopZone()),
                           QIcon::fromTheme(QStringLiteral("media-playback-start")), Qt::CTRL | Qt::SHIFT | Qt::Key_Space, QStringLiteral("navandplayback"));
    m_loopClip = new QAction(QIcon::fromTheme(QStringLiteral("media-playback-start")), i18n("Loop Selected Clip"), this);
    addAction(QStringLiteral("monitor_loop_clip"), m_loopClip);
    m_loopClip->setEnabled(false);

    addAction(QStringLiteral("transcode_clip"), i18n("Transcode Clips…"), this, SLOT(slotTranscodeClip()), QIcon::fromTheme(QStringLiteral("edit-copy")));
    QAction *exportAction = new QAction(QIcon::fromTheme(QStringLiteral("document-export")), i18n("OpenTimelineIO E&xport…"), this);
    connect(exportAction, &QAction::triggered, &m_otioConvertions, &OtioConvertions::slotExportProject);
    addAction(QStringLiteral("export_project"), exportAction);
    QAction *importAction = new QAction(QIcon::fromTheme(QStringLiteral("document-import")), i18n("OpenTimelineIO &Import…"), this);
    connect(importAction, &QAction::triggered, &m_otioConvertions, &OtioConvertions::slotImportProject);
    addAction(QStringLiteral("import_project"), importAction);

    addAction(QStringLiteral("archive_project"), i18n("Archive Project…"), this, SLOT(slotArchiveProject()),
              QIcon::fromTheme(QStringLiteral("document-save-all")));
    addAction(QStringLiteral("switch_monitor"), i18n("Switch Monitor"), this, SLOT(slotSwitchMonitors()), QIcon(), Qt::Key_T);
    addAction(QStringLiteral("focus_timecode"), i18n("Focus Timecode"), this, SLOT(slotFocusTimecode()), QIcon(), Qt::Key_Equal);
    addAction(QStringLiteral("expand_timeline_clip"), i18n("Expand Clip"), this, SLOT(slotExpandClip()), QIcon::fromTheme(QStringLiteral("document-open")));

    QAction *overlayInfo = new QAction(QIcon::fromTheme(QStringLiteral("help-hint")), i18n("Monitor Info Overlay"), this);
    addAction(QStringLiteral("monitor_overlay"), overlayInfo, {}, QStringLiteral("monitor"));
    overlayInfo->setCheckable(true);
    overlayInfo->setData(0x01);

    QAction *overlayTCInfo = new QAction(QIcon::fromTheme(QStringLiteral("help-hint")), i18n("Monitor Overlay Timecode"), this);
    addAction(QStringLiteral("monitor_overlay_tc"), overlayTCInfo, {}, QStringLiteral("monitor"));
    overlayTCInfo->setCheckable(true);
    overlayTCInfo->setData(0x02);

    QAction *overlayFpsInfo = new QAction(QIcon::fromTheme(QStringLiteral("help-hint")), i18n("Monitor Overlay Playback Fps"), this);
    addAction(QStringLiteral("monitor_overlay_fps"), overlayFpsInfo, {}, QStringLiteral("monitor"));
    overlayFpsInfo->setCheckable(true);
    overlayFpsInfo->setData(0x20);

    QAction *overlayMarkerInfo = new QAction(QIcon::fromTheme(QStringLiteral("help-hint")), i18n("Monitor Overlay Markers"), this);
    addAction(QStringLiteral("monitor_overlay_markers"), overlayMarkerInfo, {}, QStringLiteral("monitor"));
    overlayMarkerInfo->setCheckable(true);
    overlayMarkerInfo->setData(0x04);

    QAction *overlayAudioInfo = new QAction(QIcon::fromTheme(QStringLiteral("help-hint")), i18n("Monitor Overlay Audio Waveform"), this);
    addAction(QStringLiteral("monitor_overlay_audiothumb"), overlayAudioInfo, {}, QStringLiteral("monitor"));
    overlayAudioInfo->setCheckable(true);
    overlayAudioInfo->setData(0x10);

    QAction *overlayClipJobs = new QAction(QIcon::fromTheme(QStringLiteral("help-hint")), i18n("Monitor Overlay Clip Jobs"), this);
    addAction(QStringLiteral("monitor_overlay_clipjobs"), overlayClipJobs, {}, QStringLiteral("monitor"));
    overlayClipJobs->setCheckable(true);
    overlayClipJobs->setData(0x40);

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
    scale_no->setData(1);
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
    connect(pCore->monitorManager(), &MonitorManager::scalingChanged, this, [scale_2, scale_4, scale_8, scale_16, scale_no]() {
        switch (KdenliveSettings::previewScaling()) {
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

    KSelectAction *monitorGamma = new KSelectAction(i18n("Monitor Gamma"), this);
    monitorGamma->addAction(i18n("sRGB (computer)"));
    monitorGamma->addAction(i18n("Rec. 709 (TV)"));
    addAction(QStringLiteral("mlt_gamma"), monitorGamma, {}, QStringLiteral("monitor"));
    monitorGamma->setCurrentItem(KdenliveSettings::monitor_gamma());
    connect(monitorGamma, &KSelectAction::indexTriggered, this, &MainWindow::slotSetMonitorGamma);
    actionCollection()->setShortcutsConfigurable(monitorGamma, false);

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
    addAction(QStringLiteral("monitor_seek_guide_backward"), i18n("Go to Previous Guide"), this, SLOT(slotGuideRewind()),
              QIcon::fromTheme(QStringLiteral("media-seek-backward")), Qt::CTRL | Qt::Key_Left, QStringLiteral("navandplayback"));
    addAction(QStringLiteral("monitor_seek_guide_forward"), i18n("Go to Next Guide"), this, SLOT(slotGuideForward()),
              QIcon::fromTheme(QStringLiteral("media-seek-forward")), Qt::CTRL | Qt::Key_Right, QStringLiteral("navandplayback"));
    addAction(QStringLiteral("align_playhead"), i18n("Align Playhead to Mouse Position"), this, SLOT(slotAlignPlayheadToMousePos()), QIcon(), Qt::Key_P,
              QStringLiteral("navandplayback"));

    addAction(QStringLiteral("grab_item"), i18n("Grab Current Item"), this, SLOT(slotGrabItem()), QIcon::fromTheme(QStringLiteral("transform-move")),
              Qt::SHIFT | Qt::Key_G);

    QAction *stickTransition = new QAction(i18n("Automatic Transition"), this);
    stickTransition->setData(QStringLiteral("auto"));
    stickTransition->setCheckable(true);
    stickTransition->setEnabled(false);
    addAction(QStringLiteral("auto_transition"), stickTransition);
    connect(stickTransition, &QAction::triggered, this, &MainWindow::slotAutoTransition);

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
    QAction *addPreviewZone = addAction(QStringLiteral("set_render_timeline_zone"), i18n("Add Preview Zone"), this, SLOT(slotDefinePreviewRender()),
                                        QIcon::fromTheme(QStringLiteral("preview-add-zone")));
    addPreviewZone->setWhatsThis(xi18nc("@info:whatsthis", "Add the currently defined timeline/selection zone as a preview render zone"));
    QAction *removePreviewZone = addAction(QStringLiteral("unset_render_timeline_zone"), i18n("Remove Preview Zone"), this, SLOT(slotRemovePreviewRender()),
                                           QIcon::fromTheme(QStringLiteral("preview-remove-zone")));
    removePreviewZone->setWhatsThis(xi18nc(
        "@info:whatsthis",
        "Removes the currently defined timeline/selection zone from the preview render zone. Note that this can leave gaps in the preview render zones."));
    QAction *removeAllPreviewZone = addAction(QStringLiteral("clear_render_timeline_zone"), i18n("Remove All Preview Zones"), this,
                                              SLOT(slotClearPreviewRender()), QIcon::fromTheme(QStringLiteral("preview-remove-all")));
    removeAllPreviewZone->setWhatsThis(xi18nc("@info:whatsthis", "Remove all preview render zones."));
    QAction *startPreviewRender = addAction(QStringLiteral("prerender_timeline_zone"), i18n("Start Preview Render"), this, SLOT(slotPreviewRender()),
                                            QIcon::fromTheme(QStringLiteral("preview-render-on")), QKeySequence(Qt::SHIFT | Qt::Key_Return));
    startPreviewRender->setWhatsThis(xi18nc("@info:whatsthis",
                                            "Click to start the rendering of all preview zones (recommended for areas with complex and many effects).<nl/>"
                                            "Click on the down-arrow icon to get a list of options (for example: add preview render zone, remove all zones)."));
    addAction(QStringLiteral("stop_prerender_timeline"), i18n("Stop Preview Render"), this, SLOT(slotStopPreviewRender()),
              QIcon::fromTheme(QStringLiteral("preview-render-off")));

    addAction(QStringLiteral("select_timeline_zone"), i18n("Adjust Timeline Zone to Selection"), this, SLOT(slotSelectTimelineZone()),
              QIcon::fromTheme(QStringLiteral("edit-select")), Qt::SHIFT | Qt::Key_Z);
    addAction(QStringLiteral("select_timeline_clip"), i18n("Select Clip"), this, SLOT(slotSelectTimelineClip()),
              QIcon::fromTheme(QStringLiteral("edit-select")), Qt::Key_Plus);
    addAction(QStringLiteral("deselect_timeline_clip"), i18n("Deselect Clip"), this, SLOT(slotDeselectTimelineClip()),
              QIcon::fromTheme(QStringLiteral("edit-select")), Qt::Key_Minus);
    addAction(QStringLiteral("select_add_timeline_clip"), i18n("Add Clip to Selection"), this, SLOT(slotSelectAddTimelineClip()),
              QIcon::fromTheme(QStringLiteral("edit-select")), Qt::ALT | Qt::Key_Plus);
    addAction(QStringLiteral("select_timeline_transition"), i18n("Select Transition"), this, SLOT(slotSelectTimelineTransition()),
              QIcon::fromTheme(QStringLiteral("edit-select")), Qt::SHIFT | Qt::Key_Plus);
    addAction(QStringLiteral("deselect_timeline_transition"), i18n("Deselect Transition"), this, SLOT(slotDeselectTimelineTransition()),
              QIcon::fromTheme(QStringLiteral("edit-select")), Qt::SHIFT | Qt::Key_Minus);
    addAction(QStringLiteral("select_add_timeline_transition"), i18n("Add Transition to Selection"), this, SLOT(slotSelectAddTimelineTransition()),
              QIcon::fromTheme(QStringLiteral("edit-select")), Qt::ALT | Qt::SHIFT | Qt::Key_Plus);

    addAction(QStringLiteral("delete_all_clip_markers"), i18n("Delete All Markers"), this, SLOT(slotDeleteAllClipMarkers()),
              QIcon::fromTheme(QStringLiteral("edit-delete")));
    addAction(QStringLiteral("add_marker_guide_quickly"), i18n("Add Marker/Guide quickly"), this, SLOT(slotAddMarkerGuideQuickly()),
              QIcon::fromTheme(QStringLiteral("bookmark-new")), QKeySequence(Qt::KeypadModifier | Qt::Key_Asterisk));

    // Clip actions. We set some category info on the action data to enable/disable it contextually in timelinecontroller
    KActionCategory *clipActionCategory = new KActionCategory(i18n("Current Selection"), actionCollection());

    QAction *addMarker = addAction(QStringLiteral("add_clip_marker"), i18n("Add Marker"), this, SLOT(slotAddClipMarker()),
                                   QIcon::fromTheme(QStringLiteral("bookmark-new")), QKeySequence(), clipActionCategory);
    addMarker->setData('P');

    QAction *delMarker = addAction(QStringLiteral("delete_clip_marker"), i18n("Delete Marker"), this, SLOT(slotDeleteClipMarker()),
                                   QIcon::fromTheme(QStringLiteral("edit-delete")), QKeySequence(), clipActionCategory);
    delMarker->setData('P');

    QAction *editClipMarker = addAction(QStringLiteral("edit_clip_marker"), i18n("Edit Marker…"), this, SLOT(slotEditClipMarker()),
                                        QIcon::fromTheme(QStringLiteral("document-properties")), QKeySequence(), clipActionCategory);
    editClipMarker->setObjectName(QStringLiteral("edit_marker"));
    editClipMarker->setData('P');

    QAction *splitAudio = addAction(QStringLiteral("clip_split"), i18n("Restore Audio"), this, SLOT(slotSplitAV()),
                                    QIcon::fromTheme(QStringLiteral("document-new")), QKeySequence(), clipActionCategory);
    // "S" will be handled specifically to change the action name depending on current selection
    splitAudio->setData('S');
    splitAudio->setEnabled(false);

    QAction *extractClip = addAction(QStringLiteral("extract_clip"), i18n("Extract Clip"), this, SLOT(slotExtractClip()),
                                     QIcon::fromTheme(QStringLiteral("timeline-extract")), QKeySequence(), clipActionCategory);
    extractClip->setData('C');
    extractClip->setEnabled(false);

    QAction *extractToBin =
        addAction(QStringLiteral("save_to_bin"), i18n("Save Clip Part to Bin"), this, SLOT(slotSaveZoneToBin()), QIcon(), QKeySequence(), clipActionCategory);
    extractToBin->setData('C');
    extractToBin->setEnabled(false);

    QAction *switchEnable =
        addAction(QStringLiteral("clip_switch"), i18n("Disable Clip"), this, SLOT(slotSwitchClip()), QIcon(), QKeySequence(), clipActionCategory);
    // "W" will be handled specifically to change the action name depending on current selection
    switchEnable->setData('W');
    switchEnable->setEnabled(false);

    QAction *setAudioAlignReference = addAction(QStringLiteral("set_audio_align_ref"), i18n("Set Audio Reference"), this, SLOT(slotSetAudioAlignReference()),
                                                QIcon(), QKeySequence(), clipActionCategory);
    // "A" as data means this action should only be available for clips with audio
    setAudioAlignReference->setData('A');
    setAudioAlignReference->setEnabled(false);

    QAction *alignAudio =
        addAction(QStringLiteral("align_audio"), i18n("Align Audio to Reference"), this, SLOT(slotAlignAudio()), QIcon(), QKeySequence(), clipActionCategory);
    // "A" as data means this action should only be available for clips with audio
    // alignAudio->setData('A');
    alignAudio->setEnabled(false);

    QAction *act = addAction(QStringLiteral("edit_item_duration"), i18n("Edit Duration"), this, SLOT(slotEditItemDuration()),
                             QIcon::fromTheme(QStringLiteral("measure")), QKeySequence(), clipActionCategory);
    act->setEnabled(false);

    act = addAction(QStringLiteral("edit_item_speed"), i18n("Change Speed"), this, SLOT(slotEditItemSpeed()), QIcon::fromTheme(QStringLiteral("speedometer")),
                    QKeySequence(), clipActionCategory);
    // "Q" as data means this action should only be available if the item is not endless and has no time remap
    act->setData('Q');
    act->setEnabled(false);

    act = addAction(QStringLiteral("edit_item_remap"), i18n("Time Remap"), this, SLOT(slotRemapItemTime()), QIcon::fromTheme(QStringLiteral("speedometer")),
                    QKeySequence(), clipActionCategory);
    // "R" as data means this action should only be available if the item is not endless and has no speed effect
    act->setData('R');
    act->setCheckable(true);
    act->setEnabled(false);

    act = addAction(QStringLiteral("clip_in_project_tree"), i18n("Clip in Project Bin"), this, SLOT(slotClipInProjectTree()),
                    QIcon::fromTheme(QStringLiteral("find-location")), QKeySequence(), clipActionCategory);
    act->setEnabled(false);
    // "C" as data means this action should only be available for clips - not for compositions
    act->setData('C');

    addAction(QStringLiteral("cut_timeline_clip"), i18n("Cut Clip"), this, SLOT(slotCutTimelineClip()), QIcon::fromTheme(QStringLiteral("edit-cut")),
              Qt::SHIFT | Qt::Key_R);

    addAction(QStringLiteral("cut_timeline_all_clips"), i18n("Cut All Clips"), this, SLOT(slotCutTimelineAllClips()),
              QIcon::fromTheme(QStringLiteral("edit-cut")), Qt::CTRL | Qt::SHIFT | Qt::Key_R);

    addAction(QStringLiteral("delete_timeline_clip"), i18n("Delete Selected Item"), this, SLOT(slotDeleteItem()),
              QIcon::fromTheme(QStringLiteral("edit-delete")), Qt::Key_Delete);

    QAction *resizeStart = new QAction(QIcon(), i18n("Resize Item Start"), this);
    addAction(QStringLiteral("resize_timeline_clip_start"), resizeStart, QKeySequence(Qt::Key_ParenLeft));
    connect(resizeStart, &QAction::triggered, this, &MainWindow::slotResizeItemStart);

    QAction *resizeEnd = new QAction(QIcon(), i18n("Resize Item End"), this);
    addAction(QStringLiteral("resize_timeline_clip_end"), resizeEnd, QKeySequence(Qt::Key_ParenRight));
    connect(resizeEnd, &QAction::triggered, this, &MainWindow::slotResizeItemEnd);

    QAction *pasteEffects = addAction(QStringLiteral("paste_effects"), i18n("Paste Effects"), this, SLOT(slotPasteEffects()),
                                      QIcon::fromTheme(QStringLiteral("edit-paste")), QKeySequence(), clipActionCategory);
    pasteEffects->setEnabled(false);
    // "C" as data means this action should only be available for clips - not for compositions
    pasteEffects->setData('C');

    QAction *delEffects = new QAction(QIcon::fromTheme(QStringLiteral("edit-delete")), i18n("Delete Effects"), this);
    addAction(QStringLiteral("delete_effects"), delEffects, QKeySequence(), clipActionCategory);
    delEffects->setEnabled(false);
    // "C" as data means this action should only be available for clips - not for compositions
    delEffects->setData('C');
    connect(delEffects, &QAction::triggered, this, [this]() { getCurrentTimeline()->controller()->deleteEffects(); });

    QAction *groupClip = addAction(QStringLiteral("group_clip"), i18n("Group Clips"), this, SLOT(slotGroupClips()),
                                   QIcon::fromTheme(QStringLiteral("object-group")), Qt::CTRL | Qt::Key_G, clipActionCategory);
    // "G" as data means this action should only be available for multiple items selection
    groupClip->setData('G');
    groupClip->setEnabled(false);

    QAction *ungroupClip = addAction(QStringLiteral("ungroup_clip"), i18n("Ungroup Clips"), this, SLOT(slotUnGroupClips()),
                                     QIcon::fromTheme(QStringLiteral("object-ungroup")), QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_G), clipActionCategory);
    // "U" as data means this action should only be available if selection is a group
    ungroupClip->setData('U');
    ungroupClip->setEnabled(false);

    QAction *sentToSequence = addAction(QStringLiteral("send_sequence"), i18n("Create Sequence from Selection"), this, SLOT(slotCreateSequenceFromSelection()),
                                        QIcon::fromTheme(QStringLiteral("bookmark-new")), QKeySequence(), clipActionCategory);
    sentToSequence->setWhatsThis(
        xi18nc("@info:whatsthis", "Adds the clip(s) currently selected in the timeline to a new sequence clip that can be opened in another timeline tab."));
    sentToSequence->setData('G');
    sentToSequence->setEnabled(false);

    act = clipActionCategory->addAction(KStandardAction::Copy, this, SLOT(slotCopy()));
    act->setEnabled(false);

    KStandardAction::paste(this, SLOT(slotPaste()), actionCollection());

    // Keyframe actions
    m_assetPanel = new AssetPanel(this);
    connect(getBin(), &Bin::requestShowEffectStack, m_assetPanel, &AssetPanel::showEffectStack);
    KActionCategory *kfActions = new KActionCategory(i18n("Effect Keyframes"), actionCollection());
    addAction(QStringLiteral("keyframe_add"), i18n("Add/Remove Keyframe"), m_assetPanel, SLOT(slotAddRemoveKeyframe()),
              QIcon::fromTheme(QStringLiteral("keyframe-add")), QKeySequence(), kfActions);
    addAction(QStringLiteral("keyframe_next"), i18n("Go to next keyframe"), m_assetPanel, SLOT(slotNextKeyframe()),
              QIcon::fromTheme(QStringLiteral("keyframe-next")), QKeySequence(), kfActions);
    addAction(QStringLiteral("keyframe_previous"), i18n("Go to previous keyframe"), m_assetPanel, SLOT(slotPreviousKeyframe()),
              QIcon::fromTheme(QStringLiteral("keyframe-previous")), QKeySequence(), kfActions);

    /*act = KStandardAction::copy(this, SLOT(slotCopy()), actionCollection());
    clipActionCategory->addAction(KStandardAction::name(KStandardAction::Copy), act);
    act->setEnabled(false);
    act = KStandardAction::paste(this, SLOT(slotPaste()), actionCollection());
    clipActionCategory->addAction(KStandardAction::name(KStandardAction::Paste), act);
    act->setEnabled(false);*/

    kdenliveCategoryMap.insert(QStringLiteral("timelineselection"), clipActionCategory);

    addAction(QStringLiteral("insert_space"), i18n("Insert Space…"), this, SLOT(slotInsertSpace()));
    addAction(QStringLiteral("delete_space"), i18n("Remove Space"), this, SLOT(slotRemoveSpace()));
    addAction(QStringLiteral("delete_all_spaces"), i18n("Remove All Spaces After Cursor"), this, SLOT(slotRemoveAllSpacesInTrack()));
    addAction(QStringLiteral("delete_all_clips"), i18n("Remove All Clips After Cursor"), this, SLOT(slotRemoveAllClipsInTrack()));
    addAction(QStringLiteral("delete_space_all_tracks"), i18n("Remove Space in All Tracks"), this, SLOT(slotRemoveSpaceInAllTracks()));

    KActionCategory *timelineActions = new KActionCategory(i18n("Tracks"), actionCollection());
    QAction *insertTrack = new QAction(QIcon(), i18nc("@action", "Insert Track…"), this);
    connect(insertTrack, &QAction::triggered, this, &MainWindow::slotInsertTrack);
    timelineActions->addAction(QStringLiteral("insert_track"), insertTrack);

    QAction *autoTrackHeight = new QAction(QIcon(), i18n("Fit all Tracks in View"), this);
    autoTrackHeight->setCheckable(true);
    autoTrackHeight->setChecked(KdenliveSettings::autotrackheight());
    connect(autoTrackHeight, &QAction::triggered, this, &MainWindow::slotAutoTrackHeight);
    timelineActions->addAction(QStringLiteral("fit_all_tracks"), autoTrackHeight);

    QAction *masterEffectStack = new QAction(QIcon::fromTheme(QStringLiteral("kdenlive-composite")), i18n("Master effects"), this);
    connect(masterEffectStack, &QAction::triggered, this, [&]() {
        pCore->monitorManager()->activateMonitor(Kdenlive::ProjectMonitor);
        getCurrentTimeline()->controller()->showMasterEffects();
    });
    timelineActions->addAction(QStringLiteral("master_effects"), masterEffectStack);

    QAction *switchTrackTarget = new QAction(QIcon(), i18n("Switch Track Target Audio Stream"), this);
    connect(switchTrackTarget, &QAction::triggered, this, &MainWindow::slotSwitchTrackAudioStream);
    timelineActions->addAction(QStringLiteral("switch_target_stream"), switchTrackTarget);
    actionCollection()->setDefaultShortcut(switchTrackTarget, Qt::Key_Apostrophe);

    QAction *deleteTrack = new QAction(QIcon(), i18n("Delete Track…"), this);
    connect(deleteTrack, &QAction::triggered, this, &MainWindow::slotDeleteTrack);
    timelineActions->addAction(QStringLiteral("delete_track"), deleteTrack);
    deleteTrack->setData("delete_track");

    QAction *showAudio = new QAction(QIcon(), i18n("Show Record Controls"), this);
    connect(showAudio, &QAction::triggered, this, &MainWindow::slotShowTrackRec);
    timelineActions->addAction(QStringLiteral("show_track_record"), showAudio);
    showAudio->setCheckable(true);
    showAudio->setData("show_track_record");

    QAction *selectTrack = new QAction(QIcon(), i18n("Select All in Current Track"), this);
    connect(selectTrack, &QAction::triggered, this, &MainWindow::slotSelectTrack);
    timelineActions->addAction(QStringLiteral("select_track"), selectTrack);

    QAction *selectAll = KStandardAction::selectAll(this, SLOT(slotSelectAllTracks()), this);
    selectAll->setIcon(QIcon::fromTheme(QStringLiteral("edit-select-all")));
    selectAll->setShortcutContext(Qt::WidgetWithChildrenShortcut);
    timelineActions->addAction(QStringLiteral("select_all_tracks"), selectAll);

    QAction *unselectAll = KStandardAction::deselect(this, SLOT(slotUnselectAllTracks()), this);
    unselectAll->setIcon(QIcon::fromTheme(QStringLiteral("edit-select-none")));
    unselectAll->setShortcutContext(Qt::WidgetWithChildrenShortcut);
    timelineActions->addAction(QStringLiteral("unselect_all_tracks"), unselectAll);

    kdenliveCategoryMap.insert(QStringLiteral("timeline"), timelineActions);

    // Cached data management
    addAction(QStringLiteral("manage_cache"), i18n("Manage Cached Data…"), this, SLOT(slotManageCache()),
              QIcon::fromTheme(QStringLiteral("network-server-database")));

    QAction *disablePreview = new QAction(i18n("Disable Timeline Preview"), this);
    disablePreview->setCheckable(true);
    addAction(QStringLiteral("disable_preview"), disablePreview);

    addAction(QStringLiteral("add_guide"), i18n("Add/Remove Guide"), this, SLOT(slotAddGuide()), QIcon::fromTheme(QStringLiteral("list-add")), Qt::Key_G);
    addAction(QStringLiteral("delete_guide"), i18n("Delete Guide"), this, SLOT(slotDeleteGuide()), QIcon::fromTheme(QStringLiteral("edit-delete")));
    addAction(QStringLiteral("edit_guide"), i18n("Edit Guide…"), this, SLOT(slotEditGuide()), QIcon::fromTheme(QStringLiteral("document-properties")));
    addAction(QStringLiteral("search_guide"), i18n("Search Guide…"), this, SLOT(slotSearchGuide()), QIcon::fromTheme(QStringLiteral("edit-find")));
    addAction(QStringLiteral("export_guides"), i18n("Export Guides…"), this, SLOT(slotExportGuides()), QIcon::fromTheme(QStringLiteral("document-export")));

    QAction *lockGuides =
        addAction(QStringLiteral("lock_guides"), i18n("Guides Locked"), this, SLOT(slotLockGuides(bool)), QIcon::fromTheme(QStringLiteral("lock")));
    lockGuides->setCheckable(true);
    lockGuides->setChecked(KdenliveSettings::lockedGuides());
    lockGuides->setToolTip(i18n("Lock guides"));
    lockGuides->setWhatsThis(
        xi18nc("@info:whatsthis", "Lock guides. When locked, the guides won't move when using the spacer tool or inserting/removing blank in tracks."));

    addAction(QStringLiteral("delete_all_guides"), i18n("Delete All Guides"), this, SLOT(slotDeleteAllGuides()),
              QIcon::fromTheme(QStringLiteral("edit-delete")));
    addAction(QStringLiteral("add_subtitle"), i18n("Add Subtitle"), this, SLOT(slotAddSubtitle()), QIcon::fromTheme(QStringLiteral("list-add")),
              Qt::SHIFT | Qt::Key_S);
    addAction(QStringLiteral("disable_subtitle"), i18n("Disable Subtitle"), this, SLOT(slotDisableSubtitle()), QIcon::fromTheme(QStringLiteral("view-hidden")));
    addAction(QStringLiteral("lock_subtitle"), i18n("Lock Subtitle"), this, SLOT(slotLockSubtitle()), QIcon::fromTheme(QStringLiteral("lock")));

    addAction(QStringLiteral("manage_subtitle"), i18n("Manage Subtitles"), this, SLOT(slotManageSubtitle()),
              QIcon::fromTheme(QStringLiteral("settings-configure")));
    addAction(QStringLiteral("import_subtitle"), i18n("Import Subtitle File…"), this, SLOT(slotImportSubtitle()),
              QIcon::fromTheme(QStringLiteral("document-import")));
    addAction(QStringLiteral("export_subtitle"), i18n("Export Subtitle File…"), this, SLOT(slotExportSubtitle()),
              QIcon::fromTheme(QStringLiteral("document-export")));
    addAction(QStringLiteral("delete_subtitle_clip"), i18n("Delete Subtitle"), this, SLOT(slotDeleteItem()), QIcon::fromTheme(QStringLiteral("edit-delete")));
    addAction(QStringLiteral("audio_recognition"), i18n("Speech Recognition…"), this, SLOT(slotSpeechRecognition()),
              QIcon::fromTheme(QStringLiteral("autocorrection")));

    m_saveAction = KStandardAction::save(pCore->projectManager(), SLOT(saveFile()), actionCollection());
    m_saveAction->setIcon(QIcon::fromTheme(QStringLiteral("document-save")));

    QAction *const showMenuBarAction = KStandardAction::showMenubar(this, &MainWindow::showMenuBar, actionCollection());
    showMenuBarAction->setWhatsThis(xi18nc("@info:whatsthis", "This switches between having a <emphasis>Menubar</emphasis> "
                                                              "and having a <interface>Hamburger Menu</interface> button in the main Toolbar."));

    KStandardAction::quit(this, &MainWindow::close, actionCollection());
    KStandardAction::keyBindings(this, &MainWindow::slotEditKeys, actionCollection());
    KStandardAction::preferences(this, &MainWindow::slotPreferences, actionCollection());
    KStandardAction::configureNotifications(this, &MainWindow::configureNotifications, actionCollection());
    KStandardAction::fullScreen(this, &MainWindow::slotFullScreen, this, actionCollection());

    QAction *undo = KStandardAction::undo(m_commandStack, SLOT(undo()), actionCollection());
    undo->setEnabled(false);
    connect(m_commandStack, &QUndoGroup::canUndoChanged, undo, &QAction::setEnabled);
    connect(this, &MainWindow::enableUndo, this, [this, undo](bool enable) {
        if (enable) {
            enable = m_commandStack->activeStack()->canUndo();
        }
        undo->setEnabled(enable);
    });

    QAction *redo = KStandardAction::redo(m_commandStack, SLOT(redo()), actionCollection());
    redo->setEnabled(false);
    connect(m_commandStack, &QUndoGroup::canRedoChanged, redo, &QAction::setEnabled);
    connect(this, &MainWindow::enableUndo, this, [this, redo](bool enable) {
        if (enable) {
            enable = m_commandStack->activeStack()->canRedo();
        }
        redo->setEnabled(enable);
    });

    addAction(QStringLiteral("copy_debuginfo"), i18n("Copy Debug Information"), this, SLOT(slotCopyDebugInfo()), QIcon::fromTheme(QStringLiteral("edit-copy")));

    QAction *disableEffects = addAction(QStringLiteral("disable_timeline_effects"), i18n("Disable Timeline Effects"), pCore->projectManager(),
                                        SLOT(slotDisableTimelineEffects(bool)), QIcon::fromTheme(QStringLiteral("favorite")));
    disableEffects->setData("disable_timeline_effects");
    disableEffects->setCheckable(true);
    disableEffects->setChecked(false);

    addAction(QStringLiteral("switch_track_disabled"), i18n("Toggle Track Disabled"), pCore->projectManager(), SLOT(slotSwitchTrackDisabled()), QIcon(),
              Qt::SHIFT | Qt::Key_H, timelineActions);
    addAction(QStringLiteral("switch_track_lock"), i18n("Toggle Track Lock"), pCore->projectManager(), SLOT(slotSwitchTrackLock()), QIcon(),
              Qt::SHIFT | Qt::Key_L, timelineActions);
    addAction(QStringLiteral("switch_all_track_lock"), i18n("Toggle All Track Lock"), pCore->projectManager(), SLOT(slotSwitchAllTrackLock()), QIcon(),
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
              QIcon::fromTheme(QStringLiteral("bookmark-new")), {}, timelineActions);

    // Build activate track shortcut sequences
    QList<int> keysequence{Qt::Key_1, Qt::Key_2, Qt::Key_3, Qt::Key_4, Qt::Key_5, Qt::Key_6, Qt::Key_7, Qt::Key_8, Qt::Key_9};
    for (int i = 1; i < 10; i++) {
        QAction *ac = new QAction(QIcon(), i18n("Select Audio Track %1", i), this);
        ac->setData(i - 1);
        connect(ac, &QAction::triggered, this, &MainWindow::slotActivateAudioTrackSequence);
        addAction(QString("activate_audio_%1").arg(i), ac, QKeySequence(Qt::ALT | keysequence[i - 1]), timelineActions);
        QAction *ac2 = new QAction(QIcon(), i18n("Select Video Track %1", i), this);
        ac2->setData(i - 1);
        connect(ac2, &QAction::triggered, this, &MainWindow::slotActivateVideoTrackSequence);
        addAction(QString("activate_video_%1").arg(i), ac2, QKeySequence(keysequence[i - 1]), timelineActions);
        QAction *ac3 = new QAction(QIcon(), i18n("Select Target %1", i), this);
        ac3->setData(i - 1);
        connect(ac3, &QAction::triggered, this, &MainWindow::slotActivateTarget);
        addAction(QString("activate_target_%1").arg(i), ac3, QKeySequence(Qt::CTRL | keysequence[i - 1]), timelineActions);
    }

    // Setup effects and transitions actions.
    KActionCategory *transitionActions = new KActionCategory(i18n("Transitions"), actionCollection());
    // m_transitions = new QAction*[transitions.count()];
    auto allTransitions = TransitionsRepository::get()->getNames();
    for (const auto &transition : qAsConst(allTransitions)) {
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
}

void MainWindow::saveOptions()
{
    KdenliveSettings::self()->save();
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
    QFont ft = QFontDatabase::systemFont(QFontDatabase::SmallestReadableFont);
    // Default unit for timeline.qml objects size
    int baseUnit = qMax(28, qRound(QFontInfo(ft).pixelSize() * 1.8));
    if (KdenliveSettings::trackheight() == 0) {
        int trackHeight = qMax(50, int(2.2 * baseUnit + 6));
        KdenliveSettings::setTrackheight(trackHeight);
    }
    bool firstRun = false;
    KConfigGroup initialGroup(config, "version");
    if (!initialGroup.exists() || KdenliveSettings::sdlAudioBackend().isEmpty() || KdenliveSettings::kdenliverendererpath().isEmpty()) {
        // First run, check if user is on a KDE Desktop
        firstRun = true;
        // Define default video location for first run
        KRecentDirs::add(QStringLiteral(":KdenliveClipFolder"), QStandardPaths::writableLocation(QStandardPaths::MoviesLocation));

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
    ProjectSettings *w = new ProjectSettings(project, project->metadata(), getCurrentTimeline()->controller()->extractCompositionLumas(), p.second, p.first,
                                             channels, project->projectTempFolder(), true, !project->isModified(), this);
    if (ix > 0) {
        w->tabWidget->setCurrentIndex(ix);
    }
    connect(w, &ProjectSettings::disableProxies, this, &MainWindow::slotDisableProxies);
    // connect(w, SIGNAL(disablePreview()), pCore->projectManager()->currentTimeline(), SLOT(invalidateRange()));
    connect(w, &ProjectSettings::refreshProfiles, this, &MainWindow::slotRefreshProfiles);

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
            slotClearPreviewRender(false);
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
        QString newProjectFolder = w->storageFolder();

        if (w->docFolderAsStorageFolder()) {
            newProjectFolder = QFileInfo(project->url().toLocalFile()).absolutePath() + QStringLiteral("/cachefiles");
        }
        if (newProjectFolder.isEmpty()) {
            newProjectFolder = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
        }
        if (newProjectFolder != project->projectTempFolder()) {
            KMessageBox::ButtonCode answer;
            // Project folder changed:
            if (project->isModified()) {
                answer = KMessageBox::warningContinueCancel(
                    this, i18n("The current project has not been saved.<br/>This will first save the project, then move "
                               "all temporary files from <br/><b>%1</b> to <b>%2</b>,<br>and the project file will be reloaded",
                               project->projectTempFolder(), newProjectFolder));
                if (answer == KMessageBox::Continue) {
                    pCore->projectManager()->saveFile();
                }
            } else {
                answer = KMessageBox::warningContinueCancel(
                    this, i18n("This will move all temporary files from<br/><b>%1</b> to <b>%2</b>,<br/>the project file will then be reloaded",
                               project->projectTempFolder(), newProjectFolder));
            }
            if (answer == KMessageBox::Continue) {
                // Proceed with move
                QString documentId = QDir::cleanPath(project->getDocumentProperty(QStringLiteral("documentid")));
                bool ok;
                documentId.toLongLong(&ok, 10);
                if (!ok || documentId.isEmpty()) {
                    KMessageBox::error(this, i18n("Cannot perform operation, invalid document id: %1", documentId));
                } else {
                    QDir newDir(newProjectFolder);
                    QDir oldDir(project->projectTempFolder());
                    if (newDir.exists(documentId)) {
                        KMessageBox::error(this, i18n("Cannot perform operation, target directory already exists: %1", newDir.absoluteFilePath(documentId)));
                    } else {
                        // Proceed with the move
                        pCore->projectManager()->moveProjectData(oldDir.absoluteFilePath(documentId), newDir.absolutePath());
                    }
                }
            }
        }
        if (pCore->getCurrentProfile()->path() != profile || project->profileChanged(profile)) {
            if (!qFuzzyCompare(pCore->getCurrentProfile()->fps() - ProfileRepository::get()->getProfile(profile)->fps(), 0.)) {
                // Fps was changed, we save the project to an xml file with updated profile and reload project
                // Check if blank project
                if (project->url().fileName().isEmpty() && !project->isModified()) {
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
    delete w;
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
        connect(this, &MainWindow::updateRenderWidgetProfile, m_renderWidget, &RenderWidget::adjustViewToProfile);
        m_renderWidget->setGuides(project->getGuideModel(getCurrentTimeline()->getUuid()));
        m_renderWidget->updateDocumentPath();
        m_renderWidget->setRenderProfile(project->getRenderProperties());
    }

    slotCheckRenderStatus();
    if (m_renderWidget) {
        m_renderWidget->showNormal();
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
    pCore->bin()->cleanupUnused();
}

void MainWindow::slotUpdateMousePosition(int pos, int duration)
{
    if (pCore->currentDoc()) {
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

void MainWindow::slotUpdateZoneDuration(int duration)
{
    if (m_renderWidget) {
        m_renderWidget->zoneDurationChanged(duration);
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
    // update track compositing
    bool compositing = project->getDocumentProperty(QStringLiteral("compositing"), QStringLiteral("1")).toInt() > 0;
    Q_EMIT project->updateCompositionMode(compositing);
    getCurrentTimeline()->controller()->switchCompositing(compositing);
    slotUpdateProjectDuration(getCurrentTimeline()->model()->duration() - 1);
    const QUuid uuid = getCurrentTimeline()->getUuid();

    int activeTrackPosition = project->getSequenceProperty(uuid, QStringLiteral("activeTrack"), QString::number(-1)).toInt();
    if (activeTrackPosition == -2) {
        // Subtitle model track always has ID == -2
        getCurrentTimeline()->controller()->setActiveTrack(-2);
    } else if (activeTrackPosition > -1 && activeTrackPosition < getCurrentTimeline()->model()->getTracksCount()) {
        // otherwise, convert the position to a track ID
        getCurrentTimeline()->controller()->setActiveTrack(getCurrentTimeline()->model()->getTrackIndexFromPosition(activeTrackPosition));
    } else {
        qWarning() << "[BUG] \"activeTrack\" property is" << activeTrackPosition << "but track count is only"
                   << getCurrentTimeline()->model()->getTracksCount();
        // set it to some valid track instead
        getCurrentTimeline()->controller()->setActiveTrack(getCurrentTimeline()->model()->getTrackIndexFromPosition(0));
    }

    m_clipMonitor->updateDocumentUuid();
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
    connect(pCore->bin(), &Bin::processDragEnd, getCurrentTimeline(), &TimelineWidget::endDrag);

    // Load master effect zones
    getCurrentTimeline()->controller()->updateMasterZones(getCurrentTimeline()->model()->getMasterEffectZones());

    m_buttonSelectTool->setChecked(true);
    connect(m_projectMonitorDock, &QDockWidget::visibilityChanged, m_projectMonitor, &Monitor::slotRefreshMonitor, Qt::UniqueConnection);
    connect(m_clipMonitorDock, &QDockWidget::visibilityChanged, m_clipMonitor, &Monitor::slotRefreshMonitor, Qt::UniqueConnection);
    pCore->guidesList()->reset();
    getCurrentTimeline()->focusTimeline();
}

void MainWindow::slotEditKeys()
{
    KShortcutsDialog dialog(KShortcutsEditor::AllActions, KShortcutsEditor::LetterShortcutsAllowed, this);

#if KXMLGUI_VERSION >= QT_VERSION_CHECK(5, 98, 0)
    KNSWidgets::Action *downloadKeybordSchemes =
        new KNSWidgets::Action(i18n("Download New Keyboard Schemes…"), QStringLiteral(":data/kdenlive_keyboardschemes.knsrc"), this);
    connect(downloadKeybordSchemes, &KNSWidgets::Action::dialogFinished, this, [&](const QList<KNSCore::Entry> &changedEntries) {
        if (changedEntries.count() > 0) {
            dialog.refreshSchemes();
        }
    });
    dialog.addActionToSchemesMoreButton(downloadKeybordSchemes);
#else
    // Find the combobox inside KShortcutsDialog for choosing keyboard scheme
    QComboBox *schemesList = nullptr;
    for (QLabel *label : dialog.findChildren<QLabel *>()) {
        if (label->text() == i18n("Current scheme:")) {
            schemesList = qobject_cast<QComboBox *>(label->buddy());
            break;
        }
    }
    // If scheme choosing combobox was found, find the "More Actions" button in the same
    // dialog that provides a dropdown menu with additional actions, and add
    // "Download New Keyboard Schemes…" button into that menu
    if (schemesList) {
        for (QPushButton *button : dialog.findChildren<QPushButton *>()) {
            if (button->text() == i18n("More Actions")) {
                QMenu *moreActionsMenu = button->menu();
                if (moreActionsMenu) {
                    moreActionsMenu->addAction(i18n("Download New Keyboard Schemes…"), this, [this, schemesList] { slotGetNewKeyboardStuff(schemesList); });
                }
                break;
            }
        }
    } else {
        qWarning() << "Could not get list of schemes. Downloading new schemes is not available.";
    }
#endif
    dialog.addCollection(actionCollection(), i18nc("general keyboard shortcuts", "General"));
    // Update the shortcut conflicts list bewtween mainwindow and media browser
    connect(&dialog, &KShortcutsDialog::saved, pCore->mediaBrowser(), &MediaBrowser::detectShortcutConflicts);
    dialog.configure();
}

void MainWindow::slotPreferences()
{
    slotShowPreferencePage(Kdenlive::NoPage);
}

void MainWindow::slotShowPreferencePage(Kdenlive::ConfigPage page, int option)
{
    /*
     * An instance of your dialog could be already created and could be
     * cached, in which case you want to display the cached dialog
     * instead of creating another one
     */
    if (KConfigDialog::showDialog(QStringLiteral("settings"))) {
        KdenliveSettingsDialog *d = static_cast<KdenliveSettingsDialog *>(KConfigDialog::exists(QStringLiteral("settings")));
        if (page != Kdenlive::NoPage) {
            d->showPage(page, option);
        }
        return;
    }

    // KConfigDialog didn't find an instance of this dialog, so lets
    // create it :

    // Get the mappable actions in localized form
    QMap<QString, QString> actions;
    KActionCollection *collection = actionCollection();
    for (const QString &action_name : qAsConst(m_actionNames)) {
        const QString action_text = KLocalizedString::removeAcceleratorMarker(collection->action(action_name)->text());
        actions[action_text] = action_name;
    }

    auto *dialog = new KdenliveSettingsDialog(actions, m_gpuAllowed, this);
    connect(dialog, &KConfigDialog::settingsChanged, this, &MainWindow::updateConfiguration);
    connect(dialog, &KConfigDialog::settingsChanged, this, &MainWindow::configurationChanged);
    connect(dialog, &KdenliveSettingsDialog::doResetConsumer, this, [this](bool fullReset) {
        m_scaleGroup->setEnabled(!KdenliveSettings::external_display());
        pCore->projectManager()->slotResetConsumers(fullReset);
    });
    connect(dialog, &KdenliveSettingsDialog::checkTabPosition, this, &MainWindow::slotCheckTabPosition);
    connect(dialog, &KdenliveSettingsDialog::restartKdenlive, this, &MainWindow::slotRestart);
    connect(dialog, &KdenliveSettingsDialog::updateLibraryFolder, pCore.get(), &Core::updateLibraryPath);
    connect(dialog, &KdenliveSettingsDialog::audioThumbFormatChanged, m_timelineTabs, &TimelineTabs::audioThumbFormatChanged);
    connect(dialog, &KdenliveSettingsDialog::resetView, this, &MainWindow::resetTimelineTracks);
    connect(dialog, &KdenliveSettingsDialog::updateMonitorBg, [&]() { pCore->monitorManager()->updateBgColor(); });
    connect(dialog, &KdenliveSettingsDialog::resetAudioMonitoring, pCore.get(), &Core::resetAudioMonitoring);

    dialog->show();
    if (page != Kdenlive::NoPage) {
        dialog->showPage(page, option);
    }
}

void MainWindow::slotCheckTabPosition()
{
    int pos = tabPosition(Qt::LeftDockWidgetArea);
    if (KdenliveSettings::tabposition() != pos) {
        setTabPosition(Qt::AllDockWidgetAreas, QTabWidget::TabPosition(KdenliveSettings::tabposition()));
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
    cleanRestart(clean);
}

void MainWindow::cleanRestart(bool clean)
{
    m_exitCode = clean ? EXIT_CLEAN_RESTART : EXIT_RESTART;
    QApplication::closeAllWindows();
}

void MainWindow::closeEvent(QCloseEvent *event)
{
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
    Q_EMIT m_timelineTabs->showThumbnailsChanged();
    m_buttonVideoThumbs->setChecked(KdenliveSettings::videothumbnails());
}

void MainWindow::slotSwitchAudioThumbs()
{
    KdenliveSettings::setAudiothumbnails(!KdenliveSettings::audiothumbnails());
    pCore->bin()->checkAudioThumbs();
    Q_EMIT m_timelineTabs->showAudioThumbnailsChanged();
    m_buttonAudioThumbs->setChecked(KdenliveSettings::audiothumbnails());
}

void MainWindow::slotSwitchMarkersComments()
{
    KdenliveSettings::setShowmarkers(!KdenliveSettings::showmarkers());
    Q_EMIT getCurrentTimeline()->controller()->showMarkersChanged();
    m_buttonShowMarkers->setChecked(KdenliveSettings::showmarkers());
}

void MainWindow::slotSwitchSnap()
{
    KdenliveSettings::setSnaptopoints(!KdenliveSettings::snaptopoints());
    m_buttonSnap->setChecked(KdenliveSettings::snaptopoints());
    Q_EMIT getCurrentTimeline()->controller()->snapChanged();
}

void MainWindow::slotShowTimelineTags()
{
    KdenliveSettings::setTagsintimeline(!KdenliveSettings::tagsintimeline());
    m_buttonTimelineTags->setChecked(KdenliveSettings::tagsintimeline());
    // Reset view to update timeline colors
    getCurrentTimeline()->model()->_resetView();
}

void MainWindow::slotDeleteItem()
{
    if (QApplication::focusWidget() != nullptr) {
        for (auto &bin : m_binWidgets) {
            if (bin->isAncestorOf(QApplication::focusWidget())) {
                bin->slotDeleteClip();
                return;
            }
        }
    }
    if (QApplication::focusWidget() != nullptr && pCore->textEditWidget()->isAncestorOf(QApplication::focusWidget())) {
        pCore->textEditWidget()->deleteItem();
    } else {
        QWidget *widget = QApplication::focusWidget();
        while ((widget != nullptr) && widget != this) {
            if (widget == m_effectStackDock) {
                m_assetPanel->deleteCurrentEffect();
                return;
            }
            if (widget == pCore->guidesList()) {
                pCore->guidesList()->removeGuide();
                return;
            }
            widget = widget->parentWidget();
        }

        // effect stack has no focus
        getCurrentTimeline()->controller()->deleteSelectedClips();
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
            slotDeleteGuide();
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
        pCore->bin()->addClipMarker(m_clipMonitor->activeClipId(), {m_clipMonitor->position()});
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

void MainWindow::slotAddGuide()
{
    getCurrentTimeline()->controller()->switchGuide(-1, false, true);
}

void MainWindow::slotInsertSpace()
{
    getCurrentTimeline()->controller()->insertSpace();
}

void MainWindow::slotRemoveSpace()
{
    getCurrentTimeline()->controller()->removeSpace(-1, -1, false);
}

void MainWindow::slotRemoveSpaceInAllTracks()
{
    getCurrentTimeline()->controller()->removeSpace(-1, -1, true);
}

void MainWindow::slotRemoveAllSpacesInTrack()
{
    getCurrentTimeline()->controller()->removeTrackSpaces(-1, -1);
}

void MainWindow::slotRemoveAllClipsInTrack()
{
    getCurrentTimeline()->controller()->removeTrackClips(-1, -1);
}

void MainWindow::slotSeparateAudioChannel()
{
    KdenliveSettings::setDisplayallchannels(!KdenliveSettings::displayallchannels());
    Q_EMIT getCurrentTimeline()->controller()->audioThumbFormatChanged();
    if (m_clipMonitor) {
        m_clipMonitor->refreshAudioThumbs();
    }
}

void MainWindow::slotAutoTrackHeight(bool enable)
{
    KdenliveSettings::setAutotrackheight(enable);
    Q_EMIT pCore->autoTrackHeight(enable);
}

void MainWindow::slotNormalizeAudioChannel()
{
    KdenliveSettings::setNormalizechannels(!KdenliveSettings::normalizechannels());
    Q_EMIT getCurrentTimeline()->controller()->audioThumbNormalizeChanged();
    if (m_clipMonitor) {
        m_clipMonitor->normalizeAudioThumbs();
    }
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

void MainWindow::slotSwitchTrackAudioStream()
{
    getCurrentTimeline()->showTargetMenu();
}

void MainWindow::slotShowTrackRec(bool checked)
{
    if (checked) {
        pCore->mixer()->monitorAudio(getCurrentTimeline()->controller()->activeTrack(), checked);
    } else {
        pCore->mixer()->monitorAudio(pCore->mixer()->recordTrack(), false);
    }
}

void MainWindow::slotSelectTrack()
{
    getCurrentTimeline()->controller()->selectCurrentTrack();
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

void MainWindow::slotUnselectAllTracks()
{
    getCurrentTimeline()->model()->requestClearSelection();
}

void MainWindow::slotEditGuide()
{
    getCurrentTimeline()->controller()->editGuide();
}

void MainWindow::slotSearchGuide()
{
    pCore->guidesList()->filter_line->setFocus();
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
    Q_EMIT getCurrentTimeline()->controller()->guidesLockedChanged();
}

void MainWindow::slotDeleteGuide()
{
    getCurrentTimeline()->controller()->switchGuide(-1, true);
}

void MainWindow::slotDeleteAllGuides()
{
    pCore->currentDoc()->getGuideModel(getCurrentTimeline()->getUuid())->removeAllMarkers();
}

void MainWindow::slotCutTimelineClip()
{
    getCurrentTimeline()->controller()->cutClipUnderCursor();
}

void MainWindow::slotCutTimelineAllClips()
{
    getCurrentTimeline()->controller()->cutAllClipsUnderCursor();
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

void MainWindow::slotExtractClip()
{
    getCurrentTimeline()->controller()->extract();
}

void MainWindow::slotSaveZoneToBin()
{
    getCurrentTimeline()->controller()->saveZone();
}

void MainWindow::slotLiftZone()
{
    getCurrentTimeline()->controller()->extractZone(m_clipMonitor->getZoneInfo(), true);
}

void MainWindow::slotPreviewRender()
{
    if (pCore->currentDoc()) {
        getCurrentTimeline()->controller()->startPreviewRender();
    }
}

void MainWindow::slotStopPreviewRender()
{
    if (pCore->currentDoc()) {
        getCurrentTimeline()->controller()->stopPreviewRender();
    }
}

void MainWindow::slotDefinePreviewRender()
{
    if (pCore->currentDoc()) {
        getCurrentTimeline()->controller()->addPreviewRange(true);
    }
}

void MainWindow::slotRemovePreviewRender()
{
    if (pCore->currentDoc()) {
        getCurrentTimeline()->controller()->addPreviewRange(false);
    }
}

void MainWindow::slotClearPreviewRender(bool resetZones)
{
    if (pCore->currentDoc()) {
        getCurrentTimeline()->controller()->clearPreviewRange(resetZones);
    }
}

void MainWindow::slotSelectTimelineClip()
{
    getCurrentTimeline()->controller()->selectCurrentItem(KdenliveObjectType::TimelineClip, true);
}

void MainWindow::slotSelectTimelineZone()
{
    getCurrentTimeline()->controller()->setZoneToSelection();
}
void MainWindow::slotSelectTimelineTransition()
{
    bool res = getCurrentTimeline()->controller()->selectCurrentItem(KdenliveObjectType::TimelineComposition, true, false, false);
    if (!res) {
        getCurrentTimeline()->controller()->selectCurrentItem(KdenliveObjectType::TimelineMix, true);
    }
}

void MainWindow::slotDeselectTimelineClip()
{
    getCurrentTimeline()->controller()->selectCurrentItem(KdenliveObjectType::TimelineClip, false);
}

void MainWindow::slotDeselectTimelineTransition()
{
    bool res = getCurrentTimeline()->controller()->selectCurrentItem(KdenliveObjectType::TimelineComposition, false, false, false);
    if (!res) {
        getCurrentTimeline()->controller()->selectCurrentItem(KdenliveObjectType::TimelineMix, false);
    }
}

void MainWindow::slotSelectAddTimelineClip()
{
    getCurrentTimeline()->controller()->selectCurrentItem(KdenliveObjectType::TimelineClip, true, true);
}

void MainWindow::slotSelectAddTimelineTransition()
{
    getCurrentTimeline()->controller()->selectCurrentItem(KdenliveObjectType::TimelineComposition, true, true);
}

void MainWindow::slotGroupClips()
{
    getCurrentTimeline()->controller()->groupSelection();
}

void MainWindow::slotUnGroupClips()
{
    getCurrentTimeline()->controller()->unGroupSelection();
}

void MainWindow::slotEditItemDuration()
{
    getCurrentTimeline()->controller()->editItemDuration();
}

void MainWindow::slotAddProjectClip(const QUrl &url, const QString &folderInfo)
{
    pCore->bin()->droppedUrls(QList<QUrl>() << url, folderInfo);
}

void MainWindow::slotAddTextNote(const QString &text)
{
    pCore->projectManager()->slotAddTextNote(text);
}

void MainWindow::slotAddProjectClipList(const QList<QUrl> &urls)
{
    pCore->bin()->droppedUrls(urls);
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
    if (m_assetPanel->effectStackOwner().type == KdenliveObjectType::BinClip) {
        // Pass the command to bin
        pCore->bin()->slotAddEffect({}, {effectId});
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
    slotSetZoom(m_zoomSlider->value() - 1, zoomOnMouse);
    slotShowZoomSliderToolTip();
}

void MainWindow::slotZoomOut(bool zoomOnMouse)
{
    slotSetZoom(m_zoomSlider->value() + 1, zoomOnMouse);
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

void MainWindow::slotSetTool(ToolType::ProjectTool tool)
{
    if (m_activeTool == ToolType::MulticamTool) {
        // End multicam operation
        pCore->monitorManager()->switchMultiTrackView(false);
        pCore->monitorManager()->slotStopMultiTrackMode();
    }
    m_activeTool = tool;
    if (pCore->currentDoc()) {
        showToolMessage();
        getCurrentTimeline()->setTool(tool);
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
    if (mode != TimelineMode::NormalEdit) {
        if (!toolLabel.isEmpty()) {
            toolLabel.append(QStringLiteral(" | "));
        }
        if (mode == TimelineMode::InsertEdit) {
            toolLabel.append(i18n("Insert"));
            m_trimLabel->setStyleSheet(QStringLiteral("QLabel { padding-left: 2; padding-right: 2; background-color :red; }"));
        } else if (mode == TimelineMode::OverwriteEdit) {
            toolLabel.append(i18n("Overwrite"));
            m_trimLabel->setStyleSheet(QStringLiteral("QLabel { padding-left: 2; padding-right: 2; background-color :darkGreen; }"));
        }
    } else {
        m_trimLabel->setStyleSheet(
            QStringLiteral("QLabel { padding-left: 2; padding-right: 2; background-color :%1; }").arg(palette().window().color().name()));
    }
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
        }
        widget = widget->parentWidget();
    }
    getCurrentTimeline()->controller()->copyItem();
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

void MainWindow::slotPasteEffects()
{
    getCurrentTimeline()->controller()->pasteEffects();
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

void MainWindow::raiseBin()
{
    Bin *bin = activeBin();
    if (bin) {
        bin->parentWidget()->setVisible(true);
        bin->parentWidget()->raise();
    }
}

void MainWindow::slotClipInProjectTree()
{
    QList<int> ids = getCurrentTimeline()->controller()->selection();
    if (!ids.isEmpty()) {
        const QString binId = getCurrentTimeline()->controller()->getClipBinId(ids.constFirst());
        // If we have multiple bins, check first if a visible bin contains it
        bool binFound = false;
        if (binCount() > 1) {
            for (auto &bin : m_binWidgets) {
                if (bin->isVisible() && !bin->visibleRegion().isEmpty()) {
                    // Check if clip is a child of this bin
                    if (bin->containsId(binId)) {
                        binFound = true;
                        bin->setFocus();
                        raiseBin();
                    }
                }
            }
        }
        if (!binFound) {
            raiseBin();
        }
        ObjectId id(KdenliveObjectType::TimelineClip, ids.constFirst(), pCore->currentTimelineId());
        int start = pCore->getItemIn(id);
        int duration = pCore->getItemDuration(id);
        int pos = m_projectMonitor->position();
        int itemPos = pCore->getItemPosition(id);
        bool containsPos = (pos >= itemPos && pos < itemPos + duration);
        double speed = pCore->getClipSpeed(id.itemId);
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
                int max = getCurrentTimeline()->controller()->clipMaxDuration(id.itemId);
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
        if (!containsPos) {
            pos = start;
        }
        activeBin()->selectClipById(binId, pos, zone, true);
    }
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

void MainWindow::slotResizeItemStart()
{
    getCurrentTimeline()->controller()->setInPoint(m_activeTool == ToolType::RippleTool);
}

void MainWindow::slotResizeItemEnd()
{
    getCurrentTimeline()->controller()->setOutPoint(m_activeTool == ToolType::RippleTool);
}

#if KXMLGUI_VERSION < QT_VERSION_CHECK(5, 98, 0)
int MainWindow::getNewStuff(const QString &configFile)
{
#if KNEWSTUFF_VERSION > QT_VERSION_CHECK(5, 240, 0)
    KNSWidgets::Dialog dialog(configFile);
#else
    KNS3::QtQuickDialogWrapper dialog(configFile);
#endif
    const QList<KNSCore::EntryInternal> entries = dialog.exec();
    for (const auto &entry : qAsConst(entries)) {
        if (entry.status() == KNS3::Entry::Installed) {
            qCDebug(KDENLIVE_LOG) << "// Installed files: " << entry.installedFiles();
        }
    }
    return entries.size();
}
#endif

#if KXMLGUI_VERSION < QT_VERSION_CHECK(5, 98, 0)
void MainWindow::slotGetNewKeyboardStuff(QComboBox *schemesList)
{
    if (getNewStuff(QStringLiteral(":data/kdenlive_keyboardschemes.knsrc")) > 0) {
        // Refresh keyboard schemes list (schemes list creation code copied from KShortcutSchemesEditor)
        QStringList schemes;
        schemes << QStringLiteral("Default");
        // List files in the shortcuts subdir, each one is a scheme. See KShortcutSchemesHelper::{shortcutSchemeFileName,exportActionCollection}
        const QStringList shortcutsDirs = QStandardPaths::locateAll(
            QStandardPaths::GenericDataLocation, QCoreApplication::applicationName() + QStringLiteral("/shortcuts"), QStandardPaths::LocateDirectory);
        qCDebug(KDENLIVE_LOG) << "shortcut scheme dirs:" << shortcutsDirs;
        for (const QString &dir : shortcutsDirs) {
            for (const QString &file : QDir(dir).entryList(QDir::Files | QDir::NoDotAndDotDot)) {
                qCDebug(KDENLIVE_LOG) << "shortcut scheme file:" << file;
                schemes << file;
            }
        }
        schemesList->clear();
        schemesList->addItems(schemes);
    }
}
#endif

void MainWindow::slotAutoTransition()
{
    // TODO refac
    /*
    if (pCore->projectManager()->currentTimeline()) {
        pCore->projectManager()->currentTimeline()->projectView()->autoTransition();
    }
    */
}

void MainWindow::slotSplitAV()
{
    getCurrentTimeline()->controller()->splitAV();
}

void MainWindow::slotSwitchClip()
{
    getCurrentTimeline()->controller()->switchEnableState();
}

void MainWindow::slotSetAudioAlignReference()
{
    getCurrentTimeline()->controller()->setAudioRef();
}

void MainWindow::slotAlignAudio()
{
    getCurrentTimeline()->controller()->alignAudio();
}

void MainWindow::slotUpdateTimelineView(QAction *action)
{
    int viewMode = action->data().toInt();
    KdenliveSettings::setAudiotracksbelow(viewMode);
    getCurrentTimeline()->model()->_resetView();
}

void MainWindow::slotShowTimeline(bool show)
{
    if (!show) {
        m_timelineState = saveState();
        centralWidget()->setHidden(true);
    } else {
        centralWidget()->setHidden(false);
        restoreState(m_timelineState);
    }
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
    QStringList sortedList;
    for (QAction *a : qAsConst(list)) {
        if (a->objectName().startsWith(QStringLiteral("raise_"))) {
            continue;
        }
        sorted.insert(a->text(), a);
        sortedList << a->text();
    }
    QList<QAction *> orderedList;
    sortedList.sort(Qt::CaseInsensitive);
    for (const QString &text : qAsConst(sortedList)) {
        orderedList << sorted.value(text);
    }
    unplugActionList(QStringLiteral("dock_actions"));
    plugActionList(QStringLiteral("dock_actions"), orderedList);
}

void MainWindow::buildDynamicActions()
{
    KActionCategory *ts = nullptr;
    if (kdenliveCategoryMap.contains(QStringLiteral("clipjobs"))) {
        ts = kdenliveCategoryMap.take(QStringLiteral("clipjobs"));
        delete ts;
    }
    ts = new KActionCategory(i18n("Clip Jobs"), m_extraFactory->actionCollection());

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
        ts->addAction(action->text(), action);
    }

    action = new QAction(QIcon::fromTheme(QStringLiteral("configure")), i18n("Configure Clip Jobs…"), m_extraFactory->actionCollection());
    ts->addAction(action->text(), action);
    connect(action, &QAction::triggered, this, [this]() { manageClipJobs(); });

    kdenliveCategoryMap.insert(QStringLiteral("clipjobs"), ts);

    if (kdenliveCategoryMap.contains(QStringLiteral("transcoderslist"))) {
        ts = kdenliveCategoryMap.take(QStringLiteral("transcoderslist"));
        delete ts;
    }
    if (kdenliveCategoryMap.contains(QStringLiteral("audiotranscoderslist"))) {
        ts = kdenliveCategoryMap.take(QStringLiteral("audiotranscoderslist"));
        delete ts;
    }
    // transcoders
    ts = new KActionCategory(i18n("Transcoders"), m_extraFactory->actionCollection());
    KActionCategory *ats = new KActionCategory(i18n("Extract Audio"), m_extraFactory->actionCollection());
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
            std::vector<QString> ids = pCore->bin()->selectedClipsIds(true);
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
                        TranscodeTask::start(ObjectId(KdenliveObjectType::BinClip, clipId.toInt(), QUuid()), suffix, QString(), args, clipIn, clipOut, false,
                                             clip.get());
                    }
                } else {
                    TranscodeTask::start(ObjectId(KdenliveObjectType::BinClip, clipId.toInt(), QUuid()), QString(), QString(), tData, clipIn, clipOut, false,
                                         clip.get());
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
    if (kdenliveCategoryMap.contains(QStringLiteral("interface"))) {
        guiActions = kdenliveCategoryMap.take(QStringLiteral("interface"));
        delete guiActions;
    }
    guiActions = new KActionCategory(i18n("Interface"), actionCollection());
    QAction *showTimeline = new QAction(i18n("Timeline"), this);
    showTimeline->setCheckable(true);
    showTimeline->setChecked(true);
    connect(showTimeline, &QAction::triggered, this, &MainWindow::slotShowTimeline);
    guiActions->addAction(showTimeline->text(), showTimeline);
    actionCollection()->addAction(showTimeline->text(), showTimeline);

    QList<QDockWidget *> docks = findChildren<QDockWidget *>();
    for (auto dock : qAsConst(docks)) {
        QAction *dockInformations = dock->toggleViewAction();
        if (!dockInformations) {
            continue;
        }
        dockInformations->setChecked(!dock->isHidden());
        guiActions->addAction(dockInformations->text(), dockInformations);
        QAction *action = new QAction(i18n("Raise %1", dockInformations->text()), this);
        connect(action, &QAction::triggered, this, [dock]() {
            dock->raise();
            dock->setFocus();
        });
        addAction("raise_" + dock->objectName(), action, {}, guiActions);
    }
    kdenliveCategoryMap.insert(QStringLiteral("interface"), guiActions);
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
    ClipTranscode *d = new ClipTranscode(urls, params, QStringList(), desc, pCore->bin()->getCurrentFolder());
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
    ClipTranscode *d = new ClipTranscode(urls, params, QStringList(), desc, pCore->bin()->getCurrentFolder());
    connect(d, &ClipTranscode::addClip, [&, binId, sourceProps](const QUrl &url, const QString & /*folderInfo*/) {
        QMap<QString, QString> newProps;
        newProps.insert(QStringLiteral("resource"), url.toLocalFile());
        newProps.insert(QStringLiteral("kdenlive:originalurl"), url.toLocalFile());
        newProps.insert(QStringLiteral("kdenlive:clipname"), url.fileName());
        newProps.insert(QStringLiteral("kdenlive:proxy"), QStringLiteral("-"));
        newProps.insert(QStringLiteral("_fullreload"), QStringLiteral("1"));
        QMetaObject::invokeMethod(pCore->bin(), "slotEditClipCommand", Qt::QueuedConnection, Q_ARG(QString, binId), Q_ARG(stringMap, sourceProps),
                                  Q_ARG(stringMap, newProps));
    });
    d->exec();
    if (checkProfile) {
        pCore->bin()->slotCheckProfile(binId);
    }
}

void MainWindow::slotTranscodeClip()
{
    const QString dialogFilter = ClipCreationDialog::getExtensionsFilter(QStringList() << i18n("All Files") + QStringLiteral(" (*)"));
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
        QDBusInterface kdeShutdown(QStringLiteral("org.kde.Shutdown"), QStringLiteral("/Shutdown"), QStringLiteral("org.kde.Shutdown"));
        kdeShutdown.call(QStringLiteral("logoutAndShutdown"));
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
        getCurrentTimeline()->setFocus();
    } else {
        pCore->bin()->focusBinView();
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

void MainWindow::slotSetMonitorGamma(int gamma)
{
    KdenliveSettings::setMonitor_gamma(gamma);
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
    pCore->projectItemModel()->requestAddBinSubClip(id, info.x(), info.y()-1, {}, m_clipMonitor->activeClipId());
}

void MainWindow::slotMonitorRequestRenderFrame(bool request)
{
    if (request) {
        m_projectMonitor->sendFrameForAnalysis(true);
        return;
    }
    for (int i = 0; i < m_gfxScopesList.count(); ++i) {
        if (m_gfxScopesList.at(i)->isVisible() && tabifiedDockWidgets(m_gfxScopesList.at(i)).isEmpty() &&
            static_cast<AbstractGfxScopeWidget *>(m_gfxScopesList.at(i)->widget())->autoRefreshEnabled()) {
            request = true;
            break;
        }
    }

#ifdef DEBUG_MAINW
    qCDebug(KDENLIVE_LOG) << "Any scope accepting new frames? " << request;
#endif
    if (!request) {
        m_projectMonitor->sendFrameForAnalysis(false);
    }
}

void MainWindow::slotUpdateProxySettings()
{
    KdenliveDoc *project = pCore->currentDoc();
    if (m_renderWidget) {
        m_renderWidget->updateProxyConfig(project->useProxy());
    }
    pCore->bin()->refreshProxySettings();
}

void MainWindow::slotArchiveProject()
{
    KdenliveDoc *doc = pCore->currentDoc();
    pCore->projectManager()->prepareSave();
    QString sceneData = pCore->projectManager()->projectSceneList(doc->url().adjusted(QUrl::RemoveFilename | QUrl::StripTrailingSlash).toLocalFile());
    if (sceneData.isEmpty()) {
        KMessageBox::error(this, i18n("Project file could not be saved for archiving."));
        return;
    }
    QPointer<ArchiveWidget> d(new ArchiveWidget(doc->url().fileName(), sceneData, getCurrentTimeline()->controller()->extractCompositionLumas(),
                                                getCurrentTimeline()->controller()->extractExternalEffectFiles(), this));
    if (d->exec() != 0) {
        m_messageLabel->setMessage(i18n("Archiving project"), OperationCompletedMessage);
    }
}

void MainWindow::slotDownloadResources()
{
    QString currentFolder;
    if (pCore->currentDoc()) {
        currentFolder = pCore->currentDoc()->projectDataFolder();
    } else {
        currentFolder = KdenliveSettings::defaultprojectfolder();
    }
    m_onlineResourcesDock->show();
    m_onlineResourcesDock->raise();
    ;
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
    if (ev->modifiers() != Qt::KeypadModifier) {
        seq = QKeySequence(ev->key() + static_cast<int>(ev->modifiers()));
    } else {
        seq = QKeySequence(ev->key());
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

QDockWidget *MainWindow::addDock(const QString &title, const QString &objectName, QWidget *widget, Qt::DockWidgetArea area)
{
    QDockWidget *dockWidget = new QDockWidget(title, this);
    dockWidget->setObjectName(objectName);
    dockWidget->setWidget(widget);
    addDockWidget(area, dockWidget);

    // Add action to raise and focus the Dock (e.g. with a shortcut)
    /*QAction *action = new QAction(i18n("Raise %1", title), this);
    connect(action, &QAction::triggered, this, [dockWidget](){
        dockWidget->raise();
        dockWidget->setFocus();
    });
    addAction("raise_" + objectName, action, {});*/
    return dockWidget;
}

bool MainWindow::isMixedTabbed() const
{
    return !tabifiedDockWidgets(m_mixerDock).isEmpty();
}

void MainWindow::slotUpdateMonitorOverlays(int id, int code)
{
    QMenu *monitorOverlay = static_cast<QMenu *>(factory()->container(QStringLiteral("monitor_config_overlay"), this));
    if (!monitorOverlay) {
        return;
    }
    QList<QAction *> actions = monitorOverlay->actions();
    for (QAction *ac : qAsConst(actions)) {
        int mid = ac->data().toInt();
        if (mid == 0x010 || mid == 0x040) {
            ac->setVisible(id == Kdenlive::ClipMonitor);
        }
        ac->setChecked(code & mid);
    }
}

void MainWindow::slotChangeStyle(QAction *a)
{
    QString style = a->data().toString();
    KdenliveSettings::setWidgetstyle(style);
    doChangeStyle();
    // Monitor refresh is necessary
    raiseMonitor(pCore->monitorManager()->isActive(Kdenlive::ClipMonitor));
}

void MainWindow::raiseMonitor(bool clipMonitor)
{
    if (clipMonitor) {
        m_clipMonitorDock->show();
        m_clipMonitorDock->raise();
    } else {
        m_projectMonitorDock->show();
        m_projectMonitorDock->raise();
    }
}

void MainWindow::doChangeStyle()
{
    QString newStyle = KdenliveSettings::widgetstyle();
    if (newStyle.isEmpty() || newStyle == QStringLiteral("Default")) {
        newStyle = defaultStyle("Breeze");
    }
    QApplication::setStyle(QStyleFactory::create(newStyle));
}

bool MainWindow::isTabbedWith(QDockWidget *widget, const QString &otherWidget)
{
    QList<QDockWidget *> tabbed = tabifiedDockWidgets(widget);
    for (auto tab : qAsConst(tabbed)) {
        if (tab->objectName() == otherWidget) {
            return true;
        }
    }
    return false;
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
    KEditToolBar::setGlobalDefaultToolBar("timelineToolBar");
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

void MainWindow::forceIconSet(bool force)
{
    KdenliveSettings::setForce_breeze(force);
    if (force) {
        // Check current color theme
        QColor background = qApp->palette().window().color();
        bool useDarkIcons = background.value() < 100;
        KdenliveSettings::setUse_dark_breeze(useDarkIcons);
    }
    if (KMessageBox::warningContinueCancel(this, i18n("Kdenlive needs to be restarted to apply the icon theme change. Restart now?")) ==
        KMessageBox::Continue) {
        slotRestart();
    }
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

void MainWindow::closeTimelineTab(const QUuid uuid)
{
    m_timelineTabs->closeTimelineTab(uuid);
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

void MainWindow::slotRemapItemTime()
{
    TimelineWidget *current = getCurrentTimeline();
    if (current) {
        current->controller()->remapItemTime(-1);
    }
}

void MainWindow::slotEditItemSpeed()
{
    TimelineWidget *current = getCurrentTimeline();
    if (current) {
        current->controller()->changeItemSpeed(-1, -1);
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
    getCurrentTimeline()->controller()->grabCurrent();
}

void MainWindow::slotCollapse()
{
    if ((QApplication::focusWidget() != nullptr) && (QApplication::focusWidget()->parentWidget() != nullptr) &&
        QApplication::focusWidget()->parentWidget() == pCore->bin()) {
        // Bin expand/collapse?

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

void MainWindow::slotExpandClip()
{
    getCurrentTimeline()->controller()->expandActiveClip();
}

bool MainWindow::timelineVisible() const
{
    return !centralWidget()->isHidden();
}

void MainWindow::slotActivateAudioTrackSequence()
{
    auto *action = qobject_cast<QAction *>(sender());
    const QList<int> trackIds = getCurrentTimeline()->model()->getTracksIds(true);
    int trackPos = qBound(0, action->data().toInt(), trackIds.count() - 1);
    int tid = trackIds.at(trackPos);
    getCurrentTimeline()->controller()->setActiveTrack(tid);
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

void MainWindow::slotActivateTarget()
{
    auto *action = qobject_cast<QAction *>(sender());
    if (action) {
        int ix = action->data().toInt();
        getCurrentTimeline()->controller()->assignCurrentTarget(ix);
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
    getCurrentTimeline()->model()->getSubtitleModel()->addSubtitle(-1, text);
}

void MainWindow::slotDisableSubtitle()
{
    getCurrentTimeline()->controller()->switchSubtitleDisable();
}

void MainWindow::slotLockSubtitle()
{
    getCurrentTimeline()->controller()->switchSubtitleLock();
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

void MainWindow::slotExportSubtitle()
{
    if (!getCurrentTimeline()->hasSubtitles()) {
        pCore->displayMessage(i18n("No subtitles in current project"), ErrorMessage);
        return;
    }
    getCurrentTimeline()->controller()->exportSubtitle();
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
    QString debuginfo = QStringLiteral("Kdenlive: %1\n").arg(KAboutData::applicationData().version());
    QString packageType = pCore->packageType();
    debuginfo.append(QStringLiteral("Package Type: %1\n").arg(packageType.isEmpty() ? QStringLiteral("Unknown/Default") : packageType));
    debuginfo.append(QStringLiteral("MLT: %1\n").arg(mlt_version_get_string()));
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
            if (pCore->isMediaMonitoring()) {
                slotShowTrackRec(false);
                return true;
            }
            if (pCore->isMediaCapturing()) {
                pCore->switchCapture();
                return true;
            }
            if (m_activeTool != ToolType::SelectTool && m_commandStack->activeStack()->canUndo()) {
                m_buttonSelectTool->trigger();
                return true;
            } else {
                if (m_commandStack->activeStack()->canUndo()) {
                    // Don't call selection clear if a drag operation is in progress
                    getCurrentTimeline()->model()->requestClearSelection();
                }
                return true;
            }
        }
        break;
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
    updateDockMenu();
    loadDockActions();
}

void MainWindow::addBin(Bin *bin, const QString &binName)
{
    connect(bin, &Bin::findInTimeline, this, &MainWindow::slotClipInTimeline, Qt::DirectConnection);
    connect(bin, &Bin::setupTargets, this, [&](bool hasVideo, QMap<int, QString> audioStreams) {
        if (getCurrentTimeline() && getCurrentTimeline()->controller()) {
            getCurrentTimeline()->controller()->setTargetTracks(hasVideo, audioStreams);
        }
    });
    if (!m_binWidgets.isEmpty()) {
        // This is a secondary bin widget
        int ix = binCount() + 1;
        QDockWidget *binDock = addDock(binName.isEmpty() ? i18n("Project Bin %1", ix) : binName, QString("project_bin_%1").arg(ix), bin);
        bin->setupGeneratorMenu();
        connect(bin, &Bin::requestShowEffectStack, m_assetPanel, &AssetPanel::showEffectStack);
        connect(bin, &Bin::requestShowClipProperties, getBin(), &Bin::showClipProperties);
        connect(bin, &Bin::requestBinClose, this, [this, binDock]() { Q_EMIT removeBinDock(binDock->objectName()); });
        tabifyDockWidget(m_projectBinDock, binDock);
        // Disable title bar since it is tabbed
        binDock->setTitleBarWidget(new QWidget);
        // Update dock list
        updateDockMenu();
        loadDockActions();
        binDock->show();
        binDock->raise();
    }
    m_binWidgets << bin;
}

void MainWindow::tabifyBins()
{
    QList<QDockWidget *> docks = findChildren<QDockWidget *>();
    for (auto dock : qAsConst(docks)) {
        if (dock->objectName().startsWith(QLatin1String("project_bin_"))) {
            tabifyDockWidget(m_projectBinDock, dock);
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

void MainWindow::processRestoreState(const QByteArray &state)
{
    // On Wayland, restoreState crashes when quickly hiding/showing/hiding a monitor in restoreState, so hide before restoring
    m_projectMonitorDock->close();
    m_clipMonitorDock->close();
    restoreState(state);
}

void MainWindow::checkMaxCacheSize()
{
    if (KdenliveSettings::lastCacheCheck().daysTo(QDateTime::currentDateTime()) < 14) {
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
                            connect(updateAction, &QAction::triggered, this,
                                    []() { QDesktopServices::openUrl(QUrl(QStringLiteral("https://kdenlive.org/download"))); });
                            QAction *abortAction = new QAction(i18n("Never check again"), this);
                            connect(abortAction, &QAction::triggered, this, []() { KdenliveSettings::setCheckForUpdate(false); });
                            if (days > 360) {
                                pCore->displayBinMessage(i18n("Your Kdenlive version is older than 1 year, we strongly encourage you to upgrade"),
                                                         KMessageWidget::Warning, {updateAction, abortAction}, true, BinMessage::BinCategory::UpdateMessage);
                            } else {
                                pCore->displayBinMessage(i18n("Your Kdenlive version is older than 6 months, we encourage you to upgrade"),
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
    KIO::filesize_t total = 0;
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
        KIO::DirectorySizeJob *job = KIO::directorySize(QUrl::fromLocalFile(dir.absolutePath()));
        job->exec();
        total += job->totalSize();
    }
    while (!toRemove.isEmpty()) {
        QDir dir = toRemove.takeFirst();
        KIO::DirectorySizeJob *job = KIO::directorySize(QUrl::fromLocalFile(dir.absolutePath()));
        job->exec();
        total -= job->totalSize();
    }
    if (total > KIO::filesize_t(1048576) * KdenliveSettings::maxcachesize()) {
        slotManageCache();
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

TimelineWidget *MainWindow::openTimeline(const QUuid &uuid, const QString &tabName, std::shared_ptr<TimelineItemModel> timelineModel)
{
    // Create a new timeline tab
    KdenliveDoc *project = pCore->currentDoc();
    TimelineWidget *timeline = m_timelineTabs->addTimeline(uuid, tabName, timelineModel, pCore->monitorManager()->projectMonitor()->getControllerProxy());
    slotSetZoom(project->zoom(uuid).x(), false);
    getCurrentTimeline()->controller()->setZone(project->zone(uuid), false);
    getCurrentTimeline()->controller()->setScrollPos(project->getSequenceProperty(uuid, QStringLiteral("scrollPos")).toInt());
    m_projectMonitor->slotLoadClipZone(project->zone(uuid));
    return timeline;
}

bool MainWindow::raiseTimeline(const QUuid &uuid)
{
    return m_timelineTabs->raiseTimeline(uuid);
}

void MainWindow::connectTimeline()
{
    qDebug() << "::::::::::: connecting timeline: " << getCurrentTimeline()->getUuid() << ", DUR: " << getCurrentTimeline()->controller()->duration();
    if (!getCurrentTimeline()->model()) {
        qDebug() << "::::::::::: TIMELINE HAS NO MODEL";
    } else {
        getCurrentTimeline()->model()->rebuildMixer();
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
    getCurrentTimeline()->controller()->clipActions = kdenliveCategoryMap.value(QStringLiteral("timelineselection"))->actions();
    connect(getCurrentTimeline()->controller(), &TimelineController::durationChanged, pCore->projectManager(), &ProjectManager::adjustProjectDuration);
    connect(pCore->bin(), &Bin::processDragEnd, getCurrentTimeline(), &TimelineWidget::endDrag);
    pCore->monitorManager()->activateMonitor(Kdenlive::ProjectMonitor);

    KdenliveDoc *project = pCore->currentDoc();
    QSignalBlocker blocker(m_zoomSlider);
    m_zoomSlider->setValue(project->zoom(uuid).x());
    int position = project->getSequenceProperty(uuid, QStringLiteral("position"), QString::number(0)).toInt();
    pCore->monitorManager()->projectMonitor()->adjustRulerSize(getCurrentTimeline()->model()->duration() - 1, project->getFilteredGuideModel(uuid));
    pCore->monitorManager()->projectMonitor()->loadZone(getCurrentTimeline()->controller()->zoneIn(), getCurrentTimeline()->controller()->zoneOut());
    pCore->monitorManager()->projectMonitor()->setProducer(getCurrentTimeline()->model()->producer(), position);
    connect(project, &KdenliveDoc::docModified, this, &MainWindow::slotUpdateDocumentState);
    slotUpdateDocumentState(project->isModified());

    // Timeline preview
    QAction *previewRender = actionCollection()->action(QStringLiteral("prerender_timeline_zone"));
    if (previewRender) {
        previewRender->setEnabled(true);
    }
    QAction *disablePreview = actionCollection()->action(QStringLiteral("disable_preview"));
    if (getCurrentTimeline()->model()->hasTimelinePreview()) {
        disablePreview->setEnabled(true);
    } else {
        disablePreview->setEnabled(false);
    }
    disablePreview->blockSignals(true);
    disablePreview->setChecked(false);
    disablePreview->blockSignals(false);

    // Ensure the active timeline has an opaque black background for compositing
    getCurrentTimeline()->model()->makeTransparentBg(false);

    // Audio record actions
    connect(pCore.get(), &Core::finalizeRecording, getCurrentTimeline()->controller(), &TimelineController::finishRecording);
    connect(pCore.get(), &Core::recordAudio, getCurrentTimeline()->controller(), &TimelineController::switchRecording);

    // switch to active subtitle model
    pCore->subtitleWidget()->setModel(getCurrentTimeline()->model()->getSubtitleModel());
    bool hasSubtitleModel = getCurrentTimeline()->hasSubtitles();
    Q_EMIT getCurrentTimeline()->controller()->subtitlesLockedChanged();
    Q_EMIT getCurrentTimeline()->controller()->subtitlesDisabledChanged();
    bool showSubs = project->getSequenceProperty(uuid, QStringLiteral("hidesubtitle")).toInt() == 0;
    KdenliveSettings::setShowSubtitles(showSubs && hasSubtitleModel);
    getCurrentTimeline()->connectSubtitleModel(hasSubtitleModel);
    m_buttonSubtitleEditTool->setChecked(showSubs && hasSubtitleModel);
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
        m_renderWidget->updateDocumentPath();
        m_renderWidget->showRenderDuration();
    }
}

void MainWindow::disconnectTimeline(TimelineWidget *timeline)
{
    // Save current tab timeline position
    if (pCore->currentDoc()) {
        // pCore->currentDoc()->position = pCore->getTimelinePosition();
        //  disconnect(pCore->currentDoc(), &KdenliveDoc::docModified, this, &MainWindow::slotUpdateDocumentState);
        // qDebug()<<"=== SETTING POSITION  FOR DOC: "<<pCore->currentDoc()->position<<" / "<<pCore->currentDoc()->uuid;
    }
    // Ensure the active timeline has an transparent black background for embedded compositing
    timeline->model()->makeTransparentBg(true);
    disconnect(timeline->controller(), &TimelineController::durationChanged, pCore->projectManager(), &ProjectManager::adjustProjectDuration);
    disconnect(m_projectMonitor, &Monitor::multitrackView, timeline->controller(), &TimelineController::slotMultitrackView);
    disconnect(m_projectMonitor, &Monitor::activateTrack, timeline->controller(), &TimelineController::activateTrackAndSelect);
    disconnect(pCore->library(), &LibraryWidget::saveTimelineSelection, timeline->controller(), &TimelineController::saveTimelineSelection);
    timeline->controller()->clipActions = QList<QAction *>();
    disconnect(pCore->bin(), &Bin::processDragEnd, timeline, &TimelineWidget::endDrag);
    pCore->monitorManager()->projectMonitor()->setProducer(nullptr, -2);
    // Audio record actions
    disconnect(pCore.get(), &Core::finalizeRecording, timeline->controller(), &TimelineController::finishRecording);
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
    if (pCore->packageType() == QStringLiteral("appimage")) {
        qDebug() << "::::: LAUNCHING APPIMAGE BROWSER.........";
        QProcessEnvironment env = getCleanEnvironement();
        QProcess process;
        process.setProcessEnvironment(env);
        QString openPath = QStandardPaths::findExecutable(QStringLiteral("xdg-open"));
        qDebug() << "------------\nFOUND OPEN PATH: " << openPath;
        process.setProgram(openPath.isEmpty() ? QStringLiteral("xdg-open") : openPath);
        process.setArguments({QStringLiteral("https://docs.kdenlive.org")});
        process.startDetached();
    } else {
        QDesktopServices::openUrl(QUrl(QStringLiteral("https://docs.kdenlive.org")));
    }
}

void MainWindow::slotCreateSequenceFromSelection()
{
    pCore->projectManager()->slotCreateSequenceFromSelection();
}

#ifdef DEBUG_MAINW
#undef DEBUG_MAINW
#endif
