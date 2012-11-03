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
#include "kdenlivesettings.h"
#include "kdenlivesettingsdialog.h"
#include "initeffects.h"
#include "profilesdialog.h"
#include "projectsettings.h"
#include "events.h"
#include "clipmanager.h"
#include "projectlist.h"
#include "monitor.h"
#include "recmonitor.h"
#include "monitormanager.h"
#include "kdenlivedoc.h"
#include "trackview.h"
#include "customtrackview.h"
#include "effectslistview.h"
#include "effectstack/effectstackview2.h"
#include "transitionsettings.h"
#include "renderwidget.h"
#include "renderer.h"
#ifdef USE_JOGSHUTTLE
#include "jogshuttle.h"
#include "jogaction.h"
#include "jogshuttleconfig.h"
#endif
#include "clipproperties.h"
#include "wizard.h"
#include "commands/editclipcommand.h"
#include "titlewidget.h"
#include "markerdialog.h"
#include "clipitem.h"
#include "interfaces.h"
#include "config-kdenlive.h"
#include "cliptranscode.h"
#include "ui_templateclip_ui.h"
#include "scopes/scopemanager.h"
#include "scopes/colorscopes/vectorscope.h"
#include "scopes/colorscopes/waveform.h"
#include "scopes/colorscopes/rgbparade.h"
#include "scopes/colorscopes/histogram.h"
#include "scopes/audioscopes/audiosignal.h"
#include "scopes/audioscopes/audiospectrum.h"
#include "scopes/audioscopes/spectrogram.h"
#include "archivewidget.h"
#include "databackup/backupwidget.h"
#include "utils/resourcewidget.h"


#include <KApplication>
#include <KAction>
#include <KLocale>
#include <KGlobal>
#include <KActionCollection>
#include <KActionCategory>
#include <KStandardAction>
#include <KShortcutsDialog>
#include <KFileDialog>
#include <KMessageBox>
#include <KDebug>
#include <KIO/NetAccess>
#include <KSaveFile>
#include <KRuler>
#include <KConfigDialog>
#include <KXMLGUIFactory>
#include <KStatusBar>
#include <kstandarddirs.h>
#include <KUrlRequesterDialog>
#include <KTemporaryFile>
#include <KProcess>
#include <KActionMenu>
#include <KMenu>
#include <ktogglefullscreenaction.h>
#include <KFileItem>
#include <KNotification>
#include <KNotifyConfigWidget>
#if KDE_IS_VERSION(4,3,80)
#include <knewstuff3/downloaddialog.h>
#include <knewstuff3/knewstuffaction.h>
#else
#include <knewstuff2/engine.h>
#include <knewstuff2/ui/knewstuffaction.h>
#define KNS3 KNS
#endif
#include <KToolBar>
#include <KColorScheme>
#include <KProgressDialog>

#include <QTextStream>
#include <QTimer>
#include <QAction>
#include <QKeyEvent>
#include <QInputDialog>
#include <QDesktopWidget>
#include <QBitmap>

#include <stdlib.h>
#include <locale.h>

// Uncomment for deeper debugging
//#define DEBUG_MAINW

#ifdef DEBUG_MAINW
#include <QDebug>
#endif

static const char version[] = VERSION;

static const int ID_TIMELINE_POS = 0;
namespace Mlt
{
class Producer;
};

Q_DECLARE_METATYPE(QVector<int16_t>)


EffectsList MainWindow::videoEffects;
EffectsList MainWindow::audioEffects;
EffectsList MainWindow::customEffects;
EffectsList MainWindow::transitions;

QMap <QString,QImage> MainWindow::m_lumacache;

MainWindow::MainWindow(const QString &MltPath, const KUrl & Url, const QString & clipsToLoad, QWidget *parent) :
    KXmlGuiWindow(parent),
    m_activeDocument(NULL),
    m_activeTimeline(NULL),
    m_projectList(NULL),
    m_effectList(NULL),
    m_effectStack(NULL),
    m_clipMonitor(NULL),
    m_projectMonitor(NULL),
    m_recMonitor(NULL),
    m_renderWidget(NULL),
#ifdef USE_JOGSHUTTLE
    m_jogProcess(NULL),
    m_jogShuttle(NULL),
#endif
    m_findActivated(false),
    m_stopmotion(NULL),
    m_mainClip(NULL)
{
    qRegisterMetaType<QVector<int16_t> > ();
    qRegisterMetaType<stringMap> ("stringMap");
    qRegisterMetaType<audioByteArray> ("audioByteArray");

    // Init locale
    QLocale systemLocale = QLocale();
    setlocale(LC_NUMERIC, NULL);
    char *separator = localeconv()->decimal_point;
    if (separator != systemLocale.decimalPoint()) {
        kDebug()<<"------\n!!! system locale is not similar to Qt's locale... be prepared for bugs!!!\n------";
        // HACK: There is a locale conflict, so set locale to C
	// Make sure to override exported values or it won't work
	setenv("LANG", "C", 1);
	setlocale(LC_NUMERIC, "C");
        systemLocale = QLocale::c();
    }

    systemLocale.setNumberOptions(QLocale::OmitGroupSeparator);
    QLocale::setDefault(systemLocale);

    // Create DBus interface
    new MainWindowAdaptor(this);
    QDBusConnection dbus = QDBusConnection::sessionBus();
    dbus.registerObject("/MainWindow", this);

    if (!KdenliveSettings::colortheme().isEmpty()) slotChangePalette(NULL, KdenliveSettings::colortheme());
    setFont(KGlobalSettings::toolBarFont());
    parseProfiles(MltPath);
    KdenliveSettings::setCurrent_profile(KdenliveSettings::default_profile());
    m_commandStack = new QUndoGroup;
    setDockNestingEnabled(true);
    m_timelineArea = new KTabWidget(this);
    m_timelineArea->setTabReorderingEnabled(true);
    m_timelineArea->setTabBarHidden(true);
    m_timelineArea->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Preferred);
    m_timelineArea->setMinimumHeight(200);

    QToolButton *closeTabButton = new QToolButton;
    connect(closeTabButton, SIGNAL(clicked()), this, SLOT(closeCurrentDocument()));
    closeTabButton->setIcon(KIcon("tab-close"));
    closeTabButton->adjustSize();
    closeTabButton->setToolTip(i18n("Close the current tab"));
    m_timelineArea->setCornerWidget(closeTabButton);
    //connect(m_timelineArea, SIGNAL(currentChanged(int)), this, SLOT(activateDocument()));

    connect(&m_findTimer, SIGNAL(timeout()), this, SLOT(findTimeout()));
    m_findTimer.setSingleShot(true);

    // FIXME: the next call returns a newly allocated object, which leaks
    initEffects::parseEffectFiles();
    //initEffects::parseCustomEffectsFile();
    
    m_monitorManager = new MonitorManager();

    m_shortcutRemoveFocus = new QShortcut(QKeySequence("Esc"), this);
    connect(m_shortcutRemoveFocus, SIGNAL(activated()), this, SLOT(slotRemoveFocus()));


    /// Add Widgets ///

    m_projectListDock = new QDockWidget(i18n("Project Tree"), this);
    m_projectListDock->setObjectName("project_tree");
    m_projectList = new ProjectList();
    m_projectListDock->setWidget(m_projectList);
    addDockWidget(Qt::TopDockWidgetArea, m_projectListDock);

    m_clipMonitorDock = new QDockWidget(i18n("Clip Monitor"), this);
    m_clipMonitorDock->setObjectName("clip_monitor");
    m_clipMonitor = new Monitor(Kdenlive::clipMonitor, m_monitorManager, QString(), m_timelineArea);
    m_clipMonitorDock->setWidget(m_clipMonitor);

    // Connect the project list
    connect(m_projectList, SIGNAL(clipSelected(DocClipBase *, QPoint, bool)), m_clipMonitor, SLOT(slotSetClipProducer(DocClipBase *, QPoint, bool)));
    connect(m_projectList, SIGNAL(raiseClipMonitor(bool)), m_clipMonitor, SLOT(slotActivateMonitor(bool)));
    connect(m_projectList, SIGNAL(loadingIsOver()), this, SLOT(slotElapsedTime()));
    connect(m_projectList, SIGNAL(displayMessage(const QString&, int)), this, SLOT(slotGotProgressInfo(const QString&, int)));
    connect(m_projectList, SIGNAL(updateRenderStatus()), this, SLOT(slotCheckRenderStatus()));
    connect(m_projectList, SIGNAL(clipNeedsReload(const QString&)),this, SLOT(slotUpdateClip(const QString &)));
    connect(m_projectList, SIGNAL(updateProfile(const QString &)), this, SLOT(slotUpdateProjectProfile(const QString &)));
    connect(m_projectList, SIGNAL(refreshClip(const QString &, bool)), m_monitorManager, SLOT(slotRefreshCurrentMonitor(const QString &)));
    connect(m_projectList, SIGNAL(findInTimeline(const QString&)), this, SLOT(slotClipInTimeline(const QString&)));
    connect(m_clipMonitor, SIGNAL(zoneUpdated(QPoint)), m_projectList, SLOT(slotUpdateClipCut(QPoint)));
    connect(m_clipMonitor, SIGNAL(extractZone(const QString &, QPoint)), m_projectList, SLOT(slotCutClipJob(const QString &, QPoint)));

    m_projectMonitorDock = new QDockWidget(i18n("Project Monitor"), this);
    m_projectMonitorDock->setObjectName("project_monitor");
    m_projectMonitor = new Monitor(Kdenlive::projectMonitor, m_monitorManager, QString());
    m_projectMonitorDock->setWidget(m_projectMonitor);

#ifndef Q_WS_MAC
    m_recMonitorDock = new QDockWidget(i18n("Record Monitor"), this);
    m_recMonitorDock->setObjectName("record_monitor");
    m_recMonitor = new RecMonitor(Kdenlive::recordMonitor, m_monitorManager);
    m_recMonitorDock->setWidget(m_recMonitor);
    connect(m_recMonitor, SIGNAL(addProjectClip(KUrl)), this, SLOT(slotAddProjectClip(KUrl)));
    connect(m_recMonitor, SIGNAL(addProjectClipList(KUrl::List)), this, SLOT(slotAddProjectClipList(KUrl::List)));
    connect(m_recMonitor, SIGNAL(showConfigDialog(int, int)), this, SLOT(slotPreferences(int, int)));

#endif /* ! Q_WS_MAC */
    m_monitorManager->initMonitors(m_clipMonitor, m_projectMonitor, m_recMonitor);

    m_notesDock = new QDockWidget(i18n("Project Notes"), this);
    m_notesDock->setObjectName("notes_widget");
    m_notesWidget = new NotesWidget();
    connect(m_notesWidget, SIGNAL(insertNotesTimecode()), this, SLOT(slotInsertNotesTimecode()));
    connect(m_notesWidget, SIGNAL(seekProject(int)), m_projectMonitor->render, SLOT(seekToFrame(int)));

    m_notesWidget->setTabChangesFocus(true);
#if KDE_IS_VERSION(4,4,0)
    m_notesWidget->setClickMessage(i18n("Enter your project notes here ..."));
#endif
    m_notesDock->setWidget(m_notesWidget);
    addDockWidget(Qt::TopDockWidgetArea, m_notesDock);

    m_effectStackDock = new QDockWidget(i18n("Effect Stack"), this);
    m_effectStackDock->setObjectName("effect_stack");
    m_effectStack = new EffectStackView2(m_projectMonitor);
    m_effectStackDock->setWidget(m_effectStack);
    addDockWidget(Qt::TopDockWidgetArea, m_effectStackDock);
    connect(m_effectStack, SIGNAL(startFilterJob(ItemInfo, const QString&,const QString&,const QString&,const QString&,const QString&,const QMap <QString, QString>&)), m_projectList, SLOT(slotStartFilterJob(ItemInfo, const QString&,const QString&,const QString&,const QString&,const QString&,const QMap <QString, QString>&)));

    m_transitionConfigDock = new QDockWidget(i18n("Transition"), this);
    m_transitionConfigDock->setObjectName("transition");
    m_transitionConfig = new TransitionSettings(m_projectMonitor);
    m_transitionConfigDock->setWidget(m_transitionConfig);
    addDockWidget(Qt::TopDockWidgetArea, m_transitionConfigDock);

    m_effectListDock = new QDockWidget(i18n("Effect List"), this);
    m_effectListDock->setObjectName("effect_list");
    m_effectList = new EffectsListView();
    m_effectListDock->setWidget(m_effectList);
    addDockWidget(Qt::TopDockWidgetArea, m_effectListDock);

    m_scopeManager = new ScopeManager(m_monitorManager);
    m_vectorscope = new Vectorscope();
    m_vectorscopeDock = new QDockWidget(i18n("Vectorscope"), this);
    m_vectorscopeDock->setObjectName(m_vectorscope->widgetName());
    m_vectorscopeDock->setWidget(m_vectorscope);
    addDockWidget(Qt::TopDockWidgetArea, m_vectorscopeDock);
    m_scopeManager->addScope(m_vectorscope, m_vectorscopeDock);

    m_waveform = new Waveform();
    m_waveformDock = new QDockWidget(i18n("Waveform"), this);
    m_waveformDock->setObjectName(m_waveform->widgetName());
    m_waveformDock->setWidget(m_waveform);
    addDockWidget(Qt::TopDockWidgetArea, m_waveformDock);
    m_scopeManager->addScope(m_waveform, m_waveformDock);

    m_RGBParade = new RGBParade();
    m_RGBParadeDock = new QDockWidget(i18n("RGB Parade"), this);
    m_RGBParadeDock->setObjectName(m_RGBParade->widgetName());
    m_RGBParadeDock->setWidget(m_RGBParade);
    addDockWidget(Qt::TopDockWidgetArea, m_RGBParadeDock);
    m_scopeManager->addScope(m_RGBParade, m_RGBParadeDock);

    m_histogram = new Histogram();
    m_histogramDock = new QDockWidget(i18n("Histogram"), this);
    m_histogramDock->setObjectName(m_histogram->widgetName());
    m_histogramDock->setWidget(m_histogram);
    addDockWidget(Qt::TopDockWidgetArea, m_histogramDock);
    m_scopeManager->addScope(m_histogram, m_histogramDock);

    m_audiosignal = new AudioSignal;
    m_audiosignalDock = new QDockWidget(i18n("Audio Signal"), this);
    m_audiosignalDock->setObjectName("audiosignal");
    m_audiosignalDock->setWidget(m_audiosignal);
    addDockWidget(Qt::TopDockWidgetArea, m_audiosignalDock);
    m_scopeManager->addScope(m_audiosignal, m_audiosignalDock);

    m_audioSpectrum = new AudioSpectrum();
    m_audioSpectrumDock = new QDockWidget(i18n("AudioSpectrum"), this);
    m_audioSpectrumDock->setObjectName(m_audioSpectrum->widgetName());
    m_audioSpectrumDock->setWidget(m_audioSpectrum);
    addDockWidget(Qt::TopDockWidgetArea, m_audioSpectrumDock);
    m_scopeManager->addScope(m_audioSpectrum, m_audioSpectrumDock);

    m_spectrogram = new Spectrogram();
    m_spectrogramDock = new QDockWidget(i18n("Spectrogram"), this);
    m_spectrogramDock->setObjectName(m_spectrogram->widgetName());
    m_spectrogramDock->setWidget(m_spectrogram);
    addDockWidget(Qt::TopDockWidgetArea, m_spectrogramDock);
    m_scopeManager->addScope(m_spectrogram, m_spectrogramDock);

    // Add monitors here to keep them at the right of the window
    addDockWidget(Qt::TopDockWidgetArea, m_clipMonitorDock);
    addDockWidget(Qt::TopDockWidgetArea, m_projectMonitorDock);
#ifndef Q_WS_MAC
    addDockWidget(Qt::TopDockWidgetArea, m_recMonitorDock);
#endif

    m_undoViewDock = new QDockWidget(i18n("Undo History"), this);
    m_undoViewDock->setObjectName("undo_history");
    m_undoView = new QUndoView();
    m_undoView->setCleanIcon(KIcon("edit-clear"));
    m_undoView->setEmptyLabel(i18n("Clean"));
    m_undoViewDock->setWidget(m_undoView);
    m_undoView->setGroup(m_commandStack);
    addDockWidget(Qt::TopDockWidgetArea, m_undoViewDock);


    setupActions();
    connect(m_commandStack, SIGNAL(cleanChanged(bool)), m_saveAction, SLOT(setDisabled(bool)));


    // Close non-general docks for the initial layout
    // only show important ones
    m_histogramDock->close();
    m_RGBParadeDock->close();
    m_waveformDock->close();
    m_vectorscopeDock->close();

    m_audioSpectrumDock->close();
    m_spectrogramDock->close();
    m_audiosignalDock->close();

    m_undoViewDock->close();



    /// Tabify Widgets ///
    tabifyDockWidget(m_effectListDock, m_effectStackDock);
    tabifyDockWidget(m_effectListDock, m_transitionConfigDock);
    tabifyDockWidget(m_projectListDock, m_notesDock);

    tabifyDockWidget(m_clipMonitorDock, m_projectMonitorDock);
#ifndef Q_WS_MAC
    tabifyDockWidget(m_clipMonitorDock, m_recMonitorDock);
#endif
    setCentralWidget(m_timelineArea);

    m_fileOpenRecent = KStandardAction::openRecent(this, SLOT(openFile(const KUrl &)), actionCollection());
    readOptions();
    m_fileRevert = KStandardAction::revert(this, SLOT(slotRevert()), actionCollection());
    m_fileRevert->setEnabled(false);

    // Prepare layout actions
    KActionCategory *layoutActions = new KActionCategory(i18n("Layouts"), actionCollection());
    m_loadLayout = new KSelectAction(i18n("Load Layout"), actionCollection());
    for (int i = 1; i < 5; i++) {
        KAction *load = new KAction(KIcon(), i18n("Layout %1", i), this);
        load->setData('_' + QString::number(i));
	layoutActions->addAction("load_layout" + QString::number(i), load);
        m_loadLayout->addAction(load);
        KAction *save = new KAction(KIcon(), i18n("Save As Layout %1", i), this);
        save->setData('_' + QString::number(i));
        layoutActions->addAction("save_layout" + QString::number(i), save);
    }
    // Required to enable user to add the load layout action to toolbar
    layoutActions->addAction("load_layouts", m_loadLayout);
    connect(m_loadLayout, SIGNAL(triggered(QAction*)), this, SLOT(slotLoadLayout(QAction*)));

    KAction *action;
    // Stop motion actions. Beware of the order, we MUST use the same order in stopmotion/stopmotion.cpp
    m_stopmotion_actions = new KActionCategory(i18n("Stop Motion"), actionCollection());
    action = new KAction(KIcon("media-record"), i18n("Capture frame"), this);
    //action->setShortcutContext(Qt::WidgetWithChildrenShortcut);
    m_stopmotion_actions->addAction("stopmotion_capture", action);
    action = new KAction(i18n("Switch live / captured frame"), this);
    //action->setShortcutContext(Qt::WidgetWithChildrenShortcut);
    m_stopmotion_actions->addAction("stopmotion_switch", action);
    action = new KAction(KIcon("edit-paste"), i18n("Show last frame over video"), this);
    action->setCheckable(true);
    action->setChecked(false);
    m_stopmotion_actions->addAction("stopmotion_overlay", action);

    // Build effects menu
    m_effectsMenu = new QMenu(i18n("Add Effect"));
    m_effectActions = new KActionCategory(i18n("Effects"), actionCollection());
    m_effectList->reloadEffectList(m_effectsMenu, m_effectActions);
    m_effectsActionCollection->readSettings();

       // Populate View menu with show / hide actions for dock widgets
    KActionCategory *guiActions = new KActionCategory(i18n("Interface"), actionCollection());

    setupGUI();

    // Find QDockWidget tab bars and show / hide widget title bars on right click
    QList <QTabBar *> tabs = findChildren<QTabBar *>();
    for (int i = 0; i < tabs.count(); i++) {
        tabs.at(i)->setContextMenuPolicy(Qt::CustomContextMenu);
        connect(tabs.at(i), SIGNAL(customContextMenuRequested(const QPoint &)), this, SLOT(slotSwitchTitles()));
    }

    /*ScriptingPart* sp = new ScriptingPart(this, QStringList());
    guiFactory()->addClient(sp);*/
    QMenu *trackMenu = (QMenu*)(factory()->container("track_menu", this));
    if (trackMenu) trackMenu->addActions(m_tracksActionCollection->actions());


    QMenu *saveLayout = (QMenu*)(factory()->container("layout_save_as", this));
    if (saveLayout)
        connect(saveLayout, SIGNAL(triggered(QAction*)), this, SLOT(slotSaveLayout(QAction*)));


    // Load layout names from config file
    loadLayouts();

    loadPlugins();
    loadTranscoders();
    loadClipActions();

    m_projectMonitor->setupMenu(static_cast<QMenu*>(factory()->container("monitor_go", this)), m_playZone, m_loopZone, NULL, m_loopClip);
    m_clipMonitor->setupMenu(static_cast<QMenu*>(factory()->container("monitor_go", this)), m_playZone, m_loopZone, static_cast<QMenu*>(factory()->container("marker_menu", this)));

    QMenu *clipInTimeline = static_cast<QMenu*>(factory()->container("clip_in_timeline", this));
    clipInTimeline->setIcon(KIcon("go-jump"));
	QHash<QString,QMenu*> menus;
	menus.insert("addMenu",static_cast<QMenu*>(factory()->container("generators", this)));
        menus.insert("extractAudioMenu",static_cast<QMenu*>(factory()->container("extract_audio", this)));
	menus.insert("transcodeMenu",static_cast<QMenu*>(factory()->container("transcoders", this)));
	menus.insert("clipActionsMenu",static_cast<QMenu*>(factory()->container("clip_actions", this)));
	menus.insert("inTimelineMenu",clipInTimeline);
    m_projectList->setupGeneratorMenu(menus);

    // build themes menus
    QMenu *themesMenu = static_cast<QMenu*>(factory()->container("themes_menu", this));
    QActionGroup *themegroup = new QActionGroup(this);
    themegroup->setExclusive(true);
    action = new KAction(i18n("Default"), this);
    action->setCheckable(true);
    themegroup->addAction(action);
    if (KdenliveSettings::colortheme().isEmpty()) action->setChecked(true);

    const QStringList schemeFiles = KGlobal::dirs()->findAllResources("data", "color-schemes/*.colors", KStandardDirs::NoDuplicates);

    for (int i = 0; i < schemeFiles.size(); ++i) {
        // get the file name
        const QString filename = schemeFiles.at(i);
        const QFileInfo info(filename);

        // add the entry
        KSharedConfigPtr config = KSharedConfig::openConfig(filename);
        QIcon icon = createSchemePreviewIcon(config);
        KConfigGroup group(config, "General");
        const QString name = group.readEntry("Name", info.baseName());
        action = new KAction(name, this);
        action->setData(filename);
        action->setIcon(icon);
        action->setCheckable(true);
        themegroup->addAction(action);
        if (KdenliveSettings::colortheme() == filename) action->setChecked(true);
    }

    /*KGlobal::dirs()->addResourceDir("themes", KStandardDirs::installPath("data") + QString("kdenlive/themes"));
    QStringList themes = KGlobal::dirs()->findAllResources("themes", QString(), KStandardDirs::Recursive | KStandardDirs::NoDuplicates);
    for (QStringList::const_iterator it = themes.constBegin(); it != themes.constEnd(); ++it)
    {
    QFileInfo fi(*it);
        action = new QAction(fi.fileName(), this);
        action->setData(*it);
    action->setCheckable(true);
    themegroup->addAction(action);
    if (KdenliveSettings::colortheme() == *it) action->setChecked(true);
    }*/
    themesMenu->addActions(themegroup->actions());
    connect(themesMenu, SIGNAL(triggered(QAction *)), this, SLOT(slotChangePalette(QAction*)));

    // Setup and fill effects and transitions menus.


    QMenu *m = static_cast<QMenu*>(factory()->container("video_effects_menu", this));
    m->addActions(m_effectsMenu->actions());


    m_transitionsMenu = new QMenu(i18n("Add Transition"), this);
    for (int i = 0; i < transitions.count(); ++i)
        m_transitionsMenu->addAction(m_transitions[i]);

    connect(m, SIGNAL(triggered(QAction *)), this, SLOT(slotAddVideoEffect(QAction *)));
    connect(m_effectsMenu, SIGNAL(triggered(QAction *)), this, SLOT(slotAddVideoEffect(QAction *)));
    connect(m_transitionsMenu, SIGNAL(triggered(QAction *)), this, SLOT(slotAddTransition(QAction *)));    

    m_timelineContextMenu = new QMenu(this);
    m_timelineContextClipMenu = new QMenu(this);
    m_timelineContextTransitionMenu = new QMenu(this);

    m_timelineContextMenu->addAction(actionCollection()->action("insert_space"));
    m_timelineContextMenu->addAction(actionCollection()->action("delete_space"));
    m_timelineContextMenu->addAction(actionCollection()->action(KStandardAction::name(KStandardAction::Paste)));

    m_timelineContextClipMenu->addAction(actionCollection()->action("clip_in_project_tree"));
    //m_timelineContextClipMenu->addAction(actionCollection()->action("clip_to_project_tree"));
    m_timelineContextClipMenu->addAction(actionCollection()->action("delete_item"));
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

    QMenu *markersMenu = (QMenu*)(factory()->container("marker_menu", this));
    m_timelineContextClipMenu->addMenu(markersMenu);
    m_timelineContextClipMenu->addSeparator();
    m_timelineContextClipMenu->addMenu(m_transitionsMenu);
    m_timelineContextClipMenu->addMenu(m_effectsMenu);

    m_timelineContextTransitionMenu->addAction(actionCollection()->action("delete_item"));
    m_timelineContextTransitionMenu->addAction(actionCollection()->action(KStandardAction::name(KStandardAction::Copy)));

    m_timelineContextTransitionMenu->addAction(actionCollection()->action("auto_transition"));

    connect(m_projectMonitorDock, SIGNAL(visibilityChanged(bool)), m_projectMonitor, SLOT(refreshMonitor(bool)));
    connect(m_clipMonitorDock, SIGNAL(visibilityChanged(bool)), m_clipMonitor, SLOT(refreshMonitor(bool)));
    connect(m_effectList, SIGNAL(addEffect(const QDomElement)), this, SLOT(slotAddEffect(const QDomElement)));
    connect(m_effectList, SIGNAL(reloadEffects()), this, SLOT(slotReloadEffects()));

    slotConnectMonitors();

    // Open or create a file.  Command line argument passed in Url has
    // precedence, then "openlastproject", then just a plain empty file.
    // If opening Url fails, openlastproject will _not_ be used.
    if (!Url.isEmpty()) {
        // delay loading so that the window shows up
        m_startUrl = Url;
        QTimer::singleShot(500, this, SLOT(openFile()));
    } else if (KdenliveSettings::openlastproject()) {
        QTimer::singleShot(500, this, SLOT(openLastFile()));
    } else { //if (m_timelineArea->count() == 0) {
        newFile(false);
    }

    if (!clipsToLoad.isEmpty() && m_activeDocument) {
        QStringList list = clipsToLoad.split(',');
        QList <QUrl> urls;
        foreach(const QString &path, list) {
            kDebug() << QDir::current().absoluteFilePath(path);
            urls << QUrl::fromLocalFile(QDir::current().absoluteFilePath(path));
        }
        m_projectList->slotAddClip(urls);
    }

