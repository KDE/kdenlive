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
#include "effectstackview.h"
#include "transitionsettings.h"
#include "renderwidget.h"
#include "renderer.h"
#ifndef NO_JOGSHUTTLE
#include "jogshuttle.h"
#endif /* NO_JOGSHUTTLE */
#include "clipproperties.h"
#include "wizard.h"
#include "editclipcommand.h"
#include "titlewidget.h"
#include "markerdialog.h"
#include "clipitem.h"
#include "interfaces.h"
#include "kdenlive-config.h"
#include "cliptranscode.h"
#include "ui_templateclip_ui.h"

#include <KApplication>
#include <KAction>
#include <KLocale>
#include <KGlobal>
#include <KActionCollection>
#include <KStandardAction>
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
#include <locale.h>
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
#endif /* KDE_IS_VERSION(4,3,80) */
#include <KToolBar>
#include <KColorScheme>

#include <QTextStream>
#include <QTimer>
#include <QAction>
#include <QKeyEvent>
#include <QInputDialog>
#include <QDesktopWidget>
#include <QBitmap>

#include <stdlib.h>

static const char version[] = VERSION;

static const int ID_TIMELINE_POS = 0;

namespace Mlt
{
class Producer;
};

EffectsList MainWindow::videoEffects;
EffectsList MainWindow::audioEffects;
EffectsList MainWindow::customEffects;
EffectsList MainWindow::transitions;

MainWindow::MainWindow(const QString &MltPath, const KUrl & Url, QWidget *parent) :
        KXmlGuiWindow(parent),
        m_activeDocument(NULL),
        m_activeTimeline(NULL),
        m_renderWidget(NULL),
#ifndef NO_JOGSHUTTLE
        m_jogProcess(NULL),
#endif /* NO_JOGSHUTTLE */
        m_findActivated(false)
{

    // Create DBus interface
    new MainWindowAdaptor(this);
    QDBusConnection dbus = QDBusConnection::sessionBus();
    dbus.registerObject("/MainWindow", this);

    setlocale(LC_NUMERIC, "POSIX");
    if (!KdenliveSettings::colortheme().isEmpty()) slotChangePalette(NULL, KdenliveSettings::colortheme());
    setFont(KGlobalSettings::toolBarFont());
    parseProfiles(MltPath);
    m_commandStack = new QUndoGroup;
    m_timelineArea = new KTabWidget(this);
    m_timelineArea->setTabReorderingEnabled(true);
    m_timelineArea->setTabBarHidden(true);

    QToolButton *closeTabButton = new QToolButton;
    connect(closeTabButton, SIGNAL(clicked()), this, SLOT(closeCurrentDocument()));
    closeTabButton->setIcon(KIcon("tab-close"));
    closeTabButton->adjustSize();
    closeTabButton->setToolTip(i18n("Close the current tab"));
    m_timelineArea->setCornerWidget(closeTabButton);
    connect(m_timelineArea, SIGNAL(currentChanged(int)), this, SLOT(activateDocument()));

    connect(&m_findTimer, SIGNAL(timeout()), this, SLOT(findTimeout()));
    m_findTimer.setSingleShot(true);

    // FIXME: the next call returns a newly allocated object, which leaks
    initEffects::parseEffectFiles();
    //initEffects::parseCustomEffectsFile();

    m_monitorManager = new MonitorManager();

    m_projectListDock = new QDockWidget(i18n("Project Tree"), this);
    m_projectListDock->setObjectName("project_tree");
    m_projectList = new ProjectList(this);
    m_projectListDock->setWidget(m_projectList);
    addDockWidget(Qt::TopDockWidgetArea, m_projectListDock);

    m_shortcutRemoveFocus = new QShortcut(QKeySequence("Esc"), this);
    connect(m_shortcutRemoveFocus, SIGNAL(activated()), this, SLOT(slotRemoveFocus()));

    m_effectListDock = new QDockWidget(i18n("Effect List"), this);
    m_effectListDock->setObjectName("effect_list");
    m_effectList = new EffectsListView();

    //m_effectList = new KListWidget(this);
    m_effectListDock->setWidget(m_effectList);
    addDockWidget(Qt::TopDockWidgetArea, m_effectListDock);

    m_effectStackDock = new QDockWidget(i18n("Effect Stack"), this);
    m_effectStackDock->setObjectName("effect_stack");
    m_effectStack = new EffectStackView(this);
    m_effectStackDock->setWidget(m_effectStack);
    addDockWidget(Qt::TopDockWidgetArea, m_effectStackDock);

    m_transitionConfigDock = new QDockWidget(i18n("Transition"), this);
    m_transitionConfigDock->setObjectName("transition");
    m_transitionConfig = new TransitionSettings(this);
    m_transitionConfigDock->setWidget(m_transitionConfig);
    addDockWidget(Qt::TopDockWidgetArea, m_transitionConfigDock);

    KdenliveSettings::setCurrent_profile(KdenliveSettings::default_profile());
    m_fileOpenRecent = KStandardAction::openRecent(this, SLOT(openFile(const KUrl &)), actionCollection());
    readOptions();
    m_fileRevert = KStandardAction::revert(this, SLOT(slotRevert()), actionCollection());
    m_fileRevert->setEnabled(false);

    //slotDetectAudioDriver();

    m_clipMonitorDock = new QDockWidget(i18n("Clip Monitor"), this);
    m_clipMonitorDock->setObjectName("clip_monitor");
    m_clipMonitor = new Monitor("clip", m_monitorManager, QString(), this);
    m_clipMonitorDock->setWidget(m_clipMonitor);
    addDockWidget(Qt::TopDockWidgetArea, m_clipMonitorDock);
    //m_clipMonitor->stop();

    m_projectMonitorDock = new QDockWidget(i18n("Project Monitor"), this);
    m_projectMonitorDock->setObjectName("project_monitor");
    m_projectMonitor = new Monitor("project", m_monitorManager, QString(), this);
    m_projectMonitorDock->setWidget(m_projectMonitor);
    addDockWidget(Qt::TopDockWidgetArea, m_projectMonitorDock);

#ifndef Q_WS_MAC
    m_recMonitorDock = new QDockWidget(i18n("Record Monitor"), this);
    m_recMonitorDock->setObjectName("record_monitor");
    m_recMonitor = new RecMonitor("record", this);
    m_recMonitorDock->setWidget(m_recMonitor);
    addDockWidget(Qt::TopDockWidgetArea, m_recMonitorDock);

    connect(m_recMonitor, SIGNAL(addProjectClip(KUrl)), this, SLOT(slotAddProjectClip(KUrl)));
    connect(m_recMonitor, SIGNAL(showConfigDialog(int, int)), this, SLOT(slotPreferences(int, int)));
#endif

    m_undoViewDock = new QDockWidget(i18n("Undo History"), this);
    m_undoViewDock->setObjectName("undo_history");
    m_undoView = new QUndoView(this);
    m_undoView->setCleanIcon(KIcon("edit-clear"));
    m_undoView->setEmptyLabel(i18n("Clean"));
    m_undoViewDock->setWidget(m_undoView);
    m_undoView->setGroup(m_commandStack);
    addDockWidget(Qt::TopDockWidgetArea, m_undoViewDock);

    //overviewDock = new QDockWidget(i18n("Project Overview"), this);
    //overviewDock->setObjectName("project_overview");
    //m_overView = new CustomTrackView(NULL, NULL, this);
    //overviewDock->setWidget(m_overView);
    //addDockWidget(Qt::TopDockWidgetArea, overviewDock);

    setupActions();
    //tabifyDockWidget(projectListDock, effectListDock);
    tabifyDockWidget(m_projectListDock, m_effectStackDock);
    tabifyDockWidget(m_projectListDock, m_transitionConfigDock);
    //tabifyDockWidget(projectListDock, undoViewDock);


    tabifyDockWidget(m_clipMonitorDock, m_projectMonitorDock);
#ifndef Q_WS_MAC
    tabifyDockWidget(m_clipMonitorDock, m_recMonitorDock);
#endif
    setCentralWidget(m_timelineArea);
    setupGUI();

    // Find QDockWidget tab bars and show / hide widget title bars on right click
    QList <QTabBar *> tabs = findChildren<QTabBar *>();
    for (int i = 0; i < tabs.count(); i++) {
        tabs.at(i)->setContextMenuPolicy(Qt::CustomContextMenu);
        connect(tabs.at(i), SIGNAL(customContextMenuRequested(const QPoint &)), this, SLOT(slotSwitchTitles()));
    }

    /*ScriptingPart* sp = new ScriptingPart(this, QStringList());
    guiFactory()->addClient(sp);*/

    loadPlugins();
    loadTranscoders();
    //kDebug() << factory() << " " << factory()->container("video_effects_menu", this);

    m_projectMonitor->setupMenu(static_cast<QMenu*>(factory()->container("monitor_go", this)), m_playZone, m_loopZone);
    m_clipMonitor->setupMenu(static_cast<QMenu*>(factory()->container("monitor_go", this)), m_playZone, m_loopZone, static_cast<QMenu*>(factory()->container("marker_menu", this)));
    m_projectList->setupGeneratorMenu(static_cast<QMenu*>(factory()->container("generators", this)),
                                      static_cast<QMenu*>(factory()->container("transcoders", this)),
                                      static_cast<QMenu*>(factory()->container("clip_in_timeline", this)));

    QAction *action;
    // build themes menus
    QMenu *themesMenu = static_cast<QMenu*>(factory()->container("themes_menu", this));
    QActionGroup *themegroup = new QActionGroup(this);
    themegroup->setExclusive(true);
    action = new QAction(i18n("Default"), this);
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
        action = new QAction(name, this);
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

    // build effects menus
    QMenu *videoEffectsMenu = static_cast<QMenu*>(factory()->container("video_effects_menu", this));

    QStringList effectInfo;
    QMap<QString, QStringList> effectsList;
    for (int ix = 0; ix < videoEffects.count(); ix++) {
        effectInfo = videoEffects.effectIdInfo(ix);
        effectsList.insert(effectInfo.at(0).toLower(), effectInfo);
    }

    foreach(const QStringList &value, effectsList) {
        action = new QAction(value.at(0), this);
        action->setData(value);
        videoEffectsMenu->addAction(action);
    }

    QMenu *audioEffectsMenu = static_cast<QMenu*>(factory()->container("audio_effects_menu", this));


    effectsList.clear();
    for (int ix = 0; ix < audioEffects.count(); ix++) {
        effectInfo = audioEffects.effectIdInfo(ix);
        effectsList.insert(effectInfo.at(0).toLower(), effectInfo);
    }

    foreach(const QStringList &value, effectsList) {
        action = new QAction(value.at(0), this);
        action->setData(value);
        audioEffectsMenu->addAction(action);
    }

    m_customEffectsMenu = static_cast<QMenu*>(factory()->container("custom_effects_menu", this));

    if (customEffects.isEmpty()) m_customEffectsMenu->setEnabled(false);
    else m_customEffectsMenu->setEnabled(true);

    effectsList.clear();
    for (int ix = 0; ix < customEffects.count(); ix++) {
        effectInfo = customEffects.effectIdInfo(ix);
        effectsList.insert(effectInfo.at(0).toLower(), effectInfo);
    }

    foreach(const QStringList &value, effectsList) {
        action = new QAction(value.at(0), this);
        action->setData(value);
        m_customEffectsMenu->addAction(action);
    }

    QMenu *newEffect = new QMenu(this);
    newEffect->addMenu(videoEffectsMenu);
    newEffect->addMenu(audioEffectsMenu);
    newEffect->addMenu(m_customEffectsMenu);
    m_effectStack->setMenu(newEffect);


    QMenu *viewMenu = static_cast<QMenu*>(factory()->container("dockwindows", this));
    const QList<QAction *> viewActions = createPopupMenu()->actions();
    viewMenu->insertActions(NULL, viewActions);

    connect(videoEffectsMenu, SIGNAL(triggered(QAction *)), this, SLOT(slotAddVideoEffect(QAction *)));
    connect(audioEffectsMenu, SIGNAL(triggered(QAction *)), this, SLOT(slotAddAudioEffect(QAction *)));
    connect(m_customEffectsMenu, SIGNAL(triggered(QAction *)), this, SLOT(slotAddCustomEffect(QAction *)));

    m_timelineContextMenu = new QMenu(this);
    m_timelineContextClipMenu = new QMenu(this);
    m_timelineContextTransitionMenu = new QMenu(this);


    QMenu *transitionsMenu = new QMenu(i18n("Add Transition"), this);
    QStringList effects = transitions.effectNames();

    effectsList.clear();
    for (int ix = 0; ix < transitions.count(); ix++) {
        effectInfo = transitions.effectIdInfo(ix);
        effectsList.insert(effectInfo.at(0).toLower(), effectInfo);
    }
    foreach(const QStringList &value, effectsList) {
        action = new QAction(value.at(0), this);
        action->setData(value);
        transitionsMenu->addAction(action);
    }
    connect(transitionsMenu, SIGNAL(triggered(QAction *)), this, SLOT(slotAddTransition(QAction *)));

    m_timelineContextMenu->addAction(actionCollection()->action("insert_space"));
    m_timelineContextMenu->addAction(actionCollection()->action("delete_space"));
    m_timelineContextMenu->addAction(actionCollection()->action(KStandardAction::name(KStandardAction::Paste)));

    m_timelineContextClipMenu->addAction(actionCollection()->action("edit_item_duration"));
    m_timelineContextClipMenu->addAction(actionCollection()->action("delete_timeline_clip"));
    m_timelineContextClipMenu->addAction(actionCollection()->action("group_clip"));
    m_timelineContextClipMenu->addAction(actionCollection()->action("ungroup_clip"));
    m_timelineContextClipMenu->addAction(actionCollection()->action("cut_timeline_clip"));
    m_timelineContextClipMenu->addAction(actionCollection()->action(KStandardAction::name(KStandardAction::Copy)));
    m_timelineContextClipMenu->addAction(actionCollection()->action("paste_effects"));
    m_timelineContextClipMenu->addAction(actionCollection()->action("split_audio"));

    QMenu *markersMenu = (QMenu*)(factory()->container("marker_menu", this));
    m_timelineContextClipMenu->addMenu(markersMenu);
    m_timelineContextClipMenu->addMenu(transitionsMenu);
    m_timelineContextClipMenu->addMenu(videoEffectsMenu);
    m_timelineContextClipMenu->addMenu(audioEffectsMenu);
    //TODO: re-enable custom effects menu when it is implemented
    m_timelineContextClipMenu->addMenu(m_customEffectsMenu);

    m_timelineContextTransitionMenu->addAction(actionCollection()->action("edit_item_duration"));
    m_timelineContextTransitionMenu->addAction(actionCollection()->action("delete_timeline_clip"));
    m_timelineContextTransitionMenu->addAction(actionCollection()->action(KStandardAction::name(KStandardAction::Copy)));

    m_timelineContextTransitionMenu->addAction(actionCollection()->action("auto_transition"));

    connect(m_projectMonitorDock, SIGNAL(visibilityChanged(bool)), m_projectMonitor, SLOT(refreshMonitor(bool)));
    connect(m_clipMonitorDock, SIGNAL(visibilityChanged(bool)), m_clipMonitor, SLOT(refreshMonitor(bool)));
    //connect(m_monitorManager, SIGNAL(connectMonitors()), this, SLOT(slotConnectMonitors()));
    connect(m_monitorManager, SIGNAL(raiseClipMonitor(bool)), this, SLOT(slotRaiseMonitor(bool)));
    connect(m_effectList, SIGNAL(addEffect(const QDomElement)), this, SLOT(slotAddEffect(const QDomElement)));
    connect(m_effectList, SIGNAL(reloadEffects()), this, SLOT(slotReloadEffects()));

    m_monitorManager->initMonitors(m_clipMonitor, m_projectMonitor);
    slotConnectMonitors();

    // Disable drop B frames, see Kdenlive issue #1330, see also kdenlivesettingsdialog.cpp
    KdenliveSettings::setDropbframes(false);

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

#ifndef NO_JOGSHUTTLE
    activateShuttleDevice();
#endif /* NO_JOGSHUTTLE */
    m_projectListDock->raise();
}

