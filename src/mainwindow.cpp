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

#include "mainwindow.h"
#include "mainwindowadaptor.h"
#include "core.h"
#include "bin/projectclip.h"
#include "bin/generators/generators.h"
#include "library/librarywidget.h"
#include "monitor/scopes/audiographspectrum.h"
#include "mltcontroller/clipcontroller.h"
#include "kdenlivesettings.h"
#include "dialogs/kdenlivesettingsdialog.h"
#include "dialogs/clipcreationdialog.h"
#include "effectslist/initeffects.h"
#include "project/dialogs/projectsettings.h"
#include "project/clipmanager.h"
#include "monitor/monitor.h"
#include "monitor/recmonitor.h"
#include "monitor/monitormanager.h"
#include "doc/kdenlivedoc.h"
#include "timeline/timeline.h"
#include "timeline/track.h"
#include "timeline/customtrackview.h"
#include "effectslist/effectslistview.h"
#include "effectslist/effectbasket.h"
#include "effectstack/effectstackview2.h"
#include "project/transitionsettings.h"
#include "mltcontroller/bincontroller.h"
#include "mltcontroller/producerqueue.h"
#include "dialogs/renderwidget.h"
#include "renderer.h"
#include "dialogs/wizard.h"
#include "project/projectcommands.h"
#include "titler/titlewidget.h"
#include "timeline/markerdialog.h"
#include "timeline/clipitem.h"
#include "project/cliptranscode.h"
#include "scopes/scopemanager.h"
#include "project/dialogs/archivewidget.h"
#include "utils/resourcewidget.h"
#include "layoutmanagement.h"
#include "hidetitlebars.h"
#include "mltconnection.h"
#include "project/projectmanager.h"
#include "timeline/timelinesearch.h"
#include <config-kdenlive.h>
#include "utils/thememanager.h"
#include "utils/progressbutton.h"
#include "effectslist/effectslistwidget.h"

#include "utils/KoIconUtils.h"
#include "project/dialogs/temporarydata.h"
#ifdef USE_JOGSHUTTLE
#include "jogshuttle/jogmanager.h"
#endif

#include <KActionCollection>
#include <KActionCategory>
#include <KActionMenu>
#include <KStandardAction>
#include <KShortcutsDialog>
#include <KMessageBox>
#include <KConfigDialog>
#include <KXMLGUIFactory>
#include <KIconTheme>
#include <KColorSchemeManager>
#include <KRecentDirs>
#include <KUrlRequesterDialog>
#include <ktogglefullscreenaction.h>
#include <KNotifyConfigWidget>
#include <kns3/downloaddialog.h>
#include <kns3/knewstuffaction.h>
#include <KToolBar>
#include <KColorScheme>
#include <KEditToolBar>
#include <KDualAction>
#include <klocalizedstring.h>

#include <QAction>
#include "kdenlive_debug.h"
#include <QStatusBar>
#include <QTemporaryFile>
#include <QMenu>
#include <QUndoGroup>
#include <QFileDialog>
#include <QStyleFactory>
#include <QMenuBar>

#include <stdlib.h>
#include <QStandardPaths>
#include <KConfigGroup>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QVBoxLayout>
#include <QScreen>

static const char version[] = KDENLIVE_VERSION;
namespace Mlt
{
class Producer;
}

EffectsList MainWindow::videoEffects;
EffectsList MainWindow::audioEffects;
EffectsList MainWindow::customEffects;
EffectsList MainWindow::transitions;

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

MainWindow::MainWindow(QWidget *parent) :
    KXmlGuiWindow(parent),
    m_timelineArea(nullptr),
    m_stopmotion(nullptr),
    m_effectStack(nullptr),
    m_exitCode(EXIT_SUCCESS),
    m_effectList(nullptr),
    m_transitionList(nullptr),
    m_clipMonitor(nullptr),
    m_projectMonitor(nullptr),
    m_recMonitor(nullptr),
    m_renderWidget(nullptr),
    m_messageLabel(nullptr),
    m_themeInitialized(false),
    m_isDarkTheme(false)
{
}

void MainWindow::init(const QString &MltPath, const QUrl &Url, const QString &clipsToLoad)
{

    // Widget themes for non KDE users
    KActionMenu *stylesAction = new KActionMenu(i18n("Style"), this);
    QActionGroup *stylesGroup = new QActionGroup(stylesAction);

    // GTK theme does not work well with Kdenlive, and does not support color theming, so avoid it
    QStringList availableStyles = QStyleFactory::keys();
    QString desktopStyle = QApplication::style()->objectName();
    if (KdenliveSettings::widgetstyle().isEmpty()) {
        // First run
        QStringList incompatibleStyles;
        incompatibleStyles << QStringLiteral("GTK+") << QStringLiteral("windowsvista") << QStringLiteral("windowsxp");
        if (incompatibleStyles.contains(desktopStyle, Qt::CaseInsensitive)) {
            if (availableStyles.contains(QStringLiteral("breeze"), Qt::CaseInsensitive)) {
                // Auto switch to Breeze theme
                KdenliveSettings::setWidgetstyle(QStringLiteral("Breeze"));
            } else if (availableStyles.contains(QStringLiteral("fusion"), Qt::CaseInsensitive)) {
                KdenliveSettings::setWidgetstyle(QStringLiteral("Fusion"));
            }
        }
    }

    // Add default style action
    QAction *defaultStyle = new QAction(i18n("Default"), stylesGroup);
    defaultStyle->setCheckable(true);
    stylesAction->addAction(defaultStyle);
    if (KdenliveSettings::widgetstyle().isEmpty()) {
        defaultStyle->setChecked(true);
    }

    foreach (const QString &style, availableStyles) {
        QAction *a = new QAction(style, stylesGroup);
        a->setCheckable(true);
        a->setData(style);
        if (KdenliveSettings::widgetstyle() == style) {
            a->setChecked(true);
        }
        stylesAction->addAction(a);
    }
    connect(stylesGroup, &QActionGroup::triggered, this, &MainWindow::slotChangeStyle);

    // Color schemes
    KActionMenu *themeAction = new KActionMenu(i18n("Theme"), this);
    ThemeManager::instance()->setThemeMenuAction(themeAction);
    ThemeManager::instance()->setCurrentTheme(KdenliveSettings::colortheme());
    connect(ThemeManager::instance(), &ThemeManager::signalThemeChanged, this, &MainWindow::slotThemeChanged, Qt::DirectConnection);

    if (!KdenliveSettings::widgetstyle().isEmpty() && QString::compare(desktopStyle, KdenliveSettings::widgetstyle(), Qt::CaseInsensitive) != 0) {
        // User wants a custom widget style, init
        doChangeStyle();
    } else {
        ThemeManager::instance()->slotChangePalette();
    }
    //QIcon::setThemeSearchPaths(QStringList() <<QStringLiteral(":/icons/"));

    new RenderingAdaptor(this);
    QString defaultProfile = KdenliveSettings::default_profile();
    KdenliveSettings::setCurrent_profile(defaultProfile.isEmpty() ? ProjectManager::getDefaultProjectFormat() : defaultProfile);

    // If using a custom profile, make sure the file exists or fallback to default
    if (KdenliveSettings::current_profile().startsWith(QLatin1Char('/')) && !QFile::exists(KdenliveSettings::current_profile())) {
        KMessageBox::sorry(this, i18n("Cannot find your default profile, switching to ATSC 1080p 25"));
        KdenliveSettings::setCurrent_profile(QStringLiteral("atsc_1080p_25"));
        KdenliveSettings::setDefault_profile(QStringLiteral("atsc_1080p_25"));
    }
    m_commandStack = new QUndoGroup(this);
    m_timelineArea = new QTabWidget(this);
    //m_timelineArea->setTabReorderingEnabled(true);
    m_timelineArea->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    m_timelineArea->setMinimumHeight(100);
    // Hide tabbar
    QTabBar *bar = m_timelineArea->findChild<QTabBar *>();
    bar->setHidden(true);

    m_gpuAllowed = initEffects::parseEffectFiles(pCore->getMltRepository());
    //initEffects::parseCustomEffectsFile();

    m_shortcutRemoveFocus = new QShortcut(QKeySequence(QStringLiteral("Esc")), this);
    connect(m_shortcutRemoveFocus, &QShortcut::activated, this, &MainWindow::slotRemoveFocus);

    /// Add Widgets
    setDockOptions(dockOptions() | QMainWindow::AllowNestedDocks | QMainWindow::AllowTabbedDocks);
#if (QT_VERSION >= QT_VERSION_CHECK(5, 6, 0))
    setDockOptions(dockOptions() | QMainWindow::GroupedDragging);
#endif
    setTabPosition(Qt::AllDockWidgetAreas, (QTabWidget::TabPosition) KdenliveSettings::tabposition());
    m_timelineToolBar = toolBar(QStringLiteral("timelineToolBar"));
    m_timelineToolBarContainer = new QWidget(this);
    QVBoxLayout *ctnLay = new QVBoxLayout;
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
    ctnLay->addWidget(m_timelineArea);
    setCentralWidget(m_timelineToolBarContainer);
    setupActions();

    QDockWidget *libraryDock = addDock(i18n("Library"), QStringLiteral("library"), pCore->library());

    m_clipMonitor = new Monitor(Kdenlive::ClipMonitor, pCore->monitorManager(), this);
    pCore->bin()->setMonitor(m_clipMonitor);
    connect(m_clipMonitor, &Monitor::showConfigDialog, this, &MainWindow::slotPreferences);
    connect(m_clipMonitor, &Monitor::addMarker, this, &MainWindow::slotAddMarkerGuideQuickly);
    connect(m_clipMonitor, &Monitor::deleteMarker, this, &MainWindow::slotDeleteClipMarker);
    connect(m_clipMonitor, &Monitor::seekToPreviousSnap, this, &MainWindow::slotSnapRewind);
    connect(m_clipMonitor, &Monitor::seekToNextSnap, this, &MainWindow::slotSnapForward);

    connect(pCore->bin(), &Bin::clipNeedsReload, this, &MainWindow::slotUpdateClip);
    connect(pCore->bin(), &Bin::findInTimeline, this, &MainWindow::slotClipInTimeline);

    //TODO deprecated, replace with Bin methods if necessary
    /*connect(m_projectList, SIGNAL(loadingIsOver()), this, SLOT(slotElapsedTime()));
    connect(m_projectList, SIGNAL(updateRenderStatus()), this, SLOT(slotCheckRenderStatus()));
    connect(m_projectList, SIGNAL(updateProfile(QString)), this, SLOT(slotUpdateProjectProfile(QString)));
    connect(m_projectList, SIGNAL(refreshClip(QString,bool)), pCore->monitorManager(), SLOT(slotRefreshCurrentMonitor(QString)));
    connect(m_clipMonitor, SIGNAL(zoneUpdated(QPoint)), m_projectList, SLOT(slotUpdateClipCut(QPoint)));*/
    connect(m_clipMonitor, &Monitor::extractZone, pCore->bin(), &Bin::slotStartCutJob);
    connect(m_clipMonitor, &Monitor::passKeyPress, this, &MainWindow::triggerKey);

    m_projectMonitor = new Monitor(Kdenlive::ProjectMonitor, pCore->monitorManager(), this);
    connect(m_projectMonitor, &Monitor::passKeyPress, this, &MainWindow::triggerKey);
    connect(m_projectMonitor, &Monitor::addMarker, this, &MainWindow::slotAddMarkerGuideQuickly);
    connect(m_projectMonitor, &Monitor::deleteMarker, this, &MainWindow::slotDeleteClipMarker);
    connect(m_projectMonitor, &Monitor::seekToPreviousSnap, this, &MainWindow::slotSnapRewind);
    connect(m_projectMonitor, &Monitor::seekToNextSnap, this, &MainWindow::slotSnapForward);
    connect(m_loopClip, &QAction::triggered, m_projectMonitor, &Monitor::slotLoopClip);
    connect(m_projectMonitor, SIGNAL(updateGuide(int, QString)), this, SLOT(slotEditGuide(int, QString)));

    /*
        //TODO disabled until ported to qml
        m_recMonitor = new RecMonitor(Kdenlive::RecordMonitor, pCore->monitorManager(), this);
        connect(m_recMonitor, SIGNAL(addProjectClip(QUrl)), this, SLOT(slotAddProjectClip(QUrl)));
        connect(m_recMonitor, SIGNAL(addProjectClipList(QList<QUrl>)), this, SLOT(slotAddProjectClipList(QList<QUrl>)));

    */
    pCore->monitorManager()->initMonitors(m_clipMonitor, m_projectMonitor, m_recMonitor);
    connect(m_clipMonitor, SIGNAL(addMasterEffect(QString, QDomElement)), pCore->bin(), SLOT(slotEffectDropped(QString, QDomElement)));

    // Audio spectrum scope
    m_audioSpectrum = new AudioGraphSpectrum(pCore->monitorManager());
    QDockWidget *spectrumDock = addDock(i18n("Audio Spectrum"), QStringLiteral("audiospectrum"), m_audioSpectrum);
    connect(this, &MainWindow::reloadTheme, m_audioSpectrum, &AudioGraphSpectrum::refreshPixmap);
    // Close library and audiospectrum on first run
    libraryDock->close();
    spectrumDock->close();

    m_projectBinDock = addDock(i18n("Project Bin"), QStringLiteral("project_bin"), pCore->bin());
    m_effectStack = new EffectStackView2(m_projectMonitor, this);

    connect(m_effectStack, &EffectStackView2::startFilterJob, pCore->bin(), &Bin::slotStartFilterJob);
    connect(pCore->bin(), &Bin::masterClipSelected, m_effectStack, &EffectStackView2::slotMasterClipItemSelected);
    connect(pCore->bin(), &Bin::masterClipUpdated, m_effectStack, &EffectStackView2::slotRefreshMasterClipEffects);
    connect(m_effectStack, SIGNAL(addMasterEffect(QString, QDomElement)), pCore->bin(), SLOT(slotEffectDropped(QString, QDomElement)));
    connect(m_effectStack, SIGNAL(updateMasterEffect(QString,QDomElement,QDomElement,int,bool)), pCore->bin(), SLOT(slotUpdateEffect(QString,QDomElement,QDomElement,int,bool)));
    connect(m_effectStack, SIGNAL(changeMasterEffectState(QString, QList<int>, bool)), pCore->bin(), SLOT(slotChangeEffectState(QString, QList<int>, bool)));
    connect(m_effectStack, &EffectStackView2::removeMasterEffect, pCore->bin(), &Bin::slotDeleteEffect);
    connect(m_effectStack, SIGNAL(changeEffectPosition(QString, QList<int>, int)), pCore->bin(), SLOT(slotMoveEffect(QString, QList<int>, int)));
    connect(m_effectStack, &EffectStackView2::reloadEffects, this, &MainWindow::slotReloadEffects);
    connect(m_effectStack, SIGNAL(displayMessage(QString, int)), m_messageLabel, SLOT(setProgressMessage(QString, int)));

    m_effectStackDock = addDock(i18n("Properties"), QStringLiteral("effect_stack"), m_effectStack);

    m_effectList = new EffectsListView();
    m_effectListDock = addDock(i18n("Effects"), QStringLiteral("effect_list"), m_effectList);

    m_transitionList = new EffectsListView(EffectsListView::TransitionMode);
    m_transitionListDock = addDock(i18n("Transitions"), QStringLiteral("transition_list"), m_transitionList);

    // Add monitors here to keep them at the right of the window
    m_clipMonitorDock = addDock(i18n("Clip Monitor"), QStringLiteral("clip_monitor"), m_clipMonitor);
    m_projectMonitorDock = addDock(i18n("Project Monitor"), QStringLiteral("project_monitor"), m_projectMonitor);
    if (m_recMonitor) {
        m_recMonitorDock = addDock(i18n("Record Monitor"), QStringLiteral("record_monitor"), m_recMonitor);
    }

    m_undoView = new QUndoView();
    m_undoView->setCleanIcon(KoIconUtils::themedIcon(QStringLiteral("edit-clear")));
    m_undoView->setEmptyLabel(i18n("Clean"));
    m_undoView->setGroup(m_commandStack);
    m_undoViewDock = addDock(i18n("Undo History"), QStringLiteral("undo_history"), m_undoView);

    // Color and icon theme stuff
    addAction(QStringLiteral("themes_menu"), themeAction);
    connect(m_commandStack, &QUndoGroup::cleanChanged, m_saveAction, &QAction::setDisabled);
    addAction(QStringLiteral("styles_menu"), stylesAction);

    QAction *iconAction = new QAction(i18n("Force Breeze Icon Theme"), this);
    iconAction->setCheckable(true);
    iconAction->setChecked(KdenliveSettings::force_breeze());
    addAction(QStringLiteral("force_icon_theme"), iconAction);
    connect(iconAction, &QAction::triggered, this, &MainWindow::forceIconSet);

    // Close non-general docks for the initial layout
    // only show important ones
    m_undoViewDock->close();

    /// Tabify Widgets
    tabifyDockWidget(m_transitionListDock, m_effectListDock);
    tabifyDockWidget(m_effectStackDock, pCore->bin()->clipPropertiesDock());
    //tabifyDockWidget(m_effectListDock, m_effectStackDock);

    tabifyDockWidget(m_clipMonitorDock, m_projectMonitorDock);
    if (m_recMonitor) {
        tabifyDockWidget(m_clipMonitorDock, m_recMonitorDock);
    }
    bool firstRun = readOptions();

    QAction *action;
    // Stop motion actions. Beware of the order, we MUST use the same order in stopmotion/stopmotion.cpp
    m_stopmotion_actions = new KActionCategory(i18n("Stop Motion"), actionCollection());
    action = new QAction(KoIconUtils::themedIcon(QStringLiteral("media-record")), i18n("Capture frame"), this);
    //action->setShortcutContext(Qt::WidgetWithChildrenShortcut);
    m_stopmotion_actions->addAction(QStringLiteral("stopmotion_capture"), action);
    action = new QAction(i18n("Switch live / captured frame"), this);
    //action->setShortcutContext(Qt::WidgetWithChildrenShortcut);
    m_stopmotion_actions->addAction(QStringLiteral("stopmotion_switch"), action);
    action = new QAction(KoIconUtils::themedIcon(QStringLiteral("edit-paste")), i18n("Show last frame over video"), this);
    action->setCheckable(true);
    action->setChecked(false);
    m_stopmotion_actions->addAction(QStringLiteral("stopmotion_overlay"), action);

    // Monitor Record action
    addAction(QStringLiteral("switch_monitor_rec"), m_clipMonitor->recAction());

    // Build effects menu
    m_effectsMenu = new QMenu(i18n("Add Effect"), this);
    m_effectActions = new KActionCategory(i18n("Effects"), actionCollection());
    m_effectList->reloadEffectList(m_effectsMenu, m_effectActions);

    m_transitionsMenu = new QMenu(i18n("Add Transition"), this);
    m_transitionActions = new KActionCategory(i18n("Transitions"), actionCollection());
    m_transitionList->reloadEffectList(m_transitionsMenu, m_transitionActions);

    ScopeManager *scmanager = new ScopeManager(this);

    new LayoutManagement(this);
    new HideTitleBars(this);
    new TimelineSearch(this);
    m_extraFactory = new KXMLGUIClient(this);
    buildDynamicActions();

    // Add shortcut to action tooltips
    QList< KActionCollection * > collections = KActionCollection::allCollections();
    for (int i = 0; i < collections.count(); ++i) {
        KActionCollection *coll = collections.at(i);
        foreach (QAction *tempAction, coll->actions()) {
            // find the shortcut pattern and delete (note the preceding space in the RegEx)
            QString strippedTooltip = tempAction->toolTip().remove(QRegExp(QStringLiteral("\\s\\(.*\\)")));
            // append shortcut if it exists for action
            if (tempAction->shortcut() == QKeySequence(0)) {
                tempAction->setToolTip(strippedTooltip);
            } else {
                tempAction->setToolTip(strippedTooltip + QStringLiteral(" (") + tempAction->shortcut().toString() + QLatin1Char(')'));
            }
        }
    }

    // Create Effect Basket (dropdown list of favorites)
    m_effectBasket = new EffectBasket(m_effectList);
    connect(m_effectBasket, SIGNAL(addEffect(QDomElement)), this, SLOT(slotAddEffect(QDomElement)));
    QWidgetAction *widgetlist = new QWidgetAction(this);
    widgetlist->setDefaultWidget(m_effectBasket);
    //widgetlist->setText(i18n("Favorite Effects"));
    widgetlist->setToolTip(i18n("Favorite Effects"));
    widgetlist->setIcon(KoIconUtils::themedIcon(QStringLiteral("favorite")));
    QMenu *menu = new QMenu(this);
    menu->addAction(widgetlist);

    QToolButton *basketButton = new QToolButton(this);
    basketButton->setMenu(menu);
    basketButton->setToolButtonStyle(toolBar()->toolButtonStyle());
    basketButton->setDefaultAction(widgetlist);
    basketButton->setPopupMode(QToolButton::InstantPopup);
    //basketButton->setText(i18n("Favorite Effects"));
    basketButton->setToolTip(i18n("Favorite Effects"));
    basketButton->setIcon(KoIconUtils::themedIcon(QStringLiteral("favorite")));

    QWidgetAction *toolButtonAction = new QWidgetAction(this);
    toolButtonAction->setText(i18n("Favorite Effects"));
    toolButtonAction->setIcon(KoIconUtils::themedIcon(QStringLiteral("favorite")));
    toolButtonAction->setDefaultWidget(basketButton);
    addAction(QStringLiteral("favorite_effects"), toolButtonAction);
    connect(toolButtonAction, &QAction::triggered, basketButton, &QToolButton::showMenu);

    // Render button
    ProgressButton *timelineRender = new ProgressButton(i18n("Render"), 100, this);
    QMenu *tlrMenu = new QMenu(this);
    timelineRender->setMenu(tlrMenu);
    connect(this, &MainWindow::setRenderProgress, timelineRender, &ProgressButton::setProgress);
    QWidgetAction *renderButtonAction = new QWidgetAction(this);
    renderButtonAction->setText(i18n("Render Button"));
    renderButtonAction->setIcon(KoIconUtils::themedIcon(QStringLiteral("media-record")));
    renderButtonAction->setDefaultWidget(timelineRender);
    addAction(QStringLiteral("project_render_button"), renderButtonAction);

    // Timeline preview button
    ProgressButton *timelinePreview = new ProgressButton(i18n("Rendering preview"), 1000, this);
    QMenu *tlMenu = new QMenu(this);
    timelinePreview->setMenu(tlMenu);
    connect(this, &MainWindow::setPreviewProgress, timelinePreview, &ProgressButton::setProgress);
    QWidgetAction *previewButtonAction = new QWidgetAction(this);
    previewButtonAction->setText(i18n("Timeline Preview"));
    previewButtonAction->setIcon(KoIconUtils::themedIcon(QStringLiteral("preview-render-on")));
    previewButtonAction->setDefaultWidget(timelinePreview);
    addAction(QStringLiteral("timeline_preview_button"), previewButtonAction);

    setupGUI();
    if (firstRun) {
        QScreen *current = QApplication::primaryScreen();
        if (current) {
            if (current->availableSize().height() < 1000) {
                resize(current->availableSize());
            } else {
                resize(current->availableSize() / 1.5);
            }
        }
    }
    timelinePreview->setToolButtonStyle(m_timelineToolBar->toolButtonStyle());
    connect(m_timelineToolBar, &QToolBar::toolButtonStyleChanged, timelinePreview, &ProgressButton::setToolButtonStyle);

    timelineRender->setToolButtonStyle(toolBar()->toolButtonStyle());
    /*connect(m_timelineToolBar, &QToolBar::toolButtonStyleChanged, timelinePreview, &ProgressButton::setToolButtonStyle);*/

    /*ScriptingPart* sp = new ScriptingPart(this, QStringList());
    guiFactory()->addClient(sp);*/

    loadGenerators();
    loadDockActions();
    loadClipActions();

    // Connect monitor overlay info menu.
    QMenu *monitorOverlay = static_cast<QMenu *>(factory()->container(QStringLiteral("monitor_config_overlay"), this));
    connect(monitorOverlay, &QMenu::triggered, this, &MainWindow::slotSwitchMonitorOverlay);

    m_projectMonitor->setupMenu(static_cast<QMenu *>(factory()->container(QStringLiteral("monitor_go"), this)), monitorOverlay, m_playZone, m_loopZone, nullptr, m_loopClip);
    m_clipMonitor->setupMenu(static_cast<QMenu *>(factory()->container(QStringLiteral("monitor_go"), this)), monitorOverlay, m_playZone, m_loopZone, static_cast<QMenu *>(factory()->container(QStringLiteral("marker_menu"), this)));

    QMenu *clipInTimeline = static_cast<QMenu *>(factory()->container(QStringLiteral("clip_in_timeline"), this));
    clipInTimeline->setIcon(KoIconUtils::themedIcon(QStringLiteral("go-jump")));
    pCore->bin()->setupGeneratorMenu();

    connect(pCore->monitorManager(), &MonitorManager::updateOverlayInfos, this, &MainWindow::slotUpdateMonitorOverlays);

    // Setup and fill effects and transitions menus.
    QMenu *m = static_cast<QMenu *>(factory()->container(QStringLiteral("video_effects_menu"), this));
    connect(m, &QMenu::triggered, this, &MainWindow::slotAddVideoEffect);
    connect(m_effectsMenu, &QMenu::triggered, this, &MainWindow::slotAddVideoEffect);
    connect(m_transitionsMenu, &QMenu::triggered, this, &MainWindow::slotAddTransition);

    m_timelineContextMenu = new QMenu(this);
    m_timelineContextClipMenu = new QMenu(this);
    m_timelineContextTransitionMenu = new QMenu(this);

    m_timelineContextMenu->addAction(actionCollection()->action(QStringLiteral("insert_space")));
    m_timelineContextMenu->addAction(actionCollection()->action(QStringLiteral("delete_space")));
    m_timelineContextMenu->addAction(actionCollection()->action(QStringLiteral("delete_space_all_tracks")));
    m_timelineContextMenu->addAction(actionCollection()->action(KStandardAction::name(KStandardAction::Paste)));

    m_timelineContextClipMenu->addAction(actionCollection()->action(QStringLiteral("clip_in_project_tree")));
    //m_timelineContextClipMenu->addAction(actionCollection()->action("clip_to_project_tree"));
    m_timelineContextClipMenu->addAction(actionCollection()->action(QStringLiteral("delete_timeline_clip")));
    m_timelineContextClipMenu->addSeparator();
    m_timelineContextClipMenu->addAction(actionCollection()->action(QStringLiteral("group_clip")));
    m_timelineContextClipMenu->addAction(actionCollection()->action(QStringLiteral("ungroup_clip")));
    m_timelineContextClipMenu->addAction(actionCollection()->action(QStringLiteral("split_audio")));
    m_timelineContextClipMenu->addAction(actionCollection()->action(QStringLiteral("set_audio_align_ref")));
    m_timelineContextClipMenu->addAction(actionCollection()->action(QStringLiteral("align_audio")));
    m_timelineContextClipMenu->addSeparator();
    m_timelineContextClipMenu->addAction(actionCollection()->action(QStringLiteral("cut_timeline_clip")));
    m_timelineContextClipMenu->addAction(actionCollection()->action(KStandardAction::name(KStandardAction::Copy)));
    m_timelineContextClipMenu->addAction(actionCollection()->action(QStringLiteral("paste_effects")));
    m_timelineContextClipMenu->addSeparator();

    QMenu *markersMenu = static_cast<QMenu *>(factory()->container(QStringLiteral("marker_menu"), this));
    m_timelineContextClipMenu->addMenu(markersMenu);
    m_timelineContextClipMenu->addSeparator();
    m_timelineContextClipMenu->addMenu(m_transitionsMenu);
    m_timelineContextClipMenu->addMenu(m_effectsMenu);

    m_timelineContextTransitionMenu->addAction(actionCollection()->action(QStringLiteral("delete_timeline_clip")));
    m_timelineContextTransitionMenu->addAction(actionCollection()->action(KStandardAction::name(KStandardAction::Copy)));

    m_timelineContextTransitionMenu->addAction(actionCollection()->action(QStringLiteral("auto_transition")));

    connect(m_effectList, &EffectsListView::addEffect, this, &MainWindow::slotAddEffect);
    connect(m_effectList, &EffectsListView::reloadEffects, this, &MainWindow::slotReloadEffects);

    slotConnectMonitors();

    m_timelineToolBar->setToolButtonStyle(Qt::ToolButtonIconOnly);
    // TODO: let user select timeline toolbar toolbutton style
    //connect(toolBar(), &QToolBar::iconSizeChanged, m_timelineToolBar, &QToolBar::setToolButtonStyle);
    m_timelineToolBar->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_timelineToolBar, &QWidget::customContextMenuRequested, this, &MainWindow::showTimelineToolbarMenu);

    QAction *prevRender = actionCollection()->action(QStringLiteral("prerender_timeline_zone"));
    QAction *stopPrevRender = actionCollection()->action(QStringLiteral("stop_prerender_timeline"));
    tlMenu->addAction(stopPrevRender);
    tlMenu->addAction(actionCollection()->action(QStringLiteral("set_render_timeline_zone")));
    tlMenu->addAction(actionCollection()->action(QStringLiteral("unset_render_timeline_zone")));
    tlMenu->addAction(actionCollection()->action(QStringLiteral("clear_render_timeline_zone")));

    // Automatic timeline preview action
    QAction *autoRender = new QAction(KoIconUtils::themedIcon(QStringLiteral("view-refresh")), i18n("Automatic Preview"), this);
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

    //m_timelineToolBar->addAction(toolButtonAction);

    /*QWidget *sep = new QWidget(this);
    sep->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    m_timelineToolBar->addWidget(sep);
    m_timelineToolBar->addAction(m_buttonAutomaticSplitAudio);
    m_timelineToolBar->addAction(m_buttonVideoThumbs);
    m_timelineToolBar->addAction(m_buttonAudioThumbs);
    m_timelineToolBar->addAction(m_buttonShowMarkers);
    m_timelineToolBar->addAction(m_buttonSnap);
    m_timelineToolBar->addSeparator();
    m_timelineToolBar->addAction(m_buttonFitZoom);
    m_timelineToolBar->addAction(m_zoomOut);
    m_timelineToolBar->addWidget(m_zoomSlider);
    m_timelineToolBar->addAction(m_zoomIn);*/

    // Populate encoding profiles
    KConfig conf(QStringLiteral("encodingprofiles.rc"), KConfig::CascadeConfig, QStandardPaths::AppDataLocation);
    if (KdenliveSettings::proxyparams().isEmpty() || KdenliveSettings::proxyextension().isEmpty()) {
        KConfigGroup group(&conf, "proxy");
        QMap< QString, QString > values = group.entryMap();
        QMapIterator<QString, QString> i(values);
        if (i.hasNext()) {
            i.next();
            QString data = i.value();
            KdenliveSettings::setProxyparams(data.section(QLatin1Char(';'), 0, 0));
            KdenliveSettings::setProxyextension(data.section(QLatin1Char(';'), 1, 1));
        }
    }
    if (KdenliveSettings::v4l_parameters().isEmpty() || KdenliveSettings::v4l_extension().isEmpty()) {
        KConfigGroup group(&conf, "video4linux");
        QMap< QString, QString > values = group.entryMap();
        QMapIterator<QString, QString> i(values);
        if (i.hasNext()) {
            i.next();
            QString data = i.value();
            KdenliveSettings::setV4l_parameters(data.section(QLatin1Char(';'), 0, 0));
            KdenliveSettings::setV4l_extension(data.section(QLatin1Char(';'), 1, 1));
        }
    }
    if (KdenliveSettings::grab_parameters().isEmpty() || KdenliveSettings::grab_extension().isEmpty()) {
        KConfigGroup group(&conf, "screengrab");
        QMap< QString, QString > values = group.entryMap();
        QMapIterator<QString, QString> i(values);
        if (i.hasNext()) {
            i.next();
            QString data = i.value();
            KdenliveSettings::setGrab_parameters(data.section(QLatin1Char(';'), 0, 0));
            KdenliveSettings::setGrab_extension(data.section(QLatin1Char(';'), 1, 1));
        }
    }
    if (KdenliveSettings::decklink_parameters().isEmpty() || KdenliveSettings::decklink_extension().isEmpty()) {
        KConfigGroup group(&conf, "decklink");
        QMap< QString, QString > values = group.entryMap();
        QMapIterator<QString, QString> i(values);
        if (i.hasNext()) {
            i.next();
            QString data = i.value();
            KdenliveSettings::setDecklink_parameters(data.section(QLatin1Char(';'), 0, 0));
            KdenliveSettings::setDecklink_extension(data.section(QLatin1Char(';'), 1, 1));
        }
    }

    if (!QDir(KdenliveSettings::currenttmpfolder()).isReadable())
        KdenliveSettings::setCurrenttmpfolder(QStandardPaths::writableLocation(QStandardPaths::TempLocation));

    pCore->projectManager()->init(Url, clipsToLoad);
    QTimer::singleShot(0, pCore->projectManager(), &ProjectManager::slotLoadOnOpen);
    QTimer::singleShot(0, this, &MainWindow::GUISetupDone);
    connect(this, &MainWindow::reloadTheme, this, &MainWindow::slotReloadTheme, Qt::UniqueConnection);