#ifdef USE_JOGSHUTTLE
    activateShuttleDevice();
#endif
    m_projectListDock->raise();

    actionCollection()->addAssociatedWidget(m_clipMonitor->container());
    actionCollection()->addAssociatedWidget(m_projectMonitor->container());

    QMap <QString, KAction *> viewActions;
    KAction *showTimeline = new KAction(i18n("Timeline"), this);
    showTimeline->setCheckable(true);
    showTimeline->setChecked(true);
    connect(showTimeline, SIGNAL(triggered(bool)), this, SLOT(slotShowTimeline(bool)));
    viewActions.insert(showTimeline->text(), showTimeline);
    
    QList <QDockWidget *> docks = findChildren<QDockWidget *>();
    for (int i = 0; i < docks.count(); i++) {
        QDockWidget* dock = docks.at(i);
	KAction* dockInformations = new KAction(this);
	dockInformations->setText(dock->windowTitle());
	dockInformations->setCheckable(true);
	dockInformations->setChecked(!dock->isHidden());
	connect(dockInformations,SIGNAL(toggled(bool)), dock, SLOT(setVisible(bool)));
	viewActions.insert(dockInformations->text(), dockInformations);
    }


    KMenu *viewMenu = static_cast<KMenu*>(factory()->container("dockwindows", this));
    //const QList<QAction *> viewActions = createPopupMenu()->actions();
    QMap<QString, KAction *>::const_iterator i = viewActions.constBegin();
    while (i != viewActions.constEnd()) {
	viewMenu->addAction(guiActions->addAction(i.key(), i.value()));
	++i;
    }
    
    // Populate encoding profiles
    KConfig conf("encodingprofiles.rc", KConfig::FullConfig, "appdata");
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
    
    connect (KGlobalSettings::self(), SIGNAL(kdisplayPaletteChanged()), this, SLOT(slotChangePalette()));
}

MainWindow::~MainWindow()
{
    if (m_stopmotion) {
        delete m_stopmotion;
    }
    m_effectStack->slotClipItemSelected(NULL);
    m_transitionConfig->slotTransitionItemSelected(NULL, 0, QPoint(), false);

    if (m_projectMonitor) m_projectMonitor->stop();
    if (m_clipMonitor) m_clipMonitor->stop();

    delete m_activeTimeline;
    delete m_effectStack;
    delete m_transitionConfig;
    delete m_activeDocument;
    delete m_projectMonitor;
    delete m_clipMonitor;
    delete m_shortcutRemoveFocus;
    delete[] m_transitions;
    delete m_monitorManager;
    delete m_scopeManager;
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
    if (m_monitorManager) m_monitorManager->stopActiveMonitor();
    // warn the user to save if document is modified and we have clips in our project list
    if (m_activeDocument && m_activeDocument->isModified() &&
            ((m_projectList->documentClipList().isEmpty() && !m_activeDocument->url().isEmpty()) ||
             !m_projectList->documentClipList().isEmpty())) {
        raise();
        activateWindow();
        QString message;
        if (m_activeDocument->url().fileName().isEmpty())
            message = i18n("Save changes to document?");
        else
            message = i18n("The project <b>\"%1\"</b> has been changed.\nDo you want to save your changes?", m_activeDocument->url().fileName());
        switch (KMessageBox::warningYesNoCancel(this, message)) {
        case KMessageBox::Yes :
            // save document here. If saving fails, return false;
            return saveFile();
        case KMessageBox::No :
            // User does not want to save the changes, clear recovery files
            m_activeDocument->m_autosave->resize(0);
            return true;
        default: // cancel
            return false;
        }
    }
    return true;
}

