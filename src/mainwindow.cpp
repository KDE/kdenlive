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



#include <QTextStream>
#include <QTimer>
#include <QAction>
#include <QtTest>
#include <QtCore>

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
#include <KActionMenu>
#include <KMenu>
#include <ktogglefullscreenaction.h>

#include <mlt++/Mlt.h>

#include "mainwindow.h"
#include "kdenlivesettings.h"
#include "kdenlivesettingsdialog.h"
#include "initeffects.h"
#include "profilesdialog.h"
#include "projectsettings.h"
#include "events.h"
#include "renderjob.h"

#define ID_STATUS_MSG 1
#define ID_EDITMODE_MSG 2
#define ID_TIMELINE_MSG 3
#define ID_TIMELINE_BUTTONS 5
#define ID_TIMELINE_POS 6
#define ID_TIMELINE_FORMAT 7

MainWindow::MainWindow(QWidget *parent)
        : KXmlGuiWindow(parent),
        m_activeDocument(NULL), m_activeTimeline(NULL), m_renderWidget(NULL), m_jogProcess(NULL) {
    parseProfiles();

    m_commandStack = new QUndoGroup;
    m_timelineArea = new KTabWidget(this);
    m_timelineArea->setTabReorderingEnabled(true);
    m_timelineArea->setTabBarHidden(true);

    QToolButton *closeTabButton = new QToolButton;
    connect(closeTabButton, SIGNAL(clicked()), this, SLOT(slotRemoveTab()));
    closeTabButton->setIcon(KIcon("tab-close"));
    closeTabButton->adjustSize();
    closeTabButton->setToolTip(i18n("Close the current tab"));
    m_timelineArea->setCornerWidget(closeTabButton);
    connect(m_timelineArea, SIGNAL(currentChanged(int)), this, SLOT(activateDocument()));

    initEffects::parseEffectFiles(&m_audioEffects, &m_videoEffects, &m_transitions);
    m_monitorManager = new MonitorManager();

    projectListDock = new QDockWidget(i18n("Project Tree"), this);
    projectListDock->setObjectName("project_tree");
    m_projectList = new ProjectList(this);
    projectListDock->setWidget(m_projectList);
    addDockWidget(Qt::TopDockWidgetArea, projectListDock);

    effectListDock = new QDockWidget(i18n("Effect List"), this);
    effectListDock->setObjectName("effect_list");
    m_effectList = new EffectsListView(&m_audioEffects, &m_videoEffects, &m_customEffects);

    //m_effectList = new KListWidget(this);
    effectListDock->setWidget(m_effectList);
    addDockWidget(Qt::TopDockWidgetArea, effectListDock);

    effectStackDock = new QDockWidget(i18n("Effect Stack"), this);
    effectStackDock->setObjectName("effect_stack");
    effectStack = new EffectStackView(&m_audioEffects, &m_videoEffects, &m_customEffects, this);
    effectStackDock->setWidget(effectStack);
    addDockWidget(Qt::TopDockWidgetArea, effectStackDock);

    transitionConfigDock = new QDockWidget(i18n("Transition"), this);
    transitionConfigDock->setObjectName("transition");
    transitionConfig = new TransitionSettings(&m_transitions, this);
    transitionConfigDock->setWidget(transitionConfig);
    addDockWidget(Qt::TopDockWidgetArea, transitionConfigDock);


    clipMonitorDock = new QDockWidget(i18n("Clip Monitor"), this);
    clipMonitorDock->setObjectName("clip_monitor");
    m_clipMonitor = new Monitor("clip", m_monitorManager, this);
    clipMonitorDock->setWidget(m_clipMonitor);
    addDockWidget(Qt::TopDockWidgetArea, clipMonitorDock);
    //m_clipMonitor->stop();

    projectMonitorDock = new QDockWidget(i18n("Project Monitor"), this);
    projectMonitorDock->setObjectName("project_monitor");
    m_projectMonitor = new Monitor("project", m_monitorManager, this);
    projectMonitorDock->setWidget(m_projectMonitor);
    addDockWidget(Qt::TopDockWidgetArea, projectMonitorDock);

    recMonitorDock = new QDockWidget(i18n("Record Monitor"), this);
    recMonitorDock->setObjectName("record_monitor");
    m_recMonitor = new RecMonitor("record", this);
    recMonitorDock->setWidget(m_recMonitor);
    addDockWidget(Qt::TopDockWidgetArea, recMonitorDock);

    connect(m_recMonitor, SIGNAL(addProjectClip(KUrl)), this, SLOT(slotAddProjectClip(KUrl)));

    undoViewDock = new QDockWidget(i18n("Undo History"), this);
    undoViewDock->setObjectName("undo_history");
    m_undoView = new QUndoView(this);
    m_undoView->setCleanIcon(KIcon("edit-clear"));
    m_undoView->setEmptyLabel(i18n("Clean"));
    undoViewDock->setWidget(m_undoView);
    m_undoView->setGroup(m_commandStack);
    addDockWidget(Qt::TopDockWidgetArea, undoViewDock);

    overviewDock = new QDockWidget(i18n("Project Overview"), this);
    overviewDock->setObjectName("project_overview");
    m_overView = new CustomTrackView(NULL, NULL, this);
    overviewDock->setWidget(m_overView);
    addDockWidget(Qt::TopDockWidgetArea, overviewDock);

    setupActions();
    tabifyDockWidget(projectListDock, effectListDock);
    tabifyDockWidget(projectListDock, effectStackDock);
    tabifyDockWidget(projectListDock, transitionConfigDock);
    tabifyDockWidget(projectListDock, undoViewDock);
    projectListDock->raise();

    tabifyDockWidget(clipMonitorDock, projectMonitorDock);
    setCentralWidget(m_timelineArea);

    m_timecodeFormat = new KComboBox(this);
    m_timecodeFormat->addItem(i18n("hh:mm:ss::ff"));
    m_timecodeFormat->addItem(i18n("Frames"));

    statusProgressBar = new QProgressBar(this);
    statusProgressBar->setMinimum(0);
    statusProgressBar->setMaximum(100);
    statusProgressBar->setMaximumWidth(150);
    statusProgressBar->setVisible(false);
    statusLabel = new QLabel(this);

    QWidget *w = new QWidget;
    timeline_buttons_ui.setupUi(w);
    timeline_buttons_ui.buttonVideo->setDown(KdenliveSettings::videothumbnails());
    timeline_buttons_ui.buttonAudio->setDown(KdenliveSettings::audiothumbnails());
    connect(timeline_buttons_ui.buttonVideo, SIGNAL(clicked()), this, SLOT(slotSwitchVideoThumbs()));
    connect(timeline_buttons_ui.buttonAudio, SIGNAL(clicked()), this, SLOT(slotSwitchAudioThumbs()));
    connect(timeline_buttons_ui.buttonFitZoom, SIGNAL(clicked()), this, SLOT(slotFitZoom()));

    statusBar()->insertPermanentWidget(0, statusProgressBar, 1);
    statusBar()->insertPermanentWidget(1, statusLabel, 1);
    statusBar()->insertPermanentWidget(ID_TIMELINE_BUTTONS, w);
    statusBar()->insertPermanentFixedItem("00:00:00:00", ID_TIMELINE_POS);
    statusBar()->insertPermanentWidget(ID_TIMELINE_FORMAT, m_timecodeFormat);
    statusBar()->setMaximumHeight(statusBar()->font().pointSize() * 4);

    timeline_buttons_ui.buttonVideo->setIcon(KIcon("video-mpeg"));
    timeline_buttons_ui.buttonVideo->setToolTip(i18n("Show video thumbnails"));
    timeline_buttons_ui.buttonAudio->setIcon(KIcon("audio-mpeg"));
    timeline_buttons_ui.buttonAudio->setToolTip(i18n("Show audio thumbnails"));
    timeline_buttons_ui.buttonFitZoom->setIcon(KIcon("zoom-fit-best"));
    timeline_buttons_ui.buttonFitZoom->setToolTip(i18n("Fit zoom to project"));

    setupGUI(Default, "kdenliveui.rc");

    // build effects menus
    QAction *action;
    QMenu *videoEffectsMenu = (QMenu*)(factory()->container("video_effects_menu", this));
    QStringList effects = m_videoEffects.effectNames();
    foreach(QString name, effects) {
        action = new QAction(name, this);
        action->setData(name);
        videoEffectsMenu->addAction(action);
    }
    QMenu *audioEffectsMenu = (QMenu*)(factory()->container("audio_effects_menu", this));
    effects = m_audioEffects.effectNames();
    foreach(QString name, effects) {
        action = new QAction(name, this);
        action->setData(name);
        audioEffectsMenu->addAction(action);
    }
    QMenu *customEffectsMenu = (QMenu*)(factory()->container("custom_effects_menu", this));
    effects = m_customEffects.effectNames();
    foreach(QString name, effects) {
        action = new QAction(name, this);
        action->setData(name);
        customEffectsMenu->addAction(action);
    }

    connect(videoEffectsMenu, SIGNAL(triggered(QAction *)), this, SLOT(slotAddVideoEffect(QAction *)));
    connect(audioEffectsMenu, SIGNAL(triggered(QAction *)), this, SLOT(slotAddAudioEffect(QAction *)));
    connect(customEffectsMenu, SIGNAL(triggered(QAction *)), this, SLOT(slotAddCustomEffect(QAction *)));

    m_timelineContextMenu = new QMenu(this);
    m_timelineContextClipMenu = new QMenu(this);
    m_timelineContextTransitionMenu = new QMenu(this);

    action = actionCollection()->action("delete_timeline_clip");
    m_timelineContextClipMenu->addAction(action);
    m_timelineContextClipMenu->addMenu(videoEffectsMenu);
    m_timelineContextClipMenu->addMenu(audioEffectsMenu);
    m_timelineContextClipMenu->addMenu(customEffectsMenu);

    connect(projectMonitorDock, SIGNAL(visibilityChanged(bool)), m_projectMonitor, SLOT(refreshMonitor(bool)));
    connect(clipMonitorDock, SIGNAL(visibilityChanged(bool)), m_clipMonitor, SLOT(refreshMonitor(bool)));
    //connect(m_monitorManager, SIGNAL(connectMonitors()), this, SLOT(slotConnectMonitors()));
    connect(m_monitorManager, SIGNAL(raiseClipMonitor(bool)), this, SLOT(slotRaiseMonitor(bool)));
    connect(m_effectList, SIGNAL(addEffect(QDomElement)), this, SLOT(slotAddEffect(QDomElement)));
    m_monitorManager->initMonitors(m_clipMonitor, m_projectMonitor);
    slotConnectMonitors();

    setAutoSaveSettings();
    newFile();

    activateShuttleDevice();
}

