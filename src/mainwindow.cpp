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
#include "mltcontroller/clipcontroller.h"
#include "kdenlivesettings.h"
#include "dialogs/kdenlivesettingsdialog.h"
#include "dialogs/clipcreationdialog.h"
#include "effectslist/initeffects.h"
#include "dialogs/profilesdialog.h"
#include "project/dialogs/projectsettings.h"
#include "project/clipmanager.h"
#include "monitor/monitor.h"
#include "monitor/recmonitor.h"
#include "monitor/monitormanager.h"
#include "doc/kdenlivedoc.h"
#include "timeline/timeline.h"
#include "timeline/customtrackview.h"
#include "effectslist/effectslistview.h"
#include "effectstack/effectstackview2.h"
#include "project/transitionsettings.h"
#include "mltcontroller/bincontroller.h"
#include "dialogs/renderwidget.h"
#include "renderer.h"
#include "project/clipproperties.h"
#include "dialogs/wizard.h"
#include "project/projectcommands.h"
#include "titler/titlewidget.h"
#include "timeline/markerdialog.h"
#include "timeline/clipitem.h"
#include "interfaces.h"
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
#include <KColorSchemeManager>
#include <KRecentDirs>
#include <KUrlRequesterDialog>
#include <ktogglefullscreenaction.h>
#include <KNotifyConfigWidget>
#include <kns3/downloaddialog.h>
#include <kns3/knewstuffaction.h>
#include <KToolBar>
#include <KColorScheme>
#include <klocalizedstring.h>

#include <QAction>
#include <QDebug>
#include <QStatusBar>
#include <QTemporaryFile>
#include <QMenu>
#include <QDesktopWidget>
#include <QBitmap>
#include <QUndoGroup>
#include <QFileDialog>

#include <stdlib.h>
#include <QStandardPaths>
#include <KConfigGroup>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QVBoxLayout>

static const char version[] = KDENLIVE_VERSION;
namespace Mlt
{
class Producer;
};

EffectsList MainWindow::videoEffects;
EffectsList MainWindow::audioEffects;
EffectsList MainWindow::customEffects;
EffectsList MainWindow::transitions;

QMap <QString,QImage> MainWindow::m_lumacache;

static bool sortByNames(const QPair<QString, QAction *> &a, const QPair<QString, QAction*> &b)
{
    return a.first < b.first;
}

MainWindow::MainWindow(const QString &MltPath, const QUrl &Url, const QString & clipsToLoad, QWidget *parent) :
    KXmlGuiWindow(parent),
    m_timelineArea(NULL),
    m_stopmotion(NULL),
    m_effectStack(NULL),
    m_transitionConfig(NULL),
    m_exitCode(EXIT_SUCCESS),
    m_effectList(NULL),
    m_clipMonitor(NULL),
    m_projectMonitor(NULL),
    m_recMonitor(NULL),
    m_renderWidget(NULL),
    m_mainClip(NULL)
{
    qRegisterMetaType<audioShortVector> ("audioShortVector");
    qRegisterMetaType<MessageType> ("MessageType");
    qRegisterMetaType<stringMap> ("stringMap");
    qRegisterMetaType<audioByteArray> ("audioByteArray");
    qRegisterMetaType< QVector <int> > ();
    qRegisterMetaType<requestClipInfo> ("requestClipInfo");

    new RenderingAdaptor(this);
    Core::initialize(this);
    MltConnection::locateMeltAndProfilesPath(MltPath);

    KActionMenu *themeAction= new KActionMenu(i18n("Theme"), this);
    ThemeManager::instance()->setThemeMenuAction(themeAction);
    ThemeManager::instance()->setCurrentTheme(KdenliveSettings::colortheme());
    connect(ThemeManager::instance(), SIGNAL(signalThemeChanged(const QString &)), this, SLOT(slotThemeChanged(const QString &)));
    ThemeManager::instance()->slotChangePalette();
    /*
    // TODO: change icon theme accordingly to color theme ? is it even possible ?
    QColor background = qApp->palette().background().color();
    bool useDarkIcons = background.value() < 100;
    QString suffix = useDarkIcons ? QString("-dark") : QString();
    QString currentTheme = QIcon::themeName() + suffix;
    //QIcon::setThemeName(currentTheme);
    qDebug()<<"/ / / // SETTING ICON THEME: "<<currentTheme;*/

    KdenliveSettings::setCurrent_profile(KdenliveSettings::default_profile());
    m_commandStack = new QUndoGroup;
    setDockNestingEnabled(true);
    m_timelineArea = new QTabWidget(this);
    //m_timelineArea->setTabReorderingEnabled(true);
    m_timelineArea->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    m_timelineArea->setMinimumHeight(200);
    // Hide tabbar
    QTabBar *bar = m_timelineArea->findChild<QTabBar *>();
    bar->setHidden(true);

    m_gpuAllowed = initEffects::parseEffectFiles(pCore->binController()->mltRepository());
    //initEffects::parseCustomEffectsFile();

    m_shortcutRemoveFocus = new QShortcut(QKeySequence("Esc"), this);
    connect(m_shortcutRemoveFocus, SIGNAL(activated()), this, SLOT(slotRemoveFocus()));

    /// Add Widgets ///
    m_projectBinDock = addDock(i18n("Project Bin"), "project_bin", pCore->bin());

    m_clipMonitor = new Monitor(Kdenlive::ClipMonitor, pCore->monitorManager(), this);
    pCore->bin()->setMonitor(m_clipMonitor);
    connect(m_clipMonitor, &Monitor::showConfigDialog, this, &MainWindow::slotPreferences);
    connect(pCore->bin(), SIGNAL(clipNeedsReload(QString,bool)),this, SLOT(slotUpdateClip(QString,bool)));

    //TODO deprecated, replace with Bin methods if necessary
    /*connect(m_projectList, SIGNAL(loadingIsOver()), this, SLOT(slotElapsedTime()));
    connect(m_projectList, SIGNAL(displayMessage(QString,int,MessageType)), this, SLOT(slotGotProgressInfo(QString,int,MessageType)));
    connect(m_projectList, SIGNAL(updateRenderStatus()), this, SLOT(slotCheckRenderStatus()));
    connect(m_projectList, SIGNAL(updateProfile(QString)), this, SLOT(slotUpdateProjectProfile(QString)));
    connect(m_projectList, SIGNAL(refreshClip(QString,bool)), pCore->monitorManager(), SLOT(slotRefreshCurrentMonitor(QString)));
    connect(m_projectList, SIGNAL(findInTimeline(QString)), this, SLOT(slotClipInTimeline(QString)));
    connect(m_clipMonitor, SIGNAL(zoneUpdated(QPoint)), m_projectList, SLOT(slotUpdateClipCut(QPoint)));*/
    connect(m_clipMonitor, SIGNAL(extractZone(QString)), pCore->bin(), SLOT(slotStartCutJob(QString)));

    m_projectMonitor = new Monitor(Kdenlive::ProjectMonitor, pCore->monitorManager(), this);

/*
    //TODO disabled until ported to qml
    m_recMonitor = new RecMonitor(Kdenlive::RecordMonitor, pCore->monitorManager(), this);
    connect(m_recMonitor, SIGNAL(addProjectClip(QUrl)), this, SLOT(slotAddProjectClip(QUrl)));
    connect(m_recMonitor, SIGNAL(addProjectClipList(QList<QUrl>)), this, SLOT(slotAddProjectClipList(QList<QUrl>)));
    

*/
    pCore->monitorManager()->initMonitors(m_clipMonitor, m_projectMonitor, m_recMonitor);

    m_effectStack = new EffectStackView2();
    connect(m_effectStack, SIGNAL(startFilterJob(const ItemInfo&,const QString&,QMap<QString,QString>&,QMap<QString,QString>&,QMap<QString,QString>&)), pCore->bin(), SLOT(slotStartFilterJob(const ItemInfo &,const QString&,QMap<QString,QString>&,QMap<QString,QString>&,QMap<QString,QString>&)));
    connect(pCore->bin(), SIGNAL(masterClipSelected(ClipController *, Monitor *)), m_effectStack, SLOT(slotMasterClipItemSelected(ClipController *, Monitor *)));
    m_effectStackDock = addDock(i18n("Effect Stack"), "effect_stack", m_effectStack);

    m_transitionConfig = new TransitionSettings(m_projectMonitor);
    m_transitionConfigDock = addDock(i18n("Transition"), "transition", m_transitionConfig);

    m_effectList = new EffectsListView();
    m_effectListDock = addDock(i18n("Effect List"), "effect_list", m_effectList);

    // Add monitors here to keep them at the right of the window
    m_clipMonitorDock = addDock(i18n("Clip Monitor"), "clip_monitor", m_clipMonitor);
    m_projectMonitorDock = addDock(i18n("Project Monitor"), "project_monitor", m_projectMonitor);
    if (m_recMonitor) {
        m_recMonitorDock = addDock(i18n("Record Monitor"), "record_monitor", m_recMonitor);
    }

    m_undoView = new QUndoView();
    m_undoView->setCleanIcon(QIcon::fromTheme("edit-clear"));
    m_undoView->setEmptyLabel(i18n("Clean"));
    m_undoView->setGroup(m_commandStack);
    m_undoViewDock = addDock(i18n("Undo History"), "undo_history", m_undoView);


    setupActions();

    // Color and icon theme stuff
    addAction("themes_menu", themeAction);
    connect(m_commandStack, SIGNAL(cleanChanged(bool)), m_saveAction, SLOT(setDisabled(bool)));

    // Close non-general docks for the initial layout
    // only show important ones
    m_undoViewDock->close();



    /// Tabify Widgets ///
    tabifyDockWidget(m_effectListDock, m_effectStackDock);
    tabifyDockWidget(m_effectListDock, m_transitionConfigDock);

    tabifyDockWidget(m_clipMonitorDock, m_projectMonitorDock);
    if (m_recMonitor) {
        tabifyDockWidget(m_clipMonitorDock, m_recMonitorDock);
    }
    setCentralWidget(m_timelineArea);

    readOptions();

    QAction *action;
    // Stop motion actions. Beware of the order, we MUST use the same order in stopmotion/stopmotion.cpp
    m_stopmotion_actions = new KActionCategory(i18n("Stop Motion"), actionCollection());
    action = new QAction(QIcon::fromTheme("media-record"), i18n("Capture frame"), this);
    //action->setShortcutContext(Qt::WidgetWithChildrenShortcut);
    m_stopmotion_actions->addAction("stopmotion_capture", action);
    action = new QAction(i18n("Switch live / captured frame"), this);
    //action->setShortcutContext(Qt::WidgetWithChildrenShortcut);
    m_stopmotion_actions->addAction("stopmotion_switch", action);
    action = new QAction(QIcon::fromTheme("edit-paste"), i18n("Show last frame over video"), this);
    action->setCheckable(true);
    action->setChecked(false);
    m_stopmotion_actions->addAction("stopmotion_overlay", action);

    // Monitor Record action
    addAction("switch_monitor_rec", m_clipMonitor->recAction());

    // Build effects menu
    m_effectsMenu = new QMenu(i18n("Add Effect"));
    m_effectActions = new KActionCategory(i18n("Effects"), actionCollection());
    m_effectList->reloadEffectList(m_effectsMenu, m_effectActions);
    m_effectsActionCollection->readSettings();

    // Populate View menu with show / hide actions for dock widgets
    KActionCategory *guiActions = new KActionCategory(i18n("Interface"), actionCollection());

    new LayoutManagement(this);
    new HideTitleBars(this);
    new TimelineSearch(this);
    new ScopeManager(this);

    // Add shortcut to action tooltips
    QList< KActionCollection * > collections = KActionCollection::allCollections();
    for (int i = 0; i < collections.count(); ++i) {
        KActionCollection *coll = collections.at(i);
        foreach( QAction* tempAction, coll->actions()) {
            // find the shortcut pattern and delete (note the preceding space in the RegEx)
            QString strippedTooltip = tempAction->toolTip().remove(QRegExp("\\s\\(.*\\)"));
            // append shortcut if it exists for action
            if (tempAction->shortcut() == QKeySequence(0))
                tempAction->setToolTip( strippedTooltip);
            else
                tempAction->setToolTip( strippedTooltip + " (" + tempAction->shortcut().toString() + ")");
        }
    }

    setupGUI();

    emit GUISetupDone();

    /*ScriptingPart* sp = new ScriptingPart(this, QStringList());
    guiFactory()->addClient(sp);*/
    QMenu *trackMenu = static_cast<QMenu*>(factory()->container("track_menu", this));
    if (trackMenu) trackMenu->addActions(m_tracksActionCollection->actions());

    loadPlugins();
    loadTranscoders();
    loadClipActions();

    m_projectMonitor->setupMenu(static_cast<QMenu*>(factory()->container("monitor_go", this)), m_playZone, m_loopZone, NULL, m_loopClip);
    m_clipMonitor->setupMenu(static_cast<QMenu*>(factory()->container("monitor_go", this)), m_playZone, m_loopZone, static_cast<QMenu*>(factory()->container("marker_menu", this)));

    QMenu *clipInTimeline = static_cast<QMenu*>(factory()->container("clip_in_timeline", this));
    clipInTimeline->setIcon(QIcon::fromTheme("go-jump"));
    QHash<QString,QMenu*> menus;
    menus.insert("addMenu",static_cast<QMenu*>(factory()->container("generators", this)));
    menus.insert("extractAudioMenu",static_cast<QMenu*>(factory()->container("extract_audio", this)));
    menus.insert("transcodeMenu",static_cast<QMenu*>(factory()->container("transcoders", this)));
    menus.insert("clipActionsMenu",static_cast<QMenu*>(factory()->container("clip_actions", this)));
    menus.insert("inTimelineMenu",clipInTimeline);
    pCore->bin()->setupGeneratorMenu(menus);

    // Setup and fill effects and transitions menus.
    QMenu *m = static_cast<QMenu*>(factory()->container("video_effects_menu", this));
    m->addActions(m_effectsMenu->actions());


    m_transitionsMenu = new QMenu(i18n("Add Transition"), this);
    for (int i = 0; i < m_transitions.count(); ++i)
        m_transitionsMenu->addAction(m_transitions[i]);

    connect(m, SIGNAL(triggered(QAction*)), this, SLOT(slotAddVideoEffect(QAction*)));
    connect(m_effectsMenu, SIGNAL(triggered(QAction*)), this, SLOT(slotAddVideoEffect(QAction*)));
    connect(m_transitionsMenu, SIGNAL(triggered(QAction*)), this, SLOT(slotAddTransition(QAction*)));

    m_timelineContextMenu = new QMenu(this);
    m_timelineContextClipMenu = new QMenu(this);
    m_timelineContextTransitionMenu = new QMenu(this);

    m_timelineContextMenu->addAction(actionCollection()->action("insert_space"));
    m_timelineContextMenu->addAction(actionCollection()->action("delete_space"));
    m_timelineContextMenu->addAction(actionCollection()->action(KStandardAction::name(KStandardAction::Paste)));

    m_timelineContextClipMenu->addAction(actionCollection()->action("clip_in_project_tree"));
    //m_timelineContextClipMenu->addAction(actionCollection()->action("clip_to_project_tree"));
    m_timelineContextClipMenu->addAction(actionCollection()->action("delete_timeline_clip"));
    m_timelineContextClipMenu->addSeparator();
    m_timelineContextClipMenu->addAction(actionCollection()->action("group_clip"));
    m_timelineContextClipMenu->addAction(actionCollection()->action("ungroup_clip"));
    m_timelineContextClipMenu->addAction(actionCollection()->action("split_audio"));
    m_timelineContextClipMenu->addAction(actionCollection()->action("set_audio_align_ref"));
    m_timelineContextClipMenu->addAction(actionCollection()->action("align_audio"));
    m_timelineContextClipMenu->addSeparator();
    m_timelineContextClipMenu->addAction(actionCollection()->action("cut_timeline_clip"));
    m_timelineContextClipMenu->addAction(actionCollection()->action(KStandardAction::name(KStandardAction::Copy)));
    m_timelineContextClipMenu->addAction(actionCollection()->action("paste_effects"));
    m_timelineContextClipMenu->addSeparator();

    QMenu *markersMenu = static_cast<QMenu*>(factory()->container("marker_menu", this));
    m_timelineContextClipMenu->addMenu(markersMenu);
    m_timelineContextClipMenu->addSeparator();
    m_timelineContextClipMenu->addMenu(m_transitionsMenu);
    m_timelineContextClipMenu->addMenu(m_effectsMenu);

    m_timelineContextTransitionMenu->addAction(actionCollection()->action("delete_timeline_clip"));
    m_timelineContextTransitionMenu->addAction(actionCollection()->action(KStandardAction::name(KStandardAction::Copy)));

    m_timelineContextTransitionMenu->addAction(actionCollection()->action("auto_transition"));

    connect(m_effectList, SIGNAL(addEffect(QDomElement)), this, SLOT(slotAddEffect(QDomElement)));
    connect(m_effectList, SIGNAL(reloadEffects()), this, SLOT(slotReloadEffects()));

    slotConnectMonitors();

    m_projectBinDock->raise();

    /*actionCollection()->addAssociatedWidget(m_clipMonitor->container());
    actionCollection()->addAssociatedWidget(m_projectMonitor->container());*/

    QList<QPair<QString, QAction *> > viewActions;
    QPair <QString, QAction *> pair;
    QAction *showTimeline = new QAction(i18n("Timeline"), this);
    showTimeline->setCheckable(true);
    showTimeline->setChecked(true);
    connect(showTimeline, SIGNAL(triggered(bool)), this, SLOT(slotShowTimeline(bool)));
    
    QMenu *viewMenu = static_cast<QMenu*>(factory()->container("dockwindows", this));
    pair.first = showTimeline->text();
    pair.second = showTimeline;
    viewActions.append(pair);
    
    QList <QDockWidget *> docks = findChildren<QDockWidget *>();
    for (int i = 0; i < docks.count(); ++i) {
        QDockWidget* dock = docks.at(i);
        QAction * a = dock->toggleViewAction();
        if (!a) continue;
        QAction * dockInformations = new QAction(this);
        dockInformations->setText(a->text());
        dockInformations->setCheckable(true);
        dockInformations->setChecked(!dock->isHidden());
        // HACK: since QActions cannot be used in KActionCategory to allow shortcut, we create a duplicate QAction of the dock QAction and link them
        connect(a,SIGNAL(toggled(bool)), dockInformations, SLOT(setChecked(bool)));
        connect(dockInformations,SIGNAL(triggered(bool)), a, SLOT(trigger()));
        pair.first = dockInformations->text();
        pair.second = dockInformations;
        viewActions.append(pair);
    }
    
    // Sort dock view action by name
    qSort(viewActions.begin(), viewActions.end(), sortByNames);
    // Populate view menu
    for (int i = 0; i < viewActions.count(); ++i)
        viewMenu->addAction(guiActions->addAction(viewActions.at(i).first, viewActions.at(i).second));
    
    // Populate encoding profiles
    KConfig conf("encodingprofiles.rc", KConfig::CascadeConfig, QStandardPaths::DataLocation);
    if (KdenliveSettings::proxyparams().isEmpty() || KdenliveSettings::proxyextension().isEmpty()) {
        KConfigGroup group(&conf, "proxy");
        QMap< QString, QString > values = group.entryMap();
        QMapIterator<QString, QString> i(values);
        if (i.hasNext()) {
            i.next();
            QString data = i.value();
            KdenliveSettings::setProxyparams(data.section(';', 0, 0));
            KdenliveSettings::setProxyextension(data.section(';', 1, 1));
        }
    }
    if (KdenliveSettings::v4l_parameters().isEmpty() || KdenliveSettings::v4l_extension().isEmpty()) {
        KConfigGroup group(&conf, "video4linux");
        QMap< QString, QString > values = group.entryMap();
        QMapIterator<QString, QString> i(values);
        if (i.hasNext()) {
            i.next();
            QString data = i.value();
            KdenliveSettings::setV4l_parameters(data.section(';', 0, 0));
            KdenliveSettings::setV4l_extension(data.section(';', 1, 1));
        }
    }
    if (KdenliveSettings::grab_parameters().isEmpty() || KdenliveSettings::grab_extension().isEmpty()) {
        KConfigGroup group(&conf, "screengrab");
        QMap< QString, QString > values = group.entryMap();
        QMapIterator<QString, QString> i(values);
        if (i.hasNext()) {
            i.next();
            QString data = i.value();
            KdenliveSettings::setGrab_parameters(data.section(';', 0, 0));
            KdenliveSettings::setGrab_extension(data.section(';', 1, 1));
        }
    }
    if (KdenliveSettings::decklink_parameters().isEmpty() || KdenliveSettings::decklink_extension().isEmpty()) {
        KConfigGroup group(&conf, "decklink");
        QMap< QString, QString > values = group.entryMap();
        QMapIterator<QString, QString> i(values);
        if (i.hasNext()) {
            i.next();
            QString data = i.value();
            KdenliveSettings::setDecklink_parameters(data.section(';', 0, 0));
            KdenliveSettings::setDecklink_extension(data.section(';', 1, 1));
        }
    }

    pCore->projectManager()->init(Url, clipsToLoad);
    QTimer::singleShot(0, pCore->projectManager(), SLOT(slotLoadOnOpen()));
    connect(this, SIGNAL(reloadTheme()), this, SLOT(slotReloadTheme()), Qt::UniqueConnection);

#ifdef USE_JOGSHUTTLE
    new JogManager(this);
#endif
    //KMessageBox::information(this, "Warning, development version for testing only. we are currently working on core functionnalities,\ndo not save any project or your project files might be corrupted.");
}