void MainWindow::loadPlugins()
{
    foreach(QObject * plugin, QPluginLoader::staticInstances()) {
        populateMenus(plugin);
    }

    QStringList directories = KGlobal::dirs()->findDirs("module", QString());
    QStringList filters;
    filters << "libkdenlive*";
    foreach(const QString & folder, directories) {
        kDebug() << "Parsing plugin folder: " << folder;
        QDir pluginsDir(folder);
        foreach(const QString & fileName,
                pluginsDir.entryList(filters, QDir::Files)) {
            /*
             * Avoid loading the same plugin twice when there is more than one
             * installation.
             */
            if (!m_pluginFileNames.contains(fileName)) {
                kDebug() << "Found plugin: " << fileName;
                QPluginLoader loader(pluginsDir.absoluteFilePath(fileName));
                QObject *plugin = loader.instance();
                if (plugin) {
                    populateMenus(plugin);
                    m_pluginFileNames += fileName;
                } else
                    kDebug() << "Error loading plugin: " << fileName << ", " << loader.errorString();
            }
        }
    }
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
    kDebug() << "// ADD to MENU" << texts;
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

void MainWindow::aboutPlugins()
{
    //PluginDialog dialog(pluginsDir.path(), m_pluginFileNames, this);
    //dialog.exec();
}


void MainWindow::generateClip()
{
    QAction *action = qobject_cast<QAction *>(sender());
    ClipGenerator *iGenerator = qobject_cast<ClipGenerator *>(action->parent());

    KUrl clipUrl = iGenerator->generatedClip(KdenliveSettings::rendererpath(), action->data().toString(), m_activeDocument->projectFolder(),
                   QStringList(), QStringList(), m_activeDocument->fps(), m_activeDocument->width(), m_activeDocument->height());
    if (!clipUrl.isEmpty()) {
        m_projectList->slotAddClip(QList <QUrl> () << clipUrl);
    }
}

void MainWindow::saveProperties(KConfigGroup &config)
{
    // save properties here,used by session management
    saveFile();
    KMainWindow::saveProperties(config);
}


void MainWindow::readProperties(const KConfigGroup &config)
{
    // read properties here,used by session management
    KMainWindow::readProperties(config);
    QString Lastproject = config.group("Recent Files").readPathEntry("File1", QString());
    openFile(KUrl(Lastproject));
}

void MainWindow::slotReloadEffects()
{
    initEffects::parseCustomEffectsFile();
    m_effectList->reloadEffectList(m_effectsMenu, m_effectActions);
}

#ifdef USE_JOGSHUTTLE
void MainWindow::activateShuttleDevice()
{
    delete m_jogShuttle;
    m_jogShuttle = NULL;
    delete m_jogProcess;
    m_jogProcess = NULL;
    if (KdenliveSettings::enableshuttle() == false) return;

    m_jogProcess = new JogShuttle(KdenliveSettings::shuttledevice());
    m_jogShuttle = new JogShuttleAction(m_jogProcess, JogShuttleConfig::actionMap(KdenliveSettings::shuttlebuttons()));

    connect(m_jogShuttle, SIGNAL(rewindOneFrame()), m_monitorManager, SLOT(slotRewindOneFrame()));
    connect(m_jogShuttle, SIGNAL(forwardOneFrame()), m_monitorManager, SLOT(slotForwardOneFrame()));
    connect(m_jogShuttle, SIGNAL(rewind(double)), m_monitorManager, SLOT(slotRewind(double)));
    connect(m_jogShuttle, SIGNAL(forward(double)), m_monitorManager, SLOT(slotForward(double)));
    connect(m_jogShuttle, SIGNAL(action(const QString&)), this, SLOT(slotDoAction(const QString&)));
}
#endif /* USE_JOGSHUTTLE */

void MainWindow::slotDoAction(const QString& action_name)
{
    QAction* action = actionCollection()->action(action_name);
    if (!action) {
        fprintf(stderr, "%s", QString("shuttle action '%1' unknown\n").arg(action_name).toAscii().constData());
        return;
    }
    action->trigger();
}

void MainWindow::configureNotifications()
{
    KNotifyConfigWidget::configure(this);
}

void MainWindow::slotFullScreen()
{
    KToggleFullScreenAction::setFullScreen(this, actionCollection()->action("fullscreen")->isChecked());
}

void MainWindow::slotAddEffect(const QDomElement effect)
{
    if (!m_activeDocument) return;
    if (effect.isNull()) {
        kDebug() << "--- ERROR, TRYING TO APPEND NULL EFFECT";
        return;
    }
    QDomElement effectToAdd = effect.cloneNode().toElement();
    bool ok;
    int ix = m_effectStack->isTrackMode(&ok);
    if (ok) m_activeTimeline->projectView()->slotAddTrackEffect(effectToAdd, m_activeDocument->tracksCount() - ix);
    else m_activeTimeline->projectView()->slotAddEffect(effectToAdd, GenTime(), -1);
}

void MainWindow::slotUpdateClip(const QString &id)
{
    if (!m_activeDocument) return;
    DocClipBase *clip = m_activeDocument->clipManager()->getClipById(id);
    if (!clip) return;
    if (clip->numReferences() > 0) m_activeTimeline->projectView()->slotUpdateClip(id);
    if (m_clipMonitor->activeClip() && m_clipMonitor->activeClip()->getId() == id) {
        Mlt::Producer *monitorProducer = clip->getCloneProducer();
        m_clipMonitor->updateClipProducer(monitorProducer);
    }
    clip->cleanupProducers();
}

void MainWindow::slotConnectMonitors()
{
    m_projectList->setRenderer(m_projectMonitor->render);
    connect(m_projectList, SIGNAL(pauseMonitor()), m_monitorManager, SLOT(slotPause()));
    connect(m_projectList, SIGNAL(deleteProjectClips(QStringList, QMap<QString, QString>)), this, SLOT(slotDeleteProjectClips(QStringList, QMap<QString, QString>)));
    connect(m_projectList, SIGNAL(showClipProperties(DocClipBase *)), this, SLOT(slotShowClipProperties(DocClipBase *)));
    connect(m_projectList, SIGNAL(showClipProperties(QList <DocClipBase *>, QMap<QString, QString>)), this, SLOT(slotShowClipProperties(QList <DocClipBase *>, QMap<QString, QString>)));
    connect(m_projectMonitor->render, SIGNAL(replyGetImage(const QString &, const QString &, int, int)), m_projectList, SLOT(slotReplyGetImage(const QString &, const QString &, int, int)));
    connect(m_projectMonitor->render, SIGNAL(replyGetImage(const QString &, const QImage &)), m_projectList, SLOT(slotReplyGetImage(const QString &, const QImage &)));

    kDebug()<<"  - - - - - -\n CONNECTED REPLY";
    connect(m_projectMonitor->render, SIGNAL(replyGetFileProperties(const QString &, Mlt::Producer*, const stringMap &, const stringMap &, bool)), m_projectList, SLOT(slotReplyGetFileProperties(const QString &, Mlt::Producer*, const stringMap &, const stringMap &, bool)));

    connect(m_projectMonitor->render, SIGNAL(removeInvalidClip(const QString &, bool)), m_projectList, SLOT(slotRemoveInvalidClip(const QString &, bool)));

    connect(m_projectMonitor->render, SIGNAL(removeInvalidProxy(const QString &, bool)), m_projectList, SLOT(slotRemoveInvalidProxy(const QString &, bool)));

    connect(m_clipMonitor, SIGNAL(refreshClipThumbnail(const QString &, bool)), m_projectList, SLOT(slotRefreshClipThumbnail(const QString &, bool)));

    connect(m_clipMonitor, SIGNAL(adjustMonitorSize()), this, SLOT(slotAdjustClipMonitor()));
    connect(m_projectMonitor, SIGNAL(adjustMonitorSize()), this, SLOT(slotAdjustProjectMonitor()));

    connect(m_projectMonitor, SIGNAL(requestFrameForAnalysis(bool)), this, SLOT(slotMonitorRequestRenderFrame(bool)));

    connect(m_clipMonitor, SIGNAL(saveZone(Render *, QPoint, DocClipBase *)), this, SLOT(slotSaveZone(Render *, QPoint, DocClipBase *)));
    connect(m_projectMonitor, SIGNAL(saveZone(Render *, QPoint, DocClipBase *)), this, SLOT(slotSaveZone(Render *, QPoint, DocClipBase *)));
}

void MainWindow::slotAdjustClipMonitor()
{
    m_clipMonitorDock->updateGeometry();
    m_clipMonitorDock->adjustSize();
    m_clipMonitor->resetSize();
}

void MainWindow::slotAdjustProjectMonitor()
{
    m_projectMonitorDock->updateGeometry();
    m_projectMonitorDock->adjustSize();
    m_projectMonitor->resetSize();
}


class NameGrabbingKActionCollection {
public:
  NameGrabbingKActionCollection(KActionCollection* collection, QStringList& action_names)
    : m_collection(collection), m_action_names(action_names) {
      m_action_names.clear();
    }
  KAction* addAction(const QString& action_name) {
    m_action_names << action_name;
    return m_collection->addAction(action_name);
  }
  void addAction(const QString& action_name, QAction* action) {
    m_action_names << action_name;
    m_collection->addAction(action_name, action);
  }
  operator KActionCollection*() { return m_collection; }
  const QStringList& actionNames() const { return m_action_names; }
private:
  KActionCollection* m_collection;
  QStringList& m_action_names;
};

void MainWindow::setupActions()
{

    NameGrabbingKActionCollection collection(actionCollection(), m_action_names);
    m_timecodeFormat = new KComboBox(this);
    m_timecodeFormat->addItem(i18n("hh:mm:ss:ff"));
    m_timecodeFormat->addItem(i18n("Frames"));
    if (KdenliveSettings::frametimecode()) m_timecodeFormat->setCurrentIndex(1);
    connect(m_timecodeFormat, SIGNAL(activated(int)), this, SLOT(slotUpdateTimecodeFormat(int)));

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
    m_normalEditTool = new KAction(KIcon("kdenlive-normal-edit"), i18n("Normal mode"), this);
    m_normalEditTool->setShortcut(i18nc("Normal editing", "n"));
    toolbar->addAction(m_normalEditTool);
    m_normalEditTool->setCheckable(true);
    m_normalEditTool->setChecked(true);

    m_overwriteEditTool = new KAction(KIcon("kdenlive-overwrite-edit"), i18n("Overwrite mode"), this);
    //m_overwriteEditTool->setShortcut(i18nc("Overwrite mode shortcut", "o"));
    toolbar->addAction(m_overwriteEditTool);
    m_overwriteEditTool->setCheckable(true);
    m_overwriteEditTool->setChecked(false);

    m_insertEditTool = new KAction(KIcon("kdenlive-insert-edit"), i18n("Insert mode"), this);
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
    connect(editGroup, SIGNAL(triggered(QAction *)), this, SLOT(slotChangeEdit(QAction *)));
    //connect(m_overwriteEditTool, SIGNAL(toggled(bool)), this, SLOT(slotSetOverwriteMode(bool)));

    toolbar->addSeparator();

    // create tools buttons
    m_buttonSelectTool = new KAction(KIcon("kdenlive-select-tool"), i18n("Selection tool"), this);
    m_buttonSelectTool->setShortcut(i18nc("Selection tool shortcut", "s"));
    toolbar->addAction(m_buttonSelectTool);
    m_buttonSelectTool->setCheckable(true);
    m_buttonSelectTool->setChecked(true);

    m_buttonRazorTool = new KAction(KIcon("edit-cut"), i18n("Razor tool"), this);
    m_buttonRazorTool->setShortcut(i18nc("Razor tool shortcut", "x"));
    toolbar->addAction(m_buttonRazorTool);
    m_buttonRazorTool->setCheckable(true);
    m_buttonRazorTool->setChecked(false);

    m_buttonSpacerTool = new KAction(KIcon("kdenlive-spacer-tool"), i18n("Spacer tool"), this);
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

    connect(toolGroup, SIGNAL(triggered(QAction *)), this, SLOT(slotChangeTool(QAction *)));

    toolbar->addSeparator();
    m_buttonFitZoom = new KAction(KIcon("zoom-fit-best"), i18n("Fit zoom to project"), this);
    toolbar->addAction(m_buttonFitZoom);
    m_buttonFitZoom->setCheckable(false);

    m_zoomOut = new KAction(KIcon("zoom-out"), i18n("Zoom Out"), this);
    toolbar->addAction(m_zoomOut);
    m_zoomOut->setShortcut(Qt::CTRL + Qt::Key_Minus);

    m_zoomSlider = new QSlider(Qt::Horizontal, this);
    m_zoomSlider->setMaximum(13);
    m_zoomSlider->setPageStep(1);
    m_zoomSlider->setInvertedAppearance(true);

    m_zoomSlider->setMaximumWidth(150);
    m_zoomSlider->setMinimumWidth(100);
    toolbar->addWidget(m_zoomSlider);

    m_zoomIn = new KAction(KIcon("zoom-in"), i18n("Zoom In"), this);
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
    m_buttonAutomaticSplitAudio = new KAction(KIcon("kdenlive-split-audio"), i18n("Split audio and video automatically"), this);
    toolbar->addAction(m_buttonAutomaticSplitAudio);
    m_buttonAutomaticSplitAudio->setCheckable(true);
    m_buttonAutomaticSplitAudio->setChecked(KdenliveSettings::splitaudio());
    connect(m_buttonAutomaticSplitAudio, SIGNAL(triggered()), this, SLOT(slotSwitchSplitAudio()));

    m_buttonVideoThumbs = new KAction(KIcon("kdenlive-show-videothumb"), i18n("Show video thumbnails"), this);
    toolbar->addAction(m_buttonVideoThumbs);
    m_buttonVideoThumbs->setCheckable(true);
    m_buttonVideoThumbs->setChecked(KdenliveSettings::videothumbnails());
    connect(m_buttonVideoThumbs, SIGNAL(triggered()), this, SLOT(slotSwitchVideoThumbs()));

    m_buttonAudioThumbs = new KAction(KIcon("kdenlive-show-audiothumb"), i18n("Show audio thumbnails"), this);
    toolbar->addAction(m_buttonAudioThumbs);
    m_buttonAudioThumbs->setCheckable(true);
    m_buttonAudioThumbs->setChecked(KdenliveSettings::audiothumbnails());
    connect(m_buttonAudioThumbs, SIGNAL(triggered()), this, SLOT(slotSwitchAudioThumbs()));

    m_buttonShowMarkers = new KAction(KIcon("kdenlive-show-markers"), i18n("Show markers comments"), this);
    toolbar->addAction(m_buttonShowMarkers);
    m_buttonShowMarkers->setCheckable(true);
    m_buttonShowMarkers->setChecked(KdenliveSettings::showmarkers());
    connect(m_buttonShowMarkers, SIGNAL(triggered()), this, SLOT(slotSwitchMarkersComments()));

    m_buttonSnap = new KAction(KIcon("kdenlive-snap"), i18n("Snap"), this);
    toolbar->addAction(m_buttonSnap);
    m_buttonSnap->setCheckable(true);
    m_buttonSnap->setChecked(KdenliveSettings::snaptopoints());
    connect(m_buttonSnap, SIGNAL(triggered()), this, SLOT(slotSwitchSnap()));

    m_buttonJackTransport = new KAction(/*KIcon("kdenlive-snap"), */i18n("Enable jack transport"), this);
    toolbar->addAction(m_buttonJackTransport);
    m_buttonJackTransport->setCheckable(true);
    m_buttonJackTransport->setChecked(KdenliveSettings::jacktransport());
    connect(m_buttonJackTransport, SIGNAL(triggered()), this, SLOT(slotSwitchJackTransport()));


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

    actionWidget = toolbar->widgetForAction(m_buttonJackTransport);
    actionWidget->setMaximumWidth(max);
    actionWidget->setMaximumHeight(max - 4);

    m_messageLabel = new StatusBarMessageLabel(this);
    m_messageLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::MinimumExpanding);

    statusBar()->addWidget(m_messageLabel, 10);
    statusBar()->addWidget(m_statusProgressBar, 0);
    statusBar()->addPermanentWidget(toolbar);
    statusBar()->insertPermanentFixedItem("00:00:00:00", ID_TIMELINE_POS);
    statusBar()->addPermanentWidget(m_timecodeFormat);
    //statusBar()->setMaximumHeight(statusBar()->font().pointSize() * 3);

    collection.addAction("normal_mode", m_normalEditTool);
    collection.addAction("overwrite_mode", m_overwriteEditTool);
    collection.addAction("insert_mode", m_insertEditTool);
    collection.addAction("select_tool", m_buttonSelectTool);
    collection.addAction("razor_tool", m_buttonRazorTool);
    collection.addAction("spacer_tool", m_buttonSpacerTool);

    collection.addAction("automatic_split_audio", m_buttonAutomaticSplitAudio);
    collection.addAction("show_video_thumbs", m_buttonVideoThumbs);
    collection.addAction("show_audio_thumbs", m_buttonAudioThumbs);
    collection.addAction("show_markers", m_buttonShowMarkers);
    collection.addAction("snap", m_buttonSnap);
    collection.addAction("jack_transport", m_buttonJackTransport);
    collection.addAction("zoom_fit", m_buttonFitZoom);
    collection.addAction("zoom_in", m_zoomIn);
    collection.addAction("zoom_out", m_zoomOut);

    m_projectSearch = new KAction(KIcon("edit-find"), i18n("Find"), this);
    collection.addAction("project_find", m_projectSearch);
    connect(m_projectSearch, SIGNAL(triggered(bool)), this, SLOT(slotFind()));
    m_projectSearch->setShortcut(Qt::Key_Slash);

    m_projectSearchNext = new KAction(KIcon("go-down-search"), i18n("Find Next"), this);
    collection.addAction("project_find_next", m_projectSearchNext);
    connect(m_projectSearchNext, SIGNAL(triggered(bool)), this, SLOT(slotFindNext()));
    m_projectSearchNext->setShortcut(Qt::Key_F3);
    m_projectSearchNext->setEnabled(false);

    KAction* profilesAction = new KAction(KIcon("document-new"), i18n("Manage Project Profiles"), this);
    collection.addAction("manage_profiles", profilesAction);
    connect(profilesAction, SIGNAL(triggered(bool)), this, SLOT(slotEditProfiles()));

    KNS3::standardAction(i18n("Download New Wipes..."),            this, SLOT(slotGetNewLumaStuff()),       actionCollection(), "get_new_lumas");
    KNS3::standardAction(i18n("Download New Render Profiles..."),  this, SLOT(slotGetNewRenderStuff()),     actionCollection(), "get_new_profiles");
    KNS3::standardAction(i18n("Download New Project Profiles..."), this, SLOT(slotGetNewMltProfileStuff()), actionCollection(), "get_new_mlt_profiles");
    KNS3::standardAction(i18n("Download New Title Templates..."),  this, SLOT(slotGetNewTitleStuff()),      actionCollection(), "get_new_titles");

    KAction* wizAction = new KAction(KIcon("configure"), i18n("Run Config Wizard"), this);
    collection.addAction("run_wizard", wizAction);
    connect(wizAction, SIGNAL(triggered(bool)), this, SLOT(slotRunWizard()));

    KAction* projectAction = new KAction(KIcon("configure"), i18n("Project Settings"), this);
    collection.addAction("project_settings", projectAction);
    connect(projectAction, SIGNAL(triggered(bool)), this, SLOT(slotEditProjectSettings()));

    KAction* backupAction = new KAction(KIcon("edit-undo"), i18n("Open Backup File"), this);
    collection.addAction("open_backup", backupAction);
    connect(backupAction, SIGNAL(triggered(bool)), this, SLOT(slotOpenBackupDialog()));

    KAction* projectRender = new KAction(KIcon("media-record"), i18n("Render"), this);
    collection.addAction("project_render", projectRender);
    projectRender->setShortcut(Qt::CTRL + Qt::Key_Return);
    connect(projectRender, SIGNAL(triggered(bool)), this, SLOT(slotRenderProject()));

    KAction* projectClean = new KAction(KIcon("edit-clear"), i18n("Clean Project"), this);
    collection.addAction("project_clean", projectClean);
    connect(projectClean, SIGNAL(triggered(bool)), this, SLOT(slotCleanProject()));

    KAction* projectAdjust = new KAction(KIcon(), i18n("Adjust Profile to Current Clip"), this);
    collection.addAction("project_adjust_profile", projectAdjust);
    connect(projectAdjust, SIGNAL(triggered(bool)), m_projectList, SLOT(adjustProjectProfileToItem()));

    KAction* monitorPlay = new KAction(KIcon("media-playback-start"), i18n("Play"), this);
    KShortcut playShortcut;
    playShortcut.setPrimary(Qt::Key_Space);
    playShortcut.setAlternate(Qt::Key_K);
    monitorPlay->setShortcut(playShortcut);
    collection.addAction("monitor_play", monitorPlay);
    connect(monitorPlay, SIGNAL(triggered(bool)), m_monitorManager, SLOT(slotPlay()));

    KAction* monitorPause = new KAction(KIcon("media-playback-stop"), i18n("Pause"), this);
    collection.addAction("monitor_pause", monitorPause);
    connect(monitorPause, SIGNAL(triggered(bool)), m_monitorManager, SLOT(slotPause()));

    m_playZone = new KAction(KIcon("media-playback-start"), i18n("Play Zone"), this);
    m_playZone->setShortcut(Qt::CTRL + Qt::Key_Space);
    collection.addAction("monitor_play_zone", m_playZone);
    connect(m_playZone, SIGNAL(triggered(bool)), m_monitorManager, SLOT(slotPlayZone()));

    m_loopZone = new KAction(KIcon("media-playback-start"), i18n("Loop Zone"), this);
    m_loopZone->setShortcut(Qt::ALT + Qt::Key_Space);
    collection.addAction("monitor_loop_zone", m_loopZone);
    connect(m_loopZone, SIGNAL(triggered(bool)), m_monitorManager, SLOT(slotLoopZone()));

    m_loopClip = new KAction(KIcon("media-playback-start"), i18n("Loop selected clip"), this);
    m_loopClip->setEnabled(false);
    collection.addAction("monitor_loop_clip", m_loopClip);
    connect(m_loopClip, SIGNAL(triggered(bool)), m_projectMonitor, SLOT(slotLoopClip()));

    KAction *dvdWizard =  new KAction(KIcon("media-optical"), i18n("DVD Wizard"), this);
    collection.addAction("dvd_wizard", dvdWizard);
    connect(dvdWizard, SIGNAL(triggered(bool)), this, SLOT(slotDvdWizard()));

    KAction *transcodeClip =  new KAction(KIcon("edit-copy"), i18n("Transcode Clips"), this);
    collection.addAction("transcode_clip", transcodeClip);
    connect(transcodeClip, SIGNAL(triggered(bool)), this, SLOT(slotTranscodeClip()));

    KAction *archiveProject =  new KAction(KIcon("file-save"), i18n("Archive Project"), this);
    collection.addAction("archive_project", archiveProject);
    connect(archiveProject, SIGNAL(triggered(bool)), this, SLOT(slotArchiveProject()));


    KAction *markIn = collection.addAction("mark_in");
    markIn->setText(i18n("Set Zone In"));
    markIn->setShortcut(Qt::Key_I);
    connect(markIn, SIGNAL(triggered(bool)), this, SLOT(slotSetInPoint()));

    KAction *markOut = collection.addAction("mark_out");
    markOut->setText(i18n("Set Zone Out"));
    markOut->setShortcut(Qt::Key_O);
    connect(markOut, SIGNAL(triggered(bool)), this, SLOT(slotSetOutPoint()));

    KAction *switchMon = collection.addAction("switch_monitor");
    switchMon->setText(i18n("Switch monitor"));
    switchMon->setShortcut(Qt::Key_T);
    connect(switchMon, SIGNAL(triggered(bool)), this, SLOT(slotSwitchMonitors()));

    KAction *fullMon = collection.addAction("monitor_fullscreen");
    fullMon->setText(i18n("Switch monitor fullscreen"));
    fullMon->setIcon(KIcon("view-fullscreen"));
    connect(fullMon, SIGNAL(triggered(bool)), m_monitorManager, SLOT(slotSwitchFullscreen()));

    KAction *insertTree = collection.addAction("insert_project_tree");
    insertTree->setText(i18n("Insert zone in project tree"));
    insertTree->setShortcut(Qt::CTRL + Qt::Key_I);
    connect(insertTree, SIGNAL(triggered(bool)), this, SLOT(slotInsertZoneToTree()));

    KAction *insertTimeline = collection.addAction("insert_timeline");
    insertTimeline->setText(i18n("Insert zone in timeline"));
    insertTimeline->setShortcut(Qt::SHIFT + Qt::CTRL + Qt::Key_I);
    connect(insertTimeline, SIGNAL(triggered(bool)), this, SLOT(slotInsertZoneToTimeline()));

    KAction *connectJack = collection.addAction("connect_jack");
    connectJack->setText(i18n("Jack connect"));
    connectJack->setShortcut(Qt::SHIFT + Qt::CTRL + Qt::Key_A);
    connect(connectJack, SIGNAL(triggered(bool)), this, SLOT(slotConnectJack()));

    KAction *disconnectJack = collection.addAction("disconnect_jack");
    disconnectJack->setText(i18n("Jack disconnect"));
    disconnectJack->setShortcut(Qt::SHIFT + Qt::CTRL + Qt::Key_D);
    connect(disconnectJack, SIGNAL(triggered(bool)), this, SLOT(slotDisconnectJack()));

    KAction *resizeStart =  new KAction(KIcon(), i18n("Resize Item Start"), this);
    collection.addAction("resize_timeline_clip_start", resizeStart);
    resizeStart->setShortcut(Qt::Key_1);
    connect(resizeStart, SIGNAL(triggered(bool)), this, SLOT(slotResizeItemStart()));

    KAction *resizeEnd =  new KAction(KIcon(), i18n("Resize Item End"), this);
    collection.addAction("resize_timeline_clip_end", resizeEnd);
    resizeEnd->setShortcut(Qt::Key_2);
    connect(resizeEnd, SIGNAL(triggered(bool)), this, SLOT(slotResizeItemEnd()));

    KAction* monitorSeekBackward = new KAction(KIcon("media-seek-backward"), i18n("Rewind"), this);
    monitorSeekBackward->setShortcut(Qt::Key_J);
    collection.addAction("monitor_seek_backward", monitorSeekBackward);
    connect(monitorSeekBackward, SIGNAL(triggered(bool)), m_monitorManager, SLOT(slotRewind()));

    KAction* monitorSeekBackwardOneFrame = new KAction(KIcon("media-skip-backward"), i18n("Rewind 1 Frame"), this);
    monitorSeekBackwardOneFrame->setShortcut(Qt::Key_Left);
    collection.addAction("monitor_seek_backward-one-frame", monitorSeekBackwardOneFrame);
    connect(monitorSeekBackwardOneFrame, SIGNAL(triggered(bool)), m_monitorManager, SLOT(slotRewindOneFrame()));

    KAction* monitorSeekBackwardOneSecond = new KAction(KIcon("media-skip-backward"), i18n("Rewind 1 Second"), this);
    monitorSeekBackwardOneSecond->setShortcut(Qt::SHIFT + Qt::Key_Left);
    collection.addAction("monitor_seek_backward-one-second", monitorSeekBackwardOneSecond);
    connect(monitorSeekBackwardOneSecond, SIGNAL(triggered(bool)), m_monitorManager, SLOT(slotRewindOneSecond()));

    KAction* monitorSeekSnapBackward = new KAction(KIcon("media-seek-backward"), i18n("Go to Previous Snap Point"), this);
    monitorSeekSnapBackward->setShortcut(Qt::ALT + Qt::Key_Left);
    collection.addAction("monitor_seek_snap_backward", monitorSeekSnapBackward);
    connect(monitorSeekSnapBackward, SIGNAL(triggered(bool)), this, SLOT(slotSnapRewind()));

    KAction* monitorSeekForward = new KAction(KIcon("media-seek-forward"), i18n("Forward"), this);
    monitorSeekForward->setShortcut(Qt::Key_L);
    collection.addAction("monitor_seek_forward", monitorSeekForward);
    connect(monitorSeekForward, SIGNAL(triggered(bool)), m_monitorManager, SLOT(slotForward()));

    KAction* clipStart = new KAction(KIcon("media-seek-backward"), i18n("Go to Clip Start"), this);
    clipStart->setShortcut(Qt::Key_Home);
    collection.addAction("seek_clip_start", clipStart);
    connect(clipStart, SIGNAL(triggered(bool)), this, SLOT(slotClipStart()));

    KAction* clipEnd = new KAction(KIcon("media-seek-forward"), i18n("Go to Clip End"), this);
    clipEnd->setShortcut(Qt::Key_End);
    collection.addAction("seek_clip_end", clipEnd);
    connect(clipEnd, SIGNAL(triggered(bool)), this, SLOT(slotClipEnd()));

    KAction* zoneStart = new KAction(KIcon("media-seek-backward"), i18n("Go to Zone Start"), this);
    zoneStart->setShortcut(Qt::SHIFT + Qt::Key_I);
    collection.addAction("seek_zone_start", zoneStart);
    connect(zoneStart, SIGNAL(triggered(bool)), this, SLOT(slotZoneStart()));

    KAction* zoneEnd = new KAction(KIcon("media-seek-forward"), i18n("Go to Zone End"), this);
    zoneEnd->setShortcut(Qt::SHIFT + Qt::Key_O);
    collection.addAction("seek_zone_end", zoneEnd);
    connect(zoneEnd, SIGNAL(triggered(bool)), this, SLOT(slotZoneEnd()));

    KAction* projectStart = new KAction(KIcon("go-first"), i18n("Go to Project Start"), this);
    projectStart->setShortcut(Qt::CTRL + Qt::Key_Home);
    collection.addAction("seek_start", projectStart);
    connect(projectStart, SIGNAL(triggered(bool)), m_monitorManager, SLOT(slotStart()));

    KAction* projectEnd = new KAction(KIcon("go-last"), i18n("Go to Project End"), this);
    projectEnd->setShortcut(Qt::CTRL + Qt::Key_End);
    collection.addAction("seek_end", projectEnd);
    connect(projectEnd, SIGNAL(triggered(bool)), m_monitorManager, SLOT(slotEnd()));

    KAction* monitorSeekForwardOneFrame = new KAction(KIcon("media-skip-forward"), i18n("Forward 1 Frame"), this);
    monitorSeekForwardOneFrame->setShortcut(Qt::Key_Right);
    collection.addAction("monitor_seek_forward-one-frame", monitorSeekForwardOneFrame);
    connect(monitorSeekForwardOneFrame, SIGNAL(triggered(bool)), m_monitorManager, SLOT(slotForwardOneFrame()));

    KAction* monitorSeekForwardOneSecond = new KAction(KIcon("media-skip-forward"), i18n("Forward 1 Second"), this);
    monitorSeekForwardOneSecond->setShortcut(Qt::SHIFT + Qt::Key_Right);
    collection.addAction("monitor_seek_forward-one-second", monitorSeekForwardOneSecond);
    connect(monitorSeekForwardOneSecond, SIGNAL(triggered(bool)), m_monitorManager, SLOT(slotForwardOneSecond()));

    KAction* monitorSeekSnapForward = new KAction(KIcon("media-seek-forward"), i18n("Go to Next Snap Point"), this);
    monitorSeekSnapForward->setShortcut(Qt::ALT + Qt::Key_Right);
    collection.addAction("monitor_seek_snap_forward", monitorSeekSnapForward);
    connect(monitorSeekSnapForward, SIGNAL(triggered(bool)), this, SLOT(slotSnapForward()));

    KAction* deleteItem = new KAction(KIcon("edit-delete"), i18n("Delete Selected Item"), this);
    deleteItem->setShortcut(Qt::Key_Delete);
    collection.addAction("delete_timeline_clip", deleteItem);
    connect(deleteItem, SIGNAL(triggered(bool)), this, SLOT(slotDeleteItem()));

    /*KAction* editTimelineClipSpeed = new KAction(i18n("Change Clip Speed"), this);
    collection.addAction("change_clip_speed", editTimelineClipSpeed);
    editTimelineClipSpeed->setData("change_speed");
    connect(editTimelineClipSpeed, SIGNAL(triggered(bool)), this, SLOT(slotChangeClipSpeed()));*/

    KAction *stickTransition = collection.addAction("auto_transition");
    stickTransition->setData(QString("auto"));
    stickTransition->setCheckable(true);
    stickTransition->setEnabled(false);
    stickTransition->setText(i18n("Automatic Transition"));
    connect(stickTransition, SIGNAL(triggered(bool)), this, SLOT(slotAutoTransition()));

    KAction* groupClip = new KAction(KIcon("object-group"), i18n("Group Clips"), this);
    groupClip->setShortcut(Qt::CTRL + Qt::Key_G);
    collection.addAction("group_clip", groupClip);
    connect(groupClip, SIGNAL(triggered(bool)), this, SLOT(slotGroupClips()));

    KAction* ungroupClip = new KAction(KIcon("object-ungroup"), i18n("Ungroup Clips"), this);
    collection.addAction("ungroup_clip", ungroupClip);
    ungroupClip->setShortcut(Qt::CTRL + Qt::SHIFT + Qt::Key_G);
    ungroupClip->setData("ungroup_clip");
    connect(ungroupClip, SIGNAL(triggered(bool)), this, SLOT(slotUnGroupClips()));

    KAction* editItemDuration = new KAction(KIcon("measure"), i18n("Edit Duration"), this);
    collection.addAction("edit_item_duration", editItemDuration);
    connect(editItemDuration, SIGNAL(triggered(bool)), this, SLOT(slotEditItemDuration()));
    
    KAction* saveTimelineClip = new KAction(KIcon("document-save"), i18n("Save clip"), this);
    collection.addAction("save_timeline_clip", saveTimelineClip);
    connect(saveTimelineClip, SIGNAL(triggered(bool)), this, SLOT(slotSaveTimelineClip()));

    KAction* clipInProjectTree = new KAction(KIcon("go-jump-definition"), i18n("Clip in Project Tree"), this);
    collection.addAction("clip_in_project_tree", clipInProjectTree);
    connect(clipInProjectTree, SIGNAL(triggered(bool)), this, SLOT(slotClipInProjectTree()));

    /*KAction* clipToProjectTree = new KAction(KIcon("go-jump-definition"), i18n("Add Clip to Project Tree"), this);
    collection.addAction("clip_to_project_tree", clipToProjectTree);
    connect(clipToProjectTree, SIGNAL(triggered(bool)), this, SLOT(slotClipToProjectTree()));*/

    KAction* insertOvertwrite = new KAction(KIcon(), i18n("Insert Clip Zone in Timeline (Overwrite)"), this);
    insertOvertwrite->setShortcut(Qt::Key_V);
    collection.addAction("overwrite_to_in_point", insertOvertwrite);
    connect(insertOvertwrite, SIGNAL(triggered(bool)), this, SLOT(slotInsertClipOverwrite()));

    KAction* selectTimelineClip = new KAction(KIcon("edit-select"), i18n("Select Clip"), this);
    selectTimelineClip->setShortcut(Qt::Key_Plus);
    collection.addAction("select_timeline_clip", selectTimelineClip);
    connect(selectTimelineClip, SIGNAL(triggered(bool)), this, SLOT(slotSelectTimelineClip()));

    KAction* deselectTimelineClip = new KAction(KIcon("edit-select"), i18n("Deselect Clip"), this);
    deselectTimelineClip->setShortcut(Qt::Key_Minus);
    collection.addAction("deselect_timeline_clip", deselectTimelineClip);
    connect(deselectTimelineClip, SIGNAL(triggered(bool)), this, SLOT(slotDeselectTimelineClip()));

    KAction* selectAddTimelineClip = new KAction(KIcon("edit-select"), i18n("Add Clip To Selection"), this);
    selectAddTimelineClip->setShortcut(Qt::ALT + Qt::Key_Plus);
    collection.addAction("select_add_timeline_clip", selectAddTimelineClip);
    connect(selectAddTimelineClip, SIGNAL(triggered(bool)), this, SLOT(slotSelectAddTimelineClip()));

    KAction* selectTimelineTransition = new KAction(KIcon("edit-select"), i18n("Select Transition"), this);
    selectTimelineTransition->setShortcut(Qt::SHIFT + Qt::Key_Plus);
    collection.addAction("select_timeline_transition", selectTimelineTransition);
    connect(selectTimelineTransition, SIGNAL(triggered(bool)), this, SLOT(slotSelectTimelineTransition()));

    KAction* deselectTimelineTransition = new KAction(KIcon("edit-select"), i18n("Deselect Transition"), this);
    deselectTimelineTransition->setShortcut(Qt::SHIFT + Qt::Key_Minus);
    collection.addAction("deselect_timeline_transition", deselectTimelineTransition);
    connect(deselectTimelineTransition, SIGNAL(triggered(bool)), this, SLOT(slotDeselectTimelineTransition()));

    KAction* selectAddTimelineTransition = new KAction(KIcon("edit-select"), i18n("Add Transition To Selection"), this);
    selectAddTimelineTransition->setShortcut(Qt::ALT + Qt::SHIFT + Qt::Key_Plus);
    collection.addAction("select_add_timeline_transition", selectAddTimelineTransition);
    connect(selectAddTimelineTransition, SIGNAL(triggered(bool)), this, SLOT(slotSelectAddTimelineTransition()));

    KAction* cutTimelineClip = new KAction(KIcon("edit-cut"), i18n("Cut Clip"), this);
    cutTimelineClip->setShortcut(Qt::SHIFT + Qt::Key_R);
    collection.addAction("cut_timeline_clip", cutTimelineClip);
    connect(cutTimelineClip, SIGNAL(triggered(bool)), this, SLOT(slotCutTimelineClip()));

    KAction* addClipMarker = new KAction(KIcon("bookmark-new"), i18n("Add Marker"), this);
    collection.addAction("add_clip_marker", addClipMarker);
    connect(addClipMarker, SIGNAL(triggered(bool)), this, SLOT(slotAddClipMarker()));

    KAction* deleteClipMarker = new KAction(KIcon("edit-delete"), i18n("Delete Marker"), this);
    collection.addAction("delete_clip_marker", deleteClipMarker);
    connect(deleteClipMarker, SIGNAL(triggered(bool)), this, SLOT(slotDeleteClipMarker()));

    KAction* deleteAllClipMarkers = new KAction(KIcon("edit-delete"), i18n("Delete All Markers"), this);
    collection.addAction("delete_all_clip_markers", deleteAllClipMarkers);
    connect(deleteAllClipMarkers, SIGNAL(triggered(bool)), this, SLOT(slotDeleteAllClipMarkers()));

    KAction* editClipMarker = new KAction(KIcon("document-properties"), i18n("Edit Marker"), this);
    editClipMarker->setData(QString("edit_marker"));
    collection.addAction("edit_clip_marker", editClipMarker);
    connect(editClipMarker, SIGNAL(triggered(bool)), this, SLOT(slotEditClipMarker()));

    KAction* addMarkerGuideQuickly = new KAction(KIcon("bookmark-new"), i18n("Add Marker/Guide quickly"), this);
    addMarkerGuideQuickly->setShortcut(Qt::Key_Asterisk);
    collection.addAction("add_marker_guide_quickly", addMarkerGuideQuickly);
    connect(addMarkerGuideQuickly, SIGNAL(triggered(bool)), this, SLOT(slotAddMarkerGuideQuickly()));

    KAction* splitAudio = new KAction(KIcon("document-new"), i18n("Split Audio"), this);
    collection.addAction("split_audio", splitAudio);
    connect(splitAudio, SIGNAL(triggered(bool)), this, SLOT(slotSplitAudio()));

    KAction* setAudioAlignReference = new KAction(i18n("Set Audio Reference"), this);
    collection.addAction("set_audio_align_ref", setAudioAlignReference);
    connect(setAudioAlignReference, SIGNAL(triggered()), this, SLOT(slotSetAudioAlignReference()));

    KAction* alignAudio = new KAction(i18n("Align Audio to Reference"), this);
    collection.addAction("align_audio", alignAudio);
    connect(alignAudio, SIGNAL(triggered()), this, SLOT(slotAlignAudio()));

    KAction* audioOnly = new KAction(KIcon("document-new"), i18n("Audio Only"), this);
    collection.addAction("clip_audio_only", audioOnly);
    audioOnly->setData("clip_audio_only");
    audioOnly->setCheckable(true);

    KAction* videoOnly = new KAction(KIcon("document-new"), i18n("Video Only"), this);
    collection.addAction("clip_video_only", videoOnly);
    videoOnly->setData("clip_video_only");
    videoOnly->setCheckable(true);

    KAction* audioAndVideo = new KAction(KIcon("document-new"), i18n("Audio and Video"), this);
    collection.addAction("clip_audio_and_video", audioAndVideo);
    audioAndVideo->setData("clip_audio_and_video");
    audioAndVideo->setCheckable(true);

    m_clipTypeGroup = new QActionGroup(this);
    m_clipTypeGroup->addAction(audioOnly);
    m_clipTypeGroup->addAction(videoOnly);
    m_clipTypeGroup->addAction(audioAndVideo);
    connect(m_clipTypeGroup, SIGNAL(triggered(QAction *)), this, SLOT(slotUpdateClipType(QAction *)));
    m_clipTypeGroup->setEnabled(false);

    KAction *insertSpace = new KAction(KIcon(), i18n("Insert Space"), this);
    collection.addAction("insert_space", insertSpace);
    connect(insertSpace, SIGNAL(triggered()), this, SLOT(slotInsertSpace()));

    KAction *removeSpace = new KAction(KIcon(), i18n("Remove Space"), this);
    collection.addAction("delete_space", removeSpace);
    connect(removeSpace, SIGNAL(triggered()), this, SLOT(slotRemoveSpace()));

    m_tracksActionCollection = new KActionCollection(this, KGlobal::mainComponent());
    m_tracksActionCollection->addAssociatedWidget(m_timelineArea);

    KAction *insertTrack = new KAction(KIcon(), i18n("Insert Track"), m_tracksActionCollection);
    m_tracksActionCollection->addAction("insert_track", insertTrack);
    connect(insertTrack, SIGNAL(triggered()), this, SLOT(slotInsertTrack()));

    KAction *deleteTrack = new KAction(KIcon(), i18n("Delete Track"), m_tracksActionCollection);
    m_tracksActionCollection->addAction("delete_track", deleteTrack);
    connect(deleteTrack, SIGNAL(triggered()), this, SLOT(slotDeleteTrack()));

    KAction *configTracks = new KAction(KIcon("configure"), i18n("Configure Tracks"), m_tracksActionCollection);
    m_tracksActionCollection->addAction("config_tracks", configTracks);
    connect(configTracks, SIGNAL(triggered()), this, SLOT(slotConfigTrack()));

    KAction *selectTrack = new KAction(KIcon(), i18n("Select All in Current Track"), m_tracksActionCollection);
    connect(selectTrack, SIGNAL(triggered()), this, SLOT(slotSelectTrack()));
    m_tracksActionCollection->addAction("select_track", selectTrack);

    QAction *selectAll = KStandardAction::selectAll(this, SLOT(slotSelectAllTracks()), m_tracksActionCollection);
    selectAll->setShortcutContext(Qt::WidgetWithChildrenShortcut);
    m_tracksActionCollection->addAction("select_all_tracks", selectAll);

    KAction *addGuide = new KAction(KIcon("document-new"), i18n("Add Guide"), this);
    collection.addAction("add_guide", addGuide);
    connect(addGuide, SIGNAL(triggered()), this, SLOT(slotAddGuide()));

    QAction *delGuide = new KAction(KIcon("edit-delete"), i18n("Delete Guide"), this);
    collection.addAction("delete_guide", delGuide);
    connect(delGuide, SIGNAL(triggered()), this, SLOT(slotDeleteGuide()));

    QAction *editGuide = new KAction(KIcon("document-properties"), i18n("Edit Guide"), this);
    collection.addAction("edit_guide", editGuide);
    connect(editGuide, SIGNAL(triggered()), this, SLOT(slotEditGuide()));

    QAction *delAllGuides = new KAction(KIcon("edit-delete"), i18n("Delete All Guides"), this);
    collection.addAction("delete_all_guides", delAllGuides);
    connect(delAllGuides, SIGNAL(triggered()), this, SLOT(slotDeleteAllGuides()));

    QAction *pasteEffects = new KAction(KIcon("edit-paste"), i18n("Paste Effects"), this);
    collection.addAction("paste_effects", pasteEffects);
    pasteEffects->setData("paste_effects");
    connect(pasteEffects , SIGNAL(triggered()), this, SLOT(slotPasteEffects()));

    QAction *showTitleBar = new KAction(i18n("Show Title Bars"), this);
    collection.addAction("show_titlebars", showTitleBar);
    showTitleBar->setCheckable(true);
    connect(showTitleBar, SIGNAL(triggered(bool)), this, SLOT(slotShowTitleBars(bool)));
    showTitleBar->setChecked(KdenliveSettings::showtitlebars());
    slotShowTitleBars(KdenliveSettings::showtitlebars());

    m_closeAction = KStandardAction::close(this,  SLOT(closeCurrentDocument()),   collection);
    KStandardAction::quit(this,                   SLOT(close()),                  collection);
    KStandardAction::open(this,                   SLOT(openFile()),               collection);
    m_saveAction = KStandardAction::save(this,    SLOT(saveFile()),               collection);
    KStandardAction::saveAs(this,                 SLOT(saveFileAs()),             collection);
    KStandardAction::openNew(this,                SLOT(newFile()),                collection);
    // TODO: make the following connection to slotEditKeys work
    //KStandardAction::keyBindings(this,            SLOT(slotEditKeys()),           collection);
    KStandardAction::preferences(this,            SLOT(slotPreferences()),        collection);
    KStandardAction::configureNotifications(this, SLOT(configureNotifications()), collection);
    KStandardAction::copy(this,                   SLOT(slotCopy()),               collection);
    KStandardAction::paste(this,                  SLOT(slotPaste()),              collection);
    KStandardAction::fullScreen(this,             SLOT(slotFullScreen()), this,   collection);

    KAction *undo = KStandardAction::undo(m_commandStack, SLOT(undo()), collection);
    undo->setEnabled(false);
    connect(m_commandStack, SIGNAL(canUndoChanged(bool)), undo, SLOT(setEnabled(bool)));

    KAction *redo = KStandardAction::redo(m_commandStack, SLOT(redo()), collection);
    redo->setEnabled(false);
    connect(m_commandStack, SIGNAL(canRedoChanged(bool)), redo, SLOT(setEnabled(bool)));

    /*
    //TODO: Add status tooltip to actions ?
    connect(collection, SIGNAL(actionHovered(QAction*)),
            this, SLOT(slotDisplayActionMessage(QAction*)));*/


    QAction *addClip = new KAction(KIcon("kdenlive-add-clip"), i18n("Add Clip"), this);
    collection.addAction("add_clip", addClip);
    connect(addClip , SIGNAL(triggered()), m_projectList, SLOT(slotAddClip()));

    QAction *addColorClip = new KAction(KIcon("kdenlive-add-color-clip"), i18n("Add Color Clip"), this);
    collection.addAction("add_color_clip", addColorClip);
    connect(addColorClip , SIGNAL(triggered()), m_projectList, SLOT(slotAddColorClip()));

    QAction *addSlideClip = new KAction(KIcon("kdenlive-add-slide-clip"), i18n("Add Slideshow Clip"), this);
    collection.addAction("add_slide_clip", addSlideClip);
    connect(addSlideClip , SIGNAL(triggered()), m_projectList, SLOT(slotAddSlideshowClip()));

    QAction *addTitleClip = new KAction(KIcon("kdenlive-add-text-clip"), i18n("Add Title Clip"), this);
    collection.addAction("add_text_clip", addTitleClip);
    connect(addTitleClip , SIGNAL(triggered()), m_projectList, SLOT(slotAddTitleClip()));

    QAction *addTitleTemplateClip = new KAction(KIcon("kdenlive-add-text-clip"), i18n("Add Template Title"), this);
    collection.addAction("add_text_template_clip", addTitleTemplateClip);
    connect(addTitleTemplateClip , SIGNAL(triggered()), m_projectList, SLOT(slotAddTitleTemplateClip()));

    QAction *addFolderButton = new KAction(KIcon("folder-new"), i18n("Create Folder"), this);
    collection.addAction("add_folder", addFolderButton);
    connect(addFolderButton , SIGNAL(triggered()), m_projectList, SLOT(slotAddFolder()));

    QAction *downloadResources = new KAction(KIcon("download"), i18n("Online Resources"), this);
    collection.addAction("download_resource", downloadResources);
    connect(downloadResources , SIGNAL(triggered()), this, SLOT(slotDownloadResources()));

    QAction *clipProperties = new KAction(KIcon("document-edit"), i18n("Clip Properties"), this);
    collection.addAction("clip_properties", clipProperties);
    clipProperties->setData("clip_properties");
    connect(clipProperties , SIGNAL(triggered()), m_projectList, SLOT(slotEditClip()));
    clipProperties->setEnabled(false);

    QAction *openClip = new KAction(KIcon("document-open"), i18n("Edit Clip"), this);
    collection.addAction("edit_clip", openClip);
    openClip->setData("edit_clip");
    connect(openClip , SIGNAL(triggered()), m_projectList, SLOT(slotOpenClip()));
    openClip->setEnabled(false);

    QAction *deleteClip = new KAction(KIcon("edit-delete"), i18n("Delete Clip"), this);
    collection.addAction("delete_clip", deleteClip);
    deleteClip->setData("delete_clip");
    connect(deleteClip , SIGNAL(triggered()), m_projectList, SLOT(slotRemoveClip()));
    deleteClip->setEnabled(false);

    QAction *reloadClip = new KAction(KIcon("view-refresh"), i18n("Reload Clip"), this);
    collection.addAction("reload_clip", reloadClip);
    reloadClip->setData("reload_clip");
    connect(reloadClip , SIGNAL(triggered()), m_projectList, SLOT(slotReloadClip()));
    reloadClip->setEnabled(false);

    QAction *proxyClip = new KAction(i18n("Proxy Clip"), this);
    collection.addAction("proxy_clip", proxyClip);
    proxyClip->setData("proxy_clip");
    proxyClip->setCheckable(true);
    proxyClip->setChecked(false);
    connect(proxyClip, SIGNAL(toggled(bool)), m_projectList, SLOT(slotProxyCurrentItem(bool)));

    QAction *stopMotion = new KAction(KIcon("image-x-generic"), i18n("Stop Motion Capture"), this);
    collection.addAction("stopmotion", stopMotion);
    connect(stopMotion , SIGNAL(triggered()), this, SLOT(slotOpenStopmotion()));

    QMenu *addClips = new QMenu();
    addClips->addAction(addClip);
    addClips->addAction(addColorClip);
    addClips->addAction(addSlideClip);
    addClips->addAction(addTitleClip);
    addClips->addAction(addTitleTemplateClip);
    addClips->addAction(addFolderButton);
    addClips->addAction(downloadResources);

    addClips->addAction(reloadClip);
    addClips->addAction(proxyClip);
    addClips->addAction(clipProperties);
    addClips->addAction(openClip);
    addClips->addAction(deleteClip);
    m_projectList->setupMenu(addClips, addClip);

    // Setup effects and transitions actions.
    m_effectsActionCollection = new KActionCollection(this, KGlobal::mainComponent());
    //KActionCategory *transitionActions = new KActionCategory(i18n("Transitions"), m_effectsActionCollection);
    KActionCategory *transitionActions = new KActionCategory(i18n("Transitions"), collection);
    m_transitions = new KAction*[transitions.count()];
    for (int i = 0; i < transitions.count(); i++) {
        QStringList effectInfo = transitions.effectIdInfo(i);
        m_transitions[i] = new KAction(effectInfo.at(0), this);
        m_transitions[i]->setData(effectInfo);
        m_transitions[i]->setIconVisibleInMenu(false);
        QString id = effectInfo.at(2);
        if (id.isEmpty()) id = effectInfo.at(1);
        transitionActions->addAction("transition_" + id, m_transitions[i]);
    }
    //m_effectsActionCollection->readSettings();

}