void MainWindow::queryQuit()
{
    if (queryClose()) {
        if (m_projectMonitor) m_projectMonitor->stop();
        if (m_clipMonitor) m_clipMonitor->stop();
        delete m_effectStack;
        delete m_activeTimeline;
#ifndef Q_WS_MAC
        // This sometimes causes crash on exit on OS X for some reason.
        delete m_projectMonitor;
        delete m_clipMonitor;
#endif
        delete m_activeDocument;
        delete m_shortcutRemoveFocus;
        Mlt::Factory::close();
        kapp->quit();
    }
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
    if (m_activeDocument && m_activeDocument->isModified()) {
        switch (KMessageBox::warningYesNoCancel(this, i18n("Save changes to document?"))) {
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
    foreach(QObject *plugin, QPluginLoader::staticInstances())
    populateMenus(plugin);

    QStringList directories = KGlobal::dirs()->findDirs("module", QString());
    QStringList filters;
    filters << "libkdenlive*";
    foreach(const QString &folder, directories) {
        kDebug() << "// PARSING FIOLER: " << folder;
        QDir pluginsDir(folder);
        foreach(const QString &fileName, pluginsDir.entryList(filters, QDir::Files)) {
            kDebug() << "// FOUND PLUGIN: " << fileName << "= " << pluginsDir.absoluteFilePath(fileName);
            QPluginLoader loader(pluginsDir.absoluteFilePath(fileName));
            QObject *plugin = loader.instance();
            if (plugin) {
                populateMenus(plugin);
                m_pluginFileNames += fileName;
            } else kDebug() << "// ERROR LOADING PLUGIN: " << fileName << ", " << loader.errorString();
        }
    }
    //exit(1);
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
    foreach(const QString &text, texts) {
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

    KUrl clipUrl = iGenerator->generatedClip(action->data().toString(), m_activeDocument->projectFolder(), QStringList(), QStringList(), 25, 720, 576);
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
    m_customEffectsMenu->clear();
    initEffects::parseCustomEffectsFile();
    QAction *action;
    QStringList effectInfo;
    QMap<QString, QStringList> effectsList;

    for (int ix = 0; ix < customEffects.count(); ix++) {
        effectInfo = customEffects.effectIdInfo(ix);
        effectsList.insert(effectInfo.at(0).toLower(), effectInfo);
    }
    if (effectsList.isEmpty()) {
        m_customEffectsMenu->setEnabled(false);
        return;
    } else m_customEffectsMenu->setEnabled(true);

    foreach(const QStringList &value, effectsList) {
        action = new QAction(value.at(0), this);
        action->setData(value);
        m_customEffectsMenu->addAction(action);
    }
    m_effectList->reloadEffectList();
}

#ifndef NO_JOGSHUTTLE
void MainWindow::activateShuttleDevice()
{
    delete m_jogProcess;
    m_jogProcess = NULL;
    if (KdenliveSettings::enableshuttle() == false) return;
    m_jogProcess = new JogShuttle(KdenliveSettings::shuttledevice());
    connect(m_jogProcess, SIGNAL(rewind1()), m_monitorManager, SLOT(slotRewindOneFrame()));
    connect(m_jogProcess, SIGNAL(forward1()), m_monitorManager, SLOT(slotForwardOneFrame()));
    connect(m_jogProcess, SIGNAL(rewind(double)), m_monitorManager, SLOT(slotRewind(double)));
    connect(m_jogProcess, SIGNAL(forward(double)), m_monitorManager, SLOT(slotForward(double)));
    connect(m_jogProcess, SIGNAL(stop()), m_monitorManager, SLOT(slotPlay()));
    connect(m_jogProcess, SIGNAL(button(int)), this, SLOT(slotShuttleButton(int)));
}

void MainWindow::slotShuttleButton(int code)
{
    switch (code) {
    case 5:
        slotShuttleAction(KdenliveSettings::shuttle1());
        break;
    case 6:
        slotShuttleAction(KdenliveSettings::shuttle2());
        break;
    case 7:
        slotShuttleAction(KdenliveSettings::shuttle3());
        break;
    case 8:
        slotShuttleAction(KdenliveSettings::shuttle4());
        break;
    case 9:
        slotShuttleAction(KdenliveSettings::shuttle5());
        break;
    }
}

void MainWindow::slotShuttleAction(int code)
{
    switch (code) {
    case 0:
        return;
    case 1:
        m_monitorManager->slotPlay();
        break;
    default:
        m_monitorManager->slotPlay();
        break;
    }
}
#endif /* NO_JOGSHUTTLE */

void MainWindow::configureNotifications()
{
    KNotifyConfigWidget::configure(this);
}

void MainWindow::slotFullScreen()
{
    KToggleFullScreenAction::setFullScreen(this, actionCollection()->action("fullscreen")->isChecked());
}

void MainWindow::slotAddEffect(const QDomElement effect, GenTime pos, int track)
{
    if (!m_activeDocument) return;
    if (effect.isNull()) {
        kDebug() << "--- ERROR, TRYING TO APPEND NULL EFFECT";
        return;
    }
    QDomElement effectToAdd = effect.cloneNode().toElement();
    m_activeTimeline->projectView()->slotAddEffect(effectToAdd, pos, track);
}

void MainWindow::slotRaiseMonitor(bool clipMonitor)
{
    if (clipMonitor) m_clipMonitorDock->raise();
    else m_projectMonitorDock->raise();
}

void MainWindow::slotUpdateClip(const QString &id)
{
    if (!m_activeDocument) return;
    m_activeTimeline->projectView()->slotUpdateClip(id);
}

void MainWindow::slotConnectMonitors()
{

    m_projectList->setRenderer(m_projectMonitor->render);
    //connect(m_projectList, SIGNAL(receivedClipDuration(const QString &)), this, SLOT(slotUpdateClip(const QString &)));
    connect(m_projectList, SIGNAL(deleteProjectClips(QStringList, QMap<QString, QString>)), this, SLOT(slotDeleteProjectClips(QStringList, QMap<QString, QString>)));
    connect(m_projectList, SIGNAL(showClipProperties(DocClipBase *)), this, SLOT(slotShowClipProperties(DocClipBase *)));
    connect(m_projectList, SIGNAL(showClipProperties(QList <DocClipBase *>, QMap<QString, QString>)), this, SLOT(slotShowClipProperties(QList <DocClipBase *>, QMap<QString, QString>)));
    connect(m_projectList, SIGNAL(getFileProperties(const QDomElement, const QString &, int, bool)), m_projectMonitor->render, SLOT(getFileProperties(const QDomElement, const QString &, int, bool)));
    connect(m_projectMonitor->render, SIGNAL(replyGetImage(const QString &, const QPixmap &)), m_projectList, SLOT(slotReplyGetImage(const QString &, const QPixmap &)));
    connect(m_projectMonitor->render, SIGNAL(replyGetFileProperties(const QString &, Mlt::Producer*, const QMap < QString, QString > &, const QMap < QString, QString > &, bool)), m_projectList, SLOT(slotReplyGetFileProperties(const QString &, Mlt::Producer*, const QMap < QString, QString > &, const QMap < QString, QString > &, bool)));

    connect(m_projectMonitor->render, SIGNAL(removeInvalidClip(const QString &, bool)), m_projectList, SLOT(slotRemoveInvalidClip(const QString &, bool)));

    connect(m_clipMonitor, SIGNAL(refreshClipThumbnail(const QString &)), m_projectList, SLOT(slotRefreshClipThumbnail(const QString &)));

    connect(m_clipMonitor, SIGNAL(adjustMonitorSize()), this, SLOT(slotAdjustClipMonitor()));
    connect(m_projectMonitor, SIGNAL(adjustMonitorSize()), this, SLOT(slotAdjustProjectMonitor()));

    connect(m_clipMonitor, SIGNAL(saveZone(Render *, QPoint)), this, SLOT(slotSaveZone(Render *, QPoint)));
    connect(m_projectMonitor, SIGNAL(saveZone(Render *, QPoint)), this, SLOT(slotSaveZone(Render *, QPoint)));
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

void MainWindow::setupActions()
{

    KActionCollection* collection = actionCollection();
    m_timecodeFormat = new KComboBox(this);
    m_timecodeFormat->addItem(i18n("hh:mm:ss::ff"));
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
    statusBar()->setStyleSheet(QString("QStatusBar QLabel {font-size:%1pt;} QStatusBar::item { border: 0px; font-size:%1pt;padding:0px; }").arg(statusBar()->font().pointSize()));
    QString style1 = "QToolBar { border: 0px } QToolButton { border-style: inset; border:1px solid #999999;border-radius: 3px;margin: 0px 3px;padding: 0px;} QToolButton:checked { background-color: rgba(224, 224, 0, 100); border-style: inset; border:1px solid #cc6666;border-radius: 3px;}";

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

    toolbar->setStyleSheet(style1);
    connect(toolGroup, SIGNAL(triggered(QAction *)), this, SLOT(slotChangeTool(QAction *)));

    toolbar->addSeparator();
    m_buttonFitZoom = new KAction(KIcon("zoom-fit-best"), i18n("Fit zoom to project"), this);
    toolbar->addAction(m_buttonFitZoom);
    m_buttonFitZoom->setCheckable(false);
    connect(m_buttonFitZoom, SIGNAL(triggered()), this, SLOT(slotFitZoom()));

    actionWidget = toolbar->widgetForAction(m_buttonFitZoom);
    actionWidget->setMaximumWidth(max);
    actionWidget->setMaximumHeight(max - 4);

    m_zoomSlider = new QSlider(Qt::Horizontal, this);
    m_zoomSlider->setMaximum(13);
    m_zoomSlider->setPageStep(1);

    m_zoomSlider->setMaximumWidth(150);
    m_zoomSlider->setMinimumWidth(100);
    toolbar->addWidget(m_zoomSlider);

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
    statusBar()->insertPermanentFixedItem("00:00:00:00", ID_TIMELINE_POS);
    statusBar()->addPermanentWidget(m_timecodeFormat);
    //statusBar()->setMaximumHeight(statusBar()->font().pointSize() * 3);

    collection->addAction("normal_mode", m_normalEditTool);
    collection->addAction("overwrite_mode", m_overwriteEditTool);
    collection->addAction("insert_mode", m_insertEditTool);
    collection->addAction("select_tool", m_buttonSelectTool);
    collection->addAction("razor_tool", m_buttonRazorTool);
    collection->addAction("spacer_tool", m_buttonSpacerTool);

    collection->addAction("show_video_thumbs", m_buttonVideoThumbs);
    collection->addAction("show_audio_thumbs", m_buttonAudioThumbs);
    collection->addAction("show_markers", m_buttonShowMarkers);
    collection->addAction("snap", m_buttonSnap);
    collection->addAction("zoom_fit", m_buttonFitZoom);

    KAction* zoomIn = new KAction(KIcon("zoom-in"), i18n("Zoom In"), this);
    collection->addAction("zoom_in", zoomIn);
    connect(zoomIn, SIGNAL(triggered(bool)), this, SLOT(slotZoomIn()));
    zoomIn->setShortcut(Qt::CTRL + Qt::Key_Plus);

    KAction* zoomOut = new KAction(KIcon("zoom-out"), i18n("Zoom Out"), this);
    collection->addAction("zoom_out", zoomOut);
    connect(zoomOut, SIGNAL(triggered(bool)), this, SLOT(slotZoomOut()));
    zoomOut->setShortcut(Qt::CTRL + Qt::Key_Minus);

    m_projectSearch = new KAction(KIcon("edit-find"), i18n("Find"), this);
    collection->addAction("project_find", m_projectSearch);
    connect(m_projectSearch, SIGNAL(triggered(bool)), this, SLOT(slotFind()));
    m_projectSearch->setShortcut(Qt::Key_Slash);

    m_projectSearchNext = new KAction(KIcon("go-down-search"), i18n("Find Next"), this);
    collection->addAction("project_find_next", m_projectSearchNext);
    connect(m_projectSearchNext, SIGNAL(triggered(bool)), this, SLOT(slotFindNext()));
    m_projectSearchNext->setShortcut(Qt::Key_F3);
    m_projectSearchNext->setEnabled(false);

    KAction* profilesAction = new KAction(KIcon("document-new"), i18n("Manage Project Profiles"), this);
    collection->addAction("manage_profiles", profilesAction);
    connect(profilesAction, SIGNAL(triggered(bool)), this, SLOT(slotEditProfiles()));

    KNS3::standardAction(i18n("Download New Wipes..."), this, SLOT(slotGetNewLumaStuff()), actionCollection(), "get_new_lumas");

    KNS3::standardAction(i18n("Download New Render Profiles..."), this, SLOT(slotGetNewRenderStuff()), actionCollection(), "get_new_profiles");

    KNS3::standardAction(i18n("Download New Project Profiles..."), this, SLOT(slotGetNewMltProfileStuff()), actionCollection(), "get_new_mlt_profiles");

    KNS3::standardAction(i18n("Download New Title Templates..."), this, SLOT(slotGetNewTitleStuff()), actionCollection(), "get_new_titles");

    KAction* wizAction = new KAction(KIcon("configure"), i18n("Run Config Wizard"), this);
    collection->addAction("run_wizard", wizAction);
    connect(wizAction, SIGNAL(triggered(bool)), this, SLOT(slotRunWizard()));

    KAction* projectAction = new KAction(KIcon("configure"), i18n("Project Settings"), this);
    collection->addAction("project_settings", projectAction);
    connect(projectAction, SIGNAL(triggered(bool)), this, SLOT(slotEditProjectSettings()));

    KAction* projectRender = new KAction(KIcon("media-record"), i18n("Render"), this);
    collection->addAction("project_render", projectRender);
    projectRender->setShortcut(Qt::CTRL + Qt::Key_Return);
    connect(projectRender, SIGNAL(triggered(bool)), this, SLOT(slotRenderProject()));

    KAction* projectClean = new KAction(KIcon("edit-clear"), i18n("Clean Project"), this);
    collection->addAction("project_clean", projectClean);
    connect(projectClean, SIGNAL(triggered(bool)), this, SLOT(slotCleanProject()));

    KAction* monitorPlay = new KAction(KIcon("media-playback-start"), i18n("Play"), this);
    KShortcut playShortcut;
    playShortcut.setPrimary(Qt::Key_Space);
    playShortcut.setAlternate(Qt::Key_K);
    monitorPlay->setShortcut(playShortcut);
    collection->addAction("monitor_play", monitorPlay);
    connect(monitorPlay, SIGNAL(triggered(bool)), m_monitorManager, SLOT(slotPlay()));

    m_playZone = new KAction(KIcon("media-playback-start"), i18n("Play Zone"), this);
    m_playZone->setShortcut(Qt::CTRL + Qt::Key_Space);
    collection->addAction("monitor_play_zone", m_playZone);
    connect(m_playZone, SIGNAL(triggered(bool)), m_monitorManager, SLOT(slotPlayZone()));

    m_loopZone = new KAction(KIcon("media-playback-start"), i18n("Loop Zone"), this);
    m_loopZone->setShortcut(Qt::ALT + Qt::Key_Space);
    collection->addAction("monitor_loop_zone", m_loopZone);
    connect(m_loopZone, SIGNAL(triggered(bool)), m_monitorManager, SLOT(slotLoopZone()));

    KAction *dvdWizard =  new KAction(KIcon("media-optical"), i18n("DVD Wizard"), this);
    collection->addAction("dvd_wizard", dvdWizard);
    connect(dvdWizard, SIGNAL(triggered(bool)), this, SLOT(slotDvdWizard()));

    KAction *transcodeClip =  new KAction(KIcon("edit-copy"), i18n("Transcode Clips"), this);
    collection->addAction("transcode_clip", transcodeClip);
    connect(transcodeClip, SIGNAL(triggered(bool)), this, SLOT(slotTranscodeClip()));

    KAction *markIn = collection->addAction("mark_in");
    markIn->setText(i18n("Set Zone In"));
    markIn->setShortcut(Qt::Key_I);
    connect(markIn, SIGNAL(triggered(bool)), this, SLOT(slotSetInPoint()));

    KAction *markOut = collection->addAction("mark_out");
    markOut->setText(i18n("Set Zone Out"));
    markOut->setShortcut(Qt::Key_O);
    connect(markOut, SIGNAL(triggered(bool)), this, SLOT(slotSetOutPoint()));

    KAction *switchMon = collection->addAction("switch_monitor");
    switchMon->setText(i18n("Switch monitor"));
    switchMon->setShortcut(Qt::Key_T);
    connect(switchMon, SIGNAL(triggered(bool)), this, SLOT(slotSwitchMonitors()));

    KAction *insertTree = collection->addAction("insert_project_tree");
    insertTree->setText(i18n("Insert zone in project tree"));
    insertTree->setShortcut(Qt::CTRL + Qt::Key_I);
    connect(insertTree, SIGNAL(triggered(bool)), this, SLOT(slotInsertZoneToTree()));

    KAction *insertTimeline = collection->addAction("insert_timeline");
    insertTimeline->setText(i18n("Insert zone in timeline"));
    insertTimeline->setShortcut(Qt::SHIFT + Qt::CTRL + Qt::Key_I);
    connect(insertTimeline, SIGNAL(triggered(bool)), this, SLOT(slotInsertZoneToTimeline()));

    KAction *resizeStart =  new KAction(KIcon(), i18n("Resize Item Start"), this);
    collection->addAction("resize_timeline_clip_start", resizeStart);
    resizeStart->setShortcut(Qt::Key_1);
    connect(resizeStart, SIGNAL(triggered(bool)), this, SLOT(slotResizeItemStart()));

    KAction *resizeEnd =  new KAction(KIcon(), i18n("Resize Item End"), this);
    collection->addAction("resize_timeline_clip_end", resizeEnd);
    resizeEnd->setShortcut(Qt::Key_2);
    connect(resizeEnd, SIGNAL(triggered(bool)), this, SLOT(slotResizeItemEnd()));

    KAction* monitorSeekBackward = new KAction(KIcon("media-seek-backward"), i18n("Rewind"), this);
    monitorSeekBackward->setShortcut(Qt::Key_J);
    collection->addAction("monitor_seek_backward", monitorSeekBackward);
    connect(monitorSeekBackward, SIGNAL(triggered(bool)), m_monitorManager, SLOT(slotRewind()));

    KAction* monitorSeekBackwardOneFrame = new KAction(KIcon("media-skip-backward"), i18n("Rewind 1 Frame"), this);
    monitorSeekBackwardOneFrame->setShortcut(Qt::Key_Left);
    collection->addAction("monitor_seek_backward-one-frame", monitorSeekBackwardOneFrame);
    connect(monitorSeekBackwardOneFrame, SIGNAL(triggered(bool)), m_monitorManager, SLOT(slotRewindOneFrame()));

    KAction* monitorSeekBackwardOneSecond = new KAction(KIcon("media-skip-backward"), i18n("Rewind 1 Second"), this);
    monitorSeekBackwardOneSecond->setShortcut(Qt::SHIFT + Qt::Key_Left);
    collection->addAction("monitor_seek_backward-one-second", monitorSeekBackwardOneSecond);
    connect(monitorSeekBackwardOneSecond, SIGNAL(triggered(bool)), m_monitorManager, SLOT(slotRewindOneSecond()));

    KAction* monitorSeekSnapBackward = new KAction(KIcon("media-seek-backward"), i18n("Go to Previous Snap Point"), this);
    monitorSeekSnapBackward->setShortcut(Qt::ALT + Qt::Key_Left);
    collection->addAction("monitor_seek_snap_backward", monitorSeekSnapBackward);
    connect(monitorSeekSnapBackward, SIGNAL(triggered(bool)), this, SLOT(slotSnapRewind()));

    KAction* monitorSeekForward = new KAction(KIcon("media-seek-forward"), i18n("Forward"), this);
    monitorSeekForward->setShortcut(Qt::Key_L);
    collection->addAction("monitor_seek_forward", monitorSeekForward);
    connect(monitorSeekForward, SIGNAL(triggered(bool)), m_monitorManager, SLOT(slotForward()));

    KAction* clipStart = new KAction(KIcon("media-seek-backward"), i18n("Go to Clip Start"), this);
    clipStart->setShortcut(Qt::Key_Home);
    collection->addAction("seek_clip_start", clipStart);
    connect(clipStart, SIGNAL(triggered(bool)), this, SLOT(slotClipStart()));

    KAction* clipEnd = new KAction(KIcon("media-seek-forward"), i18n("Go to Clip End"), this);
    clipEnd->setShortcut(Qt::Key_End);
    collection->addAction("seek_clip_end", clipEnd);
    connect(clipEnd, SIGNAL(triggered(bool)), this, SLOT(slotClipEnd()));

    KAction* zoneStart = new KAction(KIcon("media-seek-backward"), i18n("Go to Zone Start"), this);
    zoneStart->setShortcut(Qt::SHIFT + Qt::Key_I);
    collection->addAction("seek_zone_start", zoneStart);
    connect(zoneStart, SIGNAL(triggered(bool)), this, SLOT(slotZoneStart()));

    KAction* zoneEnd = new KAction(KIcon("media-seek-forward"), i18n("Go to Zone End"), this);
    zoneEnd->setShortcut(Qt::SHIFT + Qt::Key_O);
    collection->addAction("seek_zone_end", zoneEnd);
    connect(zoneEnd, SIGNAL(triggered(bool)), this, SLOT(slotZoneEnd()));

    KAction* projectStart = new KAction(KIcon("go-first"), i18n("Go to Project Start"), this);
    projectStart->setShortcut(Qt::CTRL + Qt::Key_Home);
    collection->addAction("seek_start", projectStart);
    connect(projectStart, SIGNAL(triggered(bool)), m_monitorManager, SLOT(slotStart()));

    KAction* projectEnd = new KAction(KIcon("go-last"), i18n("Go to Project End"), this);
    projectEnd->setShortcut(Qt::CTRL + Qt::Key_End);
    collection->addAction("seek_end", projectEnd);
    connect(projectEnd, SIGNAL(triggered(bool)), m_monitorManager, SLOT(slotEnd()));

    KAction* monitorSeekForwardOneFrame = new KAction(KIcon("media-skip-forward"), i18n("Forward 1 Frame"), this);
    monitorSeekForwardOneFrame->setShortcut(Qt::Key_Right);
    collection->addAction("monitor_seek_forward-one-frame", monitorSeekForwardOneFrame);
    connect(monitorSeekForwardOneFrame, SIGNAL(triggered(bool)), m_monitorManager, SLOT(slotForwardOneFrame()));

    KAction* monitorSeekForwardOneSecond = new KAction(KIcon("media-skip-forward"), i18n("Forward 1 Second"), this);
    monitorSeekForwardOneSecond->setShortcut(Qt::SHIFT + Qt::Key_Right);
    collection->addAction("monitor_seek_forward-one-second", monitorSeekForwardOneSecond);
    connect(monitorSeekForwardOneSecond, SIGNAL(triggered(bool)), m_monitorManager, SLOT(slotForwardOneSecond()));

    KAction* monitorSeekSnapForward = new KAction(KIcon("media-seek-forward"), i18n("Go to Next Snap Point"), this);
    monitorSeekSnapForward->setShortcut(Qt::ALT + Qt::Key_Right);
    collection->addAction("monitor_seek_snap_forward", monitorSeekSnapForward);
    connect(monitorSeekSnapForward, SIGNAL(triggered(bool)), this, SLOT(slotSnapForward()));

    KAction* deleteTimelineClip = new KAction(KIcon("edit-delete"), i18n("Delete Selected Item"), this);
    deleteTimelineClip->setShortcut(Qt::Key_Delete);
    collection->addAction("delete_timeline_clip", deleteTimelineClip);
    connect(deleteTimelineClip, SIGNAL(triggered(bool)), this, SLOT(slotDeleteTimelineClip()));

    /*KAction* editTimelineClipSpeed = new KAction(i18n("Change Clip Speed"), this);
    collection->addAction("change_clip_speed", editTimelineClipSpeed);
    editTimelineClipSpeed->setData("change_speed");
    connect(editTimelineClipSpeed, SIGNAL(triggered(bool)), this, SLOT(slotChangeClipSpeed()));*/

    KAction *stickTransition = collection->addAction("auto_transition");
    stickTransition->setData(QString("auto"));
    stickTransition->setCheckable(true);
    stickTransition->setEnabled(false);
    stickTransition->setText(i18n("Automatic Transition"));
    connect(stickTransition, SIGNAL(triggered(bool)), this, SLOT(slotAutoTransition()));

    KAction* groupClip = new KAction(KIcon("object-group"), i18n("Group Clips"), this);
    groupClip->setShortcut(Qt::CTRL + Qt::Key_G);
    collection->addAction("group_clip", groupClip);
    connect(groupClip, SIGNAL(triggered(bool)), this, SLOT(slotGroupClips()));

    KAction* ungroupClip = new KAction(KIcon("object-ungroup"), i18n("Ungroup Clips"), this);
    collection->addAction("ungroup_clip", ungroupClip);
    ungroupClip->setShortcut(Qt::CTRL + Qt::SHIFT + Qt::Key_G);
    ungroupClip->setData("ungroup_clip");
    connect(ungroupClip, SIGNAL(triggered(bool)), this, SLOT(slotUnGroupClips()));

    KAction* editItemDuration = new KAction(KIcon("measure"), i18n("Edit Duration"), this);
    collection->addAction("edit_item_duration", editItemDuration);
    connect(editItemDuration, SIGNAL(triggered(bool)), this, SLOT(slotEditItemDuration()));

    KAction* insertOvertwrite = new KAction(KIcon(), i18n("Insert Clip Zone in Timeline (Overwrite)"), this);
    insertOvertwrite->setShortcut(Qt::Key_V);
    collection->addAction("overwrite_to_in_point", insertOvertwrite);
    connect(insertOvertwrite, SIGNAL(triggered(bool)), this, SLOT(slotInsertClipOverwrite()));

    KAction* selectTimelineClip = new KAction(KIcon("edit-select"), i18n("Select Clip"), this);
    selectTimelineClip->setShortcut(Qt::Key_Plus);
    collection->addAction("select_timeline_clip", selectTimelineClip);
    connect(selectTimelineClip, SIGNAL(triggered(bool)), this, SLOT(slotSelectTimelineClip()));

    KAction* deselectTimelineClip = new KAction(KIcon("edit-select"), i18n("Deselect Clip"), this);
    deselectTimelineClip->setShortcut(Qt::Key_Minus);
    collection->addAction("deselect_timeline_clip", deselectTimelineClip);
    connect(deselectTimelineClip, SIGNAL(triggered(bool)), this, SLOT(slotDeselectTimelineClip()));

    KAction* selectAddTimelineClip = new KAction(KIcon("edit-select"), i18n("Add Clip To Selection"), this);
    selectAddTimelineClip->setShortcut(Qt::ALT + Qt::Key_Plus);
    collection->addAction("select_add_timeline_clip", selectAddTimelineClip);
    connect(selectAddTimelineClip, SIGNAL(triggered(bool)), this, SLOT(slotSelectAddTimelineClip()));

    KAction* selectTimelineTransition = new KAction(KIcon("edit-select"), i18n("Select Transition"), this);
    selectTimelineTransition->setShortcut(Qt::SHIFT + Qt::Key_Plus);
    collection->addAction("select_timeline_transition", selectTimelineTransition);
    connect(selectTimelineTransition, SIGNAL(triggered(bool)), this, SLOT(slotSelectTimelineTransition()));

    KAction* deselectTimelineTransition = new KAction(KIcon("edit-select"), i18n("Deselect Transition"), this);
    deselectTimelineTransition->setShortcut(Qt::SHIFT + Qt::Key_Minus);
    collection->addAction("deselect_timeline_transition", deselectTimelineTransition);
    connect(deselectTimelineTransition, SIGNAL(triggered(bool)), this, SLOT(slotDeselectTimelineTransition()));

    KAction* selectAddTimelineTransition = new KAction(KIcon("edit-select"), i18n("Add Transition To Selection"), this);
    selectAddTimelineTransition->setShortcut(Qt::ALT + Qt::SHIFT + Qt::Key_Plus);
    collection->addAction("select_add_timeline_transition", selectAddTimelineTransition);
    connect(selectAddTimelineTransition, SIGNAL(triggered(bool)), this, SLOT(slotSelectAddTimelineTransition()));

    KAction* cutTimelineClip = new KAction(KIcon("edit-cut"), i18n("Cut Clip"), this);
    cutTimelineClip->setShortcut(Qt::SHIFT + Qt::Key_R);
    collection->addAction("cut_timeline_clip", cutTimelineClip);
    connect(cutTimelineClip, SIGNAL(triggered(bool)), this, SLOT(slotCutTimelineClip()));

    KAction* addClipMarker = new KAction(KIcon("bookmark-new"), i18n("Add Marker"), this);
    collection->addAction("add_clip_marker", addClipMarker);
    connect(addClipMarker, SIGNAL(triggered(bool)), this, SLOT(slotAddClipMarker()));

    KAction* deleteClipMarker = new KAction(KIcon("edit-delete"), i18n("Delete Marker"), this);
    collection->addAction("delete_clip_marker", deleteClipMarker);
    connect(deleteClipMarker, SIGNAL(triggered(bool)), this, SLOT(slotDeleteClipMarker()));

    KAction* deleteAllClipMarkers = new KAction(KIcon("edit-delete"), i18n("Delete All Markers"), this);
    collection->addAction("delete_all_clip_markers", deleteAllClipMarkers);
    connect(deleteAllClipMarkers, SIGNAL(triggered(bool)), this, SLOT(slotDeleteAllClipMarkers()));

    KAction* editClipMarker = new KAction(KIcon("document-properties"), i18n("Edit Marker"), this);
    collection->addAction("edit_clip_marker", editClipMarker);
    connect(editClipMarker, SIGNAL(triggered(bool)), this, SLOT(slotEditClipMarker()));

    KAction* splitAudio = new KAction(KIcon("document-new"), i18n("Split Audio"), this);
    collection->addAction("split_audio", splitAudio);
    connect(splitAudio, SIGNAL(triggered(bool)), this, SLOT(slotSplitAudio()));

    KAction* audioOnly = new KAction(KIcon("document-new"), i18n("Audio Only"), this);
    collection->addAction("clip_audio_only", audioOnly);
    audioOnly->setData("clip_audio_only");
    audioOnly->setCheckable(true);

    KAction* videoOnly = new KAction(KIcon("document-new"), i18n("Video Only"), this);
    collection->addAction("clip_video_only", videoOnly);
    videoOnly->setData("clip_video_only");
    videoOnly->setCheckable(true);

    KAction* audioAndVideo = new KAction(KIcon("document-new"), i18n("Audio and Video"), this);
    collection->addAction("clip_audio_and_video", audioAndVideo);
    audioAndVideo->setData("clip_audio_and_video");
    audioAndVideo->setCheckable(true);

    m_clipTypeGroup = new QActionGroup(this);
    m_clipTypeGroup->addAction(audioOnly);
    m_clipTypeGroup->addAction(videoOnly);
    m_clipTypeGroup->addAction(audioAndVideo);
    connect(m_clipTypeGroup, SIGNAL(triggered(QAction *)), this, SLOT(slotUpdateClipType(QAction *)));
    m_clipTypeGroup->setEnabled(false);

    KAction *insertSpace = new KAction(KIcon(), i18n("Insert Space"), this);
    collection->addAction("insert_space", insertSpace);
    connect(insertSpace, SIGNAL(triggered()), this, SLOT(slotInsertSpace()));

    KAction *removeSpace = new KAction(KIcon(), i18n("Remove Space"), this);
    collection->addAction("delete_space", removeSpace);
    connect(removeSpace, SIGNAL(triggered()), this, SLOT(slotRemoveSpace()));

    KAction *insertTrack = new KAction(KIcon(), i18n("Insert Track"), this);
    collection->addAction("insert_track", insertTrack);
    connect(insertTrack, SIGNAL(triggered()), this, SLOT(slotInsertTrack()));

    KAction *deleteTrack = new KAction(KIcon(), i18n("Delete Track"), this);
    collection->addAction("delete_track", deleteTrack);
    connect(deleteTrack, SIGNAL(triggered()), this, SLOT(slotDeleteTrack()));

    KAction *changeTrack = new KAction(KIcon(), i18n("Change Track"), this);
    collection->addAction("change_track", changeTrack);
    connect(changeTrack, SIGNAL(triggered()), this, SLOT(slotChangeTrack()));

    KAction *addGuide = new KAction(KIcon("document-new"), i18n("Add Guide"), this);
    collection->addAction("add_guide", addGuide);
    connect(addGuide, SIGNAL(triggered()), this, SLOT(slotAddGuide()));

    QAction *delGuide = new KAction(KIcon("edit-delete"), i18n("Delete Guide"), this);
    collection->addAction("delete_guide", delGuide);
    connect(delGuide, SIGNAL(triggered()), this, SLOT(slotDeleteGuide()));

    QAction *editGuide = new KAction(KIcon("document-properties"), i18n("Edit Guide"), this);
    collection->addAction("edit_guide", editGuide);
    connect(editGuide, SIGNAL(triggered()), this, SLOT(slotEditGuide()));

    QAction *delAllGuides = new KAction(KIcon("edit-delete"), i18n("Delete All Guides"), this);
    collection->addAction("delete_all_guides", delAllGuides);
    connect(delAllGuides, SIGNAL(triggered()), this, SLOT(slotDeleteAllGuides()));

    QAction *pasteEffects = new KAction(KIcon("edit-paste"), i18n("Paste Effects"), this);
    collection->addAction("paste_effects", pasteEffects);
    pasteEffects->setData("paste_effects");
    connect(pasteEffects , SIGNAL(triggered()), this, SLOT(slotPasteEffects()));

    QAction *showTimeline = new KAction(i18n("Show Timeline"), this);
    collection->addAction("show_timeline", showTimeline);
    showTimeline->setCheckable(true);
    showTimeline->setChecked(true);
    connect(showTimeline, SIGNAL(triggered(bool)), this, SLOT(slotShowTimeline(bool)));

    QAction *showTitleBar = new KAction(i18n("Show Title Bars"), this);
    collection->addAction("show_titlebars", showTitleBar);
    showTitleBar->setCheckable(true);
    connect(showTitleBar, SIGNAL(triggered(bool)), this, SLOT(slotShowTitleBars(bool)));
    showTitleBar->setChecked(KdenliveSettings::showtitlebars());
    slotShowTitleBars(KdenliveSettings::showtitlebars());

    /*QAction *maxCurrent = new KAction(i18n("Maximize Current Widget"), this);
    collection->addAction("maximize_current", maxCurrent);
    maxCurrent->setCheckable(true);
    maxCurrent->setChecked(false);
    connect(maxCurrent, SIGNAL(triggered(bool)), this, SLOT(slotMaximizeCurrent(bool)));*/


    m_closeAction = KStandardAction::close(this, SLOT(closeCurrentDocument()), collection);

    KStandardAction::quit(this, SLOT(queryQuit()), collection);

    KStandardAction::open(this, SLOT(openFile()), collection);

    m_saveAction = KStandardAction::save(this, SLOT(saveFile()), collection);

    KStandardAction::saveAs(this, SLOT(saveFileAs()), collection);

    KStandardAction::openNew(this, SLOT(newFile()), collection);

    KStandardAction::preferences(this, SLOT(slotPreferences()), collection);

    KStandardAction::configureNotifications(this , SLOT(configureNotifications()), collection);

    KStandardAction::copy(this, SLOT(slotCopy()), collection);

    KStandardAction::paste(this, SLOT(slotPaste()), collection);

    KAction *undo = KStandardAction::undo(m_commandStack, SLOT(undo()), collection);
    undo->setEnabled(false);
    connect(m_commandStack, SIGNAL(canUndoChanged(bool)), undo, SLOT(setEnabled(bool)));

    KAction *redo = KStandardAction::redo(m_commandStack, SLOT(redo()), collection);
    redo->setEnabled(false);
    connect(m_commandStack, SIGNAL(canRedoChanged(bool)), redo, SLOT(setEnabled(bool)));

    KStandardAction::fullScreen(this, SLOT(slotFullScreen()), this, collection);

    /*
    //TODO: Add status tooltip to actions ?
    connect(collection, SIGNAL(actionHovered(QAction*)),
            this, SLOT(slotDisplayActionMessage(QAction*)));*/


    QAction *addClip = new KAction(KIcon("kdenlive-add-clip"), i18n("Add Clip"), this);
    collection->addAction("add_clip", addClip);
    connect(addClip , SIGNAL(triggered()), m_projectList, SLOT(slotAddClip()));

    QAction *addColorClip = new KAction(KIcon("kdenlive-add-color-clip"), i18n("Add Color Clip"), this);
    collection->addAction("add_color_clip", addColorClip);
    connect(addColorClip , SIGNAL(triggered()), m_projectList, SLOT(slotAddColorClip()));

    QAction *addSlideClip = new KAction(KIcon("kdenlive-add-slide-clip"), i18n("Add Slideshow Clip"), this);
    collection->addAction("add_slide_clip", addSlideClip);
    connect(addSlideClip , SIGNAL(triggered()), m_projectList, SLOT(slotAddSlideshowClip()));

    QAction *addTitleClip = new KAction(KIcon("kdenlive-add-text-clip"), i18n("Add Title Clip"), this);
    collection->addAction("add_text_clip", addTitleClip);
    connect(addTitleClip , SIGNAL(triggered()), m_projectList, SLOT(slotAddTitleClip()));

    QAction *addTitleTemplateClip = new KAction(KIcon("kdenlive-add-text-clip"), i18n("Add Template Title"), this);
    collection->addAction("add_text_template_clip", addTitleTemplateClip);
    connect(addTitleTemplateClip , SIGNAL(triggered()), m_projectList, SLOT(slotAddTitleTemplateClip()));

    QAction *addFolderButton = new KAction(KIcon("folder-new"), i18n("Create Folder"), this);
    collection->addAction("add_folder", addFolderButton);
    connect(addFolderButton , SIGNAL(triggered()), m_projectList, SLOT(slotAddFolder()));

    QAction *clipProperties = new KAction(KIcon("document-edit"), i18n("Clip Properties"), this);
    collection->addAction("clip_properties", clipProperties);
    clipProperties->setData("clip_properties");
    connect(clipProperties , SIGNAL(triggered()), m_projectList, SLOT(slotEditClip()));
    clipProperties->setEnabled(false);

    QAction *openClip = new KAction(KIcon("document-open"), i18n("Edit Clip"), this);
    collection->addAction("edit_clip", openClip);
    openClip->setData("edit_clip");
    connect(openClip , SIGNAL(triggered()), m_projectList, SLOT(slotOpenClip()));
    openClip->setEnabled(false);

    QAction *deleteClip = new KAction(KIcon("edit-delete"), i18n("Delete Clip"), this);
    collection->addAction("delete_clip", deleteClip);
    deleteClip->setData("delete_clip");
    connect(deleteClip , SIGNAL(triggered()), m_projectList, SLOT(slotRemoveClip()));
    deleteClip->setEnabled(false);

    QAction *reloadClip = new KAction(KIcon("view-refresh"), i18n("Reload Clip"), this);
    collection->addAction("reload_clip", reloadClip);
    reloadClip->setData("reload_clip");
    connect(reloadClip , SIGNAL(triggered()), m_projectList, SLOT(slotReloadClip()));
    reloadClip->setEnabled(false);

    QMenu *addClips = new QMenu();
    addClips->addAction(addClip);
    addClips->addAction(addColorClip);
    addClips->addAction(addSlideClip);
    addClips->addAction(addTitleClip);
    addClips->addAction(addTitleTemplateClip);
    addClips->addAction(addFolderButton);

    addClips->addAction(reloadClip);
    addClips->addAction(clipProperties);
    addClips->addAction(openClip);
    addClips->addAction(deleteClip);
    m_projectList->setupMenu(addClips, addClip);

    //connect(collection, SIGNAL( clearStatusText() ),
    //statusBar(), SLOT( clear() ) );
}

void MainWindow::slotDisplayActionMessage(QAction *a)
{
    statusBar()->showMessage(a->data().toString(), 3000);
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

    if (!initialGroup.exists() || upgrade) {
        // this is our first run, show Wizard
        Wizard *w = new Wizard(upgrade, this);
        if (w->exec() == QDialog::Accepted && w->isOk()) {
            w->adjustSettings();
            initialGroup.writeEntry("version", version);
            delete w;
        } else {
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
    Wizard *w = new Wizard(false, this);
    if (w->exec() == QDialog::Accepted && w->isOk()) {
        w->adjustSettings();
    }
    delete w;
}

void MainWindow::newFile(bool showProjectSettings, bool force)
{
    if (!m_timelineArea->isEnabled() && !force) return;
    m_fileRevert->setEnabled(false);
    QString profileName;
    KUrl projectFolder;
    QPoint projectTracks(KdenliveSettings::videotracks(), KdenliveSettings::audiotracks());
    if (!showProjectSettings) {
        if (!KdenliveSettings::activatetabs()) closeCurrentDocument();
        profileName = KdenliveSettings::default_profile();
        projectFolder = KdenliveSettings::defaultprojectfolder();
    } else {
        ProjectSettings *w = new ProjectSettings(NULL, QStringList(), projectTracks.x(), projectTracks.y(), KdenliveSettings::defaultprojectfolder(), false, true, this);
        if (w->exec() != QDialog::Accepted) return;
        if (!KdenliveSettings::activatetabs()) closeCurrentDocument();
        if (KdenliveSettings::videothumbnails() != w->enableVideoThumbs()) slotSwitchVideoThumbs();
        if (KdenliveSettings::audiothumbnails() != w->enableAudioThumbs()) slotSwitchAudioThumbs();
        profileName = w->selectedProfile();
        projectFolder = w->selectedFolder();
        projectTracks = w->tracks();
        delete w;
    }
    m_timelineArea->setEnabled(true);
    m_projectList->setEnabled(true);
    KdenliveDoc *doc = new KdenliveDoc(KUrl(), projectFolder, m_commandStack, profileName, projectTracks, m_projectMonitor->render, this);
    doc->m_autosave = new KAutoSaveFile(KUrl(), doc);
    bool ok;
    TrackView *trackView = new TrackView(doc, &ok, this);
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
    } else m_timelineArea->setTabBarHidden(false);
    m_monitorManager->activateMonitor("clip");
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

void MainWindow::closeCurrentDocument(bool saveChanges)
{
    QWidget *w = m_timelineArea->currentWidget();
    if (!w) return;
    // closing current document
    int ix = m_timelineArea->currentIndex() + 1;
    if (ix == m_timelineArea->count()) ix = 0;
    m_timelineArea->setCurrentIndex(ix);
    TrackView *tabToClose = (TrackView *) w;
    KdenliveDoc *docToClose = tabToClose->document();
    if (docToClose && docToClose->isModified() && saveChanges) {
        switch (KMessageBox::warningYesNoCancel(this, i18n("Save changes to document?"))) {
        case KMessageBox::Yes :
            // save document here. If saving fails, return false;
            if (saveFile() == false) return;
            break;
        case KMessageBox::Cancel :
            return;
            break;
        default:
            break;
        }
    }
    m_clipMonitor->slotSetXml(NULL);
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
    } else delete docToClose;
    if (w == m_activeTimeline) {
        delete m_activeTimeline;
        m_activeTimeline = NULL;
    } else delete w;
}

bool MainWindow::saveFileAs(const QString &outputFileName)
{
    QString currentSceneList;
    m_monitorManager->stopActiveMonitor();
    if (KdenliveSettings::dropbframes()) {
        KdenliveSettings::setDropbframes(false);
        m_activeDocument->clipManager()->updatePreviewSettings();
        currentSceneList = m_projectMonitor->sceneList();
        KdenliveSettings::setDropbframes(true);
        m_activeDocument->clipManager()->updatePreviewSettings();
    } else currentSceneList = m_projectMonitor->sceneList();

    if (m_activeDocument->saveSceneList(outputFileName, currentSceneList) == false)
        return false;

    // Save timeline thumbnails
    m_activeTimeline->projectView()->saveThumbnails();
    m_activeDocument->setUrl(KUrl(outputFileName));
    if (m_activeDocument->m_autosave == NULL) {
        m_activeDocument->m_autosave = new KAutoSaveFile(KUrl(outputFileName), this);
    } else m_activeDocument->m_autosave->setManagedFile(KUrl(outputFileName));
    setCaption(m_activeDocument->description());
    m_timelineArea->setTabText(m_timelineArea->currentIndex(), m_activeDocument->description());
    m_timelineArea->setTabToolTip(m_timelineArea->currentIndex(), m_activeDocument->url().path());
    m_activeDocument->setModified(false);
    m_fileOpenRecent->addUrl(KUrl(outputFileName));
    m_fileRevert->setEnabled(true);
    return true;
}

bool MainWindow::saveFileAs()
{
    // Check that the Kdenlive mime type is correctly installed
    QString mimetype = "application/x-kdenlive";
    KMimeType::Ptr mime = KMimeType::mimeType(mimetype);
    if (!mime) mimetype = "*.kdenlive";

    QString outputFile = KFileDialog::getSaveFileName(KUrl(), mimetype);
    if (outputFile.isEmpty()) return false;
    if (QFile::exists(outputFile)) {
        if (KMessageBox::questionYesNo(this, i18n("File already exists.\nDo you want to overwrite it?")) == KMessageBox::No) return false;
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
    // Check that the Kdenlive mime type is correctly installed
    QString mimetype = "application/x-kdenlive";
    KMimeType::Ptr mime = KMimeType::mimeType(mimetype);
    if (!mime) mimetype = "*.kdenlive";

    KUrl url = KFileDialog::getOpenUrl(KUrl("kfiledialog:///projectfolder"), mimetype);
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

    // Check for backup file
    QList<KAutoSaveFile *> staleFiles = KAutoSaveFile::staleFiles(url);
    if (!staleFiles.isEmpty()) {
        if (KMessageBox::questionYesNo(this,
                                       i18n("Auto-saved files exist. Do you want to recover them now?"),
                                       i18n("File Recovery"),
                                       KGuiItem(i18n("Recover")), KGuiItem(i18n("Don't recover"))) == KMessageBox::Yes) {
            recoverFiles(staleFiles);
            return;
        } else {
            // remove the stale files
            foreach(KAutoSaveFile *stale, staleFiles) {
                stale->open(QIODevice::ReadWrite);
                delete stale;
            }
        }
    }
    if (!KdenliveSettings::activatetabs()) closeCurrentDocument();
    m_messageLabel->setMessage(i18n("Opening file %1", url.path()), InformationMessage);
    m_messageLabel->repaint();
    doOpenFile(url, NULL);
}

void MainWindow::doOpenFile(const KUrl &url, KAutoSaveFile *stale)
{
    if (!m_timelineArea->isEnabled()) return;
    m_fileRevert->setEnabled(true);
    KdenliveDoc *doc = new KdenliveDoc(url, KdenliveSettings::defaultprojectfolder(), m_commandStack, KdenliveSettings::default_profile(), QPoint(KdenliveSettings::videotracks(), KdenliveSettings::audiotracks()), m_projectMonitor->render, this);
    if (stale == NULL) {
        stale = new KAutoSaveFile(url, doc);
        doc->m_autosave = stale;
    } else {
        doc->m_autosave = stale;
        doc->setUrl(stale->managedFile());
        doc->setModified(true);
        stale->setParent(doc);
    }
    connectDocumentInfo(doc);
    bool ok;
    TrackView *trackView = new TrackView(doc, &ok, this);
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
    m_clipMonitor->refreshMonitor(true);
}

void MainWindow::recoverFiles(QList<KAutoSaveFile *> staleFiles)
{
    if (!KdenliveSettings::activatetabs()) closeCurrentDocument();
    foreach(KAutoSaveFile *stale, staleFiles) {
        /*if (!stale->open(QIODevice::QIODevice::ReadOnly)) {
                  // show an error message; we could not steal the lockfile
                  // maybe another application got to the file before us?
                  delete stale;
                  continue;
        }*/
        kDebug() << "// OPENING RECOVERY: " << stale->fileName() << "\nMANAGED: " << stale->managedFile().path();
        // the stalefiles also contain ".lock" files so we must ignore them... bug in KAutoSaveFile?
        if (!stale->fileName().endsWith(".lock")) doOpenFile(KUrl(stale->fileName()), stale);
        else KIO::NetAccess::del(KUrl(stale->fileName()), this);
    }
}


void MainWindow::parseProfiles(const QString &mltPath)
{
    // kDebug()<<" + + YOUR MLT INSTALL WAS FOUND IN: "<< MLT_PREFIX <<endl;

    //KdenliveSettings::setDefaulttmpfolder();
    if (!mltPath.isEmpty()) {
        KdenliveSettings::setMltpath(mltPath + "/share/mlt/profiles/");
        KdenliveSettings::setRendererpath(mltPath + "/bin/melt");
    }

    if (KdenliveSettings::mltpath().isEmpty()) {
        KdenliveSettings::setMltpath(QString(MLT_PREFIX) + QString("/share/mlt/profiles/"));
    }
    if (KdenliveSettings::rendererpath().isEmpty() || KdenliveSettings::rendererpath().endsWith("inigo")) {
        QString meltPath = QString(MLT_PREFIX) + QString("/bin/melt");
        if (!QFile::exists(meltPath))
            meltPath = KStandardDirs::findExe("melt");
        KdenliveSettings::setRendererpath(meltPath);
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
            QStringList profilesList = QDir(KdenliveSettings::mltpath()).entryList(profilesFilter, QDir::Files);
        }

        if (profilesList.isEmpty()) {
            // Cannot find the MLT profiles, ask for location
            KUrlRequesterDialog *getUrl = new KUrlRequesterDialog(KdenliveSettings::mltpath(), i18n("Cannot find your Mlt profiles, please give the path"), this);
            getUrl->fileDialog()->setMode(KFile::Directory);
            if (getUrl->exec() == QDialog::Rejected) {
                ::exit(0);
            }
            KUrl mltPath = getUrl->selectedUrl();
            delete getUrl;
            if (mltPath.isEmpty()) ::exit(0);
            KdenliveSettings::setMltpath(mltPath.path(KUrl::AddTrailingSlash));
            QStringList profilesList = QDir(KdenliveSettings::mltpath()).entryList(profilesFilter, QDir::Files);
        }
    }

    if (KdenliveSettings::rendererpath().isEmpty()) {
        // Cannot find the MLT melt renderer, ask for location
        KUrlRequesterDialog *getUrl = new KUrlRequesterDialog(QString(), i18n("Cannot find the melt program required for rendering (part of Mlt)"), this);
        if (getUrl->exec() == QDialog::Rejected) {
            ::exit(0);
        }
        KUrl rendererPath = getUrl->selectedUrl();
        delete getUrl;
        if (rendererPath.isEmpty()) ::exit(0);
        KdenliveSettings::setRendererpath(rendererPath.path());
    }

    kDebug() << "RESULTING MLT PATH: " << KdenliveSettings::mltpath();

    // Parse MLT profiles to build a list of available video formats
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
    ProjectSettings *w = new ProjectSettings(m_projectList, m_activeTimeline->projectView()->extractTransitionsLumas(), p.x(), p.y(), m_activeDocument->projectFolder().path(), true, !m_activeDocument->isModified(), this);

    if (w->exec() == QDialog::Accepted) {
        QString profile = w->selectedProfile();
        m_activeDocument->setProjectFolder(w->selectedFolder());
        if (m_renderWidget) m_renderWidget->setDocumentPath(w->selectedFolder().path(KUrl::AddTrailingSlash));
        if (KdenliveSettings::videothumbnails() != w->enableVideoThumbs()) slotSwitchVideoThumbs();
        if (KdenliveSettings::audiothumbnails() != w->enableAudioThumbs()) slotSwitchAudioThumbs();
        if (m_activeDocument->profilePath() != profile) {
            // Profile was changed
            double dar = m_activeDocument->dar();

            // Deselect current effect / transition
            m_effectStack->slotClipItemSelected(NULL, 0);
            m_transitionConfig->slotTransitionItemSelected(NULL, 0, QPoint(), false);
            m_clipMonitor->slotSetXml(NULL);
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
            //m_activeDocument->clipManager()->resetProducersList(m_projectMonitor->render->producersList());
            if (dar != m_activeDocument->dar()) m_projectList->reloadClipThumbnails();
            if (updateFps) m_activeTimeline->updateProjectFps();
            m_activeDocument->setModified(true);
            m_commandStack->activeStack()->clear();
            //Update the mouse position display so it will display in DF/NDF format by default based on the project setting.
            slotUpdateMousePosition(0);
            // We need to desactivate & reactivate monitors to get a refresh
            //m_monitorManager->switchMonitors();
        }
    }
    delete w;
}


void MainWindow::slotRenderProject()
{
    if (!m_renderWidget) {
        QString projectfolder = m_activeDocument ? m_activeDocument->projectFolder().path(KUrl::AddTrailingSlash) : KdenliveSettings::defaultprojectfolder();
        m_renderWidget = new RenderWidget(projectfolder, this);
        connect(m_renderWidget, SIGNAL(shutdown()), this, SLOT(slotShutdown()));
        connect(m_renderWidget, SIGNAL(selectedRenderProfile(const QString &, const QString &, const QString &, const QString&)), this, SLOT(slotSetDocumentRenderProfile(const QString &, const QString &, const QString &, const QString&)));
        connect(m_renderWidget, SIGNAL(prepareRenderingData(bool, bool, const QString&)), this, SLOT(slotPrepareRendering(bool, bool, const QString&)));
        connect(m_renderWidget, SIGNAL(abortProcess(const QString &)), this, SIGNAL(abortRenderJob(const QString &)));
        connect(m_renderWidget, SIGNAL(openDvdWizard(const QString &, const QString &)), this, SLOT(slotDvdWizard(const QString &, const QString &)));
        if (m_activeDocument) {
            m_renderWidget->setProfile(m_activeDocument->mltProfile());
            m_renderWidget->setGuides(m_activeDocument->guidesXml(), m_activeDocument->projectDuration());
            m_renderWidget->setDocumentPath(m_activeDocument->projectFolder().path(KUrl::AddTrailingSlash));
            m_renderWidget->setRenderProfile(m_activeDocument->getDocumentProperty("renderdestination"), m_activeDocument->getDocumentProperty("rendercategory"), m_activeDocument->getDocumentProperty("renderprofile"), m_activeDocument->getDocumentProperty("renderurl"));
        }
    }
    slotCheckRenderStatus();
    /*TrackView *currentTab = (TrackView *) m_timelineArea->currentWidget();
    if (currentTab) m_renderWidget->setTimeline(currentTab);
    m_renderWidget->setDocument(m_activeDocument);*/
    m_renderWidget->show();
    m_renderWidget->showNormal();
}

void MainWindow::slotCheckRenderStatus()
{
    // Make sure there are no missing clips
    if (m_renderWidget) m_renderWidget->missingClips(m_projectList->hasMissingClips());
}

void MainWindow::setRenderingProgress(const QString &url, int progress)
{
    if (m_renderWidget) m_renderWidget->setRenderJob(url, progress);
}

void MainWindow::setRenderingFinished(const QString &url, int status, const QString &error)
{
    if (m_renderWidget) m_renderWidget->setRenderStatus(url, status, error);
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
            disconnect(m_projectList, SIGNAL(projectModified()), m_activeDocument, SLOT(setModified()));
            disconnect(m_projectMonitor->render, SIGNAL(refreshDocumentProducers()), m_activeDocument, SLOT(checkProjectClips()));

            disconnect(m_activeDocument, SIGNAL(guidesUpdated()), this, SLOT(slotGuidesUpdated()));
            disconnect(m_activeDocument, SIGNAL(addProjectClip(DocClipBase *, bool)), m_projectList, SLOT(slotAddClip(DocClipBase *, bool)));
            disconnect(m_activeDocument, SIGNAL(resetProjectList()), m_projectList, SLOT(slotResetProjectList()));
            disconnect(m_activeDocument, SIGNAL(signalDeleteProjectClip(const QString &)), m_projectList, SLOT(slotDeleteClip(const QString &)));
            disconnect(m_activeDocument, SIGNAL(updateClipDisplay(const QString &)), m_projectList, SLOT(slotUpdateClip(const QString &)));
            disconnect(m_activeDocument, SIGNAL(selectLastAddedClip(const QString &)), m_projectList, SLOT(slotSelectClip(const QString &)));
            disconnect(m_activeDocument, SIGNAL(deleteTimelineClip(const QString &)), m_activeTimeline, SLOT(slotDeleteClip(const QString &)));
            disconnect(m_activeTimeline->projectView(), SIGNAL(clipItemSelected(ClipItem*, int)), m_effectStack, SLOT(slotClipItemSelected(ClipItem*, int)));
            disconnect(m_activeTimeline->projectView(), SIGNAL(clipItemSelected(ClipItem*, int)), this, SLOT(slotActivateEffectStackView()));
            disconnect(m_activeTimeline->projectView(), SIGNAL(transitionItemSelected(Transition*, int, QPoint, bool)), m_transitionConfig, SLOT(slotTransitionItemSelected(Transition*, int, QPoint, bool)));
            disconnect(m_activeTimeline->projectView(), SIGNAL(transitionItemSelected(Transition*, int, QPoint, bool)), this, SLOT(slotActivateTransitionView(Transition *)));
            disconnect(m_activeTimeline->projectView(), SIGNAL(playMonitor()), m_projectMonitor, SLOT(slotPlay()));
            disconnect(m_zoomSlider, SIGNAL(valueChanged(int)), m_activeTimeline, SLOT(slotChangeZoom(int)));
            disconnect(m_activeTimeline->projectView(), SIGNAL(displayMessage(const QString&, MessageType)), m_messageLabel, SLOT(setMessage(const QString&, MessageType)));
            disconnect(m_activeTimeline->projectView(), SIGNAL(showClipFrame(DocClipBase *, QPoint, const int)), m_clipMonitor, SLOT(slotSetXml(DocClipBase *, QPoint, const int)));
            disconnect(m_activeTimeline, SIGNAL(cursorMoved()), m_projectMonitor, SLOT(activateMonitor()));
            disconnect(m_activeTimeline, SIGNAL(insertTrack(int)), this, SLOT(slotInsertTrack(int)));
            disconnect(m_activeTimeline, SIGNAL(deleteTrack(int)), this, SLOT(slotDeleteTrack(int)));
            disconnect(m_activeTimeline, SIGNAL(changeTrack(int)), this, SLOT(slotChangeTrack(int)));
            disconnect(m_activeDocument, SIGNAL(docModified(bool)), this, SLOT(slotUpdateDocumentState(bool)));
            disconnect(m_effectStack, SIGNAL(updateClipEffect(ClipItem*, QDomElement, QDomElement, int)), m_activeTimeline->projectView(), SLOT(slotUpdateClipEffect(ClipItem*, QDomElement, QDomElement, int)));
            disconnect(m_effectStack, SIGNAL(removeEffect(ClipItem*, QDomElement)), m_activeTimeline->projectView(), SLOT(slotDeleteEffect(ClipItem*, QDomElement)));
            disconnect(m_effectStack, SIGNAL(changeEffectState(ClipItem*, int, bool)), m_activeTimeline->projectView(), SLOT(slotChangeEffectState(ClipItem*, int, bool)));
            disconnect(m_effectStack, SIGNAL(changeEffectPosition(ClipItem*, int, int)), m_activeTimeline->projectView(), SLOT(slotChangeEffectPosition(ClipItem*, int, int)));
            disconnect(m_effectStack, SIGNAL(refreshEffectStack(ClipItem*)), m_activeTimeline->projectView(), SLOT(slotRefreshEffects(ClipItem*)));
            disconnect(m_effectStack, SIGNAL(reloadEffects()), this, SLOT(slotReloadEffects()));
            disconnect(m_transitionConfig, SIGNAL(transitionUpdated(Transition *, QDomElement)), m_activeTimeline->projectView() , SLOT(slotTransitionUpdated(Transition *, QDomElement)));
            disconnect(m_transitionConfig, SIGNAL(seekTimeline(int)), m_activeTimeline->projectView() , SLOT(setCursorPos(int)));
            disconnect(m_activeTimeline->projectView(), SIGNAL(activateDocumentMonitor()), m_projectMonitor, SLOT(activateMonitor()));
            disconnect(m_activeTimeline, SIGNAL(zoneMoved(int, int)), this, SLOT(slotZoneMoved(int, int)));
            disconnect(m_projectList, SIGNAL(loadingIsOver()), m_activeTimeline->projectView(), SLOT(slotUpdateAllThumbs()));
            disconnect(m_projectList, SIGNAL(displayMessage(const QString&, int)), this, SLOT(slotGotProgressInfo(const QString&, int)));
            disconnect(m_projectList, SIGNAL(updateRenderStatus()), this, SLOT(slotCheckRenderStatus()));
            disconnect(m_projectList, SIGNAL(clipNeedsReload(const QString&, bool)), m_activeTimeline->projectView(), SLOT(slotUpdateClip(const QString &, bool)));
            m_effectStack->clear();
        }
        //m_activeDocument->setRenderer(NULL);
        disconnect(m_projectList, SIGNAL(clipSelected(DocClipBase *)), m_clipMonitor, SLOT(slotSetXml(DocClipBase *)));
        disconnect(m_projectList, SIGNAL(refreshClip()), m_clipMonitor, SLOT(refreshMonitor()));
        m_clipMonitor->stop();
    }
    KdenliveSettings::setCurrent_profile(doc->profilePath());
    KdenliveSettings::setProject_fps(doc->fps());
    m_monitorManager->resetProfiles(doc->timecode());
    m_projectList->setDocument(doc);
    m_transitionConfig->updateProjectFormat(doc->mltProfile(), doc->timecode(), doc->tracksList());
    m_effectStack->updateProjectFormat(doc->mltProfile(), doc->timecode());
    connect(m_projectList, SIGNAL(clipSelected(DocClipBase *, QPoint)), m_clipMonitor, SLOT(slotSetXml(DocClipBase *, QPoint)));
    connect(m_projectList, SIGNAL(refreshClip()), m_clipMonitor, SLOT(refreshMonitor()));
    connect(m_projectList, SIGNAL(clipNeedsReload(const QString&, bool)), trackView->projectView(), SLOT(slotUpdateClip(const QString &, bool)));

    connect(m_projectList, SIGNAL(projectModified()), doc, SLOT(setModified()));
    connect(m_projectList, SIGNAL(clipNameChanged(const QString, const QString)), trackView->projectView(), SLOT(clipNameChanged(const QString, const QString)));

    connect(m_projectList, SIGNAL(findInTimeline(const QString&)), this, SLOT(slotClipInTimeline(const QString&)));


    connect(trackView, SIGNAL(cursorMoved()), m_projectMonitor, SLOT(activateMonitor()));
    connect(trackView, SIGNAL(insertTrack(int)), this, SLOT(slotInsertTrack(int)));
    connect(trackView, SIGNAL(deleteTrack(int)), this, SLOT(slotDeleteTrack(int)));
    connect(trackView, SIGNAL(changeTrack(int)), this, SLOT(slotChangeTrack(int)));
    connect(trackView, SIGNAL(updateTracksInfo()), this, SLOT(slotUpdateTrackInfo()));
    connect(trackView, SIGNAL(mousePosition(int)), this, SLOT(slotUpdateMousePosition(int)));
    connect(m_projectMonitor, SIGNAL(renderPosition(int)), trackView, SLOT(moveCursorPos(int)));
    connect(m_projectMonitor, SIGNAL(zoneUpdated(QPoint)), trackView, SLOT(slotSetZone(QPoint)));
    connect(m_clipMonitor, SIGNAL(zoneUpdated(QPoint)), m_projectList, SLOT(slotUpdateClipCut(QPoint)));
    connect(m_projectMonitor, SIGNAL(durationChanged(int)), trackView, SLOT(setDuration(int)));
    connect(m_projectMonitor->render, SIGNAL(refreshDocumentProducers()), doc, SLOT(checkProjectClips()));

    connect(doc, SIGNAL(addProjectClip(DocClipBase *, bool)), m_projectList, SLOT(slotAddClip(DocClipBase *, bool)));
    connect(doc, SIGNAL(resetProjectList()), m_projectList, SLOT(slotResetProjectList()));
    connect(doc, SIGNAL(signalDeleteProjectClip(const QString &)), m_projectList, SLOT(slotDeleteClip(const QString &)));
    connect(doc, SIGNAL(updateClipDisplay(const QString &)), m_projectList, SLOT(slotUpdateClip(const QString &)));
    connect(doc, SIGNAL(selectLastAddedClip(const QString &)), m_projectList, SLOT(slotSelectClip(const QString &)));

    connect(doc, SIGNAL(deleteTimelineClip(const QString &)), trackView, SLOT(slotDeleteClip(const QString &)));
    connect(doc, SIGNAL(docModified(bool)), this, SLOT(slotUpdateDocumentState(bool)));
    connect(doc, SIGNAL(guidesUpdated()), this, SLOT(slotGuidesUpdated()));


    connect(trackView->projectView(), SIGNAL(clipItemSelected(ClipItem*, int)), m_effectStack, SLOT(slotClipItemSelected(ClipItem*, int)));
    connect(trackView->projectView(), SIGNAL(updateClipMarkers(DocClipBase *)), this, SLOT(slotUpdateClipMarkers(DocClipBase*)));

    connect(trackView->projectView(), SIGNAL(clipItemSelected(ClipItem*, int)), this, SLOT(slotActivateEffectStackView()));
    connect(trackView->projectView(), SIGNAL(transitionItemSelected(Transition*, int, QPoint, bool)), m_transitionConfig, SLOT(slotTransitionItemSelected(Transition*, int, QPoint, bool)));
    connect(trackView->projectView(), SIGNAL(transitionItemSelected(Transition*, int, QPoint, bool)), this, SLOT(slotActivateTransitionView(Transition *)));
    m_zoomSlider->setValue(doc->zoom().x());
    connect(m_zoomSlider, SIGNAL(valueChanged(int)), trackView, SLOT(slotChangeZoom(int)));
    connect(trackView->projectView(), SIGNAL(zoomIn()), this, SLOT(slotZoomIn()));
    connect(trackView->projectView(), SIGNAL(zoomOut()), this, SLOT(slotZoomOut()));
    connect(trackView, SIGNAL(setZoom(int)), this, SLOT(slotSetZoom(int)));
    connect(trackView->projectView(), SIGNAL(displayMessage(const QString&, MessageType)), m_messageLabel, SLOT(setMessage(const QString&, MessageType)));

    connect(trackView->projectView(), SIGNAL(showClipFrame(DocClipBase *, QPoint, const int)), m_clipMonitor, SLOT(slotSetXml(DocClipBase *, QPoint, const int)));
    connect(trackView->projectView(), SIGNAL(playMonitor()), m_projectMonitor, SLOT(slotPlay()));


    connect(m_effectStack, SIGNAL(updateClipEffect(ClipItem*, QDomElement, QDomElement, int)), trackView->projectView(), SLOT(slotUpdateClipEffect(ClipItem*, QDomElement, QDomElement, int)));
    connect(m_effectStack, SIGNAL(removeEffect(ClipItem*, QDomElement)), trackView->projectView(), SLOT(slotDeleteEffect(ClipItem*, QDomElement)));
    connect(m_effectStack, SIGNAL(changeEffectState(ClipItem*, int, bool)), trackView->projectView(), SLOT(slotChangeEffectState(ClipItem*, int, bool)));
    connect(m_effectStack, SIGNAL(changeEffectPosition(ClipItem*, int, int)), trackView->projectView(), SLOT(slotChangeEffectPosition(ClipItem*, int, int)));
    connect(m_effectStack, SIGNAL(refreshEffectStack(ClipItem*)), trackView->projectView(), SLOT(slotRefreshEffects(ClipItem*)));
    connect(m_transitionConfig, SIGNAL(transitionUpdated(Transition *, QDomElement)), trackView->projectView() , SLOT(slotTransitionUpdated(Transition *, QDomElement)));
    connect(m_transitionConfig, SIGNAL(seekTimeline(int)), trackView->projectView() , SLOT(setCursorPos(int)));
    connect(m_effectStack, SIGNAL(seekTimeline(int)), trackView->projectView() , SLOT(setCursorPos(int)));
    connect(m_effectStack, SIGNAL(reloadEffects()), this, SLOT(slotReloadEffects()));

    connect(trackView->projectView(), SIGNAL(activateDocumentMonitor()), m_projectMonitor, SLOT(activateMonitor()));
    connect(trackView, SIGNAL(zoneMoved(int, int)), this, SLOT(slotZoneMoved(int, int)));
    connect(m_projectList, SIGNAL(loadingIsOver()), trackView->projectView(), SLOT(slotUpdateAllThumbs()));
    connect(m_projectList, SIGNAL(displayMessage(const QString&, int)), this, SLOT(slotGotProgressInfo(const QString&, int)));
    connect(m_projectList, SIGNAL(updateRenderStatus()), this, SLOT(slotCheckRenderStatus()));


    trackView->projectView()->setContextMenu(m_timelineContextMenu, m_timelineContextClipMenu, m_timelineContextTransitionMenu, m_clipTypeGroup, (QMenu*)(factory()->container("marker_menu", this)));
    m_activeTimeline = trackView;
    if (m_renderWidget) {
        slotCheckRenderStatus();
        m_renderWidget->setProfile(doc->mltProfile());
        m_renderWidget->setGuides(doc->guidesXml(), doc->projectDuration());
        m_renderWidget->setDocumentPath(doc->projectFolder().path(KUrl::AddTrailingSlash));
        m_renderWidget->setRenderProfile(doc->getDocumentProperty("renderdestination"), doc->getDocumentProperty("rendercategory"), doc->getDocumentProperty("renderprofile"), doc->getDocumentProperty("renderurl"));
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
    if (KdenliveSettings::dropbframes()) slotUpdatePreviewSettings();
    //Update the mouse position display so it will display in DF/NDF format by default based on the project setting.
    slotUpdateMousePosition(0);
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
    if (m_renderWidget) m_renderWidget->setGuides(m_activeDocument->guidesXml(), m_activeDocument->projectDuration());
}

void MainWindow::slotPreferences(int page, int option)
{
    //An instance of your dialog could be already created and could be
    // cached, in which case you want to display the cached dialog
    // instead of creating another one
    if (KConfigDialog::showDialog("settings")) {
        KdenliveSettingsDialog* d = static_cast <KdenliveSettingsDialog*>(KConfigDialog::exists("settings"));
        if (page != -1) d->showPage(page, option);
        return;
    }

    // KConfigDialog didn't find an instance of this dialog, so lets
    // create it :
    KdenliveSettingsDialog* dialog = new KdenliveSettingsDialog(this);
    connect(dialog, SIGNAL(settingsChanged(const QString&)), this, SLOT(updateConfiguration()));
    //connect(dialog, SIGNAL(doResetProfile()), this, SLOT(slotDetectAudioDriver()));
    connect(dialog, SIGNAL(doResetProfile()), m_monitorManager, SLOT(slotResetProfiles()));
    connect(dialog, SIGNAL(updatePreviewSettings()), this, SLOT(slotUpdatePreviewSettings()));
#ifndef Q_WS_MAC
    connect(dialog, SIGNAL(updateCaptureFolder()), m_recMonitor, SLOT(slotUpdateCaptureFolder()));
#endif
    //connect(dialog, SIGNAL(updatePreviewSettings()), this, SLOT(slotUpdatePreviewSettings()));
    dialog->show();
    if (page != -1) dialog->showPage(page, option);
}

void MainWindow::slotUpdatePreviewSettings()
{
    if (m_activeDocument) {
        m_clipMonitor->slotSetXml(NULL);
        m_activeDocument->updatePreviewSettings();
    }
}

void MainWindow::updateConfiguration()
{
    //TODO: we should apply settings to all projects, not only the current one
    if (m_activeTimeline) {
        m_activeTimeline->refresh();
        m_activeTimeline->projectView()->checkAutoScroll();
        m_activeTimeline->projectView()->checkTrackHeight();
        if (m_activeDocument) m_activeDocument->clipManager()->checkAudioThumbs();
    }
    m_buttonAudioThumbs->setChecked(KdenliveSettings::audiothumbnails());
    m_buttonVideoThumbs->setChecked(KdenliveSettings::videothumbnails());

    // Update list of transcoding profiles
    loadTranscoders();
#ifndef NO_JOGSHUTTLE
    activateShuttleDevice();
#endif /* NO_JOGSHUTTLE */

}


void MainWindow::slotSwitchVideoThumbs()
{
    KdenliveSettings::setVideothumbnails(!KdenliveSettings::videothumbnails());
    if (m_activeTimeline) {
        m_activeTimeline->projectView()->slotUpdateAllThumbs();
    }
    m_buttonVideoThumbs->setChecked(KdenliveSettings::videothumbnails());
}

void MainWindow::slotSwitchAudioThumbs()
{
    KdenliveSettings::setAudiothumbnails(!KdenliveSettings::audiothumbnails());
    if (m_activeTimeline) {
        m_activeTimeline->refresh();
        m_activeTimeline->projectView()->checkAutoScroll();
        if (m_activeDocument) m_activeDocument->clipManager()->checkAudioThumbs();
    }
    m_buttonAudioThumbs->setChecked(KdenliveSettings::audiothumbnails());
}

void MainWindow::slotSwitchMarkersComments()
{
    KdenliveSettings::setShowmarkers(!KdenliveSettings::showmarkers());
    if (m_activeTimeline) {
        m_activeTimeline->refresh();
    }
    m_buttonShowMarkers->setChecked(KdenliveSettings::showmarkers());
}

void MainWindow::slotSwitchSnap()
{
    KdenliveSettings::setSnaptopoints(!KdenliveSettings::snaptopoints());
    m_buttonSnap->setChecked(KdenliveSettings::snaptopoints());
}


void MainWindow::slotDeleteTimelineClip()
{
    if (QApplication::focusWidget() && QApplication::focusWidget()->parentWidget() && QApplication::focusWidget()->parentWidget()->parentWidget() && QApplication::focusWidget()->parentWidget()->parentWidget() == m_projectListDock) m_projectList->slotRemoveClip();
    else if (m_activeTimeline) {
        m_activeTimeline->projectView()->deleteSelectedClips();
    }
}

void MainWindow::slotUpdateClipMarkers(DocClipBase *clip)
{
    if (m_clipMonitor->isActive()) m_clipMonitor->checkOverlay();
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
    CommentedTime marker(pos, i18n("Marker"));
    MarkerDialog d(clip, marker, m_activeDocument->timecode(), i18n("Add Marker"), this);
    if (d.exec() == QDialog::Accepted) {
        m_activeTimeline->projectView()->slotAddClipMarker(id, d.newMarker().time(), d.newMarker().comment());
    }
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
    QString oldcomment = clip->markerComment(pos);
    if (oldcomment.isEmpty()) {
        m_messageLabel->setMessage(i18n("No marker found at cursor time"), ErrorMessage);
        return;
    }

    CommentedTime marker(pos, oldcomment);
    MarkerDialog d(clip, marker, m_activeDocument->timecode(), i18n("Edit Marker"), this);
    if (d.exec() == QDialog::Accepted) {
        m_activeTimeline->projectView()->slotAddClipMarker(id, d.newMarker().time(), d.newMarker().comment());
        if (d.newMarker().time() != pos) {
            // remove old marker
            m_activeTimeline->projectView()->slotAddClipMarker(id, pos, QString());
        }
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
    m_projectMonitor->activateMonitor();
    if (m_activeTimeline)
        m_activeTimeline->projectView()->slotInsertTrack(ix);
    if (m_activeDocument)
        m_transitionConfig->updateProjectFormat(m_activeDocument->mltProfile(), m_activeDocument->timecode(), m_activeDocument->tracksList());
}

void MainWindow::slotDeleteTrack(int ix)
{
    m_projectMonitor->activateMonitor();
    if (m_activeTimeline)
        m_activeTimeline->projectView()->slotDeleteTrack(ix);
    if (m_activeDocument)
        m_transitionConfig->updateProjectFormat(m_activeDocument->mltProfile(), m_activeDocument->timecode(), m_activeDocument->tracksList());
}

void MainWindow::slotChangeTrack(int ix)
{
    m_projectMonitor->activateMonitor();
    if (m_activeTimeline)
        m_activeTimeline->projectView()->slotChangeTrack(ix);
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
    if (m_activeTimeline) {
        m_activeTimeline->projectView()->cutSelectedClips();
    }
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
    if (m_activeTimeline) {
        m_activeTimeline->projectView()->selectClip(true);
    }
}

void MainWindow::slotSelectTimelineTransition()
{
    if (m_activeTimeline) {
        m_activeTimeline->projectView()->selectTransition(true);
    }
}

void MainWindow::slotDeselectTimelineClip()
{
    if (m_activeTimeline) {
        m_activeTimeline->projectView()->selectClip(false, true);
    }
}

void MainWindow::slotDeselectTimelineTransition()
{
    if (m_activeTimeline) {
        m_activeTimeline->projectView()->selectTransition(false, true);
    }
}

void MainWindow::slotSelectAddTimelineClip()
{
    if (m_activeTimeline) {
        m_activeTimeline->projectView()->selectClip(true, true);
    }
}

void MainWindow::slotSelectAddTimelineTransition()
{
    if (m_activeTimeline) {
        m_activeTimeline->projectView()->selectTransition(true, true);
    }
}

void MainWindow::slotGroupClips()
{
    if (m_activeTimeline) {
        m_activeTimeline->projectView()->groupClips();
    }
}

void MainWindow::slotUnGroupClips()
{
    if (m_activeTimeline) {
        m_activeTimeline->projectView()->groupClips(false);
    }
}

void MainWindow::slotEditItemDuration()
{
    if (m_activeTimeline) {
        m_activeTimeline->projectView()->editItemDuration();
    }
}

void MainWindow::slotAddProjectClip(KUrl url)
{
    if (m_activeDocument)
        m_activeDocument->slotAddClipFile(url, QString());
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
    QStringList info = result->data().toStringList();
    if (info.isEmpty()) return;
    QDomElement effect = videoEffects.getEffectByTag(info.at(1), info.at(2));
    slotAddEffect(effect);
}

void MainWindow::slotAddAudioEffect(QAction *result)
{
    if (!result) return;
    QStringList info = result->data().toStringList();
    if (info.isEmpty()) return;
    QDomElement effect = audioEffects.getEffectByTag(info.at(1), info.at(2));
    slotAddEffect(effect);
}

void MainWindow::slotAddCustomEffect(QAction *result)
{
    if (!result) return;
    QStringList info = result->data().toStringList();
    if (info.isEmpty()) return;
    QDomElement effect = customEffects.getEffectByTag(info.at(1), info.at(2));
    slotAddEffect(effect);
}

void MainWindow::slotZoomIn()
{
    m_zoomSlider->setValue(m_zoomSlider->value() - 1);
}

void MainWindow::slotZoomOut()
{
    m_zoomSlider->setValue(m_zoomSlider->value() + 1);
}

void MainWindow::slotFitZoom()
{
    if (m_activeTimeline) {
        m_zoomSlider->setValue(m_activeTimeline->fitZoom());
    }
}

void MainWindow::slotSetZoom(int value)
{
    m_zoomSlider->setValue(value);
}

void MainWindow::slotGotProgressInfo(const QString &message, int progress)
{
    m_statusProgressBar->setValue(progress);
    if (progress >= 0) {
        if (!message.isEmpty()) m_messageLabel->setMessage(message, InformationMessage);//statusLabel->setText(message);
        m_statusProgressBar->setVisible(true);
    } else if (progress == -2) {
        if (!message.isEmpty()) m_messageLabel->setMessage(message, ErrorMessage);
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
                    EditClipCommand *command = new EditClipCommand(m_projectList, clip->getId(), clip->properties(), newprops, true);
                    m_activeDocument->commandStack()->push(command);
                }
            }
            delete dia;
            return;
        }
        QString path = clip->getProperty("resource");
        TitleWidget *dia_ui = new TitleWidget(KUrl(), m_activeDocument->timecode(), titlepath, m_projectMonitor->render, this);
        QDomDocument doc;
        doc.setContent(clip->getProperty("xmldata"));
        dia_ui->setXml(doc);
        if (dia_ui->exec() == QDialog::Accepted) {
            QMap <QString, QString> newprops;
            newprops.insert("xmldata", dia_ui->xml().toString());
            if (dia_ui->outPoint() != clip->duration().frames(m_activeDocument->fps()) - 1) {
                // duration changed, we need to update duration
                newprops.insert("out", QString::number(dia_ui->outPoint()));
            }
            EditClipCommand *command = new EditClipCommand(m_projectList, clip->getId(), clip->properties(), newprops, true);
            m_activeDocument->commandStack()->push(command);
            //m_activeTimeline->projectView()->slotUpdateClip(clip->getId());
            m_activeDocument->setModified(true);
        }
        delete dia_ui;

        //m_activeDocument->editTextClip(clip->getProperty("xml"), clip->getId());
        return;
    }
    ClipProperties dia(clip, m_activeDocument->timecode(), m_activeDocument->fps(), this);
    connect(&dia, SIGNAL(addMarker(const QString &, GenTime, QString)), m_activeTimeline->projectView(), SLOT(slotAddClipMarker(const QString &, GenTime, QString)));
    if (dia.exec() == QDialog::Accepted) {
        QMap <QString, QString> newprops = dia.properties();
        if (newprops.isEmpty()) return;
        EditClipCommand *command = new EditClipCommand(m_projectList, clip->getId(), clip->properties(), newprops, true);
        m_activeDocument->commandStack()->push(command);

        if (dia.needsTimelineRefresh()) {
            // update clip occurences in timeline
            m_activeTimeline->projectView()->slotUpdateClip(clip->getId(), dia.needsTimelineReload());
        }
    }
}


void MainWindow::slotShowClipProperties(QList <DocClipBase *> cliplist, QMap<QString, QString> commonproperties)
{
    ClipProperties dia(cliplist, m_activeDocument->timecode(), commonproperties, this);
    if (dia.exec() == QDialog::Accepted) {
        QUndoCommand *command = new QUndoCommand();
        command->setText(i18n("Edit clips"));
        for (int i = 0; i < cliplist.count(); i++) {
            DocClipBase *clip = cliplist.at(i);
            new EditClipCommand(m_projectList, clip->getId(), clip->properties(), dia.properties(), true, command);
        }
        m_activeDocument->commandStack()->push(command);
        for (int i = 0; i < cliplist.count(); i++) {
            m_activeTimeline->projectView()->slotUpdateClip(cliplist.at(i)->getId(), dia.needsTimelineReload());
        }
    }
}

void MainWindow::customEvent(QEvent* e)
{
    if (e->type() == QEvent::User) {
        m_messageLabel->setMessage(static_cast <MltErrorEvent *>(e)->message(), MltError);
    }
}
void MainWindow::slotActivateEffectStackView()
{
    m_effectStack->raiseWindow(m_effectStackDock);
}

void MainWindow::slotActivateTransitionView(Transition *t)
{
    if (t) m_transitionConfig->raiseWindow(m_transitionConfigDock);
}

void MainWindow::slotSnapRewind()
{
    if (m_projectMonitor->isActive()) {
        if (m_activeTimeline)
            m_activeTimeline->projectView()->slotSeekToPreviousSnap();
    } else m_clipMonitor->slotSeekToPreviousSnap();
}

void MainWindow::slotSnapForward()
{
    if (m_projectMonitor->isActive()) {
        if (m_activeTimeline)
            m_activeTimeline->projectView()->slotSeekToNextSnap();
    } else m_clipMonitor->slotSeekToNextSnap();
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
    if (m_projectMonitor->isActive()) m_projectMonitor->slotZoneStart();
    else m_clipMonitor->slotZoneStart();
}

void MainWindow::slotZoneEnd()
{
    if (m_projectMonitor->isActive()) m_projectMonitor->slotZoneEnd();
    else m_clipMonitor->slotZoneEnd();
}

void MainWindow::slotChangeTool(QAction * action)
{
    if (action == m_buttonSelectTool) slotSetTool(SELECTTOOL);
    else if (action == m_buttonRazorTool) slotSetTool(RAZORTOOL);
    else if (action == m_buttonSpacerTool) slotSetTool(SPACERTOOL);
}

void MainWindow::slotChangeEdit(QAction * action)
{
    if (!m_activeTimeline) return;
    if (action == m_overwriteEditTool) m_activeTimeline->projectView()->setEditMode(OVERWRITEEDIT);
    else if (action == m_insertEditTool) m_activeTimeline->projectView()->setEditMode(INSERTEDIT);
    else m_activeTimeline->projectView()->setEditMode(NORMALEDIT);
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
    if (!m_activeDocument || !m_activeTimeline) return;
    m_activeTimeline->projectView()->copyClip();
}

void MainWindow::slotPaste()
{
    if (!m_activeDocument || !m_activeTimeline) return;
    m_activeTimeline->projectView()->pasteClip();
}

void MainWindow::slotPasteEffects()
{
    if (!m_activeDocument || !m_activeTimeline) return;
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
    if (m_activeTimeline && m_activeTimeline->projectView()->findNextString(m_findString)) {
        statusBar()->showMessage(i18n("Found: %1", m_findString));
    } else {
        statusBar()->showMessage(i18n("Reached end of project"));
    }
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

        for (int i = 0; i < matching.size(); ++i) {
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
    } else KXmlGuiWindow::keyPressEvent(ke);
}


/** Gets called when the window gets hidden */
void MainWindow::hideEvent(QHideEvent */*event*/)
{
    // kDebug() << "I was hidden";
    // issue http://www.kdenlive.org/mantis/view.php?id=231
    if (isMinimized()) {
        // kDebug() << "I am minimized";
        if (m_monitorManager) m_monitorManager->stopActiveMonitor();
    }
}

bool MainWindow::eventFilter(QObject *obj, QEvent *event)
{
    if (m_findActivated) {
        if (event->type() == QEvent::ShortcutOverride) {
            QKeyEvent* ke = (QKeyEvent*) event;
            if (ke->text().trimmed().isEmpty()) return false;
            ke->accept();
            return true;
        } else return false;
    } else {
        // pass the event on to the parent class
        return QMainWindow::eventFilter(obj, event);
    }
}


void MainWindow::slotSaveZone(Render *render, QPoint zone)
{
    KDialog *dialog = new KDialog(this);
    dialog->setCaption("Save clip zone");
    dialog->setButtons(KDialog::Ok | KDialog::Cancel);

    QWidget *widget = new QWidget(dialog);
    dialog->setMainWidget(widget);

    QVBoxLayout *vbox = new QVBoxLayout(widget);
    QLabel *label1 = new QLabel(i18n("Save clip zone as:"), this);
    QString path = m_activeDocument->projectFolder().path();
    path.append("/");
    path.append("untitled.mlt");
    KUrlRequester *url = new KUrlRequester(KUrl(path), this);
    url->setFilter("video/mlt-playlist");
    QLabel *label2 = new QLabel(i18n("Description:"), this);
    KLineEdit *edit = new KLineEdit(this);
    vbox->addWidget(label1);
    vbox->addWidget(url);
    vbox->addWidget(label2);
    vbox->addWidget(edit);
    if (dialog->exec() == QDialog::Accepted) render->saveZone(url->url(), edit->text(), zone);

}

void MainWindow::slotSetInPoint()
{
    if (m_clipMonitor->isActive()) {
        m_clipMonitor->slotSetZoneStart();
    } else m_projectMonitor->slotSetZoneStart();
    //else m_activeTimeline->projectView()->setInPoint();
}

void MainWindow::slotSetOutPoint()
{
    if (m_clipMonitor->isActive()) {
        m_clipMonitor->slotSetZoneEnd();
    } else m_projectMonitor->slotSetZoneEnd();
    // else m_activeTimeline->projectView()->setOutPoint();
}

void MainWindow::slotResizeItemStart()
{
    if (m_activeTimeline) m_activeTimeline->projectView()->setInPoint();
}

void MainWindow::slotResizeItemEnd()
{
    if (m_activeTimeline) m_activeTimeline->projectView()->setOutPoint();
}

int MainWindow::getNewStuff(const QString &configFile)
{
    KNS3::Entry::List entries;
#if KDE_IS_VERSION(4,3,80)
    KNS3::DownloadDialog dialog(configFile);
    dialog.exec();
    entries = dialog.changedEntries();
    foreach(const KNS3::Entry &entry, entries) {
        if (entry.status() == KNS3::Entry::Installed)
            kDebug() << "// Installed files: " << entry.installedFiles();
    }
#else
    KNS::Engine engine(0);
    if (engine.init(configFile))
        entries = engine.downloadDialogModal(this);
    foreach(KNS::Entry *entry, entries) {
        if (entry->status() == KNS::Entry::Installed)
            kDebug() << "// Installed files: " << entry->installedFiles();
    }
#endif /* KDE_IS_VERSION(4,3,80) */
    return entries.size();
}

void MainWindow::slotGetNewTitleStuff()
{
    if (getNewStuff("kdenlive_titles.knsrc") > 0) {
        TitleWidget::refreshTitleTemplates();
    }
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
    if (getNewStuff("kdenlive_renderprofiles.knsrc") > 0) {
        if (m_renderWidget)
            m_renderWidget->reloadProfiles();
    }
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
    if (m_activeTimeline) m_activeTimeline->projectView()->autoTransition();
}

void MainWindow::slotSplitAudio()
{
    if (m_activeTimeline) m_activeTimeline->projectView()->splitAudio();
}

void MainWindow::slotUpdateClipType(QAction *action)
{
    if (m_activeTimeline) {
        if (action->data().toString() == "clip_audio_only") m_activeTimeline->projectView()->setAudioOnly();
        else if (action->data().toString() == "clip_video_only") m_activeTimeline->projectView()->setVideoOnly();
        else m_activeTimeline->projectView()->setAudioAndVideo();
    }
}

void MainWindow::slotDvdWizard(const QString &url, const QString &profile)
{
    // We must stop the monitors since we create a new on in the dvd wizard
    m_clipMonitor->stop();
    m_projectMonitor->stop();
    DvdWizard w(url, profile, this);
    w.exec();
    m_projectMonitor->start();
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

void MainWindow::slotMaximizeCurrent(bool /*show*/)
{
    //TODO: is there a way to maximize current widget?
    //if (show == true)
    {
        m_timelineState = saveState();
        QWidget *par = focusWidget()->parentWidget();
        while (par->parentWidget() && par->parentWidget() != this) {
            par = par->parentWidget();
        }
        kDebug() << "CURRENT WIDGET: " << par->objectName();
    }
    /*else {
    //centralWidget()->setHidden(false);
    //restoreState(m_timelineState);
    }*/
}

void MainWindow::loadTranscoders()
{
    QMenu *transMenu = static_cast<QMenu*>(factory()->container("transcoders", this));
    transMenu->clear();

    KSharedConfigPtr config = KSharedConfig::openConfig("kdenlivetranscodingrc");
    KConfigGroup transConfig(config, "Transcoding");
    // read the entries
    QMap< QString, QString > profiles = transConfig.entryMap();
    QMapIterator<QString, QString> i(profiles);
    while (i.hasNext()) {
        i.next();
        QStringList data = i.value().split(";", QString::SkipEmptyParts);
        QAction *a = transMenu->addAction(i.key());
        a->setData(data);
        if (data.count() > 1) {
            a->setToolTip(data.at(1));
        }
        connect(a, SIGNAL(triggered()), this, SLOT(slotTranscode()));
    }
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
        if (data.count() > 2) condition = data.at(2);
        urls << m_projectList->getConditionalUrls(condition);
        urls.removeAll(KUrl());
    }
    if (urls.isEmpty()) {
        m_messageLabel->setMessage(i18n("No clip to transcode"), ErrorMessage);
        return;
    }
    ClipTranscode *d = new ClipTranscode(urls, params, desc);
    connect(d, SIGNAL(addClip(KUrl)), this, SLOT(slotAddProjectClip(KUrl)));
    d->show();
    //QProcess::startDetached("ffmpeg", parameters);
}

void MainWindow::slotTranscodeClip()
{
    KUrl::List urls = KFileDialog::getOpenUrls(KUrl("kfiledialog:///projectfolder"));
    if (urls.isEmpty()) return;
    slotTranscode(urls);
}

void MainWindow::slotSetDocumentRenderProfile(const QString &dest, const QString &group, const QString &name, const QString &file)
{
    if (m_activeDocument == NULL) return;
    m_activeDocument->setDocumentProperty("renderdestination", dest);
    m_activeDocument->setDocumentProperty("rendercategory", group);
    m_activeDocument->setDocumentProperty("renderprofile", name);
    m_activeDocument->setDocumentProperty("renderurl", file);
    m_activeDocument->setModified(true);
}


void MainWindow::slotPrepareRendering(bool scriptExport, bool zoneOnly, const QString &chapterFile)
{
    if (m_activeDocument == NULL || m_renderWidget == NULL) return;
    QString scriptPath;
    QString playlistPath;
    if (scriptExport) {
        bool ok;
        QString scriptsFolder = m_activeDocument->projectFolder().path() + "/scripts/";
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
        m_projectMonitor->saveSceneList(playlistPath);
    } else {
        KTemporaryFile temp;
        temp.setAutoRemove(false);
        temp.setSuffix(".mlt");
        temp.open();
        playlistPath = temp.fileName();
        m_projectMonitor->saveSceneList(playlistPath);
    }

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

    m_renderWidget->slotExport(scriptExport, m_activeTimeline->inPoint(), m_activeTimeline->outPoint(), playlistPath, scriptPath);
}

void MainWindow::slotUpdateTimecodeFormat(int ix)
{
    KdenliveSettings::setFrametimecode(ix == 1);
    m_clipMonitor->updateTimecodeFormat();
    m_projectMonitor->updateTimecodeFormat();
    m_activeTimeline->projectView()->clearSelection();
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
    closeCurrentDocument(false);
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
    QPalette plt;
    if (theme.isEmpty())
        plt = QApplication::desktop()->palette();
    else {
        KSharedConfigPtr config = KSharedConfig::openConfig(theme);
        plt = KGlobalSettings::createApplicationPalette(config);
    }

    kapp->setPalette(plt);
    const QObjectList children = statusBar()->children();

    foreach(QObject *child, children) {
        if (child->isWidgetType())
            ((QWidget*)child)->setPalette(plt);
        const QObjectList subchildren = child->children();
        foreach(QObject *subchild, subchildren) {
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
    m_monitorManager->slotSwitchMonitors(m_clipMonitor->isActive());
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
    for (int i = 0; i < ids.size(); ++i) {
        m_activeTimeline->slotDeleteClip(ids.at(i));
    }
    m_activeDocument->clipManager()->slotDeleteClips(ids);
    if (!folderids.isEmpty()) m_projectList->deleteProjectFolder(folderids);

}

void MainWindow::slotShowTitleBars(bool show)
{
    if (show) {
        m_effectStackDock->setTitleBarWidget(0);
        m_clipMonitorDock->setTitleBarWidget(0);
        m_projectMonitorDock->setTitleBarWidget(0);
#ifndef Q_WS_MAC
        m_recMonitorDock->setTitleBarWidget(0);
#endif
        m_effectListDock->setTitleBarWidget(0);
        m_transitionConfigDock->setTitleBarWidget(0);
        m_projectListDock->setTitleBarWidget(0);
        m_undoViewDock->setTitleBarWidget(0);
    } else {
        if (!m_effectStackDock->isFloating()) m_effectStackDock->setTitleBarWidget(new QWidget(this));
        if (!m_clipMonitorDock->isFloating()) m_clipMonitorDock->setTitleBarWidget(new QWidget(this));
        if (!m_projectMonitorDock->isFloating()) m_projectMonitorDock->setTitleBarWidget(new QWidget(this));
#ifndef Q_WS_MAC
        if (!m_recMonitorDock->isFloating()) m_recMonitorDock->setTitleBarWidget(new QWidget(this));
#endif
        if (!m_effectListDock->isFloating()) m_effectListDock->setTitleBarWidget(new QWidget(this));
        if (!m_transitionConfigDock->isFloating()) m_transitionConfigDock->setTitleBarWidget(new QWidget(this));
        if (!m_projectListDock->isFloating()) m_projectListDock->setTitleBarWidget(new QWidget(this));
        if (!m_undoViewDock->isFloating()) m_undoViewDock->setTitleBarWidget(new QWidget(this));
    }
    KdenliveSettings::setShowtitlebars(show);
}

void MainWindow::slotSwitchTitles()
{
    slotShowTitleBars(!KdenliveSettings::showtitlebars());
}

#include "mainwindow.moc"