#ifdef USE_JOGSHUTTLE
    new JogManager(this);
#endif
    scmanager->slotCheckActiveScopes();
    //m_messageLabel->setMessage(QStringLiteral("This is a beta version. Always backup your data"), MltError);
}

void MainWindow::slotThemeChanged(const QString &theme)
{
    disconnect(this, &MainWindow::reloadTheme, this, &MainWindow::slotReloadTheme);
    KSharedConfigPtr config = KSharedConfig::openConfig(theme);
    setPalette(KColorScheme::createApplicationPalette(config));
    qApp->setPalette(palette());
    QPalette plt = palette();

    KdenliveSettings::setColortheme(theme);
    if (m_effectStack) {
        m_effectStack->updatePalette();
        m_effectStack->transitionConfig()->updatePalette();
    }
    if (m_effectList) {
        m_effectList->updatePalette();
    }
    if (m_transitionList) {
        m_transitionList->updatePalette();
    }
    if (m_clipMonitor) {
        m_clipMonitor->setPalette(plt);
    }
    if (m_projectMonitor) {
        m_projectMonitor->setPalette(plt);
    }
    if (m_messageLabel) {
        m_messageLabel->updatePalette();
    }
    if (pCore->projectManager() && pCore->projectManager()->currentTimeline()) {
        pCore->projectManager()->currentTimeline()->updatePalette();
    }
    if (m_timelineArea) {
        m_timelineArea->setPalette(plt);
    }

#if KXMLGUI_VERSION_MINOR < 23 && KXMLGUI_VERSION_MAJOR == 5
    // Not required anymore with auto colored icons since KF5 5.23
    QColor background = plt.window().color();
    bool useDarkIcons = background.value() < 100;
    if (m_themeInitialized && useDarkIcons != m_isDarkTheme) {
        if (pCore->bin()) {
            pCore->bin()->refreshIcons();
        }
        if (m_clipMonitor) {
            m_clipMonitor->refreshIcons();
        }
        if (m_projectMonitor) {
            m_projectMonitor->refreshIcons();
        }
        if (pCore->monitorManager()) {
            pCore->monitorManager()->refreshIcons();
        }
        if (m_effectStack && m_effectStack->transitionConfig()) {
            m_effectStack->transitionConfig()->refreshIcons();
        }
        if (m_effectList) {
            m_effectList->refreshIcons();
        }
        if (m_effectStack) {
            m_effectStack->refreshIcons();
        }
        if (pCore->projectManager() && pCore->projectManager()->currentTimeline()) {
            pCore->projectManager()->currentTimeline()->refreshIcons();
        }

        foreach (QAction *action, actionCollection()->actions()) {
            QIcon icon = action->icon();
            if (icon.isNull()) {
                continue;
            }
            QString iconName = icon.name();
            QIcon newIcon = KoIconUtils::themedIcon(iconName);
            if (newIcon.isNull()) {
                continue;
            }
            action->setIcon(newIcon);
        }
    }
    m_themeInitialized = true;
    m_isDarkTheme = useDarkIcons;
#endif
    connect(this, &MainWindow::reloadTheme, this, &MainWindow::slotReloadTheme, Qt::UniqueConnection);
}

bool MainWindow::event(QEvent *e)
{
    switch (e->type()) {
    case QEvent::ApplicationPaletteChange:
        emit reloadTheme();
        e->accept();
        break;
    default:
        break;
    }
    return KXmlGuiWindow::event(e);
}

void MainWindow::slotReloadTheme()
{
    ThemeManager::instance()->slotSettingsChanged();
}

MainWindow::~MainWindow()
{
    delete m_stopmotion;
    delete m_audioSpectrum;
    m_effectStack->slotClipItemSelected(nullptr, m_projectMonitor);
    m_effectStack->slotTransitionItemSelected(nullptr, 0, QPoint(), false);
    if (m_projectMonitor) {
        m_projectMonitor->stop();
    }
    if (m_clipMonitor) {
        m_clipMonitor->stop();
    }
    delete pCore;
    delete m_effectStack;
    delete m_projectMonitor;
    delete m_clipMonitor;
    delete m_shortcutRemoveFocus;
    qDeleteAll(m_transitions);
    Mlt::Factory::close();
}