void MainWindow::slotDisplayActionMessage(QAction *a)
{
    statusBar()->showMessage(a->data().toString(), 3000);
}

void MainWindow::setStatusBarStyleSheet(const QPalette &p)
{
    KColorScheme scheme(p.currentColorGroup(), KColorScheme::Window, KSharedConfig::openConfig(KdenliveSettings::colortheme()));
    QColor buttonBg = scheme.background(KColorScheme::LinkBackground).color();
    QColor buttonBord = scheme.foreground(KColorScheme::LinkText).color();
    QColor buttonBord2 = scheme.shade(KColorScheme::LightShade);
    statusBar()->setStyleSheet(QString("QStatusBar QLabel {font-size:%1pt;} QStatusBar::item { border: 0px; font-size:%1pt;padding:0px; }").arg(statusBar()->font().pointSize()));
    QString style1 = QString("QToolBar { border: 0px } QToolButton { border-style: inset; border:1px solid transparent;border-radius: 3px;margin: 0px 3px;padding: 0px;} QToolButton:hover { background: %3;border-style: inset; border:1px solid %3;border-radius: 3px;} QToolButton:checked { background-color: %1; border-style: inset; border:1px solid %2;border-radius: 3px;}").arg(buttonBg.name()).arg(buttonBord.name()).arg(buttonBord2.name());
    statusBar()->setStyleSheet(style1);
}

void MainWindow::loadLayouts()
{
    QMenu *saveLayout = (QMenu*)(factory()->container("layout_save_as", this));
    if (m_loadLayout == NULL || saveLayout == NULL) return;
    KSharedConfigPtr config = KGlobal::config();
    KConfigGroup layoutGroup(config, "Layouts");
    QStringList entries = layoutGroup.keyList();
    QList<QAction *> loadActions = m_loadLayout->actions();
    QList<QAction *> saveActions = saveLayout->actions();
    for (int i = 1; i < 5; i++) {
        // Rename the layouts actions
        foreach(const QString & key, entries) {
            if (key.endsWith(QString("_%1").arg(i))) {
                // Found previously saved layout
                QString layoutName = key.section('_', 0, -2);
                for (int j = 0; j < loadActions.count(); j++) {
                    if (loadActions.at(j)->data().toString().endsWith('_' + QString::number(i))) {
                        loadActions[j]->setText(layoutName);
                        loadActions[j]->setData(key);
                        break;
                    }
                }
                for (int j = 0; j < saveActions.count(); j++) {
                    if (saveActions.at(j)->data().toString().endsWith('_' + QString::number(i))) {
                        saveActions[j]->setText(i18n("Save as %1", layoutName));
                        saveActions[j]->setData(key);
                        break;
                    }
                }
            }
        }
    }
}

void MainWindow::slotLoadLayout(QAction *action)
{
    if (!action) return;
    QString layoutId = action->data().toString();
    if (layoutId.isEmpty()) return;
    KSharedConfigPtr config = KGlobal::config();
    KConfigGroup layouts(config, "Layouts");
    QByteArray state = QByteArray::fromBase64(layouts.readEntry(layoutId).toAscii());
    restoreState(state);
}

void MainWindow::slotSaveLayout(QAction *action)
{
    QString originallayoutName = action->data().toString();
    int layoutId = originallayoutName.section('_', -1).toInt();

    QString layoutName = QInputDialog::getText(this, i18n("Save Layout"), i18n("Layout name:"), QLineEdit::Normal, originallayoutName.section('_', 0, -2));
    if (layoutName.isEmpty()) return;
    KSharedConfigPtr config = KGlobal::config();
    KConfigGroup layouts(config, "Layouts");
    layouts.deleteEntry(originallayoutName);

    QByteArray st = saveState();
    layoutName.append('_' + QString::number(layoutId));
    layouts.writeEntry(layoutName, st.toBase64());
    loadLayouts();
}

void MainWindow::saveOptions()
{
    KdenliveSettings::self()->writeConfig();
    KSharedConfigPtr config = KGlobal::config();
    m_fileOpenRecent->saveEntries(KConfigGroup(config, "Recent Files"));
    KConfigGroup treecolumns(config, "Project Tree");
    treecolumns.writeEntry("columns", m_projectList->headerInfo());
    config->sync();
}

void MainWindow::readOptions()
{
    KSharedConfigPtr config = KGlobal::config();
    m_fileOpenRecent->loadEntries(KConfigGroup(config, "Recent Files"));
    KConfigGroup initialGroup(config, "version");
    bool upgrade = false;
    if (initialGroup.exists()) {
        if (initialGroup.readEntry("version", QString()).section(' ', 0, 0) != QString(version).section(' ', 0, 0)) {
            upgrade = true;
        }

        if (initialGroup.readEntry("version") == "0.7") {
            //Add new settings from 0.7.1
            if (KdenliveSettings::defaultprojectfolder().isEmpty()) {
                QString path = QDir::homePath() + "/kdenlive";
                if (KStandardDirs::makeDir(path)  == false) {
                    kDebug() << "/// ERROR CREATING PROJECT FOLDER: " << path;
                } else KdenliveSettings::setDefaultprojectfolder(path);
            }
        }

    }
    if (KdenliveSettings::ffmpegpath().isEmpty() || KdenliveSettings::ffplaypath().isEmpty()) upgrade = true;
    if (!initialGroup.exists() || upgrade) {
        // this is our first run, show Wizard
        QPointer<Wizard> w = new Wizard(upgrade, this);
        if (w->exec() == QDialog::Accepted && w->isOk()) {
            w->adjustSettings();
            initialGroup.writeEntry("version", version);
            delete w;
        } else {
            delete w;
            ::exit(1);
        }
    }
    KConfigGroup treecolumns(config, "Project Tree");
    const QByteArray state = treecolumns.readEntry("columns", QByteArray());
    if (!state.isEmpty())
        m_projectList->setHeaderInfo(state);
}

void MainWindow::slotRunWizard()
{
    QPointer<Wizard> w = new Wizard(false, this);
    if (w->exec() == QDialog::Accepted && w->isOk()) {
        w->adjustSettings();
    }
    delete w;
}

void MainWindow::newFile(bool showProjectSettings, bool force)
{
    if (!m_timelineArea->isEnabled() && !force)
        return;
    m_fileRevert->setEnabled(false);
    QString profileName = KdenliveSettings::default_profile();
    KUrl projectFolder = KdenliveSettings::defaultprojectfolder();
    QMap <QString, QString> documentProperties;
    QMap <QString, QString> documentMetadata;
    QPoint projectTracks(KdenliveSettings::videotracks(), KdenliveSettings::audiotracks());
    if (!showProjectSettings) {
        if (!KdenliveSettings::activatetabs())
            if (!closeCurrentDocument())
                return;
    } else {
        QPointer<ProjectSettings> w = new ProjectSettings(NULL, QMap <QString, QString> (), QStringList(), projectTracks.x(), projectTracks.y(), KdenliveSettings::defaultprojectfolder(), false, true, this);
        if (w->exec() != QDialog::Accepted) {
            delete w;
            return;
        }
        if (!KdenliveSettings::activatetabs())
            if (!closeCurrentDocument()) {
                delete w;
                return;
            }
        if (KdenliveSettings::videothumbnails() != w->enableVideoThumbs())
            slotSwitchVideoThumbs();
        if (KdenliveSettings::audiothumbnails() != w->enableAudioThumbs())
            slotSwitchAudioThumbs();
        profileName = w->selectedProfile();
        projectFolder = w->selectedFolder();
        projectTracks = w->tracks();
        documentProperties.insert("enableproxy", QString::number((int) w->useProxy()));
        documentProperties.insert("generateproxy", QString::number((int) w->generateProxy()));
        documentProperties.insert("proxyminsize", QString::number(w->proxyMinSize()));
        documentProperties.insert("proxyparams", w->proxyParams());
        documentProperties.insert("proxyextension", w->proxyExtension());
        documentProperties.insert("generateimageproxy", QString::number((int) w->generateImageProxy()));
        documentProperties.insert("proxyimageminsize", QString::number(w->proxyImageMinSize()));
        documentMetadata = w->metadata();
        delete w;
    }
    m_timelineArea->setEnabled(true);
    m_projectList->setEnabled(true);
    bool openBackup;
    KdenliveDoc *doc = new KdenliveDoc(KUrl(), projectFolder, m_commandStack, profileName, documentProperties, documentMetadata, projectTracks, m_projectMonitor->render, m_notesWidget, &openBackup, this);
    doc->m_autosave = new KAutoSaveFile(KUrl(), doc);
    bool ok;
    TrackView *trackView = new TrackView(doc, m_tracksActionCollection->actions(), &ok, this);
    m_timelineArea->addTab(trackView, KIcon("kdenlive"), doc->description());
    if (!ok) {
        // MLT is broken
        //m_timelineArea->setEnabled(false);
        //m_projectList->setEnabled(false);
        slotPreferences(6);
        return;
    }
    if (m_timelineArea->count() == 1) {
        connectDocumentInfo(doc);
        connectDocument(trackView, doc);
    } else
        m_timelineArea->setTabBarHidden(false);
    m_monitorManager->activateMonitor(Kdenlive::clipMonitor);
    m_closeAction->setEnabled(m_timelineArea->count() > 1);
}

void MainWindow::activateDocument()
{
    if (m_timelineArea->currentWidget() == NULL || !m_timelineArea->isEnabled()) return;
    TrackView *currentTab = (TrackView *) m_timelineArea->currentWidget();
    KdenliveDoc *currentDoc = currentTab->document();
    connectDocumentInfo(currentDoc);
    connectDocument(currentTab, currentDoc);
}

bool MainWindow::closeCurrentDocument(bool saveChanges)
{
    QWidget *w = m_timelineArea->currentWidget();
    if (!w) return true;
    // closing current document
    int ix = m_timelineArea->currentIndex() + 1;
    if (ix == m_timelineArea->count()) ix = 0;
    m_timelineArea->setCurrentIndex(ix);
    TrackView *tabToClose = (TrackView *) w;
    KdenliveDoc *docToClose = tabToClose->document();
    if (docToClose && docToClose->isModified() && saveChanges) {
        QString message;
        if (m_activeDocument->url().fileName().isEmpty())
            message = i18n("Save changes to document?");
        else
            message = i18n("The project <b>\"%1\"</b> has been changed.\nDo you want to save your changes?", m_activeDocument->url().fileName());
        switch (KMessageBox::warningYesNoCancel(this, message)) {
        case KMessageBox::Yes :
            // save document here. If saving fails, return false;
            if (saveFile() == false) return false;
            break;
        case KMessageBox::Cancel :
            return false;
            break;
        default:
            break;
        }
    }
    slotTimelineClipSelected(NULL, false);
    m_clipMonitor->slotSetClipProducer(NULL);
    m_projectList->slotResetProjectList();
    m_timelineArea->removeTab(m_timelineArea->indexOf(w));
    if (m_timelineArea->count() == 1) {
        m_timelineArea->setTabBarHidden(true);
        m_closeAction->setEnabled(false);
    }
    if (docToClose == m_activeDocument) {
        delete m_activeDocument;
        m_activeDocument = NULL;
        m_effectStack->clear();
        m_transitionConfig->slotTransitionItemSelected(NULL, 0, QPoint(), false);
    } else {
        delete docToClose;
    }
    if (w == m_activeTimeline) {
        delete m_activeTimeline;
        m_activeTimeline = NULL;
    } else {
        delete w;
    }
    return true;
}

bool MainWindow::saveFileAs(const QString &outputFileName)
{
    QString currentSceneList;
    m_monitorManager->stopActiveMonitor();

    if (m_activeDocument->saveSceneList(outputFileName, m_projectMonitor->sceneList(), m_projectList->expandedFolders()) == false)
        return false;

    // Save timeline thumbnails
    m_activeTimeline->projectView()->saveThumbnails();
    m_activeDocument->setUrl(KUrl(outputFileName));
    QByteArray hash = QCryptographicHash::hash(KUrl(outputFileName).encodedPath(), QCryptographicHash::Md5).toHex();
    if (m_activeDocument->m_autosave == NULL) {
        m_activeDocument->m_autosave = new KAutoSaveFile(KUrl(hash), this);
    } else m_activeDocument->m_autosave->setManagedFile(KUrl(hash));
    setCaption(m_activeDocument->description());
    m_timelineArea->setTabText(m_timelineArea->currentIndex(), m_activeDocument->description());
    m_timelineArea->setTabToolTip(m_timelineArea->currentIndex(), m_activeDocument->url().path());
    m_activeDocument->setModified(false);
    m_fileOpenRecent->addUrl(KUrl(outputFileName));
    m_fileRevert->setEnabled(true);
    m_undoView->stack()->setClean();
    return true;
}

bool MainWindow::saveFileAs()
{
    QString outputFile = KFileDialog::getSaveFileName(m_activeDocument->projectFolder(), getMimeType(false));
    if (outputFile.isEmpty()) {
        return false;
    }
    if (QFile::exists(outputFile)) {
        // Show the file dialog again if the user does not want to overwrite the file
        if (KMessageBox::questionYesNo(this, i18n("File %1 already exists.\nDo you want to overwrite it?", outputFile)) == KMessageBox::No)
            return saveFileAs();
    }
    return saveFileAs(outputFile);
}

bool MainWindow::saveFile()
{
    if (!m_activeDocument) return true;
    if (m_activeDocument->url().isEmpty()) {
        return saveFileAs();
    } else {
        bool result = saveFileAs(m_activeDocument->url().path());
        m_activeDocument->m_autosave->resize(0);
        return result;
    }
}

void MainWindow::openFile()
{
    if (!m_startUrl.isEmpty()) {
        openFile(m_startUrl);
        m_startUrl = KUrl();
        return;
    }
    KUrl url = KFileDialog::getOpenUrl(KUrl("kfiledialog:///projectfolder"), getMimeType());
    if (url.isEmpty()) return;
    m_fileOpenRecent->addUrl(url);
    openFile(url);
}

void MainWindow::openLastFile()
{
    KSharedConfigPtr config = KGlobal::config();
    KUrl::List urls = m_fileOpenRecent->urls();
    //WARNING: this is buggy, we get a random url, not the last one. Bug in KRecentFileAction?
    if (urls.isEmpty()) newFile(false);
    else openFile(urls.last());
}

void MainWindow::openFile(const KUrl &url)
{
    // Make sure the url is a Kdenlive project file
    KMimeType::Ptr mime = KMimeType::findByUrl(url);
    if (mime.data()->is("application/x-compressed-tar")) {
        // Opening a compressed project file, we need to process it
        kDebug()<<"Opening archive, processing";
        QPointer<ArchiveWidget> ar = new ArchiveWidget(url);
        if (ar->exec() == QDialog::Accepted) openFile(KUrl(ar->extractedProjectFile()));
        delete ar;
        return;
    }
    if (!url.fileName().endsWith(".kdenlive")) {
        // This is not a Kdenlive project file, abort loading
        KMessageBox::sorry(this, i18n("File %1 is not a Kdenlive project file", url.path()));
        return;
    }

    // Check if the document is already opened
    const int ct = m_timelineArea->count();
    bool isOpened = false;
    int i;
    for (i = 0; i < ct; i++) {
        TrackView *tab = (TrackView *) m_timelineArea->widget(i);
        KdenliveDoc *doc = tab->document();
        if (doc->url() == url) {
            isOpened = true;
            break;
        }
    }
    if (isOpened) {
        m_timelineArea->setCurrentIndex(i);
        return;
    }

    if (!KdenliveSettings::activatetabs()) if (!closeCurrentDocument()) return;

    // Check for backup file
    QByteArray hash = QCryptographicHash::hash(url.encodedPath(), QCryptographicHash::Md5).toHex();
    QList<KAutoSaveFile *> staleFiles = KAutoSaveFile::staleFiles(KUrl(hash));
    if (!staleFiles.isEmpty()) {
        if (KMessageBox::questionYesNo(this,
                                       i18n("Auto-saved files exist. Do you want to recover them now?"),
                                       i18n("File Recovery"),
                                       KGuiItem(i18n("Recover")), KGuiItem(i18n("Don't recover"))) == KMessageBox::Yes) {
            recoverFiles(staleFiles, url);
            return;
        } else {
            // remove the stale files
            foreach(KAutoSaveFile * stale, staleFiles) {
                stale->open(QIODevice::ReadWrite);
                delete stale;
            }
        }
    }
    m_messageLabel->setMessage(i18n("Opening file %1", url.path()), InformationMessage);
    m_messageLabel->repaint();
    doOpenFile(url, NULL);
}

void MainWindow::doOpenFile(const KUrl &url, KAutoSaveFile *stale)
{
    if (!m_timelineArea->isEnabled()) return;
    m_fileRevert->setEnabled(true);

    // Recreate stopmotion widget on document change
    if (m_stopmotion) {
        delete m_stopmotion;
        m_stopmotion = NULL;
    }

    m_timer.start();
    KProgressDialog progressDialog(this, i18n("Loading project"), i18n("Loading project"));
    progressDialog.setAllowCancel(false);
    progressDialog.progressBar()->setMaximum(4);
    progressDialog.show();
    progressDialog.progressBar()->setValue(0);
    qApp->processEvents();

    bool openBackup;
    KdenliveDoc *doc = new KdenliveDoc(stale ? KUrl(stale->fileName()) : url, KdenliveSettings::defaultprojectfolder(), m_commandStack, KdenliveSettings::default_profile(), QMap <QString, QString> (), QMap <QString, QString> (), QPoint(KdenliveSettings::videotracks(), KdenliveSettings::audiotracks()), m_projectMonitor->render, m_notesWidget, &openBackup, this, &progressDialog);

    progressDialog.progressBar()->setValue(1);
    progressDialog.progressBar()->setMaximum(4);
    progressDialog.setLabelText(i18n("Loading project"));
    qApp->processEvents();

    if (stale == NULL) {
        QByteArray hash = QCryptographicHash::hash(url.encodedPath(), QCryptographicHash::Md5).toHex();
        stale = new KAutoSaveFile(KUrl(hash), doc);
        doc->m_autosave = stale;
    } else {
        doc->m_autosave = stale;
        doc->setUrl(url);//stale->managedFile());
        doc->setModified(true);
        stale->setParent(doc);
    }
    connectDocumentInfo(doc);

    progressDialog.progressBar()->setValue(2);
    qApp->processEvents();

    bool ok;
    TrackView *trackView = new TrackView(doc, m_tracksActionCollection->actions(), &ok, this);
    connectDocument(trackView, doc);
    progressDialog.progressBar()->setValue(3);
    qApp->processEvents();

    m_timelineArea->setCurrentIndex(m_timelineArea->addTab(trackView, KIcon("kdenlive"), doc->description()));
    if (!ok) {
        m_timelineArea->setEnabled(false);
        m_projectList->setEnabled(false);
        KMessageBox::sorry(this, i18n("Cannot open file %1.\nProject is corrupted.", url.path()));
        slotGotProgressInfo(QString(), -1);
        newFile(false, true);
        return;
    }
    m_timelineArea->setTabToolTip(m_timelineArea->currentIndex(), doc->url().path());
    trackView->setDuration(trackView->duration());
    trackView->projectView()->initCursorPos(m_projectMonitor->render->seekPosition().frames(doc->fps()));

    if (m_timelineArea->count() > 1) m_timelineArea->setTabBarHidden(false);
    slotGotProgressInfo(QString(), -1);
    m_projectMonitor->adjustRulerSize(trackView->duration());
    m_projectMonitor->slotZoneMoved(trackView->inPoint(), trackView->outPoint());
    progressDialog.progressBar()->setValue(4);
    if (openBackup) slotOpenBackupDialog(url);
}