void MainWindow::slotThemeChanged(const QString &theme)
{
    disconnect(this, SIGNAL(reloadTheme()), this, SLOT(slotReloadTheme()));
    KSharedConfigPtr config = KSharedConfig::openConfig(theme);
    setPalette(KColorScheme::createApplicationPalette(config));
    qApp->setPalette(palette());
    KdenliveSettings::setColortheme(theme);
    QPalette plt = palette();
    if (m_effectStack) m_effectStack->updatePalette();
    if (m_effectList) m_effectList->updatePalette();
    if (m_transitionConfig) m_transitionConfig->updatePalette();
    if (m_clipMonitor) m_clipMonitor->setPalette(plt);
    if (m_projectMonitor) m_projectMonitor->setPalette(plt);
    setStatusBarStyleSheet(plt);
    if (pCore->projectManager()->currentTimeline()) {
        pCore->projectManager()->currentTimeline()->updatePalette();
    }
    if (m_timelineArea) {
        m_timelineArea->setPalette(plt);
    }
    if (statusBar()) {
        const QObjectList children = statusBar()->children();
        foreach(QObject * child, children) {
            if (child->isWidgetType())
                ((QWidget*)child)->setPalette(plt);
            const QObjectList subchildren = child->children();
            foreach(QObject * subchild, subchildren) {
                if (subchild->isWidgetType())
                    ((QWidget*)subchild)->setPalette(plt);
            }
        }
    }
    connect(this, SIGNAL(reloadTheme()), this, SLOT(slotReloadTheme()), Qt::UniqueConnection);
}

bool MainWindow::event(QEvent *e) {
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
    m_effectStack->slotClipItemSelected(NULL);
    m_transitionConfig->slotTransitionItemSelected(NULL, 0, QPoint(), false);
    if (m_projectMonitor) m_projectMonitor->stop();
    if (m_clipMonitor) m_clipMonitor->stop();
    delete pCore;
    delete m_effectStack;
    delete m_transitionConfig;
    delete m_projectMonitor;
    delete m_clipMonitor;
    delete m_shortcutRemoveFocus;
    qDeleteAll(m_transitions);
    Mlt::Factory::close();
}

