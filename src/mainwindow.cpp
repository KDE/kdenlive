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
#include "kdenlivesettings.h"
#include "dialogs/kdenlivesettingsdialog.h"
#include "effectslist/initeffects.h"
#include "dialogs/profilesdialog.h"
#include "project/dialogs/projectsettings.h"
#include "project/clipmanager.h"
#include "project/projectlist.h"
#include "monitor/monitor.h"
#include "monitor/recmonitor.h"
#include "monitor/monitormanager.h"
#include "doc/kdenlivedoc.h"
#include "timeline/trackview.h"
#include "timeline/customtrackview.h"
#include "effectslist/effectslistview.h"
#include "effectstack/effectstackview2.h"
#include "project/transitionsettings.h"
#include "dialogs/renderwidget.h"
#include "renderer.h"
#include "project/clipproperties.h"
#include "dialogs/wizard.h"
#include "project/projectcommands.h"
#include "titler/titlewidget.h"
#include "timeline/markerdialog.h"
#include "timeline/clipitem.h"
#include "interfaces.h"
#include <config-kdenlive.h>
#include "project/cliptranscode.h"
#include "ui_templateclip_ui.h"
#include "scopes/scopemanager.h"
#include "scopes/colorscopes/vectorscope.h"
#include "scopes/colorscopes/waveform.h"
#include "scopes/colorscopes/rgbparade.h"
#include "scopes/colorscopes/histogram.h"
#include "scopes/audioscopes/audiosignal.h"
#include "scopes/audioscopes/audiospectrum.h"
#include "scopes/audioscopes/spectrogram.h"
#include "project/dialogs/archivewidget.h"
#include "utils/resourcewidget.h"
#include "layoutmanagement.h"
#include "hidetitlebars.h"
#include "project/projectmanager.h"
#ifdef USE_JOGSHUTTLE
#include "jogshuttle/jogmanager.h"
#endif

#include <KApplication>
#include <KAction>
#include <KLocalizedString>
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
#include <QUndoGroup>

#include <stdlib.h>

static const char version[] = KDENLIVE_VERSION;

namespace Mlt
{
class Producer;
};

Q_DECLARE_METATYPE(QVector<qint16>)


EffectsList MainWindow::videoEffects;
EffectsList MainWindow::audioEffects;
EffectsList MainWindow::customEffects;
EffectsList MainWindow::transitions;

QMap <QString,QImage> MainWindow::m_lumacache;

static bool sortByNames(const QPair<QString, KAction*> &a, const QPair<QString, KAction*> &b)
{
    return a.first < b.first;
}