void MainWindow::recoverFiles(QList<KAutoSaveFile *> staleFiles, const KUrl &originUrl)
{
    foreach(KAutoSaveFile * stale, staleFiles) {
        /*if (!stale->open(QIODevice::QIODevice::ReadOnly)) {
                  // show an error message; we could not steal the lockfile
                  // maybe another application got to the file before us?
                  delete stale;
                  continue;
        }*/
        kDebug() << "// OPENING RECOVERY: " << stale->fileName() << "\nMANAGED: " << stale->managedFile().path();
        // the stalefiles also contain ".lock" files so we must ignore them... bug in KAutoSaveFile?
        if (!stale->fileName().endsWith(".lock")) doOpenFile(originUrl, stale);
        else KIO::NetAccess::del(KUrl(stale->fileName()), this);
    }
}

void MainWindow::parseProfiles(const QString &mltPath)
{
    //KdenliveSettings::setDefaulttmpfolder();
    if (!mltPath.isEmpty()) {
        KdenliveSettings::setMltpath(mltPath + "/share/mlt/profiles/");
        KdenliveSettings::setRendererpath(mltPath + "/bin/melt");
    }

    if (KdenliveSettings::mltpath().isEmpty())
        KdenliveSettings::setMltpath(QString(MLT_PREFIX) + QString("/share/mlt/profiles/"));

    if (KdenliveSettings::rendererpath().isEmpty() || KdenliveSettings::rendererpath().endsWith("inigo")) {
        QString meltPath = QString(MLT_PREFIX) + QString("/bin/melt");
        if (!QFile::exists(meltPath))
            meltPath = KStandardDirs::findExe("melt");
        KdenliveSettings::setRendererpath(meltPath);
    }

    if (KdenliveSettings::rendererpath().isEmpty()) {
        // Cannot find the MLT melt renderer, ask for location
        QPointer<KUrlRequesterDialog> getUrl = new KUrlRequesterDialog(QString(), i18n("Cannot find the melt program required for rendering (part of MLT)"), this);
        if (getUrl->exec() == QDialog::Rejected) {
            delete getUrl;
            ::exit(0);
        }
        KUrl rendererPath = getUrl->selectedUrl();
        delete getUrl;
        if (rendererPath.isEmpty()) ::exit(0);
        KdenliveSettings::setRendererpath(rendererPath.path());
    }

    QStringList profilesFilter;
    profilesFilter << "*";
    QStringList profilesList = QDir(KdenliveSettings::mltpath()).entryList(profilesFilter, QDir::Files);
    if (profilesList.isEmpty()) {
        // Cannot find MLT path, try finding melt
        QString profilePath = KdenliveSettings::rendererpath();
        if (!profilePath.isEmpty()) {
            profilePath = profilePath.section('/', 0, -3);
            KdenliveSettings::setMltpath(profilePath + "/share/mlt/profiles/");
            profilesList = QDir(KdenliveSettings::mltpath()).entryList(profilesFilter, QDir::Files);
        }
        if (profilesList.isEmpty()) {
            // Cannot find the MLT profiles, ask for location
            QPointer<KUrlRequesterDialog> getUrl = new KUrlRequesterDialog(KdenliveSettings::mltpath(), i18n("Cannot find your MLT profiles, please give the path"), this);
            getUrl->fileDialog()->setMode(KFile::Directory);
            if (getUrl->exec() == QDialog::Rejected) {
                delete getUrl;
                ::exit(0);
            }
            KUrl mltPath = getUrl->selectedUrl();
            delete getUrl;
            if (mltPath.isEmpty()) ::exit(0);
            KdenliveSettings::setMltpath(mltPath.path(KUrl::AddTrailingSlash));
            profilesList = QDir(KdenliveSettings::mltpath()).entryList(profilesFilter, QDir::Files);
        }
    }

    kDebug() << "RESULTING MLT PATH: " << KdenliveSettings::mltpath();

    // Parse again MLT profiles to build a list of available video formats
    if (profilesList.isEmpty()) parseProfiles();
}


void MainWindow::slotEditProfiles()
{
    ProfilesDialog *w = new ProfilesDialog;
    if (w->exec() == QDialog::Accepted) {
        KdenliveSettingsDialog* d = static_cast <KdenliveSettingsDialog*>(KConfigDialog::exists("settings"));
        if (d) d->checkProfile();
    }
    delete w;
}

void MainWindow::slotDetectAudioDriver()
{
    /* WARNING: do not use this method because sometimes detects wrong driver (pulse instead of alsa),
    leading to no audio output, see bug #934 */

    //decide which audio driver is really best, in some cases SDL is wrong
    if (KdenliveSettings::audiodrivername().isEmpty()) {
        QString driver;
        KProcess readProcess;
        //PulseAudio needs to be selected if it exists, the ALSA pulse pcm device is not fast enough.
        if (!KStandardDirs::findExe("pactl").isEmpty()) {
            readProcess.setOutputChannelMode(KProcess::OnlyStdoutChannel);
            readProcess.setProgram("pactl", QStringList() << "stat");
            readProcess.execute(2000); // Kill it after 2 seconds

            QString result = QString(readProcess.readAllStandardOutput());
            kDebug() << "// / / / / / READING PACTL: ";
            kDebug() << result;
            if (!result.isEmpty()) {
                driver = "pulse";
                kDebug() << "// / / / / PULSEAUDIO DETECTED";
            }
        }
        //put others here
        KdenliveSettings::setAutoaudiodrivername(driver);
    }
}

void MainWindow::slotEditProjectSettings()
{
    QPoint p = m_activeDocument->getTracksCount();
    QPointer<ProjectSettings> w = new ProjectSettings(m_projectList, m_activeDocument->metadata(), m_activeTimeline->projectView()->extractTransitionsLumas(), p.x(), p.y(), m_activeDocument->projectFolder().path(), true, !m_activeDocument->isModified(), this);
    connect(w, SIGNAL(disableProxies()), this, SLOT(slotDisableProxies()));

    if (w->exec() == QDialog::Accepted) {
        QString profile = w->selectedProfile();
        m_activeDocument->setProjectFolder(w->selectedFolder());
#ifndef Q_WS_MAC
        m_recMonitor->slotUpdateCaptureFolder(m_activeDocument->projectFolder().path(KUrl::AddTrailingSlash));
#endif
        if (m_renderWidget) m_renderWidget->setDocumentPath(m_activeDocument->projectFolder().path(KUrl::AddTrailingSlash));
        if (KdenliveSettings::videothumbnails() != w->enableVideoThumbs()) slotSwitchVideoThumbs();
        if (KdenliveSettings::audiothumbnails() != w->enableAudioThumbs()) slotSwitchAudioThumbs();
        if (m_activeDocument->profilePath() != profile) slotUpdateProjectProfile(profile);
        if (m_activeDocument->getDocumentProperty("proxyparams") != w->proxyParams()) {
            m_activeDocument->setModified();
            m_activeDocument->setDocumentProperty("proxyparams", w->proxyParams());
            if (m_activeDocument->clipManager()->clipsCount() > 0 && KMessageBox::questionYesNo(this, i18n("You have changed the proxy parameters. Do you want to recreate all proxy clips for this project?")) == KMessageBox::Yes) {
                //TODO: rebuild all proxies
                //m_projectList->rebuildProxies();
            }
        }
        if (m_activeDocument->getDocumentProperty("proxyextension") != w->proxyExtension()) {
            m_activeDocument->setModified();
            m_activeDocument->setDocumentProperty("proxyextension", w->proxyExtension());
        }
        if (m_activeDocument->getDocumentProperty("generateproxy") != QString::number((int) w->generateProxy())) {
            m_activeDocument->setModified();
            m_activeDocument->setDocumentProperty("generateproxy", QString::number((int) w->generateProxy()));
        }
        if (m_activeDocument->getDocumentProperty("proxyminsize") != QString::number(w->proxyMinSize())) {
            m_activeDocument->setModified();
            m_activeDocument->setDocumentProperty("proxyminsize", QString::number(w->proxyMinSize()));
        }
        if (m_activeDocument->getDocumentProperty("generateimageproxy") != QString::number((int) w->generateImageProxy())) {
            m_activeDocument->setModified();
            m_activeDocument->setDocumentProperty("generateimageproxy", QString::number((int) w->generateImageProxy()));
        }
        if (m_activeDocument->getDocumentProperty("proxyimageminsize") != QString::number(w->proxyImageMinSize())) {
            m_activeDocument->setModified();
            m_activeDocument->setDocumentProperty("proxyimageminsize", QString::number(w->proxyImageMinSize()));
        }
        if (QString::number((int) w->useProxy()) != m_activeDocument->getDocumentProperty("enableproxy")) {
            m_activeDocument->setDocumentProperty("enableproxy", QString::number((int) w->useProxy()));
            m_activeDocument->setModified();
            slotUpdateProxySettings();
        }
        m_activeDocument->setMetadata(w->metadata());
    }
    delete w;
}

void MainWindow::slotDisableProxies()
{
    m_activeDocument->setDocumentProperty("enableproxy", QString::number((int) false));
    m_activeDocument->setModified();
    slotUpdateProxySettings();
}

void MainWindow::slotUpdateProjectProfile(const QString &profile)
{
    // Recreate the stopmotion widget if profile changes
    if (m_stopmotion) {
        delete m_stopmotion;
        m_stopmotion = NULL;
    }

    // Deselect current effect / transition
    m_effectStack->slotClipItemSelected(NULL);
    m_transitionConfig->slotTransitionItemSelected(NULL, 0, QPoint(), false);
    m_clipMonitor->slotSetClipProducer(NULL);
    bool updateFps = m_activeDocument->setProfilePath(profile);
    KdenliveSettings::setCurrent_profile(profile);
    KdenliveSettings::setProject_fps(m_activeDocument->fps());
    setCaption(m_activeDocument->description(), m_activeDocument->isModified());

    m_activeDocument->clipManager()->clearUnusedProducers();
    m_monitorManager->resetProfiles(m_activeDocument->timecode());
    m_transitionConfig->updateProjectFormat(m_activeDocument->mltProfile(), m_activeDocument->timecode(), m_activeDocument->tracksList());
    m_effectStack->updateProjectFormat(m_activeDocument->mltProfile(), m_activeDocument->timecode());
    m_projectList->updateProjectFormat(m_activeDocument->timecode());
    if (m_renderWidget) m_renderWidget->setProfile(m_activeDocument->mltProfile());
    m_timelineArea->setTabText(m_timelineArea->currentIndex(), m_activeDocument->description());
    if (updateFps) m_activeTimeline->updateProjectFps();
    m_activeDocument->clipManager()->clearCache();
    m_activeTimeline->updateProfile();
    m_activeDocument->setModified(true);
    m_commandStack->activeStack()->clear();
    //Update the mouse position display so it will display in DF/NDF format by default based on the project setting.
    slotUpdateMousePosition(0);
    // We need to desactivate & reactivate monitors to get a refresh
    //m_monitorManager->switchMonitors();
}


void MainWindow::slotRenderProject()
{
    if (!m_renderWidget) {
        QString projectfolder = m_activeDocument ? m_activeDocument->projectFolder().path(KUrl::AddTrailingSlash) : KdenliveSettings::defaultprojectfolder();
        MltVideoProfile profile;
        if (m_activeDocument) profile = m_activeDocument->mltProfile();
        m_renderWidget = new RenderWidget(projectfolder, m_projectList->useProxy(), profile, this);
        connect(m_renderWidget, SIGNAL(shutdown()), this, SLOT(slotShutdown()));
        connect(m_renderWidget, SIGNAL(selectedRenderProfile(QMap <QString, QString>)), this, SLOT(slotSetDocumentRenderProfile(QMap <QString, QString>)));
        connect(m_renderWidget, SIGNAL(prepareRenderingData(bool, bool, const QString&)), this, SLOT(slotPrepareRendering(bool, bool, const QString&)));
        connect(m_renderWidget, SIGNAL(abortProcess(const QString &)), this, SIGNAL(abortRenderJob(const QString &)));
        connect(m_renderWidget, SIGNAL(openDvdWizard(const QString &)), this, SLOT(slotDvdWizard(const QString &)));
        if (m_activeDocument) {
            m_renderWidget->setProfile(m_activeDocument->mltProfile());
            m_renderWidget->setGuides(m_activeDocument->guidesXml(), m_activeDocument->projectDuration());
            m_renderWidget->setDocumentPath(m_activeDocument->projectFolder().path(KUrl::AddTrailingSlash));
            m_renderWidget->setRenderProfile(m_activeDocument->getRenderProperties());
        }
    }
    slotCheckRenderStatus();
    m_renderWidget->show();
    m_renderWidget->showNormal();

    // What are the following lines supposed to do?
    //m_activeTimeline->tracksNumber();
    //m_renderWidget->enableAudio(false);
    //m_renderWidget->export_audio;
}

void MainWindow::slotCheckRenderStatus()
{
    // Make sure there are no missing clips
    if (m_renderWidget)
        m_renderWidget->missingClips(m_projectList->hasMissingClips());
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
    m_projectList->cleanup();
}

void MainWindow::slotUpdateMousePosition(int pos)
{
    if (m_activeDocument)
        switch (m_timecodeFormat->currentIndex()) {
        case 0:
            statusBar()->changeItem(m_activeDocument->timecode().getTimecodeFromFrames(pos), ID_TIMELINE_POS);
            break;
        default:
            statusBar()->changeItem(QString::number(pos), ID_TIMELINE_POS);
        }
}

void MainWindow::slotUpdateDocumentState(bool modified)
{
    if (!m_activeDocument) return;
    setCaption(m_activeDocument->description(), modified);
    m_saveAction->setEnabled(modified);
    if (modified) {
        m_timelineArea->setTabTextColor(m_timelineArea->currentIndex(), palette().color(QPalette::Link));
        m_timelineArea->setTabIcon(m_timelineArea->currentIndex(), KIcon("document-save"));
    } else {
        m_timelineArea->setTabTextColor(m_timelineArea->currentIndex(), palette().color(QPalette::Text));
        m_timelineArea->setTabIcon(m_timelineArea->currentIndex(), KIcon("kdenlive"));
    }
}

void MainWindow::connectDocumentInfo(KdenliveDoc *doc)
{
    if (m_activeDocument) {
        if (m_activeDocument == doc) return;
        disconnect(m_activeDocument, SIGNAL(progressInfo(const QString &, int)), this, SLOT(slotGotProgressInfo(const QString &, int)));
    }
    connect(doc, SIGNAL(progressInfo(const QString &, int)), this, SLOT(slotGotProgressInfo(const QString &, int)));
}

void MainWindow::connectDocument(TrackView *trackView, KdenliveDoc *doc)   //changed
{
    //m_projectMonitor->stop();
    m_closeAction->setEnabled(m_timelineArea->count() > 1);
    kDebug() << "///////////////////   CONNECTING DOC TO PROJECT VIEW ////////////////";
    if (m_activeDocument) {
        if (m_activeDocument == doc) return;
        if (m_activeTimeline) {
            disconnect(m_projectMonitor, SIGNAL(renderPosition(int)), m_activeTimeline, SLOT(moveCursorPos(int)));
            disconnect(m_projectMonitor, SIGNAL(zoneUpdated(QPoint)), m_activeTimeline, SLOT(slotSetZone(QPoint)));
            disconnect(m_projectMonitor, SIGNAL(durationChanged(int)), m_activeTimeline, SLOT(setDuration(int)));
            disconnect(m_projectMonitor, SIGNAL(zoneUpdated(QPoint)), m_activeDocument, SLOT(setModified()));
            disconnect(m_notesWidget, SIGNAL(textChanged()), m_activeDocument, SLOT(setModified()));
            disconnect(m_clipMonitor, SIGNAL(zoneUpdated(QPoint)), m_activeDocument, SLOT(setModified()));
            disconnect(m_projectList, SIGNAL(projectModified()), m_activeDocument, SLOT(setModified()));

            disconnect(m_projectMonitor->render, SIGNAL(refreshDocumentProducers(bool, bool)), m_activeDocument, SLOT(checkProjectClips(bool, bool)));

            disconnect(m_activeDocument, SIGNAL(guidesUpdated()), this, SLOT(slotGuidesUpdated()));
            disconnect(m_activeDocument, SIGNAL(addProjectClip(DocClipBase *, bool)), m_projectList, SLOT(slotAddClip(DocClipBase *, bool)));
            disconnect(m_activeDocument, SIGNAL(resetProjectList()), m_projectList, SLOT(slotResetProjectList()));
            disconnect(m_activeDocument, SIGNAL(signalDeleteProjectClip(const QString &)), this, SLOT(slotDeleteClip(const QString &)));
            disconnect(m_activeDocument, SIGNAL(updateClipDisplay(const QString &)), m_projectList, SLOT(slotUpdateClip(const QString &)));
            disconnect(m_activeDocument, SIGNAL(selectLastAddedClip(const QString &)), m_projectList, SLOT(slotSelectClip(const QString &)));
            disconnect(m_activeTimeline->projectView(), SIGNAL(clipItemSelected(ClipItem*, bool)), this, SLOT(slotTimelineClipSelected(ClipItem*, bool)));
            disconnect(m_activeTimeline->projectView(), SIGNAL(transitionItemSelected(Transition*, int, QPoint, bool)), m_transitionConfig, SLOT(slotTransitionItemSelected(Transition*, int, QPoint, bool)));
            disconnect(m_activeTimeline->projectView(), SIGNAL(transitionItemSelected(Transition*, int, QPoint, bool)), this, SLOT(slotActivateTransitionView(Transition *)));
            disconnect(m_activeTimeline->projectView(), SIGNAL(playMonitor()), m_projectMonitor, SLOT(slotPlay()));
            disconnect(m_activeTimeline->projectView(), SIGNAL(displayMessage(const QString&, MessageType)), m_messageLabel, SLOT(setMessage(const QString&, MessageType)));
            disconnect(m_activeTimeline->projectView(), SIGNAL(showClipFrame(DocClipBase *, QPoint, bool, const int)), m_clipMonitor, SLOT(slotSetClipProducer(DocClipBase *, QPoint, bool, const int)));
            disconnect(m_projectList, SIGNAL(gotFilterJobResults(const QString &, int, int, stringMap,stringMap)), m_activeTimeline->projectView(), SLOT(slotGotFilterJobResults(const QString &, int, int, stringMap, stringMap)));

            disconnect(m_activeTimeline, SIGNAL(cursorMoved()), m_projectMonitor, SLOT(slotActivateMonitor()));
            disconnect(m_activeTimeline, SIGNAL(configTrack(int)), this, SLOT(slotConfigTrack(int)));
            disconnect(m_activeDocument, SIGNAL(docModified(bool)), this, SLOT(slotUpdateDocumentState(bool)));
            disconnect(m_effectStack, SIGNAL(updateEffect(ClipItem*, int, QDomElement, QDomElement, int,bool)), m_activeTimeline->projectView(), SLOT(slotUpdateClipEffect(ClipItem*, int, QDomElement, QDomElement, int,bool)));
            disconnect(m_effectStack, SIGNAL(removeEffect(ClipItem*, int, QDomElement)), m_activeTimeline->projectView(), SLOT(slotDeleteEffect(ClipItem*, int, QDomElement)));
	    disconnect(m_effectStack, SIGNAL(addEffect(ClipItem*, QDomElement)), trackView->projectView(), SLOT(slotAddEffect(ClipItem*, QDomElement)));
            disconnect(m_effectStack, SIGNAL(changeEffectState(ClipItem*, int, QList <int>, bool)), m_activeTimeline->projectView(), SLOT(slotChangeEffectState(ClipItem*, int, QList <int>, bool)));
            disconnect(m_effectStack, SIGNAL(changeEffectPosition(ClipItem*, int, QList<int>, int)), m_activeTimeline->projectView(), SLOT(slotChangeEffectPosition(ClipItem*, int, QList <int>, int)));
            disconnect(m_effectStack, SIGNAL(refreshEffectStack(ClipItem*)), m_activeTimeline->projectView(), SLOT(slotRefreshEffects(ClipItem*)));
            disconnect(m_effectStack, SIGNAL(reloadEffects()), this, SLOT(slotReloadEffects()));
            disconnect(m_effectStack, SIGNAL(displayMessage(const QString&, int)), this, SLOT(slotGotProgressInfo(const QString&, int)));
            disconnect(m_transitionConfig, SIGNAL(transitionUpdated(Transition *, QDomElement)), m_activeTimeline->projectView() , SLOT(slotTransitionUpdated(Transition *, QDomElement)));
            disconnect(m_transitionConfig, SIGNAL(seekTimeline(int)), m_activeTimeline->projectView() , SLOT(setCursorPos(int)));
	    disconnect(m_transitionConfig, SIGNAL(importClipKeyframes(GRAPHICSRECTITEM)), m_activeTimeline->projectView() , SLOT(slotImportClipKeyframes(GRAPHICSRECTITEM)));

            disconnect(m_activeTimeline->projectView(), SIGNAL(activateDocumentMonitor()), m_projectMonitor, SLOT(slotActivateMonitor()));
            disconnect(m_activeTimeline, SIGNAL(zoneMoved(int, int)), this, SLOT(slotZoneMoved(int, int)));
            disconnect(m_projectList, SIGNAL(loadingIsOver()), m_activeTimeline->projectView(), SLOT(slotUpdateAllThumbs()));
            disconnect(m_projectList, SIGNAL(refreshClip(const QString &)), m_activeTimeline->projectView(), SLOT(slotRefreshThumbs(const QString &)));
	    disconnect(m_projectList, SIGNAL(addMarkers(const QString &, QList <CommentedTime>)), m_activeTimeline->projectView(), SLOT(slotAddClipMarker(const QString &, QList <CommentedTime>)));
            m_effectStack->clear();
        }
        //m_activeDocument->setRenderer(NULL);
        m_clipMonitor->stop();
    }
    KdenliveSettings::setCurrent_profile(doc->profilePath());
    KdenliveSettings::setProject_fps(doc->fps());
    m_monitorManager->resetProfiles(doc->timecode());
    m_clipMonitorDock->raise();
    m_projectList->setDocument(doc);
    m_transitionConfig->updateProjectFormat(doc->mltProfile(), doc->timecode(), doc->tracksList());
    m_effectStack->updateProjectFormat(doc->mltProfile(), doc->timecode());
    connect(m_projectList, SIGNAL(refreshClip(const QString &, bool)), trackView->projectView(), SLOT(slotRefreshThumbs(const QString &, bool)));

    connect(m_projectList, SIGNAL(projectModified()), doc, SLOT(setModified()));
    connect(m_projectList, SIGNAL(clipNameChanged(const QString, const QString)), trackView->projectView(), SLOT(clipNameChanged(const QString, const QString)));

    connect(trackView, SIGNAL(configTrack(int)), this, SLOT(slotConfigTrack(int)));
    connect(trackView, SIGNAL(updateTracksInfo()), this, SLOT(slotUpdateTrackInfo()));
    connect(trackView, SIGNAL(mousePosition(int)), this, SLOT(slotUpdateMousePosition(int)));
    connect(trackView->projectView(), SIGNAL(forceClipProcessing(const QString &)), m_projectList, SLOT(slotForceProcessing(const QString &)));

    connect(trackView->projectView(), SIGNAL(importKeyframes(GRAPHICSRECTITEM, const QString&, int)), this, SLOT(slotProcessImportKeyframes(GRAPHICSRECTITEM, const QString&, int)));
    
    connect(m_projectMonitor, SIGNAL(renderPosition(int)), trackView, SLOT(moveCursorPos(int)));
    connect(m_projectMonitor, SIGNAL(zoneUpdated(QPoint)), trackView, SLOT(slotSetZone(QPoint)));
    connect(m_projectMonitor, SIGNAL(zoneUpdated(QPoint)), doc, SLOT(setModified()));
    connect(m_clipMonitor, SIGNAL(zoneUpdated(QPoint)), doc, SLOT(setModified()));
    connect(m_projectMonitor, SIGNAL(durationChanged(int)), trackView, SLOT(setDuration(int)));
    connect(m_projectMonitor->render, SIGNAL(refreshDocumentProducers(bool, bool)), doc, SLOT(checkProjectClips(bool, bool)));

    connect(doc, SIGNAL(addProjectClip(DocClipBase *, bool)), m_projectList, SLOT(slotAddClip(DocClipBase *, bool)));
    connect(doc, SIGNAL(resetProjectList()), m_projectList, SLOT(slotResetProjectList()));
    connect(doc, SIGNAL(signalDeleteProjectClip(const QString &)), this, SLOT(slotDeleteClip(const QString &)));
    connect(doc, SIGNAL(updateClipDisplay(const QString &)), m_projectList, SLOT(slotUpdateClip(const QString &)));
    connect(doc, SIGNAL(selectLastAddedClip(const QString &)), m_projectList, SLOT(slotSelectClip(const QString &)));

    connect(doc, SIGNAL(docModified(bool)), this, SLOT(slotUpdateDocumentState(bool)));
    connect(doc, SIGNAL(guidesUpdated()), this, SLOT(slotGuidesUpdated()));
    connect(doc, SIGNAL(saveTimelinePreview(const QString &)), trackView, SLOT(slotSaveTimelinePreview(const QString)));

    connect(m_notesWidget, SIGNAL(textChanged()), doc, SLOT(setModified()));

    connect(trackView->projectView(), SIGNAL(updateClipMarkers(DocClipBase *)), this, SLOT(slotUpdateClipMarkers(DocClipBase*)));
    connect(trackView, SIGNAL(showTrackEffects(int, TrackInfo)), this, SLOT(slotTrackSelected(int, TrackInfo)));

    connect(trackView->projectView(), SIGNAL(clipItemSelected(ClipItem*, bool)), this, SLOT(slotTimelineClipSelected(ClipItem*, bool)));
    connect(trackView->projectView(), SIGNAL(transitionItemSelected(Transition*, int, QPoint, bool)), m_transitionConfig, SLOT(slotTransitionItemSelected(Transition*, int, QPoint, bool)));
    connect(trackView->projectView(), SIGNAL(transitionItemSelected(Transition*, int, QPoint, bool)), this, SLOT(slotActivateTransitionView(Transition *)));
    m_zoomSlider->setValue(doc->zoom().x());
    connect(trackView->projectView(), SIGNAL(zoomIn()), this, SLOT(slotZoomIn()));
    connect(trackView->projectView(), SIGNAL(zoomOut()), this, SLOT(slotZoomOut()));
    connect(trackView, SIGNAL(setZoom(int)), this, SLOT(slotSetZoom(int)));
    connect(trackView->projectView(), SIGNAL(displayMessage(const QString&, MessageType)), m_messageLabel, SLOT(setMessage(const QString&, MessageType)));

    connect(trackView->projectView(), SIGNAL(showClipFrame(DocClipBase *, QPoint, bool, const int)), m_clipMonitor, SLOT(slotSetClipProducer(DocClipBase *, QPoint, bool, const int)));
    connect(trackView->projectView(), SIGNAL(playMonitor()), m_projectMonitor, SLOT(slotPlay()));

    connect(trackView->projectView(), SIGNAL(transitionItemSelected(Transition*, int, QPoint, bool)), m_projectMonitor, SLOT(slotSetSelectedClip(Transition*)));

    connect(m_projectList, SIGNAL(gotFilterJobResults(const QString &, int, int, stringMap,stringMap)), trackView->projectView(), SLOT(slotGotFilterJobResults(const QString &, int, int, stringMap,stringMap)));

    connect(m_projectList, SIGNAL(addMarkers(const QString &, QList <CommentedTime>)), trackView->projectView(), SLOT(slotAddClipMarker(const QString &, QList <CommentedTime>)));

    // Effect stack signals
    connect(m_effectStack, SIGNAL(updateEffect(ClipItem*, int, QDomElement, QDomElement, int,bool)), trackView->projectView(), SLOT(slotUpdateClipEffect(ClipItem*, int, QDomElement, QDomElement, int,bool)));
    connect(m_effectStack, SIGNAL(updateClipRegion(ClipItem*, int, QString)), trackView->projectView(), SLOT(slotUpdateClipRegion(ClipItem*, int, QString)));
    connect(m_effectStack, SIGNAL(removeEffect(ClipItem*, int, QDomElement)), trackView->projectView(), SLOT(slotDeleteEffect(ClipItem*, int, QDomElement)));
    connect(m_effectStack, SIGNAL(addEffect(ClipItem*, QDomElement)), trackView->projectView(), SLOT(slotAddEffect(ClipItem*, QDomElement)));
    connect(m_effectStack, SIGNAL(changeEffectState(ClipItem*, int, QList <int>, bool)), trackView->projectView(), SLOT(slotChangeEffectState(ClipItem*, int, QList <int>, bool)));
    connect(m_effectStack, SIGNAL(changeEffectPosition(ClipItem*, int, QList <int>, int)), trackView->projectView(), SLOT(slotChangeEffectPosition(ClipItem*, int, QList <int>, int)));
    
    connect(m_effectStack, SIGNAL(refreshEffectStack(ClipItem*)), trackView->projectView(), SLOT(slotRefreshEffects(ClipItem*)));
    connect(m_effectStack, SIGNAL(seekTimeline(int)), trackView->projectView(), SLOT(seekCursorPos(int)));
    connect(m_effectStack, SIGNAL(importClipKeyframes(GRAPHICSRECTITEM)), trackView->projectView(), SLOT(slotImportClipKeyframes(GRAPHICSRECTITEM)));
    connect(m_effectStack, SIGNAL(reloadEffects()), this, SLOT(slotReloadEffects()));
    connect(m_effectStack, SIGNAL(displayMessage(const QString&, int)), this, SLOT(slotGotProgressInfo(const QString&, int)));
    
    // Transition config signals
    connect(m_transitionConfig, SIGNAL(transitionUpdated(Transition *, QDomElement)), trackView->projectView() , SLOT(slotTransitionUpdated(Transition *, QDomElement)));
    connect(m_transitionConfig, SIGNAL(importClipKeyframes(GRAPHICSRECTITEM)), trackView->projectView() , SLOT(slotImportClipKeyframes(GRAPHICSRECTITEM)));
    connect(m_transitionConfig, SIGNAL(seekTimeline(int)), trackView->projectView() , SLOT(seekCursorPos(int)));

    connect(trackView->projectView(), SIGNAL(activateDocumentMonitor()), m_projectMonitor, SLOT(slotActivateMonitor()));
    connect(trackView, SIGNAL(zoneMoved(int, int)), this, SLOT(slotZoneMoved(int, int)));
    connect(m_projectList, SIGNAL(loadingIsOver()), trackView->projectView(), SLOT(slotUpdateAllThumbs()));
    trackView->projectView()->setContextMenu(m_timelineContextMenu, m_timelineContextClipMenu, m_timelineContextTransitionMenu, m_clipTypeGroup, static_cast<QMenu*>(factory()->container("marker_menu", this)));
    m_activeTimeline = trackView;
    if (m_renderWidget) {
        slotCheckRenderStatus();
        m_renderWidget->setProfile(doc->mltProfile());
        m_renderWidget->setGuides(doc->guidesXml(), doc->projectDuration());
        m_renderWidget->setDocumentPath(doc->projectFolder().path(KUrl::AddTrailingSlash));
        m_renderWidget->setRenderProfile(doc->getRenderProperties());
    }
    //doc->setRenderer(m_projectMonitor->render);
    m_commandStack->setActiveStack(doc->commandStack());
    KdenliveSettings::setProject_display_ratio(doc->dar());
    //doc->clipManager()->checkAudioThumbs();

    //m_overView->setScene(trackView->projectScene());
    //m_overView->scale(m_overView->width() / trackView->duration(), m_overView->height() / (50 * trackView->tracksNumber()));
    //m_overView->fitInView(m_overView->itemAt(0, 50), Qt::KeepAspectRatio);

    setCaption(doc->description(), doc->isModified());
    m_saveAction->setEnabled(doc->isModified());
    m_normalEditTool->setChecked(true);
    m_activeDocument = doc;
    m_activeTimeline->updateProjectFps();
    m_activeDocument->checkProjectClips();
#ifndef Q_WS_MAC
    m_recMonitor->slotUpdateCaptureFolder(m_activeDocument->projectFolder().path(KUrl::AddTrailingSlash));
#endif
    //Update the mouse position display so it will display in DF/NDF format by default based on the project setting.
    slotUpdateMousePosition(0);
    m_monitorManager->activateMonitor(Kdenlive::clipMonitor);
    // set tool to select tool
    m_buttonSelectTool->setChecked(true);
}