//virtual
bool MainWindow::queryClose()
{
    if (m_renderWidget) {
        int waitingJobs = m_renderWidget->waitingJobsCount();
        if (waitingJobs > 0) {
            switch (KMessageBox::warningYesNoCancel(this, i18np("You have 1 rendering job waiting in the queue.\nWhat do you want to do with this job?", "You have %1 rendering jobs waiting in the queue.\nWhat do you want to do with these jobs?", waitingJobs), QString(), KGuiItem(i18n("Start them now")), KGuiItem(i18n("Delete them")))) {
            case KMessageBox::Yes :
                // create script with waiting jobs and start it
                if (m_renderWidget->startWaitingRenderJobs() == false) {
                    return false;
                }
                break;
            case KMessageBox::No :
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

void MainWindow::loadGenerators()
{
    QMenu *addMenu = static_cast<QMenu *>(factory()->container(QStringLiteral("generators"), this));
    Generators::getGenerators(KdenliveSettings::producerslist(), addMenu);
    connect(addMenu, &QMenu::triggered, this, &MainWindow::buildGenerator);
}

void MainWindow::buildGenerator(QAction *action)
{
    Generators gen(m_clipMonitor, action->data().toString(), this);
    if (gen.exec() == QDialog::Accepted) {
        pCore->bin()->slotAddClipToProject(gen.getSavedClip());
    }
}

void MainWindow::saveProperties(KConfigGroup &config)
{
    // save properties here
    KXmlGuiWindow::saveProperties(config);
    //TODO: fix session management
    if (qApp->isSavingSession() && pCore->projectManager()) {
        if (pCore->projectManager()->current() && !pCore->projectManager()->current()->url().isEmpty()) {
            config.writeEntry("kdenlive_lastUrl", pCore->projectManager()->current()->url().toLocalFile());
        }
    }
}

void MainWindow::readProperties(const KConfigGroup &config)
{
    // read properties here
    KXmlGuiWindow::readProperties(config);
    //TODO: fix session management
    /*if (qApp->isSessionRestored()) {
    pCore->projectManager()->openFile(QUrl::fromLocalFile(config.readEntry("kdenlive_lastUrl", QString())));
    }*/
}

void MainWindow::saveNewToolbarConfig()
{
    KXmlGuiWindow::saveNewToolbarConfig();
    //TODO for some reason all dynamically inserted actions are removed by the save toolbar
    // So we currently re-add them manually....
    loadDockActions();
    loadClipActions();
    pCore->bin()->rebuildMenu();
    QMenu *monitorOverlay = static_cast<QMenu *>(factory()->container(QStringLiteral("monitor_config_overlay"), this));
    if (monitorOverlay) {
        m_projectMonitor->setupMenu(static_cast<QMenu *>(factory()->container(QStringLiteral("monitor_go"), this)), monitorOverlay, m_playZone, m_loopZone, nullptr, m_loopClip);
        m_clipMonitor->setupMenu(static_cast<QMenu *>(factory()->container(QStringLiteral("monitor_go"), this)), monitorOverlay, m_playZone, m_loopZone, static_cast<QMenu *>(factory()->container(QStringLiteral("marker_menu"), this)));
    }
}

void MainWindow::slotReloadEffects()
{
    initEffects::parseCustomEffectsFile();
    m_effectList->reloadEffectList(m_effectsMenu, m_effectActions);
}

void MainWindow::configureNotifications()
{
    KNotifyConfigWidget::configure(this);
}

void MainWindow::slotFullScreen()
{
    KToggleFullScreenAction::setFullScreen(this, actionCollection()->action(QStringLiteral("fullscreen"))->isChecked());
}

void MainWindow::slotAddEffect(const QDomElement &effect)
{
    if (effect.isNull()) {
        qCDebug(KDENLIVE_LOG) << "--- ERROR, TRYING TO APPEND nullptr EFFECT";
        return;
    }
    QDomElement effectToAdd = effect.cloneNode().toElement();
    EFFECTMODE status = m_effectStack->effectStatus();
    switch (status) {
    case TIMELINE_TRACK:
        pCore->projectManager()->currentTimeline()->projectView()->slotAddTrackEffect(effectToAdd, m_effectStack->trackIndex());
        break;
    case TIMELINE_CLIP:
        pCore->projectManager()->currentTimeline()->projectView()->slotAddEffectToCurrentItem(effectToAdd);
        break;
    case MASTER_CLIP:
        pCore->bin()->slotEffectDropped(QString(), effectToAdd);
        break;
    default:
        // No clip selected
        m_messageLabel->setMessage(i18n("Select a clip if you want to apply an effect"), ErrorMessage);
    }
}

void MainWindow::slotUpdateClip(const QString &id, bool reload)
{
    ProjectClip *clip = pCore->bin()->getBinClip(id);
    if (!clip) {
        return;
    }
    pCore->projectManager()->currentTimeline()->projectView()->slotUpdateClip(id, reload);
}

void MainWindow::slotConnectMonitors()
{
    //connect(m_projectList, SIGNAL(deleteProjectClips(QStringList,QMap<QString,QString>)), this, SLOT(slotDeleteProjectClips(QStringList,QMap<QString,QString>)));
    connect(m_projectMonitor->render, SIGNAL(gotFileProperties(requestClipInfo, ClipController *)), pCore->bin(), SLOT(slotProducerReady(requestClipInfo, ClipController *)), Qt::DirectConnection);

    connect(m_clipMonitor, &Monitor::refreshClipThumbnail, pCore->bin(), &Bin::slotRefreshClipThumbnail);
    connect(m_projectMonitor, &Monitor::requestFrameForAnalysis, this, &MainWindow::slotMonitorRequestRenderFrame);
    connect(m_projectMonitor, &Monitor::createSplitOverlay, this, &MainWindow::createSplitOverlay);
    connect(m_projectMonitor, &Monitor::removeSplitOverlay, this, &MainWindow::removeSplitOverlay);
}

void MainWindow::createSplitOverlay(Mlt::Filter *filter)
{
    if (pCore->projectManager()->currentTimeline()) {
        if (pCore->projectManager()->currentTimeline()->projectView()->createSplitOverlay(filter)) {
            m_projectMonitor->activateSplit();
        }
    }
}

void MainWindow::removeSplitOverlay()
{
    if (pCore->projectManager()->currentTimeline()) {
        pCore->projectManager()->currentTimeline()->projectView()->removeSplitOverlay();
    }
}

void MainWindow::addAction(const QString &name, QAction *action)
{
    m_actionNames.append(name);
    actionCollection()->addAction(name, action);
    actionCollection()->setDefaultShortcut(action, action->shortcut()); //Fix warning about setDefaultShortcut
}

QAction *MainWindow::addAction(const QString &name, const QString &text, const QObject *receiver,
                               const char *member, const QIcon &icon, const QKeySequence &shortcut)
{
    QAction *action = new QAction(text, this);
    if (!icon.isNull()) {
        action->setIcon(icon);
    }
    if (!shortcut.isEmpty()) {
        action->setShortcut(shortcut);
    }
    addAction(name, action);
    connect(action, SIGNAL(triggered(bool)), receiver, member);

    return action;
}

void MainWindow::setupActions()
{
    //create edit mode buttons
    m_normalEditTool = new QAction(KoIconUtils::themedIcon(QStringLiteral("kdenlive-normal-edit")), i18n("Normal mode"), this);
    m_normalEditTool->setShortcut(i18nc("Normal editing", "n"));
    m_normalEditTool->setCheckable(true);
    m_normalEditTool->setChecked(true);

    m_overwriteEditTool = new QAction(KoIconUtils::themedIcon(QStringLiteral("kdenlive-overwrite-edit")), i18n("Overwrite mode"), this);
    m_overwriteEditTool->setCheckable(true);
    m_overwriteEditTool->setChecked(false);

    m_insertEditTool = new QAction(KoIconUtils::themedIcon(QStringLiteral("kdenlive-insert-edit")), i18n("Insert mode"), this);
    m_insertEditTool->setCheckable(true);
    m_insertEditTool->setChecked(false);

    KSelectAction *sceneMode = new KSelectAction(i18n("Timeline Edit Mode"), this);
    sceneMode->addAction(m_normalEditTool);
    sceneMode->addAction(m_overwriteEditTool);
    sceneMode->addAction(m_insertEditTool);
    sceneMode->setCurrentItem(0);
    connect(sceneMode, SIGNAL(triggered(QAction *)), this, SLOT(slotChangeEdit(QAction *)));
    addAction(QStringLiteral("timeline_mode"), sceneMode);

    KDualAction *ac = new KDualAction(i18n("Don't Use Timeline Zone for Insert"), i18n("Use Timeline Zone for Insert"), this);
    ac->setActiveIcon(KoIconUtils::themedIcon(QStringLiteral("timeline-use-zone-on")));
    ac->setInactiveIcon(KoIconUtils::themedIcon(QStringLiteral("timeline-use-zone-off")));
    ac->setShortcut(Qt::Key_G);
    addAction(QStringLiteral("use_timeline_zone_in_edit"), ac);

    m_compositeAction = new KSelectAction(KoIconUtils::themedIcon(QStringLiteral("composite-track-off")), i18n("Track compositing"), this);
    m_compositeAction->setToolTip(i18n("Track compositing"));
    QAction *noComposite = new QAction(KoIconUtils::themedIcon(QStringLiteral("composite-track-off")),
                                       i18n("None"), this);
    noComposite->setCheckable(true);
    noComposite->setData(0);
    m_compositeAction->addAction(noComposite);
    int compose = KdenliveDoc::compositingMode();
    if (compose == 2) {
        // Movit, do not show "preview" option since movit is faster
        QAction *hqComposite = new QAction(KoIconUtils::themedIcon(QStringLiteral("composite-track-on")), i18n("High Quality"), this);
        hqComposite->setCheckable(true);
        hqComposite->setData(2);
        m_compositeAction->addAction(hqComposite);
        m_compositeAction->setCurrentAction(hqComposite);
    } else {
        QAction *previewComposite = new QAction(KoIconUtils::themedIcon(QStringLiteral("composite-track-preview")), i18n("Preview"), this);
        previewComposite->setCheckable(true);
        previewComposite->setData(1);
        m_compositeAction->addAction(previewComposite);
        if (compose == 1) {
            QAction *hqComposite = new QAction(KoIconUtils::themedIcon(QStringLiteral("composite-track-on")), i18n("High Quality"), this);
            hqComposite->setData(2);
            hqComposite->setCheckable(true);
            m_compositeAction->addAction(hqComposite);
            m_compositeAction->setCurrentAction(hqComposite);
        } else {
            m_compositeAction->setCurrentAction(previewComposite);
        }
    }
    connect(m_compositeAction, SIGNAL(triggered(QAction *)), this, SLOT(slotUpdateCompositing(QAction *)));
    addAction(QStringLiteral("timeline_compositing"), m_compositeAction);

    m_timeFormatButton = new KSelectAction(QStringLiteral("00:00:00:00 / 00:00:00:00"), this);
    m_timeFormatButton->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    m_timeFormatButton->addAction(i18n("hh:mm:ss:ff"));
    m_timeFormatButton->addAction(i18n("Frames"));
    if (KdenliveSettings::frametimecode()) {
        m_timeFormatButton->setCurrentItem(1);
    } else {
        m_timeFormatButton->setCurrentItem(0);
    }
    connect(m_timeFormatButton, SIGNAL(triggered(int)), this, SLOT(slotUpdateTimecodeFormat(int)));
    m_timeFormatButton->setToolBarMode(KSelectAction::MenuMode);
    m_timeFormatButton->setToolButtonPopupMode(QToolButton::InstantPopup);
    addAction(QStringLiteral("timeline_timecode"), m_timeFormatButton);

    // create tools buttons
    m_buttonSelectTool = new QAction(KoIconUtils::themedIcon(QStringLiteral("cursor-arrow")), i18n("Selection tool"), this);
    m_buttonSelectTool->setShortcut(i18nc("Selection tool shortcut", "s"));
    //toolbar->addAction(m_buttonSelectTool);
    m_buttonSelectTool->setCheckable(true);
    m_buttonSelectTool->setChecked(true);

    m_buttonRazorTool = new QAction(KoIconUtils::themedIcon(QStringLiteral("edit-cut")), i18n("Razor tool"), this);
    m_buttonRazorTool->setShortcut(i18nc("Razor tool shortcut", "x"));
    //toolbar->addAction(m_buttonRazorTool);
    m_buttonRazorTool->setCheckable(true);
    m_buttonRazorTool->setChecked(false);

    m_buttonSpacerTool = new QAction(KoIconUtils::themedIcon(QStringLiteral("distribute-horizontal-x")), i18n("Spacer tool"), this);
    m_buttonSpacerTool->setShortcut(i18nc("Spacer tool shortcut", "m"));
    //toolbar->addAction(m_buttonSpacerTool);
    m_buttonSpacerTool->setCheckable(true);
    m_buttonSpacerTool->setChecked(false);
    QActionGroup *toolGroup = new QActionGroup(this);
    toolGroup->addAction(m_buttonSelectTool);
    toolGroup->addAction(m_buttonRazorTool);
    toolGroup->addAction(m_buttonSpacerTool);
    toolGroup->setExclusive(true);
    //toolbar->setToolButtonStyle(Qt::ToolButtonIconOnly);

    /*QWidget * actionWidget;
    int max = toolbar->iconSizeDefault() + 2;
    actionWidget = toolbar->widgetForAction(m_normalEditTool);
    actionWidget->setMaximumWidth(max);
    actionWidget->setMaximumHeight(max - 4);

    actionWidget = toolbar->widgetForAction(m_insertEditTool);
    actionWidget->setMaximumWidth(max);
    actionWidget->setMaximumHeight(max - 4);

    actionWidget = toolbar->widgetForAction(m_overwriteEditTool);
    actionWidget->setMaximumWidth(max);
    actionWidget->setMaximumHeight(max - 4);

    actionWidget = toolbar->widgetForAction(m_buttonSelectTool);
    actionWidget->setMaximumWidth(max);
    actionWidget->setMaximumHeight(max - 4);

    actionWidget = toolbar->widgetForAction(m_buttonRazorTool);
    actionWidget->setMaximumWidth(max);
    actionWidget->setMaximumHeight(max - 4);

    actionWidget = toolbar->widgetForAction(m_buttonSpacerTool);
    actionWidget->setMaximumWidth(max);
    actionWidget->setMaximumHeight(max - 4);*/

    connect(toolGroup, &QActionGroup::triggered, this, &MainWindow::slotChangeTool);

    //create automatic audio split button
    m_buttonAutomaticSplitAudio = new QAction(KoIconUtils::themedIcon(QStringLiteral("kdenlive-split-audio")), i18n("Split audio and video automatically"), this);
    m_buttonAutomaticSplitAudio->setCheckable(true);
    m_buttonAutomaticSplitAudio->setChecked(KdenliveSettings::splitaudio());
    connect(m_buttonAutomaticSplitAudio, &QAction::toggled, this, &MainWindow::slotSwitchSplitAudio);

    m_buttonVideoThumbs = new QAction(KoIconUtils::themedIcon(QStringLiteral("kdenlive-show-videothumb")), i18n("Show video thumbnails"), this);

    m_buttonVideoThumbs->setCheckable(true);
    m_buttonVideoThumbs->setChecked(KdenliveSettings::videothumbnails());
    connect(m_buttonVideoThumbs, &QAction::triggered, this, &MainWindow::slotSwitchVideoThumbs);

    m_buttonAudioThumbs = new QAction(KoIconUtils::themedIcon(QStringLiteral("kdenlive-show-audiothumb")), i18n("Show audio thumbnails"), this);

    m_buttonAudioThumbs->setCheckable(true);
    m_buttonAudioThumbs->setChecked(KdenliveSettings::audiothumbnails());
    connect(m_buttonAudioThumbs, &QAction::triggered, this, &MainWindow::slotSwitchAudioThumbs);

    m_buttonShowMarkers = new QAction(KoIconUtils::themedIcon(QStringLiteral("kdenlive-show-markers")), i18n("Show markers comments"), this);

    m_buttonShowMarkers->setCheckable(true);
    m_buttonShowMarkers->setChecked(KdenliveSettings::showmarkers());
    connect(m_buttonShowMarkers, &QAction::triggered, this, &MainWindow::slotSwitchMarkersComments);

    m_buttonSnap = new QAction(KoIconUtils::themedIcon(QStringLiteral("kdenlive-snap")), i18n("Snap"), this);

    m_buttonSnap->setCheckable(true);
    m_buttonSnap->setChecked(KdenliveSettings::snaptopoints());
    connect(m_buttonSnap, &QAction::triggered, this, &MainWindow::slotSwitchSnap);

    m_buttonAutomaticTransition = new QAction(KoIconUtils::themedIcon(QStringLiteral("auto-transition")), i18n("Automatic transitions"), this);

    m_buttonAutomaticTransition->setCheckable(true);
    m_buttonAutomaticTransition->setChecked(KdenliveSettings::automatictransitions());
    connect(m_buttonAutomaticTransition, &QAction::triggered, this, &MainWindow::slotSwitchAutomaticTransition);

    m_buttonFitZoom = new QAction(KoIconUtils::themedIcon(QStringLiteral("zoom-fit-best")), i18n("Fit zoom to project"), this);

    m_buttonFitZoom->setCheckable(false);

    m_zoomOut = new QAction(KoIconUtils::themedIcon(QStringLiteral("zoom-out")), i18n("Zoom Out"), this);
    m_zoomOut->setShortcut(Qt::CTRL + Qt::Key_Minus);

    m_zoomSlider = new QSlider(Qt::Horizontal, this);
    m_zoomSlider->setMaximum(13);
    m_zoomSlider->setPageStep(1);
    m_zoomSlider->setInvertedAppearance(true);
    m_zoomSlider->setInvertedControls(true);

    m_zoomSlider->setMaximumWidth(150);
    m_zoomSlider->setMinimumWidth(100);

    m_zoomIn = new QAction(KoIconUtils::themedIcon(QStringLiteral("zoom-in")), i18n("Zoom In"), this);
    m_zoomIn->setShortcut(Qt::CTRL + Qt::Key_Plus);

    /*actionWidget = toolbar->widgetForAction(m_buttonFitZoom);
    actionWidget->setMaximumWidth(max);
    actionWidget->setMaximumHeight(max - 4);
    actionWidget->setStyleSheet(styleBorderless);

    actionWidget = toolbar->widgetForAction(m_zoomIn);
    actionWidget->setMaximumWidth(max);
    actionWidget->setMaximumHeight(max - 4);
    actionWidget->setStyleSheet(styleBorderless);

    actionWidget = toolbar->widgetForAction(m_zoomOut);
    actionWidget->setMaximumWidth(max);
    actionWidget->setMaximumHeight(max - 4);
    actionWidget->setStyleSheet(styleBorderless);*/

    connect(m_zoomSlider, SIGNAL(valueChanged(int)), this, SLOT(slotSetZoom(int)));
    connect(m_zoomSlider, &QAbstractSlider::sliderMoved, this, &MainWindow::slotShowZoomSliderToolTip);
    connect(m_buttonFitZoom, &QAction::triggered, this, &MainWindow::slotFitZoom);
    connect(m_zoomIn, &QAction::triggered, this, &MainWindow::slotZoomIn);
    connect(m_zoomOut, &QAction::triggered, this, &MainWindow::slotZoomOut);

    m_trimLabel = new QLabel(QStringLiteral(" "), this);
    m_trimLabel->setFont(QFontDatabase::systemFont(QFontDatabase::SmallestReadableFont));
    //m_trimLabel->setAutoFillBackground(true);
    m_trimLabel->setAlignment(Qt::AlignHCenter);
    m_trimLabel->setStyleSheet(QStringLiteral("QLabel { background-color :red; }"));

    KToolBar *toolbar = new KToolBar(QStringLiteral("statusToolBar"), this, Qt::BottomToolBarArea);
    toolbar->setMovable(false);
    toolbar->setToolButtonStyle(Qt::ToolButtonIconOnly);

    /*QString styleBorderless = QStringLiteral("QToolButton { border-width: 0px;margin: 1px 3px 0px;padding: 0px;}");*/
    toolbar->addWidget(m_trimLabel);
    toolbar->addAction(m_buttonAutomaticSplitAudio);
    toolbar->addAction(m_buttonAutomaticTransition);
    toolbar->addAction(m_buttonVideoThumbs);
    toolbar->addAction(m_buttonAudioThumbs);
    toolbar->addAction(m_buttonShowMarkers);
    toolbar->addAction(m_buttonSnap);
    toolbar->addSeparator();
    toolbar->addAction(m_buttonFitZoom);
    toolbar->addAction(m_zoomOut);
    toolbar->addWidget(m_zoomSlider);
    toolbar->addAction(m_zoomIn);

    /*actionWidget = toolbar->widgetForAction(m_buttonAutomaticSplitAudio);
    actionWidget->setMaximumWidth(max);
    actionWidget->setMaximumHeight(max - 4);

    actionWidget = toolbar->widgetForAction(m_buttonVideoThumbs);
    actionWidget->setMaximumWidth(max);
    actionWidget->setMaximumHeight(max - 4);

    actionWidget = toolbar->widgetForAction(m_buttonAudioThumbs);
    actionWidget->setMaximumWidth(max);
    actionWidget->setMaximumHeight(max - 4);

    actionWidget = toolbar->widgetForAction(m_buttonShowMarkers);
    actionWidget->setMaximumWidth(max);
    actionWidget->setMaximumHeight(max - 4);

    actionWidget = toolbar->widgetForAction(m_buttonSnap);
    actionWidget->setMaximumWidth(max);
    actionWidget->setMaximumHeight(max - 4);*/

    int small = style()->pixelMetric(QStyle::PM_SmallIconSize);
    statusBar()->setMaximumHeight(2 * small);
    m_messageLabel = new StatusBarMessageLabel(this);
    m_messageLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::MinimumExpanding);
    connect(this, &MainWindow::displayMessage, m_messageLabel, &StatusBarMessageLabel::setMessage);
    statusBar()->addWidget(m_messageLabel, 0);
    QWidget *spacer = new QWidget(this);
    spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    statusBar()->addWidget(spacer, 1);
    statusBar()->addPermanentWidget(toolbar);
    toolbar->setIconSize(QSize(small, small));
    toolbar->layout()->setContentsMargins(0, 0, 0, 0);
    statusBar()->setContentsMargins(0, 0, 0, 0);

    addAction(QStringLiteral("normal_mode"), m_normalEditTool);
    addAction(QStringLiteral("overwrite_mode"), m_overwriteEditTool);
    addAction(QStringLiteral("insert_mode"), m_insertEditTool);
    addAction(QStringLiteral("select_tool"), m_buttonSelectTool);
    addAction(QStringLiteral("razor_tool"), m_buttonRazorTool);
    addAction(QStringLiteral("spacer_tool"), m_buttonSpacerTool);

    addAction(QStringLiteral("automatic_split_audio"), m_buttonAutomaticSplitAudio);
    addAction(QStringLiteral("automatic_transition"), m_buttonAutomaticTransition);
    addAction(QStringLiteral("show_video_thumbs"), m_buttonVideoThumbs);
    addAction(QStringLiteral("show_audio_thumbs"), m_buttonAudioThumbs);
    addAction(QStringLiteral("show_markers"), m_buttonShowMarkers);
    addAction(QStringLiteral("snap"), m_buttonSnap);
    addAction(QStringLiteral("zoom_fit"), m_buttonFitZoom);
    addAction(QStringLiteral("zoom_in"), m_zoomIn);
    addAction(QStringLiteral("zoom_out"), m_zoomOut);

    KNS3::standardAction(i18n("Download New Wipes..."),            this, SLOT(slotGetNewLumaStuff()),       actionCollection(), "get_new_lumas");
    KNS3::standardAction(i18n("Download New Render Profiles..."),  this, SLOT(slotGetNewRenderStuff()),     actionCollection(), "get_new_profiles");
    //KNS3::standardAction(i18n("Download New Project Profiles..."), this, SLOT(slotGetNewMltProfileStuff()), actionCollection(), "get_new_mlt_profiles");
    KNS3::standardAction(i18n("Download New Title Templates..."),  this, SLOT(slotGetNewTitleStuff()),      actionCollection(), "get_new_titles");

    addAction(QStringLiteral("run_wizard"), i18n("Run Config Wizard"), this, SLOT(slotRunWizard()), KoIconUtils::themedIcon(QStringLiteral("tools-wizard")));
    addAction(QStringLiteral("project_settings"), i18n("Project Settings"), this, SLOT(slotEditProjectSettings()), KoIconUtils::themedIcon(QStringLiteral("configure")));

    addAction(QStringLiteral("project_render"), i18n("Render"), this, SLOT(slotRenderProject()), KoIconUtils::themedIcon(QStringLiteral("media-record")), Qt::CTRL + Qt::Key_Return);

    addAction(QStringLiteral("stop_project_render"), i18n("Stop Render"), this, SLOT(slotStopRenderProject()), KoIconUtils::themedIcon(QStringLiteral("media-record")));

    addAction(QStringLiteral("project_clean"), i18n("Clean Project"), this, SLOT(slotCleanProject()), KoIconUtils::themedIcon(QStringLiteral("edit-clear")));
    //TODO
    //addAction("project_adjust_profile", i18n("Adjust Profile to Current Clip"), pCore->bin(), SLOT(adjustProjectProfileToItem()));

    m_playZone = addAction(QStringLiteral("monitor_play_zone"), i18n("Play Zone"), pCore->monitorManager(), SLOT(slotPlayZone()),
                           KoIconUtils::themedIcon(QStringLiteral("media-playback-start")), Qt::CTRL + Qt::Key_Space);
    m_loopZone = addAction(QStringLiteral("monitor_loop_zone"), i18n("Loop Zone"), pCore->monitorManager(), SLOT(slotLoopZone()),
                           KoIconUtils::themedIcon(QStringLiteral("media-playback-start")), Qt::ALT + Qt::Key_Space);
    m_loopClip = new QAction(KoIconUtils::themedIcon(QStringLiteral("media-playback-start")), i18n("Loop selected clip"), this);
    addAction(QStringLiteral("monitor_loop_clip"), m_loopClip);
    m_loopClip->setEnabled(false);

    addAction(QStringLiteral("dvd_wizard"), i18n("DVD Wizard"), this, SLOT(slotDvdWizard()), KoIconUtils::themedIcon(QStringLiteral("media-optical")));
    addAction(QStringLiteral("transcode_clip"), i18n("Transcode Clips"), this, SLOT(slotTranscodeClip()), KoIconUtils::themedIcon(QStringLiteral("edit-copy")));
    addAction(QStringLiteral("archive_project"), i18n("Archive Project"), this, SLOT(slotArchiveProject()), KoIconUtils::themedIcon(QStringLiteral("document-save-all")));
    addAction(QStringLiteral("switch_monitor"), i18n("Switch monitor"), this, SLOT(slotSwitchMonitors()), QIcon(), Qt::Key_T);
    addAction(QStringLiteral("expand_timeline_clip"), i18n("Expand Clip"), pCore->projectManager(), SLOT(slotExpandClip()), KoIconUtils::themedIcon(QStringLiteral("document-open")));

    QAction *overlayInfo =  new QAction(KoIconUtils::themedIcon(QStringLiteral("help-hint")), i18n("Monitor Info Overlay"), this);
    addAction(QStringLiteral("monitor_overlay"), overlayInfo);
    overlayInfo->setCheckable(true);
    overlayInfo->setData(0x01);

    QAction *overlayTCInfo =  new QAction(KoIconUtils::themedIcon(QStringLiteral("help-hint")), i18n("Monitor Overlay Timecode"), this);
    addAction(QStringLiteral("monitor_overlay_tc"), overlayTCInfo);
    overlayTCInfo->setCheckable(true);
    overlayTCInfo->setData(0x02);

    QAction *overlayFpsInfo =  new QAction(KoIconUtils::themedIcon(QStringLiteral("help-hint")), i18n("Monitor Overlay Playback Fps"), this);
    addAction(QStringLiteral("monitor_overlay_fps"), overlayFpsInfo);
    overlayFpsInfo->setCheckable(true);
    overlayFpsInfo->setData(0x20);

    QAction *overlayMarkerInfo =  new QAction(KoIconUtils::themedIcon(QStringLiteral("help-hint")), i18n("Monitor Overlay Markers"), this);
    addAction(QStringLiteral("monitor_overlay_markers"), overlayMarkerInfo);
    overlayMarkerInfo->setCheckable(true);
    overlayMarkerInfo->setData(0x04);

    QAction *overlaySafeInfo =  new QAction(KoIconUtils::themedIcon(QStringLiteral("help-hint")), i18n("Monitor Overlay Safe Zones"), this);
    addAction(QStringLiteral("monitor_overlay_safezone"), overlaySafeInfo);
    overlaySafeInfo->setCheckable(true);
    overlaySafeInfo->setData(0x08);

    QAction *overlayAudioInfo =  new QAction(KoIconUtils::themedIcon(QStringLiteral("help-hint")), i18n("Monitor Overlay Audio Waveform"), this);
    addAction(QStringLiteral("monitor_overlay_audiothumb"), overlayAudioInfo);
    overlayAudioInfo->setCheckable(true);
    overlayAudioInfo->setData(0x10);

    QAction *dropFrames = new QAction(QIcon(), i18n("Real Time (drop frames)"), this);
    dropFrames->setCheckable(true);
    dropFrames->setChecked(KdenliveSettings::monitor_dropframes());
    connect(dropFrames, &QAction::toggled, this, &MainWindow::slotSwitchDropFrames);

    KSelectAction *monitorGamma = new KSelectAction(i18n("Monitor Gamma"), this);
    monitorGamma->addAction(i18n("sRGB (computer)"));
    monitorGamma->addAction(i18n("Rec. 709 (TV)"));
    addAction(QStringLiteral("mlt_gamma"), monitorGamma);
    monitorGamma->setCurrentItem(KdenliveSettings::monitor_gamma());
    connect(monitorGamma, SIGNAL(triggered(int)), this, SLOT(slotSetMonitorGamma(int)));

    addAction(QStringLiteral("switch_trim"), i18n("Trim Mode"), this, SLOT(slotSwitchTrimMode()), KoIconUtils::themedIcon(QStringLiteral("cursor-arrow")));
    // disable shortcut until fully working, Qt::CTRL + Qt::Key_T);

    addAction(QStringLiteral("insert_project_tree"), i18n("Insert Zone in Project Bin"), this, SLOT(slotInsertZoneToTree()), QIcon(), Qt::CTRL + Qt::Key_I);
    addAction(QStringLiteral("insert_timeline"), i18n("Insert Zone in Timeline"), this, SLOT(slotInsertZoneToTimeline()), QIcon(), Qt::SHIFT + Qt::CTRL + Qt::Key_I);

    QAction *resizeStart =  new QAction(QIcon(), i18n("Resize Item Start"), this);
    addAction(QStringLiteral("resize_timeline_clip_start"), resizeStart);
    resizeStart->setShortcut(Qt::Key_1);
    connect(resizeStart, &QAction::triggered, this, &MainWindow::slotResizeItemStart);

    QAction *resizeEnd =  new QAction(QIcon(), i18n("Resize Item End"), this);
    addAction(QStringLiteral("resize_timeline_clip_end"), resizeEnd);
    resizeEnd->setShortcut(Qt::Key_2);
    connect(resizeEnd, &QAction::triggered, this, &MainWindow::slotResizeItemEnd);

    addAction(QStringLiteral("monitor_seek_snap_backward"), i18n("Go to Previous Snap Point"), this, SLOT(slotSnapRewind()),
              KoIconUtils::themedIcon(QStringLiteral("media-seek-backward")), Qt::ALT + Qt::Key_Left);
    addAction(QStringLiteral("seek_clip_start"), i18n("Go to Clip Start"), this, SLOT(slotClipStart()), KoIconUtils::themedIcon(QStringLiteral("media-seek-backward")), Qt::Key_Home);
    addAction(QStringLiteral("seek_clip_end"), i18n("Go to Clip End"), this, SLOT(slotClipEnd()), KoIconUtils::themedIcon(QStringLiteral("media-seek-forward")), Qt::Key_End);
    addAction(QStringLiteral("monitor_seek_snap_forward"), i18n("Go to Next Snap Point"), this, SLOT(slotSnapForward()),
              KoIconUtils::themedIcon(QStringLiteral("media-seek-forward")), Qt::ALT + Qt::Key_Right);
    addAction(QStringLiteral("delete_timeline_clip"), i18n("Delete Selected Item"), this, SLOT(slotDeleteItem()), KoIconUtils::themedIcon(QStringLiteral("edit-delete")), Qt::Key_Delete);
    addAction(QStringLiteral("align_playhead"), i18n("Align Playhead to Mouse Position"), this, SLOT(slotAlignPlayheadToMousePos()), QIcon(), Qt::Key_P);

    QAction *stickTransition = new QAction(i18n("Automatic Transition"), this);
    stickTransition->setData(QStringLiteral("auto"));
    stickTransition->setCheckable(true);
    stickTransition->setEnabled(false);
    addAction(QStringLiteral("auto_transition"), stickTransition);
    connect(stickTransition, &QAction::triggered, this, &MainWindow::slotAutoTransition);

    addAction(QStringLiteral("group_clip"), i18n("Group Clips"), this, SLOT(slotGroupClips()), KoIconUtils::themedIcon(QStringLiteral("object-group")), Qt::CTRL + Qt::Key_G);

    QAction *ungroupClip = addAction(QStringLiteral("ungroup_clip"), i18n("Ungroup Clips"), this, SLOT(slotUnGroupClips()), KoIconUtils::themedIcon(QStringLiteral("object-ungroup")), Qt::CTRL + Qt::SHIFT + Qt::Key_G);
    ungroupClip->setData("ungroup_clip");

    addAction(QStringLiteral("edit_item_duration"), i18n("Edit Duration"), this, SLOT(slotEditItemDuration()), KoIconUtils::themedIcon(QStringLiteral("measure")));
    addAction(QStringLiteral("clip_in_project_tree"), i18n("Clip in Project Bin"), this, SLOT(slotClipInProjectTree()), KoIconUtils::themedIcon(QStringLiteral("go-jump-definition")));
    addAction(QStringLiteral("overwrite_to_in_point"), i18n("Overwrite Clip Zone in Timeline"), this, SLOT(slotInsertClipOverwrite()), KoIconUtils::themedIcon(QStringLiteral("timeline-overwrite")), Qt::Key_B);
    addAction(QStringLiteral("insert_to_in_point"), i18n("Insert Clip Zone in Timeline"), this, SLOT(slotInsertClipInsert()), KoIconUtils::themedIcon(QStringLiteral("timeline-insert")), Qt::Key_V);
    addAction(QStringLiteral("remove_extract"), i18n("Extract Timeline Zone"), this, SLOT(slotExtractZone()), KoIconUtils::themedIcon(QStringLiteral("timeline-extract")), Qt::SHIFT + Qt::Key_X);
    addAction(QStringLiteral("remove_lift"), i18n("Lift Timeline Zone"), this, SLOT(slotLiftZone()), KoIconUtils::themedIcon(QStringLiteral("timeline-lift")), Qt::Key_Z);
    addAction(QStringLiteral("set_render_timeline_zone"), i18n("Add Preview Zone"), this, SLOT(slotDefinePreviewRender()), KoIconUtils::themedIcon(QStringLiteral("preview-add-zone")));
    addAction(QStringLiteral("unset_render_timeline_zone"), i18n("Remove Preview Zone"), this, SLOT(slotRemovePreviewRender()), KoIconUtils::themedIcon(QStringLiteral("preview-remove-zone")));
    addAction(QStringLiteral("clear_render_timeline_zone"), i18n("Remove All Preview Zones"), this, SLOT(slotClearPreviewRender()), KoIconUtils::themedIcon(QStringLiteral("preview-remove-all")));
    addAction(QStringLiteral("prerender_timeline_zone"), i18n("Start Preview Render"), this, SLOT(slotPreviewRender()), KoIconUtils::themedIcon(QStringLiteral("preview-render-on")), QKeySequence(Qt::SHIFT + Qt::Key_Return));
    addAction(QStringLiteral("stop_prerender_timeline"), i18n("Stop Preview Render"), this, SLOT(slotStopPreviewRender()), KoIconUtils::themedIcon(QStringLiteral("preview-render-off")));

    addAction(QStringLiteral("select_timeline_clip"), i18n("Select Clip"), this, SLOT(slotSelectTimelineClip()), KoIconUtils::themedIcon(QStringLiteral("edit-select")), Qt::Key_Plus);
    addAction(QStringLiteral("deselect_timeline_clip"), i18n("Deselect Clip"), this, SLOT(slotDeselectTimelineClip()), KoIconUtils::themedIcon(QStringLiteral("edit-select")), Qt::Key_Minus);
    addAction(QStringLiteral("select_add_timeline_clip"), i18n("Add Clip To Selection"), this, SLOT(slotSelectAddTimelineClip()),
              KoIconUtils::themedIcon(QStringLiteral("edit-select")), Qt::ALT + Qt::Key_Plus);
    addAction(QStringLiteral("select_timeline_transition"), i18n("Select Transition"), this, SLOT(slotSelectTimelineTransition()),
              KoIconUtils::themedIcon(QStringLiteral("edit-select")), Qt::SHIFT + Qt::Key_Plus);
    addAction(QStringLiteral("deselect_timeline_transition"), i18n("Deselect Transition"), this, SLOT(slotDeselectTimelineTransition()),
              KoIconUtils::themedIcon(QStringLiteral("edit-select")), Qt::SHIFT + Qt::Key_Minus);
    addAction(QStringLiteral("select_add_timeline_transition"), i18n("Add Transition To Selection"), this, SLOT(slotSelectAddTimelineTransition()),
              KoIconUtils::themedIcon(QStringLiteral("edit-select")), Qt::ALT + Qt::SHIFT + Qt::Key_Plus);
    addAction(QStringLiteral("cut_timeline_clip"), i18n("Cut Clip"), this, SLOT(slotCutTimelineClip()), KoIconUtils::themedIcon(QStringLiteral("edit-cut")), Qt::SHIFT + Qt::Key_R);
    addAction(QStringLiteral("add_clip_marker"), i18n("Add Marker"), this, SLOT(slotAddClipMarker()), KoIconUtils::themedIcon(QStringLiteral("bookmark-new")));
    addAction(QStringLiteral("delete_clip_marker"), i18n("Delete Marker"), this, SLOT(slotDeleteClipMarker()), KoIconUtils::themedIcon(QStringLiteral("edit-delete")));
    addAction(QStringLiteral("delete_all_clip_markers"), i18n("Delete All Markers"), this, SLOT(slotDeleteAllClipMarkers()), KoIconUtils::themedIcon(QStringLiteral("edit-delete")));

    QAction *editClipMarker = addAction(QStringLiteral("edit_clip_marker"), i18n("Edit Marker"), this, SLOT(slotEditClipMarker()), KoIconUtils::themedIcon(QStringLiteral("document-properties")));
    editClipMarker->setData(QStringLiteral("edit_marker"));

    addAction(QStringLiteral("add_marker_guide_quickly"), i18n("Add Marker/Guide quickly"), this, SLOT(slotAddMarkerGuideQuickly()),
              KoIconUtils::themedIcon(QStringLiteral("bookmark-new")), Qt::Key_Asterisk);

    QAction *splitAudio = addAction(QStringLiteral("split_audio"), i18n("Split Audio"), this, SLOT(slotSplitAudio()), KoIconUtils::themedIcon(QStringLiteral("document-new")));
    // "A+V" as data means this action should only be available for clips with audio AND video
    splitAudio->setData("A+V");

    QAction *setAudioAlignReference = addAction(QStringLiteral("set_audio_align_ref"), i18n("Set Audio Reference"), this, SLOT(slotSetAudioAlignReference()));
    // "A" as data means this action should only be available for clips with audio
    setAudioAlignReference->setData("A");

    QAction *alignAudio = addAction(QStringLiteral("align_audio"), i18n("Align Audio to Reference"), this, SLOT(slotAlignAudio()), QIcon());
    // "A" as data means this action should only be available for clips with audio
    alignAudio->setData("A");

    QAction *audioOnly = new QAction(KoIconUtils::themedIcon(QStringLiteral("document-new")), i18n("Audio Only"), this);
    addAction(QStringLiteral("clip_audio_only"), audioOnly);
    audioOnly->setData(PlaylistState::AudioOnly);
    audioOnly->setCheckable(true);

    QAction *videoOnly = new QAction(KoIconUtils::themedIcon(QStringLiteral("document-new")), i18n("Video Only"), this);
    addAction(QStringLiteral("clip_video_only"), videoOnly);
    videoOnly->setData(PlaylistState::VideoOnly);
    videoOnly->setCheckable(true);

    QAction *audioAndVideo = new QAction(KoIconUtils::themedIcon(QStringLiteral("document-new")), i18n("Audio and Video"), this);
    addAction(QStringLiteral("clip_audio_and_video"), audioAndVideo);
    audioAndVideo->setData(PlaylistState::Original);
    audioAndVideo->setCheckable(true);

    m_clipTypeGroup = new QActionGroup(this);
    m_clipTypeGroup->addAction(audioOnly);
    m_clipTypeGroup->addAction(videoOnly);
    m_clipTypeGroup->addAction(audioAndVideo);
    connect(m_clipTypeGroup, &QActionGroup::triggered, this, &MainWindow::slotUpdateClipType);
    m_clipTypeGroup->setEnabled(false);

    addAction(QStringLiteral("insert_space"), i18n("Insert Space"), this, SLOT(slotInsertSpace()));
    addAction(QStringLiteral("delete_space"), i18n("Remove Space"), this, SLOT(slotRemoveSpace()));
    addAction(QStringLiteral("delete_space_all_tracks"), i18n("Remove Space In All Tracks"), this, SLOT(slotRemoveAllSpace()));

    KActionCategory *timelineActions = new KActionCategory(i18n("Tracks"), actionCollection());
    QAction *insertTrack = new QAction(QIcon(), i18n("Insert Track"), this);
    connect(insertTrack, &QAction::triggered, this, &MainWindow::slotInsertTrack);
    timelineActions->addAction(QStringLiteral("insert_track"), insertTrack);

    QAction *deleteTrack = new QAction(QIcon(), i18n("Delete Track"), this);
    connect(deleteTrack, &QAction::triggered, this, &MainWindow::slotDeleteTrack);
    timelineActions->addAction(QStringLiteral("delete_track"), deleteTrack);
    deleteTrack->setData("delete_track");

    QAction *configTracks = new QAction(KoIconUtils::themedIcon(QStringLiteral("configure")), i18n("Configure Tracks"), this);
    connect(configTracks, &QAction::triggered, this, &MainWindow::slotConfigTrack);
    timelineActions->addAction(QStringLiteral("config_tracks"), configTracks);

    QAction *selectTrack = new QAction(QIcon(), i18n("Select All in Current Track"), this);
    connect(selectTrack, &QAction::triggered, this, &MainWindow::slotSelectTrack);
    timelineActions->addAction(QStringLiteral("select_track"), selectTrack);

    QAction *selectAll = KStandardAction::selectAll(this, SLOT(slotSelectAllTracks()), this);
    selectAll->setIcon(KoIconUtils::themedIcon(QStringLiteral("kdenlive-select-all")));
    selectAll->setShortcutContext(Qt::WidgetWithChildrenShortcut);
    timelineActions->addAction(QStringLiteral("select_all_tracks"), selectAll);

    QAction *unselectAll = KStandardAction::deselect(this, SLOT(slotUnselectAllTracks()), this);
    unselectAll->setIcon(KoIconUtils::themedIcon(QStringLiteral("kdenlive-unselect-all")));
    unselectAll->setShortcutContext(Qt::WidgetWithChildrenShortcut);
    timelineActions->addAction(QStringLiteral("unselect_all_tracks"), unselectAll);

    kdenliveCategoryMap.insert(QStringLiteral("timeline"), timelineActions);

    // Cached data management
    addAction(QStringLiteral("manage_cache"), i18n("Manage Cached Data"), this, SLOT(slotManageCache()), KoIconUtils::themedIcon(QStringLiteral("network-server-database")));

    QAction *disablePreview = new QAction(i18n("Disable Timeline Preview"), this);
    disablePreview->setCheckable(true);
    addAction(QStringLiteral("disable_preview"), disablePreview);

    addAction(QStringLiteral("add_guide"), i18n("Add Guide"), this, SLOT(slotAddGuide()), KoIconUtils::themedIcon(QStringLiteral("list-add")));
    addAction(QStringLiteral("delete_guide"), i18n("Delete Guide"), this, SLOT(slotDeleteGuide()), KoIconUtils::themedIcon(QStringLiteral("edit-delete")));
    addAction(QStringLiteral("edit_guide"), i18n("Edit Guide"), this, SLOT(slotEditGuide()), KoIconUtils::themedIcon(QStringLiteral("document-properties")));
    addAction(QStringLiteral("delete_all_guides"), i18n("Delete All Guides"), this, SLOT(slotDeleteAllGuides()), KoIconUtils::themedIcon(QStringLiteral("edit-delete")));

    QAction *pasteEffects = addAction(QStringLiteral("paste_effects"), i18n("Paste Effects"), this, SLOT(slotPasteEffects()), KoIconUtils::themedIcon(QStringLiteral("edit-paste")));
    pasteEffects->setData("paste_effects");

    m_saveAction = KStandardAction::save(pCore->projectManager(), SLOT(saveFile()), actionCollection());
    m_saveAction->setIcon(KoIconUtils::themedIcon(QStringLiteral("document-save")));

    addAction(QStringLiteral("save_selection"), i18n("Save Selection"), pCore->projectManager(), SLOT(slotSaveSelection()), KoIconUtils::themedIcon(QStringLiteral("document-save")));

    QAction *sentToLibrary = addAction(QStringLiteral("send_library"), i18n("Add Timeline Selection to Library"), pCore->library(), SLOT(slotAddToLibrary()), KoIconUtils::themedIcon(QStringLiteral("bookmark-new")));
    pCore->library()->setupActions(QList<QAction *>() << sentToLibrary);

    KStandardAction::showMenubar(this, SLOT(showMenuBar(bool)), actionCollection());

    QAction *a = KStandardAction::quit(this, SLOT(close()), actionCollection());
    a->setIcon(KoIconUtils::themedIcon(QStringLiteral("application-exit")));
    // TODO: make the following connection to slotEditKeys work
    //KStandardAction::keyBindings(this,            SLOT(slotEditKeys()),           actionCollection());
    a = KStandardAction::preferences(this, SLOT(slotPreferences()),        actionCollection());
    a->setIcon(KoIconUtils::themedIcon(QStringLiteral("configure")));
    a = KStandardAction::configureNotifications(this, SLOT(configureNotifications()), actionCollection());
    a->setIcon(KoIconUtils::themedIcon(QStringLiteral("configure")));
    a = KStandardAction::copy(this,                   SLOT(slotCopy()),               actionCollection());
    a->setIcon(KoIconUtils::themedIcon(QStringLiteral("edit-copy")));
    a = KStandardAction::paste(this,                  SLOT(slotPaste()),              actionCollection());
    a->setIcon(KoIconUtils::themedIcon(QStringLiteral("edit-paste")));
    a = KStandardAction::fullScreen(this,             SLOT(slotFullScreen()), this,   actionCollection());
    a->setIcon(KoIconUtils::themedIcon(QStringLiteral("view-fullscreen")));

    QAction *undo = KStandardAction::undo(m_commandStack, SLOT(undo()), actionCollection());
    undo->setIcon(KoIconUtils::themedIcon(QStringLiteral("edit-undo")));
    undo->setEnabled(false);
    connect(m_commandStack, &QUndoGroup::canUndoChanged, undo, &QAction::setEnabled);

    QAction *redo = KStandardAction::redo(m_commandStack, SLOT(redo()), actionCollection());
    redo->setIcon(KoIconUtils::themedIcon(QStringLiteral("edit-redo")));
    redo->setEnabled(false);
    connect(m_commandStack, &QUndoGroup::canRedoChanged, redo, &QAction::setEnabled);

    QMenu *addClips = new QMenu(this);

    QAction *addClip = addAction(QStringLiteral("add_clip"), i18n("Add Clip"), pCore->bin(), SLOT(slotAddClip()), KoIconUtils::themedIcon(QStringLiteral("kdenlive-add-clip")));
    addClips->addAction(addClip);
    QAction *action = addAction(QStringLiteral("add_color_clip"), i18n("Add Color Clip"), pCore->bin(), SLOT(slotCreateProjectClip()), KoIconUtils::themedIcon(QStringLiteral("kdenlive-add-color-clip")));
    action->setData((int) Color);
    addClips->addAction(action);
    action = addAction(QStringLiteral("add_slide_clip"), i18n("Add Slideshow Clip"), pCore->bin(), SLOT(slotCreateProjectClip()), KoIconUtils::themedIcon(QStringLiteral("kdenlive-add-slide-clip")));
    action->setData((int) SlideShow);
    addClips->addAction(action);
    action = addAction(QStringLiteral("add_text_clip"), i18n("Add Title Clip"), pCore->bin(), SLOT(slotCreateProjectClip()), KoIconUtils::themedIcon(QStringLiteral("kdenlive-add-text-clip")));
    action->setData((int) Text);
    addClips->addAction(action);
    action = addAction(QStringLiteral("add_text_template_clip"), i18n("Add Template Title"), pCore->bin(), SLOT(slotCreateProjectClip()), KoIconUtils::themedIcon(QStringLiteral("kdenlive-add-text-clip")));
    action->setData((int) TextTemplate);
    addClips->addAction(action);
    /*action = addAction(QStringLiteral("add_qtext_clip"), i18n("Add Simple Text Clip"), pCore->bin(), SLOT(slotCreateProjectClip()), KoIconUtils::themedIcon(QStringLiteral("kdenlive-add-text-clip")));
    action->setData((int) QText);
    addClips->addAction(action);*/

    QAction *addFolder = addAction(QStringLiteral("add_folder"), i18n("Create Folder"), pCore->bin(), SLOT(slotAddFolder()), KoIconUtils::themedIcon(QStringLiteral("folder-new")));
    addClips->addAction(addAction(QStringLiteral("download_resource"), i18n("Online Resources"), this, SLOT(slotDownloadResources()), KoIconUtils::themedIcon(QStringLiteral("edit-download"))));

    QAction *clipProperties = addAction(QStringLiteral("clip_properties"), i18n("Clip Properties"), pCore->bin(), SLOT(slotSwitchClipProperties()), KoIconUtils::themedIcon(QStringLiteral("document-edit")));
    clipProperties->setData("clip_properties");

    QAction *openClip = addAction(QStringLiteral("edit_clip"), i18n("Edit Clip"), pCore->bin(), SLOT(slotOpenClip()), KoIconUtils::themedIcon(QStringLiteral("document-open")));
    openClip->setData("edit_clip");
    openClip->setEnabled(false);

    QAction *deleteClip = addAction(QStringLiteral("delete_clip"), i18n("Delete Clip"), pCore->bin(), SLOT(slotDeleteClip()), KoIconUtils::themedIcon(QStringLiteral("edit-delete")));
    deleteClip->setData("delete_clip");
    deleteClip->setEnabled(false);

    QAction *reloadClip = addAction(QStringLiteral("reload_clip"), i18n("Reload Clip"), pCore->bin(), SLOT(slotReloadClip()), KoIconUtils::themedIcon(QStringLiteral("view-refresh")));
    reloadClip->setData("reload_clip");
    reloadClip->setEnabled(false);

    QAction *disableEffects = addAction(QStringLiteral("disable_timeline_effects"), i18n("Disable Timeline Effects"), pCore->projectManager(), SLOT(slotDisableTimelineEffects(bool)), KoIconUtils::themedIcon(QStringLiteral("favorite")));
    disableEffects->setData("disable_timeline_effects");
    disableEffects->setCheckable(true);
    disableEffects->setChecked(false);

    QAction *locateClip = addAction(QStringLiteral("locate_clip"), i18n("Locate Clip..."), pCore->bin(), SLOT(slotLocateClip()), KoIconUtils::themedIcon(QStringLiteral("edit-file")));
    locateClip->setData("locate_clip");
    locateClip->setEnabled(false);

    QAction *duplicateClip = addAction(QStringLiteral("duplicate_clip"), i18n("Duplicate Clip"), pCore->bin(), SLOT(slotDuplicateClip()), KoIconUtils::themedIcon(QStringLiteral("edit-copy")));
    duplicateClip->setData("duplicate_clip");
    duplicateClip->setEnabled(false);

    QAction *proxyClip = new QAction(i18n("Proxy Clip"), this);
    addAction(QStringLiteral("proxy_clip"), proxyClip);
    proxyClip->setData(QStringList() << QString::number((int) AbstractClipJob::PROXYJOB));
    proxyClip->setCheckable(true);
    proxyClip->setChecked(false);

    //TODO: port stopmotion to new Monitor code
    //addAction("stopmotion", i18n("Stop Motion Capture"), this, SLOT(slotOpenStopmotion()), KoIconUtils::themedIcon("image-x-generic"));
    addAction(QStringLiteral("switch_track_lock"), i18n("Toggle Track Lock"), pCore->projectManager(), SLOT(slotSwitchTrackLock()), QIcon(), Qt::SHIFT + Qt::Key_L);
    addAction(QStringLiteral("switch_all_track_lock"), i18n("Toggle All Track Lock"), pCore->projectManager(), SLOT(slotSwitchAllTrackLock()), QIcon(), Qt::CTRL + Qt::SHIFT + Qt::Key_L);
    addAction(QStringLiteral("switch_track_target"), i18n("Toggle Track Target"), pCore->projectManager(), SLOT(slotSwitchTrackTarget()), QIcon(), Qt::SHIFT + Qt::Key_T);

    QHash<QString, QAction *> actions;
    actions.insert(QStringLiteral("locate"), locateClip);
    actions.insert(QStringLiteral("reload"), reloadClip);
    actions.insert(QStringLiteral("duplicate"), duplicateClip);
    actions.insert(QStringLiteral("proxy"), proxyClip);
    actions.insert(QStringLiteral("properties"), clipProperties);
    actions.insert(QStringLiteral("open"), openClip);
    actions.insert(QStringLiteral("delete"), deleteClip);
    actions.insert(QStringLiteral("folder"), addFolder);
    pCore->bin()->setupMenu(addClips, addClip, actions);

    // Setup effects and transitions actions.
    KActionCategory *transitionActions = new KActionCategory(i18n("Transitions"), actionCollection());
    //m_transitions = new QAction*[transitions.count()];
    for (int i = 0; i < transitions.count(); ++i) {
        QStringList effectInfo = transitions.effectIdInfo(i);
        if (effectInfo.isEmpty()) {
            continue;
        }
        QAction *a = new QAction(effectInfo.at(0), this);
        a->setData(effectInfo);
        a->setIconVisibleInMenu(false);
        m_transitions << a;
        QString id = effectInfo.at(2);
        if (id.isEmpty()) {
            id = effectInfo.at(1);
        }
        transitionActions->addAction("transition_" + id, a);
    }
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
        QDir dir(QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation));
        dir.mkpath(QStringLiteral("."));
        KdenliveSettings::setDefaultprojectfolder(dir.absolutePath());
    }
    if (KdenliveSettings::trackheight() == 0) {
        QFontMetrics metrics(font());
        int trackHeight = 2 * metrics.height();
        QStyle *style = qApp->style();
        trackHeight += style->pixelMetric(QStyle::PM_ToolBarIconSize) + 2 * style->pixelMetric(QStyle::PM_ToolBarItemMargin) + style->pixelMetric(QStyle::PM_ToolBarItemSpacing) + 2;
        KdenliveSettings::setTrackheight(trackHeight);
    }
    if (KdenliveSettings::trackheight() == 0) {
        KdenliveSettings::setTrackheight(50);
    }
    bool firstRun = false;
    KConfigGroup initialGroup(config, "version");
    if (!initialGroup.exists()) {
        // First run, check if user is on a KDE Desktop
        firstRun = true;
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
    initialGroup.writeEntry("version", version);
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
    KdenliveSettingsDialog *d = static_cast <KdenliveSettingsDialog *>(KConfigDialog::exists(QStringLiteral("settings")));
    if (d) {
        d->checkProfile();
    }
}

void MainWindow::slotEditProjectSettings()
{
    KdenliveDoc *project = pCore->projectManager()->current();
    QPoint p = pCore->projectManager()->currentTimeline()->getTracksCount();

    ProjectSettings* w = new ProjectSettings(project, project->metadata(), pCore->projectManager()->currentTimeline()->projectView()->extractTransitionsLumas(), p.x(), p.y(), project->projectTempFolder(), true, !project->isModified(), this);
    connect(w, &ProjectSettings::disableProxies, this, &MainWindow::slotDisableProxies);
    connect(w, SIGNAL(disablePreview()), pCore->projectManager()->currentTimeline(), SLOT(invalidateRange()));
    connect(w, &ProjectSettings::refreshProfiles, this, &MainWindow::slotRefreshProfiles);

    if (w->exec() == QDialog::Accepted) {
        QString profile = w->selectedProfile();
        //project->setProjectFolder(w->selectedFolder());
        pCore->projectManager()->currentTimeline()->updatePreviewSettings(w->selectedPreview());
        bool modified = false;
        if (m_recMonitor) {
            m_recMonitor->slotUpdateCaptureFolder(project->projectDataFolder() + QDir::separator());
        }
        if (m_renderWidget) {
            m_renderWidget->setDocumentPath(project->projectDataFolder() + QDir::separator());
        }
        if (KdenliveSettings::videothumbnails() != w->enableVideoThumbs()) {
            slotSwitchVideoThumbs();
        }
        if (KdenliveSettings::audiothumbnails() != w->enableAudioThumbs()) {
            slotSwitchAudioThumbs();
        }
        if (project->profilePath() != profile || project->profileChanged(profile)) {
            KdenliveSettings::setCurrent_profile(profile);
            pCore->projectManager()->slotResetProfiles();
            slotUpdateDocumentState(true);
        }
        if (project->getDocumentProperty(QStringLiteral("proxyparams")) != w->proxyParams() || project->getDocumentProperty(QStringLiteral("proxyextension")) != w->proxyExtension()) {
            modified = true;
            project->setDocumentProperty(QStringLiteral("proxyparams"), w->proxyParams());
            project->setDocumentProperty(QStringLiteral("proxyextension"), w->proxyExtension());
            if (pCore->binController()->clipCount() > 0 && KMessageBox::questionYesNo(this, i18n("You have changed the proxy parameters. Do you want to recreate all proxy clips for this project?")) == KMessageBox::Yes) {
                //TODO: rebuild all proxies
                pCore->bin()->rebuildProxies();
            }
        }
        if (project->getDocumentProperty(QStringLiteral("generateproxy")) != QString::number((int) w->generateProxy())) {
            modified = true;
            project->setDocumentProperty(QStringLiteral("generateproxy"), QString::number((int) w->generateProxy()));
        }
        if (project->getDocumentProperty(QStringLiteral("proxyminsize")) != QString::number(w->proxyMinSize())) {
            modified = true;
            project->setDocumentProperty(QStringLiteral("proxyminsize"), QString::number(w->proxyMinSize()));
        }
        if (project->getDocumentProperty(QStringLiteral("generateimageproxy")) != QString::number((int) w->generateImageProxy())) {
            modified = true;
            project->setDocumentProperty(QStringLiteral("generateimageproxy"), QString::number((int) w->generateImageProxy()));
        }
        if (project->getDocumentProperty(QStringLiteral("proxyimageminsize")) != QString::number(w->proxyImageMinSize())) {
            modified = true;
            project->setDocumentProperty(QStringLiteral("proxyimageminsize"), QString::number(w->proxyImageMinSize()));
        }
        if (QString::number((int) w->useProxy()) != project->getDocumentProperty(QStringLiteral("enableproxy"))) {
            project->setDocumentProperty(QStringLiteral("enableproxy"), QString::number((int) w->useProxy()));
            modified = true;
            slotUpdateProxySettings();
        }
        if (w->metadata() != project->metadata()) {
            project->setMetadata(w->metadata());
        }
        QString newProjectFolder = w->storageFolder();
        if (newProjectFolder.isEmpty()) {
            newProjectFolder = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
        }
        if (newProjectFolder != project->projectTempFolder()) {
            KMessageBox::ButtonCode answer;
            // Project folder changed:
            if (project->isModified()) {
                answer = KMessageBox::warningContinueCancel(this, i18n("The current project has not been saved. This will first save the project, then move all temporary files from <b>%1</b> to <b>%2</b>, and the project file will be reloaded", project->projectTempFolder(), newProjectFolder));
                if (answer == KMessageBox::Continue) {
                    pCore->projectManager()->saveFile();
                }
            } else {
                answer = KMessageBox::warningContinueCancel(this, i18n("This will move all temporary files from <b>%1</b> to <b>%2</b>, the project file will then be reloaded", project->projectTempFolder(), newProjectFolder));
            }
            if (answer == KMessageBox::Continue) {
                // Proceeed with move
                QString documentId = QDir::cleanPath(project->getDocumentProperty(QStringLiteral("documentid")));
                bool ok;
                documentId.toLongLong(&ok, 10);
                if (!ok || documentId.isEmpty()) {
                    KMessageBox::sorry(this, i18n("Cannot perform operation, invalid document id: %1", documentId));
                } else {
                    QDir newDir(newProjectFolder);
                    QDir oldDir(project->projectTempFolder());
                    if (newDir.exists(documentId)) {
                        KMessageBox::sorry(this, i18n("Cannot perform operation, target directory already exists: %1", newDir.absoluteFilePath(documentId)));
                    } else {
                        // Proceed with the move
                        pCore->projectManager()->moveProjectData(oldDir.absoluteFilePath(documentId), newDir.absolutePath());
                    }
                }
            }
        }
        if (modified) {
            project->setModified();
        }
    }
    delete w;
}

void MainWindow::slotDisableProxies()
{
    pCore->projectManager()->current()->setDocumentProperty(QStringLiteral("enableproxy"), QString::number((int) false));
    pCore->projectManager()->current()->setModified();
    slotUpdateProxySettings();
}

void MainWindow::slotStopRenderProject()
{
    if (m_renderWidget) {
        m_renderWidget->slotAbortCurrentJob();
    }
}

void MainWindow::slotRenderProject()
{
    KdenliveDoc *project = pCore->projectManager()->current();

    if (!m_renderWidget) {
        QString projectfolder = project ? project->projectDataFolder() + QDir::separator() : KdenliveSettings::defaultprojectfolder();
        MltVideoProfile profile;
        if (project) {
            profile = project->mltProfile();
            m_renderWidget = new RenderWidget(projectfolder, project->useProxy(), profile.path, this);
            connect(m_renderWidget, &RenderWidget::shutdown, this, &MainWindow::slotShutdown);
            connect(m_renderWidget, &RenderWidget::selectedRenderProfile, this, &MainWindow::slotSetDocumentRenderProfile);
            connect(m_renderWidget, &RenderWidget::prepareRenderingData, this, &MainWindow::slotPrepareRendering);
            connect(m_renderWidget, &RenderWidget::abortProcess, this, &MainWindow::abortRenderJob);
            connect(m_renderWidget, &RenderWidget::openDvdWizard, this, &MainWindow::slotDvdWizard);
            m_renderWidget->setProfile(project->mltProfile().path);
            m_renderWidget->setGuides(pCore->projectManager()->currentTimeline()->projectView()->guidesData(), project->projectDuration());
            m_renderWidget->setDocumentPath(project->projectDataFolder() + QDir::separator());
            m_renderWidget->setRenderProfile(project->getRenderProperties());
        }
        if (m_compositeAction->currentAction()) {
            m_renderWidget->errorMessage(RenderWidget::CompositeError, m_compositeAction->currentAction()->data().toInt() == 1 ? i18n("Rendering using low quality track compositing") : QString());
        }
    }
    slotCheckRenderStatus();
    m_renderWidget->show();
    //m_renderWidget->showNormal();

    // What are the following lines supposed to do?
    //pCore->projectManager()->currentTimeline()->tracksNumber();
    //m_renderWidget->enableAudio(false);
    //m_renderWidget->export_audio;
}

void MainWindow::slotCheckRenderStatus()
{
    // Make sure there are no missing clips
    //TODO
    /*if (m_renderWidget)
        m_renderWidget->missingClips(pCore->bin()->hasMissingClips());*/
}

void MainWindow::setRenderingProgress(const QString &url, int progress)
{
    emit setRenderProgress(progress);
    if (m_renderWidget) {
        m_renderWidget->setRenderJob(url, progress);
    }
}

void MainWindow::setRenderingFinished(const QString &url, int status, const QString &error)
{
    emit setRenderProgress(100);
    if (m_renderWidget) {
        m_renderWidget->setRenderStatus(url, status, error);
    }
}

void MainWindow::addProjectClip(const QString &url)
{
    if (pCore->projectManager()->current()) {
        QStringList ids = pCore->binController()->getBinIdsByResource(QFileInfo(url));
        if (!ids.isEmpty()) {
            // Clip is already in project bin, abort
            return;
        }
        ClipCreationDialog::createClipsCommand(pCore->projectManager()->current(), QList<QUrl>() << QUrl::fromLocalFile(url), QStringList(), pCore->bin());
    }
}

void MainWindow::addTimelineClip(const QString &url)
{
    if (pCore->projectManager()->current()) {
        QStringList ids = pCore->binController()->getBinIdsByResource(QFileInfo(url));
        if (!ids.isEmpty()) {
            pCore->bin()->selectClipById(ids.first());
            slotInsertClipInsert();
        }
    }
}

void MainWindow::addEffect(const QString &effectName)
{
    QStringList effectInfo;
    effectInfo << effectName << effectName;
    const QDomElement effect = EffectsListWidget::itemEffect(5, effectInfo);
    if (!effect.isNull()) {
        slotAddEffect(effect);
    } else {
        qCDebug(KDENLIVE_LOG) << " * * *EFFECT: " << effectName << " NOT AVAILABLE";
        exitApp();
    }
}

void MainWindow::scriptRender(const QString &url)
{
    slotRenderProject();
    m_renderWidget->slotPrepareExport(true, url);
}

void MainWindow::exitApp()
{
    QApplication::exit(0);
}

void MainWindow::slotCleanProject()
{
    if (KMessageBox::warningContinueCancel(this, i18n("This will remove all unused clips from your project."), i18n("Clean up project")) == KMessageBox::Cancel) {
        return;
    }
    pCore->bin()->cleanup();
}

void MainWindow::slotUpdateMousePosition(int pos)
{
    if (pCore->projectManager()->current()) {
        switch (m_timeFormatButton->currentItem()) {
        case 0:
            m_timeFormatButton->setText(pCore->projectManager()->current()->timecode().getTimecodeFromFrames(pos) + QStringLiteral(" / ") + pCore->projectManager()->current()->timecode().getTimecodeFromFrames(pCore->projectManager()->currentTimeline()->duration()));
            break;
        default:
            m_timeFormatButton->setText(QStringLiteral("%1 / %2").arg(pos, 6, 10, QLatin1Char('0')).arg(pCore->projectManager()->currentTimeline()->duration(), 6, 10, QLatin1Char('0')));
        }
    }
}

void MainWindow::slotUpdateProjectDuration(int pos)
{
    if (pCore->projectManager()->current()) {
        pCore->projectManager()->currentTimeline()->setDuration(pos);
        slotUpdateMousePosition(pCore->projectManager()->currentTimeline()->projectView()->getMousePos());
    }
}

void MainWindow::slotUpdateDocumentState(bool modified)
{
    setWindowTitle(pCore->projectManager()->current()->description());
    setWindowModified(modified);
    m_saveAction->setEnabled(modified);
}

void MainWindow::connectDocument()
{
    KdenliveDoc *project = pCore->projectManager()->current();
    Timeline *trackView = pCore->projectManager()->currentTimeline();
    connect(project, &KdenliveDoc::startAutoSave, pCore->projectManager(), &ProjectManager::slotStartAutoSave);
    connect(project, &KdenliveDoc::reloadEffects, this, &MainWindow::slotReloadEffects);
    KdenliveSettings::setProject_fps(project->fps());
    m_clipMonitorDock->raise();
    m_effectStack->transitionConfig()->updateProjectFormat();

    connect(trackView, &Timeline::configTrack, this, &MainWindow::slotConfigTrack);
    connect(trackView, &Timeline::updateTracksInfo, this, &MainWindow::slotUpdateTrackInfo);
    connect(trackView, &Timeline::mousePosition, this, &MainWindow::slotUpdateMousePosition);
    connect(pCore->producerQueue(), &ProducerQueue::infoProcessingFinished, trackView->projectView(), &CustomTrackView::slotInfoProcessingFinished, Qt::DirectConnection);

    connect(trackView->projectView(), &CustomTrackView::importKeyframes, this, &MainWindow::slotProcessImportKeyframes);
    connect(trackView->projectView(), &CustomTrackView::updateTrimMode, this, &MainWindow::setTrimMode);
    connect(m_projectMonitor, &Monitor::multitrackView, trackView, &Timeline::slotMultitrackView);
    connect(m_projectMonitor, SIGNAL(renderPosition(int)), trackView, SLOT(moveCursorPos(int)));
    connect(m_projectMonitor, SIGNAL(zoneUpdated(QPoint)), trackView, SLOT(slotSetZone(QPoint)));
    connect(m_projectMonitor, SIGNAL(zoneUpdated(QPoint)), project, SLOT(setModified()));
    connect(m_clipMonitor, SIGNAL(zoneUpdated(QPoint)), project, SLOT(setModified()));

    connect(project, &KdenliveDoc::docModified, this, &MainWindow::slotUpdateDocumentState);
    connect(trackView->projectView(), &CustomTrackView::guidesUpdated, this, &MainWindow::slotGuidesUpdated);
    connect(trackView->projectView(), &CustomTrackView::loadMonitorScene, m_projectMonitor, &Monitor::slotShowEffectScene);
    connect(trackView->projectView(), &CustomTrackView::setQmlProperty, m_projectMonitor, &Monitor::setQmlProperty);
    connect(m_projectMonitor, SIGNAL(acceptRipple(bool)), trackView->projectView(), SLOT(slotAcceptRipple(bool)));
    connect(m_projectMonitor, SIGNAL(switchTrimMode(int)), trackView->projectView(), SLOT(switchTrimMode(int)));
    connect(project, &KdenliveDoc::saveTimelinePreview, trackView, &Timeline::slotSaveTimelinePreview);

    connect(trackView, SIGNAL(showTrackEffects(int, TrackInfo)), this, SLOT(slotTrackSelected(int, TrackInfo)));

    connect(trackView->projectView(), &CustomTrackView::clipItemSelected, this, &MainWindow::slotTimelineClipSelected, Qt::DirectConnection);
    connect(trackView->projectView(), &CustomTrackView::setActiveKeyframe, m_effectStack, &EffectStackView2::setActiveKeyframe);
    connect(trackView->projectView(), SIGNAL(transitionItemSelected(Transition *, int, QPoint, bool)), m_effectStack, SLOT(slotTransitionItemSelected(Transition *, int, QPoint, bool)), Qt::DirectConnection);

    connect(trackView->projectView(), SIGNAL(transitionItemSelected(Transition *, int, QPoint, bool)), this, SLOT(slotActivateTransitionView(Transition *)));

    connect(trackView->projectView(), &CustomTrackView::zoomIn, this, &MainWindow::slotZoomIn);
    connect(trackView->projectView(), &CustomTrackView::zoomOut, this, &MainWindow::slotZoomOut);
    connect(trackView, SIGNAL(setZoom(int)), this, SLOT(slotSetZoom(int)));

    connect(trackView, SIGNAL(displayMessage(QString, MessageType)), m_messageLabel, SLOT(setMessage(QString, MessageType)));
    connect(trackView->projectView(), SIGNAL(displayMessage(QString, MessageType)), m_messageLabel, SLOT(setMessage(QString, MessageType)));
    connect(pCore->bin(), &Bin::clipNameChanged, trackView->projectView(), &CustomTrackView::clipNameChanged);
    connect(pCore->bin(), SIGNAL(displayMessage(QString, int, MessageType)), m_messageLabel, SLOT(setProgressMessage(QString, int, MessageType)));

    connect(trackView->projectView(), SIGNAL(showClipFrame(QString, int)), pCore->bin(), SLOT(selectClipById(QString, int)));
    connect(trackView->projectView(), SIGNAL(playMonitor()), m_projectMonitor, SLOT(slotPlay()));
    connect(trackView->projectView(), &CustomTrackView::pauseMonitor, m_projectMonitor, &Monitor::pause, Qt::DirectConnection);
    connect(m_projectMonitor, &Monitor::addEffect, trackView->projectView(), &CustomTrackView::slotAddEffectToCurrentItem);

    connect(trackView->projectView(), SIGNAL(transitionItemSelected(Transition *, int, QPoint, bool)), m_projectMonitor, SLOT(slotSetSelectedClip(Transition *)));

    connect(pCore->bin(), SIGNAL(gotFilterJobResults(QString, int, int, stringMap, stringMap)), trackView->projectView(), SLOT(slotGotFilterJobResults(QString, int, int, stringMap, stringMap)));

    //TODO
    //connect(m_projectList, SIGNAL(addMarkers(QString,QList<CommentedTime>)), trackView->projectView(), SLOT(slotAddClipMarker(QString,QList<CommentedTime>)));

    // Effect stack signals
    connect(m_effectStack, &EffectStackView2::updateEffect, trackView->projectView(), &CustomTrackView::slotUpdateClipEffect);
    connect(m_effectStack, &EffectStackView2::updateClipRegion, trackView->projectView(), &CustomTrackView::slotUpdateClipRegion);
    connect(m_effectStack, SIGNAL(removeEffect(ClipItem *, int, QDomElement)), trackView->projectView(), SLOT(slotDeleteEffect(ClipItem *, int, QDomElement)));
    connect(m_effectStack, SIGNAL(removeEffectGroup(ClipItem *, int, QDomDocument)), trackView->projectView(), SLOT(slotDeleteEffectGroup(ClipItem *, int, QDomDocument)));

    connect(m_effectStack, SIGNAL(addEffect(ClipItem *, QDomElement, int)), trackView->projectView(), SLOT(slotAddEffect(ClipItem *, QDomElement, int)));
    connect(m_effectStack, SIGNAL(changeEffectState(ClipItem *, int, QList<int>, bool)), trackView->projectView(), SLOT(slotChangeEffectState(ClipItem *, int, QList<int>, bool)));
    connect(m_effectStack, SIGNAL(changeEffectPosition(ClipItem *, int, QList<int>, int)), trackView->projectView(), SLOT(slotChangeEffectPosition(ClipItem *, int, QList<int>, int)));

    connect(m_effectStack, &EffectStackView2::refreshEffectStack, trackView->projectView(), &CustomTrackView::slotRefreshEffects);
    connect(m_effectStack, &EffectStackView2::seekTimeline, trackView->projectView(), &CustomTrackView::seekCursorPos);
    connect(m_effectStack, SIGNAL(importClipKeyframes(GraphicsRectItem, ItemInfo, QDomElement, QMap<QString, QString>)), trackView->projectView(), SLOT(slotImportClipKeyframes(GraphicsRectItem, ItemInfo, QDomElement, QMap<QString, QString>)));

    // Transition config signals
    connect(m_effectStack->transitionConfig(), SIGNAL(transitionUpdated(Transition *, QDomElement)), trackView->projectView(), SLOT(slotTransitionUpdated(Transition *, QDomElement)));
    connect(m_effectStack->transitionConfig(), &TransitionSettings::seekTimeline, trackView->projectView(), &CustomTrackView::seekCursorPos);

    connect(trackView->projectView(), SIGNAL(activateDocumentMonitor()), m_projectMonitor, SLOT(slotActivateMonitor()), Qt::DirectConnection);
    connect(project, &KdenliveDoc::updateFps, trackView, &Timeline::updateProfile, Qt::DirectConnection);
    connect(trackView, &Timeline::zoneMoved, this, &MainWindow::slotZoneMoved);
    trackView->projectView()->setContextMenu(m_timelineContextMenu, m_timelineContextClipMenu, m_timelineContextTransitionMenu, m_clipTypeGroup, static_cast<QMenu *>(factory()->container(QStringLiteral("marker_menu"), this)));
    if (m_renderWidget) {
        slotCheckRenderStatus();
        m_renderWidget->setProfile(project->mltProfile().path);
        m_renderWidget->setGuides(pCore->projectManager()->currentTimeline()->projectView()->guidesData(), project->projectDuration());
        m_renderWidget->setDocumentPath(project->projectDataFolder() + QDir::separator());
        m_renderWidget->setRenderProfile(project->getRenderProperties());
    }
    m_zoomSlider->setValue(project->zoom().x());
    m_commandStack->setActiveStack(project->commandStack());
    KdenliveSettings::setProject_display_ratio(project->dar());

    setWindowTitle(project->description());
    setWindowModified(project->isModified());
    m_saveAction->setEnabled(project->isModified());
    m_normalEditTool->setChecked(true);
    connect(m_projectMonitor, &Monitor::durationChanged, this, &MainWindow::slotUpdateProjectDuration);
    pCore->monitorManager()->setDocument(project);
    trackView->updateProfile(1.0);
    if (m_recMonitor) {
        m_recMonitor->slotUpdateCaptureFolder(project->projectDataFolder() + QDir::separator());
    }
    // Init document zone
    m_projectMonitor->slotZoneMoved(trackView->inPoint(), trackView->outPoint());

    //Update the mouse position display so it will display in DF/NDF format by default based on the project setting.
    slotUpdateMousePosition(0);

    // Update guides info in render widget
    slotGuidesUpdated();

    // set tool to select tool
    setTrimMode(QStringLiteral());
    m_buttonSelectTool->setChecked(true);
    connect(m_projectMonitorDock, &QDockWidget::visibilityChanged, m_projectMonitor, &Monitor::slotRefreshMonitor, Qt::UniqueConnection);
    connect(m_clipMonitorDock, &QDockWidget::visibilityChanged, m_clipMonitor, &Monitor::slotRefreshMonitor, Qt::UniqueConnection);
}

void MainWindow::slotZoneMoved(int start, int end)
{
    pCore->projectManager()->current()->setZone(start, end);
    m_projectMonitor->slotZoneMoved(start, end);
}

void MainWindow::slotGuidesUpdated()
{
    QMap<double, QString> guidesData = pCore->projectManager()->currentTimeline()->projectView()->guidesData();
    if (m_renderWidget) {
        m_renderWidget->setGuides(guidesData, pCore->projectManager()->current()->projectDuration());
    }
    m_projectMonitor->setGuides(guidesData);
}

void MainWindow::slotEditKeys()
{
    KShortcutsDialog dialog(KShortcutsEditor::AllActions, KShortcutsEditor::LetterShortcutsAllowed, this);
    dialog.addCollection(actionCollection(), i18nc("general keyboard shortcuts", "General"));
    dialog.configure();
}

void MainWindow::slotPreferences(int page, int option)
{
    /*
     * An instance of your dialog could be already created and could be
     * cached, in which case you want to display the cached dialog
     * instead of creating another one
     */
    if (m_stopmotion) {
        m_stopmotion->slotLive(false);
    }

    if (KConfigDialog::showDialog(QStringLiteral("settings"))) {
        KdenliveSettingsDialog *d = static_cast <KdenliveSettingsDialog *>(KConfigDialog::exists(QStringLiteral("settings")));
        if (page != -1) {
            d->showPage(page, option);
        }
        return;
    }

    // KConfigDialog didn't find an instance of this dialog, so lets
    // create it :

    // Get the mappable actions in localized form
    QMap<QString, QString> actions;
    KActionCollection *collection = actionCollection();
    QRegExp ampEx("&{1,1}");
    foreach (const QString &action_name, m_actionNames) {
        QString action_text = collection->action(action_name)->text();
        action_text.remove(ampEx);
        actions[action_text] = action_name;
    }

    KdenliveSettingsDialog *dialog = new KdenliveSettingsDialog(actions, m_gpuAllowed, this);
    connect(dialog, &KConfigDialog::settingsChanged, this, &MainWindow::updateConfiguration);
    connect(dialog, &KConfigDialog::settingsChanged, this, &MainWindow::configurationChanged);
    connect(dialog, &KdenliveSettingsDialog::doResetProfile, pCore->projectManager(), &ProjectManager::slotResetProfiles);
    connect(dialog, &KdenliveSettingsDialog::checkTabPosition, this, &MainWindow::slotCheckTabPosition);
    connect(dialog, &KdenliveSettingsDialog::restartKdenlive, this, &MainWindow::slotRestart);
    connect(dialog, &KdenliveSettingsDialog::updateLibraryFolder, pCore, &Core::updateLibraryPath);

    if (m_recMonitor) {
        connect(dialog, &KdenliveSettingsDialog::updateCaptureFolder, this, &MainWindow::slotUpdateCaptureFolder);
        connect(dialog, &KdenliveSettingsDialog::updateFullScreenGrab, m_recMonitor, &RecMonitor::slotUpdateFullScreenGrab);
    }
    dialog->show();
    if (page != -1) {
        dialog->showPage(page, option);
    }
}

void MainWindow::slotCheckTabPosition()
{
    int pos = tabPosition(Qt::LeftDockWidgetArea);
    if (KdenliveSettings::tabposition() != pos) {
        setTabPosition(Qt::AllDockWidgetAreas, (QTabWidget::TabPosition) KdenliveSettings::tabposition());
    }
}

void MainWindow::slotRestart()
{
    m_exitCode = EXIT_RESTART;
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

void MainWindow::slotUpdateCaptureFolder()
{
    if (m_recMonitor) {
        if (pCore->projectManager()->current()) {
            m_recMonitor->slotUpdateCaptureFolder(pCore->projectManager()->current()->projectDataFolder() + QDir::separator());
        } else {
            m_recMonitor->slotUpdateCaptureFolder(KdenliveSettings::defaultprojectfolder());
        }
    }
}

void MainWindow::updateConfiguration()
{
    //TODO: we should apply settings to all projects, not only the current one
    if (pCore->projectManager()->currentTimeline()) {
        pCore->projectManager()->currentTimeline()->refresh();
        pCore->projectManager()->currentTimeline()->projectView()->checkAutoScroll();
        pCore->projectManager()->currentTimeline()->checkTrackHeight();
    }
    m_buttonAudioThumbs->setChecked(KdenliveSettings::audiothumbnails());
    m_buttonVideoThumbs->setChecked(KdenliveSettings::videothumbnails());
    m_buttonShowMarkers->setChecked(KdenliveSettings::showmarkers());
    slotSwitchSplitAudio(KdenliveSettings::splitaudio());
    slotSwitchAutomaticTransition();

    // Update list of transcoding profiles
    buildDynamicActions();
    loadClipActions();
}

void MainWindow::slotSwitchSplitAudio(bool enable)
{
    KdenliveSettings::setSplitaudio(enable);
    m_buttonAutomaticSplitAudio->setChecked(KdenliveSettings::splitaudio());
    if (pCore->projectManager()->currentTimeline()) {
        pCore->projectManager()->currentTimeline()->updateHeaders();
    }
}

void MainWindow::slotSwitchVideoThumbs()
{
    KdenliveSettings::setVideothumbnails(!KdenliveSettings::videothumbnails());
    if (pCore->projectManager()->currentTimeline()) {
        pCore->projectManager()->currentTimeline()->projectView()->slotUpdateAllThumbs();
    }
    m_buttonVideoThumbs->setChecked(KdenliveSettings::videothumbnails());
}

void MainWindow::slotSwitchAudioThumbs()
{
    KdenliveSettings::setAudiothumbnails(!KdenliveSettings::audiothumbnails());
    pCore->binController()->checkAudioThumbs();
    if (pCore->projectManager()->currentTimeline()) {
        pCore->projectManager()->currentTimeline()->refresh();
        pCore->projectManager()->currentTimeline()->projectView()->checkAutoScroll();
    }
    m_buttonAudioThumbs->setChecked(KdenliveSettings::audiothumbnails());
}

void MainWindow::slotSwitchMarkersComments()
{
    KdenliveSettings::setShowmarkers(!KdenliveSettings::showmarkers());
    if (pCore->projectManager()->currentTimeline()) {
        pCore->projectManager()->currentTimeline()->refresh();
    }
    m_buttonShowMarkers->setChecked(KdenliveSettings::showmarkers());
}

void MainWindow::slotSwitchSnap()
{
    KdenliveSettings::setSnaptopoints(!KdenliveSettings::snaptopoints());
    m_buttonSnap->setChecked(KdenliveSettings::snaptopoints());
}

void MainWindow::slotSwitchAutomaticTransition()
{
    KdenliveSettings::setAutomatictransitions(!KdenliveSettings::automatictransitions());
    m_buttonAutomaticTransition->setChecked(KdenliveSettings::automatictransitions());
}

void MainWindow::slotDeleteItem()
{
    if (QApplication::focusWidget() &&
            QApplication::focusWidget()->parentWidget() &&
            QApplication::focusWidget()->parentWidget() == pCore->bin()) {
        pCore->bin()->slotDeleteClip();

    } else {
        QWidget *widget = QApplication::focusWidget();
        while (widget && widget != this) {
            if (widget == m_effectStackDock) {
                m_effectStack->deleteCurrentEffect();
                return;
            }
            widget = widget->parentWidget();
        }

        // effect stack has no focus
        if (pCore->projectManager()->currentTimeline()) {
            pCore->projectManager()->currentTimeline()->projectView()->deleteSelectedClips();
        }
    }
}

void MainWindow::slotAddClipMarker()
{
    KdenliveDoc *project = pCore->projectManager()->current();

    ClipController *clip = nullptr;
    GenTime pos;
    if (m_projectMonitor->isActive()) {
        if (pCore->projectManager()->currentTimeline()) {
            ClipItem *item = pCore->projectManager()->currentTimeline()->projectView()->getActiveClipUnderCursor();
            if (item) {
                pos = GenTime((int)((m_projectMonitor->position() - item->startPos() + item->cropStart()).frames(project->fps()) * item->speed() + 0.5), project->fps());
                clip = pCore->binController()->getController(item->getBinId());
            }
        }
    } else {
        clip = m_clipMonitor->currentController();
        pos = m_clipMonitor->position();
    }
    if (!clip) {
        m_messageLabel->setMessage(i18n("Cannot find clip to add marker"), ErrorMessage);
        return;
    }
    QString id = clip->clipId();
    CommentedTime marker(pos, i18n("Marker"), KdenliveSettings::default_marker_type());
    QPointer<MarkerDialog> d = new MarkerDialog(clip, marker,
            project->timecode(), i18n("Add Marker"), this);
    if (d->exec() == QDialog::Accepted) {
        pCore->bin()->slotAddClipMarker(id, QList<CommentedTime>() << d->newMarker());
        QString hash = clip->getClipHash();
        if (!hash.isEmpty()) {
            project->cacheImage(hash + QLatin1Char('#') + QString::number(d->newMarker().time().frames(project->fps())), d->markerImage());
        }
    }
    delete d;
}

void MainWindow::slotDeleteClipMarker(bool allowGuideDeletion)
{
    ClipController *clip = nullptr;
    GenTime pos;
    if (m_projectMonitor->isActive()) {
        if (pCore->projectManager()->currentTimeline()) {
            ClipItem *item = pCore->projectManager()->currentTimeline()->projectView()->getActiveClipUnderCursor();
            if (item) {
                pos = (m_projectMonitor->position() - item->startPos() + item->cropStart()) / item->speed();
                clip = pCore->binController()->getController(item->getBinId());
            }
        }
    } else {
        clip = m_clipMonitor->currentController();
        pos = m_clipMonitor->position();
    }
    if (!clip) {
        m_messageLabel->setMessage(i18n("Cannot find clip to remove marker"), ErrorMessage);
        return;
    }

    QString id = clip->clipId();
    QString comment = clip->markerComment(pos);
    if (comment.isEmpty()) {
        if (allowGuideDeletion && m_projectMonitor->isActive()) {
            slotDeleteGuide();
        } else {
            m_messageLabel->setMessage(i18n("No marker found at cursor time"), ErrorMessage);
        }
        return;
    }
    pCore->bin()->deleteClipMarker(comment, id, pos);
}

void MainWindow::slotDeleteAllClipMarkers()
{
    ClipController *clip = nullptr;
    if (m_projectMonitor->isActive()) {
        if (pCore->projectManager()->currentTimeline()) {
            ClipItem *item = pCore->projectManager()->currentTimeline()->projectView()->getActiveClipUnderCursor();
            if (item) {
                clip = pCore->binController()->getController(item->getBinId());
            }
        }
    } else {
        clip = m_clipMonitor->currentController();
    }
    if (!clip) {
        m_messageLabel->setMessage(i18n("Cannot find clip to remove marker"), ErrorMessage);
        return;
    }
    pCore->bin()->deleteAllClipMarkers(clip->clipId());
}

void MainWindow::slotEditClipMarker()
{
    ClipController *clip = nullptr;
    GenTime pos;
    if (m_projectMonitor->isActive()) {
        if (pCore->projectManager()->currentTimeline()) {
            ClipItem *item = pCore->projectManager()->currentTimeline()->projectView()->getActiveClipUnderCursor();
            if (item) {
                pos = (m_projectMonitor->position() - item->startPos() + item->cropStart()) / item->speed();
                clip = pCore->binController()->getController(item->getBinId());
            }
        }
    } else {
        clip = m_clipMonitor->currentController();
        pos = m_clipMonitor->position();
    }
    if (!clip) {
        m_messageLabel->setMessage(i18n("Cannot find clip to remove marker"), ErrorMessage);
        return;
    }

    QString id = clip->clipId();
    CommentedTime oldMarker = clip->markerAt(pos);
    if (oldMarker == CommentedTime()) {
        m_messageLabel->setMessage(i18n("No marker found at cursor time"), ErrorMessage);
        return;
    }

    QPointer<MarkerDialog> d = new MarkerDialog(clip, oldMarker,
            pCore->projectManager()->current()->timecode(), i18n("Edit Marker"), this);
    if (d->exec() == QDialog::Accepted) {
        pCore->bin()->slotAddClipMarker(id, QList<CommentedTime>() << d->newMarker());
        QString hash = clip->getClipHash();
        if (!hash.isEmpty()) {
            pCore->projectManager()->current()->cacheImage(hash + QLatin1Char('#') + QString::number(d->newMarker().time().frames(pCore->projectManager()->current()->fps())), d->markerImage());
        }
        if (d->newMarker().time() != pos) {
            // remove old marker
            oldMarker.setMarkerType(-1);
            pCore->bin()->slotAddClipMarker(id, QList<CommentedTime>() << oldMarker);
        }
    }
    delete d;
}

void MainWindow::slotAddMarkerGuideQuickly()
{
    if (!pCore->projectManager()->currentTimeline() || !pCore->projectManager()->current()) {
        return;
    }

    if (m_clipMonitor->isActive()) {
        ClipController *clip = m_clipMonitor->currentController();
        GenTime pos = m_clipMonitor->position();

        if (!clip) {
            m_messageLabel->setMessage(i18n("Cannot find clip to add marker"), ErrorMessage);
            return;
        }
        //TODO: allow user to set default marker category
        CommentedTime marker(pos, pCore->projectManager()->current()->timecode().getDisplayTimecode(pos, false), KdenliveSettings::default_marker_type());
        pCore->bin()->slotAddClipMarker(clip->clipId(), QList<CommentedTime>() << marker);
    } else {
        pCore->projectManager()->currentTimeline()->projectView()->slotAddGuide(false);
    }
}

void MainWindow::slotAddGuide()
{
    if (pCore->projectManager()->currentTimeline()) {
        pCore->projectManager()->currentTimeline()->projectView()->slotAddGuide();
    }
}

void MainWindow::slotInsertSpace()
{
    if (pCore->projectManager()->currentTimeline()) {
        pCore->projectManager()->currentTimeline()->projectView()->slotInsertSpace();
    }
}

void MainWindow::slotRemoveSpace()
{
    if (pCore->projectManager()->currentTimeline()) {
        pCore->projectManager()->currentTimeline()->projectView()->slotRemoveSpace();
    }
}

void MainWindow::slotRemoveAllSpace()
{
    if (pCore->projectManager()->currentTimeline()) {
        pCore->projectManager()->currentTimeline()->projectView()->slotRemoveSpace(true);
    }
}

void MainWindow::slotInsertTrack()
{
    pCore->monitorManager()->activateMonitor(Kdenlive::ProjectMonitor);
    if (pCore->projectManager()->currentTimeline()) {
        int ix = pCore->projectManager()->currentTimeline()->projectView()->selectedTrack();
        pCore->projectManager()->currentTimeline()->projectView()->slotInsertTrack(ix);
    }
    if (pCore->projectManager()->current()) {
        m_effectStack->transitionConfig()->updateProjectFormat();
    }
}

void MainWindow::slotDeleteTrack()
{
    pCore->monitorManager()->activateMonitor(Kdenlive::ProjectMonitor);
    if (pCore->projectManager()->currentTimeline()) {
        int ix = pCore->projectManager()->currentTimeline()->projectView()->selectedTrack();
        pCore->projectManager()->currentTimeline()->projectView()->slotDeleteTrack(ix);
    }
    if (pCore->projectManager()->current()) {
        m_effectStack->transitionConfig()->updateProjectFormat();
    }
}

void MainWindow::slotConfigTrack()
{
    pCore->monitorManager()->activateMonitor(Kdenlive::ProjectMonitor);
    if (pCore->projectManager()->currentTimeline()) {
        int ix = pCore->projectManager()->currentTimeline()->projectView()->selectedTrack();
        pCore->projectManager()->currentTimeline()->projectView()->slotConfigTracks(ix);
    }
    if (pCore->projectManager()->current()) {
        m_effectStack->transitionConfig()->updateProjectFormat();
    }
}

void MainWindow::slotSelectTrack()
{
    pCore->monitorManager()->activateMonitor(Kdenlive::ProjectMonitor);
    if (pCore->projectManager()->currentTimeline()) {
        pCore->projectManager()->currentTimeline()->projectView()->slotSelectClipsInTrack();
    }
}

void MainWindow::slotSelectAllTracks()
{
    pCore->monitorManager()->activateMonitor(Kdenlive::ProjectMonitor);
    if (pCore->projectManager()->currentTimeline()) {
        pCore->projectManager()->currentTimeline()->projectView()->slotSelectAllClips();
    }
}

void MainWindow::slotUnselectAllTracks()
{
    pCore->monitorManager()->activateMonitor(Kdenlive::ProjectMonitor);
    if (pCore->projectManager()->currentTimeline()) {
        pCore->projectManager()->currentTimeline()->projectView()->clearSelection();
    }
}

void MainWindow::slotEditGuide(int pos, const QString &text)
{
    if (pCore->projectManager()->currentTimeline()) {
        pCore->projectManager()->currentTimeline()->projectView()->slotEditGuide(pos, text);
    }
}

void MainWindow::slotDeleteGuide()
{
    if (pCore->projectManager()->currentTimeline()) {
        pCore->projectManager()->currentTimeline()->projectView()->slotDeleteGuide();
    }
}

void MainWindow::slotDeleteAllGuides()
{
    if (pCore->projectManager()->currentTimeline()) {
        pCore->projectManager()->currentTimeline()->projectView()->slotDeleteAllGuides();
    }
}

void MainWindow::slotCutTimelineClip()
{
    if (pCore->projectManager()->currentTimeline()) {
        pCore->projectManager()->currentTimeline()->projectView()->cutSelectedClips();
    }
}

void MainWindow::slotInsertClipOverwrite()
{
    if (pCore->projectManager()->currentTimeline()) {
        QPoint binZone = m_clipMonitor->getZoneInfo();
        pCore->projectManager()->currentTimeline()->projectView()->insertZone(TimelineMode::OverwriteEdit, m_clipMonitor->activeClipId(), binZone);
    }
}

void MainWindow::slotInsertClipInsert()
{
    if (pCore->projectManager()->currentTimeline()) {
        QPoint binZone = m_clipMonitor->getZoneInfo();
        int pos = pCore->projectManager()->currentTimeline()->projectView()->insertZone(TimelineMode::InsertEdit, m_clipMonitor->activeClipId(), binZone);
        if (pos > 0) {
            m_projectMonitor->silentSeek(pos);
        }
    }
}

void MainWindow::slotExtractZone()
{
    if (pCore->projectManager()->currentTimeline()) {
        pCore->projectManager()->currentTimeline()->projectView()->extractZone(QPoint(), true);
    }
}

void MainWindow::slotLiftZone()
{
    if (pCore->projectManager()->currentTimeline()) {
        pCore->projectManager()->currentTimeline()->projectView()->extractZone(QPoint(), false);
    }
}

void MainWindow::slotPreviewRender()
{
    if (pCore->projectManager()->current()) {
        pCore->projectManager()->currentTimeline()->startPreviewRender();
    }
}

void MainWindow::slotStopPreviewRender()
{
    if (pCore->projectManager()->current()) {
        pCore->projectManager()->currentTimeline()->stopPreviewRender();
    }
}

void MainWindow::slotDefinePreviewRender()
{
    if (pCore->projectManager()->current()) {
        pCore->projectManager()->currentTimeline()->addPreviewRange(true);
    }
}

void MainWindow::slotRemovePreviewRender()
{
    if (pCore->projectManager()->current()) {
        pCore->projectManager()->currentTimeline()->addPreviewRange(false);
    }
}

void MainWindow::slotClearPreviewRender()
{
    if (pCore->projectManager()->current()) {
        pCore->projectManager()->currentTimeline()->clearPreviewRange();
    }
}

void MainWindow::slotSelectTimelineClip()
{
    if (pCore->projectManager()->currentTimeline()) {
        pCore->projectManager()->currentTimeline()->projectView()->selectClip(true);
    }
}

void MainWindow::slotSelectTimelineTransition()
{
    if (pCore->projectManager()->currentTimeline()) {
        pCore->projectManager()->currentTimeline()->projectView()->selectTransition(true);
    }
}

void MainWindow::slotDeselectTimelineClip()
{
    if (pCore->projectManager()->currentTimeline()) {
        pCore->projectManager()->currentTimeline()->projectView()->selectClip(false, true);
    }
}

void MainWindow::slotDeselectTimelineTransition()
{
    if (pCore->projectManager()->currentTimeline()) {
        pCore->projectManager()->currentTimeline()->projectView()->selectTransition(false, true);
    }
}

void MainWindow::slotSelectAddTimelineClip()
{
    if (pCore->projectManager()->currentTimeline()) {
        pCore->projectManager()->currentTimeline()->projectView()->selectClip(true, true);
    }
}

void MainWindow::slotSelectAddTimelineTransition()
{
    if (pCore->projectManager()->currentTimeline()) {
        pCore->projectManager()->currentTimeline()->projectView()->selectTransition(true, true);
    }
}

void MainWindow::slotGroupClips()
{
    if (pCore->projectManager()->currentTimeline()) {
        pCore->projectManager()->currentTimeline()->projectView()->groupClips();
    }
}

void MainWindow::slotUnGroupClips()
{
    if (pCore->projectManager()->currentTimeline()) {
        pCore->projectManager()->currentTimeline()->projectView()->groupClips(false);
    }
}

void MainWindow::slotEditItemDuration()
{
    if (pCore->projectManager()->currentTimeline()) {
        pCore->projectManager()->currentTimeline()->projectView()->editItemDuration();
    }
}

void MainWindow::slotAddProjectClip(const QUrl &url, const QStringList &folderInfo)
{
    pCore->bin()->droppedUrls(QList<QUrl>() << url, folderInfo);
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
    QStringList info = result->data().toStringList();
    if (info.isEmpty() || info.count() < 2) {
        return;
    }
    QDomElement transition = transitions.getEffectByTag(info.at(0), info.at(1));
    if (pCore->projectManager()->currentTimeline() && !transition.isNull()) {
        pCore->projectManager()->currentTimeline()->projectView()->slotAddTransitionToSelectedClips(transition.cloneNode().toElement());
    }
}

void MainWindow::slotAddVideoEffect(QAction *result)
{
    if (!result) {
        return;
    }
    QStringList info = result->data().toStringList();
    if (info.isEmpty() || info.size() < 3) {
        return;
    }
    QDomElement effect;
    int effectType = info.last().toInt();
    switch (effectType) {
    case EffectsList::EFFECT_VIDEO:
    case EffectsList::EFFECT_GPU:
        effect = videoEffects.getEffectByTag(info.at(0), info.at(1));
        break;
    case EffectsList::EFFECT_AUDIO:
        effect = audioEffects.getEffectByTag(info.at(0), info.at(1));
        break;
    case EffectsList::EFFECT_CUSTOM:
        effect = customEffects.getEffectByTag(info.at(0), info.at(1));
        break;
    default:
        effect = videoEffects.getEffectByTag(info.at(0), info.at(1));
        if (!effect.isNull()) {
            break;
        }
        effect = audioEffects.getEffectByTag(info.at(0), info.at(1));
        if (!effect.isNull()) {
            break;
        }
        effect = customEffects.getEffectByTag(info.at(0), info.at(1));
        break;
    }

    if (!effect.isNull()) {
        slotAddEffect(effect);
    } else {
        m_messageLabel->setMessage(i18n("Cannot find effect %1 / %2", info.at(0), info.at(1)), ErrorMessage);
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
    if (pCore->projectManager()->currentTimeline()) {
        m_zoomSlider->setValue(pCore->projectManager()->currentTimeline()->fitZoom());
        // Make sure to reset scroll bar to start
        pCore->projectManager()->currentTimeline()->projectView()->scrollToStart();
    }
}

void MainWindow::slotSetZoom(int value, bool zoomOnMouse)
{
    value = qBound(m_zoomSlider->minimum(), value, m_zoomSlider->maximum());

    if (pCore->projectManager()->currentTimeline()) {
        pCore->projectManager()->currentTimeline()->slotChangeZoom(value, -1, zoomOnMouse);
    }

    m_zoomOut->setEnabled(value < m_zoomSlider->maximum());
    m_zoomIn->setEnabled(value > m_zoomSlider->minimum());
    slotUpdateZoomSliderToolTip(value);

    m_zoomSlider->blockSignals(true);
    m_zoomSlider->setValue(value);
    m_zoomSlider->blockSignals(false);
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
    m_zoomSlider->setToolTip(i18n("Zoom Level: %1/13", (13 - zoomlevel)));
}

void MainWindow::slotGotProgressInfo(const QString &message, int progress, MessageType type)
{
    m_messageLabel->setProgressMessage(message, progress, type);
}

void MainWindow::customEvent(QEvent *e)
{
    if (e->type() == QEvent::User) {
        m_messageLabel->setMessage(static_cast <MltErrorEvent *>(e)->message(), MltError);
    }
}

void MainWindow::slotTimelineClipSelected(ClipItem *item, bool reloadStack)
{
    m_effectStack->slotClipItemSelected(item, m_projectMonitor, reloadStack);
    m_projectMonitor->slotSetSelectedClip(item);
    if (KdenliveSettings::raisepropsclips()) {
        m_effectStack->raiseWindow(m_effectStackDock);
    }
}

void MainWindow::slotTrackSelected(int index, const TrackInfo &info, bool raise)
{
    m_effectStack->slotTrackItemSelected(index, info, m_projectMonitor);
    if (raise && KdenliveSettings::raisepropstracks()) {
        m_effectStack->raiseWindow(m_effectStackDock);
    }
}

void MainWindow::slotActivateTransitionView(Transition *transition)
{
    if (transition && KdenliveSettings::raisepropstransitions()) {
        m_effectStack->raiseWindow(m_effectStackDock);
    }
}

void MainWindow::slotSnapRewind()
{
    if (m_projectMonitor->isActive()) {
        if (pCore->projectManager()->currentTimeline()) {
            pCore->projectManager()->currentTimeline()->projectView()->slotSeekToPreviousSnap();
        }
    } else  {
        m_clipMonitor->slotSeekToPreviousSnap();
    }
}

void MainWindow::slotSnapForward()
{
    if (m_projectMonitor->isActive()) {
        if (pCore->projectManager()->currentTimeline()) {
            pCore->projectManager()->currentTimeline()->projectView()->slotSeekToNextSnap();
        }
    } else {
        m_clipMonitor->slotSeekToNextSnap();
    }
}

void MainWindow::slotClipStart()
{
    if (m_projectMonitor->isActive()) {
        if (pCore->projectManager()->currentTimeline()) {
            pCore->projectManager()->currentTimeline()->projectView()->clipStart();
        }
    }
}

void MainWindow::slotClipEnd()
{
    if (m_projectMonitor->isActive()) {
        if (pCore->projectManager()->currentTimeline()) {
            pCore->projectManager()->currentTimeline()->projectView()->clipEnd();
        }
    }
}

void MainWindow::slotChangeTool(QAction *action)
{
    if (action == m_buttonSelectTool) {
        slotSetTool(SelectTool);
    } else if (action == m_buttonRazorTool) {
        slotSetTool(RazorTool);
    } else if (action == m_buttonSpacerTool) {
        slotSetTool(SpacerTool);
    }
}

void MainWindow::slotChangeEdit(QAction *action)
{
    if (!pCore->projectManager()->currentTimeline()) {
        return;
    }

    if (action == m_overwriteEditTool) {
        pCore->projectManager()->currentTimeline()->projectView()->setEditMode(TimelineMode::OverwriteEdit);
    } else if (action == m_insertEditTool) {
        pCore->projectManager()->currentTimeline()->projectView()->setEditMode(TimelineMode::InsertEdit);
    } else {
        pCore->projectManager()->currentTimeline()->projectView()->setEditMode(TimelineMode::NormalEdit);
    }
}

void MainWindow::slotSetTool(ProjectTool tool)
{
    if (pCore->projectManager()->current() && pCore->projectManager()->currentTimeline()) {
        //pCore->projectManager()->current()->setTool(tool);
        QString message;
        switch (tool)  {
        case SpacerTool:
            message = i18n("Ctrl + click to use spacer on current track only");
            break;
        case RazorTool:
            message = i18n("Click on a clip to cut it, Shift + move to preview cut frame");
            break;
        default:
            message = i18n("Shift + click to create a selection rectangle, Ctrl + click to add an item to selection");
            break;
        }
        m_messageLabel->setMessage(message, InformationMessage);
        pCore->projectManager()->currentTimeline()->projectView()->setTool(tool);
    }
}

void MainWindow::slotCopy()
{
    if (pCore->projectManager()->currentTimeline()) {
        pCore->projectManager()->currentTimeline()->projectView()->copyClip();
    }
}

void MainWindow::slotPaste()
{
    if (pCore->projectManager()->currentTimeline()) {
        pCore->projectManager()->currentTimeline()->projectView()->pasteClip();
    }
}

void MainWindow::slotPasteEffects()
{
    if (pCore->projectManager()->currentTimeline()) {
        pCore->projectManager()->currentTimeline()->projectView()->pasteClipEffects();
    }
}

void MainWindow::slotClipInTimeline(const QString &clipId)
{
    if (pCore->projectManager()->currentTimeline()) {
        QList<ItemInfo> matching = pCore->projectManager()->currentTimeline()->projectView()->findId(clipId);
        QMenu *inTimelineMenu = static_cast<QMenu *>(factory()->container(QStringLiteral("clip_in_timeline"), this));

        QList<QAction *> actionList;
        for (int i = 0; i < matching.count(); ++i) {
            QString track = pCore->projectManager()->currentTimeline()->getTrackInfo(matching.at(i).track).trackName;
            QString start = pCore->projectManager()->current()->timecode().getTimecode(matching.at(i).startPos);
            int j = 0;
            QAction *a = new QAction(track + QStringLiteral(": ") + start, inTimelineMenu);
            a->setData(QStringList() << track << start);
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
}

void MainWindow::slotClipInProjectTree()
{
    if (pCore->projectManager()->currentTimeline()) {
        int pos = -1;
        QPoint zone;
        const QString selectedId = pCore->projectManager()->currentTimeline()->projectView()->getClipUnderCursor(&pos, &zone);
        if (selectedId.isEmpty()) {
            return;
        }
        m_projectBinDock->raise();
        pCore->bin()->selectClipById(selectedId, pos, zone);
        if (m_projectMonitor->isActive()) {
            slotSwitchMonitors();
        }
    }
}

void MainWindow::slotSelectClipInTimeline()
{
    if (pCore->projectManager()->currentTimeline()) {
        QAction *action = qobject_cast<QAction *>(sender());
        QStringList data = action->data().toStringList();
        pCore->projectManager()->currentTimeline()->projectView()->selectFound(data.at(0), data.at(1));
    }
}

/** Gets called when the window gets hidden */
void MainWindow::hideEvent(QHideEvent */*event*/)
{
    if (isMinimized() && pCore->monitorManager()) {
        pCore->monitorManager()->pauseActiveMonitor();
    }
}

/*void MainWindow::slotSaveZone(Render *render, const QPoint &zone, DocClipBase *baseClip, QUrl path)
{
    QPointer<QDialog> dialog = new QDialog(this);
    dialog->setWindowTitle("Save clip zone");
    QDialogButtonBox *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel);
    QVBoxLayout *mainLayout = new QVBoxLayout;
    dialog->setLayout(mainLayout);

    QPushButton *okButton = buttonBox->button(QDialogButtonBox::Ok);
    okButton->setDefault(true);
    okButton->setShortcut(Qt::CTRL | Qt::Key_Return);
    dialog->connect(buttonBox, SIGNAL(accepted()), dialog, SLOT(accept()));
    dialog->connect(buttonBox, SIGNAL(rejected()), dialog, SLOT(reject()));

    QLabel *label1 = new QLabel(i18n("Save clip zone as:"), this);
    if (path.isEmpty()) {
        QString tmppath = pCore->projectManager()->current()->projectFolder().path() + QDir::separator();
        if (baseClip == nullptr) {
            tmppath.append("untitled.mlt");
        } else {
            tmppath.append((baseClip->name().isEmpty() ? baseClip->fileURL().fileName() : baseClip->name()) + '-' + QString::number(zone.x()).rightJustified(4, '0') + QStringLiteral(".mlt"));
        }
        path = QUrl(tmppath);
    }
    KUrlRequester *url = new KUrlRequester(path, this);
    url->setFilter("video/mlt-playlist");
    QLabel *label2 = new QLabel(i18n("Description:"), this);
    QLineEdit *edit = new QLineEdit(this);
    mainLayout->addWidget(label1);
    mainLayout->addWidget(url);
    mainLayout->addWidget(label2);
    mainLayout->addWidget(edit);
    mainLayout->addWidget(buttonBox);

    if (dialog->exec() == QDialog::Accepted) {
        if (QFile::exists(url->url().path())) {
            if (KMessageBox::questionYesNo(this, i18n("File %1 already exists.\nDo you want to overwrite it?", url->url().path())) == KMessageBox::No) {
                slotSaveZone(render, zone, baseClip, url->url());
                delete dialog;
                return;
            }
        }
        if (baseClip && !baseClip->fileURL().isEmpty()) {
            // create zone from clip url, so that we don't have problems with proxy clips
            QProcess p;
            QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
            env.remove("MLT_PROFILE");
            p.setProcessEnvironment(env);
            p.start(KdenliveSettings::rendererpath(), QStringList() << baseClip->fileURL().toLocalFile() << "in=" + QString::number(zone.x()) << "out=" + QString::number(zone.y()) << "-consumer" << "xml:" + url->url().path());
            if (!p.waitForStarted(3000)) {
                KMessageBox::sorry(this, i18n("Cannot start MLT's renderer:\n%1", KdenliveSettings::rendererpath()));
            }
            else if (!p.waitForFinished(5000)) {
                KMessageBox::sorry(this, i18n("Timeout while creating xml output"));
            }
        }
        else render->saveZone(url->url(), edit->text(), zone);
    }
    delete dialog;
}*/

void MainWindow::slotResizeItemStart()
{
    if (pCore->projectManager()->currentTimeline()) {
        pCore->projectManager()->currentTimeline()->projectView()->setInPoint();
    }
}

void MainWindow::slotResizeItemEnd()
{
    if (pCore->projectManager()->currentTimeline()) {
        pCore->projectManager()->currentTimeline()->projectView()->setOutPoint();
    }
}

int MainWindow::getNewStuff(const QString &configFile)
{
    KNS3::Entry::List entries;
    QPointer<KNS3::DownloadDialog> dialog = new KNS3::DownloadDialog(configFile);
    if (dialog->exec()) {
        entries = dialog->changedEntries();
    }
    foreach (const KNS3::Entry &entry, entries) {
        if (entry.status() == KNS3::Entry::Installed) {
            qCDebug(KDENLIVE_LOG) << "// Installed files: " << entry.installedFiles();
        }
    }
    delete dialog;
    return entries.size();
}

void MainWindow::slotGetNewTitleStuff()
{
    if (getNewStuff(QStringLiteral("kdenlive_titles.knsrc")) > 0) {
        // get project title path
        QString titlePath = pCore->projectManager()->current()->projectDataFolder() + QStringLiteral("/titles/");
        TitleWidget::refreshTitleTemplates(titlePath);
    }
}

void MainWindow::slotGetNewLumaStuff()
{
    if (getNewStuff(QStringLiteral("kdenlive_wipes.knsrc")) > 0) {
        initEffects::refreshLumas();
        pCore->projectManager()->currentTimeline()->projectView()->reloadTransitionLumas();
    }
}

void MainWindow::slotGetNewRenderStuff()
{
    if (getNewStuff(QStringLiteral("kdenlive_renderprofiles.knsrc")) > 0)
        if (m_renderWidget) {
            m_renderWidget->reloadProfiles();
        }
}

void MainWindow::slotGetNewMltProfileStuff()
{
    if (getNewStuff(QStringLiteral("kdenlive_projectprofiles.knsrc")) > 0) {
        // update the list of profiles in settings dialog
        KdenliveSettingsDialog *d = static_cast <KdenliveSettingsDialog *>(KConfigDialog::exists(QStringLiteral("settings")));
        if (d) {
            d->checkProfile();
        }
    }
}

void MainWindow::slotAutoTransition()
{
    if (pCore->projectManager()->currentTimeline()) {
        pCore->projectManager()->currentTimeline()->projectView()->autoTransition();
    }
}

void MainWindow::slotSplitAudio()
{
    if (pCore->projectManager()->currentTimeline()) {
        pCore->projectManager()->currentTimeline()->projectView()->splitAudio();
    }
}

void MainWindow::slotSetAudioAlignReference()
{
    if (pCore->projectManager()->currentTimeline()) {
        pCore->projectManager()->currentTimeline()->projectView()->setAudioAlignReference();
    }
}

void MainWindow::slotAlignAudio()
{
    if (pCore->projectManager()->currentTimeline()) {
        pCore->projectManager()->currentTimeline()->projectView()->alignAudio();
    }
}

void MainWindow::slotUpdateClipType(QAction *action)
{
    if (pCore->projectManager()->currentTimeline()) {
        PlaylistState::ClipState state = (PlaylistState::ClipState) action->data().toInt();
        pCore->projectManager()->currentTimeline()->projectView()->setClipType(state);
    }
}

void MainWindow::slotDvdWizard(const QString &url)
{
    // We must stop the monitors since we create a new on in the dvd wizard
    QPointer<DvdWizard> w = new DvdWizard(pCore->monitorManager(), url, this);
    w->exec();
    delete w;
    pCore->monitorManager()->activateMonitor(Kdenlive::ClipMonitor);
}

void MainWindow::slotShowTimeline(bool show)
{
    if (show == false) {
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

    QList<QAction *>clipJobActions = getExtraActions(QStringLiteral("clipjobs"));
    unplugActionList(QStringLiteral("clip_jobs"));
    plugActionList(QStringLiteral("clip_jobs"), clipJobActions);

    QList<QAction *>atcActions = getExtraActions(QStringLiteral("audiotranscoderslist"));
    unplugActionList(QStringLiteral("audio_transcoders_list"));
    plugActionList(QStringLiteral("audio_transcoders_list"), atcActions);

    QList<QAction *>tcActions = getExtraActions(QStringLiteral("transcoderslist"));
    unplugActionList(QStringLiteral("transcoders_list"));
    plugActionList(QStringLiteral("transcoders_list"), tcActions);
}

void MainWindow::loadDockActions()
{
    QList<QAction *>list = kdenliveCategoryMap.value(QStringLiteral("interface"))->actions();
    // Sort actions
    QMap<QString, QAction *> sorted;
    QStringList sortedList;
    foreach (QAction *a, list) {
        sorted.insert(a->text(), a);
        sortedList << a->text();
    }
    QList<QAction *>orderedList;
    sortedList.sort(Qt::CaseInsensitive);
    foreach (const QString &text, sortedList) {
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

    Mlt::Profile profile;
    Mlt::Filter *filter;

    foreach (const QString &stab, QStringList() << "vidstab" << "videostab2" << "videostab") {
        filter = Mlt::Factory::filter(profile, (char *)stab.toUtf8().constData());
        if (filter && filter->is_valid()) {
            QAction *action = new QAction(i18n("Stabilize") + QStringLiteral(" (") + stab + QLatin1Char(')'), m_extraFactory->actionCollection());
            action->setData(QStringList() << QString::number((int) AbstractClipJob::FILTERCLIPJOB) << stab);
            ts->addAction(action->text(), action);
            connect(action, &QAction::triggered, pCore->bin(), &Bin::slotStartClipJob);
            delete filter;
            break;
        }
        delete filter;
    }
    filter = Mlt::Factory::filter(profile, (char *)"motion_est");
    if (filter) {
        if (filter->is_valid()) {
            QAction *action = new QAction(i18n("Automatic scene split"), m_extraFactory->actionCollection());
            QStringList stabJob;
            stabJob << QString::number((int) AbstractClipJob::FILTERCLIPJOB) << QStringLiteral("motion_est");
            action->setData(stabJob);
            ts->addAction(action->text(), action);
            connect(action, &QAction::triggered, pCore->bin(), &Bin::slotStartClipJob);
        }
        delete filter;
    }
    if (KdenliveSettings::producerslist().contains(QStringLiteral("timewarp"))) {
        QAction *action = new QAction(i18n("Duplicate clip with speed change"), m_extraFactory->actionCollection());
        QStringList stabJob;
        stabJob << QString::number((int) AbstractClipJob::FILTERCLIPJOB) << QStringLiteral("timewarp");
        action->setData(stabJob);
        ts->addAction(action->text(), action);
        connect(action, &QAction::triggered, pCore->bin(), &Bin::slotStartClipJob);
    }
    QAction *action = new QAction(i18n("Analyse keyframes"), m_extraFactory->actionCollection());
    QStringList stabJob(QString::number((int) AbstractClipJob::ANALYSECLIPJOB));
    action->setData(stabJob);
    ts->addAction(action->text(), action);
    connect(action, &QAction::triggered, pCore->bin(), &Bin::slotStartClipJob);
    kdenliveCategoryMap.insert(QStringLiteral("clipjobs"), ts);

    if (kdenliveCategoryMap.contains(QStringLiteral("transcoderslist"))) {
        ts = kdenliveCategoryMap.take(QStringLiteral("transcoderslist"));
        delete ts;
    }
    if (kdenliveCategoryMap.contains(QStringLiteral("audiotranscoderslist"))) {
        ts = kdenliveCategoryMap.take(QStringLiteral("audiotranscoderslist"));
        delete ts;
    }
    ts = new KActionCategory(i18n("Transcoders"), m_extraFactory->actionCollection());
    KActionCategory *ats = new KActionCategory(i18n("Extract Audio"), m_extraFactory->actionCollection());
    KSharedConfigPtr config = KSharedConfig::openConfig(QStandardPaths::locate(QStandardPaths::AppDataLocation, QStringLiteral("kdenlivetranscodingrc")), KConfig::CascadeConfig);
    KConfigGroup transConfig(config, "Transcoding");
    // read the entries
    QMap< QString, QString > profiles = transConfig.entryMap();
    QMapIterator<QString, QString> i(profiles);
    while (i.hasNext()) {
        i.next();
        QStringList data;
        data << QString::number((int) AbstractClipJob::TRANSCODEJOB);
        data << i.value().split(QLatin1Char(';'));
        QAction *a = new QAction(i.key(), m_extraFactory->actionCollection());
        a->setData(data);
        if (data.count() > 1) {
            a->setToolTip(data.at(1));
        }
        // slottranscode
        connect(a, &QAction::triggered, pCore->bin(), &Bin::slotStartClipJob);
        if (data.count() > 3 && data.at(3) == QLatin1String("audio")) {
            // This is an audio transcoding action
            ats->addAction(i.key(), a);
        } else {
            ts->addAction(i.key(), a);
        }
    }
    kdenliveCategoryMap.insert(QStringLiteral("transcoderslist"), ts);
    kdenliveCategoryMap.insert(QStringLiteral("audiotranscoderslist"), ats);

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
    for (int i = 0; i < docks.count(); ++i) {
        QDockWidget *dock = docks.at(i);
        QAction *dockInformations = dock->toggleViewAction();
        if (!dockInformations) {
            continue;
        }
        dockInformations->setChecked(!dock->isHidden());
        guiActions->addAction(dockInformations->text(), dockInformations);
    }
    kdenliveCategoryMap.insert(QStringLiteral("interface"), guiActions);
}

QList<QAction *> MainWindow::getExtraActions(const QString &name)
{
    if (!kdenliveCategoryMap.contains(name)) {
        return QList<QAction *> ();
    }
    return kdenliveCategoryMap.value(name)->actions();
}

void MainWindow::slotTranscode(const QStringList &urls)
{
    QString params;
    QString desc;
    if (urls.isEmpty()) {
        QAction *action = qobject_cast<QAction *>(sender());
        QStringList data = action->data().toStringList();
        pCore->bin()->startClipJob(data);
        return;
    }
    if (urls.isEmpty()) {
        m_messageLabel->setMessage(i18n("No clip to transcode"), ErrorMessage);
        return;
    }
    qCDebug(KDENLIVE_LOG) << "// TRANSODING FOLDER: " << pCore->bin()->getFolderInfo();
    ClipTranscode *d = new ClipTranscode(urls, params, QStringList(), desc, pCore->bin()->getFolderInfo());
    connect(d, &ClipTranscode::addClip, this, &MainWindow::slotAddProjectClip);
    d->show();
}

void MainWindow::slotTranscodeClip()
{
    QString allExtensions = ClipCreationDialog::getExtensions().join(QLatin1Char(' '));
    const QString dialogFilter =  i18n("All Supported Files") + QLatin1Char('(') + allExtensions + QStringLiteral(");;") + i18n("All Files") + QStringLiteral("(*)");
    QString clipFolder = KRecentDirs::dir(QStringLiteral(":KdenliveClipFolder"));
    QStringList urls = QFileDialog::getOpenFileNames(this, i18n("Files to transcode"), clipFolder, dialogFilter);
    if (urls.isEmpty()) {
        return;
    }
    slotTranscode(urls);
}

void MainWindow::slotSetDocumentRenderProfile(const QMap<QString, QString> &props)
{
    KdenliveDoc *project = pCore->projectManager()->current();

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

void MainWindow::slotPrepareRendering(bool scriptExport, bool zoneOnly, const QString &chapterFile, QString scriptPath)
{
    KdenliveDoc *project = pCore->projectManager()->current();

    if (m_renderWidget == nullptr) {
        return;
    }
    QString playlistPath;
    QString mltSuffix(QStringLiteral(".mlt"));
    QList<QString> playlistPaths;
    QList<QString> trackNames;
    int tracksCount = 1;
    bool stemExport = m_renderWidget->isStemAudioExportEnabled();

    if (scriptExport) {
        //QString scriptsFolder = project->projectFolder().path(QUrl::AddTrailingSlash) + "scripts/";
        if (scriptPath.isEmpty()) {
            QString path = m_renderWidget->getFreeScriptName(project->url());
            QPointer<KUrlRequesterDialog> getUrl = new KUrlRequesterDialog(QUrl::fromLocalFile(path), i18n("Create Render Script"), this);
            getUrl->urlRequester()->setMode(KFile::File);
            if (getUrl->exec() == QDialog::Rejected) {
                delete getUrl;
                return;
            }
            scriptPath = getUrl->selectedUrl().toLocalFile();
            delete getUrl;
        }
        QFile f(scriptPath);
        if (f.exists()) {
            if (KMessageBox::warningYesNo(this, i18n("Script file already exists. Do you want to overwrite it?")) != KMessageBox::Yes) {
                return;
            }
        }
        playlistPath = scriptPath;
    } else {
        QTemporaryFile temp(QDir::tempPath() + QStringLiteral("/kdenlive_rendering_XXXXXX.mlt"));
        temp.setAutoRemove(false);
        temp.open();
        playlistPath = temp.fileName();
    }
    int in = 0;
    int out;
    if (zoneOnly) {
        in = pCore->projectManager()->currentTimeline()->inPoint();
        out = pCore->projectManager()->currentTimeline()->outPoint();
    } else {
        out = (int) GenTime(project->projectDuration()).frames(project->fps()) - 2;
    }
    QString playlistContent = pCore->projectManager()->projectSceneList(project->url().adjusted(QUrl::RemoveFilename | QUrl::StripTrailingSlash).toLocalFile());
    if (!chapterFile.isEmpty()) {
        QDomDocument doc;
        QDomElement chapters = doc.createElement(QStringLiteral("chapters"));
        chapters.setAttribute(QStringLiteral("fps"), project->fps());
        doc.appendChild(chapters);

        QMap<double, QString> guidesData = pCore->projectManager()->currentTimeline()->projectView()->guidesData();
        QMapIterator<double, QString> g(guidesData);
        while (g.hasNext()) {
            g.next();
            int time = (int) GenTime(g.key()).frames(project->fps());
            if (time >= in && time < out) {
                if (zoneOnly) {
                    time = time - in;
                }
                QDomElement chapter = doc.createElement(QStringLiteral("chapter"));
                chapters.appendChild(chapter);
                chapter.setAttribute(QStringLiteral("title"), g.value());
                chapter.setAttribute(QStringLiteral("time"), time);
            }
        }

        if (!chapters.childNodes().isEmpty()) {
            if (pCore->projectManager()->currentTimeline()->projectView()->hasGuide(out, true) == -1) {
                // Always insert a guide in pos 0
                QDomElement chapter = doc.createElement(QStringLiteral("chapter"));
                chapters.insertBefore(chapter, QDomNode());
                chapter.setAttribute(QStringLiteral("title"), i18nc("the first in a list of chapters", "Start"));
                chapter.setAttribute(QStringLiteral("time"), QStringLiteral("0"));
            }
            // save chapters file
            QFile file(chapterFile);
            if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
                qCWarning(KDENLIVE_LOG) << "//////  ERROR writing DVD CHAPTER file: " << chapterFile;
            } else {
                file.write(doc.toString().toUtf8());
                if (file.error() != QFile::NoError) {
                    qCWarning(KDENLIVE_LOG) << "//////  ERROR writing DVD CHAPTER file: " << chapterFile;
                }
                file.close();
            }
        }
    }

    // check if audio export is selected
    bool exportAudio;
    if (m_renderWidget->automaticAudioExport()) {
        exportAudio = pCore->projectManager()->currentTimeline()->checkProjectAudio();
    } else {
        exportAudio = m_renderWidget->selectedAudioExport();
    }

    // Set playlist audio volume to 100%
    QDomDocument doc;
    doc.setContent(playlistContent);
    QDomElement tractor = doc.documentElement().firstChildElement(QStringLiteral("tractor"));
    if (!tractor.isNull()) {
        QDomNodeList props = tractor.elementsByTagName(QStringLiteral("property"));
        for (int i = 0; i < props.count(); ++i) {
            if (props.at(i).toElement().attribute(QStringLiteral("name")) == QLatin1String("meta.volume")) {
                props.at(i).firstChild().setNodeValue(QStringLiteral("1"));
                break;
            }
        }
    }

    // Add autoclose to playlists.
    QDomNodeList playlists = doc.elementsByTagName(QStringLiteral("playlist"));
    for (int i = 0; i < playlists.length(); ++i) {
        playlists.item(i).toElement().setAttribute(QStringLiteral("autoclose"), 1);
    }

    // Do we want proxy rendering
    if (project->useProxy() && !m_renderWidget->proxyRendering()) {
        QString root = doc.documentElement().attribute(QStringLiteral("root"));
        if (!root.isEmpty() && !root.endsWith(QLatin1Char('/'))) {
            root.append(QLatin1Char('/'));
        }

        // replace proxy clips with originals
        //TODO
        QMap<QString, QString> proxies = pCore->binController()->getProxies();

        QDomNodeList producers = doc.elementsByTagName(QStringLiteral("producer"));
        QString producerResource;
        QString producerService;
        QString suffix;
        QString prefix;
        for (int n = 0; n < producers.length(); ++n) {
            QDomElement e = producers.item(n).toElement();
            producerResource = EffectsList::property(e, QStringLiteral("resource"));
            producerService = EffectsList::property(e, QStringLiteral("mlt_service"));
            if (producerResource.isEmpty() || producerService == QLatin1String("color")) {
                continue;
            }
            if (producerService == QLatin1String("timewarp")) {
                // slowmotion producer
                prefix = producerResource.section(QLatin1Char(':'), 0, 0) + QLatin1Char(':');
                producerResource = producerResource.section(QLatin1Char(':'), 1);
            } else {
                prefix.clear();
            }
            if (producerService == QLatin1String("framebuffer")) {
                // slowmotion producer
                suffix = QLatin1Char('?') + producerResource.section(QLatin1Char('?'), 1);
                producerResource = producerResource.section(QLatin1Char('?'), 0, 0);
            } else {
                suffix.clear();
            }
            if (!producerResource.isEmpty()) {
                if (QFileInfo(producerResource).isRelative()) {
                    producerResource.prepend(root);
                }
                if (proxies.contains(producerResource)) {
                    QString replacementResource = proxies.value(producerResource);
                    EffectsList::setProperty(e, QStringLiteral("resource"), prefix + replacementResource + suffix);
                    if (producerService == QLatin1String("timewarp")) {
                        EffectsList::setProperty(e, QStringLiteral("warp_resource"), replacementResource);
                    }
                    // We need to delete the "aspect_ratio" property because proxy clips
                    // sometimes have different ratio than original clips
                    EffectsList::removeProperty(e, QStringLiteral("aspect_ratio"));
                    EffectsList::removeMetaProperties(e);
                }
            }
        }
    }

    QList<QDomDocument> docList;

    // check which audio tracks have to be exported
    if (stemExport) {
        Timeline *ct = pCore->projectManager()->currentTimeline();
        int allTracksCount = ct->tracksCount();

        // reset tracks count (tracks to be rendered)
        tracksCount = 0;
        // begin with track 1 (track zero is a hidden black track)
        for (int i = 1; i < allTracksCount; i++) {
            Track *track = ct->track(i);
            // add only tracks to render list that are not muted and have audio
            if (track && !track->info().isMute && track->hasAudio()) {
                QDomDocument docCopy = doc.cloneNode(true).toDocument();
                QString trackName = track->info().trackName;

                // save track name
                trackNames << trackName;
                qCDebug(KDENLIVE_LOG) << "Track-Name: " << trackName;

                // create stem export doc content
                QDomNodeList tracks = docCopy.elementsByTagName(QStringLiteral("track"));
                for (int j = 0; j < allTracksCount; j++) {
                    if (j != i) {
                        // mute other tracks
                        tracks.at(j).toElement().setAttribute(QStringLiteral("hide"), QStringLiteral("both"));
                    }
                }
                docList << docCopy;
                tracksCount++;
            }
        }
    } else {
        docList << doc;
    }

    // create full playlistPaths
    for (int i = 0; i < tracksCount; i++) {
        QString plPath(playlistPath);

        // add track number to path name
        if (stemExport) {
            plPath = plPath + QLatin1Char('_') + QString(trackNames.at(i)).replace(QLatin1Char(' '), QLatin1Char('_'));
        }
        // add mlt suffix
        if (!plPath.endsWith(mltSuffix)) {
            plPath += mltSuffix;
        }
        playlistPaths << plPath;
        qCDebug(KDENLIVE_LOG) << "playlistPath: " << plPath << endl;

        // Do save scenelist
        QFile file(plPath);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            m_messageLabel->setMessage(i18n("Cannot write to file %1", plPath), ErrorMessage);
            return;
        }
        file.write(docList.at(i).toString().toUtf8());
        if (file.error() != QFile::NoError) {
            m_messageLabel->setMessage(i18n("Cannot write to file %1", plPath), ErrorMessage);
            file.close();
            return;
        }
        file.close();
    }
    m_renderWidget->slotExport(scriptExport, in, out, project->metadata(), playlistPaths, trackNames, scriptPath, exportAudio);
}

void MainWindow::slotUpdateTimecodeFormat(int ix)
{
    KdenliveSettings::setFrametimecode(ix == 1);
    m_clipMonitor->updateTimecodeFormat();
    m_projectMonitor->updateTimecodeFormat();
    m_effectStack->transitionConfig()->updateTimecodeFormat();
    m_effectStack->updateTimecodeFormat();
    pCore->bin()->updateTimecodeFormat();
    pCore->projectManager()->currentTimeline()->updateRuler();
    m_timeFormatButton->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
}

void MainWindow::slotRemoveFocus()
{
    pCore->projectManager()->currentTimeline()->setFocus();
}

void MainWindow::slotShutdown()
{
    pCore->projectManager()->current()->setModified(false);
    // Call shutdown
    QDBusConnectionInterface *interface = QDBusConnection::sessionBus().interface();
    if (interface && interface->isServiceRegistered(QStringLiteral("org.kde.ksmserver"))) {
        QDBusInterface smserver(QStringLiteral("org.kde.ksmserver"), QStringLiteral("/KSMServer"), QStringLiteral("org.kde.KSMServerInterface"));
        smserver.call(QStringLiteral("logout"), 1, 2, 2);
    } else if (interface && interface->isServiceRegistered(QStringLiteral("org.gnome.SessionManager"))) {
        QDBusInterface smserver(QStringLiteral("org.gnome.SessionManager"), QStringLiteral("/org/gnome/SessionManager"), QStringLiteral("org.gnome.SessionManager"));
        smserver.call(QStringLiteral("Shutdown"));
    }
}

void MainWindow::slotUpdateTrackInfo()
{
    m_effectStack->transitionConfig()->updateProjectFormat();
}

void MainWindow::slotSwitchMonitors()
{
    pCore->monitorManager()->slotSwitchMonitors(!m_clipMonitor->isActive());
    if (m_projectMonitor->isActive()) {
        pCore->projectManager()->currentTimeline()->projectView()->setFocus();
    } else {
        pCore->bin()->focusBinView();
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
    m_clipMonitor->switchDropFrames(drop);
    m_projectMonitor->switchDropFrames(drop);
}

void MainWindow::slotSetMonitorGamma(int gamma)
{
    KdenliveSettings::setMonitor_gamma(gamma);
    m_clipMonitor->updateMonitorGamma();
    m_projectMonitor->updateMonitorGamma();
}

void MainWindow::slotInsertZoneToTree()
{
    if (!m_clipMonitor->isActive() || m_clipMonitor->currentController() == nullptr) {
        return;
    }
    QPoint info = m_clipMonitor->getZoneInfo();
    pCore->bin()->slotAddClipCut(m_clipMonitor->activeClipId(), info.x(), info.y());
}

void MainWindow::slotInsertZoneToTimeline()
{
    if (pCore->projectManager()->currentTimeline() == nullptr || m_clipMonitor->currentController() == nullptr) {
        return;
    }
    QPoint info = m_clipMonitor->getZoneInfo();
    pCore->projectManager()->currentTimeline()->projectView()->insertClipCut(m_clipMonitor->activeClipId(), info.x(), info.y());
}

void MainWindow::slotMonitorRequestRenderFrame(bool request)
{
    if (request) {
        m_projectMonitor->render->sendFrameForAnalysis = true;
        return;
    } else {
        for (int i = 0; i < m_gfxScopesList.count(); ++i) {
            if (m_gfxScopesList.at(i)->isVisible() && tabifiedDockWidgets(m_gfxScopesList.at(i)).isEmpty() && static_cast<AbstractGfxScopeWidget *>(m_gfxScopesList.at(i)->widget())->autoRefreshEnabled()) {
                request = true;
                break;
            }
        }
    }
#ifdef DEBUG_MAINW
    qCDebug(KDENLIVE_LOG) << "Any scope accepting new frames? " << request;
#endif
    if (!request) {
        m_projectMonitor->render->sendFrameForAnalysis = false;
    }
}

void MainWindow::slotOpenStopmotion()
{
    if (m_stopmotion == nullptr) {
        //m_stopmotion = new StopmotionWidget(pCore->monitorManager(), pCore->projectManager()->current()->projectFolder(), m_stopmotion_actions->actions(), this);
        //TODO
        //connect(m_stopmotion, SIGNAL(addOrUpdateSequence(QString)), m_projectList, SLOT(slotAddOrUpdateSequence(QString)));
        //for (int i = 0; i < m_gfxScopesList.count(); ++i) {
        // Check if we need the renderer to send a new frame for update
        /*if (!m_scopesList.at(i)->widget()->visibleRegion().isEmpty() && !(static_cast<AbstractScopeWidget *>(m_scopesList.at(i)->widget())->autoRefreshEnabled())) request = true;*/
        //connect(m_stopmotion, SIGNAL(gotFrame(QImage)), static_cast<AbstractGfxScopeWidget *>(m_gfxScopesList.at(i)->widget()), SLOT(slotRenderZoneUpdated(QImage)));
        //static_cast<AbstractScopeWidget *>(m_scopesList.at(i)->widget())->slotMonitorCapture();
        //}
    }
    m_stopmotion->show();
}

void MainWindow::slotUpdateProxySettings()
{
    KdenliveDoc *project = pCore->projectManager()->current();
    if (m_renderWidget) {
        m_renderWidget->updateProxyConfig(project->useProxy());
    }
    pCore->bin()->refreshProxySettings();
}

void MainWindow::slotArchiveProject()
{
    QList<ClipController *> list = pCore->binController()->getControllerList();
    KdenliveDoc *doc = pCore->projectManager()->current();
    pCore->binController()->saveDocumentProperties(pCore->projectManager()->currentTimeline()->documentProperties(), doc->metadata(), pCore->projectManager()->currentTimeline()->projectView()->guidesData());
    QDomDocument xmlDoc = doc->xmlSceneList(m_projectMonitor->sceneList(doc->url().adjusted(QUrl::RemoveFilename | QUrl::StripTrailingSlash).toLocalFile()));
    QPointer<ArchiveWidget> d = new ArchiveWidget(doc->url().fileName(), xmlDoc, list, pCore->projectManager()->currentTimeline()->projectView()->extractTransitionsLumas(), this);
    if (d->exec()) {
        m_messageLabel->setMessage(i18n("Archiving project"), OperationCompletedMessage);
    }
    delete d;
}

void MainWindow::slotDownloadResources()
{
    QString currentFolder;
    if (pCore->projectManager()->current()) {
        currentFolder = pCore->projectManager()->current()->projectDataFolder();
    } else {
        currentFolder = KdenliveSettings::defaultprojectfolder();
    }
    ResourceWidget *d = new ResourceWidget(currentFolder);
    connect(d, SIGNAL(addClip(QUrl, QStringList)), this, SLOT(slotAddProjectClip(QUrl, QStringList)));
    d->show();
}

void MainWindow::slotProcessImportKeyframes(GraphicsRectItem type, const QString &tag, const QString &data)
{
    if (type == AVWidget) {
        // This data should be sent to the effect stack
        m_effectStack->setKeyframes(tag, data);
    } else if (type == TransitionWidget) {
        // This data should be sent to the transition stack
        m_effectStack->transitionConfig()->setKeyframes(tag, data);
    } else {
        // Error
    }
}

void MainWindow::slotAlignPlayheadToMousePos()
{
    pCore->monitorManager()->activateMonitor(Kdenlive::ProjectMonitor);
    pCore->projectManager()->currentTimeline()->projectView()->slotAlignPlayheadToMousePos();
}

void MainWindow::triggerKey(QKeyEvent *ev)
{
    // Hack: The QQuickWindow that displays fullscreen monitor does not integrate quith QActions.
    // so on keypress events we parse keys and check for shortcuts in all existing actions
    QKeySequence seq;
    // Remove the Num modifier or some shortcuts like "*" will not work
    if (ev->modifiers() != Qt::KeypadModifier) {
        seq = QKeySequence(ev->key() + ev->modifiers());
    } else {
        seq = QKeySequence(ev->key());
    }
    QList< KActionCollection * > collections = KActionCollection::allCollections();
    for (int i = 0; i < collections.count(); ++i) {
        KActionCollection *coll = collections.at(i);
        foreach (QAction *tempAction, coll->actions()) {
            if (tempAction->shortcuts().contains(seq)) {
                // Trigger action
                tempAction->trigger();
                ev->accept();
                return;
            }
        }
    }
}

QDockWidget *MainWindow::addDock(const QString &title, const QString &objectName, QWidget *widget, Qt::DockWidgetArea area)
{
    QDockWidget *dockWidget = new QDockWidget(title, this);
    dockWidget->setObjectName(objectName);
    dockWidget->setWidget(widget);
    addDockWidget(area, dockWidget);
    connect(dockWidget, &QDockWidget::dockLocationChanged, this, &MainWindow::updateDockTitleBars);
    connect(dockWidget, &QDockWidget::topLevelChanged, this, &MainWindow::updateDockTitleBars);
    return dockWidget;
}

void MainWindow::slotUpdateMonitorOverlays(int id, int code)
{
    QMenu *monitorOverlay = static_cast<QMenu *>(factory()->container(QStringLiteral("monitor_config_overlay"), this));
    if (!monitorOverlay) {
        return;
    }
    QList<QAction *> actions = monitorOverlay->actions();
    foreach (QAction *ac, actions) {
        int data = ac->data().toInt();
        if (data == 0x010) {
            ac->setEnabled(id == Kdenlive::ClipMonitor);
        }
        ac->setChecked(code & data);
    }
}

void MainWindow::slotChangeStyle(QAction *a)
{
    QString style = a->data().toString();
    KdenliveSettings::setWidgetstyle(style);
    doChangeStyle();
}

void MainWindow::doChangeStyle()
{
    QString newStyle = KdenliveSettings::widgetstyle();
    if (newStyle.isEmpty() || newStyle == QStringLiteral("Default")) {
        newStyle = defaultStyle("Breeze");
    }
    QApplication::setStyle(QStyleFactory::create(newStyle));
    // Changing widget style resets color theme, so update color theme again
    ThemeManager::instance()->slotChangePalette();
}

bool MainWindow::isTabbedWith(QDockWidget *widget, const QString &otherWidget)
{
    QList<QDockWidget *> tabbed = tabifiedDockWidgets(widget);
    for (int i = 0; i < tabbed.count(); i++) {
        if (tabbed.at(i)->objectName() == otherWidget) {
            return true;
        }
    }
    return false;
}

void MainWindow::updateDockTitleBars(bool isTopLevel)
{
    if (!KdenliveSettings::showtitlebars() || !isTopLevel) {
        return;
    }
#if (QT_VERSION >= QT_VERSION_CHECK(5, 6, 0))
    QList<QDockWidget *> docks = pCore->window()->findChildren<QDockWidget *>();
    for (int i = 0; i < docks.count(); ++i) {
        QDockWidget *dock = docks.at(i);
        QWidget *bar = dock->titleBarWidget();
        if (dock->isFloating()) {
            if (bar) {
                dock->setTitleBarWidget(nullptr);
                delete bar;
            }
            continue;
        }
        QList<QDockWidget *> docked = pCore->window()->tabifiedDockWidgets(dock);
        if (docked.isEmpty()) {
            if (bar) {
                dock->setTitleBarWidget(nullptr);
                delete bar;
            }
            continue;
        }
        bool hasVisibleDockSibling = false;
        foreach (QDockWidget *sub, docked) {
            if (sub->toggleViewAction()->isChecked()) {
                // we have another docked widget, so tabs are visible and can be used instead of title bars
                hasVisibleDockSibling = true;
                break;
            }
        }
        if (!hasVisibleDockSibling) {
            if (bar) {
                dock->setTitleBarWidget(nullptr);
                delete bar;
            }
            continue;
        }
        if (!bar) {
            dock->setTitleBarWidget(new QWidget);
        }
    }
#endif
}

void MainWindow::slotToggleAutoPreview(bool enable)
{
    KdenliveSettings::setAutopreview(enable);
    if (enable && pCore->projectManager()->currentTimeline()) {
        pCore->projectManager()->currentTimeline()->startPreviewRender();
    }
}

void MainWindow::configureToolbars()
{
    // Since our timeline toolbar is a non-standard toolbar (as it is docked in a custom widget, not
    // in a QToolBarDockArea, we have to hack KXmlGuiWindow to avoid a crash when saving toolbar config.
    // This is why we hijack the configureToolbars() and temporarily move the toolbar to a standard location
    QVBoxLayout *ctnLay = (QVBoxLayout *) m_timelineToolBarContainer->layout();
    ctnLay->removeWidget(m_timelineToolBar);
    addToolBar(Qt::BottomToolBarArea, m_timelineToolBar);
    KEditToolBar *toolBarEditor = new KEditToolBar(guiFactory(), this);
    toolBarEditor->setAttribute(Qt::WA_DeleteOnClose);
    connect(toolBarEditor, SIGNAL(newToolBarConfig()), SLOT(saveNewToolbarConfig()));
    connect(toolBarEditor, &QDialog::finished, this, &MainWindow::rebuildTimlineToolBar);
    toolBarEditor->show();
}

void MainWindow::rebuildTimlineToolBar()
{
    // Timeline toolbar settings changed, we can now re-add our toolbar to custom location
    m_timelineToolBar = toolBar(QStringLiteral("timelineToolBar"));
    removeToolBar(m_timelineToolBar);
    m_timelineToolBar->setToolButtonStyle(Qt::ToolButtonIconOnly);
    QVBoxLayout *ctnLay = (QVBoxLayout *) m_timelineToolBarContainer->layout();
    if (ctnLay) {
        ctnLay->insertWidget(0, m_timelineToolBar);
    }
    m_timelineToolBar->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_timelineToolBar, &QWidget::customContextMenuRequested, this, &MainWindow::showTimelineToolbarMenu);
    m_timelineToolBar->setVisible(true);
}

void MainWindow::showTimelineToolbarMenu(const QPoint &pos)
{
    QMenu menu;
    menu.addAction(actionCollection()->action(KStandardAction::name(KStandardAction::ConfigureToolbars)));
    QMenu *contextSize = new QMenu(i18n("Icon Size"));
    menu.addMenu(contextSize);
    QActionGroup *sizeGroup = new QActionGroup(contextSize);
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

    qSort(avSizes);

    if (avSizes.count() < 10) {
        // Fixed or threshold type icons
        Q_FOREACH (int it, avSizes) {
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
            QAction *a = new QAction(text, contextSize);
            a->setData(it);
            a->setCheckable(true);
            a->setActionGroup(sizeGroup);
            if (it == currentSize) {
                a->setChecked(true);
            }
            contextSize->addAction(a);
        }
    } else {
        // Scalable icons.
        const int progression[] = { 16, 22, 32, 48, 64, 96, 128, 192, 256 };

        for (uint i = 0; i < 9; i++) {
            Q_FOREACH (int it, avSizes) {
                if (it >= progression[ i ]) {
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
                    QAction *a = new QAction(text, contextSize);
                    a->setData(it);
                    a->setCheckable(true);
                    a->setActionGroup(sizeGroup);
                    if (it == currentSize) {
                        a->setChecked(true);
                    }
                    contextSize->addAction(a);
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
    QDialog d(this);
    d.setWindowTitle(i18n("Manage Cache Data"));
    QVBoxLayout *lay = new QVBoxLayout;
    TemporaryData tmp(pCore->projectManager()->current(), false, this);
    connect(&tmp, &TemporaryData::disableProxies, this, &MainWindow::slotDisableProxies);
    connect(&tmp, SIGNAL(disablePreview()), pCore->projectManager()->currentTimeline(), SLOT(invalidateRange()));
    QDialogButtonBox *buttonBox = new QDialogButtonBox(QDialogButtonBox::Close);
    connect(buttonBox, &QDialogButtonBox::rejected, &d, &QDialog::reject);
    lay->addWidget(&tmp);
    lay->addWidget(buttonBox);
    d.setLayout(lay);
    d.exec();
}

void MainWindow::slotUpdateCompositing(QAction *compose)
{
    if (pCore->projectManager()->currentTimeline()) {
        int mode = compose->data().toInt();
        pCore->projectManager()->currentTimeline()->switchComposite(mode);
        if (m_renderWidget) {
            m_renderWidget->errorMessage(RenderWidget::CompositeError, mode == 1 ? i18n("Rendering using low quality track compositing") : QString());
        }
    }
}

void MainWindow::slotUpdateCompositeAction(int mode)
{
    QList<QAction *> actions = m_compositeAction->actions();
    for (int i = 0; i < actions.count(); i++) {
        if (actions.at(i)->data().toInt() == mode) {
            m_compositeAction->setCurrentAction(actions.at(i));
            break;
        }
    }
    if (m_renderWidget) {
        m_renderWidget->errorMessage(RenderWidget::CompositeError, mode == 1 ? i18n("Rendering using low quality track compositing") : QString());
    }
}

void MainWindow::showMenuBar(bool show)
{
    if (!show) {
        KMessageBox::information(this, i18n("This will hide the menu bar completely. You can show it again by typing Ctrl+M."), i18n("Hide menu bar"), QStringLiteral("show-menubar-warning"));
    }
    menuBar()->setVisible(show);
}

void MainWindow::forceIconSet(bool force)
{
    KdenliveSettings::setForce_breeze(force);
    if (KMessageBox::warningContinueCancel(this, i18n("Kdenlive needs to be restarted to apply icon theme change. Restart now ?")) == KMessageBox::Continue) {
        slotRestart();
    }
}

void MainWindow::slotSwitchTrimMode()
{
    if (pCore->projectManager()->currentTimeline()) {
        pCore->projectManager()->currentTimeline()->projectView()->switchTrimMode();
    }
}

void MainWindow::setTrimMode(const QString &mode)
{
    if (pCore->projectManager()->currentTimeline()) {
        m_trimLabel->setText(mode);
        m_trimLabel->setVisible(!mode.isEmpty());
    }
}

#ifdef DEBUG_MAINW
#undef DEBUG_MAINW
#endif