MainWindow::MainWindow(const QString &MltPath, const KUrl & Url, const QString & clipsToLoad, QWidget *parent) :
    KXmlGuiWindow(parent),
    m_projectList(NULL),
    m_effectList(NULL),
    m_effectStack(NULL),
    m_clipMonitor(NULL),
    m_projectMonitor(NULL),
    m_recMonitor(NULL),
    m_renderWidget(NULL),
    m_findActivated(false),
    m_stopmotion(NULL),
    m_mainClip(NULL)
{
    qRegisterMetaType<QVector<qint16> > ();
    qRegisterMetaType<stringMap> ("stringMap");
    qRegisterMetaType<audioByteArray> ("audioByteArray");

    Core::initialize(this);

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

    connect(&m_findTimer, SIGNAL(timeout()), this, SLOT(findTimeout()));
    m_findTimer.setSingleShot(true);

    // FIXME: the next call returns a newly allocated object, which leaks
    initEffects::parseEffectFiles();
    //initEffects::parseCustomEffectsFile();

    m_shortcutRemoveFocus = new QShortcut(QKeySequence("Esc"), this);
    connect(m_shortcutRemoveFocus, SIGNAL(activated()), this, SLOT(slotRemoveFocus()));


    /// Add Widgets ///

    m_projectList = new ProjectList();
    m_projectListDock = addDock(i18n("Project Tree"), "project_tree", m_projectList);

    m_clipMonitor = new Monitor(Kdenlive::ClipMonitor, pCore->monitorManager(), QString(), m_timelineArea);

    // Connect the project list
    connect(m_projectList, SIGNAL(clipSelected(DocClipBase*,QPoint,bool)), m_clipMonitor, SLOT(slotSetClipProducer(DocClipBase*,QPoint,bool)));
    connect(m_projectList, SIGNAL(raiseClipMonitor(bool)), m_clipMonitor, SLOT(slotActivateMonitor(bool)));
    connect(m_projectList, SIGNAL(loadingIsOver()), this, SLOT(slotElapsedTime()));
    connect(m_projectList, SIGNAL(displayMessage(QString,int,MessageType)), this, SLOT(slotGotProgressInfo(QString,int,MessageType)));
    connect(m_projectList, SIGNAL(updateRenderStatus()), this, SLOT(slotCheckRenderStatus()));
    connect(m_projectList, SIGNAL(clipNeedsReload(QString)),this, SLOT(slotUpdateClip(QString)));
    connect(m_projectList, SIGNAL(updateProfile(QString)), this, SLOT(slotUpdateProjectProfile(QString)));
    connect(m_projectList, SIGNAL(refreshClip(QString,bool)), pCore->monitorManager(), SLOT(slotRefreshCurrentMonitor(QString)));
    connect(m_projectList, SIGNAL(findInTimeline(QString)), this, SLOT(slotClipInTimeline(QString)));
    connect(m_clipMonitor, SIGNAL(zoneUpdated(QPoint)), m_projectList, SLOT(slotUpdateClipCut(QPoint)));
    connect(m_clipMonitor, SIGNAL(extractZone(QString,QPoint)), m_projectList, SLOT(slotCutClipJob(QString,QPoint)));

    m_projectMonitor = new Monitor(Kdenlive::ProjectMonitor, pCore->monitorManager(), QString());

#ifndef Q_WS_MAC
    m_recMonitor = new RecMonitor(Kdenlive::RecordMonitor, pCore->monitorManager());
    connect(m_recMonitor, SIGNAL(addProjectClip(KUrl)), this, SLOT(slotAddProjectClip(KUrl)));
    connect(m_recMonitor, SIGNAL(addProjectClipList(KUrl::List)), this, SLOT(slotAddProjectClipList(KUrl::List)));
    connect(m_recMonitor, SIGNAL(showConfigDialog(int,int)), this, SLOT(slotPreferences(int,int)));

#endif /* ! Q_WS_MAC */
    pCore->monitorManager()->initMonitors(m_clipMonitor, m_projectMonitor, m_recMonitor);

    m_notesWidget = new NotesWidget();
    connect(m_notesWidget, SIGNAL(insertNotesTimecode()), this, SLOT(slotInsertNotesTimecode()));
    connect(m_notesWidget, SIGNAL(seekProject(int)), m_projectMonitor->render, SLOT(seekToFrame(int)));
    m_notesWidget->setTabChangesFocus(true);
#if KDE_IS_VERSION(4,4,0)
    m_notesWidget->setClickMessage(i18n("Enter your project notes here ..."));
#endif
    m_notesDock = addDock(i18n("Project Notes"), "notes_widget", m_notesWidget);

    m_effectStack = new EffectStackView2(m_projectMonitor);
    connect(m_effectStack, SIGNAL(startFilterJob(ItemInfo,QString,QString,QString,QString,QString,QMap<QString,QString>)), m_projectList, SLOT(slotStartFilterJob(ItemInfo,QString,QString,QString,QString,QString,QMap<QString,QString>)));
    m_effectStackDock = addDock(i18n("Effect Stack"), "effect_stack", m_effectStack);

    m_transitionConfig = new TransitionSettings(m_projectMonitor);
    m_transitionConfigDock = addDock(i18n("Transition"), "transition", m_transitionConfig);

    m_effectList = new EffectsListView();
    m_effectListDock = addDock(i18n("Effect List"), "effect_list", m_effectList);

    m_scopeManager = new ScopeManager(pCore->monitorManager());
    m_vectorscope = new Vectorscope();
    m_vectorscopeDock = addDock(i18n("Vectorscope"), m_vectorscope->widgetName(), m_vectorscope);
    m_scopeManager->addScope(m_vectorscope, m_vectorscopeDock);

    m_waveform = new Waveform();
    m_waveformDock = addDock(i18n("Waveform"), m_waveform->widgetName(), m_waveform);
    m_scopeManager->addScope(m_waveform, m_waveformDock);

    m_RGBParade = new RGBParade();
    m_RGBParadeDock = addDock(i18n("RGB Parade"), m_RGBParade->widgetName(), m_RGBParade);
    m_scopeManager->addScope(m_RGBParade, m_RGBParadeDock);

    m_histogram = new Histogram();
    m_histogramDock = addDock(i18n("Histogram"), m_histogram->widgetName(), m_histogram);
    m_scopeManager->addScope(m_histogram, m_histogramDock);

    m_audiosignal = new AudioSignal;
    m_audiosignalDock = addDock(i18n("Audio Signal"), m_audiosignal->widgetName(), m_audiosignal);
    m_scopeManager->addScope(m_audiosignal, m_audiosignalDock);

    m_audioSpectrum = new AudioSpectrum();
    m_audioSpectrumDock = addDock(i18n("AudioSpectrum"), m_audioSpectrum->widgetName(), m_audioSpectrum);
    m_scopeManager->addScope(m_audioSpectrum, m_audioSpectrumDock);

    m_spectrogram = new Spectrogram();
    m_spectrogramDock = addDock(i18n("Spectrogram"), m_spectrogram->widgetName(), m_spectrogram);
    m_scopeManager->addScope(m_spectrogram, m_spectrogramDock);

    // Add monitors here to keep them at the right of the window
    m_clipMonitorDock = addDock(i18n("Clip Monitor"), "clip_monitor", m_clipMonitor);
    m_projectMonitorDock = addDock(i18n("Project Monitor"), "project_monitor", m_projectMonitor);
#ifndef Q_WS_MAC
    m_recMonitorDock = addDock(i18n("Record Monitor"), "record_monitor", m_recMonitor);
#endif

    m_undoView = new QUndoView();
    m_undoView->setCleanIcon(KIcon("edit-clear"));
    m_undoView->setEmptyLabel(i18n("Clean"));
    m_undoView->setGroup(m_commandStack);
    m_undoViewDock = addDock(i18n("Undo History"), "undo_history", m_undoView);


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

    m_fileOpenRecent = KStandardAction::openRecent(pCore->projectManager(), SLOT(openFile(KUrl)), actionCollection());
    readOptions();

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

    new LayoutManagement(this);
    new HideTitleBars(this);

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
    connect(themesMenu, SIGNAL(triggered(QAction*)), this, SLOT(slotChangePalette(QAction*)));

    // Setup and fill effects and transitions menus.


    QMenu *m = static_cast<QMenu*>(factory()->container("video_effects_menu", this));
    m->addActions(m_effectsMenu->actions());


    m_transitionsMenu = new QMenu(i18n("Add Transition"), this);
    for (int i = 0; i < transitions.count(); ++i)
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

    connect(m_projectMonitorDock, SIGNAL(visibilityChanged(bool)), m_projectMonitor, SLOT(refreshMonitor(bool)));
    connect(m_clipMonitorDock, SIGNAL(visibilityChanged(bool)), m_clipMonitor, SLOT(refreshMonitor(bool)));
    connect(m_effectList, SIGNAL(addEffect(QDomElement)), this, SLOT(slotAddEffect(QDomElement)));
    connect(m_effectList, SIGNAL(reloadEffects()), this, SLOT(slotReloadEffects()));

    slotConnectMonitors();

    m_projectListDock->raise();

    actionCollection()->addAssociatedWidget(m_clipMonitor->container());
    actionCollection()->addAssociatedWidget(m_projectMonitor->container());

    QList<QPair<QString, KAction *> > viewActions;
    QPair <QString, KAction *> pair;
    KAction *showTimeline = new KAction(i18n("Timeline"), this);
    showTimeline->setCheckable(true);
    showTimeline->setChecked(true);
    connect(showTimeline, SIGNAL(triggered(bool)), this, SLOT(slotShowTimeline(bool)));
    
    KMenu *viewMenu = static_cast<KMenu*>(factory()->container("dockwindows", this));
    pair.first = showTimeline->text();
    pair.second = showTimeline;
    viewActions.append(pair);
    
    QList <QDockWidget *> docks = findChildren<QDockWidget *>();
    for (int i = 0; i < docks.count(); ++i) {
        QDockWidget* dock = docks.at(i);
        QAction * a = dock->toggleViewAction();
        if (!a) continue;
        KAction* dockInformations = new KAction(this);
        dockInformations->setText(a->text());
        dockInformations->setCheckable(true);
        dockInformations->setChecked(!dock->isHidden());
        // HACK: since QActions cannot be used in KActionCategory to allow shortcut, we create a duplicate KAction of the dock QAction and link them
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
    KConfig conf("encodingprofiles.rc", KConfig::CascadeConfig, "appdata");
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
    
    connect (KGlobalSettings::self(), SIGNAL(kdisplayPaletteChanged()), this, SLOT(slotChangePalette()));

    pCore->projectManager()->init(Url, clipsToLoad);

#ifdef USE_JOGSHUTTLE
    new JogManager(this);
#endif
}

MainWindow::~MainWindow()
{
    delete m_stopmotion;

    m_effectStack->slotClipItemSelected(NULL);
    m_transitionConfig->slotTransitionItemSelected(NULL, 0, QPoint(), false);

    if (m_projectMonitor) m_projectMonitor->stop();
    if (m_clipMonitor) m_clipMonitor->stop();

    delete m_effectStack;
    delete m_transitionConfig;
    delete m_projectMonitor;
    delete m_clipMonitor;
    delete m_projectList;
    delete m_shortcutRemoveFocus;
    delete[] m_transitions;
    delete m_scopeManager;
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
    if (pCore->monitorManager()) {
        pCore->monitorManager()->stopActiveMonitor();
    }

    // WARNING: According to KMainWindow::queryClose documentation we are not supposed to close the document here?
    return pCore->projectManager()->closeCurrentDocument();
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


void MainWindow::generateClip()
{
    QAction *action = qobject_cast<QAction *>(sender());
    ClipGenerator *iGenerator = qobject_cast<ClipGenerator *>(action->parent());

    KdenliveDoc *project = pCore->projectManager()->current();
    KUrl clipUrl = iGenerator->generatedClip(KdenliveSettings::rendererpath(), action->data().toString(), project->projectFolder(),
                                             QStringList(), QStringList(), project->fps(), project->width(), project->height());
    if (!clipUrl.isEmpty()) {
        m_projectList->slotAddClip(QList <QUrl> () << clipUrl);
    }
}

void MainWindow::saveProperties(KConfigGroup &config)
{
    // save properties here,used by session management
    pCore->projectManager()->saveFile();
    KMainWindow::saveProperties(config);
}


void MainWindow::readProperties(const KConfigGroup &config)
{
    // read properties here,used by session management
    KMainWindow::readProperties(config);
    QString Lastproject = config.group("Recent Files").readPathEntry("File1", QString());
    pCore->projectManager()->openFile(KUrl(Lastproject));
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
        kDebug() << "--- ERROR, TRYING TO APPEND NULL EFFECT";
        return;
    }
    QDomElement effectToAdd = effect.cloneNode().toElement();
    bool ok;
    int ix = m_effectStack->isTrackMode(&ok);
    if (ok) pCore->projectManager()->currentTrackView()->projectView()->slotAddTrackEffect(effectToAdd, pCore->projectManager()->current()->tracksCount() - ix);
    else pCore->projectManager()->currentTrackView()->projectView()->slotAddEffect(effectToAdd, GenTime(), -1);
}

void MainWindow::slotUpdateClip(const QString &id)
{
    DocClipBase *clip = pCore->projectManager()->current()->clipManager()->getClipById(id);
    if (!clip) {
        return;
    }
    if (clip->numReferences() > 0) {
        pCore->projectManager()->currentTrackView()->projectView()->slotUpdateClip(id);
    }
    if (m_clipMonitor->activeClip() && m_clipMonitor->activeClip()->getId() == id) {
        Mlt::Producer *monitorProducer = clip->getCloneProducer();
        m_clipMonitor->updateClipProducer(monitorProducer);
    }
    clip->cleanupProducers();
}

void MainWindow::slotConnectMonitors()
{
    m_projectList->setRenderer(m_projectMonitor->render);
    connect(m_projectList, SIGNAL(pauseMonitor()), pCore->monitorManager(), SLOT(slotPause()));
    connect(m_projectList, SIGNAL(deleteProjectClips(QStringList,QMap<QString,QString>)), this, SLOT(slotDeleteProjectClips(QStringList,QMap<QString,QString>)));
    connect(m_projectList, SIGNAL(showClipProperties(DocClipBase*)), this, SLOT(slotShowClipProperties(DocClipBase*)));
    connect(m_projectList, SIGNAL(showClipProperties(QList<DocClipBase*>,QMap<QString,QString>)), this, SLOT(slotShowClipProperties(QList<DocClipBase*>,QMap<QString,QString>)));
    connect(m_projectMonitor->render, SIGNAL(replyGetImage(QString,QString,int,int)), m_projectList, SLOT(slotReplyGetImage(QString,QString,int,int)));
    connect(m_projectMonitor->render, SIGNAL(replyGetImage(QString,QImage)), m_projectList, SLOT(slotReplyGetImage(QString,QImage)));

    connect(m_projectMonitor->render, SIGNAL(replyGetFileProperties(QString,Mlt::Producer*,stringMap,stringMap,bool)), m_projectList, SLOT(slotReplyGetFileProperties(QString,Mlt::Producer*,stringMap,stringMap,bool)));

    connect(m_projectMonitor->render, SIGNAL(removeInvalidClip(QString,bool)), m_projectList, SLOT(slotRemoveInvalidClip(QString,bool)));

    connect(m_projectMonitor->render, SIGNAL(removeInvalidProxy(QString,bool)), m_projectList, SLOT(slotRemoveInvalidProxy(QString,bool)));

    connect(m_clipMonitor, SIGNAL(refreshClipThumbnail(QString,bool)), m_projectList, SLOT(slotRefreshClipThumbnail(QString,bool)));

    connect(m_clipMonitor, SIGNAL(adjustMonitorSize()), this, SLOT(slotAdjustClipMonitor()));
    connect(m_projectMonitor, SIGNAL(adjustMonitorSize()), this, SLOT(slotAdjustProjectMonitor()));

    connect(m_projectMonitor, SIGNAL(requestFrameForAnalysis(bool)), this, SLOT(slotMonitorRequestRenderFrame(bool)));

    connect(m_clipMonitor, SIGNAL(saveZone(Render*,QPoint,DocClipBase*)), this, SLOT(slotSaveZone(Render*,QPoint,DocClipBase*)));
    connect(m_projectMonitor, SIGNAL(saveZone(Render*,QPoint,DocClipBase*)), this, SLOT(slotSaveZone(Render*,QPoint,DocClipBase*)));
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

void MainWindow::addAction(const QString &name, QAction *action)
{
    m_actionNames.append(name);
    actionCollection()->addAction(name, action);
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
    connect(editGroup, SIGNAL(triggered(QAction*)), this, SLOT(slotChangeEdit(QAction*)));
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

    connect(toolGroup, SIGNAL(triggered(QAction*)), this, SLOT(slotChangeTool(QAction*)));

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

    m_projectSearch = new KAction(KIcon("edit-find"), i18n("Find"), this);
    addAction("project_find", m_projectSearch);
    connect(m_projectSearch, SIGNAL(triggered(bool)), this, SLOT(slotFind()));
    m_projectSearch->setShortcut(Qt::Key_Slash);

    m_projectSearchNext = new KAction(KIcon("go-down-search"), i18n("Find Next"), this);
    addAction("project_find_next", m_projectSearchNext);
    connect(m_projectSearchNext, SIGNAL(triggered(bool)), this, SLOT(slotFindNext()));
    m_projectSearchNext->setShortcut(Qt::Key_F3);
    m_projectSearchNext->setEnabled(false);

    KAction* profilesAction = new KAction(KIcon("document-new"), i18n("Manage Project Profiles"), this);
    addAction("manage_profiles", profilesAction);
    connect(profilesAction, SIGNAL(triggered(bool)), this, SLOT(slotEditProfiles()));

    KNS3::standardAction(i18n("Download New Wipes..."),            this, SLOT(slotGetNewLumaStuff()),       actionCollection(), "get_new_lumas");
    KNS3::standardAction(i18n("Download New Render Profiles..."),  this, SLOT(slotGetNewRenderStuff()),     actionCollection(), "get_new_profiles");
    KNS3::standardAction(i18n("Download New Project Profiles..."), this, SLOT(slotGetNewMltProfileStuff()), actionCollection(), "get_new_mlt_profiles");
    KNS3::standardAction(i18n("Download New Title Templates..."),  this, SLOT(slotGetNewTitleStuff()),      actionCollection(), "get_new_titles");

    KAction* wizAction = new KAction(KIcon("configure"), i18n("Run Config Wizard"), this);
    addAction("run_wizard", wizAction);
    connect(wizAction, SIGNAL(triggered(bool)), this, SLOT(slotRunWizard()));

    KAction* projectAction = new KAction(KIcon("configure"), i18n("Project Settings"), this);
    addAction("project_settings", projectAction);
    connect(projectAction, SIGNAL(triggered(bool)), this, SLOT(slotEditProjectSettings()));

    KAction* projectRender = new KAction(KIcon("media-record"), i18n("Render"), this);
    addAction("project_render", projectRender);
    projectRender->setShortcut(Qt::CTRL + Qt::Key_Return);
    connect(projectRender, SIGNAL(triggered(bool)), this, SLOT(slotRenderProject()));

    KAction* projectClean = new KAction(KIcon("edit-clear"), i18n("Clean Project"), this);
    addAction("project_clean", projectClean);
    connect(projectClean, SIGNAL(triggered(bool)), this, SLOT(slotCleanProject()));

    KAction* projectAdjust = new KAction(KIcon(), i18n("Adjust Profile to Current Clip"), this);
    addAction("project_adjust_profile", projectAdjust);
    connect(projectAdjust, SIGNAL(triggered(bool)), m_projectList, SLOT(adjustProjectProfileToItem()));

    m_playZone = new KAction(KIcon("media-playback-start"), i18n("Play Zone"), this);
    m_playZone->setShortcut(Qt::CTRL + Qt::Key_Space);
    addAction("monitor_play_zone", m_playZone);
    connect(m_playZone, SIGNAL(triggered(bool)), pCore->monitorManager(), SLOT(slotPlayZone()));

    m_loopZone = new KAction(KIcon("media-playback-start"), i18n("Loop Zone"), this);
    m_loopZone->setShortcut(Qt::ALT + Qt::Key_Space);
    addAction("monitor_loop_zone", m_loopZone);
    connect(m_loopZone, SIGNAL(triggered(bool)), pCore->monitorManager(), SLOT(slotLoopZone()));

    m_loopClip = new KAction(KIcon("media-playback-start"), i18n("Loop selected clip"), this);
    m_loopClip->setEnabled(false);
    addAction("monitor_loop_clip", m_loopClip);
    connect(m_loopClip, SIGNAL(triggered(bool)), m_projectMonitor, SLOT(slotLoopClip()));

    KAction *dvdWizard =  new KAction(KIcon("media-optical"), i18n("DVD Wizard"), this);
    addAction("dvd_wizard", dvdWizard);
    connect(dvdWizard, SIGNAL(triggered(bool)), this, SLOT(slotDvdWizard()));

    KAction *transcodeClip =  new KAction(KIcon("edit-copy"), i18n("Transcode Clips"), this);
    addAction("transcode_clip", transcodeClip);
    connect(transcodeClip, SIGNAL(triggered(bool)), this, SLOT(slotTranscodeClip()));

    KAction *archiveProject =  new KAction(KIcon("document-save-all"), i18n("Archive Project"), this);
    addAction("archive_project", archiveProject);
    connect(archiveProject, SIGNAL(triggered(bool)), this, SLOT(slotArchiveProject()));


    KAction *markIn = new KAction(i18n("Set Zone In"), this);
    markIn->setShortcut(Qt::Key_I);
    addAction("mark_in", markIn);
    connect(markIn, SIGNAL(triggered(bool)), this, SLOT(slotSetInPoint()));

    KAction *markOut = new KAction(i18n("Set Zone Out"), this);
    markOut->setShortcut(Qt::Key_O);
    addAction("mark_out", markOut);
    connect(markOut, SIGNAL(triggered(bool)), this, SLOT(slotSetOutPoint()));

    KAction *switchMonitor = new KAction(i18n("Switch monitor"), this);
    switchMonitor->setShortcut(Qt::Key_T);
    addAction("switch_monitor", switchMonitor);
    connect(switchMonitor, SIGNAL(triggered(bool)), this, SLOT(slotSwitchMonitors()));

    KAction *insertTree = new KAction(i18n("Insert zone in project tree"), this);
    insertTree->setShortcut(Qt::CTRL + Qt::Key_I);
    addAction("insert_project_tree", insertTree);
    connect(insertTree, SIGNAL(triggered(bool)), this, SLOT(slotInsertZoneToTree()));

    KAction *insertTimeline = new KAction(i18n("Insert zone in timeline"), this);
    insertTimeline->setShortcut(Qt::SHIFT + Qt::CTRL + Qt::Key_I);
    addAction("insert_timeline", insertTimeline);
    connect(insertTimeline, SIGNAL(triggered(bool)), this, SLOT(slotInsertZoneToTimeline()));

    KAction *resizeStart =  new KAction(KIcon(), i18n("Resize Item Start"), this);
    addAction("resize_timeline_clip_start", resizeStart);
    resizeStart->setShortcut(Qt::Key_1);
    connect(resizeStart, SIGNAL(triggered(bool)), this, SLOT(slotResizeItemStart()));

    KAction *resizeEnd =  new KAction(KIcon(), i18n("Resize Item End"), this);
    addAction("resize_timeline_clip_end", resizeEnd);
    resizeEnd->setShortcut(Qt::Key_2);
    connect(resizeEnd, SIGNAL(triggered(bool)), this, SLOT(slotResizeItemEnd()));

    KAction* monitorSeekSnapBackward = new KAction(KIcon("media-seek-backward"), i18n("Go to Previous Snap Point"), this);
    monitorSeekSnapBackward->setShortcut(Qt::ALT + Qt::Key_Left);
    addAction("monitor_seek_snap_backward", monitorSeekSnapBackward);
    connect(monitorSeekSnapBackward, SIGNAL(triggered(bool)), this, SLOT(slotSnapRewind()));

    KAction* clipStart = new KAction(KIcon("media-seek-backward"), i18n("Go to Clip Start"), this);
    clipStart->setShortcut(Qt::Key_Home);
    addAction("seek_clip_start", clipStart);
    connect(clipStart, SIGNAL(triggered(bool)), this, SLOT(slotClipStart()));

    KAction* clipEnd = new KAction(KIcon("media-seek-forward"), i18n("Go to Clip End"), this);
    clipEnd->setShortcut(Qt::Key_End);
    addAction("seek_clip_end", clipEnd);
    connect(clipEnd, SIGNAL(triggered(bool)), this, SLOT(slotClipEnd()));

    KAction* zoneStart = new KAction(KIcon("media-seek-backward"), i18n("Go to Zone Start"), this);
    zoneStart->setShortcut(Qt::SHIFT + Qt::Key_I);
    addAction("seek_zone_start", zoneStart);
    connect(zoneStart, SIGNAL(triggered(bool)), this, SLOT(slotZoneStart()));

    KAction* zoneEnd = new KAction(KIcon("media-seek-forward"), i18n("Go to Zone End"), this);
    zoneEnd->setShortcut(Qt::SHIFT + Qt::Key_O);
    addAction("seek_zone_end", zoneEnd);
    connect(zoneEnd, SIGNAL(triggered(bool)), this, SLOT(slotZoneEnd()));

    KAction* monitorSeekSnapForward = new KAction(KIcon("media-seek-forward"), i18n("Go to Next Snap Point"), this);
    monitorSeekSnapForward->setShortcut(Qt::ALT + Qt::Key_Right);
    addAction("monitor_seek_snap_forward", monitorSeekSnapForward);
    connect(monitorSeekSnapForward, SIGNAL(triggered(bool)), this, SLOT(slotSnapForward()));

    KAction* deleteItem = new KAction(KIcon("edit-delete"), i18n("Delete Selected Item"), this);
    deleteItem->setShortcut(Qt::Key_Delete);
    addAction("delete_timeline_clip", deleteItem);
    connect(deleteItem, SIGNAL(triggered(bool)), this, SLOT(slotDeleteItem()));

    KAction* alignPlayhead = new KAction(i18n("Align Playhead to Mouse Position"), this);
    alignPlayhead->setShortcut(Qt::Key_P);
    addAction("align_playhead", alignPlayhead);
    connect(alignPlayhead, SIGNAL(triggered(bool)), this, SLOT(slotAlignPlayheadToMousePos()));

    /*KAction* editTimelineClipSpeed = new KAction(i18n("Change Clip Speed"), this);
    addAction("change_clip_speed", editTimelineClipSpeed);
    editTimelineClipSpeed->setData("change_speed");
    connect(editTimelineClipSpeed, SIGNAL(triggered(bool)), this, SLOT(slotChangeClipSpeed()));*/

    KAction *stickTransition = new KAction(i18n("Automatic Transition"), this);
    stickTransition->setData(QString("auto"));
    stickTransition->setCheckable(true);
    stickTransition->setEnabled(false);
    addAction("auto_transition", stickTransition);
    connect(stickTransition, SIGNAL(triggered(bool)), this, SLOT(slotAutoTransition()));

    KAction* groupClip = new KAction(KIcon("object-group"), i18n("Group Clips"), this);
    groupClip->setShortcut(Qt::CTRL + Qt::Key_G);
    addAction("group_clip", groupClip);
    connect(groupClip, SIGNAL(triggered(bool)), this, SLOT(slotGroupClips()));

    KAction* ungroupClip = new KAction(KIcon("object-ungroup"), i18n("Ungroup Clips"), this);
    addAction("ungroup_clip", ungroupClip);
    ungroupClip->setShortcut(Qt::CTRL + Qt::SHIFT + Qt::Key_G);
    ungroupClip->setData("ungroup_clip");
    connect(ungroupClip, SIGNAL(triggered(bool)), this, SLOT(slotUnGroupClips()));

    KAction* editItemDuration = new KAction(KIcon("measure"), i18n("Edit Duration"), this);
    addAction("edit_item_duration", editItemDuration);
    connect(editItemDuration, SIGNAL(triggered(bool)), this, SLOT(slotEditItemDuration()));
    
    KAction* saveTimelineClip = new KAction(KIcon("document-save"), i18n("Save clip"), this);
    addAction("save_timeline_clip", saveTimelineClip);
    connect(saveTimelineClip, SIGNAL(triggered(bool)), this, SLOT(slotSaveTimelineClip()));

    KAction* clipInProjectTree = new KAction(KIcon("go-jump-definition"), i18n("Clip in Project Tree"), this);
    addAction("clip_in_project_tree", clipInProjectTree);
    connect(clipInProjectTree, SIGNAL(triggered(bool)), this, SLOT(slotClipInProjectTree()));

    /*KAction* clipToProjectTree = new KAction(KIcon("go-jump-definition"), i18n("Add Clip to Project Tree"), this);
    addAction("clip_to_project_tree", clipToProjectTree);
    connect(clipToProjectTree, SIGNAL(triggered(bool)), this, SLOT(slotClipToProjectTree()));*/

    KAction* insertOvertwrite = new KAction(KIcon(), i18n("Insert Clip Zone in Timeline (Overwrite)"), this);
    insertOvertwrite->setShortcut(Qt::Key_V);
    addAction("overwrite_to_in_point", insertOvertwrite);
    connect(insertOvertwrite, SIGNAL(triggered(bool)), this, SLOT(slotInsertClipOverwrite()));

    KAction* selectTimelineClip = new KAction(KIcon("edit-select"), i18n("Select Clip"), this);
    selectTimelineClip->setShortcut(Qt::Key_Plus);
    addAction("select_timeline_clip", selectTimelineClip);
    connect(selectTimelineClip, SIGNAL(triggered(bool)), this, SLOT(slotSelectTimelineClip()));

    KAction* deselectTimelineClip = new KAction(KIcon("edit-select"), i18n("Deselect Clip"), this);
    deselectTimelineClip->setShortcut(Qt::Key_Minus);
    addAction("deselect_timeline_clip", deselectTimelineClip);
    connect(deselectTimelineClip, SIGNAL(triggered(bool)), this, SLOT(slotDeselectTimelineClip()));

    KAction* selectAddTimelineClip = new KAction(KIcon("edit-select"), i18n("Add Clip To Selection"), this);
    selectAddTimelineClip->setShortcut(Qt::ALT + Qt::Key_Plus);
    addAction("select_add_timeline_clip", selectAddTimelineClip);
    connect(selectAddTimelineClip, SIGNAL(triggered(bool)), this, SLOT(slotSelectAddTimelineClip()));

    KAction* selectTimelineTransition = new KAction(KIcon("edit-select"), i18n("Select Transition"), this);
    selectTimelineTransition->setShortcut(Qt::SHIFT + Qt::Key_Plus);
    addAction("select_timeline_transition", selectTimelineTransition);
    connect(selectTimelineTransition, SIGNAL(triggered(bool)), this, SLOT(slotSelectTimelineTransition()));

    KAction* deselectTimelineTransition = new KAction(KIcon("edit-select"), i18n("Deselect Transition"), this);
    deselectTimelineTransition->setShortcut(Qt::SHIFT + Qt::Key_Minus);
    addAction("deselect_timeline_transition", deselectTimelineTransition);
    connect(deselectTimelineTransition, SIGNAL(triggered(bool)), this, SLOT(slotDeselectTimelineTransition()));

    KAction* selectAddTimelineTransition = new KAction(KIcon("edit-select"), i18n("Add Transition To Selection"), this);
    selectAddTimelineTransition->setShortcut(Qt::ALT + Qt::SHIFT + Qt::Key_Plus);
    addAction("select_add_timeline_transition", selectAddTimelineTransition);
    connect(selectAddTimelineTransition, SIGNAL(triggered(bool)), this, SLOT(slotSelectAddTimelineTransition()));

    KAction* cutTimelineClip = new KAction(KIcon("edit-cut"), i18n("Cut Clip"), this);
    cutTimelineClip->setShortcut(Qt::SHIFT + Qt::Key_R);
    addAction("cut_timeline_clip", cutTimelineClip);
    connect(cutTimelineClip, SIGNAL(triggered(bool)), this, SLOT(slotCutTimelineClip()));

    KAction* addClipMarker = new KAction(KIcon("bookmark-new"), i18n("Add Marker"), this);
    addAction("add_clip_marker", addClipMarker);
    connect(addClipMarker, SIGNAL(triggered(bool)), this, SLOT(slotAddClipMarker()));

    KAction* deleteClipMarker = new KAction(KIcon("edit-delete"), i18n("Delete Marker"), this);
    addAction("delete_clip_marker", deleteClipMarker);
    connect(deleteClipMarker, SIGNAL(triggered(bool)), this, SLOT(slotDeleteClipMarker()));

    KAction* deleteAllClipMarkers = new KAction(KIcon("edit-delete"), i18n("Delete All Markers"), this);
    addAction("delete_all_clip_markers", deleteAllClipMarkers);
    connect(deleteAllClipMarkers, SIGNAL(triggered(bool)), this, SLOT(slotDeleteAllClipMarkers()));

    KAction* editClipMarker = new KAction(KIcon("document-properties"), i18n("Edit Marker"), this);
    editClipMarker->setData(QString("edit_marker"));
    addAction("edit_clip_marker", editClipMarker);
    connect(editClipMarker, SIGNAL(triggered(bool)), this, SLOT(slotEditClipMarker()));

    KAction* addMarkerGuideQuickly = new KAction(KIcon("bookmark-new"), i18n("Add Marker/Guide quickly"), this);
    addMarkerGuideQuickly->setShortcut(Qt::Key_Asterisk);
    addAction("add_marker_guide_quickly", addMarkerGuideQuickly);
    connect(addMarkerGuideQuickly, SIGNAL(triggered(bool)), this, SLOT(slotAddMarkerGuideQuickly()));

    KAction* splitAudio = new KAction(KIcon("document-new"), i18n("Split Audio"), this);
    addAction("split_audio", splitAudio);
    // "A+V" as data means this action should only be available for clips with audio AND video
    splitAudio->setData("A+V");
    connect(splitAudio, SIGNAL(triggered(bool)), this, SLOT(slotSplitAudio()));

    KAction* setAudioAlignReference = new KAction(i18n("Set Audio Reference"), this);
    addAction("set_audio_align_ref", setAudioAlignReference);
    // "A" as data means this action should only be available for clips with audio
    setAudioAlignReference->setData("A");
    connect(setAudioAlignReference, SIGNAL(triggered()), this, SLOT(slotSetAudioAlignReference()));

    KAction* alignAudio = new KAction(i18n("Align Audio to Reference"), this);
    addAction("align_audio", alignAudio);
    // "A" as data means this action should only be available for clips with audio
    alignAudio->setData("A");
    connect(alignAudio, SIGNAL(triggered()), this, SLOT(slotAlignAudio()));

    KAction* audioOnly = new KAction(KIcon("document-new"), i18n("Audio Only"), this);
    addAction("clip_audio_only", audioOnly);
    audioOnly->setData("clip_audio_only");
    audioOnly->setCheckable(true);

    KAction* videoOnly = new KAction(KIcon("document-new"), i18n("Video Only"), this);
    addAction("clip_video_only", videoOnly);
    videoOnly->setData("clip_video_only");
    videoOnly->setCheckable(true);

    KAction* audioAndVideo = new KAction(KIcon("document-new"), i18n("Audio and Video"), this);
    addAction("clip_audio_and_video", audioAndVideo);
    audioAndVideo->setData("clip_audio_and_video");
    audioAndVideo->setCheckable(true);

    m_clipTypeGroup = new QActionGroup(this);
    m_clipTypeGroup->addAction(audioOnly);
    m_clipTypeGroup->addAction(videoOnly);
    m_clipTypeGroup->addAction(audioAndVideo);
    connect(m_clipTypeGroup, SIGNAL(triggered(QAction*)), this, SLOT(slotUpdateClipType(QAction*)));
    m_clipTypeGroup->setEnabled(false);

    KAction *insertSpace = new KAction(KIcon(), i18n("Insert Space"), this);
    addAction("insert_space", insertSpace);
    connect(insertSpace, SIGNAL(triggered()), this, SLOT(slotInsertSpace()));

    KAction *removeSpace = new KAction(KIcon(), i18n("Remove Space"), this);
    addAction("delete_space", removeSpace);
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
    addAction("add_guide", addGuide);
    connect(addGuide, SIGNAL(triggered()), this, SLOT(slotAddGuide()));

    QAction *delGuide = new KAction(KIcon("edit-delete"), i18n("Delete Guide"), this);
    addAction("delete_guide", delGuide);
    connect(delGuide, SIGNAL(triggered()), this, SLOT(slotDeleteGuide()));

    QAction *editGuide = new KAction(KIcon("document-properties"), i18n("Edit Guide"), this);
    addAction("edit_guide", editGuide);
    connect(editGuide, SIGNAL(triggered()), this, SLOT(slotEditGuide()));

    QAction *delAllGuides = new KAction(KIcon("edit-delete"), i18n("Delete All Guides"), this);
    addAction("delete_all_guides", delAllGuides);
    connect(delAllGuides, SIGNAL(triggered()), this, SLOT(slotDeleteAllGuides()));

    QAction *pasteEffects = new KAction(KIcon("edit-paste"), i18n("Paste Effects"), this);
    addAction("paste_effects", pasteEffects);
    pasteEffects->setData("paste_effects");
    connect(pasteEffects , SIGNAL(triggered()), this, SLOT(slotPasteEffects()));

    m_saveAction = KStandardAction::save(pCore->projectManager(),    SLOT(saveFile()),               actionCollection());
    KStandardAction::quit(this,                   SLOT(close()),                  actionCollection());
    // TODO: make the following connection to slotEditKeys work
    //KStandardAction::keyBindings(this,            SLOT(slotEditKeys()),           actionCollection());
    KStandardAction::preferences(this,            SLOT(slotPreferences()),        actionCollection());
    KStandardAction::configureNotifications(this, SLOT(configureNotifications()), actionCollection());
    KStandardAction::copy(this,                   SLOT(slotCopy()),               actionCollection());
    KStandardAction::paste(this,                  SLOT(slotPaste()),              actionCollection());
    KStandardAction::fullScreen(this,             SLOT(slotFullScreen()), this,   actionCollection());

    KAction *undo = KStandardAction::undo(m_commandStack, SLOT(undo()), actionCollection());
    undo->setEnabled(false);
    connect(m_commandStack, SIGNAL(canUndoChanged(bool)), undo, SLOT(setEnabled(bool)));

    KAction *redo = KStandardAction::redo(m_commandStack, SLOT(redo()), actionCollection());
    redo->setEnabled(false);
    connect(m_commandStack, SIGNAL(canRedoChanged(bool)), redo, SLOT(setEnabled(bool)));

    /*
    //TODO: Add status tooltip to actions ?
    connect(actionCollection(), SIGNAL(actionHovered(QAction*)),
            this, SLOT(slotDisplayActionMessage(QAction*)));*/


    QAction *addClip = new KAction(KIcon("kdenlive-add-clip"), i18n("Add Clip"), this);
    addAction("add_clip", addClip);
    connect(addClip , SIGNAL(triggered()), m_projectList, SLOT(slotAddClip()));

    QAction *addColorClip = new KAction(KIcon("kdenlive-add-color-clip"), i18n("Add Color Clip"), this);
    addAction("add_color_clip", addColorClip);
    connect(addColorClip , SIGNAL(triggered()), m_projectList, SLOT(slotAddColorClip()));

    QAction *addSlideClip = new KAction(KIcon("kdenlive-add-slide-clip"), i18n("Add Slideshow Clip"), this);
    addAction("add_slide_clip", addSlideClip);
    connect(addSlideClip , SIGNAL(triggered()), m_projectList, SLOT(slotAddSlideshowClip()));

    QAction *addTitleClip = new KAction(KIcon("kdenlive-add-text-clip"), i18n("Add Title Clip"), this);
    addAction("add_text_clip", addTitleClip);
    connect(addTitleClip , SIGNAL(triggered()), m_projectList, SLOT(slotAddTitleClip()));

    QAction *addTitleTemplateClip = new KAction(KIcon("kdenlive-add-text-clip"), i18n("Add Template Title"), this);
    addAction("add_text_template_clip", addTitleTemplateClip);
    connect(addTitleTemplateClip , SIGNAL(triggered()), m_projectList, SLOT(slotAddTitleTemplateClip()));

    QAction *addFolderButton = new KAction(KIcon("folder-new"), i18n("Create Folder"), this);
    addAction("add_folder", addFolderButton);
    connect(addFolderButton , SIGNAL(triggered()), m_projectList, SLOT(slotAddFolder()));

    QAction *downloadResources = new KAction(KIcon("download"), i18n("Online Resources"), this);
    addAction("download_resource", downloadResources);
    connect(downloadResources , SIGNAL(triggered()), this, SLOT(slotDownloadResources()));

    QAction *clipProperties = new KAction(KIcon("document-edit"), i18n("Clip Properties"), this);
    addAction("clip_properties", clipProperties);
    clipProperties->setData("clip_properties");
    connect(clipProperties , SIGNAL(triggered()), m_projectList, SLOT(slotEditClip()));
    clipProperties->setEnabled(false);

    QAction *openClip = new KAction(KIcon("document-open"), i18n("Edit Clip"), this);
    addAction("edit_clip", openClip);
    openClip->setData("edit_clip");
    connect(openClip , SIGNAL(triggered()), m_projectList, SLOT(slotOpenClip()));
    openClip->setEnabled(false);

    QAction *deleteClip = new KAction(KIcon("edit-delete"), i18n("Delete Clip"), this);
    addAction("delete_clip", deleteClip);
    deleteClip->setData("delete_clip");
    connect(deleteClip , SIGNAL(triggered()), m_projectList, SLOT(slotRemoveClip()));
    deleteClip->setEnabled(false);

    QAction *reloadClip = new KAction(KIcon("view-refresh"), i18n("Reload Clip"), this);
    addAction("reload_clip", reloadClip);
    reloadClip->setData("reload_clip");
    connect(reloadClip , SIGNAL(triggered()), m_projectList, SLOT(slotReloadClip()));
    reloadClip->setEnabled(false);

    QAction *proxyClip = new KAction(i18n("Proxy Clip"), this);
    addAction("proxy_clip", proxyClip);
    proxyClip->setData("proxy_clip");
    proxyClip->setCheckable(true);
    proxyClip->setChecked(false);
    connect(proxyClip, SIGNAL(toggled(bool)), m_projectList, SLOT(slotProxyCurrentItem(bool)));

    QAction *stopMotion = new KAction(KIcon("image-x-generic"), i18n("Stop Motion Capture"), this);
    addAction("stopmotion", stopMotion);
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
    KActionCategory *transitionActions = new KActionCategory(i18n("Transitions"), actionCollection());
    m_transitions = new KAction*[transitions.count()];
    for (int i = 0; i < transitions.count(); ++i) {
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
    QString style1 = QString("QToolBar { border: 0px } QToolButton { border-style: inset; border:1px solid transparent;border-radius: 3px;margin: 0px 3px;padding: 0px;} QToolButton#timecode {padding-right:10px;} QToolButton:hover { background: %3;border-style: inset; border:1px solid %3;border-radius: 3px;} QToolButton:checked { background-color: %1; border-style: inset; border:1px solid %2;border-radius: 3px;}").arg(buttonBg.name()).arg(buttonBord.name()).arg(buttonBord2.name());
    statusBar()->setStyleSheet(style1);
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
                } else {
                    KdenliveSettings::setDefaultprojectfolder(path);
                }
            }
        }
    }

    if (KdenliveSettings::ffmpegpath().isEmpty() || KdenliveSettings::ffplaypath().isEmpty()) {
        upgrade = true;
    }

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
    if (!state.isEmpty()) {
        m_projectList->setHeaderInfo(state);
    }
}