void MainWindow::slotZoneMoved(int start, int end)
{
    m_activeDocument->setZone(start, end);
    m_projectMonitor->slotZoneMoved(start, end);
}

void MainWindow::slotGuidesUpdated()
{
    if (m_renderWidget)
        m_renderWidget->setGuides(m_activeDocument->guidesXml(), m_activeDocument->projectDuration());
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
    if (m_stopmotion) m_stopmotion->slotLive(false);
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
    foreach (const QString& action_name, m_action_names) {
        actions[collection->action(action_name)->text()] = action_name;
    }

    KdenliveSettingsDialog* dialog = new KdenliveSettingsDialog(actions, this);
    connect(dialog, SIGNAL(settingsChanged(const QString&)), this, SLOT(updateConfiguration()));
    connect(dialog, SIGNAL(doResetProfile()), m_monitorManager, SLOT(slotResetProfiles()));
#ifndef Q_WS_MAC
    connect(dialog, SIGNAL(updateCaptureFolder()), this, SLOT(slotUpdateCaptureFolder()));
#endif
    dialog->show();
    if (page != -1) dialog->showPage(page, option);
}

void MainWindow::slotUpdateCaptureFolder()
{

#ifndef Q_WS_MAC
    if (m_activeDocument) m_recMonitor->slotUpdateCaptureFolder(m_activeDocument->projectFolder().path(KUrl::AddTrailingSlash));
    else m_recMonitor->slotUpdateCaptureFolder(KdenliveSettings::defaultprojectfolder());
#endif
}

void MainWindow::updateConfiguration()
{
    //TODO: we should apply settings to all projects, not only the current one
    if (m_activeTimeline) {
        m_activeTimeline->refresh();
        m_activeTimeline->projectView()->checkAutoScroll();
        m_activeTimeline->checkTrackHeight();
        if (m_activeDocument)
            m_activeDocument->clipManager()->checkAudioThumbs();
    }
    m_buttonAudioThumbs->setChecked(KdenliveSettings::audiothumbnails());
    m_buttonVideoThumbs->setChecked(KdenliveSettings::videothumbnails());
    m_buttonShowMarkers->setChecked(KdenliveSettings::showmarkers());
    m_buttonAutomaticSplitAudio->setChecked(KdenliveSettings::splitaudio());

    // Update list of transcoding profiles
    loadTranscoders();
    loadClipActions();
#ifdef USE_JOGSHUTTLE
    activateShuttleDevice();
#endif

}

void MainWindow::slotSwitchSplitAudio()
{
    KdenliveSettings::setSplitaudio(!KdenliveSettings::splitaudio());
    m_buttonAutomaticSplitAudio->setChecked(KdenliveSettings::splitaudio());
}

void MainWindow::slotSwitchVideoThumbs()
{
    KdenliveSettings::setVideothumbnails(!KdenliveSettings::videothumbnails());
    if (m_activeTimeline)
        m_activeTimeline->projectView()->slotUpdateAllThumbs();
    m_buttonVideoThumbs->setChecked(KdenliveSettings::videothumbnails());
}

void MainWindow::slotSwitchAudioThumbs()
{
    KdenliveSettings::setAudiothumbnails(!KdenliveSettings::audiothumbnails());
    if (m_activeTimeline) {
        m_activeTimeline->refresh();
        m_activeTimeline->projectView()->checkAutoScroll();
        if (m_activeDocument)
            m_activeDocument->clipManager()->checkAudioThumbs();
    }
    m_buttonAudioThumbs->setChecked(KdenliveSettings::audiothumbnails());
}

void MainWindow::slotSwitchMarkersComments()
{
    KdenliveSettings::setShowmarkers(!KdenliveSettings::showmarkers());
    if (m_activeTimeline)
        m_activeTimeline->refresh();
    m_buttonShowMarkers->setChecked(KdenliveSettings::showmarkers());
}

void MainWindow::slotSwitchSnap()
{
    KdenliveSettings::setSnaptopoints(!KdenliveSettings::snaptopoints());
    m_buttonSnap->setChecked(KdenliveSettings::snaptopoints());
}

void MainWindow::slotSwitchJackTransport()
{
    KdenliveSettings::setJacktransport(!KdenliveSettings::jacktransport());
    m_buttonJackTransport->setChecked(KdenliveSettings::jacktransport());

    /* switch transport settings */
    if (KdenliveSettings::jacktransport()) {
    	m_monitorManager->slotEnableSlaveTransport();
    } else {
    	m_monitorManager->slotDisableSlaveTransport();
    }
}

void MainWindow::slotDeleteItem()
{
    if (QApplication::focusWidget() &&
            QApplication::focusWidget()->parentWidget() &&
            QApplication::focusWidget()->parentWidget()->parentWidget() &&
            QApplication::focusWidget()->parentWidget()->parentWidget() == m_projectListDock) {
        m_projectList->slotRemoveClip();

    } else {
        QWidget *widget = QApplication::focusWidget();
        while (widget) {
            if (widget == m_effectStackDock) {
                m_effectStack->deleteCurrentEffect();
                return;
            }
            widget = widget->parentWidget();
        }

        // effect stack has no focus
        if (m_activeTimeline)
            m_activeTimeline->projectView()->deleteSelectedClips();
    }
}

void MainWindow::slotUpdateClipMarkers(DocClipBase *clip)
{
    if (m_clipMonitor->isActive())
        m_clipMonitor->checkOverlay();
    m_clipMonitor->updateMarkers(clip);
}

void MainWindow::slotAddClipMarker()
{
    DocClipBase *clip = NULL;
    GenTime pos;
    if (m_projectMonitor->isActive()) {
        if (m_activeTimeline) {
            ClipItem *item = m_activeTimeline->projectView()->getActiveClipUnderCursor();
            if (item) {
                pos = GenTime((int)((m_projectMonitor->position() - item->startPos() + item->cropStart()).frames(m_activeDocument->fps()) * item->speed() + 0.5), m_activeDocument->fps());
                clip = item->baseClip();
            }
        }
    } else {
        clip = m_clipMonitor->activeClip();
        pos = m_clipMonitor->position();
    }
    if (!clip) {
        m_messageLabel->setMessage(i18n("Cannot find clip to add marker"), ErrorMessage);
        return;
    }
    QString id = clip->getId();
    CommentedTime marker(pos, i18n("Marker"), KdenliveSettings::default_marker_type());
    QPointer<MarkerDialog> d = new MarkerDialog(clip, marker,
                       m_activeDocument->timecode(), i18n("Add Marker"), this);
    if (d->exec() == QDialog::Accepted)
        m_activeTimeline->projectView()->slotAddClipMarker(id, QList <CommentedTime>() << d->newMarker());
    delete d;
}

void MainWindow::slotDeleteClipMarker()
{
    DocClipBase *clip = NULL;
    GenTime pos;
    if (m_projectMonitor->isActive()) {
        if (m_activeTimeline) {
            ClipItem *item = m_activeTimeline->projectView()->getActiveClipUnderCursor();
            if (item) {
                pos = (m_projectMonitor->position() - item->startPos() + item->cropStart()) / item->speed();
                clip = item->baseClip();
            }
        }
    } else {
        clip = m_clipMonitor->activeClip();
        pos = m_clipMonitor->position();
    }
    if (!clip) {
        m_messageLabel->setMessage(i18n("Cannot find clip to remove marker"), ErrorMessage);
        return;
    }

    QString id = clip->getId();
    QString comment = clip->markerComment(pos);
    if (comment.isEmpty()) {
        m_messageLabel->setMessage(i18n("No marker found at cursor time"), ErrorMessage);
        return;
    }
    m_activeTimeline->projectView()->slotDeleteClipMarker(comment, id, pos);
}

void MainWindow::slotDeleteAllClipMarkers()
{
    DocClipBase *clip = NULL;
    if (m_projectMonitor->isActive()) {
        if (m_activeTimeline) {
            ClipItem *item = m_activeTimeline->projectView()->getActiveClipUnderCursor();
            if (item) {
                clip = item->baseClip();
            }
        }
    } else {
        clip = m_clipMonitor->activeClip();
    }
    if (!clip) {
        m_messageLabel->setMessage(i18n("Cannot find clip to remove marker"), ErrorMessage);
        return;
    }
    m_activeTimeline->projectView()->slotDeleteAllClipMarkers(clip->getId());
}

void MainWindow::slotEditClipMarker()
{
    DocClipBase *clip = NULL;
    GenTime pos;
    if (m_projectMonitor->isActive()) {
        if (m_activeTimeline) {
            ClipItem *item = m_activeTimeline->projectView()->getActiveClipUnderCursor();
            if (item) {
                pos = (m_projectMonitor->position() - item->startPos() + item->cropStart()) / item->speed();
                clip = item->baseClip();
            }
        }
    } else {
        clip = m_clipMonitor->activeClip();
        pos = m_clipMonitor->position();
    }
    if (!clip) {
        m_messageLabel->setMessage(i18n("Cannot find clip to remove marker"), ErrorMessage);
        return;
    }

    QString id = clip->getId();
    CommentedTime oldMarker = clip->markerAt(pos);
    if (oldMarker == CommentedTime()) {
        m_messageLabel->setMessage(i18n("No marker found at cursor time"), ErrorMessage);
        return;
    }

    QPointer<MarkerDialog> d = new MarkerDialog(clip, oldMarker,
                      m_activeDocument->timecode(), i18n("Edit Marker"), this);
    if (d->exec() == QDialog::Accepted) {
        m_activeTimeline->projectView()->slotAddClipMarker(id, QList <CommentedTime>() <<d->newMarker());
        if (d->newMarker().time() != pos) {
            // remove old marker
            oldMarker.setMarkerType(-1);
            m_activeTimeline->projectView()->slotAddClipMarker(id, QList <CommentedTime>() <<oldMarker);
        }
    }
    delete d;
}

void MainWindow::slotAddMarkerGuideQuickly()
{
    if (!m_activeTimeline || !m_activeDocument)
        return;

    if (m_clipMonitor->isActive()) {
        DocClipBase *clip = m_clipMonitor->activeClip();
        GenTime pos = m_clipMonitor->position();

        if (!clip) {
            m_messageLabel->setMessage(i18n("Cannot find clip to add marker"), ErrorMessage);
            return;
        }
        //TODO: allow user to set default marker category
	CommentedTime marker(pos, m_activeDocument->timecode().getDisplayTimecode(pos, false), KdenliveSettings::default_marker_type());
        m_activeTimeline->projectView()->slotAddClipMarker(clip->getId(), QList <CommentedTime>() <<marker);
    } else {
        m_activeTimeline->projectView()->slotAddGuide(false);
    }
}

void MainWindow::slotAddGuide()
{
    if (m_activeTimeline)
        m_activeTimeline->projectView()->slotAddGuide();
}

void MainWindow::slotInsertSpace()
{
    if (m_activeTimeline)
        m_activeTimeline->projectView()->slotInsertSpace();
}

void MainWindow::slotRemoveSpace()
{
    if (m_activeTimeline)
        m_activeTimeline->projectView()->slotRemoveSpace();
}

void MainWindow::slotInsertTrack(int ix)
{
    m_projectMonitor->slotActivateMonitor();
    if (m_activeTimeline) {
        if (ix == -1) ix = m_activeTimeline->projectView()->selectedTrack();
        m_activeTimeline->projectView()->slotInsertTrack(ix);
    }
    if (m_activeDocument)
        m_transitionConfig->updateProjectFormat(m_activeDocument->mltProfile(), m_activeDocument->timecode(), m_activeDocument->tracksList());
}

void MainWindow::slotDeleteTrack(int ix)
{
    m_projectMonitor->slotActivateMonitor();
    if (m_activeTimeline) {
        if (ix == -1) ix = m_activeTimeline->projectView()->selectedTrack();
        m_activeTimeline->projectView()->slotDeleteTrack(ix);
    }
    if (m_activeDocument)
        m_transitionConfig->updateProjectFormat(m_activeDocument->mltProfile(), m_activeDocument->timecode(), m_activeDocument->tracksList());
}

void MainWindow::slotConfigTrack(int ix)
{
    m_projectMonitor->slotActivateMonitor();
    if (m_activeTimeline)
        m_activeTimeline->projectView()->slotConfigTracks(ix);
    if (m_activeDocument)
        m_transitionConfig->updateProjectFormat(m_activeDocument->mltProfile(), m_activeDocument->timecode(), m_activeDocument->tracksList());
}

void MainWindow::slotSelectTrack()
{
    m_projectMonitor->slotActivateMonitor();
    if (m_activeTimeline) {
        m_activeTimeline->projectView()->slotSelectClipsInTrack();
    }
}

void MainWindow::slotSelectAllTracks()
{
    m_projectMonitor->slotActivateMonitor();
    if (m_activeTimeline)
        m_activeTimeline->projectView()->slotSelectAllClips();
}

void MainWindow::slotEditGuide()
{
    if (m_activeTimeline)
        m_activeTimeline->projectView()->slotEditGuide();
}

void MainWindow::slotDeleteGuide()
{
    if (m_activeTimeline)
        m_activeTimeline->projectView()->slotDeleteGuide();
}

void MainWindow::slotDeleteAllGuides()
{
    if (m_activeTimeline)
        m_activeTimeline->projectView()->slotDeleteAllGuides();
}

void MainWindow::slotCutTimelineClip()
{
    if (m_activeTimeline)
        m_activeTimeline->projectView()->cutSelectedClips();
}

void MainWindow::slotInsertClipOverwrite()
{
    if (m_activeTimeline) {
        QStringList data = m_clipMonitor->getZoneInfo();
        m_activeTimeline->projectView()->insertZoneOverwrite(data, m_activeTimeline->inPoint());
    }
}

void MainWindow::slotSelectTimelineClip()
{
    if (m_activeTimeline)
        m_activeTimeline->projectView()->selectClip(true);
}

void MainWindow::slotSelectTimelineTransition()
{
    if (m_activeTimeline)
        m_activeTimeline->projectView()->selectTransition(true);
}

void MainWindow::slotDeselectTimelineClip()
{
    if (m_activeTimeline)
        m_activeTimeline->projectView()->selectClip(false, true);
}

void MainWindow::slotDeselectTimelineTransition()
{
    if (m_activeTimeline)
        m_activeTimeline->projectView()->selectTransition(false, true);
}

void MainWindow::slotSelectAddTimelineClip()
{
    if (m_activeTimeline)
        m_activeTimeline->projectView()->selectClip(true, true);
}

void MainWindow::slotSelectAddTimelineTransition()
{
    if (m_activeTimeline)
        m_activeTimeline->projectView()->selectTransition(true, true);
}

void MainWindow::slotGroupClips()
{
    if (m_activeTimeline)
        m_activeTimeline->projectView()->groupClips();
}

void MainWindow::slotUnGroupClips()
{
    if (m_activeTimeline)
        m_activeTimeline->projectView()->groupClips(false);
}

void MainWindow::slotEditItemDuration()
{
    if (m_activeTimeline)
        m_activeTimeline->projectView()->editItemDuration();
}

void MainWindow::slotAddProjectClip(KUrl url, stringMap data)
{
    if (m_activeDocument) {
        m_activeDocument->slotAddClipFile(url, data);
    }
}

void MainWindow::slotAddProjectClipList(KUrl::List urls)
{
    if (m_activeDocument)
        m_activeDocument->slotAddClipList(urls);
}

void MainWindow::slotAddTransition(QAction *result)
{
    if (!result) return;
    QStringList info = result->data().toStringList();
    if (info.isEmpty()) return;
    QDomElement transition = transitions.getEffectByTag(info.at(1), info.at(2));
    if (m_activeTimeline && !transition.isNull()) {
        m_activeTimeline->projectView()->slotAddTransitionToSelectedClips(transition.cloneNode().toElement());
    }
}

void MainWindow::slotAddVideoEffect(QAction *result)
{
    if (!result) return;
    const int EFFECT_VIDEO = 1;
    const int EFFECT_AUDIO = 2;
    QStringList info = result->data().toStringList();

    if (info.isEmpty() || info.size() < 3) return;
    QDomElement effect ;
    if (info.last() == QString::number((int) EFFECT_VIDEO))
            effect = videoEffects.getEffectByTag(info.at(0), info.at(1));
    else if (info.last() == QString::number((int) EFFECT_AUDIO))
            effect = audioEffects.getEffectByTag(info.at(0), info.at(1));
    else
            effect = customEffects.getEffectByTag(info.at(0), info.at(1));
    if (!effect.isNull()) slotAddEffect(effect);
    else m_messageLabel->setMessage(i18n("Cannot find effect %1 / %2", info.at(0), info.at(1)), ErrorMessage);
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
    if (m_activeTimeline)
        m_zoomSlider->setValue(m_activeTimeline->fitZoom());
}

void MainWindow::slotSetZoom(int value)
{
    value = qMax(m_zoomSlider->minimum(), value);
    value = qMin(m_zoomSlider->maximum(), value);

    if (m_activeTimeline)
        m_activeTimeline->slotChangeZoom(value);

    m_zoomOut->setEnabled(value < m_zoomSlider->maximum());
    m_zoomIn->setEnabled(value > m_zoomSlider->minimum());
    slotUpdateZoomSliderToolTip(value);

    m_zoomSlider->blockSignals(true);
    m_zoomSlider->setValue(value);
    m_zoomSlider->blockSignals(false);
}

void MainWindow::slotShowZoomSliderToolTip(int zoomlevel)
{
    if (zoomlevel != -1)
        slotUpdateZoomSliderToolTip(zoomlevel);

    QPoint global = m_zoomSlider->rect().topLeft();
    global.ry() += m_zoomSlider->height() / 2;
    QHelpEvent toolTipEvent(QEvent::ToolTip, QPoint(0, 0), m_zoomSlider->mapToGlobal(global));
    QApplication::sendEvent(m_zoomSlider, &toolTipEvent);
}

void MainWindow::slotUpdateZoomSliderToolTip(int zoomlevel)
{
    m_zoomSlider->setToolTip(i18n("Zoom Level: %1/13", (13 - zoomlevel)));
}

void MainWindow::slotGotProgressInfo(const QString &message, int progress)
{
    m_statusProgressBar->setValue(progress);
    if (progress >= 0) {
        if (!message.isEmpty())
            m_messageLabel->setMessage(message, InformationMessage);//statusLabel->setText(message);
        m_statusProgressBar->setVisible(true);
    } else if (progress == -2) {
        if (!message.isEmpty())
            m_messageLabel->setMessage(message, ErrorMessage);
        m_statusProgressBar->setVisible(false);
    } else {
        m_messageLabel->setMessage(QString(), DefaultMessage);
        m_statusProgressBar->setVisible(false);
    }
}