//virtual
bool MainWindow::queryClose() {
    saveOptions();
    switch (KMessageBox::warningYesNoCancel(this, i18n("Save changes to document ?"))) {
    case KMessageBox::Yes :
        // save document here. If saving fails, return false;
        return true;
    case KMessageBox::No :
        return true;
    default: // cancel
        return false;
    }
}

void MainWindow::activateShuttleDevice() {
    if (m_jogProcess) delete m_jogProcess;
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

void MainWindow::slotShuttleButton(int code) {
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

void MainWindow::slotShuttleAction(int code) {
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

void MainWindow::slotFullScreen() {
    //KToggleFullScreenAction::setFullScreen(this, actionCollection()->action("fullscreen")->isChecked());
}

void MainWindow::slotAddEffect(QDomElement effect, GenTime pos, int track) {
    if (!m_activeDocument) return;
    if (effect.isNull()) {
        kDebug() << "--- ERROR, TRYING TO APPEND NULL EFFECT";
        return;
    }
    TrackView *currentTimeLine = (TrackView *) m_timelineArea->currentWidget();
    currentTimeLine->projectView()->slotAddEffect(effect, pos, track);
}

void MainWindow::slotRaiseMonitor(bool clipMonitor) {
    if (clipMonitor) clipMonitorDock->raise();
    else projectMonitorDock->raise();
}

void MainWindow::slotSetClipDuration(int id, int duration) {
    if (!m_activeDocument) return;
    m_activeDocument->setProducerDuration(id, duration);
}

void MainWindow::slotConnectMonitors() {

    m_projectList->setRenderer(m_clipMonitor->render);
    connect(m_projectList, SIGNAL(clipSelected(const QDomElement &)), m_clipMonitor, SLOT(slotSetXml(const QDomElement &)));
    connect(m_projectList, SIGNAL(receivedClipDuration(int, int)), this, SLOT(slotSetClipDuration(int, int)));
    connect(m_projectList, SIGNAL(getFileProperties(const QDomElement &, int)), m_clipMonitor->render, SLOT(getFileProperties(const QDomElement &, int)));
    connect(m_clipMonitor->render, SIGNAL(replyGetImage(int, int, const QPixmap &, int, int)), m_projectList, SLOT(slotReplyGetImage(int, int, const QPixmap &, int, int)));
    connect(m_clipMonitor->render, SIGNAL(replyGetFileProperties(int, const QMap < QString, QString > &, const QMap < QString, QString > &)), m_projectList, SLOT(slotReplyGetFileProperties(int, const QMap < QString, QString > &, const QMap < QString, QString > &)));
}

void MainWindow::setupActions() {
    KAction* clearAction = new KAction(this);
    clearAction->setText(i18n("Clear"));
    clearAction->setIcon(KIcon("document-new"));
    clearAction->setShortcut(Qt::CTRL + Qt::Key_W);
    actionCollection()->addAction("clear", clearAction);
    /*connect(clearAction, SIGNAL(triggered(bool)),
            textArea, SLOT(clear()));*/

    KAction* profilesAction = new KAction(this);
    profilesAction->setText(i18n("Manage Profiles"));
    profilesAction->setIcon(KIcon("document-new"));
    actionCollection()->addAction("manage_profiles", profilesAction);
    connect(profilesAction, SIGNAL(triggered(bool)), this, SLOT(slotEditProfiles()));

    KAction* projectAction = new KAction(this);
    projectAction->setText(i18n("Project Settings"));
    projectAction->setIcon(KIcon("document-new"));
    actionCollection()->addAction("project_settings", projectAction);
    connect(projectAction, SIGNAL(triggered(bool)), this, SLOT(slotEditProjectSettings()));

    KAction* projectRender = new KAction(this);
    projectRender->setText(i18n("Render Project"));
    projectRender->setIcon(KIcon("document-new"));
    actionCollection()->addAction("project_render", projectRender);
    connect(projectRender, SIGNAL(triggered(bool)), this, SLOT(slotRenderProject()));

    KAction* monitorPlay = new KAction(this);
    monitorPlay->setText(i18n("Play"));
    monitorPlay->setIcon(KIcon("media-playback-start"));
    monitorPlay->setShortcut(Qt::Key_Space);
    actionCollection()->addAction("monitor_play", monitorPlay);
    connect(monitorPlay, SIGNAL(triggered(bool)), m_monitorManager, SLOT(slotPlay()));

    KAction* monitorSeekBackward = new KAction(this);
    monitorSeekBackward->setText(i18n("Rewind"));
    monitorSeekBackward->setIcon(KIcon("media-seek-backward"));
    monitorSeekBackward->setShortcut(Qt::Key_J);
    actionCollection()->addAction("monitor_seek_backward", monitorSeekBackward);
    connect(monitorSeekBackward, SIGNAL(triggered(bool)), m_monitorManager, SLOT(slotRewind()));

    KAction* monitorSeekBackwardOneFrame = new KAction(this);
    monitorSeekBackwardOneFrame->setText(i18n("Rewind 1 Frame"));
    monitorSeekBackwardOneFrame->setIcon(KIcon("media-skip-backward"));
    monitorSeekBackwardOneFrame->setShortcut(Qt::Key_Left);
    actionCollection()->addAction("monitor_seek_backward-one-frame", monitorSeekBackwardOneFrame);
    connect(monitorSeekBackwardOneFrame, SIGNAL(triggered(bool)), m_monitorManager, SLOT(slotRewindOneFrame()));

    KAction* monitorSeekForward = new KAction(this);
    monitorSeekForward->setText(i18n("Forward"));
    monitorSeekForward->setIcon(KIcon("media-seek-forward"));
    monitorSeekForward->setShortcut(Qt::Key_L);
    actionCollection()->addAction("monitor_seek_forward", monitorSeekForward);
    connect(monitorSeekForward, SIGNAL(triggered(bool)), m_monitorManager, SLOT(slotForward()));

    KAction* monitorSeekForwardOneFrame = new KAction(this);
    monitorSeekForwardOneFrame->setText(i18n("Forward 1 Frame"));
    monitorSeekForwardOneFrame->setIcon(KIcon("media-skip-forward"));
    monitorSeekForwardOneFrame->setShortcut(Qt::Key_Right);
    actionCollection()->addAction("monitor_seek_forward-one-frame", monitorSeekForwardOneFrame);
    connect(monitorSeekForwardOneFrame, SIGNAL(triggered(bool)), m_monitorManager, SLOT(slotForwardOneFrame()));

    KAction* deleteTimelineClip = new KAction(this);
    deleteTimelineClip->setText(i18n("Delete Clip"));
    deleteTimelineClip->setShortcut(Qt::Key_Delete);
    deleteTimelineClip->setIcon(KIcon("edit-delete"));
    actionCollection()->addAction("delete_timeline_clip", deleteTimelineClip);
    connect(deleteTimelineClip, SIGNAL(triggered(bool)), this, SLOT(slotDeleteTimelineClip()));

    KStandardAction::quit(kapp, SLOT(quit()),
                          actionCollection());

    KStandardAction::open(this, SLOT(openFile()),
                          actionCollection());

    m_fileOpenRecent = KStandardAction::openRecent(this, SLOT(openFile(const KUrl &)),
                       actionCollection());

    KStandardAction::save(this, SLOT(saveFile()),
                          actionCollection());

    KStandardAction::saveAs(this, SLOT(saveFileAs()),
                            actionCollection());

    KStandardAction::openNew(this, SLOT(newFile()),
                             actionCollection());

    KStandardAction::preferences(this, SLOT(slotPreferences()),
                                 actionCollection());

    KStandardAction::undo(this, SLOT(undo()),
                          actionCollection());

    KStandardAction::redo(this, SLOT(redo()),
                          actionCollection());

    KStandardAction::fullScreen(this, SLOT(slotFullScreen()), this, actionCollection());

    connect(actionCollection(), SIGNAL(actionHovered(QAction*)),
            this, SLOT(slotDisplayActionMessage(QAction*)));
    //connect(actionCollection(), SIGNAL( clearStatusText() ),
    //statusBar(), SLOT( clear() ) );

    readOptions();
}

void MainWindow::undo() {
    m_commandStack->undo();
}

void MainWindow::redo() {
    m_commandStack->redo();
}

void MainWindow::slotDisplayActionMessage(QAction *a) {
    statusBar()->showMessage(a->data().toString(), 3000);
}

void MainWindow::saveOptions() {
    KSharedConfigPtr config = KGlobal::config();
    m_fileOpenRecent->saveEntries(KConfigGroup(config, "Recent Files"));
    config->sync();
}

void MainWindow::readOptions() {
    KSharedConfigPtr config = KGlobal::config();
    m_fileOpenRecent->loadEntries(KConfigGroup(config, "Recent Files"));
}

void MainWindow::newFile() {
    QString profileName;
    if (m_timelineArea->count() == 0) profileName = KdenliveSettings::default_profile();
    else {
        ProjectSettings *w = new ProjectSettings;
        w->exec();
        profileName = w->selectedProfile();
        delete w;
    }
    MltVideoProfile prof = ProfilesDialog::getVideoProfile(profileName);
    if (prof.width == 0) prof = ProfilesDialog::getVideoProfile("dv_pal");
    KdenliveDoc *doc = new KdenliveDoc(KUrl(), prof, m_commandStack);
    TrackView *trackView = new TrackView(doc, this);
    m_timelineArea->addTab(trackView, KIcon("kdenlive"), i18n("Untitled") + " / " + prof.description);
    if (m_timelineArea->count() == 1)
        connectDocument(trackView, doc);
    else m_timelineArea->setTabBarHidden(false);
}

void MainWindow::activateDocument() {
    TrackView *currentTab = (TrackView *) m_timelineArea->currentWidget();
    KdenliveDoc *currentDoc = currentTab->document();
    connectDocument(currentTab, currentDoc);
}

void MainWindow::slotRemoveTab() {
    QWidget *w = m_timelineArea->currentWidget();
    // closing current document
    int ix = m_timelineArea->currentIndex() + 1;
    if (ix == m_timelineArea->count()) ix = 0;
    m_timelineArea->setCurrentIndex(ix);
    TrackView *tabToClose = (TrackView *) w;
    KdenliveDoc *docToClose = tabToClose->document();
    m_timelineArea->removeTab(m_timelineArea->indexOf(w));
    if (m_timelineArea->count() == 1) m_timelineArea->setTabBarHidden(true);
    delete docToClose;
    delete w;
}

void MainWindow::saveFileAs(const QString &outputFileName) {
    m_projectMonitor->saveSceneList(outputFileName, m_activeDocument->documentInfoXml());
    m_activeDocument->setUrl(KUrl(outputFileName));
    setCaption(m_activeDocument->description());
    m_timelineArea->setTabText(m_timelineArea->currentIndex(), m_activeDocument->description());
    m_timelineArea->setTabToolTip(m_timelineArea->currentIndex(), m_activeDocument->url().path());
    m_activeDocument->setModified(false);
}

void MainWindow::saveFileAs() {
    QString outputFile = KFileDialog::getSaveFileName();
    if (QFile::exists(outputFile)) {
        if (KMessageBox::questionYesNo(this, i18n("File already exists.\nDo you want to overwrite it ?")) == KMessageBox::No) return;
    }
    saveFileAs(outputFile);
}

void MainWindow::saveFile() {
    if (!m_activeDocument) return;
    if (m_activeDocument->url().isEmpty()) {
        saveFileAs();
    } else {
        saveFileAs(m_activeDocument->url().path());
    }
}

void MainWindow::openFile() { //changed
    KUrl url = KFileDialog::getOpenUrl(KUrl(), "application/vnd.kde.kdenlive;*.kdenlive");
    if (url.isEmpty()) return;
    m_fileOpenRecent->addUrl(url);
    openFile(url);
}

void MainWindow::openFile(const KUrl &url) { //new
    //TODO: get video profile from url before opening it
    MltVideoProfile prof = ProfilesDialog::getVideoProfile(KdenliveSettings::default_profile());
    if (prof.width == 0) prof = ProfilesDialog::getVideoProfile("dv_pal");
    KdenliveDoc *doc = new KdenliveDoc(url, prof, m_commandStack);
    TrackView *trackView = new TrackView(doc, this);
    m_timelineArea->setCurrentIndex(m_timelineArea->addTab(trackView, KIcon("kdenlive"), doc->description()));
    m_timelineArea->setTabToolTip(m_timelineArea->currentIndex(), doc->url().path());
    if (m_timelineArea->count() > 1) m_timelineArea->setTabBarHidden(false);
    //connectDocument(trackView, doc);
}


void MainWindow::parseProfiles() {
    //kdDebug()<<" + + YOUR MLT INSTALL WAS FOUND IN: "<< MLT_PREFIX <<endl;
    if (KdenliveSettings::mltpath().isEmpty()) {
        KdenliveSettings::setMltpath(QString(MLT_PREFIX) + QString("/share/mlt/profiles/"));
    }
    if (KdenliveSettings::rendererpath().isEmpty()) {
        KdenliveSettings::setRendererpath(KStandardDirs::findExe("inigo"));
    }
    QStringList profilesFilter;
    profilesFilter << "*";
    QStringList profilesList = QDir(KdenliveSettings::mltpath()).entryList(profilesFilter, QDir::Files);

    if (profilesList.isEmpty()) {
        // Cannot find MLT path, try finding inigo
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
            getUrl->exec();
            KUrl mltPath = getUrl->selectedUrl();
            delete getUrl;
            if (mltPath.isEmpty()) exit(1);
            KdenliveSettings::setMltpath(mltPath.path());
            QStringList profilesList = QDir(KdenliveSettings::mltpath()).entryList(profilesFilter, QDir::Files);
        }
    }

    if (KdenliveSettings::rendererpath().isEmpty()) {
        // Cannot find the MLT inigo renderer, ask for location
        KUrlRequesterDialog *getUrl = new KUrlRequesterDialog(KdenliveSettings::mltpath(), i18n("Cannot find the inigo program required for rendering (part of Mlt)"), this);
        getUrl->exec();
        KUrl rendererPath = getUrl->selectedUrl();
        delete getUrl;
        if (rendererPath.isEmpty()) exit(1);
        KdenliveSettings::setRendererpath(rendererPath.path());
    }

    kDebug() << "RESULTING MLT PATH: " << KdenliveSettings::mltpath();

    // Parse MLT profiles to build a list of available video formats
    if (profilesList.isEmpty()) parseProfiles();
}


void MainWindow::slotEditProfiles() {
    ProfilesDialog *w = new ProfilesDialog;
    w->exec();
    delete w;
}

void MainWindow::slotEditProjectSettings() {
    ProjectSettings *w = new ProjectSettings;
    if (w->exec() == QDialog::Accepted) {
        QString profile = w->selectedProfile();
        m_activeDocument->setProfilePath(profile);
        m_monitorManager->resetProfiles(profile);
        setCaption(m_activeDocument->description());
        KdenliveSettings::setCurrent_profile(m_activeDocument->profilePath());
        if (m_renderWidget) m_renderWidget->setDocumentStandard(m_activeDocument->getDocumentStandard());
        m_monitorManager->setTimecode(m_activeDocument->timecode());
        m_timelineArea->setTabText(m_timelineArea->currentIndex(), m_activeDocument->description());

        // We need to desactivate & reactivate monitors to get a refresh
        m_monitorManager->switchMonitors();
    }
    delete w;
}

void MainWindow::slotRenderProject() {
    if (!m_renderWidget) {
        m_renderWidget = new RenderWidget(this);
        connect(m_renderWidget, SIGNAL(doRender(const QString&, const QString&, const QStringList &, bool, bool)), this, SLOT(slotDoRender(const QString&, const QString&, const QStringList &, bool, bool)));
    }
    /*TrackView *currentTab = (TrackView *) m_timelineArea->currentWidget();
    if (currentTab) m_renderWidget->setTimeline(currentTab);
    m_renderWidget->setDocument(m_activeDocument);*/
    m_renderWidget->show();
}

void MainWindow::slotDoRender(const QString &dest, const QString &render, const QStringList &avformat_args, bool zoneOnly, bool playAfter) {
    if (dest.isEmpty()) return;
    int in;
    int out;
    TrackView *currentTab = (TrackView *) m_timelineArea->currentWidget();
    if (currentTab && zoneOnly) {
        in = currentTab->inPoint();
        out = currentTab->outPoint();
    }
    KTemporaryFile temp;
    temp.setAutoRemove(false);
    temp.setSuffix(".westley");
    if (temp.open()) {
        m_projectMonitor->saveSceneList(temp.fileName());
        QStringList args;
        args << "-erase";
        if (zoneOnly) args << "in=" + QString::number(in) << "out=" + QString::number(out);
        QString videoPlayer = "-";
        if (playAfter) videoPlayer = "kmplayer";
        args << "inigo" << m_activeDocument->profilePath() << render << videoPlayer << temp.fileName() << dest << avformat_args;
        QProcess::startDetached("kdenlive_render", args);
    }
}

void MainWindow::slotUpdateMousePosition(int pos) {
    if (m_activeDocument)
        switch (m_timecodeFormat->currentIndex()) {
        case 0:
            statusBar()->changeItem(m_activeDocument->timecode().getTimecodeFromFrames(pos), ID_TIMELINE_POS);
            break;
        default:
            statusBar()->changeItem(QString::number(pos), ID_TIMELINE_POS);
        }
}

void MainWindow::slotUpdateDocumentState(bool modified) {
    setCaption(m_activeDocument->description(), modified);
    if (modified) {
        m_timelineArea->setTabTextColor(m_timelineArea->currentIndex(), palette().color(QPalette::Link));
        m_timelineArea->setTabIcon(m_timelineArea->currentIndex(), KIcon("document-save"));
    } else {
        m_timelineArea->setTabTextColor(m_timelineArea->currentIndex(), palette().color(QPalette::Text));
        m_timelineArea->setTabIcon(m_timelineArea->currentIndex(), KIcon("kdenlive"));
    }
}

void MainWindow::connectDocument(TrackView *trackView, KdenliveDoc *doc) { //changed
    //m_projectMonitor->stop();
    kDebug() << "///////////////////   CONNECTING DOC TO PROJECT VIEW ////////////////";
    if (m_activeDocument) {
        if (m_activeDocument == doc) return;
        m_activeDocument->backupMltPlaylist();
        if (m_activeTimeline) {
            disconnect(m_projectMonitor, SIGNAL(renderPosition(int)), m_activeTimeline, SLOT(moveCursorPos(int)));
            disconnect(m_projectMonitor, SIGNAL(durationChanged(int)), m_activeTimeline, SLOT(setDuration(int)));
            disconnect(m_activeDocument, SIGNAL(addProjectClip(DocClipBase *)), m_projectList, SLOT(slotAddClip(DocClipBase *)));
            disconnect(m_activeDocument, SIGNAL(signalDeleteProjectClip(int)), m_projectList, SLOT(slotDeleteClip(int)));
            disconnect(m_activeDocument, SIGNAL(updateClipDisplay(int)), m_projectList, SLOT(slotUpdateClip(int)));
            disconnect(m_activeDocument, SIGNAL(deletTimelineClip(int)), m_activeTimeline, SLOT(slotDeleteClip(int)));
            disconnect(m_activeDocument, SIGNAL(thumbsProgress(KUrl, int)), this, SLOT(slotGotProgressInfo(KUrl, int)));
            disconnect(m_activeTimeline, SIGNAL(clipItemSelected(ClipItem*)), effectStack, SLOT(slotClipItemSelected(ClipItem*)));
            disconnect(m_activeTimeline, SIGNAL(transitionItemSelected(Transition*)), transitionConfig, SLOT(slotTransitionItemSelected(Transition*)));
            disconnect(timeline_buttons_ui.zoom_slider, SIGNAL(valueChanged(int)), m_activeTimeline, SLOT(slotChangeZoom(int)));
            disconnect(m_activeDocument, SIGNAL(docModified(bool)), this, SLOT(slotUpdateDocumentState(bool)));
            disconnect(effectStack, SIGNAL(updateClipEffect(ClipItem*, QDomElement, QDomElement)), m_activeTimeline->projectView(), SLOT(slotUpdateClipEffect(ClipItem*, QDomElement, QDomElement)));
            disconnect(effectStack, SIGNAL(removeEffect(ClipItem*, QDomElement)), m_activeTimeline->projectView(), SLOT(slotDeleteEffect(ClipItem*, QDomElement)));
            disconnect(effectStack, SIGNAL(changeEffectState(ClipItem*, QDomElement, bool)), m_activeTimeline->projectView(), SLOT(slotChangeEffectState(ClipItem*, QDomElement, bool)));
            disconnect(effectStack, SIGNAL(refreshEffectStack(ClipItem*)), m_activeTimeline->projectView(), SLOT(slotRefreshEffects(ClipItem*)));
            disconnect(m_activeTimeline->projectView(), SIGNAL(activateDocumentMonitor()), m_projectMonitor, SLOT(activateMonitor()));
        }
        m_activeDocument->setRenderer(NULL);
        disconnect(m_projectList, SIGNAL(clipSelected(const QDomElement &)), m_clipMonitor, SLOT(slotSetXml(const QDomElement &)));
        m_clipMonitor->stop();
    }
    m_monitorManager->resetProfiles(doc->profilePath());
    m_projectList->setDocument(doc);
    connect(m_projectList, SIGNAL(clipSelected(const QDomElement &)), m_clipMonitor, SLOT(slotSetXml(const QDomElement &)));
    connect(trackView, SIGNAL(cursorMoved()), m_projectMonitor, SLOT(activateMonitor()));
    connect(trackView, SIGNAL(mousePosition(int)), this, SLOT(slotUpdateMousePosition(int)));
    connect(m_projectMonitor, SIGNAL(renderPosition(int)), trackView, SLOT(moveCursorPos(int)));
    connect(m_projectMonitor, SIGNAL(durationChanged(int)), trackView, SLOT(setDuration(int)));
    connect(doc, SIGNAL(addProjectClip(DocClipBase *)), m_projectList, SLOT(slotAddClip(DocClipBase *)));
    connect(doc, SIGNAL(signalDeleteProjectClip(int)), m_projectList, SLOT(slotDeleteClip(int)));
    connect(doc, SIGNAL(updateClipDisplay(int)), m_projectList, SLOT(slotUpdateClip(int)));
    connect(doc, SIGNAL(deletTimelineClip(int)), trackView, SLOT(slotDeleteClip(int)));
    connect(doc, SIGNAL(thumbsProgress(KUrl, int)), this, SLOT(slotGotProgressInfo(KUrl, int)));
    connect(doc, SIGNAL(docModified(bool)), this, SLOT(slotUpdateDocumentState(bool)));

    connect(trackView, SIGNAL(clipItemSelected(ClipItem*)), effectStack, SLOT(slotClipItemSelected(ClipItem*)));
    connect(trackView, SIGNAL(transitionItemSelected(Transition*)), transitionConfig, SLOT(slotTransitionItemSelected(Transition*)));
    timeline_buttons_ui.zoom_slider->setValue(trackView->currentZoom());
    connect(timeline_buttons_ui.zoom_slider, SIGNAL(valueChanged(int)), trackView, SLOT(slotChangeZoom(int)));
    connect(trackView->projectView(), SIGNAL(zoomIn()), this, SLOT(slotZoomIn()));
    connect(trackView->projectView(), SIGNAL(zoomOut()), this, SLOT(slotZoomOut()));
    connect(effectStack, SIGNAL(updateClipEffect(ClipItem*, QDomElement, QDomElement)), trackView->projectView(), SLOT(slotUpdateClipEffect(ClipItem*, QDomElement, QDomElement)));
    connect(effectStack, SIGNAL(removeEffect(ClipItem*, QDomElement)), trackView->projectView(), SLOT(slotDeleteEffect(ClipItem*, QDomElement)));
    connect(effectStack, SIGNAL(changeEffectState(ClipItem*, QDomElement, bool)), trackView->projectView(), SLOT(slotChangeEffectState(ClipItem*, QDomElement, bool)));
    connect(effectStack, SIGNAL(refreshEffectStack(ClipItem*)), trackView->projectView(), SLOT(slotRefreshEffects(ClipItem*)));
    connect(trackView->projectView(), SIGNAL(activateDocumentMonitor()), m_projectMonitor, SLOT(activateMonitor()));
    trackView->projectView()->setContextMenu(m_timelineContextMenu, m_timelineContextClipMenu, m_timelineContextTransitionMenu);
    m_activeTimeline = trackView;
    KdenliveSettings::setCurrent_profile(doc->profilePath());
    if (m_renderWidget) m_renderWidget->setDocumentStandard(doc->getDocumentStandard());
    m_monitorManager->setTimecode(doc->timecode());
    doc->setRenderer(m_projectMonitor->render);
    m_commandStack->setActiveStack(doc->commandStack());
    if (m_commandStack->isClean()) kDebug() << "////////////  UNDO STACK IS CLEAN";
    else  kDebug() << "////////////  UNDO STACK IS NOT CLEAN*******************";

    m_overView->setScene(trackView->projectScene());
    //m_overView->scale(m_overView->width() / trackView->duration(), m_overView->height() / (50 * trackView->tracksNumber()));
    //m_overView->fitInView(m_overView->itemAt(0, 50), Qt::KeepAspectRatio);

    setCaption(doc->description());
    m_activeDocument = doc;
}

void MainWindow::slotPreferences() {
    //An instance of your dialog could be already created and could be
    // cached, in which case you want to display the cached dialog
    // instead of creating another one
    if (KConfigDialog::showDialog("settings"))
        return;

    // KConfigDialog didn't find an instance of this dialog, so lets
    // create it :
    KdenliveSettingsDialog* dialog = new KdenliveSettingsDialog(this);
    connect(dialog, SIGNAL(settingsChanged(const QString&)), this, SLOT(updateConfiguration()));
    dialog->show();
}

void MainWindow::updateConfiguration() {
    //TODO: we should apply settings to all projects, not only the current one
    TrackView *currentTab = (TrackView *) m_timelineArea->currentWidget();
    if (currentTab) {
        currentTab->refresh();
        currentTab->projectView()->checkAutoScroll();
        currentTab->projectView()->checkTrackHeight();
        if (m_activeDocument) m_activeDocument->clipManager()->checkAudioThumbs();
    }
    timeline_buttons_ui.buttonAudio->setDown(KdenliveSettings::audiothumbnails());
    timeline_buttons_ui.buttonVideo->setDown(KdenliveSettings::videothumbnails());
    activateShuttleDevice();

}

void MainWindow::slotSwitchVideoThumbs() {
    KdenliveSettings::setVideothumbnails(!KdenliveSettings::videothumbnails());
    TrackView *currentTab = (TrackView *) m_timelineArea->currentWidget();
    if (currentTab) {
        currentTab->refresh();
    }
    timeline_buttons_ui.buttonVideo->setDown(KdenliveSettings::videothumbnails());
}

void MainWindow::slotSwitchAudioThumbs() {
    KdenliveSettings::setAudiothumbnails(!KdenliveSettings::audiothumbnails());
    TrackView *currentTab = (TrackView *) m_timelineArea->currentWidget();
    if (currentTab) {
        currentTab->refresh();
        currentTab->projectView()->checkAutoScroll();
        if (m_activeDocument) m_activeDocument->clipManager()->checkAudioThumbs();
    }
    timeline_buttons_ui.buttonAudio->setDown(KdenliveSettings::audiothumbnails());
}

void MainWindow::slotDeleteTimelineClip() {
    TrackView *currentTab = (TrackView *) m_timelineArea->currentWidget();
    if (currentTab) {
        currentTab->projectView()->deleteSelectedClips();
    }
}

void MainWindow::slotAddProjectClip(KUrl url) {
    if (m_activeDocument)
        m_activeDocument->slotAddClipFile(url, QString());
}

void MainWindow::slotAddVideoEffect(QAction *result) {
    if (!result) return;
    QDomElement effect = m_videoEffects.getEffectByName(result->data().toString());
    slotAddEffect(effect);
}

void MainWindow::slotAddAudioEffect(QAction *result) {
    if (!result) return;
    QDomElement effect = m_audioEffects.getEffectByName(result->data().toString());
    slotAddEffect(effect);
}

void MainWindow::slotAddCustomEffect(QAction *result) {
    if (!result) return;
    QDomElement effect = m_customEffects.getEffectByName(result->data().toString());
    slotAddEffect(effect);
}

void MainWindow::slotZoomIn() {
    timeline_buttons_ui.zoom_slider->setValue(timeline_buttons_ui.zoom_slider->value() - 1);
}

void MainWindow::slotZoomOut() {
    timeline_buttons_ui.zoom_slider->setValue(timeline_buttons_ui.zoom_slider->value() + 1);
}

void MainWindow::slotFitZoom() {
    TrackView *currentTab = (TrackView *) m_timelineArea->currentWidget();
    if (currentTab) {
        timeline_buttons_ui.zoom_slider->setValue(currentTab->fitZoom());
    }
}

void MainWindow::slotGotProgressInfo(KUrl url, int progress) {
    statusProgressBar->setValue(progress);
    if (progress > 0) {
        statusLabel->setText(tr("Creating Audio Thumbs"));
        statusProgressBar->setVisible(true);
    } else {
        statusLabel->setText("");
        statusProgressBar->setVisible(false);
    }
}

void MainWindow::customEvent(QEvent* e) {
    if (e->type() == QEvent::User) {
        // The timeline playing position changed...
        kDebug() << "RECIEVED JOG EVEMNT!!!";
    }
}


#include "mainwindow.moc"