void MainWindow::slotRunWizard()
{
    QPointer<Wizard> w = new Wizard(false, this);
    if (w->exec() == QDialog::Accepted && w->isOk()) {
        w->adjustSettings();
    }
    delete w;
}

void MainWindow::parseProfiles(const QString &mltPath)
{

    //KdenliveSettings::setDefaulttmpfolder();
    if (!mltPath.isEmpty()) {
        KdenliveSettings::setMltpath(mltPath + "/share/mlt/profiles/");
        KdenliveSettings::setRendererpath(mltPath + "/bin/melt");
    } else if (!qgetenv("MLT_PREFIX").isEmpty()) {
        KdenliveSettings::setMltpath(qgetenv("MLT_PREFIX") + "/share/mlt/profiles/");
        KdenliveSettings::setRendererpath(qgetenv("MLT_PREFIX") + "/bin/melt");
    }
    if (KdenliveSettings::mltpath().isEmpty()) {
        KdenliveSettings::setMltpath(QString(MLT_PREFIX) + "/share/mlt/profiles/");
        KdenliveSettings::setRendererpath(QString(MLT_PREFIX) + "/bin/melt");
    }

    if (KdenliveSettings::rendererpath().isEmpty() || KdenliveSettings::rendererpath().endsWith(QLatin1String("inigo"))) {
        QString meltPath = QString(MLT_PREFIX) + QString("/bin/melt");
        if (!QFile::exists(meltPath)) {
            meltPath = KStandardDirs::findExe("melt");
        }
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
    if (profilesList.isEmpty()) {
        parseProfiles();
    }
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
    QPoint p = project->getTracksCount();
    QPointer<ProjectSettings> w = new ProjectSettings(m_projectList, project->metadata(), pCore->projectManager()->currentTrackView()->projectView()->extractTransitionsLumas(), p.x(), p.y(), project->projectFolder().path(), true, !project->isModified(), this);
    connect(w, SIGNAL(disableProxies()), this, SLOT(slotDisableProxies()));

    if (w->exec() == QDialog::Accepted) {
        QString profile = w->selectedProfile();
        project->setProjectFolder(w->selectedFolder());
#ifndef Q_WS_MAC
        m_recMonitor->slotUpdateCaptureFolder(project->projectFolder().path(KUrl::AddTrailingSlash));
#endif
        if (m_renderWidget) {
            m_renderWidget->setDocumentPath(project->projectFolder().path(KUrl::AddTrailingSlash));
        }
        if (KdenliveSettings::videothumbnails() != w->enableVideoThumbs()) {
            slotSwitchVideoThumbs();
        }
        if (KdenliveSettings::audiothumbnails() != w->enableAudioThumbs()) {
            slotSwitchAudioThumbs();
        }
        if (project->profilePath() != profile) {
            slotUpdateProjectProfile(profile);
        }
        if (project->getDocumentProperty("proxyparams") != w->proxyParams()) {
            project->setModified();
            project->setDocumentProperty("proxyparams", w->proxyParams());
            if (project->clipManager()->clipsCount() > 0 && KMessageBox::questionYesNo(this, i18n("You have changed the proxy parameters. Do you want to recreate all proxy clips for this project?")) == KMessageBox::Yes) {
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

void MainWindow::slotUpdateProjectProfile(const QString &profile)
{
    KdenliveDoc *project = pCore->projectManager()->current();

    // Recreate the stopmotion widget if profile changes
    if (m_stopmotion) {
        delete m_stopmotion;
        m_stopmotion = NULL;
    }

    // Deselect current effect / transition
    m_effectStack->slotClipItemSelected(NULL);
    m_transitionConfig->slotTransitionItemSelected(NULL, 0, QPoint(), false);
    m_clipMonitor->slotSetClipProducer(NULL);
    bool updateFps = project->setProfilePath(profile);
    KdenliveSettings::setCurrent_profile(profile);
    KdenliveSettings::setProject_fps(project->fps());
    setCaption(project->description(), project->isModified());
    project->clipManager()->clearUnusedProducers();
    pCore->monitorManager()->resetProfiles(project->timecode());
    m_transitionConfig->updateProjectFormat(project->mltProfile(), project->timecode(), project->tracksList());
    m_effectStack->updateProjectFormat(project->mltProfile(), project->timecode());
    m_projectList->updateProjectFormat(project->timecode());
    if (m_renderWidget) {
        m_renderWidget->setProfile(project->mltProfile());
    }
    if (updateFps) {
        pCore->projectManager()->currentTrackView()->updateProjectFps();
    }
    project->clipManager()->clearCache();
    pCore->projectManager()->currentTrackView()->updateProfile();
    project->setModified(true);
    m_commandStack->activeStack()->clear();
    //Update the mouse position display so it will display in DF/NDF format by default based on the project setting.
    slotUpdateMousePosition(0);
    m_projectList->slotReloadClip();
    // We need to desactivate & reactivate monitors to get a refresh
    //pCore->monitorManager()->switchMonitors();
}


void MainWindow::slotRenderProject()
{
    KdenliveDoc *project = pCore->projectManager()->current();

    if (!m_renderWidget) {
        QString projectfolder = project ? project->projectFolder().path(KUrl::AddTrailingSlash) : KdenliveSettings::defaultprojectfolder();
        MltVideoProfile profile;
        if (project) {
            profile = project->mltProfile();
        }
        m_renderWidget = new RenderWidget(projectfolder, m_projectList->useProxy(), profile, this);
        connect(m_renderWidget, SIGNAL(shutdown()), this, SLOT(slotShutdown()));
        connect(m_renderWidget, SIGNAL(selectedRenderProfile(QMap<QString,QString>)), this, SLOT(slotSetDocumentRenderProfile(QMap<QString,QString>)));
        connect(m_renderWidget, SIGNAL(prepareRenderingData(bool,bool,QString)), this, SLOT(slotPrepareRendering(bool,bool,QString)));
        connect(m_renderWidget, SIGNAL(abortProcess(QString)), this, SIGNAL(abortRenderJob(QString)));
        connect(m_renderWidget, SIGNAL(openDvdWizard(QString)), this, SLOT(slotDvdWizard(QString)));
        if (project) {
            m_renderWidget->setProfile(project->mltProfile());
            m_renderWidget->setGuides(project->guidesXml(), project->projectDuration());
            m_renderWidget->setDocumentPath(project->projectFolder().path(KUrl::AddTrailingSlash));
            m_renderWidget->setRenderProfile(project->getRenderProperties());
        }
    }
    slotCheckRenderStatus();
    m_renderWidget->show();
    //m_renderWidget->showNormal();

    // What are the following lines supposed to do?
    //pCore->projectManager()->currentTrackView()->tracksNumber();
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
    if (pCore->projectManager()->current()) {
        switch (m_timeFormatButton->currentItem()) {
        case 0:
            m_timeFormatButton->setText(pCore->projectManager()->current()->timecode().getTimecodeFromFrames(pos) + " / " + pCore->projectManager()->current()->timecode().getTimecodeFromFrames(pCore->projectManager()->currentTrackView()->duration()));
            break;
        default:
            m_timeFormatButton->setText(QString::number(pos) + " / " + QString::number(pCore->projectManager()->currentTrackView()->duration()));
        }
    }
}

void MainWindow::slotUpdateProjectDuration(int pos)
{
    if (pCore->projectManager()->current()) {
        pCore->projectManager()->currentTrackView()->setDuration(pos);
        slotUpdateMousePosition(pCore->projectManager()->currentTrackView()->projectView()->getMousePos());
    }
}

void MainWindow::slotUpdateDocumentState(bool modified)
{
    setCaption(pCore->projectManager()->current()->description(), modified);
    m_saveAction->setEnabled(modified);
}

void MainWindow::connectDocument(KdenliveDoc *newDoc, KdenliveDoc **projectManagerDoc)
{
    kDebug() << "///////////////////   CONNECTING DOC TO PROJECT VIEW ////////////////";

    KdenliveSettings::setCurrent_profile(newDoc->profilePath());
    KdenliveSettings::setProject_fps(newDoc->fps());
    pCore->monitorManager()->resetProfiles(newDoc->timecode());
    m_clipMonitorDock->raise();
    m_projectList->setDocument(newDoc);
    m_transitionConfig->updateProjectFormat(newDoc->mltProfile(), newDoc->timecode(), newDoc->tracksList());
    m_effectStack->updateProjectFormat(newDoc->mltProfile(), newDoc->timecode());
    connect(m_projectList, SIGNAL(refreshClip(QString,bool)), pCore->projectManager()->currentTrackView()->projectView(), SLOT(slotRefreshThumbs(QString,bool)));

    connect(m_projectList, SIGNAL(projectModified()), newDoc, SLOT(setModified()));
    connect(m_projectList, SIGNAL(clipNameChanged(QString,QString)), pCore->projectManager()->currentTrackView()->projectView(), SLOT(clipNameChanged(QString,QString)));

    connect(pCore->projectManager()->currentTrackView(), SIGNAL(configTrack(int)), this, SLOT(slotConfigTrack(int)));
    connect(pCore->projectManager()->currentTrackView(), SIGNAL(updateTracksInfo()), this, SLOT(slotUpdateTrackInfo()));
    connect(pCore->projectManager()->currentTrackView(), SIGNAL(mousePosition(int)), this, SLOT(slotUpdateMousePosition(int)));
    connect(m_projectMonitor->render, SIGNAL(infoProcessingFinished()), pCore->projectManager()->currentTrackView()->projectView(), SLOT(slotInfoProcessingFinished()), Qt::DirectConnection);

    connect(pCore->projectManager()->currentTrackView()->projectView(), SIGNAL(importKeyframes(GraphicsRectItem,QString,int)), this, SLOT(slotProcessImportKeyframes(GraphicsRectItem,QString,int)));

    connect(m_projectMonitor, SIGNAL(renderPosition(int)), pCore->projectManager()->currentTrackView(), SLOT(moveCursorPos(int)));
    connect(m_projectMonitor, SIGNAL(zoneUpdated(QPoint)), pCore->projectManager()->currentTrackView(), SLOT(slotSetZone(QPoint)));
    connect(m_projectMonitor, SIGNAL(zoneUpdated(QPoint)), newDoc, SLOT(setModified()));
    connect(m_clipMonitor, SIGNAL(zoneUpdated(QPoint)), newDoc, SLOT(setModified()));
    connect(m_projectMonitor->render, SIGNAL(refreshDocumentProducers(bool,bool)), newDoc, SLOT(checkProjectClips(bool,bool)));

    connect(newDoc, SIGNAL(addProjectClip(DocClipBase*,bool)), m_projectList, SLOT(slotAddClip(DocClipBase*,bool)));
    connect(newDoc, SIGNAL(resetProjectList()), m_projectList, SLOT(slotResetProjectList()));
    connect(newDoc, SIGNAL(signalDeleteProjectClip(QString)), this, SLOT(slotDeleteClip(QString)));
    connect(newDoc, SIGNAL(updateClipDisplay(QString)), m_projectList, SLOT(slotUpdateClip(QString)));
    connect(newDoc, SIGNAL(selectLastAddedClip(QString)), m_projectList, SLOT(slotSelectClip(QString)));

    connect(newDoc, SIGNAL(docModified(bool)), this, SLOT(slotUpdateDocumentState(bool)));
    connect(newDoc, SIGNAL(guidesUpdated()), this, SLOT(slotGuidesUpdated()));
    connect(newDoc, SIGNAL(saveTimelinePreview(QString)), pCore->projectManager()->currentTrackView(), SLOT(slotSaveTimelinePreview(QString)));

    connect(m_notesWidget, SIGNAL(textChanged()), newDoc, SLOT(setModified()));

    connect(pCore->projectManager()->currentTrackView()->projectView(), SIGNAL(updateClipMarkers(DocClipBase*)), this, SLOT(slotUpdateClipMarkers(DocClipBase*)));
    connect(pCore->projectManager()->currentTrackView(), SIGNAL(showTrackEffects(int,TrackInfo)), this, SLOT(slotTrackSelected(int,TrackInfo)));

    connect(pCore->projectManager()->currentTrackView()->projectView(), SIGNAL(clipItemSelected(ClipItem*,bool)), this, SLOT(slotTimelineClipSelected(ClipItem*,bool)));
    connect(pCore->projectManager()->currentTrackView()->projectView(), SIGNAL(transitionItemSelected(Transition*,int,QPoint,bool)), m_transitionConfig, SLOT(slotTransitionItemSelected(Transition*,int,QPoint,bool)));
    connect(pCore->projectManager()->currentTrackView()->projectView(), SIGNAL(transitionItemSelected(Transition*,int,QPoint,bool)), this, SLOT(slotActivateTransitionView(Transition*)));
    m_zoomSlider->setValue(newDoc->zoom().x());
    connect(pCore->projectManager()->currentTrackView()->projectView(), SIGNAL(zoomIn()), this, SLOT(slotZoomIn()));
    connect(pCore->projectManager()->currentTrackView()->projectView(), SIGNAL(zoomOut()), this, SLOT(slotZoomOut()));
    connect(pCore->projectManager()->currentTrackView(), SIGNAL(setZoom(int)), this, SLOT(slotSetZoom(int)));
    connect(pCore->projectManager()->currentTrackView()->projectView(), SIGNAL(displayMessage(QString,MessageType)), m_messageLabel, SLOT(setMessage(QString,MessageType)));

    connect(pCore->projectManager()->currentTrackView()->projectView(), SIGNAL(showClipFrame(DocClipBase*,QPoint,bool,int)), m_clipMonitor, SLOT(slotSetClipProducer(DocClipBase*,QPoint,bool,int)));
    connect(pCore->projectManager()->currentTrackView()->projectView(), SIGNAL(playMonitor()), m_projectMonitor, SLOT(slotPlay()));

    connect(pCore->projectManager()->currentTrackView()->projectView(), SIGNAL(transitionItemSelected(Transition*,int,QPoint,bool)), m_projectMonitor, SLOT(slotSetSelectedClip(Transition*)));

    connect(m_projectList, SIGNAL(gotFilterJobResults(QString,int,int,stringMap,stringMap)), pCore->projectManager()->currentTrackView()->projectView(), SLOT(slotGotFilterJobResults(QString,int,int,stringMap,stringMap)));

    connect(m_projectList, SIGNAL(addMarkers(QString,QList<CommentedTime>)), pCore->projectManager()->currentTrackView()->projectView(), SLOT(slotAddClipMarker(QString,QList<CommentedTime>)));

    // Effect stack signals
    connect(m_effectStack, SIGNAL(updateEffect(ClipItem*,int,QDomElement,QDomElement,int,bool)), pCore->projectManager()->currentTrackView()->projectView(), SLOT(slotUpdateClipEffect(ClipItem*,int,QDomElement,QDomElement,int,bool)));
    connect(m_effectStack, SIGNAL(updateClipRegion(ClipItem*,int,QString)), pCore->projectManager()->currentTrackView()->projectView(), SLOT(slotUpdateClipRegion(ClipItem*,int,QString)));
    connect(m_effectStack, SIGNAL(removeEffect(ClipItem*,int,QDomElement)), pCore->projectManager()->currentTrackView()->projectView(), SLOT(slotDeleteEffect(ClipItem*,int,QDomElement)));
    connect(m_effectStack, SIGNAL(addEffect(ClipItem*,QDomElement)), pCore->projectManager()->currentTrackView()->projectView(), SLOT(slotAddEffect(ClipItem*,QDomElement)));
    connect(m_effectStack, SIGNAL(changeEffectState(ClipItem*,int,QList<int>,bool)), pCore->projectManager()->currentTrackView()->projectView(), SLOT(slotChangeEffectState(ClipItem*,int,QList<int>,bool)));
    connect(m_effectStack, SIGNAL(changeEffectPosition(ClipItem*,int,QList<int>,int)), pCore->projectManager()->currentTrackView()->projectView(), SLOT(slotChangeEffectPosition(ClipItem*,int,QList<int>,int)));
    
    connect(m_effectStack, SIGNAL(refreshEffectStack(ClipItem*)), pCore->projectManager()->currentTrackView()->projectView(), SLOT(slotRefreshEffects(ClipItem*)));
    connect(m_effectStack, SIGNAL(seekTimeline(int)), pCore->projectManager()->currentTrackView()->projectView(), SLOT(seekCursorPos(int)));
    connect(m_effectStack, SIGNAL(importClipKeyframes(GraphicsRectItem)), pCore->projectManager()->currentTrackView()->projectView(), SLOT(slotImportClipKeyframes(GraphicsRectItem)));
    connect(m_effectStack, SIGNAL(reloadEffects()), this, SLOT(slotReloadEffects()));
    connect(m_effectStack, SIGNAL(displayMessage(QString,int)), this, SLOT(slotGotProgressInfo(QString,int)));
    
    // Transition config signals
    connect(m_transitionConfig, SIGNAL(transitionUpdated(Transition*,QDomElement)), pCore->projectManager()->currentTrackView()->projectView() , SLOT(slotTransitionUpdated(Transition*,QDomElement)));
    connect(m_transitionConfig, SIGNAL(importClipKeyframes(GraphicsRectItem)), pCore->projectManager()->currentTrackView()->projectView() , SLOT(slotImportClipKeyframes(GraphicsRectItem)));
    connect(m_transitionConfig, SIGNAL(seekTimeline(int)), pCore->projectManager()->currentTrackView()->projectView() , SLOT(seekCursorPos(int)));

    connect(pCore->projectManager()->currentTrackView()->projectView(), SIGNAL(activateDocumentMonitor()), m_projectMonitor, SLOT(slotActivateMonitor()));
    connect(pCore->projectManager()->currentTrackView(), SIGNAL(zoneMoved(int,int)), this, SLOT(slotZoneMoved(int,int)));
    connect(m_projectList, SIGNAL(loadingIsOver()), pCore->projectManager()->currentTrackView()->projectView(), SLOT(slotUpdateAllThumbs()));
    pCore->projectManager()->currentTrackView()->projectView()->setContextMenu(m_timelineContextMenu, m_timelineContextClipMenu, m_timelineContextTransitionMenu, m_clipTypeGroup, static_cast<QMenu*>(factory()->container("marker_menu", this)));
    if (m_renderWidget) {
        slotCheckRenderStatus();
        m_renderWidget->setProfile(newDoc->mltProfile());
        m_renderWidget->setGuides(newDoc->guidesXml(), newDoc->projectDuration());
        m_renderWidget->setDocumentPath(newDoc->projectFolder().path(KUrl::AddTrailingSlash));
        m_renderWidget->setRenderProfile(newDoc->getRenderProperties());
    }
    m_commandStack->setActiveStack(newDoc->commandStack());
    KdenliveSettings::setProject_display_ratio(newDoc->dar());

    setCaption(newDoc->description(), newDoc->isModified());
    m_saveAction->setEnabled(newDoc->isModified());
    m_normalEditTool->setChecked(true);
    *projectManagerDoc = newDoc;
    connect(m_projectMonitor, SIGNAL(durationChanged(int)), this, SLOT(slotUpdateProjectDuration(int)));
    pCore->monitorManager()->setDocument(newDoc);
    pCore->projectManager()->currentTrackView()->updateProjectFps();
    newDoc->checkProjectClips();
#ifndef Q_WS_MAC
    m_recMonitor->slotUpdateCaptureFolder(newDoc->projectFolder().path(KUrl::AddTrailingSlash));
#endif
    //Update the mouse position display so it will display in DF/NDF format by default based on the project setting.
    slotUpdateMousePosition(0);

    // Make sure monitor is visible so that it is painted black on startup
    show();
    pCore->monitorManager()->activateMonitor(Kdenlive::ClipMonitor, true);
    // set tool to select tool
    m_buttonSelectTool->setChecked(true);
}

void MainWindow::slotZoneMoved(int start, int end)
{
    pCore->projectManager()->current()->setZone(start, end);
    m_projectMonitor->slotZoneMoved(start, end);
}

void MainWindow::slotGuidesUpdated()
{
    if (m_renderWidget)
        m_renderWidget->setGuides(pCore->projectManager()->current()->guidesXml(), pCore->projectManager()->current()->projectDuration());
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

    KdenliveSettingsDialog* dialog = new KdenliveSettingsDialog(actions, this);
    connect(dialog, SIGNAL(settingsChanged(QString)), this, SLOT(updateConfiguration()));
    connect(dialog, SIGNAL(settingsChanged(QString)), SIGNAL(configurationChanged()));
    connect(dialog, SIGNAL(doResetProfile()), pCore->monitorManager(), SLOT(slotResetProfiles()));
#ifndef Q_WS_MAC
    connect(dialog, SIGNAL(updateCaptureFolder()), this, SLOT(slotUpdateCaptureFolder()));
#endif
    dialog->show();
    if (page != -1) {
        dialog->showPage(page, option);
    }
}

void MainWindow::slotUpdateCaptureFolder()
{

#ifndef Q_WS_MAC
    if (pCore->projectManager()->current())
        m_recMonitor->slotUpdateCaptureFolder(pCore->projectManager()->current()->projectFolder().path(KUrl::AddTrailingSlash));
    else
        m_recMonitor->slotUpdateCaptureFolder(KdenliveSettings::defaultprojectfolder());
#endif
}

void MainWindow::updateConfiguration()
{
    //TODO: we should apply settings to all projects, not only the current one
    if (pCore->projectManager()->currentTrackView()) {
        pCore->projectManager()->currentTrackView()->refresh();
        pCore->projectManager()->currentTrackView()->projectView()->checkAutoScroll();
        pCore->projectManager()->currentTrackView()->checkTrackHeight();
        if (pCore->projectManager()->current())
            pCore->projectManager()->current()->clipManager()->checkAudioThumbs();
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
    if (pCore->projectManager()->currentTrackView()) {
        pCore->projectManager()->currentTrackView()->projectView()->slotUpdateAllThumbs();
    }
    m_buttonVideoThumbs->setChecked(KdenliveSettings::videothumbnails());
}

void MainWindow::slotSwitchAudioThumbs()
{
    KdenliveSettings::setAudiothumbnails(!KdenliveSettings::audiothumbnails());
    if (pCore->projectManager()->currentTrackView()) {
        pCore->projectManager()->currentTrackView()->refresh();
        pCore->projectManager()->currentTrackView()->projectView()->checkAutoScroll();
        if (pCore->projectManager()->current()) {
            pCore->projectManager()->current()->clipManager()->checkAudioThumbs();
        }
    }
    m_buttonAudioThumbs->setChecked(KdenliveSettings::audiothumbnails());
}

void MainWindow::slotSwitchMarkersComments()
{
    KdenliveSettings::setShowmarkers(!KdenliveSettings::showmarkers());
    if (pCore->projectManager()->currentTrackView()) {
        pCore->projectManager()->currentTrackView()->refresh();
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
        if (pCore->projectManager()->currentTrackView()) {
            pCore->projectManager()->currentTrackView()->projectView()->deleteSelectedClips();
        }
    }
}

void MainWindow::slotUpdateClipMarkers(DocClipBase *clip)
{
    if (m_clipMonitor->isActive()) {
        m_clipMonitor->checkOverlay();
    }
    m_clipMonitor->updateMarkers(clip);
}

void MainWindow::slotAddClipMarker()
{
    KdenliveDoc *project = pCore->projectManager()->current();

    DocClipBase *clip = NULL;
    GenTime pos;
    if (m_projectMonitor->isActive()) {
        if (pCore->projectManager()->currentTrackView()) {
            ClipItem *item = pCore->projectManager()->currentTrackView()->projectView()->getActiveClipUnderCursor();
            if (item) {
                pos = GenTime((int)((m_projectMonitor->position() - item->startPos() + item->cropStart()).frames(project->fps()) * item->speed() + 0.5), project->fps());
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
                                                project->timecode(), i18n("Add Marker"), this);
    if (d->exec() == QDialog::Accepted) {
        pCore->projectManager()->currentTrackView()->projectView()->slotAddClipMarker(id, QList <CommentedTime>() << d->newMarker());
        QString hash = clip->getClipHash();
        if (!hash.isEmpty()) project->cacheImage(hash + '#' + QString::number(d->newMarker().time().frames(project->fps())), d->markerImage());
    }
    delete d;
}

void MainWindow::slotDeleteClipMarker()
{
    DocClipBase *clip = NULL;
    GenTime pos;
    if (m_projectMonitor->isActive()) {
        if (pCore->projectManager()->currentTrackView()) {
            ClipItem *item = pCore->projectManager()->currentTrackView()->projectView()->getActiveClipUnderCursor();
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
    pCore->projectManager()->currentTrackView()->projectView()->slotDeleteClipMarker(comment, id, pos);
}

void MainWindow::slotDeleteAllClipMarkers()
{
    DocClipBase *clip = NULL;
    if (m_projectMonitor->isActive()) {
        if (pCore->projectManager()->currentTrackView()) {
            ClipItem *item = pCore->projectManager()->currentTrackView()->projectView()->getActiveClipUnderCursor();
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
    pCore->projectManager()->currentTrackView()->projectView()->slotDeleteAllClipMarkers(clip->getId());
}

void MainWindow::slotEditClipMarker()
{
    DocClipBase *clip = NULL;
    GenTime pos;
    if (m_projectMonitor->isActive()) {
        if (pCore->projectManager()->currentTrackView()) {
            ClipItem *item = pCore->projectManager()->currentTrackView()->projectView()->getActiveClipUnderCursor();
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
                                                pCore->projectManager()->current()->timecode(), i18n("Edit Marker"), this);
    if (d->exec() == QDialog::Accepted) {
        pCore->projectManager()->currentTrackView()->projectView()->slotAddClipMarker(id, QList <CommentedTime>() <<d->newMarker());
        QString hash = clip->getClipHash();
        if (!hash.isEmpty()) pCore->projectManager()->current()->cacheImage(hash + '#' + QString::number(d->newMarker().time().frames(pCore->projectManager()->current()->fps())), d->markerImage());
        if (d->newMarker().time() != pos) {
            // remove old marker
            oldMarker.setMarkerType(-1);
            pCore->projectManager()->currentTrackView()->projectView()->slotAddClipMarker(id, QList <CommentedTime>() <<oldMarker);
        }
    }
    delete d;
}

void MainWindow::slotAddMarkerGuideQuickly()
{
    if (!pCore->projectManager()->currentTrackView() || !pCore->projectManager()->current())
        return;

    if (m_clipMonitor->isActive()) {
        DocClipBase *clip = m_clipMonitor->activeClip();
        GenTime pos = m_clipMonitor->position();

        if (!clip) {
            m_messageLabel->setMessage(i18n("Cannot find clip to add marker"), ErrorMessage);
            return;
        }
        //TODO: allow user to set default marker category
        CommentedTime marker(pos, pCore->projectManager()->current()->timecode().getDisplayTimecode(pos, false), KdenliveSettings::default_marker_type());
        pCore->projectManager()->currentTrackView()->projectView()->slotAddClipMarker(clip->getId(), QList <CommentedTime>() <<marker);
    } else {
        pCore->projectManager()->currentTrackView()->projectView()->slotAddGuide(false);
    }
}

void MainWindow::slotAddGuide()
{
    if (pCore->projectManager()->currentTrackView())
        pCore->projectManager()->currentTrackView()->projectView()->slotAddGuide();
}

void MainWindow::slotInsertSpace()
{
    if (pCore->projectManager()->currentTrackView())
        pCore->projectManager()->currentTrackView()->projectView()->slotInsertSpace();
}

void MainWindow::slotRemoveSpace()
{
    if (pCore->projectManager()->currentTrackView())
        pCore->projectManager()->currentTrackView()->projectView()->slotRemoveSpace();
}

void MainWindow::slotInsertTrack(int ix)
{
    pCore->monitorManager()->activateMonitor(Kdenlive::ProjectMonitor);
    if (pCore->projectManager()->currentTrackView()) {
        if (ix == -1) {
            ix = pCore->projectManager()->currentTrackView()->projectView()->selectedTrack();
        }
        pCore->projectManager()->currentTrackView()->projectView()->slotInsertTrack(ix);
    }
    if (pCore->projectManager()->current()) {
        m_transitionConfig->updateProjectFormat(pCore->projectManager()->current()->mltProfile(),
                                                pCore->projectManager()->current()->timecode(),
                                                pCore->projectManager()->current()->tracksList());
    }
}

void MainWindow::slotDeleteTrack(int ix)
{
    pCore->monitorManager()->activateMonitor(Kdenlive::ProjectMonitor);
    if (pCore->projectManager()->currentTrackView()) {
        if (ix == -1) {
            ix = pCore->projectManager()->currentTrackView()->projectView()->selectedTrack();
        }
        pCore->projectManager()->currentTrackView()->projectView()->slotDeleteTrack(ix);
    }
    if (pCore->projectManager()->current()) {
        m_transitionConfig->updateProjectFormat(pCore->projectManager()->current()->mltProfile(),
                                                pCore->projectManager()->current()->timecode(),
                                                pCore->projectManager()->current()->tracksList());
    }
}

void MainWindow::slotConfigTrack(int ix)
{
    pCore->monitorManager()->activateMonitor(Kdenlive::ProjectMonitor);
    if (pCore->projectManager()->currentTrackView())
        pCore->projectManager()->currentTrackView()->projectView()->slotConfigTracks(ix);
    if (pCore->projectManager()->current())
        m_transitionConfig->updateProjectFormat(pCore->projectManager()->current()->mltProfile(),
                                                pCore->projectManager()->current()->timecode(),
                                                pCore->projectManager()->current()->tracksList());
}

void MainWindow::slotSelectTrack()
{
    pCore->monitorManager()->activateMonitor(Kdenlive::ProjectMonitor);
    if (pCore->projectManager()->currentTrackView()) {
        pCore->projectManager()->currentTrackView()->projectView()->slotSelectClipsInTrack();
    }
}

void MainWindow::slotSelectAllTracks()
{
    pCore->monitorManager()->activateMonitor(Kdenlive::ProjectMonitor);
    if (pCore->projectManager()->currentTrackView())
        pCore->projectManager()->currentTrackView()->projectView()->slotSelectAllClips();
}

void MainWindow::slotEditGuide()
{
    if (pCore->projectManager()->currentTrackView())
        pCore->projectManager()->currentTrackView()->projectView()->slotEditGuide();
}

void MainWindow::slotDeleteGuide()
{
    if (pCore->projectManager()->currentTrackView())
        pCore->projectManager()->currentTrackView()->projectView()->slotDeleteGuide();
}

void MainWindow::slotDeleteAllGuides()
{
    if (pCore->projectManager()->currentTrackView())
        pCore->projectManager()->currentTrackView()->projectView()->slotDeleteAllGuides();
}

void MainWindow::slotCutTimelineClip()
{
    if (pCore->projectManager()->currentTrackView())
        pCore->projectManager()->currentTrackView()->projectView()->cutSelectedClips();
}

void MainWindow::slotInsertClipOverwrite()
{
    if (pCore->projectManager()->currentTrackView()) {
        QStringList data = m_clipMonitor->getZoneInfo();
        pCore->projectManager()->currentTrackView()->projectView()->insertZoneOverwrite(data, pCore->projectManager()->currentTrackView()->inPoint());
    }
}

void MainWindow::slotSelectTimelineClip()
{
    if (pCore->projectManager()->currentTrackView())
        pCore->projectManager()->currentTrackView()->projectView()->selectClip(true);
}

void MainWindow::slotSelectTimelineTransition()
{
    if (pCore->projectManager()->currentTrackView())
        pCore->projectManager()->currentTrackView()->projectView()->selectTransition(true);
}

void MainWindow::slotDeselectTimelineClip()
{
    if (pCore->projectManager()->currentTrackView())
        pCore->projectManager()->currentTrackView()->projectView()->selectClip(false, true);
}

void MainWindow::slotDeselectTimelineTransition()
{
    if (pCore->projectManager()->currentTrackView())
        pCore->projectManager()->currentTrackView()->projectView()->selectTransition(false, true);
}

void MainWindow::slotSelectAddTimelineClip()
{
    if (pCore->projectManager()->currentTrackView())
        pCore->projectManager()->currentTrackView()->projectView()->selectClip(true, true);
}

void MainWindow::slotSelectAddTimelineTransition()
{
    if (pCore->projectManager()->currentTrackView())
        pCore->projectManager()->currentTrackView()->projectView()->selectTransition(true, true);
}

void MainWindow::slotGroupClips()
{
    if (pCore->projectManager()->currentTrackView())
        pCore->projectManager()->currentTrackView()->projectView()->groupClips();
}

void MainWindow::slotUnGroupClips()
{
    if (pCore->projectManager()->currentTrackView())
        pCore->projectManager()->currentTrackView()->projectView()->groupClips(false);
}

void MainWindow::slotEditItemDuration()
{
    if (pCore->projectManager()->currentTrackView())
        pCore->projectManager()->currentTrackView()->projectView()->editItemDuration();
}

void MainWindow::slotAddProjectClip(const KUrl &url, const stringMap &data)
{
    pCore->projectManager()->current()->slotAddClipFile(url, data);
}

void MainWindow::slotAddProjectClipList(const KUrl::List &urls)
{
    pCore->projectManager()->current()->slotAddClipList(urls);
}

void MainWindow::slotAddTransition(QAction *result)
{
    if (!result) return;
    QStringList info = result->data().toStringList();
    if (info.isEmpty()) return;
    QDomElement transition = transitions.getEffectByTag(info.at(1), info.at(2));
    if (pCore->projectManager()->currentTrackView() && !transition.isNull()) {
        pCore->projectManager()->currentTrackView()->projectView()->slotAddTransitionToSelectedClips(transition.cloneNode().toElement());
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
    if (pCore->projectManager()->currentTrackView())
        m_zoomSlider->setValue(pCore->projectManager()->currentTrackView()->fitZoom());
}

void MainWindow::slotSetZoom(int value)
{
    value = qMax(m_zoomSlider->minimum(), value);
    value = qMin(m_zoomSlider->maximum(), value);

    if (pCore->projectManager()->currentTrackView()) {
        pCore->projectManager()->currentTrackView()->slotChangeZoom(value);
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

void MainWindow::slotShowClipProperties(DocClipBase *clip)
{
    KdenliveDoc *project = pCore->projectManager()->current();

    if (clip->clipType() == Text) {
        QString titlepath = project->projectFolder().path(KUrl::AddTrailingSlash) + "titles/";
        if (!clip->getProperty("resource").isEmpty() && clip->getProperty("xmldata").isEmpty()) {
            // template text clip

            // Get the list of existing templates
            QStringList filter;
            filter << "*.kdenlivetitle";
            QStringList templateFiles = QDir(titlepath).entryList(filter, QDir::Files);

            QPointer<QDialog> dia = new QDialog(this);
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
                if (textTemplate.isEmpty()) {
                    textTemplate = dia_ui.template_list->comboBox()->currentText();
                }

                QMap <QString, QString> newprops;

                if (KUrl(textTemplate).path() != templatePath) {
                    // The template was changed
                    newprops.insert("resource", textTemplate);
                }

                if (dia_ui.description->toPlainText() != clip->getProperty("description")) {
                    newprops.insert("description", dia_ui.description->toPlainText());
                }

                QString newtemplate = newprops.value("xmltemplate");
                if (newtemplate.isEmpty()) {
                    newtemplate = templatePath;
                }

                // template modified we need to update xmldata
                QString description = newprops.value("description");
                if (description.isEmpty()) {
                    description = clip->getProperty("description");
                } else {
                    newprops.insert("templatetext", description);
                }

                //newprops.insert("xmldata", m_projectList->generateTemplateXml(newtemplate, description).toString());
                if (!newprops.isEmpty()) {
                    EditClipCommand *command = new EditClipCommand(m_projectList, clip->getId(), clip->currentProperties(newprops), newprops, true);
                    project->commandStack()->push(command);
                }
            }
            delete dia;
            return;
        }
        QString path = clip->getProperty("resource");
        QPointer<TitleWidget> dia_ui = new TitleWidget(KUrl(), project->timecode(), titlepath, m_projectMonitor->render, this);
        QDomDocument doc;
        doc.setContent(clip->getProperty("xmldata"));
        dia_ui->setXml(doc);
        if (dia_ui->exec() == QDialog::Accepted) {
            QMap <QString, QString> newprops;
            newprops.insert("xmldata", dia_ui->xml().toString());
            if (dia_ui->duration() != clip->duration().frames(project->fps())) {
                // duration changed, we need to update duration
                newprops.insert("out", QString::number(dia_ui->duration() - 1));
                int currentLength = QString(clip->producerProperty("length")).toInt();
                if (currentLength <= dia_ui->duration()) {
                    newprops.insert("length", QString::number(dia_ui->duration()));
                } else {
                    newprops.insert("length", clip->producerProperty("length"));
                }
            }
            if (!path.isEmpty()) {
                // we are editing an external file, asked if we want to detach from that file or save the result to that title file.
                if (KMessageBox::questionYesNo(this, i18n("You are editing an external title clip (%1). Do you want to save your changes to the title file or save the changes for this project only?", path), i18n("Save Title"), KGuiItem(i18n("Save to title file")), KGuiItem(i18n("Save in project only"))) == KMessageBox::Yes) {
                    // save to external file
                    dia_ui->saveTitle(path);
                } else {
                    newprops.insert("resource", QString());
                }
            }
            EditClipCommand *command = new EditClipCommand(m_projectList, clip->getId(), clip->currentProperties(newprops), newprops, true);
            project->commandStack()->push(command);
            //pCore->projectManager()->currentTrackView()->projectView()->slotUpdateClip(clip->getId());
            project->setModified(true);
        }
        delete dia_ui;

        //project->editTextClip(clip->getProperty("xml"), clip->getId());
        return;
    }
    
    // Check if we already have a properties dialog opened for that clip
    QList <ClipProperties *> list = findChildren<ClipProperties *>();
    for (int i = 0; i < list.size(); ++i) {
        if (list.at(i)->clipId() == clip->getId()) {
            // We have one dialog, show it
            list.at(i)->raise();
            return;
        }
    }

    // any type of clip but a title
    ClipProperties *dia = new ClipProperties(clip, project->timecode(), project->fps(), this);

    if (clip->clipType() == AV || clip->clipType() == Video || clip->clipType() == Playlist || clip->clipType() == SlideShow) {
        // request clip thumbnails
        connect(project->clipManager(), SIGNAL(gotClipPropertyThumbnail(QString,QImage)), dia, SLOT(slotGotThumbnail(QString,QImage)));
        connect(dia, SIGNAL(requestThumb(QString,QList<int>)), project->clipManager(), SLOT(slotRequestThumbs(QString,QList<int>)));
        project->clipManager()->slotRequestThumbs(QString('?' + clip->getId()), QList<int>() << clip->getClipThumbFrame());
    }
    
    connect(dia, SIGNAL(addMarkers(QString,QList<CommentedTime>)), pCore->projectManager()->currentTrackView()->projectView(), SLOT(slotAddClipMarker(QString,QList<CommentedTime>)));
    connect(dia, SIGNAL(editAnalysis(QString,QString,QString)), pCore->projectManager()->currentTrackView()->projectView(), SLOT(slotAddClipExtraData(QString,QString,QString)));
    connect(pCore->projectManager()->currentTrackView()->projectView(), SIGNAL(updateClipMarkers(DocClipBase*)), dia, SLOT(slotFillMarkersList(DocClipBase*)));
    connect(pCore->projectManager()->currentTrackView()->projectView(), SIGNAL(updateClipExtraData(DocClipBase*)), dia, SLOT(slotUpdateAnalysisData(DocClipBase*)));
    connect(m_projectList, SIGNAL(updateAnalysisData(DocClipBase*)), dia, SLOT(slotUpdateAnalysisData(DocClipBase*)));
    connect(dia, SIGNAL(loadMarkers(QString)), pCore->projectManager()->currentTrackView()->projectView(), SLOT(slotLoadClipMarkers(QString)));
    connect(dia, SIGNAL(saveMarkers(QString)), pCore->projectManager()->currentTrackView()->projectView(), SLOT(slotSaveClipMarkers(QString)));
    connect(dia, SIGNAL(deleteProxy(QString)), m_projectList, SLOT(slotDeleteProxy(QString)));
    connect(dia, SIGNAL(applyNewClipProperties(QString,QMap<QString,QString>,QMap<QString,QString>,bool,bool)), this, SLOT(slotApplyNewClipProperties(QString,QMap<QString,QString>,QMap<QString,QString>,bool,bool)));
    dia->show();
}


void MainWindow::slotApplyNewClipProperties(const QString &id, const QMap <QString, QString> &props, const QMap <QString, QString> &newprops, bool refresh, bool reload)
{
    if (newprops.isEmpty()) {
        return;
    }

    EditClipCommand *command = new EditClipCommand(m_projectList, id, props, newprops, true);
    pCore->projectManager()->current()->commandStack()->push(command);
    pCore->projectManager()->current()->setModified();

    if (refresh) {
        // update clip occurences in timeline
        pCore->projectManager()->currentTrackView()->projectView()->slotUpdateClip(id, reload);
    }
}


void MainWindow::slotShowClipProperties(const QList <DocClipBase *> &cliplist, const QMap<QString, QString> &commonproperties)
{
    QPointer<ClipProperties> dia = new ClipProperties(cliplist,
                                                      pCore->projectManager()->current()->timecode(), commonproperties, this);
    if (dia->exec() == QDialog::Accepted) {
        QUndoCommand *command = new QUndoCommand();
        command->setText(i18n("Edit clips"));
        QMap <QString, QString> newImageProps = dia->properties();
        // Transparency setting applies only for images
        QMap <QString, QString> newProps = newImageProps;
        newProps.remove("transparency");

        for (int i = 0; i < cliplist.count(); ++i) {
            DocClipBase *clip = cliplist.at(i);
            if (clip->clipType() == Image)
                new EditClipCommand(m_projectList, clip->getId(), clip->currentProperties(newImageProps), newImageProps, true, command);
            else
                new EditClipCommand(m_projectList, clip->getId(), clip->currentProperties(newProps), newProps, true, command);
        }
        pCore->projectManager()->current()->commandStack()->push(command);
        for (int i = 0; i < cliplist.count(); ++i) {
            pCore->projectManager()->currentTrackView()->projectView()->slotUpdateClip(cliplist.at(i)->getId(), dia->needsTimelineReload());
        }
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
        if (m_mainClip) {
            m_mainClip->setMainSelectedClip(false);
        }
        if (item) {
            item->setMainSelectedClip(true);
        }
        m_mainClip = item;
    }

    m_effectStack->slotClipItemSelected(item);
    m_projectMonitor->slotSetSelectedClip(item);
    if (raise) {
        m_effectStack->raiseWindow(m_effectStackDock);
    }
}

void MainWindow::slotTrackSelected(int index, const TrackInfo &info, bool raise)
{
    m_effectStack->slotTrackItemSelected(index, info);
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
        if (pCore->projectManager()->currentTrackView()) {
            pCore->projectManager()->currentTrackView()->projectView()->slotSeekToPreviousSnap();
        }
    } else  {
        m_clipMonitor->slotSeekToPreviousSnap();
    }
}

void MainWindow::slotSnapForward()
{
    if (m_projectMonitor->isActive()) {
        if (pCore->projectManager()->currentTrackView()) {
            pCore->projectManager()->currentTrackView()->projectView()->slotSeekToNextSnap();
        }
    } else {
        m_clipMonitor->slotSeekToNextSnap();
    }
}

void MainWindow::slotClipStart()
{
    if (m_projectMonitor->isActive()) {
        if (pCore->projectManager()->currentTrackView()) {
            pCore->projectManager()->currentTrackView()->projectView()->clipStart();
        }
    }
}

void MainWindow::slotClipEnd()
{
    if (m_projectMonitor->isActive()) {
        if (pCore->projectManager()->currentTrackView()) {
            pCore->projectManager()->currentTrackView()->projectView()->clipEnd();
        }
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
        slotSetTool(SelectTool);
    else if (action == m_buttonRazorTool)
        slotSetTool(RazorTool);
    else if (action == m_buttonSpacerTool)
        slotSetTool(SpacerTool);
}

void MainWindow::slotChangeEdit(QAction * action)
{
    if (!pCore->projectManager()->currentTrackView())
        return;

    if (action == m_overwriteEditTool)
        pCore->projectManager()->currentTrackView()->projectView()->setEditMode(OverwriteEdit);
    else if (action == m_insertEditTool)
        pCore->projectManager()->currentTrackView()->projectView()->setEditMode(InsertEdit);
    else
        pCore->projectManager()->currentTrackView()->projectView()->setEditMode(NormalEdit);
}

void MainWindow::slotSetTool(ProjectTool tool)
{
    if (pCore->projectManager()->current() && pCore->projectManager()->currentTrackView()) {
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
        pCore->projectManager()->currentTrackView()->projectView()->setTool(tool);
    }
}

void MainWindow::slotCopy()
{
    if (pCore->projectManager()->currentTrackView())
        pCore->projectManager()->currentTrackView()->projectView()->copyClip();
}

void MainWindow::slotPaste()
{
    if (pCore->projectManager()->currentTrackView())
        pCore->projectManager()->currentTrackView()->projectView()->pasteClip();
}

void MainWindow::slotPasteEffects()
{
    if (pCore->projectManager()->currentTrackView())
        pCore->projectManager()->currentTrackView()->projectView()->pasteClipEffects();
}

void MainWindow::slotFind()
{
    if (!pCore->projectManager()->currentTrackView()) {
        return;
    }

    m_projectSearch->setEnabled(false);
    m_findActivated = true;
    m_findString.clear();
    pCore->projectManager()->currentTrackView()->projectView()->initSearchStrings();
    statusBar()->showMessage(i18n("Starting -- find text as you type"));
    m_findTimer.start(5000);
    qApp->installEventFilter(this);
}

void MainWindow::slotFindNext()
{
    if (pCore->projectManager()->currentTrackView() && pCore->projectManager()->currentTrackView()->projectView()->findNextString(m_findString)) {
        statusBar()->showMessage(i18n("Found: %1", m_findString));
    } else {
        statusBar()->showMessage(i18n("Reached end of project"));
    }
    m_findTimer.start(4000);
}

void MainWindow::findAhead()
{
    if (pCore->projectManager()->currentTrackView() && pCore->projectManager()->currentTrackView()->projectView()->findString(m_findString)) {
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
    if (pCore->projectManager()->currentTrackView()) pCore->projectManager()->currentTrackView()->projectView()->clearSearchStrings();
    m_projectSearch->setEnabled(true);
    removeEventFilter(this);
}

void MainWindow::slotClipInTimeline(const QString &clipId)
{
    if (pCore->projectManager()->currentTrackView()) {
        QList<ItemInfo> matching = pCore->projectManager()->currentTrackView()->projectView()->findId(clipId);

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
    if (pCore->projectManager()->currentTrackView()) {
        QStringList clipIds;
        if (m_mainClip) {
            clipIds << m_mainClip->clipProducer();
        } else {
            clipIds = pCore->projectManager()->currentTrackView()->projectView()->selectedClips();
        }
        if (clipIds.isEmpty()) {
            return;
        }
        m_projectListDock->raise();
        m_projectList->selectItemById(clipIds.at(0));
        if (m_projectMonitor->isActive()) {
            slotSwitchMonitors();
        }
    }
}

/*void MainWindow::slotClipToProjectTree()
{
    if (pCore->projectManager()->currentTrackView()) {
    const QList<ClipItem *> clips =  pCore->projectManager()->currentTrackView()->projectView()->selectedClipItems();
        if (clips.isEmpty()) return;
        for (int i = 0; i < clips.count(); ++i) {
        m_projectList->slotAddXmlClip(clips.at(i)->itemXml());
        }
        //m_projectList->selectItemById(clipIds.at(i));
    }
}*/

void MainWindow::slotSelectClipInTimeline()
{
    if (pCore->projectManager()->currentTrackView()) {
        QAction *action = qobject_cast<QAction *>(sender());
        QStringList data = action->data().toStringList();
        pCore->projectManager()->currentTrackView()->projectView()->selectFound(data.at(0), data.at(1));
    }
}

void MainWindow::keyPressEvent(QKeyEvent *ke)
{
    if (m_findActivated) {
        if (ke->key() == Qt::Key_Backspace) {
            m_findString = m_findString.left(m_findString.length() - 1);

            if (!m_findString.isEmpty()) {
                findAhead();
            } else {
                findTimeout();
            }

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
    if (isMinimized() && pCore->monitorManager())
        pCore->monitorManager()->stopActiveMonitor();
}

bool MainWindow::eventFilter(QObject *obj, QEvent *event)
{
    if (m_findActivated) {
        if (event->type() == QEvent::ShortcutOverride) {
            QKeyEvent* ke = static_cast<QKeyEvent*>(event);
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


void MainWindow::slotSaveZone(Render *render, const QPoint &zone, DocClipBase *baseClip, KUrl path)
{
    QPointer<KDialog> dialog = new KDialog(this);
    dialog->setCaption("Save clip zone");
    dialog->setButtons(KDialog::Ok | KDialog::Cancel);

    QWidget *widget = new QWidget(dialog);
    dialog->setMainWidget(widget);

    QVBoxLayout *vbox = new QVBoxLayout(widget);
    QLabel *label1 = new QLabel(i18n("Save clip zone as:"), this);
    if (path.isEmpty()) {
        QString tmppath = pCore->projectManager()->current()->projectFolder().path(KUrl::AddTrailingSlash);
        if (baseClip == NULL) {
            tmppath.append("untitled.mlt");
        } else {
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
                delete dialog;
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
    delete dialog;
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
    if (pCore->projectManager()->currentTrackView())
        pCore->projectManager()->currentTrackView()->projectView()->setInPoint();
}

void MainWindow::slotResizeItemEnd()
{
    if (pCore->projectManager()->currentTrackView())
        pCore->projectManager()->currentTrackView()->projectView()->setOutPoint();
}

int MainWindow::getNewStuff(const QString &configFile)
{
    KNS3::Entry::List entries;
#if KDE_IS_VERSION(4,3,80)
    QPointer<KNS3::DownloadDialog> dialog = new KNS3::DownloadDialog(configFile);
    if (dialog->exec()) entries = dialog->changedEntries();
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
        pCore->projectManager()->currentTrackView()->projectView()->reloadTransitionLumas();
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
    if (pCore->projectManager()->currentTrackView())
        pCore->projectManager()->currentTrackView()->projectView()->autoTransition();
}

void MainWindow::slotSplitAudio()
{
    if (pCore->projectManager()->currentTrackView())
        pCore->projectManager()->currentTrackView()->projectView()->splitAudio();
}

void MainWindow::slotSetAudioAlignReference()
{
    if (pCore->projectManager()->currentTrackView()) {
        pCore->projectManager()->currentTrackView()->projectView()->setAudioAlignReference();
    }
}

void MainWindow::slotAlignAudio()
{
    if (pCore->projectManager()->currentTrackView()) {
        pCore->projectManager()->currentTrackView()->projectView()->alignAudio();
    }
}

void MainWindow::slotUpdateClipType(QAction *action)
{
    if (pCore->projectManager()->currentTrackView()) {
        bool videoOnly = false;
        bool audioOnly = false;
        if (action->data().toString() == "clip_audio_only") 
            audioOnly = true;
        else if (action->data().toString() == "clip_video_only")
            videoOnly = true;
        pCore->projectManager()->currentTrackView()->projectView()->setClipType(videoOnly, audioOnly);
    }
}

void MainWindow::slotDvdWizard(const QString &url)
{
    // We must stop the monitors since we create a new on in the dvd wizard
    pCore->monitorManager()->activateMonitor(Kdenlive::DvdMonitor);
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
                QAction *action=actionMenu->addAction(i18n("Stabilize (vid.stab)"));
                action->setData("vidstab");
                connect(action,SIGNAL(triggered()), this, SLOT(slotStartClipAction()));
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
                action->setData("motion_est");
                connect(action,SIGNAL(triggered()), this, SLOT(slotStartClipAction()));
            }
        }
        if (KdenliveSettings::producerslist().contains("framebuffer")) {
            QAction *action=actionMenu->addAction(i18n("Reverse clip"));
            action->setData("framebuffer");
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
}

void MainWindow::slotTranscode(const KUrl::List &urls)
{
    QString params;
    QString desc;
    QString condition;
    if (urls.isEmpty()) {
        QAction *action = qobject_cast<QAction *>(sender());
        QStringList data = action->data().toStringList();
        params = data.at(0);
        if (data.count() > 1)
            desc = data.at(1);
        if (data.count() > 3)
            condition = data.at(3);
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
    if (scriptExport) {
        //QString scriptsFolder = project->projectFolder().path(KUrl::AddTrailingSlash) + "scripts/";
        QString path = m_renderWidget->getFreeScriptName(project->url());
        QPointer<KUrlRequesterDialog> getUrl = new KUrlRequesterDialog(path, i18n("Create Render Script"), this);
        getUrl->fileDialog()->setMode(KFile::File);
        getUrl->fileDialog()->setOperationMode(KFileDialog::Saving);
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
        if (!zoneOnly) out = (int) GenTime(project->projectDuration()).frames(project->fps());
        else {
            in = pCore->projectManager()->currentTrackView()->inPoint();
            out = pCore->projectManager()->currentTrackView()->outPoint();
        }
        QDomDocument doc;
        QDomElement chapters = doc.createElement("chapters");
        chapters.setAttribute("fps", project->fps());
        doc.appendChild(chapters);

        QDomElement guidesxml = project->guidesXml();
        QDomNodeList nodes = guidesxml.elementsByTagName("guide");
        for (int i = 0; i < nodes.count(); ++i) {
            QDomElement e = nodes.item(i).toElement();
            if (!e.isNull()) {
                QString comment = e.attribute("comment");
                int time = (int) GenTime(e.attribute("time").toDouble()).frames(project->fps());
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
            if (pCore->projectManager()->currentTrackView()->projectView()->hasGuide(out, 0) == -1) {
                // Always insert a guide in pos 0
                QDomElement chapter = doc.createElement("chapter");
                chapters.insertBefore(chapter, QDomNode());
                chapter.setAttribute("title", i18nc("the first in a list of chapters", "Start"));
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
        exportAudio = pCore->projectManager()->currentTrackView()->checkProjectAudio();
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
    if (m_projectList->useProxy() && !m_renderWidget->proxyRendering()) {
        QString root = doc.documentElement().attribute("root");

        // replace proxy clips with originals
        QMap <QString, QString> proxies = m_projectList->getProxies();

        QDomNodeList producers = doc.elementsByTagName("producer");
        QString producerResource;
        QString suffix;
        for (uint n = 0; n < producers.length(); ++n) {
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
    m_renderWidget->slotExport(scriptExport, pCore->projectManager()->currentTrackView()->inPoint(), pCore->projectManager()->currentTrackView()->outPoint(), project->metadata(), playlistPath, scriptPath, exportAudio);
}

void MainWindow::slotUpdateTimecodeFormat(int ix)
{
    KdenliveSettings::setFrametimecode(ix == 1);
    m_clipMonitor->updateTimecodeFormat();
    m_projectMonitor->updateTimecodeFormat();
    m_transitionConfig->updateTimecodeFormat();
    m_effectStack->updateTimecodeFormat();
    //pCore->projectManager()->currentTrackView()->projectView()->clearSelection();
    pCore->projectManager()->currentTrackView()->updateRuler();
    slotUpdateMousePosition(pCore->projectManager()->currentTrackView()->projectView()->getMousePos());
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
    m_transitionConfig->updateProjectFormat(pCore->projectManager()->current()->mltProfile(),
                                            pCore->projectManager()->current()->timecode(),
                                            pCore->projectManager()->current()->tracksList());
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

        for ( int i = 0; i < 3 ; ++i ) {
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
    pCore->monitorManager()->slotSwitchMonitors(!m_clipMonitor->isActive());
    if (m_projectMonitor->isActive()) pCore->projectManager()->currentTrackView()->projectView()->setFocus();
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
    if (pCore->projectManager()->currentTrackView() == NULL || m_clipMonitor->activeClip() == NULL) return;
    QStringList info = m_clipMonitor->getZoneInfo();
    pCore->projectManager()->currentTrackView()->projectView()->insertClipCut(m_clipMonitor->activeClip(), info.at(1).toInt(), info.at(2).toInt());
}


void MainWindow::slotDeleteProjectClips(const QStringList &ids, const QMap<QString, QString> &folderids)
{
    if (pCore->projectManager()->currentTrackView()) {
        if (!ids.isEmpty()) {
            for (int i = 0; i < ids.size(); ++i) {
                pCore->projectManager()->currentTrackView()->slotDeleteClip(ids.at(i));
            }
            pCore->projectManager()->current()->clipManager()->slotDeleteClips(ids);
        }
        if (!folderids.isEmpty()) m_projectList->deleteProjectFolder(folderids);
        pCore->projectManager()->current()->setModified(true);
    }
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
        connect(m_stopmotion, SIGNAL(addOrUpdateSequence(QString)), m_projectList, SLOT(slotAddOrUpdateSequence(QString)));
        //for (int i = 0; i < m_gfxScopesList.count(); ++i) {
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
        KStandardDirs::makeDir(pCore->projectManager()->current()->projectFolder().path(KUrl::AddTrailingSlash) + "proxy/");
    m_projectList->updateProxyConfig();
}

void MainWindow::slotInsertNotesTimecode()
{
    int frames = m_projectMonitor->render->seekPosition().frames(pCore->projectManager()->current()->fps());
    QString position = pCore->projectManager()->current()->timecode().getTimecodeFromFrames(frames);
    m_notesWidget->insertHtml("<a href=\"" + QString::number(frames) + "\">" + position + "</a> ");
}

void MainWindow::slotArchiveProject()
{
    QList <DocClipBase*> list = m_projectList->documentClipList();
    QDomDocument doc = pCore->projectManager()->current()->xmlSceneList(m_projectMonitor->sceneList(), m_projectList->expandedFolders());
    QPointer<ArchiveWidget> d = new ArchiveWidget(pCore->projectManager()->current()->url().fileName(), doc, list, pCore->projectManager()->currentTrackView()->projectView()->extractTransitionsLumas(), this);
    if (d->exec()) {
        m_messageLabel->setMessage(i18n("Archiving project"), OperationCompletedMessage);
    }
    delete d;
}

void MainWindow::slotElapsedTime()
{
    kDebug()<<"-----------------------------------------\n"<<"Time elapsed: "<<m_timer.elapsed()<<"\n-------------------------";
}


void MainWindow::slotDownloadResources()
{
    QString currentFolder;
    if (pCore->projectManager()->current())
        currentFolder = pCore->projectManager()->current()->projectFolder().path();
    else
        currentFolder = KdenliveSettings::defaultprojectfolder();
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
    if (pCore->projectManager()->currentTrackView()) {
        pCore->projectManager()->currentTrackView()->updatePalette();
    }
}

void MainWindow::slotSaveTimelineClip()
{
    if (pCore->projectManager()->currentTrackView() && m_projectMonitor->render) {
        ClipItem *clip = pCore->projectManager()->currentTrackView()->projectView()->getActiveClipUnderCursor(true);
        if (!clip) {
            m_messageLabel->setMessage(i18n("Select a clip to save"), InformationMessage);
            return;
        }
        KUrl url = KFileDialog::getSaveUrl(pCore->projectManager()->current()->projectFolder(), "video/mlt-playlist");
        if (!url.isEmpty()) {
            m_projectMonitor->render->saveClip(pCore->projectManager()->current()->tracksCount() - clip->track(), clip->startPos(), url);
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
    pCore->projectManager()->currentTrackView()->projectView()->slotAlignPlayheadToMousePos();
}

QDockWidget *MainWindow::addDock(const QString &title, const QString &objectName, QWidget* widget, Qt::DockWidgetArea area)
{
    QDockWidget *dockWidget = new QDockWidget(title, this);
    dockWidget->setObjectName(objectName);
    dockWidget->setWidget(widget);
    addDockWidget(area, dockWidget);
    return dockWidget;
}

#include "mainwindow.moc"

#ifdef DEBUG_MAINW
#undef DEBUG_MAINW
#endif