//virtual
bool MainWindow::queryClose()
{
    //TODO: use this function?
    if (m_renderWidget) {
        int waitingJobs = m_renderWidget->waitingJobsCount();
        if (waitingJobs > 0) {
            switch (KMessageBox::warningYesNoCancel(this, i18np("You have 1 rendering job waiting in the queue.\nWhat do you want to do with this job?", "You have %1 rendering jobs waiting in the queue.\nWhat do you want to do with these jobs?", waitingJobs), QString(), KGuiItem(i18n("Start them now")), KGuiItem(i18n("Delete them")))) {
            case KMessageBox::Yes :
                // create script with waiting jobs and start it
                if (m_renderWidget->startWaitingRenderJobs() == false) return false;
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

void MainWindow::loadPlugins()
{
    foreach(QObject * plugin, QPluginLoader::staticInstances()) {
        populateMenus(plugin);
    }

    /*QStringList directories = KGlobal::dirs()->findDirs("module", QString()); // port to QStandardPaths ?
    QStringList filters;
    filters << "libkdenlive*";
    foreach(const QString & folder, directories) {
        //qDebug() << "Parsing plugin folder: " << folder;
        QDir pluginsDir(folder);
        foreach(const QString & fileName,
                pluginsDir.entryList(filters, QDir::Files)) {
            //
            // Avoid loading the same plugin twice when there is more than one
            // installation.
            //
            if (!m_pluginFileNames.contains(fileName)) {
                //qDebug() << "Found plugin: " << fileName;
                QPluginLoader loader(pluginsDir.absoluteFilePath(fileName));
                QObject *plugin = loader.instance();
                if (plugin) {
                    populateMenus(plugin);
                    m_pluginFileNames += fileName;
                } else
                    qDebug() << "Error loading plugin: " << fileName << ", " << loader.errorString();
            }
        }
    }*/
}

void MainWindow::populateMenus(QObject *plugin)
{
    QMenu *addMenu = static_cast<QMenu*>(factory()->container("generators", this));
    ClipGenerator *iGenerator = qobject_cast<ClipGenerator *>(plugin);
    if (iGenerator)
        addToMenu(plugin, iGenerator->generators(KdenliveSettings::producerslist()), addMenu, SLOT(generateClip()),
                  NULL);
}

void MainWindow::addToMenu(QObject *plugin, const QStringList &texts,
                           QMenu *menu, const char *member,
                           QActionGroup *actionGroup)
{
    //qDebug() << "// ADD to MENU" << texts;
    foreach(const QString & text, texts) {
        QAction *action = new QAction(text, plugin);
        action->setData(text);
        connect(action, SIGNAL(triggered()), this, member);
        menu->addAction(action);

        if (actionGroup) {
            action->setCheckable(true);
            actionGroup->addAction(action);
        }
    }
}


void MainWindow::generateClip()
{
    QAction *action = qobject_cast<QAction *>(sender());
    ClipGenerator *iGenerator = qobject_cast<ClipGenerator *>(action->parent());

    KdenliveDoc *project = pCore->projectManager()->current();
    QUrl clipUrl = iGenerator->generatedClip(KdenliveSettings::rendererpath(), action->data().toString(), project->projectFolder(),
                                             QStringList(), QStringList(), project->fps(), project->width(), project->height());
    if (clipUrl.isValid()) {
        pCore->bin()->droppedUrls(QList <QUrl> () << clipUrl);
    }
}

void MainWindow::saveProperties(KConfigGroup &config)
{
    // save properties here
    KMainWindow::saveProperties(config);
    
    //TODO: fix session management
    if (qApp->isSavingSession()) {
	if (pCore->projectManager()->current() && !pCore->projectManager()->current()->url().isEmpty()) {
	    config.writeEntry("kdenlive_lastUrl", pCore->projectManager()->current()->url().path());
	}
    }
}

void MainWindow::readProperties(const KConfigGroup &config)
{
    // read properties here
    KMainWindow::readProperties(config);
    //TODO: fix session management
    /*if (qApp->isSessionRestored()) {
	pCore->projectManager()->openFile(QUrl::fromLocalFile(config.readEntry("kdenlive_lastUrl", QString())));
    }*/
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
    KToggleFullScreenAction::setFullScreen(this, actionCollection()->action("fullscreen")->isChecked());
}

void MainWindow::slotAddEffect(const QDomElement &effect)
{
    if (effect.isNull()) {
        //qDebug() << "--- ERROR, TRYING TO APPEND NULL EFFECT";
        return;
    }
    QDomElement effectToAdd = effect.cloneNode().toElement();
    EFFECTMODE status = m_effectStack->effectStatus();
    if (status == TIMELINE_TRACK) {
        pCore->projectManager()->currentTimeline()->projectView()->slotAddTrackEffect(effectToAdd, pCore->projectManager()->currentTimeline()->tracksCount() - m_effectStack->trackIndex());
    }
    else if (status == TIMELINE_CLIP) {
        pCore->projectManager()->currentTimeline()->projectView()->slotAddEffect(effectToAdd, GenTime(), -1);
    }
    else if (status == MASTER_CLIP) {
        pCore->bin()->addEffect(QString(), effectToAdd);
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
    connect(m_projectMonitor->render, SIGNAL(replyGetImage(QString,QImage,bool)), pCore->bin(), SLOT(slotThumbnailReady(QString,QImage,bool)));
    connect(m_projectMonitor->render, SIGNAL(gotFileProperties(requestClipInfo,ClipController *)), pCore->bin(), SLOT(slotProducerReady(requestClipInfo,ClipController *)), Qt::DirectConnection);
    connect(m_projectMonitor->render, SIGNAL(removeInvalidClip(QString,bool)), pCore->bin(), SLOT(slotRemoveInvalidClip(QString,bool)), Qt::DirectConnection);

    //DirectConnection was necessary not to mess the analyze queue, but the monitor thread shouldn't show any UI widget (profile dialog), so adding an AutoConnection in between?

    /*connect(m_projectMonitor->render, SIGNAL(removeInvalidProxy(QString,bool)), pCore->bin(), SLOT(slotRemoveInvalidProxy(QString,bool)));*/
    connect(m_clipMonitor, SIGNAL(refreshClipThumbnail(QString)), pCore->bin(), SLOT(slotRefreshClipThumbnail(QString)));
    connect(m_projectMonitor, SIGNAL(requestFrameForAnalysis(bool)), this, SLOT(slotMonitorRequestRenderFrame(bool)));

    //TODO
    /*connect(m_clipMonitor, SIGNAL(saveZone(Render*,QPoint,DocClipBase*)), this, SLOT(slotSaveZone(Render*,QPoint,DocClipBase*)));
    connect(m_projectMonitor, SIGNAL(saveZone(Render*,QPoint,DocClipBase*)), this, SLOT(slotSaveZone(Render*,QPoint,DocClipBase*)));*/
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
    m_statusProgressBar = new QProgressBar(this);
    m_statusProgressBar->setMinimum(0);
    m_statusProgressBar->setMaximum(100);
    m_statusProgressBar->setMaximumWidth(150);
    m_statusProgressBar->setVisible(false);

    KToolBar *toolbar = new KToolBar("statusToolBar", this, Qt::BottomToolBarArea);
    toolbar->setMovable(false);
    
    setStatusBarStyleSheet(palette());
    QString styleBorderless = "QToolButton { border-width: 0px;margin: 1px 3px 0px;padding: 0px;}";
    
    //create edit mode buttons
    m_normalEditTool = new QAction(QIcon::fromTheme("kdenlive-normal-edit"), i18n("Normal mode"), this);
    m_normalEditTool->setShortcut(i18nc("Normal editing", "n"));
    toolbar->addAction(m_normalEditTool);
    m_normalEditTool->setCheckable(true);
    m_normalEditTool->setChecked(true);

    m_overwriteEditTool = new QAction(QIcon::fromTheme("kdenlive-overwrite-edit"), i18n("Overwrite mode"), this);
    //m_overwriteEditTool->setShortcut(i18nc("Overwrite mode shortcut", "o"));
    toolbar->addAction(m_overwriteEditTool);
    m_overwriteEditTool->setCheckable(true);
    m_overwriteEditTool->setChecked(false);

    m_insertEditTool = new QAction(QIcon::fromTheme("kdenlive-insert-edit"), i18n("Insert mode"), this);
    //m_insertEditTool->setShortcut(i18nc("Insert mode shortcut", "i"));
    toolbar->addAction(m_insertEditTool);
    m_insertEditTool->setCheckable(true);
    m_insertEditTool->setChecked(false);
    // not implemented yet
    m_insertEditTool->setEnabled(false);

    QActionGroup *editGroup = new QActionGroup(this);
    editGroup->addAction(m_normalEditTool);
    editGroup->addAction(m_overwriteEditTool);
    editGroup->addAction(m_insertEditTool);
    editGroup->setExclusive(true);
    connect(editGroup, SIGNAL(triggered(QAction*)), this, SLOT(slotChangeEdit(QAction*)));
    //connect(m_overwriteEditTool, SIGNAL(toggled(bool)), this, SLOT(slotSetOverwriteMode(bool)));

    toolbar->addSeparator();

    // create tools buttons
    m_buttonSelectTool = new QAction(QIcon::fromTheme("cursor-arrow"), i18n("Selection tool"), this);
    m_buttonSelectTool->setShortcut(i18nc("Selection tool shortcut", "s"));
    toolbar->addAction(m_buttonSelectTool);
    m_buttonSelectTool->setCheckable(true);
    m_buttonSelectTool->setChecked(true);

    m_buttonRazorTool = new QAction(QIcon::fromTheme("edit-cut"), i18n("Razor tool"), this);
    m_buttonRazorTool->setShortcut(i18nc("Razor tool shortcut", "x"));
    toolbar->addAction(m_buttonRazorTool);
    m_buttonRazorTool->setCheckable(true);
    m_buttonRazorTool->setChecked(false);

    m_buttonSpacerTool = new QAction(QIcon::fromTheme("distribute-horizontal-x"), i18n("Spacer tool"), this);
    m_buttonSpacerTool->setShortcut(i18nc("Spacer tool shortcut", "m"));
    toolbar->addAction(m_buttonSpacerTool);
    m_buttonSpacerTool->setCheckable(true);
    m_buttonSpacerTool->setChecked(false);
    QActionGroup *toolGroup = new QActionGroup(this);
    toolGroup->addAction(m_buttonSelectTool);
    toolGroup->addAction(m_buttonRazorTool);
    toolGroup->addAction(m_buttonSpacerTool);
    toolGroup->setExclusive(true);
    toolbar->setToolButtonStyle(Qt::ToolButtonIconOnly);

    QWidget * actionWidget;
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
    actionWidget->setMaximumHeight(max - 4);

    connect(toolGroup, SIGNAL(triggered(QAction*)), this, SLOT(slotChangeTool(QAction*)));

    toolbar->addSeparator();
    m_buttonFitZoom = new QAction(QIcon::fromTheme("zoom-fit-best"), i18n("Fit zoom to project"), this);
    toolbar->addAction(m_buttonFitZoom);
    m_buttonFitZoom->setCheckable(false);

    m_zoomOut = new QAction(QIcon::fromTheme("zoom-out"), i18n("Zoom Out"), this);
    toolbar->addAction(m_zoomOut);
    m_zoomOut->setShortcut(Qt::CTRL + Qt::Key_Minus);

    m_zoomSlider = new QSlider(Qt::Horizontal, this);
    m_zoomSlider->setMaximum(13);
    m_zoomSlider->setPageStep(1);
    m_zoomSlider->setInvertedAppearance(true);

    m_zoomSlider->setMaximumWidth(150);
    m_zoomSlider->setMinimumWidth(100);
    toolbar->addWidget(m_zoomSlider);

    m_zoomIn = new QAction(QIcon::fromTheme("zoom-in"), i18n("Zoom In"), this);
    toolbar->addAction(m_zoomIn);
    m_zoomIn->setShortcut(Qt::CTRL + Qt::Key_Plus);

    actionWidget = toolbar->widgetForAction(m_buttonFitZoom);
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
    actionWidget->setStyleSheet(styleBorderless);

    connect(m_zoomSlider, SIGNAL(valueChanged(int)), this, SLOT(slotSetZoom(int)));
    connect(m_zoomSlider, SIGNAL(sliderMoved(int)), this, SLOT(slotShowZoomSliderToolTip(int)));
    connect(m_buttonFitZoom, SIGNAL(triggered()), this, SLOT(slotFitZoom()));
    connect(m_zoomIn, SIGNAL(triggered(bool)), this, SLOT(slotZoomIn()));
    connect(m_zoomOut, SIGNAL(triggered(bool)), this, SLOT(slotZoomOut()));

    toolbar->addSeparator();

    //create automatic audio split button
    m_buttonAutomaticSplitAudio = new QAction(QIcon::fromTheme("kdenlive-split-audio"), i18n("Split audio and video automatically"), this);
    toolbar->addAction(m_buttonAutomaticSplitAudio);
    m_buttonAutomaticSplitAudio->setCheckable(true);
    m_buttonAutomaticSplitAudio->setChecked(KdenliveSettings::splitaudio());
    connect(m_buttonAutomaticSplitAudio, SIGNAL(triggered()), this, SLOT(slotSwitchSplitAudio()));

    m_buttonVideoThumbs = new QAction(QIcon::fromTheme("kdenlive-show-videothumb"), i18n("Show video thumbnails"), this);
    toolbar->addAction(m_buttonVideoThumbs);
    m_buttonVideoThumbs->setCheckable(true);
    m_buttonVideoThumbs->setChecked(KdenliveSettings::videothumbnails());
    connect(m_buttonVideoThumbs, SIGNAL(triggered()), this, SLOT(slotSwitchVideoThumbs()));

    m_buttonAudioThumbs = new QAction(QIcon::fromTheme("kdenlive-show-audiothumb"), i18n("Show audio thumbnails"), this);
    toolbar->addAction(m_buttonAudioThumbs);
    m_buttonAudioThumbs->setCheckable(true);
    m_buttonAudioThumbs->setChecked(KdenliveSettings::audiothumbnails());
    connect(m_buttonAudioThumbs, SIGNAL(triggered()), this, SLOT(slotSwitchAudioThumbs()));

    m_buttonShowMarkers = new QAction(QIcon::fromTheme("kdenlive-show-markers"), i18n("Show markers comments"), this);
    toolbar->addAction(m_buttonShowMarkers);
    m_buttonShowMarkers->setCheckable(true);
    m_buttonShowMarkers->setChecked(KdenliveSettings::showmarkers());
    connect(m_buttonShowMarkers, SIGNAL(triggered()), this, SLOT(slotSwitchMarkersComments()));
    m_buttonSnap = new QAction(QIcon::fromTheme("kdenlive-snap"), i18n("Snap"), this);
    toolbar->addAction(m_buttonSnap);
    m_buttonSnap->setCheckable(true);
    m_buttonSnap->setChecked(KdenliveSettings::snaptopoints());
    connect(m_buttonSnap, SIGNAL(triggered()), this, SLOT(slotSwitchSnap()));

    actionWidget = toolbar->widgetForAction(m_buttonAutomaticSplitAudio);
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
    actionWidget->setMaximumHeight(max - 4);

    m_messageLabel = new StatusBarMessageLabel(this);
    m_messageLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::MinimumExpanding);

    statusBar()->addWidget(m_messageLabel, 10);
    statusBar()->addWidget(m_statusProgressBar, 0);
    statusBar()->addPermanentWidget(toolbar);

    m_timeFormatButton = new KSelectAction("00:00:00:00 / 00:00:00:00", this);
    m_timeFormatButton->addAction(i18n("hh:mm:ss:ff"));
    m_timeFormatButton->addAction(i18n("Frames"));
    if (KdenliveSettings::frametimecode()) m_timeFormatButton->setCurrentItem(1);
    else m_timeFormatButton->setCurrentItem(0);
    connect(m_timeFormatButton, SIGNAL(triggered(int)), this, SLOT(slotUpdateTimecodeFormat(int)));
    m_timeFormatButton->setToolBarMode(KSelectAction::MenuMode);
    toolbar->addAction(m_timeFormatButton);

    const QFontMetrics metric(statusBar()->font());
    int requiredWidth = metric.boundingRect("00:00:00:00 / 00:00:00:00").width() + 20;
    actionWidget = toolbar->widgetForAction(m_timeFormatButton);
    actionWidget->setObjectName("timecode");
    actionWidget->setMinimumWidth(requiredWidth);

    addAction("normal_mode", m_normalEditTool);
    addAction("overwrite_mode", m_overwriteEditTool);
    addAction("insert_mode", m_insertEditTool);
    addAction("select_tool", m_buttonSelectTool);
    addAction("razor_tool", m_buttonRazorTool);
    addAction("spacer_tool", m_buttonSpacerTool);

    addAction("automatic_split_audio", m_buttonAutomaticSplitAudio);
    addAction("show_video_thumbs", m_buttonVideoThumbs);
    addAction("show_audio_thumbs", m_buttonAudioThumbs);
    addAction("show_markers", m_buttonShowMarkers);
    addAction("snap", m_buttonSnap);
    addAction("zoom_fit", m_buttonFitZoom);
    addAction("zoom_in", m_zoomIn);
    addAction("zoom_out", m_zoomOut);

    addAction("manage_profiles", i18n("Manage Project Profiles"), this, SLOT(slotEditProfiles()), QIcon::fromTheme("document-new"));
    KNS3::standardAction(i18n("Download New Wipes..."),            this, SLOT(slotGetNewLumaStuff()),       actionCollection(), "get_new_lumas");
    KNS3::standardAction(i18n("Download New Render Profiles..."),  this, SLOT(slotGetNewRenderStuff()),     actionCollection(), "get_new_profiles");
    KNS3::standardAction(i18n("Download New Project Profiles..."), this, SLOT(slotGetNewMltProfileStuff()), actionCollection(), "get_new_mlt_profiles");
    KNS3::standardAction(i18n("Download New Title Templates..."),  this, SLOT(slotGetNewTitleStuff()),      actionCollection(), "get_new_titles");

    addAction("run_wizard", i18n("Run Config Wizard"), this, SLOT(slotRunWizard()), QIcon::fromTheme("tools-wizard"));
    addAction("project_settings", i18n("Project Settings"), this, SLOT(slotEditProjectSettings()), QIcon::fromTheme("configure"));
    addAction("project_render", i18n("Render"), this, SLOT(slotRenderProject()), QIcon::fromTheme("media-record"), Qt::CTRL + Qt::Key_Return);

    addAction("project_clean", i18n("Clean Project"), this, SLOT(slotCleanProject()), QIcon::fromTheme("edit-clear"));
    //TODO
    //addAction("project_adjust_profile", i18n("Adjust Profile to Current Clip"), pCore->bin(), SLOT(adjustProjectProfileToItem()));

    m_playZone = addAction("monitor_play_zone", i18n("Play Zone"), pCore->monitorManager(), SLOT(slotPlayZone()),
                           QIcon::fromTheme("media-playback-start"), Qt::CTRL + Qt::Key_Space);
    m_loopZone = addAction("monitor_loop_zone", i18n("Loop Zone"), pCore->monitorManager(), SLOT(slotLoopZone()),
                           QIcon::fromTheme("media-playback-start"), Qt::ALT + Qt::Key_Space);
    m_loopClip = addAction("monitor_loop_clip", i18n("Loop selected clip"), m_projectMonitor, SLOT(slotLoopClip()), QIcon::fromTheme("media-playback-start"));
    m_loopClip->setEnabled(false);

    addAction("dvd_wizard", i18n("DVD Wizard"), this, SLOT(slotDvdWizard()), QIcon::fromTheme("media-optical"));
    addAction("transcode_clip", i18n("Transcode Clips"), this, SLOT(slotTranscodeClip()), QIcon::fromTheme("edit-copy"));
    addAction("archive_project", i18n("Archive Project"), this, SLOT(slotArchiveProject()), QIcon::fromTheme("document-save-all"));
    addAction("switch_monitor", i18n("Switch monitor"), this, SLOT(slotSwitchMonitors()), QIcon(), Qt::Key_T);

    QAction *overlayInfo =  new QAction(QIcon::fromTheme("help-hint"), i18n("Monitor Info Overlay"), this);
    addAction("monitor_overlay", overlayInfo);
    overlayInfo->setCheckable(true);
    overlayInfo->setChecked(KdenliveSettings::displayMonitorInfo());
    connect(overlayInfo, SIGNAL(triggered(bool)), this, SLOT(slotSwitchMonitorOverlay(bool)));

    QAction *dropFrames = new QAction(QIcon(), i18n("Real Time (drop frames)"), this);
    dropFrames->setCheckable(true);
    dropFrames->setChecked(KdenliveSettings::monitor_dropframes());
    connect(dropFrames, SIGNAL(toggled(bool)), this, SLOT(slotSwitchDropFrames(bool)));

    KSelectAction *monitorGamma = new KSelectAction(i18n("Monitor Gamma"), this);
    monitorGamma->addAction(i18n("sRGB (computer)"));
    monitorGamma->addAction(i18n("Rec. 709 (TV)"));
    addAction("mlt_gamma", monitorGamma);
    monitorGamma->setCurrentItem(KdenliveSettings::monitor_gamma());
    connect(monitorGamma, SIGNAL(triggered(int)), this, SLOT(slotSetMonitorGamma(int)));

    addAction("insert_project_tree", i18n("Insert Zone in Project Bin"), this, SLOT(slotInsertZoneToTree()), QIcon(), Qt::CTRL + Qt::Key_I);
    addAction("insert_timeline", i18n("Insert Zone in Timeline"), this, SLOT(slotInsertZoneToTimeline()), QIcon(), Qt::SHIFT + Qt::CTRL + Qt::Key_I);

    QAction *resizeStart =  new QAction(QIcon(), i18n("Resize Item Start"), this);
    addAction("resize_timeline_clip_start", resizeStart);
    resizeStart->setShortcut(Qt::Key_1);
    connect(resizeStart, SIGNAL(triggered(bool)), this, SLOT(slotResizeItemStart()));

    QAction *resizeEnd =  new QAction(QIcon(), i18n("Resize Item End"), this);
    addAction("resize_timeline_clip_end", resizeEnd);
    resizeEnd->setShortcut(Qt::Key_2);
    connect(resizeEnd, SIGNAL(triggered(bool)), this, SLOT(slotResizeItemEnd()));

    addAction("monitor_seek_snap_backward", i18n("Go to Previous Snap Point"), this, SLOT(slotSnapRewind()),
              QIcon::fromTheme("media-seek-backward"), Qt::ALT + Qt::Key_Left);
    addAction("seek_clip_start", i18n("Go to Clip Start"), this, SLOT(slotClipStart()), QIcon::fromTheme("media-seek-backward"), Qt::Key_Home);
    addAction("seek_clip_end", i18n("Go to Clip End"), this, SLOT(slotClipEnd()), QIcon::fromTheme("media-seek-forward"), Qt::Key_End);
    addAction("monitor_seek_snap_forward", i18n("Go to Next Snap Point"), this, SLOT(slotSnapForward()),
              QIcon::fromTheme("media-seek-forward"), Qt::ALT + Qt::Key_Right);
    addAction("delete_timeline_clip", i18n("Delete Selected Item"), this, SLOT(slotDeleteItem()), QIcon::fromTheme("edit-delete"), Qt::Key_Delete);
    addAction("align_playhead", i18n("Align Playhead to Mouse Position"), this, SLOT(slotAlignPlayheadToMousePos()), QIcon(), Qt::Key_P);

    QAction *stickTransition = new QAction(i18n("Automatic Transition"), this);
    stickTransition->setData(QString("auto"));
    stickTransition->setCheckable(true);
    stickTransition->setEnabled(false);
    addAction("auto_transition", stickTransition);
    connect(stickTransition, SIGNAL(triggered(bool)), this, SLOT(slotAutoTransition()));

    addAction("group_clip", i18n("Group Clips"), this, SLOT(slotGroupClips()), QIcon::fromTheme("object-group"), Qt::CTRL + Qt::Key_G);

    QAction * ungroupClip = new QAction(QIcon::fromTheme("object-ungroup"), i18n("Ungroup Clips"), this);
    addAction("ungroup_clip", ungroupClip);
    ungroupClip->setShortcut(Qt::CTRL + Qt::SHIFT + Qt::Key_G);
    ungroupClip->setData("ungroup_clip");
    connect(ungroupClip, SIGNAL(triggered(bool)), this, SLOT(slotUnGroupClips()));
    addAction("edit_item_duration", i18n("Edit Duration"), this, SLOT(slotEditItemDuration()), QIcon::fromTheme("measure"));
    addAction("save_timeline_clip", i18n("Save clip"), this, SLOT(slotSaveTimelineClip()), QIcon::fromTheme("document-save"));
    addAction("clip_in_project_tree", i18n("Clip in Project Tree"), this, SLOT(slotClipInProjectTree()), QIcon::fromTheme("go-jump-definition"));
    addAction("overwrite_to_in_point", i18n("Insert Clip Zone in Timeline (Overwrite)"), this, SLOT(slotInsertClipOverwrite()), QIcon(), Qt::Key_V);
    addAction("select_timeline_clip", i18n("Select Clip"), this, SLOT(slotSelectTimelineClip()), QIcon::fromTheme("edit-select"), Qt::Key_Plus);
    addAction("deselect_timeline_clip", i18n("Deselect Clip"), this, SLOT(slotDeselectTimelineClip()), QIcon::fromTheme("edit-select"), Qt::Key_Minus);
    addAction("select_add_timeline_clip", i18n("Add Clip To Selection"), this, SLOT(slotSelectAddTimelineClip()),
              QIcon::fromTheme("edit-select"), Qt::ALT + Qt::Key_Plus);
    addAction("select_timeline_transition", i18n("Select Transition"), this, SLOT(slotSelectTimelineTransition()),
          QIcon::fromTheme("edit-select"), Qt::SHIFT + Qt::Key_Plus);
    addAction("deselect_timeline_transition", i18n("Deselect Transition"), this, SLOT(slotDeselectTimelineTransition()),
              QIcon::fromTheme("edit-select"), Qt::SHIFT + Qt::Key_Minus);
    addAction("select_add_timeline_transition", i18n("Add Transition To Selection"), this, SLOT(slotSelectAddTimelineTransition()),
          QIcon::fromTheme("edit-select"), Qt::ALT + Qt::SHIFT + Qt::Key_Plus);
    addAction("cut_timeline_clip", i18n("Cut Clip"), this, SLOT(slotCutTimelineClip()), QIcon::fromTheme("edit-cut"), Qt::SHIFT + Qt::Key_R);
    addAction("add_clip_marker", i18n("Add Marker"), this, SLOT(slotAddClipMarker()), QIcon::fromTheme("bookmark-new"));
    addAction("delete_clip_marker", i18n("Delete Marker"), this, SLOT(slotDeleteClipMarker()), QIcon::fromTheme("edit-delete"));
    addAction("delete_all_clip_markers", i18n("Delete All Markers"), this, SLOT(slotDeleteAllClipMarkers()), QIcon::fromTheme("edit-delete"));

    QAction * editClipMarker = addAction("edit_clip_marker", i18n("Edit Marker"), this, SLOT(slotEditClipMarker()), QIcon::fromTheme("document-properties"));
    editClipMarker->setData(QString("edit_marker"));

    addAction("add_marker_guide_quickly", i18n("Add Marker/Guide quickly"), this, SLOT(slotAddMarkerGuideQuickly()),
              QIcon::fromTheme("bookmark-new"), Qt::Key_Asterisk);

    QAction * splitAudio = addAction("split_audio", i18n("Split Audio"), this, SLOT(slotSplitAudio()), QIcon::fromTheme("document-new"));
    // "A+V" as data means this action should only be available for clips with audio AND video
    splitAudio->setData("A+V");

    QAction * setAudioAlignReference = addAction("set_audio_align_ref", i18n("Set Audio Reference"), this, SLOT(slotSetAudioAlignReference()));
    // "A" as data means this action should only be available for clips with audio
    setAudioAlignReference->setData("A");

    QAction * alignAudio = addAction("align_audio", i18n("Align Audio to Reference"), this, SLOT(slotAlignAudio()), QIcon());
    // "A" as data means this action should only be available for clips with audio
    alignAudio->setData("A");

    QAction * audioOnly = new QAction(QIcon::fromTheme("document-new"), i18n("Audio Only"), this);
    addAction("clip_audio_only", audioOnly);
    audioOnly->setData("clip_audio_only");
    audioOnly->setCheckable(true);

    QAction * videoOnly = new QAction(QIcon::fromTheme("document-new"), i18n("Video Only"), this);
    addAction("clip_video_only", videoOnly);
    videoOnly->setData("clip_video_only");
    videoOnly->setCheckable(true);

    QAction * audioAndVideo = new QAction(QIcon::fromTheme("document-new"), i18n("Audio and Video"), this);
    addAction("clip_audio_and_video", audioAndVideo);
    audioAndVideo->setData("clip_audio_and_video");
    audioAndVideo->setCheckable(true);

    m_clipTypeGroup = new QActionGroup(this);
    m_clipTypeGroup->addAction(audioOnly);
    m_clipTypeGroup->addAction(videoOnly);
    m_clipTypeGroup->addAction(audioAndVideo);
    connect(m_clipTypeGroup, SIGNAL(triggered(QAction*)), this, SLOT(slotUpdateClipType(QAction*)));
    m_clipTypeGroup->setEnabled(false);

    addAction("insert_space", i18n("Insert Space"), this, SLOT(slotInsertSpace()));
    addAction("delete_space", i18n("Remove Space"), this, SLOT(slotRemoveSpace()));
    m_tracksActionCollection = new KActionCollection(this, "tracks"); //KGlobal::mainComponent());
    //m_effectsActionCollection->setComponentDisplayName("Trasck");//i18n("Tracks").toUtf8());
    m_tracksActionCollection->addAssociatedWidget(m_timelineArea);

    QAction *insertTrack = new QAction(QIcon(), i18n("Insert Track"), m_tracksActionCollection);
    m_tracksActionCollection->addAction("insert_track", insertTrack);
    connect(insertTrack, SIGNAL(triggered()), this, SLOT(slotInsertTrack()));

    QAction *deleteTrack = new QAction(QIcon(), i18n("Delete Track"), m_tracksActionCollection);
    m_tracksActionCollection->addAction("delete_track", deleteTrack);
    connect(deleteTrack, SIGNAL(triggered()), this, SLOT(slotDeleteTrack()));

    QAction *configTracks = new QAction(QIcon::fromTheme("configure"), i18n("Configure Tracks"), m_tracksActionCollection);
    m_tracksActionCollection->addAction("config_tracks", configTracks);
    connect(configTracks, SIGNAL(triggered()), this, SLOT(slotConfigTrack()));

    QAction *selectTrack = new QAction(QIcon(), i18n("Select All in Current Track"), m_tracksActionCollection);
    connect(selectTrack, SIGNAL(triggered()), this, SLOT(slotSelectTrack()));
    m_tracksActionCollection->addAction("select_track", selectTrack);

    QAction *selectAll = KStandardAction::selectAll(this, SLOT(slotSelectAllTracks()), m_tracksActionCollection);
    selectAll->setShortcutContext(Qt::WidgetWithChildrenShortcut);
    m_tracksActionCollection->addAction("select_all_tracks", selectAll);

    addAction("add_guide", i18n("Add Guide"), this, SLOT(slotAddGuide()), QIcon::fromTheme("document-new"));
    addAction("delete_guide", i18n("Delete Guide"), this, SLOT(slotDeleteGuide()), QIcon::fromTheme("edit-delete"));
    addAction("edit_guide", i18n("Edit Guide"), this, SLOT(slotEditGuide()), QIcon::fromTheme("document-properties"));
    addAction("delete_all_guides", i18n("Delete All Guides"), this, SLOT(slotDeleteAllGuides()), QIcon::fromTheme("edit-delete"));

    QAction *pasteEffects = addAction("paste_effects", i18n("Paste Effects"), this, SLOT(slotPasteEffects()), QIcon::fromTheme("edit-paste"));
    pasteEffects->setData("paste_effects");

    m_saveAction = KStandardAction::save(pCore->projectManager(), SLOT(saveFile()), actionCollection());
    KStandardAction::quit(this,                   SLOT(close()),                  actionCollection());
    // TODO: make the following connection to slotEditKeys work
    //KStandardAction::keyBindings(this,            SLOT(slotEditKeys()),           actionCollection());
    KStandardAction::preferences(this,            SLOT(slotPreferences()),        actionCollection());
    KStandardAction::configureNotifications(this, SLOT(configureNotifications()), actionCollection());
    KStandardAction::copy(this,                   SLOT(slotCopy()),               actionCollection());
    KStandardAction::paste(this,                  SLOT(slotPaste()),              actionCollection());
    KStandardAction::fullScreen(this,             SLOT(slotFullScreen()), this,   actionCollection());

    QAction *undo = KStandardAction::undo(m_commandStack, SLOT(undo()), actionCollection());
    undo->setEnabled(false);
    connect(m_commandStack, SIGNAL(canUndoChanged(bool)), undo, SLOT(setEnabled(bool)));

    QAction *redo = KStandardAction::redo(m_commandStack, SLOT(redo()), actionCollection());
    redo->setEnabled(false);
    connect(m_commandStack, SIGNAL(canRedoChanged(bool)), redo, SLOT(setEnabled(bool)));

    QMenu *addClips = new QMenu();

    QAction *addClip = addAction("add_clip", i18n("Add Clip"), pCore->bin(), SLOT(slotAddClip()), QIcon::fromTheme("kdenlive-add-clip"));
    addClips->addAction(addClip);
    QAction *action = addAction("add_color_clip", i18n("Add Color Clip"), pCore->bin(), SLOT(slotCreateProjectClip()), 
                                  QIcon::fromTheme("kdenlive-add-color-clip"));
    action->setData((int) Color);
    addClips->addAction(action);
    action = addAction("add_slide_clip", i18n("Add Slideshow Clip"), pCore->bin(), SLOT(slotCreateProjectClip()),
                                  QIcon::fromTheme("kdenlive-add-slide-clip"));
    action->setData((int) SlideShow);
    addClips->addAction(action);
    action = addAction("add_text_clip", i18n("Add Title Clip"), pCore->bin(), SLOT(slotCreateProjectClip()),
                                  QIcon::fromTheme("kdenlive-add-text-clip"));
    action->setData((int) Text);
    addClips->addAction(action);
    action = addAction("add_text_template_clip", i18n("Add Template Title"), pCore->bin(), SLOT(slotCreateProjectClip()),
                                  QIcon::fromTheme("kdenlive-add-text-clip"));
    action->setData((int) TextTemplate);
    addClips->addAction(action);

    addClips->addAction(addAction("add_folder", i18n("Create Folder"), pCore->bin(), SLOT(slotAddFolder()),
                                  QIcon::fromTheme("folder-new")));
    addClips->addAction(addAction("download_resource", i18n("Online Resources"), this, SLOT(slotDownloadResources()),
                                  QIcon::fromTheme("download")));
    
    QAction *clipProperties = addAction("clip_properties", i18n("Clip Properties"), pCore->bin(), SLOT(slotSwitchClipProperties()), QIcon::fromTheme("document-edit"));
    clipProperties->setData("clip_properties");
    clipProperties->setEnabled(false);

    QAction *openClip = addAction("edit_clip", i18n("Edit Clip"), pCore->bin(), SLOT(slotOpenClip()), QIcon::fromTheme("document-open"));
    openClip->setData("edit_clip");
    openClip->setEnabled(false);

    QAction *deleteClip = addAction("delete_clip", i18n("Delete Clip"), pCore->bin(), SLOT(slotDeleteClip()), QIcon::fromTheme("edit-delete"));
    deleteClip->setData("delete_clip");
    deleteClip->setEnabled(false);

    QAction *reloadClip = addAction("reload_clip", i18n("Reload Clip"), pCore->bin(), SLOT(slotReloadClip()), QIcon::fromTheme("view-refresh"));
    reloadClip->setData("reload_clip");
    reloadClip->setEnabled(false);
    
    QAction *duplicateClip = addAction("duplicate_clip", i18n("Duplicate Clip"), pCore->bin(), SLOT(slotDuplicateClip()), QIcon::fromTheme("edit-copy"));
    duplicateClip->setData("duplicate_clip");
    duplicateClip->setEnabled(false);

    QAction *proxyClip = new QAction(i18n("Proxy Clip"), this);
    addAction("proxy_clip", proxyClip);
    proxyClip->setData(QStringList() << QString::number((int) AbstractClipJob::PROXYJOB));
    proxyClip->setCheckable(true);
    proxyClip->setChecked(false);

    //TODO: port stopmotion to new Monitor code
    //addAction("stopmotion", i18n("Stop Motion Capture"), this, SLOT(slotOpenStopmotion()), QIcon::fromTheme("image-x-generic"));
    addAction("ripple_delete", i18n("Ripple Delete"), this, SLOT(slotRippleDelete()), QIcon(), Qt::CTRL + Qt::Key_X);

    QHash <QString, QAction*> actions;
    actions.insert("reload", reloadClip);
    actions.insert("duplicate", duplicateClip);
    actions.insert("proxy", proxyClip);
    actions.insert("properties", clipProperties);
    actions.insert("open", openClip);
    actions.insert("delete", deleteClip);
    pCore->bin()->setupMenu(addClips, addClip, actions);

    // Setup effects and transitions actions.
    m_effectsActionCollection = new KActionCollection(this, "effects");//KGlobal::mainComponent());
    //m_effectsActionCollection->setComponentDisplayName("Effects");//i18n("Effects and transitions").toUtf8());
    //KActionCategory *transitionActions = new KActionCategory(i18n("Transitions"), m_effectsActionCollection);
    KActionCategory *transitionActions = new KActionCategory(i18n("Transitions"), actionCollection());
    //m_transitions = new QAction*[transitions.count()];
    for (int i = 0; i < transitions.count(); ++i) {
        QStringList effectInfo = transitions.effectIdInfo(i);
        if (effectInfo.isEmpty()) continue;
        QAction *a = new QAction(effectInfo.at(0), this);
        a->setData(effectInfo);
        a->setIconVisibleInMenu(false);
        m_transitions << a;
        QString id = effectInfo.at(2);
        if (id.isEmpty()) id = effectInfo.at(1);
        transitionActions->addAction("transition_" + id, a);
    }
    //m_effectsActionCollection->readSettings();
}

void MainWindow::setStatusBarStyleSheet(const QPalette &p)
{
    KColorScheme scheme(p.currentColorGroup(), KColorScheme::Window, KSharedConfig::openConfig(KdenliveSettings::colortheme()));
    QColor buttonBg = scheme.background(KColorScheme::LinkBackground).color();
    QColor buttonBord = scheme.foreground(KColorScheme::LinkText).color();
    QColor buttonBord2 = scheme.shade(KColorScheme::LightShade);
    statusBar()->setStyleSheet(QString("QStatusBar QLabel {font-size:%1pt;} QStatusBar::item { border: 0px; font-size:%1pt;padding:0px; }").arg(statusBar()->font().pointSize()));
    QString style1 = QString("QToolBar { border: 0px } QToolButton { border-style: inset; border:1px solid transparent;border-radius: 3px;margin: 0px 3px;padding: 0px;} QToolButton#timecode {padding-right:10px;} QToolButton:hover { background: %3;border-style: inset; border:1px solid %3;border-radius: 3px;} QToolButton:checked { background-color: %1; border-style: inset; border:1px solid %2;border-radius: 3px;}").arg(buttonBg.name()).arg(buttonBord.name()).arg(buttonBord2.name());
    statusBar()->setStyleSheet(style1);
}

void MainWindow::saveOptions()
{
    KdenliveSettings::self()->save();
    KSharedConfigPtr config = KSharedConfig::openConfig();
    pCore->projectManager()->recentFilesAction()->saveEntries(KConfigGroup(config, "Recent Files"));
    config->sync();
}

void MainWindow::readOptions()
{
    KSharedConfigPtr config = KSharedConfig::openConfig();
    pCore->projectManager()->recentFilesAction()->loadEntries(KConfigGroup(config, "Recent Files"));

    if (KdenliveSettings::defaultprojectfolder().isEmpty()) {
        QDir dir(QDir::homePath());
        if (!dir.mkdir("kdenlive")) {
            qDebug() << "/// ERROR CREATING PROJECT FOLDER: ";
        } else {
            dir.cd("kdenlive");
            KdenliveSettings::setDefaultprojectfolder(dir.path());
        }
    }

    KConfigGroup initialGroup(config, "version");
    if (!initialGroup.exists() || KdenliveSettings::ffmpegpath().isEmpty() || KdenliveSettings::ffplaypath().isEmpty()) {
        // this is our first run, show Wizard
        QPointer<Wizard> w = new Wizard(false, this);
        if (w->exec() == QDialog::Accepted && w->isOk()) {
            w->adjustSettings();
            delete w;
        } else {
            delete w;
            ::exit(1);
        }
    }
    initialGroup.writeEntry("version", version);
}

void MainWindow::slotRunWizard()
{
    QPointer<Wizard> w = new Wizard(false, this);
    if (w->exec() == QDialog::Accepted && w->isOk()) {
        w->adjustSettings();
    }
    delete w;
}

void MainWindow::slotEditProfiles()
{
    ProfilesDialog *w = new ProfilesDialog;
    if (w->exec() == QDialog::Accepted) {
        KdenliveSettingsDialog* d = static_cast <KdenliveSettingsDialog*>(KConfigDialog::exists("settings"));
        if (d) {
            d->checkProfile();
        }
    }
    delete w;
}

void MainWindow::slotEditProjectSettings()
{
    KdenliveDoc *project = pCore->projectManager()->current();
    QPoint p = pCore->projectManager()->currentTimeline()->getTracksCount();
    
    QPointer<ProjectSettings> w = new ProjectSettings(project, project->metadata(), pCore->projectManager()->currentTimeline()->projectView()->extractTransitionsLumas(), p.x(), p.y(), project->projectFolder().path(), true, !project->isModified(), this);
    connect(w, SIGNAL(disableProxies()), this, SLOT(slotDisableProxies()));

    if (w->exec() == QDialog::Accepted) {
        QString profile = w->selectedProfile();
        project->setProjectFolder(w->selectedFolder());
        if (m_recMonitor) {
            m_recMonitor->slotUpdateCaptureFolder(project->projectFolder().path() + QDir::separator());
        }
        if (m_renderWidget) {
            m_renderWidget->setDocumentPath(project->projectFolder().path() + QDir::separator());
        }
        if (KdenliveSettings::videothumbnails() != w->enableVideoThumbs()) {
            slotSwitchVideoThumbs();
        }
        if (KdenliveSettings::audiothumbnails() != w->enableAudioThumbs()) {
            slotSwitchAudioThumbs();
        }
        if (project->profilePath() != profile) {
            KdenliveSettings::setCurrent_profile(profile);
            pCore->projectManager()->slotResetProfiles();
            slotUpdateDocumentState(true);
        }
        if (project->getDocumentProperty("proxyparams") != w->proxyParams()) {
            project->setModified();
            project->setDocumentProperty("proxyparams", w->proxyParams());
            if (pCore->binController()->clipCount() > 0 && KMessageBox::questionYesNo(this, i18n("You have changed the proxy parameters. Do you want to recreate all proxy clips for this project?")) == KMessageBox::Yes) {
                //TODO: rebuild all proxies
                //m_projectList->rebuildProxies();
            }
        }
        if (project->getDocumentProperty("proxyextension") != w->proxyExtension()) {
            project->setModified();
            project->setDocumentProperty("proxyextension", w->proxyExtension());
        }
        if (project->getDocumentProperty("generateproxy") != QString::number((int) w->generateProxy())) {
            project->setModified();
            project->setDocumentProperty("generateproxy", QString::number((int) w->generateProxy()));
        }
        if (project->getDocumentProperty("proxyminsize") != QString::number(w->proxyMinSize())) {
            project->setModified();
            project->setDocumentProperty("proxyminsize", QString::number(w->proxyMinSize()));
        }
        if (project->getDocumentProperty("generateimageproxy") != QString::number((int) w->generateImageProxy())) {
            project->setModified();
            project->setDocumentProperty("generateimageproxy", QString::number((int) w->generateImageProxy()));
        }
        if (project->getDocumentProperty("proxyimageminsize") != QString::number(w->proxyImageMinSize())) {
            project->setModified();
            project->setDocumentProperty("proxyimageminsize", QString::number(w->proxyImageMinSize()));
        }
        if (QString::number((int) w->useProxy()) != project->getDocumentProperty("enableproxy")) {
            project->setDocumentProperty("enableproxy", QString::number((int) w->useProxy()));
            project->setModified();
            slotUpdateProxySettings();
        }
        if (w->metadata() != project->metadata()) {
            project->setMetadata(w->metadata());
        }
    }
    delete w;
}

void MainWindow::slotDisableProxies()
{
    pCore->projectManager()->current()->setDocumentProperty("enableproxy", QString::number((int) false));
    pCore->projectManager()->current()->setModified();
    slotUpdateProxySettings();
}

void MainWindow::slotRenderProject()
{
    KdenliveDoc *project = pCore->projectManager()->current();

    if (!m_renderWidget) {
        QString projectfolder = project ? project->projectFolder().path() + QDir::separator() : KdenliveSettings::defaultprojectfolder();
        MltVideoProfile profile;
        if (project) {
            profile = project->mltProfile();
        }
        m_renderWidget = new RenderWidget(projectfolder, project->useProxy(), profile, this);
        connect(m_renderWidget, SIGNAL(shutdown()), this, SLOT(slotShutdown()));
        connect(m_renderWidget, SIGNAL(selectedRenderProfile(QMap<QString,QString>)), this, SLOT(slotSetDocumentRenderProfile(QMap<QString,QString>)));
        connect(m_renderWidget, SIGNAL(prepareRenderingData(bool,bool,QString)), this, SLOT(slotPrepareRendering(bool,bool,QString)));
        connect(m_renderWidget, SIGNAL(abortProcess(QString)), this, SIGNAL(abortRenderJob(QString)));
        connect(m_renderWidget, SIGNAL(openDvdWizard(QString)), this, SLOT(slotDvdWizard(QString)));
        if (project) {
            m_renderWidget->setProfile(project->mltProfile());
            m_renderWidget->setGuides(pCore->projectManager()->currentTimeline()->projectView()->guidesData(), project->projectDuration());
            m_renderWidget->setDocumentPath(project->projectFolder().path() + QDir::separator());
            m_renderWidget->setRenderProfile(project->getRenderProperties());
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
    if (m_renderWidget)
        m_renderWidget->setRenderJob(url, progress);
}

void MainWindow::setRenderingFinished(const QString &url, int status, const QString &error)
{
    if (m_renderWidget)
        m_renderWidget->setRenderStatus(url, status, error);
}

void MainWindow::slotCleanProject()
{
    if (KMessageBox::warningContinueCancel(this, i18n("This will remove all unused clips from your project."), i18n("Clean up project")) == KMessageBox::Cancel) return;
    //TODO
    //m_projectList->cleanup();
}

void MainWindow::slotUpdateMousePosition(int pos)
{
    if (pCore->projectManager()->current()) {
        switch (m_timeFormatButton->currentItem()) {
        case 0:
            m_timeFormatButton->setText(pCore->projectManager()->current()->timecode().getTimecodeFromFrames(pos) + " / " + pCore->projectManager()->current()->timecode().getTimecodeFromFrames(pCore->projectManager()->currentTimeline()->duration()));
            break;
        default:
            m_timeFormatButton->setText(QString::number(pos) + " / " + QString::number(pCore->projectManager()->currentTimeline()->duration()));
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
    connect(project, SIGNAL(startAutoSave()), pCore->projectManager(), SLOT(slotStartAutoSave()));
    connect(project, SIGNAL(reloadEffects()), this, SLOT(slotReloadEffects()));
    KdenliveSettings::setProject_fps(project->fps());
    m_clipMonitorDock->raise();
    m_transitionConfig->updateProjectFormat();
    m_effectStack->updateProjectFormat(project->timecode());
    //connect(m_projectList, SIGNAL(refreshClip(QString,bool)), trackView->projectView(), SLOT(slotRefreshThumbs(QString,bool)));
    //connect(m_projectList, SIGNAL(projectModified()), project, SLOT(setModified()));
    //connect(m_projectList, SIGNAL(clipNameChanged(QString,QString)), trackView->projectView(), SLOT(clipNameChanged(QString,QString)));

    connect(trackView, SIGNAL(configTrack(int)), this, SLOT(slotConfigTrack(int)));
    connect(trackView, SIGNAL(updateTracksInfo()), this, SLOT(slotUpdateTrackInfo()));
    connect(trackView, SIGNAL(mousePosition(int)), this, SLOT(slotUpdateMousePosition(int)));
    connect(m_projectMonitor->render, SIGNAL(infoProcessingFinished()), trackView->projectView(), SLOT(slotInfoProcessingFinished()), Qt::DirectConnection);

    connect(trackView->projectView(), SIGNAL(importKeyframes(GraphicsRectItem,QString,int)), this, SLOT(slotProcessImportKeyframes(GraphicsRectItem,QString,int)));

    connect(m_projectMonitor, SIGNAL(renderPosition(int)), trackView, SLOT(moveCursorPos(int)));
    connect(m_projectMonitor, SIGNAL(zoneUpdated(QPoint)), trackView, SLOT(slotSetZone(QPoint)));
    connect(m_projectMonitor, SIGNAL(zoneUpdated(QPoint)), project, SLOT(setModified()));
    connect(m_clipMonitor, SIGNAL(zoneUpdated(QPoint)), project, SLOT(setModified()));
    connect(project, SIGNAL(docModified(bool)), this, SLOT(slotUpdateDocumentState(bool)));
    connect(trackView->projectView(), SIGNAL(guidesUpdated()), this, SLOT(slotGuidesUpdated()));
    connect(project, SIGNAL(saveTimelinePreview(QString)), trackView, SLOT(slotSaveTimelinePreview(QString)));

    connect(trackView, SIGNAL(showTrackEffects(int,TrackInfo)), this, SLOT(slotTrackSelected(int,TrackInfo)));

    connect(trackView->projectView(), SIGNAL(clipItemSelected(ClipItem*,bool)), this, SLOT(slotTimelineClipSelected(ClipItem*,bool)));
    connect(trackView->projectView(), SIGNAL(transitionItemSelected(Transition*,int,QPoint,bool)), m_transitionConfig, SLOT(slotTransitionItemSelected(Transition*,int,QPoint,bool)));
    connect(trackView->projectView(), SIGNAL(transitionItemSelected(Transition*,int,QPoint,bool)), this, SLOT(slotActivateTransitionView(Transition*)));
    connect(trackView->projectView(), SIGNAL(zoomIn()), this, SLOT(slotZoomIn()));
    connect(trackView->projectView(), SIGNAL(zoomOut()), this, SLOT(slotZoomOut()));
    connect(trackView, SIGNAL(setZoom(int)), this, SLOT(slotSetZoom(int)));
    connect(trackView->projectView(), SIGNAL(displayMessage(QString,MessageType)), m_messageLabel, SLOT(setMessage(QString,MessageType)));
    connect(pCore->bin(), SIGNAL(clipNameChanged(QString)), trackView->projectView(), SLOT(clipNameChanged(QString)));    
    connect(pCore->bin(), SIGNAL(displayMessage(QString,MessageType)), m_messageLabel, SLOT(setMessage(QString,MessageType)));

    //connect(trackView->projectView(), SIGNAL(showClipFrame(DocClipBase*,QPoint,bool,int)), m_clipMonitor, SLOT(slotSetClipProducer(DocClipBase*,QPoint,bool,int)));
    connect(trackView->projectView(), SIGNAL(playMonitor()), m_projectMonitor, SLOT(slotPlay()));

    connect(trackView->projectView(), SIGNAL(transitionItemSelected(Transition*,int,QPoint,bool)), m_projectMonitor, SLOT(slotSetSelectedClip(Transition*)));

    connect(pCore->bin(), SIGNAL(gotFilterJobResults(QString,int,int,stringMap,stringMap)), trackView->projectView(), SLOT(slotGotFilterJobResults(QString,int,int,stringMap,stringMap)));

    //TODO
    //connect(m_projectList, SIGNAL(addMarkers(QString,QList<CommentedTime>)), trackView->projectView(), SLOT(slotAddClipMarker(QString,QList<CommentedTime>)));

    // Effect stack signals
    connect(m_effectStack, SIGNAL(updateEffect(ClipItem*,int,QDomElement,QDomElement,int,bool)), trackView->projectView(), SLOT(slotUpdateClipEffect(ClipItem*,int,QDomElement,QDomElement,int,bool)));
    connect(m_effectStack, SIGNAL(updateClipRegion(ClipItem*,int,QString)), trackView->projectView(), SLOT(slotUpdateClipRegion(ClipItem*,int,QString)));
    connect(m_effectStack, SIGNAL(removeEffect(ClipItem*,int,QDomElement)), trackView->projectView(), SLOT(slotDeleteEffect(ClipItem*,int,QDomElement)));
    connect(m_effectStack, SIGNAL(removeMasterEffect(QString,QDomElement)), pCore->bin(), SLOT(slotDeleteEffect(QString,QDomElement)));
    connect(m_effectStack, SIGNAL(addEffect(ClipItem*,QDomElement)), trackView->projectView(), SLOT(slotAddEffect(ClipItem*,QDomElement)));
    connect(m_effectStack, SIGNAL(changeEffectState(ClipItem*,int,QList<int>,bool)), trackView->projectView(), SLOT(slotChangeEffectState(ClipItem*,int,QList<int>,bool)));
    connect(m_effectStack, SIGNAL(changeEffectPosition(ClipItem*,int,QList<int>,int)), trackView->projectView(), SLOT(slotChangeEffectPosition(ClipItem*,int,QList<int>,int)));
    
    connect(m_effectStack, SIGNAL(refreshEffectStack(ClipItem*)), trackView->projectView(), SLOT(slotRefreshEffects(ClipItem*)));
    connect(m_effectStack, SIGNAL(seekTimeline(int)), trackView->projectView(), SLOT(seekCursorPos(int)));
    connect(m_effectStack, SIGNAL(importClipKeyframes(GraphicsRectItem, QMap<QString,QString>)), trackView->projectView(), SLOT(slotImportClipKeyframes(GraphicsRectItem, QMap<QString,QString>)));
    connect(m_effectStack, SIGNAL(reloadEffects()), this, SLOT(slotReloadEffects()));
    connect(m_effectStack, SIGNAL(displayMessage(QString,int)), this, SLOT(slotGotProgressInfo(QString,int)));
    
    // Transition config signals
    connect(m_transitionConfig, SIGNAL(transitionUpdated(Transition*,QDomElement)), trackView->projectView() , SLOT(slotTransitionUpdated(Transition*,QDomElement)));
    connect(m_transitionConfig, SIGNAL(importClipKeyframes(GraphicsRectItem, QMap<QString,QString>)), trackView->projectView() , SLOT(slotImportClipKeyframes(GraphicsRectItem, QMap<QString,QString>)));
    connect(m_transitionConfig, SIGNAL(seekTimeline(int)), trackView->projectView() , SLOT(seekCursorPos(int)));

    connect(trackView->projectView(), SIGNAL(activateDocumentMonitor()), m_projectMonitor, SLOT(slotActivateMonitor()));
    connect(trackView, SIGNAL(zoneMoved(int,int)), this, SLOT(slotZoneMoved(int,int)));
    trackView->projectView()->setContextMenu(m_timelineContextMenu, m_timelineContextClipMenu, m_timelineContextTransitionMenu, m_clipTypeGroup, static_cast<QMenu*>(factory()->container("marker_menu", this)));
    if (m_renderWidget) {
        slotCheckRenderStatus();
        m_renderWidget->setProfile(project->mltProfile());
        m_renderWidget->setGuides(pCore->projectManager()->currentTimeline()->projectView()->guidesData(), project->projectDuration());
        m_renderWidget->setDocumentPath(project->projectFolder().path() + QDir::separator());
        m_renderWidget->setRenderProfile(project->getRenderProperties());
    }
    m_zoomSlider->setValue(project->zoom().x());
    m_commandStack->setActiveStack(project->commandStack());
    KdenliveSettings::setProject_display_ratio(project->dar());

    setWindowTitle(project->description());
    setWindowModified(project->isModified());
    m_saveAction->setEnabled(project->isModified());
    m_normalEditTool->setChecked(true);
    connect(m_projectMonitor, SIGNAL(durationChanged(int)), this, SLOT(slotUpdateProjectDuration(int)));
    pCore->monitorManager()->setDocument(project);
    trackView->updateProjectFps();
    if (m_recMonitor) {
        m_recMonitor->slotUpdateCaptureFolder(project->projectFolder().path() + QDir::separator());
    }
    //Update the mouse position display so it will display in DF/NDF format by default based on the project setting.
    slotUpdateMousePosition(0);

    // Update guides info in render widget
    slotGuidesUpdated();

    // Make sure monitor is visible so that it is painted black on startup
    //show();
    //pCore->monitorManager()->activateMonitor(Kdenlive::ClipMonitor, true);
    // set tool to select tool
    m_buttonSelectTool->setChecked(true);
    connect(m_projectMonitorDock, SIGNAL(visibilityChanged(bool)), m_projectMonitor, SLOT(refreshMonitor(bool)), Qt::UniqueConnection);
    connect(m_clipMonitorDock, SIGNAL(visibilityChanged(bool)), m_clipMonitor, SLOT(refreshMonitor(bool)), Qt::UniqueConnection);
}

void MainWindow::slotZoneMoved(int start, int end)
{
    pCore->projectManager()->current()->setZone(start, end);
    m_projectMonitor->slotZoneMoved(start, end);
}

void MainWindow::slotGuidesUpdated()
{
    if (m_renderWidget)
        m_renderWidget->setGuides(pCore->projectManager()->currentTimeline()->projectView()->guidesData(), pCore->projectManager()->current()->projectDuration());
}

void MainWindow::slotEditKeys()
{
    KShortcutsDialog dialog(KShortcutsEditor::AllActions, KShortcutsEditor::LetterShortcutsAllowed, this);
    dialog.addCollection(actionCollection(), i18nc("general keyboard shortcuts", "General"));
    dialog.addCollection(m_effectsActionCollection, i18nc("effects and transitions keyboard shortcuts", "Effects & Transitions"));
    dialog.addCollection(m_tracksActionCollection, i18nc("timeline track keyboard shortcuts", "Timeline and Tracks"));
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

    if (KConfigDialog::showDialog("settings")) {
        KdenliveSettingsDialog* d = static_cast <KdenliveSettingsDialog*>(KConfigDialog::exists("settings"));
        if (page != -1) d->showPage(page, option);
        return;
    }

    // KConfigDialog didn't find an instance of this dialog, so lets
    // create it :

    // Get the mappable actions in localized form
    QMap<QString, QString> actions;
    KActionCollection* collection = actionCollection();
    foreach (const QString& action_name, m_actionNames) {
        actions[collection->action(action_name)->text()] = action_name;
    }

    KdenliveSettingsDialog* dialog = new KdenliveSettingsDialog(actions, m_gpuAllowed, this);
    connect(dialog, SIGNAL(settingsChanged(QString)), this, SLOT(updateConfiguration()));
    connect(dialog, SIGNAL(settingsChanged(QString)), SIGNAL(configurationChanged()));
    connect(dialog, SIGNAL(doResetProfile()), pCore->projectManager(), SLOT(slotResetProfiles()));
    connect(dialog, SIGNAL(restartKdenlive()), this, SLOT(slotRestart()));
    if (m_recMonitor) {
        connect(dialog, SIGNAL(updateCaptureFolder()), this, SLOT(slotUpdateCaptureFolder()));
        connect(dialog, SIGNAL(updateFullScreenGrab()), m_recMonitor, SLOT(slotUpdateFullScreenGrab()));
    }
    dialog->show();
    if (page != -1) {
        dialog->showPage(page, option);
    }
}

void MainWindow::slotRestart()
{
    m_exitCode = EXIT_RESTART;
    QApplication::closeAllWindows();
}

void MainWindow::closeEvent(QCloseEvent* event)
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
        if (pCore->projectManager()->current())
            m_recMonitor->slotUpdateCaptureFolder(pCore->projectManager()->current()->projectFolder().path() + QDir::separator());
        else
            m_recMonitor->slotUpdateCaptureFolder(KdenliveSettings::defaultprojectfolder());
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
    m_buttonAutomaticSplitAudio->setChecked(KdenliveSettings::splitaudio());

    // Update list of transcoding profiles
    loadTranscoders();
    loadClipActions();
}

void MainWindow::slotSwitchSplitAudio()
{
    KdenliveSettings::setSplitaudio(!KdenliveSettings::splitaudio());
    m_buttonAutomaticSplitAudio->setChecked(KdenliveSettings::splitaudio());
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


void MainWindow::slotDeleteItem()
{
    if (QApplication::focusWidget() &&
            QApplication::focusWidget()->parentWidget() &&
            QApplication::focusWidget()->parentWidget()->parentWidget() &&
            QApplication::focusWidget()->parentWidget()->parentWidget() == pCore->bin()) {
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

    ClipController *clip = NULL;
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
        pCore->bin()->slotAddClipMarker(id, QList <CommentedTime>() << d->newMarker());
        QString hash = clip->getClipHash();
        if (!hash.isEmpty()) project->cacheImage(hash + '#' + QString::number(d->newMarker().time().frames(project->fps())), d->markerImage());
    }
    delete d;
}

void MainWindow::slotDeleteClipMarker()
{
    ClipController *clip = NULL;
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
        m_messageLabel->setMessage(i18n("No marker found at cursor time"), ErrorMessage);
        return;
    }
    pCore->bin()->deleteClipMarker(comment, id, pos);
}

void MainWindow::slotDeleteAllClipMarkers()
{
    ClipController *clip = NULL;
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
    ClipController *clip = NULL;
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
        pCore->bin()->slotAddClipMarker(id, QList <CommentedTime>() <<d->newMarker());
        QString hash = clip->getClipHash();
        if (!hash.isEmpty()) pCore->projectManager()->current()->cacheImage(hash + '#' + QString::number(d->newMarker().time().frames(pCore->projectManager()->current()->fps())), d->markerImage());
        if (d->newMarker().time() != pos) {
            // remove old marker
            oldMarker.setMarkerType(-1);
            pCore->bin()->slotAddClipMarker(id, QList <CommentedTime>() <<oldMarker);
        }
    }
    delete d;
}

void MainWindow::slotAddMarkerGuideQuickly()
{
    if (!pCore->projectManager()->currentTimeline() || !pCore->projectManager()->current())
        return;

    if (m_clipMonitor->isActive()) {
        ClipController *clip = m_clipMonitor->currentController();
        GenTime pos = m_clipMonitor->position();

        if (!clip) {
            m_messageLabel->setMessage(i18n("Cannot find clip to add marker"), ErrorMessage);
            return;
        }
        //TODO: allow user to set default marker category
        CommentedTime marker(pos, pCore->projectManager()->current()->timecode().getDisplayTimecode(pos, false), KdenliveSettings::default_marker_type());
        pCore->bin()->slotAddClipMarker(clip->clipId(), QList <CommentedTime>() <<marker);
    } else {
        pCore->projectManager()->currentTimeline()->projectView()->slotAddGuide(false);
    }
}

void MainWindow::slotAddGuide()
{
    if (pCore->projectManager()->currentTimeline())
        pCore->projectManager()->currentTimeline()->projectView()->slotAddGuide();
}

void MainWindow::slotInsertSpace()
{
    if (pCore->projectManager()->currentTimeline())
        pCore->projectManager()->currentTimeline()->projectView()->slotInsertSpace();
}

void MainWindow::slotRemoveSpace()
{
    if (pCore->projectManager()->currentTimeline())
        pCore->projectManager()->currentTimeline()->projectView()->slotRemoveSpace();
}

void MainWindow::slotRippleDelete()
{
  if (!m_projectMonitor->isActive() || !pCore->projectManager()->currentTimeline()) return;
      
  int zoneStart = m_projectMonitor->getZoneStart();
  int zoneEnd = m_projectMonitor->getZoneEnd();
  if (!zoneStart && zoneEnd == (pCore->projectManager()->currentTimeline()->duration() - 1)) return;

  int zoneFrameCount = zoneEnd - zoneStart;
  
  m_projectMonitor->slotZoneStart();
  pCore->projectManager()->currentTimeline()->projectView()->setCursorPos(zoneStart);
  pCore->projectManager()->currentTimeline()->projectView()->slotSelectAllClips();
  pCore->projectManager()->currentTimeline()->projectView()->cutSelectedClips();    
  pCore->projectManager()->currentTimeline()->projectView()->resetSelectionGroup(false);
  m_projectMonitor->slotZoneEnd();
  pCore->projectManager()->currentTimeline()->projectView()->setCursorPos(zoneEnd);
  zoneEnd++;
  pCore->projectManager()->currentTimeline()->projectView()->selectItemsRightOfFrame(zoneEnd);
  pCore->projectManager()->currentTimeline()->projectView()->setInPoint();
  pCore->projectManager()->currentTimeline()->projectView()->resetSelectionGroup(false);

  zoneEnd++;
  pCore->projectManager()->currentTimeline()->projectView()->selectItemsRightOfFrame(zoneEnd);
  
  pCore->projectManager()->currentTimeline()->projectView()->spaceToolMoveToSnapPos((double) zoneEnd);
  pCore->projectManager()->currentTimeline()->projectView()->spaceToolMoveToSnapPos((double) zoneStart);
  
  GenTime timeOffset = GenTime(zoneFrameCount * -1, pCore->projectManager()->current()->fps());
  pCore->projectManager()->currentTimeline()->projectView()->completeSpaceOperation(-1, timeOffset);
  
  m_projectMonitor->slotZoneStart();
  pCore->projectManager()->currentTimeline()->projectView()->setCursorPos(zoneStart);

  pCore->projectManager()->currentTimeline()->projectView()->resetSelectionGroup(false);
  
  return;
}

void MainWindow::slotInsertTrack(int ix)
{
    pCore->monitorManager()->activateMonitor(Kdenlive::ProjectMonitor);
    if (pCore->projectManager()->currentTimeline()) {
        if (ix == -1) {
            ix = pCore->projectManager()->currentTimeline()->projectView()->selectedTrack();
        }
        pCore->projectManager()->currentTimeline()->projectView()->slotInsertTrack(ix);
    }
    if (pCore->projectManager()->current()) {
        m_transitionConfig->updateProjectFormat();
    }
}

void MainWindow::slotDeleteTrack(int ix)
{
    pCore->monitorManager()->activateMonitor(Kdenlive::ProjectMonitor);
    if (pCore->projectManager()->currentTimeline()) {
        if (ix == -1) {
            ix = pCore->projectManager()->currentTimeline()->projectView()->selectedTrack();
        }
        pCore->projectManager()->currentTimeline()->projectView()->slotDeleteTrack(ix);
    }
    if (pCore->projectManager()->current()) {
        m_transitionConfig->updateProjectFormat();
    }
}

void MainWindow::slotConfigTrack(int ix)
{
    pCore->monitorManager()->activateMonitor(Kdenlive::ProjectMonitor);
    if (pCore->projectManager()->currentTimeline())
        pCore->projectManager()->currentTimeline()->projectView()->slotConfigTracks(ix);
    if (pCore->projectManager()->current())
        m_transitionConfig->updateProjectFormat();
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
    if (pCore->projectManager()->currentTimeline())
        pCore->projectManager()->currentTimeline()->projectView()->slotSelectAllClips();
}

void MainWindow::slotEditGuide()
{
    if (pCore->projectManager()->currentTimeline())
        pCore->projectManager()->currentTimeline()->projectView()->slotEditGuide();
}

void MainWindow::slotDeleteGuide()
{
    if (pCore->projectManager()->currentTimeline())
        pCore->projectManager()->currentTimeline()->projectView()->slotDeleteGuide();
}

void MainWindow::slotDeleteAllGuides()
{
    if (pCore->projectManager()->currentTimeline())
        pCore->projectManager()->currentTimeline()->projectView()->slotDeleteAllGuides();
}

void MainWindow::slotCutTimelineClip()
{
    if (pCore->projectManager()->currentTimeline())
        pCore->projectManager()->currentTimeline()->projectView()->cutSelectedClips();
}

void MainWindow::slotInsertClipOverwrite()
{
    if (pCore->projectManager()->currentTimeline()) {
        QStringList data = m_clipMonitor->getZoneInfo();
        pCore->projectManager()->currentTimeline()->projectView()->insertZoneOverwrite(data, pCore->projectManager()->currentTimeline()->inPoint());
    }
}

void MainWindow::slotSelectTimelineClip()
{
    if (pCore->projectManager()->currentTimeline())
        pCore->projectManager()->currentTimeline()->projectView()->selectClip(true);
}

void MainWindow::slotSelectTimelineTransition()
{
    if (pCore->projectManager()->currentTimeline())
        pCore->projectManager()->currentTimeline()->projectView()->selectTransition(true);
}

void MainWindow::slotDeselectTimelineClip()
{
    if (pCore->projectManager()->currentTimeline())
        pCore->projectManager()->currentTimeline()->projectView()->selectClip(false, true);
}

void MainWindow::slotDeselectTimelineTransition()
{
    if (pCore->projectManager()->currentTimeline())
        pCore->projectManager()->currentTimeline()->projectView()->selectTransition(false, true);
}

void MainWindow::slotSelectAddTimelineClip()
{
    if (pCore->projectManager()->currentTimeline())
        pCore->projectManager()->currentTimeline()->projectView()->selectClip(true, true);
}

void MainWindow::slotSelectAddTimelineTransition()
{
    if (pCore->projectManager()->currentTimeline())
        pCore->projectManager()->currentTimeline()->projectView()->selectTransition(true, true);
}

void MainWindow::slotGroupClips()
{
    if (pCore->projectManager()->currentTimeline())
        pCore->projectManager()->currentTimeline()->projectView()->groupClips();
}

void MainWindow::slotUnGroupClips()
{
    if (pCore->projectManager()->currentTimeline())
        pCore->projectManager()->currentTimeline()->projectView()->groupClips(false);
}

void MainWindow::slotEditItemDuration()
{
    if (pCore->projectManager()->currentTimeline())
        pCore->projectManager()->currentTimeline()->projectView()->editItemDuration();
}

void MainWindow::slotAddProjectClip(const QUrl &url)
{
    pCore->bin()->droppedUrls(QList<QUrl>() << url);
}

void MainWindow::slotAddProjectClipList(const QList<QUrl> &urls)
{
    pCore->bin()->droppedUrls(urls);
}

void MainWindow::slotAddTransition(QAction *result)
{
    if (!result) return;
    QStringList info = result->data().toStringList();
    if (info.isEmpty()) return;
    QDomElement transition = transitions.getEffectByTag(info.at(1), info.at(2));
    if (pCore->projectManager()->currentTimeline() && !transition.isNull()) {
        pCore->projectManager()->currentTimeline()->projectView()->slotAddTransitionToSelectedClips(transition.cloneNode().toElement());
    }
}

void MainWindow::slotAddVideoEffect(QAction *result)
{
    if (!result) {
        return;
    }
    const int VideoEffect = 1;
    const int AudioEffect = 2;
    QStringList info = result->data().toStringList();

    if (info.isEmpty() || info.size() < 3) {
        return;
    }
    QDomElement effect ;
    if (info.last() == QString::number((int) VideoEffect)) {
        effect = videoEffects.getEffectByTag(info.at(0), info.at(1));
    } else if (info.last() == QString::number((int) AudioEffect)) {
        effect = audioEffects.getEffectByTag(info.at(0), info.at(1));
    } else {
        effect = customEffects.getEffectByTag(info.at(0), info.at(1));
    }

    if (!effect.isNull()) {
        slotAddEffect(effect);
    } else {
        m_messageLabel->setMessage(i18n("Cannot find effect %1 / %2", info.at(0), info.at(1)), ErrorMessage);
    }
}


void MainWindow::slotZoomIn()
{
    m_zoomSlider->setValue(m_zoomSlider->value() - 1);
    slotShowZoomSliderToolTip();
}

void MainWindow::slotZoomOut()
{
    m_zoomSlider->setValue(m_zoomSlider->value() + 1);
    slotShowZoomSliderToolTip();
}

void MainWindow::slotFitZoom()
{
    if (pCore->projectManager()->currentTimeline())
        m_zoomSlider->setValue(pCore->projectManager()->currentTimeline()->fitZoom());
}

void MainWindow::slotSetZoom(int value)
{
    value = qMax(m_zoomSlider->minimum(), value);
    value = qMin(m_zoomSlider->maximum(), value);

    if (pCore->projectManager()->currentTimeline()) {
        pCore->projectManager()->currentTimeline()->slotChangeZoom(value);
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
    if (type == DefaultMessage) {
        m_statusProgressBar->setValue(progress);
    }
    m_messageLabel->setMessage(message, type);
    if (progress >= 0) {
        if (type == DefaultMessage) {
            m_statusProgressBar->setVisible(true);
        }
    } else {
        m_statusProgressBar->setVisible(false);
    }
}

void MainWindow::customEvent(QEvent* e)
{
    if (e->type() == QEvent::User)
        m_messageLabel->setMessage(static_cast <MltErrorEvent *>(e)->message(), MltError);
}

void MainWindow::slotTimelineClipSelected(ClipItem* item, bool raise)
{
    if (item != m_mainClip) {
        if (m_mainClip) {
            m_mainClip->setMainSelectedClip(false);
        }
        if (item) {
            item->setMainSelectedClip(true);
        }
        m_mainClip = item;
    }
    m_effectStack->slotClipItemSelected(item, m_projectMonitor);
    m_projectMonitor->slotSetSelectedClip(item);
    if (raise) {
        m_effectStack->raiseWindow(m_effectStackDock);
    }
}

void MainWindow::slotTrackSelected(int index, const TrackInfo &info, bool raise)
{
    m_effectStack->slotTrackItemSelected(index, info, m_projectMonitor);
    if (raise) {
        m_effectStack->raiseWindow(m_effectStackDock);
    }
}

void MainWindow::slotActivateTransitionView(Transition *transition)
{
    if (transition)
        m_transitionConfig->raiseWindow(m_transitionConfigDock);
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

void MainWindow::slotChangeTool(QAction * action)
{
    if (action == m_buttonSelectTool)
        slotSetTool(SelectTool);
    else if (action == m_buttonRazorTool)
        slotSetTool(RazorTool);
    else if (action == m_buttonSpacerTool)
        slotSetTool(SpacerTool);
}

void MainWindow::slotChangeEdit(QAction * action)
{
    if (!pCore->projectManager()->currentTimeline())
        return;

    if (action == m_overwriteEditTool)
        pCore->projectManager()->currentTimeline()->projectView()->setEditMode(OverwriteEdit);
    else if (action == m_insertEditTool)
        pCore->projectManager()->currentTimeline()->projectView()->setEditMode(InsertEdit);
    else
        pCore->projectManager()->currentTimeline()->projectView()->setEditMode(NormalEdit);
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
            message = i18n("Click on a clip to cut it");
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
    if (pCore->projectManager()->currentTimeline())
        pCore->projectManager()->currentTimeline()->projectView()->copyClip();
}

void MainWindow::slotPaste()
{
    if (pCore->projectManager()->currentTimeline())
        pCore->projectManager()->currentTimeline()->projectView()->pasteClip();
}

void MainWindow::slotPasteEffects()
{
    if (pCore->projectManager()->currentTimeline())
        pCore->projectManager()->currentTimeline()->projectView()->pasteClipEffects();
}

void MainWindow::slotClipInTimeline(const QString &clipId)
{
    if (pCore->projectManager()->currentTimeline()) {
        QList<ItemInfo> matching = pCore->projectManager()->currentTimeline()->projectView()->findId(clipId);

        QMenu *inTimelineMenu = static_cast<QMenu*>(factory()->container("clip_in_timeline", this));
        inTimelineMenu->clear();

        QList <QAction *> actionList;

        for (int i = 0; i < matching.count(); ++i) {
            QString track = QString::number(matching.at(i).track);
            QString start = pCore->projectManager()->current()->timecode().getTimecode(matching.at(i).startPos);
            int j = 0;
            QAction *a = new QAction(track + ": " + start, this);
            a->setData(QStringList() << track << start);
            connect(a, SIGNAL(triggered()), this, SLOT(slotSelectClipInTimeline()));
            while (j < actionList.count()) {
                if (actionList.at(j)->text() > a->text()) break;
                j++;
            }
            actionList.insert(j, a);
        }
        inTimelineMenu->addActions(actionList);

        if (matching.empty()) {
            inTimelineMenu->setEnabled(false);
        } else {
            inTimelineMenu->setEnabled(true);
        }
    }
}

void MainWindow::slotClipInProjectTree()
{
    if (pCore->projectManager()->currentTimeline()) {
        QStringList clipIds;
        if (m_mainClip) {
            clipIds << m_mainClip->getBinId();
        } else {
            clipIds = pCore->projectManager()->currentTimeline()->projectView()->selectedClips();
        }
        if (clipIds.isEmpty()) {
            return;
        }
        m_projectBinDock->raise();
        pCore->bin()->selectClipById(clipIds.at(0));
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
    if (isMinimized() && pCore->monitorManager())
        pCore->monitorManager()->pauseActiveMonitor();
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
        if (baseClip == NULL) {
            tmppath.append("untitled.mlt");
        } else {
            tmppath.append((baseClip->name().isEmpty() ? baseClip->fileURL().fileName() : baseClip->name()) + '-' + QString::number(zone.x()).rightJustified(4, '0') + ".mlt");
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
    if (pCore->projectManager()->currentTimeline())
        pCore->projectManager()->currentTimeline()->projectView()->setInPoint();
}

void MainWindow::slotResizeItemEnd()
{
    if (pCore->projectManager()->currentTimeline())
        pCore->projectManager()->currentTimeline()->projectView()->setOutPoint();
}

int MainWindow::getNewStuff(const QString &configFile)
{
    KNS3::Entry::List entries;
    QPointer<KNS3::DownloadDialog> dialog = new KNS3::DownloadDialog(configFile);
    if (dialog->exec()) entries = dialog->changedEntries();
    foreach(const KNS3::Entry & entry, entries) {
        if (entry.status() == KNS3::Entry::Installed)
            qDebug() << "// Installed files: " << entry.installedFiles();
    }
    delete dialog;
    return entries.size();
}

void MainWindow::slotGetNewTitleStuff()
{
    if (getNewStuff("kdenlive_titles.knsrc") > 0)
        TitleWidget::refreshTitleTemplates();
}

void MainWindow::slotGetNewLumaStuff()
{
    if (getNewStuff("kdenlive_wipes.knsrc") > 0) {
        initEffects::refreshLumas();
        pCore->projectManager()->currentTimeline()->projectView()->reloadTransitionLumas();
    }
}

void MainWindow::slotGetNewRenderStuff()
{
    if (getNewStuff("kdenlive_renderprofiles.knsrc") > 0)
        if (m_renderWidget)
            m_renderWidget->reloadProfiles();
}

void MainWindow::slotGetNewMltProfileStuff()
{
    if (getNewStuff("kdenlive_projectprofiles.knsrc") > 0) {
        // update the list of profiles in settings dialog
        KdenliveSettingsDialog* d = static_cast <KdenliveSettingsDialog*>(KConfigDialog::exists("settings"));
        if (d)
            d->checkProfile();
    }
}

void MainWindow::slotAutoTransition()
{
    if (pCore->projectManager()->currentTimeline())
        pCore->projectManager()->currentTimeline()->projectView()->autoTransition();
}

void MainWindow::slotSplitAudio()
{
    if (pCore->projectManager()->currentTimeline())
        pCore->projectManager()->currentTimeline()->projectView()->splitAudio();
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
        PlaylistState::ClipState state = PlaylistState::Original;
        if (action->data().toString() == "clip_audio_only") 
            state = PlaylistState::AudioOnly;
        else if (action->data().toString() == "clip_video_only")
            state = PlaylistState::VideoOnly;
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
    QMenu* actionMenu= static_cast<QMenu*>(factory()->container("clip_actions", this));
    if (actionMenu){
        actionMenu->clear();
        Mlt::Profile profile;
        Mlt::Filter *filter = Mlt::Factory::filter(profile,(char*)"vidstab");
        if (filter) {
            if (!filter->is_valid()) {
                delete filter;
            }
            else {
                delete filter;
                QAction *action=actionMenu->addAction(i18n("Stabilize"));
                QStringList stabJob;
                stabJob << QString::number((int) AbstractClipJob::FILTERCLIPJOB) << "vidstab";
                action->setData(stabJob);
                connect(action, SIGNAL(triggered(bool)), pCore->bin(), SLOT(slotStartClipJob(bool)));
            }
        }
        filter = Mlt::Factory::filter(profile,(char*)"motion_est");
        if (filter) {
            if (!filter->is_valid()) {
                delete filter;
            }
            else {
                delete filter;
                QAction *action=actionMenu->addAction(i18n("Automatic scene split"));
                QStringList stabJob;
                stabJob << QString::number((int) AbstractClipJob::FILTERCLIPJOB) << "motion_est";
                action->setData(stabJob);
                connect(action, SIGNAL(triggered(bool)), pCore->bin(), SLOT(slotStartClipJob(bool)));
            }
        }
        if (KdenliveSettings::producerslist().contains("framebuffer")) {
            QAction *action=actionMenu->addAction(i18n("Reverse clip"));
            QStringList stabJob;
            stabJob << QString::number((int) AbstractClipJob::FILTERCLIPJOB) << "framebuffer";
            action->setData(stabJob);
            connect(action, SIGNAL(triggered(bool)), pCore->bin(), SLOT(slotStartClipJob(bool)));
        }
    }

}

void MainWindow::loadTranscoders()
{
    QMenu *transMenu = static_cast<QMenu*>(factory()->container("transcoders", this));
    transMenu->clear();

    QMenu *extractAudioMenu = static_cast<QMenu*>(factory()->container("extract_audio", this));
    extractAudioMenu->clear();

    KSharedConfigPtr config = KSharedConfig::openConfig(QStandardPaths::locate(QStandardPaths::DataLocation, "kdenlivetranscodingrc"), KConfig::CascadeConfig);
    KConfigGroup transConfig(config, "Transcoding");
    // read the entries
    QMap< QString, QString > profiles = transConfig.entryMap();
    QMapIterator<QString, QString> i(profiles);
    while (i.hasNext()) {
        i.next();
        QStringList data;
        data << QString::number((int) AbstractClipJob::TRANSCODEJOB);
        data << i.value().split(';');
        QAction *a;
        // separate audio transcoding in a separate menu
        if (data.count() > 3 && data.at(3) == "audio") {
            a = extractAudioMenu->addAction(i.key());
        }
        else {
            a = transMenu->addAction(i.key());
        }
        a->setData(data);
        if (data.count() > 1) a->setToolTip(data.at(1));
        // slottranscode
        connect(a, SIGNAL(triggered(bool)), pCore->bin(), SLOT(slotStartClipJob(bool)));
    }
}

void MainWindow::slotTranscode(const QStringList &urls)
{
    QString params;
    QString desc;
    QString condition;
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
    ClipTranscode *d = new ClipTranscode(urls, params, QStringList(), desc);
    connect(d, SIGNAL(addClip(QUrl)), this, SLOT(slotAddProjectClip(QUrl)));
    d->show();
}

void MainWindow::slotTranscodeClip()
{
    QString allExtensions = ClipCreationDialog::getExtensions().join(" ");
    const QString dialogFilter =  i18n("All Supported Files") + '(' + allExtensions + ");;" + i18n("All Files") + "(*)";
    QString clipFolder = KRecentDirs::dir(":KdenliveClipFolder");
    QStringList urls = QFileDialog::getOpenFileNames(this, i18n("Files to transcode"), clipFolder, dialogFilter);
    if (urls.isEmpty()) return;
    slotTranscode(urls);
}

void MainWindow::slotSetDocumentRenderProfile(const QMap <QString, QString> &props)
{
    QMapIterator<QString, QString> i(props);
    while (i.hasNext()) {
        i.next();
        pCore->projectManager()->current()->setDocumentProperty(i.key(), i.value());
    }
    pCore->projectManager()->current()->setModified(true);
}


void MainWindow::slotPrepareRendering(bool scriptExport, bool zoneOnly, const QString &chapterFile)
{
    KdenliveDoc *project = pCore->projectManager()->current();

    if (m_renderWidget == NULL) return;
    QString scriptPath;
    QString playlistPath;
    QString mltSuffix(".mlt");
    QList<QString> playlistPaths;
    QList<QString> trackNames;
    const QList <TrackInfo> trackInfoList = pCore->projectManager()->currentTimeline()->getTracksInfo();
    int tracksCount = 1;
    bool stemExport = m_renderWidget->isStemAudioExportEnabled();

    if (scriptExport) {
        //QString scriptsFolder = project->projectFolder().path(QUrl::AddTrailingSlash) + "scripts/";
        QString path = m_renderWidget->getFreeScriptName(project->url());
        QPointer<KUrlRequesterDialog> getUrl = new KUrlRequesterDialog(QUrl::fromLocalFile(path), i18n("Create Render Script"), this);
        getUrl->urlRequester()->setMode(KFile::File);
        if (getUrl->exec() == QDialog::Rejected) {
            delete getUrl;
            return;
        }
        scriptPath = getUrl->selectedUrl().path();
        delete getUrl;
        QFile f(scriptPath);
        if (f.exists()) {
            if (KMessageBox::warningYesNo(this, i18n("Script file already exists. Do you want to overwrite it?")) != KMessageBox::Yes)
                return;
        }
        playlistPath = scriptPath;
    } else {
        QTemporaryFile temp(QDir::tempPath() + QLatin1String("/kdenlive_rendering_XXXXXX.mlt"));
        temp.setAutoRemove(false);
        temp.open();
        playlistPath = temp.fileName();
    }

    QString playlistContent = m_projectMonitor->sceneList();
    if (!chapterFile.isEmpty()) {
        int in = 0;
        int out;
        if (!zoneOnly) out = (int) GenTime(project->projectDuration()).frames(project->fps());
        else {
            in = pCore->projectManager()->currentTimeline()->inPoint();
            out = pCore->projectManager()->currentTimeline()->outPoint();
        }
        QDomDocument doc;
        QDomElement chapters = doc.createElement("chapters");
        chapters.setAttribute("fps", project->fps());
        doc.appendChild(chapters);

        
        QMap <double, QString> guidesData = pCore->projectManager()->currentTimeline()->projectView()->guidesData();
        QMapIterator<double, QString> g(guidesData);
        QLocale locale;
        while (g.hasNext()) {
            g.next();
            int time = (int) GenTime(g.key()).frames(project->fps());
            if (time >= in && time < out) {
                if (zoneOnly) time = time - in;
                QDomElement chapter = doc.createElement("chapter");
                chapters.appendChild(chapter);
                chapter.setAttribute("title", g.value());
                chapter.setAttribute("time", time);
            }
        }

        if (chapters.childNodes().count() > 0) {
            if (pCore->projectManager()->currentTimeline()->projectView()->hasGuide(out, 0) == -1) {
                // Always insert a guide in pos 0
                QDomElement chapter = doc.createElement("chapter");
                chapters.insertBefore(chapter, QDomNode());
                chapter.setAttribute("title", i18nc("the first in a list of chapters", "Start"));
                chapter.setAttribute("time", "0");
            }
            // save chapters file
            QFile file(chapterFile);
            if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
                qWarning() << "//////  ERROR writing DVD CHAPTER file: " << chapterFile;
            } else {
                file.write(doc.toString().toUtf8());
                if (file.error() != QFile::NoError) {
                    qWarning() << "//////  ERROR writing DVD CHAPTER file: " << chapterFile;
                }
                file.close();
            }
        }
    }

    // check if audio export is selected
    bool exportAudio;
    if (m_renderWidget->automaticAudioExport()) {
        exportAudio = pCore->projectManager()->currentTimeline()->checkProjectAudio();
    } else exportAudio = m_renderWidget->selectedAudioExport();

    // Set playlist audio volume to 100%
    QDomDocument doc;
    doc.setContent(playlistContent);
    QDomElement tractor = doc.documentElement().firstChildElement("tractor");
    if (!tractor.isNull()) {
        QDomNodeList props = tractor.elementsByTagName("property");
        for (int i = 0; i < props.count(); ++i) {
            if (props.at(i).toElement().attribute("name") == "meta.volume") {
                props.at(i).firstChild().setNodeValue("1");
                break;
            }
        }
    }

    // Do we want proxy rendering
    if (project->useProxy() && !m_renderWidget->proxyRendering()) {
        QString root = doc.documentElement().attribute("root");

        // replace proxy clips with originals
        //TODO
        QMap <QString, QString> proxies; // = m_projectList->getProxies();

        QDomNodeList producers = doc.elementsByTagName("producer");
        QString producerResource;
        QString suffix;
        for (int n = 0; n < producers.length(); ++n) {
            QDomElement e = producers.item(n).toElement();
            producerResource = EffectsList::property(e, "resource");
            if (producerResource.isEmpty()) {
                continue;
            }
            if (!producerResource.startsWith('/')) {
                producerResource.prepend(root + '/');
            }
            if (producerResource.contains('?')) {
                // slowmotion producer
                suffix = '?' + producerResource.section('?', 1);
                producerResource = producerResource.section('?', 0, 0);
            }
            else {
                suffix.clear();
            }
            if (!producerResource.isEmpty()) {
                if (proxies.contains(producerResource)) {
                    EffectsList::setProperty(e, "resource", proxies.value(producerResource) + suffix);
                    // We need to delete the "aspect_ratio" property because proxy clips
                    // sometimes have different ratio than original clips
                    EffectsList::removeProperty(e, "aspect_ratio");
                    EffectsList::removeMetaProperties(e);
                }
            }
        }
    }

    QList<QDomDocument> docList;

    // check which audio tracks have to be exported
    if (stemExport) {
        CustomTrackView* ctv = pCore->projectManager()->currentTimeline()->projectView();
        int trackInfoCount = trackInfoList.count();
        tracksCount = 0;

        for (int i = 0; i < trackInfoCount; i++) {
            TrackInfo info = trackInfoList.at(trackInfoCount - i - 1);
            if (!info.isMute && ctv->hasAudio(i)) {
                QDomDocument docCopy = doc.cloneNode(true).toDocument();
                // save track name
                trackNames << info.trackName;
                qDebug() << "Track-Name: " << info.trackName;
                // create stem export playlist content
                QDomNodeList tracks = docCopy.elementsByTagName("track");
                for (int j = trackInfoCount; j >= 0; j--) {
                    if (j != (trackInfoCount - i)) {
                        tracks.at(j).toElement().setAttribute("hide", "both");
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
            plPath = plPath + "_" + QString(trackNames.at(i)).replace(" ", "_");
        }
        // add mlt suffix
        plPath += mltSuffix;
        playlistPaths << plPath;
        qDebug() << "playlistPath: " << plPath << endl;

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
    m_renderWidget->slotExport(scriptExport,
            pCore->projectManager()->currentTimeline()->inPoint(),
            pCore->projectManager()->currentTimeline()->outPoint(),
            project->metadata(),
            playlistPaths, trackNames, scriptPath, exportAudio);
}

void MainWindow::slotUpdateTimecodeFormat(int ix)
{
    KdenliveSettings::setFrametimecode(ix == 1);
    m_clipMonitor->updateTimecodeFormat();
    m_projectMonitor->updateTimecodeFormat();
    m_transitionConfig->updateTimecodeFormat();
    m_effectStack->updateTimecodeFormat();
    pCore->bin()->updateTimecodeFormat();
    //pCore->projectManager()->currentTimeline()->projectView()->clearSelection();
    pCore->projectManager()->currentTimeline()->updateRuler();
    slotUpdateMousePosition(pCore->projectManager()->currentTimeline()->projectView()->getMousePos());
}

void MainWindow::slotRemoveFocus()
{
    statusBar()->setFocus();
    statusBar()->clearFocus();
}

void MainWindow::slotShutdown()
{
    pCore->projectManager()->current()->setModified(false);
    // Call shutdown
    QDBusConnectionInterface* interface = QDBusConnection::sessionBus().interface();
    if (interface && interface->isServiceRegistered("org.kde.ksmserver")) {
        QDBusInterface smserver("org.kde.ksmserver", "/KSMServer", "org.kde.KSMServerInterface");
        smserver.call("logout", 1, 2, 2);
    } else if (interface && interface->isServiceRegistered("org.gnome.SessionManager")) {
        QDBusInterface smserver("org.gnome.SessionManager", "/org/gnome/SessionManager", "org.gnome.SessionManager");
        smserver.call("Shutdown");
    }
}

void MainWindow::slotUpdateTrackInfo()
{
    m_transitionConfig->updateProjectFormat();
}

void MainWindow::slotSwitchMonitors()
{
    pCore->monitorManager()->slotSwitchMonitors(!m_clipMonitor->isActive());
    if (m_projectMonitor->isActive()) pCore->projectManager()->currentTimeline()->projectView()->setFocus();
    else pCore->bin()->focusBinView();
}

void MainWindow::slotSwitchMonitorOverlay(bool show)
{
    m_clipMonitor->switchMonitorInfo(show);
    m_projectMonitor->switchMonitorInfo(show);
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
    if (!m_clipMonitor->isActive() || m_clipMonitor->currentController() == NULL) return;
    QStringList info = m_clipMonitor->getZoneInfo();
    pCore->bin()->slotAddClipCut(info.at(0), info.at(1).toInt(), info.at(2).toInt());
}

void MainWindow::slotInsertZoneToTimeline()
{
    if (pCore->projectManager()->currentTimeline() == NULL || m_clipMonitor->currentController() == NULL) return;
    QStringList info = m_clipMonitor->getZoneInfo();
    pCore->projectManager()->currentTimeline()->projectView()->insertClipCut(m_clipMonitor->activeClipId(), info.at(1).toInt(), info.at(2).toInt());
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
    qDebug() << "Any scope accepting new frames? " << request;
#endif
    if (!request) {
        m_projectMonitor->render->sendFrameForAnalysis = false;
    }
}


void MainWindow::slotOpenStopmotion()
{
    if (m_stopmotion == NULL) {
        m_stopmotion = new StopmotionWidget(pCore->monitorManager(), pCore->projectManager()->current()->projectFolder(), m_stopmotion_actions->actions(), this);
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
    if (m_renderWidget) m_renderWidget->updateProxyConfig(project->useProxy());
    if (KdenliveSettings::enableproxy()) {
        QDir dir(pCore->projectManager()->current()->projectFolder().path());
        dir.mkdir("proxy");
    }
    //TODO
    //m_projectList->updateProxyConfig();
}

void MainWindow::slotArchiveProject()
{
    QList <ClipController*> list = pCore->binController()->getControllerList();
    pCore->binController()->saveDocumentProperties(pCore->projectManager()->current()->documentProperties(), pCore->projectManager()->currentTimeline()->projectView()->guidesData());
    QDomDocument doc = pCore->projectManager()->current()->xmlSceneList(m_projectMonitor->sceneList());
    QPointer<ArchiveWidget> d = new ArchiveWidget(pCore->projectManager()->current()->url().fileName(), doc, list, pCore->projectManager()->currentTimeline()->projectView()->extractTransitionsLumas(), this);
    if (d->exec()) {
        m_messageLabel->setMessage(i18n("Archiving project"), OperationCompletedMessage);
    }
    delete d;
}

void MainWindow::slotElapsedTime()
{
    //qDebug()<<"-----------------------------------------\n"<<"Time elapsed: "<<m_timer.elapsed()<<"\n-------------------------";
}


void MainWindow::slotDownloadResources()
{
    QString currentFolder;
    if (pCore->projectManager()->current())
        currentFolder = pCore->projectManager()->current()->projectFolder().path();
    else
        currentFolder = KdenliveSettings::defaultprojectfolder();
    ResourceWidget *d = new ResourceWidget(currentFolder);
    connect(d, SIGNAL(addClip(QUrl)), this, SLOT(slotAddProjectClip(QUrl)));
    d->show();
}


void MainWindow::slotSaveTimelineClip()
{
    if (pCore->projectManager()->currentTimeline() && m_projectMonitor->render) {
        ClipItem *clip = pCore->projectManager()->currentTimeline()->projectView()->getActiveClipUnderCursor(true);
        if (!clip) {
            m_messageLabel->setMessage(i18n("Select a clip to save"), InformationMessage);
            return;
        }
        QUrl url = QFileDialog::getSaveFileUrl(this, i18n("Save clip"), pCore->projectManager()->current()->projectFolder(), i18n("MLT playlist (*.mlt)"));
        if (url.isValid()) {
            m_projectMonitor->render->saveClip(pCore->projectManager()->currentTimeline()->tracksCount() - clip->track(), clip->startPos(), url);
        }
    }
}

void MainWindow::slotProcessImportKeyframes(GraphicsRectItem type, const QString& data, int maximum)
{
    if (type == AVWidget) {
        // This data should be sent to the effect stack
        m_effectStack->setKeyframes(data, maximum);
    }
    else if (type == TransitionWidget) {
        // This data should be sent to the transition stack
        m_transitionConfig->setKeyframes(data, maximum);
    }
    else {
        // Error
    }
}

void MainWindow::slotAlignPlayheadToMousePos()
{
    pCore->monitorManager()->activateMonitor(Kdenlive::ProjectMonitor);
    pCore->projectManager()->currentTimeline()->projectView()->slotAlignPlayheadToMousePos();
}

QDockWidget *MainWindow::addDock(const QString &title, const QString &objectName, QWidget* widget, Qt::DockWidgetArea area)
{
    QDockWidget *dockWidget = new QDockWidget(title, this);
    dockWidget->setObjectName(objectName);
    dockWidget->setWidget(widget);
    addDockWidget(area, dockWidget);
    return dockWidget;
}


#ifdef DEBUG_MAINW
#undef DEBUG_MAINW
#endif