void MainWindow::slotShowClipProperties(DocClipBase *clip)
{
    if (clip->clipType() == TEXT) {
        QString titlepath = m_activeDocument->projectFolder().path(KUrl::AddTrailingSlash) + "titles/";
        if (!clip->getProperty("resource").isEmpty() && clip->getProperty("xmldata").isEmpty()) {
            // template text clip

            // Get the list of existing templates
            QStringList filter;
            filter << "*.kdenlivetitle";
            QStringList templateFiles = QDir(titlepath).entryList(filter, QDir::Files);

            QDialog *dia = new QDialog(this);
            Ui::TemplateClip_UI dia_ui;
            dia_ui.setupUi(dia);
            int ix = -1;
            const QString templatePath = clip->getProperty("resource");
            for (int i = 0; i < templateFiles.size(); ++i) {
                dia_ui.template_list->comboBox()->addItem(templateFiles.at(i), titlepath + templateFiles.at(i));
                if (templatePath == KUrl(titlepath + templateFiles.at(i)).path()) ix = i;
            }
            if (ix != -1) dia_ui.template_list->comboBox()->setCurrentIndex(ix);
            else dia_ui.template_list->comboBox()->insertItem(0, templatePath);
            dia_ui.template_list->fileDialog()->setFilter("*.kdenlivetitle");
            //warning: setting base directory doesn't work??
            KUrl startDir(titlepath);
            dia_ui.template_list->fileDialog()->setUrl(startDir);
            dia_ui.description->setText(clip->getProperty("description"));
            if (dia->exec() == QDialog::Accepted) {
                QString textTemplate = dia_ui.template_list->comboBox()->itemData(dia_ui.template_list->comboBox()->currentIndex()).toString();
                if (textTemplate.isEmpty()) textTemplate = dia_ui.template_list->comboBox()->currentText();

                QMap <QString, QString> newprops;

                if (KUrl(textTemplate).path() != templatePath) {
                    // The template was changed
                    newprops.insert("resource", textTemplate);
                }

                if (dia_ui.description->toPlainText() != clip->getProperty("description")) {
                    newprops.insert("description", dia_ui.description->toPlainText());
                }

                QString newtemplate = newprops.value("xmltemplate");
                if (newtemplate.isEmpty()) newtemplate = templatePath;

                // template modified we need to update xmldata
                QString description = newprops.value("description");
                if (description.isEmpty()) description = clip->getProperty("description");
                else newprops.insert("templatetext", description);
                //newprops.insert("xmldata", m_projectList->generateTemplateXml(newtemplate, description).toString());
                if (!newprops.isEmpty()) {
                    EditClipCommand *command = new EditClipCommand(m_projectList, clip->getId(), clip->currentProperties(newprops), newprops, true);
                    m_activeDocument->commandStack()->push(command);
                }
            }
            delete dia;
            return;
        }
        QString path = clip->getProperty("resource");
        QPointer<TitleWidget> dia_ui = new TitleWidget(KUrl(), m_activeDocument->timecode(), titlepath, m_projectMonitor->render, this);
        QDomDocument doc;
        doc.setContent(clip->getProperty("xmldata"));
        dia_ui->setXml(doc);
        if (dia_ui->exec() == QDialog::Accepted) {
            QMap <QString, QString> newprops;
            newprops.insert("xmldata", dia_ui->xml().toString());
            if (dia_ui->outPoint() != clip->duration().frames(m_activeDocument->fps())) {
                // duration changed, we need to update duration
                newprops.insert("out", QString::number(dia_ui->outPoint()));
                int currentLength = QString(clip->producerProperty("length")).toInt();
                if (currentLength <= dia_ui->outPoint())
                        newprops.insert("length", QString::number(dia_ui->outPoint() + 1));
                else newprops.insert("length", clip->producerProperty("length"));
            }
            if (!path.isEmpty()) {
                // we are editing an external file, asked if we want to detach from that file or save the result to that title file.
                if (KMessageBox::questionYesNo(this, i18n("You are editing an external title clip (%1). Do you want to save your changes to the title file or save the changes for this project only?", path), i18n("Save Title"), KGuiItem(i18n("Save to title file")), KGuiItem(i18n("Save in project only"))) == KMessageBox::Yes) {
                    // save to external file
                    dia_ui->saveTitle(path);
                } else newprops.insert("resource", QString());
            }
            EditClipCommand *command = new EditClipCommand(m_projectList, clip->getId(), clip->currentProperties(newprops), newprops, true);
            m_activeDocument->commandStack()->push(command);
            //m_activeTimeline->projectView()->slotUpdateClip(clip->getId());
            m_activeDocument->setModified(true);
        }
        delete dia_ui;

        //m_activeDocument->editTextClip(clip->getProperty("xml"), clip->getId());
        return;
    }

    // any type of clip but a title
    ClipProperties *dia = new ClipProperties(clip, m_activeDocument->timecode(), m_activeDocument->fps(), this);

    if (clip->clipType() == AV || clip->clipType() == VIDEO || clip->clipType() == PLAYLIST) {
	// request clip thumbnails
	m_activeDocument->clipManager()->requestThumbs(QString('?' + clip->getId()), QList<int>() << clip->getClipThumbFrame());
	connect(m_activeDocument->clipManager(), SIGNAL(gotClipPropertyThumbnail(const QString&,QImage)), dia, SLOT(slotGotThumbnail(const QString&,QImage)));
    }
    
    connect(dia, SIGNAL(addMarkers(const QString &, QList <CommentedTime>)), m_activeTimeline->projectView(), SLOT(slotAddClipMarker(const QString &, QList <CommentedTime>)));
    connect(dia, SIGNAL(editAnalysis(QString,QString,QString)), m_activeTimeline->projectView(), SLOT(slotAddClipExtraData(QString,QString,QString)));
    connect(m_activeTimeline->projectView(), SIGNAL(updateClipMarkers(DocClipBase *)), dia, SLOT(slotFillMarkersList(DocClipBase *)));
    connect(m_activeTimeline->projectView(), SIGNAL(updateClipExtraData(DocClipBase *)), dia, SLOT(slotUpdateAnalysisData(DocClipBase *)));
    connect(m_projectList, SIGNAL(updateAnalysisData(DocClipBase *)), dia, SLOT(slotUpdateAnalysisData(DocClipBase *)));
    connect(dia, SIGNAL(loadMarkers(const QString &)), m_activeTimeline->projectView(), SLOT(slotLoadClipMarkers(const QString &)));
    connect(dia, SIGNAL(saveMarkers(const QString &)), m_activeTimeline->projectView(), SLOT(slotSaveClipMarkers(const QString &)));
    connect(dia, SIGNAL(deleteProxy(const QString)), m_projectList, SLOT(slotDeleteProxy(const QString)));
    connect(dia, SIGNAL(applyNewClipProperties(const QString, QMap <QString, QString> , QMap <QString, QString> , bool, bool)), this, SLOT(slotApplyNewClipProperties(const QString, QMap <QString, QString> , QMap <QString, QString> , bool, bool)));
    dia->show();
}


void MainWindow::slotApplyNewClipProperties(const QString id, QMap <QString, QString> props, QMap <QString, QString> newprops, bool refresh, bool reload)
{
    if (newprops.isEmpty()) return;
    EditClipCommand *command = new EditClipCommand(m_projectList, id, props, newprops, true);
    m_activeDocument->commandStack()->push(command);
    m_activeDocument->setModified();

    if (refresh) {
        // update clip occurences in timeline
        m_activeTimeline->projectView()->slotUpdateClip(id, reload);
    }
}


void MainWindow::slotShowClipProperties(QList <DocClipBase *> cliplist, QMap<QString, QString> commonproperties)
{
    QPointer<ClipProperties> dia = new ClipProperties(cliplist,
                         m_activeDocument->timecode(), commonproperties, this);
    if (dia->exec() == QDialog::Accepted) {
        QUndoCommand *command = new QUndoCommand();
        command->setText(i18n("Edit clips"));
        QMap <QString, QString> newImageProps = dia->properties();
        // Transparency setting applies only for images
        QMap <QString, QString> newProps = newImageProps;
        newProps.remove("transparency");

        for (int i = 0; i < cliplist.count(); i++) {
            DocClipBase *clip = cliplist.at(i);
            if (clip->clipType() == IMAGE)
                new EditClipCommand(m_projectList, clip->getId(), clip->currentProperties(newImageProps), newImageProps, true, command);
            else
                new EditClipCommand(m_projectList, clip->getId(), clip->currentProperties(newProps), newProps, true, command);
        }
        m_activeDocument->commandStack()->push(command);
        for (int i = 0; i < cliplist.count(); i++)
            m_activeTimeline->projectView()->slotUpdateClip(cliplist.at(i)->getId(), dia->needsTimelineReload());
    }
    delete dia;
}

void MainWindow::customEvent(QEvent* e)
{
    if (e->type() == QEvent::User)
        m_messageLabel->setMessage(static_cast <MltErrorEvent *>(e)->message(), MltError);
}

void MainWindow::slotTimelineClipSelected(ClipItem* item, bool raise)
{
    if (item != m_mainClip) {
	if (m_mainClip) m_mainClip->setMainSelectedClip(false);
	if (item) item->setMainSelectedClip(true);
	m_mainClip = item;
    }
    m_effectStack->slotClipItemSelected(item);
    m_projectMonitor->slotSetSelectedClip(item);
    if (raise)
        m_effectStack->raiseWindow(m_effectStackDock);
}

void MainWindow::slotTrackSelected(int index, TrackInfo info, bool raise)
{
    m_effectStack->slotTrackItemSelected(index, info);
    if (raise)
        m_effectStack->raiseWindow(m_effectStackDock);
}

void MainWindow::slotActivateTransitionView(Transition *t)
{
    if (t)
        m_transitionConfig->raiseWindow(m_transitionConfigDock);
}

void MainWindow::slotSnapRewind()
{
    if (m_projectMonitor->isActive()) {
        if (m_activeTimeline)
            m_activeTimeline->projectView()->slotSeekToPreviousSnap();
    } else  {
        m_clipMonitor->slotSeekToPreviousSnap();
    }
}

void MainWindow::slotSnapForward()
{
    if (m_projectMonitor->isActive()) {
        if (m_activeTimeline)
            m_activeTimeline->projectView()->slotSeekToNextSnap();
    } else {
        m_clipMonitor->slotSeekToNextSnap();
    }
}

void MainWindow::slotClipStart()
{
    if (m_projectMonitor->isActive()) {
        if (m_activeTimeline)
            m_activeTimeline->projectView()->clipStart();
    }
}

void MainWindow::slotClipEnd()
{
    if (m_projectMonitor->isActive()) {
        if (m_activeTimeline)
            m_activeTimeline->projectView()->clipEnd();
    }
}

void MainWindow::slotZoneStart()
{
    if (m_projectMonitor->isActive())
        m_projectMonitor->slotZoneStart();
    else
        m_clipMonitor->slotZoneStart();
}

void MainWindow::slotZoneEnd()
{
    if (m_projectMonitor->isActive())
        m_projectMonitor->slotZoneEnd();
    else
        m_clipMonitor->slotZoneEnd();
}

void MainWindow::slotChangeTool(QAction * action)
{
    if (action == m_buttonSelectTool)
        slotSetTool(SELECTTOOL);
    else if (action == m_buttonRazorTool)
        slotSetTool(RAZORTOOL);
    else if (action == m_buttonSpacerTool)
        slotSetTool(SPACERTOOL);
}

void MainWindow::slotChangeEdit(QAction * action)
{
    if (!m_activeTimeline)
        return;

    if (action == m_overwriteEditTool)
        m_activeTimeline->projectView()->setEditMode(OVERWRITEEDIT);
    else if (action == m_insertEditTool)
        m_activeTimeline->projectView()->setEditMode(INSERTEDIT);
    else
        m_activeTimeline->projectView()->setEditMode(NORMALEDIT);
}

void MainWindow::slotSetTool(PROJECTTOOL tool)
{
    if (m_activeDocument && m_activeTimeline) {
        //m_activeDocument->setTool(tool);
        QString message;
        switch (tool)  {
        case SPACERTOOL:
            message = i18n("Ctrl + click to use spacer on current track only");
            break;
        case RAZORTOOL:
            message = i18n("Click on a clip to cut it");
            break;
        default:
            message = i18n("Shift + click to create a selection rectangle, Ctrl + click to add an item to selection");
            break;
        }
        m_messageLabel->setMessage(message, InformationMessage);
        m_activeTimeline->projectView()->setTool(tool);
    }
}

void MainWindow::slotCopy()
{
    if (m_activeDocument && m_activeTimeline)
        m_activeTimeline->projectView()->copyClip();
}

void MainWindow::slotPaste()
{
    if (m_activeDocument && m_activeTimeline)
        m_activeTimeline->projectView()->pasteClip();
}

void MainWindow::slotPasteEffects()
{
    if (m_activeDocument && m_activeTimeline)
        m_activeTimeline->projectView()->pasteClipEffects();
}

void MainWindow::slotFind()
{
    if (!m_activeDocument || !m_activeTimeline) return;
    m_projectSearch->setEnabled(false);
    m_findActivated = true;
    m_findString.clear();
    m_activeTimeline->projectView()->initSearchStrings();
    statusBar()->showMessage(i18n("Starting -- find text as you type"));
    m_findTimer.start(5000);
    qApp->installEventFilter(this);
}

void MainWindow::slotFindNext()
{
    if (m_activeTimeline && m_activeTimeline->projectView()->findNextString(m_findString))
        statusBar()->showMessage(i18n("Found: %1", m_findString));
    else
        statusBar()->showMessage(i18n("Reached end of project"));
    m_findTimer.start(4000);
}

void MainWindow::findAhead()
{
    if (m_activeTimeline && m_activeTimeline->projectView()->findString(m_findString)) {
        m_projectSearchNext->setEnabled(true);
        statusBar()->showMessage(i18n("Found: %1", m_findString));
    } else {
        m_projectSearchNext->setEnabled(false);
        statusBar()->showMessage(i18n("Not found: %1", m_findString));
    }
}

void MainWindow::findTimeout()
{
    m_projectSearchNext->setEnabled(false);
    m_findActivated = false;
    m_findString.clear();
    statusBar()->showMessage(i18n("Find stopped"), 3000);
    if (m_activeTimeline) m_activeTimeline->projectView()->clearSearchStrings();
    m_projectSearch->setEnabled(true);
    removeEventFilter(this);
}

void MainWindow::slotClipInTimeline(const QString &clipId)
{
    if (m_activeTimeline && m_activeDocument) {
        QList<ItemInfo> matching = m_activeTimeline->projectView()->findId(clipId);

        QMenu *inTimelineMenu = static_cast<QMenu*>(factory()->container("clip_in_timeline", this));
        inTimelineMenu->clear();

        QList <QAction *> actionList;

        for (int i = 0; i < matching.count(); ++i) {
            QString track = QString::number(matching.at(i).track);
            QString start = m_activeDocument->timecode().getTimecode(matching.at(i).startPos);
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

        if (matching.empty())
            inTimelineMenu->setEnabled(false);
        else
            inTimelineMenu->setEnabled(true);
    }
}

void MainWindow::slotClipInProjectTree()
{
    if (m_activeTimeline) {
        const QStringList &clipIds = m_activeTimeline->projectView()->selectedClips();
        if (clipIds.isEmpty())
            return;
        m_projectListDock->raise();
        for (int i = 0; i < clipIds.count(); i++)
            m_projectList->selectItemById(clipIds.at(i));
        if (m_projectMonitor->isActive())
            slotSwitchMonitors();
    }
}

/*void MainWindow::slotClipToProjectTree()
{
    if (m_activeTimeline) {
    const QList<ClipItem *> clips =  m_activeTimeline->projectView()->selectedClipItems();
        if (clips.isEmpty()) return;
        for (int i = 0; i < clips.count(); i++) {
        m_projectList->slotAddXmlClip(clips.at(i)->itemXml());
        }
        //m_projectList->selectItemById(clipIds.at(i));
    }
}*/

void MainWindow::slotSelectClipInTimeline()
{
    if (m_activeTimeline) {
        QAction *action = qobject_cast<QAction *>(sender());
        QStringList data = action->data().toStringList();
        m_activeTimeline->projectView()->selectFound(data.at(0), data.at(1));
    }
}

void MainWindow::keyPressEvent(QKeyEvent *ke)
{
    if (m_findActivated) {
        if (ke->key() == Qt::Key_Backspace) {
            m_findString = m_findString.left(m_findString.length() - 1);

            if (!m_findString.isEmpty())
                findAhead();
            else
                findTimeout();

            m_findTimer.start(4000);
            ke->accept();
            return;
        } else if (ke->key() == Qt::Key_Escape) {
            findTimeout();
            ke->accept();
            return;
        } else if (ke->key() == Qt::Key_Space || !ke->text().trimmed().isEmpty()) {
            m_findString += ke->text();

            findAhead();

            m_findTimer.start(4000);
            ke->accept();
            return;
        }
    } else {
        KXmlGuiWindow::keyPressEvent(ke);
    }
}


/** Gets called when the window gets hidden */
void MainWindow::hideEvent(QHideEvent */*event*/)
{
    if (isMinimized() && m_monitorManager)
        m_monitorManager->stopActiveMonitor();
}

bool MainWindow::eventFilter(QObject *obj, QEvent *event)
{
    if (m_findActivated) {
        if (event->type() == QEvent::ShortcutOverride) {
            QKeyEvent* ke = (QKeyEvent*) event;
            if (ke->text().trimmed().isEmpty()) return false;
            ke->accept();
            return true;
        } else {
            return false;
        }
    } else {
        // pass the event on to the parent class
        return QMainWindow::eventFilter(obj, event);
    }
}


void MainWindow::slotSaveZone(Render *render, QPoint zone, DocClipBase *baseClip, KUrl path)
{
    KDialog *dialog = new KDialog(this);
    dialog->setCaption("Save clip zone");
    dialog->setButtons(KDialog::Ok | KDialog::Cancel);

    QWidget *widget = new QWidget(dialog);
    dialog->setMainWidget(widget);

    QVBoxLayout *vbox = new QVBoxLayout(widget);
    QLabel *label1 = new QLabel(i18n("Save clip zone as:"), this);
    if (path.isEmpty()) {
        QString tmppath = m_activeDocument->projectFolder().path(KUrl::AddTrailingSlash);
        if (baseClip == NULL) tmppath.append("untitled.mlt");
        else {
            tmppath.append((baseClip->name().isEmpty() ? baseClip->fileURL().fileName() : baseClip->name()) + '-' + QString::number(zone.x()).rightJustified(4, '0') + ".mlt");
        }
        path = KUrl(tmppath);
    }
    KUrlRequester *url = new KUrlRequester(path, this);
    url->setFilter("video/mlt-playlist");
    QLabel *label2 = new QLabel(i18n("Description:"), this);
    KLineEdit *edit = new KLineEdit(this);
    vbox->addWidget(label1);
    vbox->addWidget(url);
    vbox->addWidget(label2);
    vbox->addWidget(edit);
    if (dialog->exec() == QDialog::Accepted) {
        if (QFile::exists(url->url().path())) {
            if (KMessageBox::questionYesNo(this, i18n("File %1 already exists.\nDo you want to overwrite it?", url->url().path())) == KMessageBox::No) {
                slotSaveZone(render, zone, baseClip, url->url());
                return;
            }
        }
        if (baseClip && !baseClip->fileURL().isEmpty()) {
            // create zone from clip url, so that we don't have problems with proxy clips
            QProcess p;
#if QT_VERSION >= 0x040600
            QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
            env.remove("MLT_PROFILE");
            p.setProcessEnvironment(env);
#else
            QStringList env = QProcess::systemEnvironment();
            env << "MLT_PROFILE='\0'";
            p.setEnvironment(env);
#endif
            p.start(KdenliveSettings::rendererpath(), QStringList() << baseClip->fileURL().path() << "in=" + QString::number(zone.x()) << "out=" + QString::number(zone.y()) << "-consumer" << "xml:" + url->url().path());
            if (!p.waitForStarted(3000)) {
                KMessageBox::sorry(this, i18n("Cannot start MLT's renderer:\n%1", KdenliveSettings::rendererpath()));
            }
            else if (!p.waitForFinished(5000)) {
                KMessageBox::sorry(this, i18n("Timeout while creating xml output"));
            }
        }
        else render->saveZone(url->url(), edit->text(), zone);
    }

}

void MainWindow::slotSetInPoint()
{
    if (m_clipMonitor->isActive()) {
        m_clipMonitor->slotSetZoneStart();
    } else {
        m_projectMonitor->slotSetZoneStart();
    }
}

void MainWindow::slotSetOutPoint()
{
    if (m_clipMonitor->isActive()) {
        m_clipMonitor->slotSetZoneEnd();
    } else {
        m_projectMonitor->slotSetZoneEnd();
    }
}

void MainWindow::slotResizeItemStart()
{
    if (m_activeTimeline)
        m_activeTimeline->projectView()->setInPoint();
}

void MainWindow::slotResizeItemEnd()
{
    if (m_activeTimeline)
        m_activeTimeline->projectView()->setOutPoint();
}

int MainWindow::getNewStuff(const QString &configFile)
{
    KNS3::Entry::List entries;
#if KDE_IS_VERSION(4,3,80)
    QPointer<KNS3::DownloadDialog> dialog = new KNS3::DownloadDialog(configFile);
    dialog->exec();
    if (dialog) entries = dialog->changedEntries();
    foreach(const KNS3::Entry & entry, entries) {
        if (entry.status() == KNS3::Entry::Installed)
            kDebug() << "// Installed files: " << entry.installedFiles();
    }
    delete dialog;
#else
    KNS::Engine engine(0);
    if (engine.init(configFile))
        entries = engine.downloadDialogModal(this);
    foreach(KNS::Entry * entry, entries) {
        if (entry->status() == KNS::Entry::Installed)
            kDebug() << "// Installed files: " << entry->installedFiles();
    }
#endif
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
        m_activeTimeline->projectView()->reloadTransitionLumas();
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
    if (m_activeTimeline)
        m_activeTimeline->projectView()->autoTransition();
}

void MainWindow::slotSplitAudio()
{
    if (m_activeTimeline)
        m_activeTimeline->projectView()->splitAudio();
}

void MainWindow::slotSetAudioAlignReference()
{
    if (m_activeTimeline) {
        m_activeTimeline->projectView()->setAudioAlignReference();
    }
}

void MainWindow::slotAlignAudio()
{
    if (m_activeTimeline) {
        m_activeTimeline->projectView()->alignAudio();
    }
}

void MainWindow::slotUpdateClipType(QAction *action)
{
    if (m_activeTimeline) {
        if (action->data().toString() == "clip_audio_only") m_activeTimeline->projectView()->setAudioOnly();
        else if (action->data().toString() == "clip_video_only") m_activeTimeline->projectView()->setVideoOnly();
        else m_activeTimeline->projectView()->setAudioAndVideo();
    }
}

void MainWindow::slotDvdWizard(const QString &url)
{
    // We must stop the monitors since we create a new on in the dvd wizard
    m_clipMonitor->stop();
    m_projectMonitor->stop();
    QPointer<DvdWizard> w = new DvdWizard(url, this);
    w->exec();
    m_projectMonitor->start();
    delete w;
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

void MainWindow::slotMaximizeCurrent(bool)
{
    //TODO: is there a way to maximize current widget?

    m_timelineState = saveState();
    QWidget *par = focusWidget()->parentWidget();
    while (par->parentWidget() && par->parentWidget() != this)
        par = par->parentWidget();
    kDebug() << "CURRENT WIDGET: " << par->objectName();
}

void MainWindow::loadClipActions()
{
	QMenu* actionMenu= static_cast<QMenu*>(factory()->container("clip_actions", this));
	if (actionMenu){
		actionMenu->clear();
		Mlt::Profile profile;
		Mlt::Filter *filter = Mlt::Factory::filter(profile,(char*)"videostab");
		if (filter) {
			delete filter;
			QAction *action=actionMenu->addAction(i18n("Stabilize (vstab)"));
			action->setData("videostab");
			connect(action,SIGNAL(triggered()), this, SLOT(slotStartClipAction()));
		}
		filter = Mlt::Factory::filter(profile,(char*)"videostab2");
		if (filter) {
			delete filter;
			QAction *action=actionMenu->addAction(i18n("Stabilize (transcode)"));
			action->setData("videostab2");
			connect(action,SIGNAL(triggered()), this, SLOT(slotStartClipAction()));
		}
		filter = Mlt::Factory::filter(profile,(char*)"motion_est");
		if (filter) {
			delete filter;
			QAction *action=actionMenu->addAction(i18n("Automatic scene split"));
			action->setData("motion_est");
			connect(action,SIGNAL(triggered()), this, SLOT(slotStartClipAction()));
		}
	}

}

void MainWindow::loadTranscoders()
{
    QMenu *transMenu = static_cast<QMenu*>(factory()->container("transcoders", this));
    transMenu->clear();

    QMenu *extractAudioMenu = static_cast<QMenu*>(factory()->container("extract_audio", this));
    extractAudioMenu->clear();

    KSharedConfigPtr config = KSharedConfig::openConfig("kdenlivetranscodingrc", KConfig::CascadeConfig);
    KConfigGroup transConfig(config, "Transcoding");
    // read the entries
    QMap< QString, QString > profiles = transConfig.entryMap();
    QMapIterator<QString, QString> i(profiles);
    while (i.hasNext()) {
        i.next();
        QStringList data = i.value().split(';');
        QAction *a;
        // separate audio transcoding in a separate menu
        if (data.count() > 2 && data.at(2) == "audio") {
            a = extractAudioMenu->addAction(i.key());
        }
        else {
            a = transMenu->addAction(i.key());
        }
        a->setData(data);
        if (data.count() > 1) a->setToolTip(data.at(1));
        connect(a, SIGNAL(triggered()), this, SLOT(slotTranscode()));
    }
}

void MainWindow::slotStartClipAction()
{
    QString condition,filtername;
    QStringList ids;

    // Stablize selected clips
    QAction *action = qobject_cast<QAction *>(sender());
    if (action){
        filtername=action->data().toString();
    }
    m_projectList->startClipFilterJob(filtername, condition);
    /*
    if (ids.isEmpty()) {
        m_messageLabel->setMessage(i18n("No clip to transcode"), ErrorMessage);
        return;
    }
    QString destination;
    ProjectItem *item = m_projectList->getClipById(ids.at(0));
    if (ids.count() == 1) {

    }
    ClipStabilize *d = new ClipStabilize(destination, ids.count(), filtername);
    //connect(d, SIGNAL(addClip(KUrl)), this, SLOT(slotAddProjectClip(KUrl)));
    if (d->exec() == QDialog::Accepted) {
        m_projectList->slotStabilizeClipJob(ids, d->autoAddClip(), d->params(), d->desc());
    }
    delete d;*/
}

void MainWindow::slotTranscode(KUrl::List urls)
{
    QString params;
    QString desc;
    QString condition;
    if (urls.isEmpty()) {
        QAction *action = qobject_cast<QAction *>(sender());
        QStringList data = action->data().toStringList();
        params = data.at(0);
        if (data.count() > 1) desc = data.at(1);
        if (data.count() > 3) condition = data.at(3);
        m_projectList->slotTranscodeClipJob(condition, params, desc);
        return;
    }
    if (urls.isEmpty()) {
        m_messageLabel->setMessage(i18n("No clip to transcode"), ErrorMessage);
        return;
    }
    ClipTranscode *d = new ClipTranscode(urls, params, QStringList(), desc);
    connect(d, SIGNAL(addClip(KUrl)), this, SLOT(slotAddProjectClip(KUrl)));
    d->show();
}

void MainWindow::slotTranscodeClip()
{
    KUrl::List urls = KFileDialog::getOpenUrls(KUrl("kfiledialog:///projectfolder"));
    if (urls.isEmpty()) return;
    slotTranscode(urls);
}

void MainWindow::slotSetDocumentRenderProfile(QMap <QString, QString> props)
{
    if (m_activeDocument == NULL) return;
    QMapIterator<QString, QString> i(props);
    while (i.hasNext()) {
        i.next();
        m_activeDocument->setDocumentProperty(i.key(), i.value());
    }
    m_activeDocument->setModified(true);
}


void MainWindow::slotPrepareRendering(bool scriptExport, bool zoneOnly, const QString &chapterFile)
{
    if (m_activeDocument == NULL || m_renderWidget == NULL) return;
    QString scriptPath;
    QString playlistPath;
    if (scriptExport) {
        bool ok;
        QString scriptsFolder = m_activeDocument->projectFolder().path(KUrl::AddTrailingSlash) + "scripts/";
        QString path = m_renderWidget->getFreeScriptName();
        scriptPath = QInputDialog::getText(this, i18n("Create Render Script"), i18n("Script name (will be saved in: %1)", scriptsFolder), QLineEdit::Normal, KUrl(path).fileName(), &ok);
        if (!ok || scriptPath.isEmpty()) return;
        scriptPath.prepend(scriptsFolder);
        QFile f(scriptPath);
        if (f.exists()) {
            if (KMessageBox::warningYesNo(this, i18n("Script file already exists. Do you want to overwrite it?")) != KMessageBox::Yes)
                return;
        }
        playlistPath = scriptPath + ".mlt";
    } else {
        KTemporaryFile temp;
        temp.setAutoRemove(false);
        temp.setSuffix(".mlt");
        temp.open();
        playlistPath = temp.fileName();
    }
    QString playlistContent = m_projectMonitor->sceneList();
    if (!chapterFile.isEmpty()) {
        int in = 0;
        int out;
        if (!zoneOnly) out = (int) GenTime(m_activeDocument->projectDuration()).frames(m_activeDocument->fps());
        else {
            in = m_activeTimeline->inPoint();
            out = m_activeTimeline->outPoint();
        }
        QDomDocument doc;
        QDomElement chapters = doc.createElement("chapters");
        chapters.setAttribute("fps", m_activeDocument->fps());
        doc.appendChild(chapters);

        QDomElement guidesxml = m_activeDocument->guidesXml();
        QDomNodeList nodes = guidesxml.elementsByTagName("guide");
        for (int i = 0; i < nodes.count(); i++) {
            QDomElement e = nodes.item(i).toElement();
            if (!e.isNull()) {
                QString comment = e.attribute("comment");
                int time = (int) GenTime(e.attribute("time").toDouble()).frames(m_activeDocument->fps());
                if (time >= in && time < out) {
                    if (zoneOnly) time = time - in;
                    QDomElement chapter = doc.createElement("chapter");
                    chapters.appendChild(chapter);
                    chapter.setAttribute("title", comment);
                    chapter.setAttribute("time", time);
                }
            }
        }
        if (chapters.childNodes().count() > 0) {
            if (m_activeTimeline->projectView()->hasGuide(out, 0) == -1) {
                // Always insert a guide in pos 0
                QDomElement chapter = doc.createElement("chapter");
                chapters.insertBefore(chapter, QDomNode());
                chapter.setAttribute("title", i18n("Start"));
                chapter.setAttribute("time", "0");
            }
            // save chapters file
            QFile file(chapterFile);
            if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
                kWarning() << "//////  ERROR writing DVD CHAPTER file: " << chapterFile;
            } else {
                file.write(doc.toString().toUtf8());
                if (file.error() != QFile::NoError) {
                    kWarning() << "//////  ERROR writing DVD CHAPTER file: " << chapterFile;
                }
                file.close();
            }
        }
    }
    bool exportAudio;
    if (m_renderWidget->automaticAudioExport()) {
        exportAudio = m_activeTimeline->checkProjectAudio();
    } else exportAudio = m_renderWidget->selectedAudioExport();

    // Set playlist audio volume to 100%
    QDomDocument doc;
    doc.setContent(playlistContent);
    QDomElement tractor = doc.documentElement().firstChildElement("tractor");
    if (!tractor.isNull()) {
        QDomNodeList props = tractor.elementsByTagName("property");
        for (int i = 0; i < props.count(); i++) {
            if (props.at(i).toElement().attribute("name") == "meta.volume") {
                props.at(i).firstChild().setNodeValue("1");
                break;
            }
        }
    }

    // Do we want proxy rendering
    if (m_projectList->useProxy() && !m_renderWidget->proxyRendering()) {
        QString root = doc.documentElement().attribute("root");

        // replace proxy clips with originals
        QMap <QString, QString> proxies = m_projectList->getProxies();

        QDomNodeList producers = doc.elementsByTagName("producer");
        QString producerResource;
        QString suffix;
        for (uint n = 0; n < producers.length(); n++) {
            QDomElement e = producers.item(n).toElement();
            producerResource = EffectsList::property(e, "resource");
            if (producerResource.isEmpty()) continue;
            if (!producerResource.startsWith('/')) {
                producerResource.prepend(root + '/');
            }
            if (producerResource.contains('?')) {
                // slowmotion producer
                suffix = '?' + producerResource.section('?', 1);
                producerResource = producerResource.section('?', 0, 0);
            }
            else suffix.clear();
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

        /*QMapIterator<QString, QString> i(proxies);
        while (i.hasNext()) {
            i.next();
            // Replace all keys with their values (proxy path with original path)
            QString key = i.key();
            playlistContent.replace(key, i.value());
            if (!root.isEmpty() && key.startsWith(root)) {
                // in case the resource path in MLT playlist is relative
                key.remove(0, root.count() + 1);
                playlistContent.replace(key, i.value());
            }
        }*/
    }
    playlistContent = doc.toString();

    // Do save scenelist
    QFile file(playlistPath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        m_messageLabel->setMessage(i18n("Cannot write to file %1", playlistPath), ErrorMessage);
        return;
    }
    file.write(playlistContent.toUtf8());
    if (file.error() != QFile::NoError) {
        m_messageLabel->setMessage(i18n("Cannot write to file %1", playlistPath), ErrorMessage);
        file.close();
        return;
    }
    file.close();
    m_renderWidget->slotExport(scriptExport, m_activeTimeline->inPoint(), m_activeTimeline->outPoint(), m_activeDocument->metadata(), playlistPath, scriptPath, exportAudio);
}

void MainWindow::slotUpdateTimecodeFormat(int ix)
{
    KdenliveSettings::setFrametimecode(ix == 1);
    m_clipMonitor->updateTimecodeFormat();
    m_projectMonitor->updateTimecodeFormat();
    m_transitionConfig->updateTimecodeFormat();
    m_effectStack->updateTimecodeFormat();
    //m_activeTimeline->projectView()->clearSelection();
    m_activeTimeline->updateRuler();
}

void MainWindow::slotRemoveFocus()
{
    statusBar()->setFocus();
    statusBar()->clearFocus();
}

void MainWindow::slotRevert()
{
    if (KMessageBox::warningContinueCancel(this, i18n("This will delete all changes made since you last saved your project. Are you sure you want to continue?"), i18n("Revert to last saved version")) == KMessageBox::Cancel) return;
    KUrl url = m_activeDocument->url();
    if (closeCurrentDocument(false))
        doOpenFile(url, NULL);
}


void MainWindow::slotShutdown()
{
    if (m_activeDocument) m_activeDocument->setModified(false);
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
    if (m_activeDocument)
        m_transitionConfig->updateProjectFormat(m_activeDocument->mltProfile(), m_activeDocument->timecode(), m_activeDocument->tracksList());
}

void MainWindow::slotChangePalette(QAction *action, const QString &themename)
{
    // Load the theme file
    QString theme;
    if (action == NULL) theme = themename;
    else theme = action->data().toString();
    KdenliveSettings::setColortheme(theme);
    // Make palette for all widgets.
    QPalette plt = kapp->palette();
    if (theme.isEmpty()) {
        plt = QApplication::desktop()->palette();
    } else {
        KSharedConfigPtr config = KSharedConfig::openConfig(theme);

#if KDE_IS_VERSION(4,6,3)
        plt = KGlobalSettings::createNewApplicationPalette(config);
#else
        // Since there was a bug in createApplicationPalette in KDE < 4.6.3 we need
        // to do the palette loading stuff ourselves. (https://bugs.kde.org/show_bug.cgi?id=263497)
        QPalette::ColorGroup states[3] = { QPalette::Active, QPalette::Inactive,
                                            QPalette::Disabled };
        // TT thinks tooltips shouldn't use active, so we use our active colors for all states
        KColorScheme schemeTooltip(QPalette::Active, KColorScheme::Tooltip, config);

        for ( int i = 0; i < 3 ; i++ ) {
            QPalette::ColorGroup state = states[i];
            KColorScheme schemeView(state, KColorScheme::View, config);
            KColorScheme schemeWindow(state, KColorScheme::Window, config);
            KColorScheme schemeButton(state, KColorScheme::Button, config);
            KColorScheme schemeSelection(state, KColorScheme::Selection, config);

            plt.setBrush( state, QPalette::WindowText, schemeWindow.foreground() );
            plt.setBrush( state, QPalette::Window, schemeWindow.background() );
            plt.setBrush( state, QPalette::Base, schemeView.background() );
            plt.setBrush( state, QPalette::Text, schemeView.foreground() );
            plt.setBrush( state, QPalette::Button, schemeButton.background() );
            plt.setBrush( state, QPalette::ButtonText, schemeButton.foreground() );
            plt.setBrush( state, QPalette::Highlight, schemeSelection.background() );
            plt.setBrush( state, QPalette::HighlightedText, schemeSelection.foreground() );
            plt.setBrush( state, QPalette::ToolTipBase, schemeTooltip.background() );
            plt.setBrush( state, QPalette::ToolTipText, schemeTooltip.foreground() );

            plt.setColor( state, QPalette::Light, schemeWindow.shade( KColorScheme::LightShade ) );
            plt.setColor( state, QPalette::Midlight, schemeWindow.shade( KColorScheme::MidlightShade ) );
            plt.setColor( state, QPalette::Mid, schemeWindow.shade( KColorScheme::MidShade ) );
            plt.setColor( state, QPalette::Dark, schemeWindow.shade( KColorScheme::DarkShade ) );
            plt.setColor( state, QPalette::Shadow, schemeWindow.shade( KColorScheme::ShadowShade ) );

            plt.setBrush( state, QPalette::AlternateBase, schemeView.background( KColorScheme::AlternateBackground) );
            plt.setBrush( state, QPalette::Link, schemeView.foreground( KColorScheme::LinkText ) );
            plt.setBrush( state, QPalette::LinkVisited, schemeView.foreground( KColorScheme::VisitedText ) );
        }
#endif
    }

    kapp->setPalette(plt);
    slotChangePalette();
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


QPixmap MainWindow::createSchemePreviewIcon(const KSharedConfigPtr &config)
{
    // code taken from kdebase/workspace/kcontrol/colors/colorscm.cpp
    const uchar bits1[] = { 0xff, 0xff, 0xff, 0x2c, 0x16, 0x0b };
    const uchar bits2[] = { 0x68, 0x34, 0x1a, 0xff, 0xff, 0xff };
    const QSize bitsSize(24, 2);
    const QBitmap b1 = QBitmap::fromData(bitsSize, bits1);
    const QBitmap b2 = QBitmap::fromData(bitsSize, bits2);

    QPixmap pixmap(23, 16);
    pixmap.fill(Qt::black); // ### use some color other than black for borders?

    KConfigGroup group(config, "WM");
    QPainter p(&pixmap);
    KColorScheme windowScheme(QPalette::Active, KColorScheme::Window, config);
    p.fillRect(1,  1, 7, 7, windowScheme.background());
    p.fillRect(2,  2, 5, 2, QBrush(windowScheme.foreground().color(), b1));

    KColorScheme buttonScheme(QPalette::Active, KColorScheme::Button, config);
    p.fillRect(8,  1, 7, 7, buttonScheme.background());
    p.fillRect(9,  2, 5, 2, QBrush(buttonScheme.foreground().color(), b1));

    p.fillRect(15,  1, 7, 7, group.readEntry("activeBackground", QColor(96, 148, 207)));
    p.fillRect(16,  2, 5, 2, QBrush(group.readEntry("activeForeground", QColor(255, 255, 255)), b1));

    KColorScheme viewScheme(QPalette::Active, KColorScheme::View, config);
    p.fillRect(1,  8, 7, 7, viewScheme.background());
    p.fillRect(2, 12, 5, 2, QBrush(viewScheme.foreground().color(), b2));

    KColorScheme selectionScheme(QPalette::Active, KColorScheme::Selection, config);
    p.fillRect(8,  8, 7, 7, selectionScheme.background());
    p.fillRect(9, 12, 5, 2, QBrush(selectionScheme.foreground().color(), b2));

    p.fillRect(15,  8, 7, 7, group.readEntry("inactiveBackground", QColor(224, 223, 222)));
    p.fillRect(16, 12, 5, 2, QBrush(group.readEntry("inactiveForeground", QColor(20, 19, 18)), b2));

    p.end();
    return pixmap;
}

void MainWindow::slotSwitchMonitors()
{
    m_monitorManager->slotSwitchMonitors(!m_clipMonitor->isActive());
    if (m_projectMonitor->isActive()) m_activeTimeline->projectView()->setFocus();
    else m_projectList->focusTree();
}

void MainWindow::slotInsertZoneToTree()
{
    if (!m_clipMonitor->isActive() || m_clipMonitor->activeClip() == NULL) return;
    QStringList info = m_clipMonitor->getZoneInfo();
    m_projectList->slotAddClipCut(info.at(0), info.at(1).toInt(), info.at(2).toInt());
}

void MainWindow::slotInsertZoneToTimeline()
{
    if (m_activeTimeline == NULL || m_clipMonitor->activeClip() == NULL) return;
    QStringList info = m_clipMonitor->getZoneInfo();
    m_activeTimeline->projectView()->insertClipCut(m_clipMonitor->activeClip(), info.at(1).toInt(), info.at(2).toInt());
}


void MainWindow::slotDeleteProjectClips(QStringList ids, QMap<QString, QString> folderids)
{
    if (m_activeDocument && m_activeTimeline) {
        if (!ids.isEmpty()) {
            for (int i = 0; i < ids.size(); ++i) {
                m_activeTimeline->slotDeleteClip(ids.at(i));
            }
            m_activeDocument->clipManager()->slotDeleteClips(ids);
        }
        if (!folderids.isEmpty()) m_projectList->deleteProjectFolder(folderids);
        m_activeDocument->setModified(true);
    }
}

void MainWindow::slotShowTitleBars(bool show)
{
    QList <QDockWidget *> docks = findChildren<QDockWidget *>();
    for (int i = 0; i < docks.count(); i++) {
        QDockWidget* dock = docks.at(i);
        if (show) {
            dock->setTitleBarWidget(0);
        } else {
            if (!dock->isFloating()) {
                dock->setTitleBarWidget(new QWidget);
            }
        }
    }
    KdenliveSettings::setShowtitlebars(show);
}

void MainWindow::slotSwitchTitles()
{
    slotShowTitleBars(!KdenliveSettings::showtitlebars());
}

QString MainWindow::getMimeType(bool open)
{
    QString mimetype = "application/x-kdenlive";
    KMimeType::Ptr mime = KMimeType::mimeType(mimetype);
    if (!mime) {
        mimetype = "*.kdenlive";
        if (open) mimetype.append(" *.tar.gz");
    }
    else if (open) mimetype.append(" application/x-compressed-tar");
    return mimetype;
}

void MainWindow::slotMonitorRequestRenderFrame(bool request)
{
    if (request) {
        m_projectMonitor->render->sendFrameForAnalysis = true;
        return;
    } else {
        for (int i = 0; i < m_gfxScopesList.count(); i++) {
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
        m_stopmotion = new StopmotionWidget(m_monitorManager, m_activeDocument->projectFolder(), m_stopmotion_actions->actions(), this);
        connect(m_stopmotion, SIGNAL(addOrUpdateSequence(const QString &)), m_projectList, SLOT(slotAddOrUpdateSequence(const QString)));
        //for (int i = 0; i < m_gfxScopesList.count(); i++) {
            // Check if we need the renderer to send a new frame for update
            /*if (!m_scopesList.at(i)->widget()->visibleRegion().isEmpty() && !(static_cast<AbstractScopeWidget *>(m_scopesList.at(i)->widget())->autoRefreshEnabled())) request = true;*/
            //connect(m_stopmotion, SIGNAL(gotFrame(QImage)), static_cast<AbstractGfxScopeWidget *>(m_gfxScopesList.at(i)->widget()), SLOT(slotRenderZoneUpdated(QImage)));
            //static_cast<AbstractScopeWidget *>(m_scopesList.at(i)->widget())->slotMonitorCapture();
        //}
    }
    m_stopmotion->show();
}

void MainWindow::slotDeleteClip(const QString &id)
{
    QList <ClipProperties *> list = findChildren<ClipProperties *>();
    for (int i = 0; i < list.size(); ++i) {
        list.at(i)->disableClipId(id);
    }
    m_projectList->slotDeleteClip(id);
}

void MainWindow::slotUpdateProxySettings()
{
    if (m_renderWidget) m_renderWidget->updateProxyConfig(m_projectList->useProxy());
    if (KdenliveSettings::enableproxy())
        KStandardDirs::makeDir(m_activeDocument->projectFolder().path(KUrl::AddTrailingSlash) + "proxy/");
    m_projectList->updateProxyConfig();
}

void MainWindow::slotInsertNotesTimecode()
{
    int frames = m_projectMonitor->render->seekPosition().frames(m_activeDocument->fps());
    QString position = m_activeDocument->timecode().getTimecodeFromFrames(frames);
    m_notesWidget->insertHtml("<a href=\"" + QString::number(frames) + "\">" + position + "</a> ");
}

void MainWindow::slotArchiveProject()
{
    QList <DocClipBase*> list = m_projectList->documentClipList();
    QDomDocument doc = m_activeDocument->xmlSceneList(m_projectMonitor->sceneList(), m_projectList->expandedFolders());
    ArchiveWidget *d = new ArchiveWidget(m_activeDocument->url().fileName(), doc, list, m_activeTimeline->projectView()->extractTransitionsLumas(), this);
    d->exec();
}


void MainWindow::slotOpenBackupDialog(const KUrl url)
{
    KUrl projectFile;
    KUrl projectFolder;
    QString projectId;
    kDebug()<<"// BACKUP URL: "<<url.path();
    if (!url.isEmpty()) {
        // we could not open the project file, guess where the backups are
        projectFolder = KUrl(KdenliveSettings::defaultprojectfolder());
        projectFile = url;
    }
    else {
        projectFolder = m_activeDocument->projectFolder();
        projectFile = m_activeDocument->url();
        projectId = m_activeDocument->getDocumentProperty("documentid");
    }

    QPointer<BackupWidget> dia = new BackupWidget(projectFile, projectFolder, projectId, this);
    if (dia->exec() == QDialog::Accepted) {
        QString requestedBackup = dia->selectedFile();
        m_activeDocument->backupLastSavedVersion(projectFile.path());
        closeCurrentDocument(false);
        doOpenFile(KUrl(requestedBackup), NULL);
        m_activeDocument->setUrl(projectFile);
        m_activeDocument->setModified(true);
        setCaption(m_activeDocument->description());
    }
    delete dia;
}

void MainWindow::slotElapsedTime()
{
    kDebug()<<"-----------------------------------------\n"<<"Time elapsed: "<<m_timer.elapsed()<<"\n-------------------------";
}


void MainWindow::slotDownloadResources()
{
    QString currentFolder;
    if (m_activeDocument) currentFolder = m_activeDocument->projectFolder().path();
    else currentFolder = KdenliveSettings::defaultprojectfolder();
    ResourceWidget *d = new ResourceWidget(currentFolder);
    connect(d, SIGNAL(addClip(KUrl,stringMap)), this, SLOT(slotAddProjectClip(KUrl,stringMap)));
    d->show();
}

void MainWindow::slotChangePalette()
{
    QPalette plt = QApplication::palette();
    if (m_effectStack) m_effectStack->updatePalette();
    if (m_projectList) m_projectList->updatePalette();
    if (m_effectList) m_effectList->updatePalette();
    
    if (m_clipMonitor) m_clipMonitor->setPalette(plt);
    if (m_projectMonitor) m_projectMonitor->setPalette(plt);
    
    setStatusBarStyleSheet(plt);
    if (m_activeTimeline) {
        m_activeTimeline->updatePalette();
    }
}

void MainWindow::slotSaveTimelineClip()
{
    if (m_activeTimeline && m_projectMonitor->render) {
	ClipItem *clip = m_activeTimeline->projectView()->getActiveClipUnderCursor(true);
	if (!clip) {
	    m_messageLabel->setMessage(i18n("Select a clip to save"), InformationMessage);
	    return;
	}
	KUrl url = KFileDialog::getSaveUrl(m_activeDocument->projectFolder(), "video/mlt-playlist");
	if (!url.isEmpty()) m_projectMonitor->render->saveClip(m_activeDocument->tracksCount() - clip->track(), clip->startPos(), url);
    }
}

//#ifdef USE_JACK
void MainWindow::slotConnectJack()
{
	Render * rp;
	Render * rc;

	if (m_projectMonitor != NULL) {
		rp = m_projectMonitor->render;

		if ( rp != NULL)
		{
			setenv("JACK_NAME_PRJ", "KdlivePrjMon", 1);
			rp->connectSlave();
			unsetenv("JACK_NAME_PRJ");
			kDebug() << "Project monitor connected to jack" << "\n";
		}
	}

//	if (m_clipMonitor != NULL)	{
//		rc = m_clipMonitor->render;
//
//		if ( rc != NULL )
//		{
//			rc->setJackFilter(rp->getJackFilter());
////			setenv("JACK_NAME_CLIP", "KdliveClipMon", 1);
////			rc->mltConnectJack();
////			unsetenv("JACK_NAME_CLIP");
////			kDebug() << "Clip monitor connected to jack" << "\n";
//		}
//	}

}

void MainWindow::slotDisconnectJack()
{
	if (m_projectMonitor != NULL)
	{
		Render * rp = m_projectMonitor->render;

		if ( rp != NULL)
		{
			rp->disconnectSlave();
			kDebug() << "Project monitor disconnected from Jack" << "\n";
		}
	}

//	if (m_clipMonitor != NULL) {
//
//		Render * rc = m_clipMonitor->render;
//		if ( rc != NULL)
//		{
//			rc->mltDisconnectJack();
//
//			kDebug() << "Clip monitor disconnected from Jack" << "\n";
//		}
//	}
}
//#endif

void MainWindow::slotProcessImportKeyframes(GRAPHICSRECTITEM type, const QString& data, int maximum)
{
    if (type == AVWIDGET) {
	// This data should be sent to the effect stack
    }
    else if (type == TRANSITIONWIDGET) {
	// This data should be sent to the transition stack
	m_transitionConfig->setKeyframes(data, maximum);
    }
    else {
	// Error
    }
}


#include "mainwindow.moc"

#ifdef DEBUG_MAINW
#undef DEBUG_MAINW
#endif
