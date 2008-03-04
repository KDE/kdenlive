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

#include <mlt++/Mlt.h>

#include "mainwindow.h"
#include "kdenlivesettings.h"
#include "kdenlivesettingsdialog.h"
#include "initeffects.h"
#include "profilesdialog.h"
#include "projectsettings.h"
#include "events.h"

#define ID_STATUS_MSG 1
#define ID_EDITMODE_MSG 2
#define ID_TIMELINE_MSG 3
#define ID_TIMELINE_POS 4
#define ID_TIMELINE_FORMAT 5

MainWindow::MainWindow(QWidget *parent)
        : KXmlGuiWindow(parent),
        fileName(QString()), m_activeDocument(NULL), m_commandStack(NULL) {
    parseProfiles();
    m_timelineArea = new KTabWidget(this);
    m_timelineArea->setHoverCloseButton(true);
    m_timelineArea->setTabReorderingEnabled(true);
    m_timelineArea->setTabBarHidden(true);
    connect(m_timelineArea, SIGNAL(currentChanged(int)), this, SLOT(activateDocument()));

    initEffects::parseEffectFiles(&m_audioEffects, &m_videoEffects);
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
    transitionConfig = new KListWidget(this);
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

    undoViewDock = new QDockWidget(i18n("Undo History"), this);
    undoViewDock->setObjectName("undo_history");
    m_undoView = new QUndoView(this);
    undoViewDock->setWidget(m_undoView);
    m_undoView->setStack(m_commandStack);
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

    statusBar()->insertPermanentWidget(0, statusProgressBar, 1);
    statusBar()->insertPermanentWidget(1, statusLabel, 1);
    statusBar()->insertPermanentFixedItem("00:00:00:00", ID_TIMELINE_POS);
    statusBar()->insertPermanentWidget(ID_TIMELINE_FORMAT, m_timecodeFormat);

    setupGUI(Default, "kdenliveui.rc");

    connect(projectMonitorDock, SIGNAL(visibilityChanged(bool)), m_projectMonitor, SLOT(refreshMonitor(bool)));
    connect(clipMonitorDock, SIGNAL(visibilityChanged(bool)), m_clipMonitor, SLOT(refreshMonitor(bool)));
    connect(m_monitorManager, SIGNAL(connectMonitors()), this, SLOT(slotConnectMonitors()));
    connect(m_monitorManager, SIGNAL(raiseClipMonitor(bool)), this, SLOT(slotRaiseMonitor(bool)));
    connect(m_effectList, SIGNAL(addEffect(QDomElement)), this, SLOT(slotAddEffect(QDomElement)));
    m_monitorManager->initMonitors(m_clipMonitor, m_projectMonitor);

    setAutoSaveSettings();
    newFile();
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

    KAction* monitorPlay = new KAction(this);
    monitorPlay->setText(i18n("Play"));
    monitorPlay->setIcon(KIcon("media-playback-start"));
    monitorPlay->setShortcut(Qt::Key_Space);
    actionCollection()->addAction("monitor_play", monitorPlay);
    connect(monitorPlay, SIGNAL(triggered(bool)), m_monitorManager, SLOT(slotPlay()));

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

    /*KStandardAction::undo(this, SLOT(undo()),
                          actionCollection());

    KStandardAction::redo(this, SLOT(redo()),
                          actionCollection());*/

    connect(actionCollection(), SIGNAL(actionHighlighted(QAction*)),
            this, SLOT(slotDisplayActionMessage(QAction*)));
    //connect(actionCollection(), SIGNAL( clearStatusText() ),
    //statusBar(), SLOT( clear() ) );

    readOptions();

    /*m_redo = m_commandStack->createRedoAction(actionCollection());
    m_undo = m_commandStack->createUndoAction(actionCollection());*/
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
    KdenliveDoc *doc = new KdenliveDoc(KUrl(), 25, 720, 576);
    TrackView *trackView = new TrackView(doc);
    m_timelineArea->addTab(trackView, i18n("Untitled") + " / " + ProfilesDialog::getProfileDescription(KdenliveSettings::default_profile()));
    if (m_timelineArea->count() == 1)
        connectDocument(trackView, doc);
    else m_timelineArea->setTabBarHidden(false);
}

void MainWindow::activateDocument() {
    TrackView *currentTab = (TrackView *) m_timelineArea->currentWidget();
    KdenliveDoc *currentDoc = currentTab->document();
    connectDocument(currentTab, currentDoc);
}

void MainWindow::saveFileAs(const QString &outputFileName) {
    KSaveFile file(outputFileName);
    file.open();

    QByteArray outputByteArray;
    //outputByteArray.append(textArea->toPlainText());
    file.write(outputByteArray);
    file.finalize();
    file.close();

    fileName = outputFileName;
}

void MainWindow::saveFileAs() {
    saveFileAs(KFileDialog::getSaveFileName());
}

void MainWindow::saveFile() {
    if (!fileName.isEmpty()) {
        saveFileAs(fileName);
    } else {
        saveFileAs();
    }
}

void MainWindow::openFile() { //changed
    KUrl url = KFileDialog::getOpenUrl(KUrl(), "application/vnd.kde.kdenlive;*.kdenlive");
    if (url.isEmpty()) return;
    m_fileOpenRecent->addUrl(url);
    openFile(url);
}

void MainWindow::openFile(const KUrl &url) { //new
    KdenliveDoc *doc = new KdenliveDoc(url, 25, 720, 576);
    TrackView *trackView = new TrackView(doc);
    m_timelineArea->setCurrentIndex(m_timelineArea->addTab(trackView, QIcon(), doc->documentName()));
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
    w->exec();
    delete w;
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

void MainWindow::connectDocument(TrackView *trackView, KdenliveDoc *doc) { //changed
    //m_projectMonitor->stop();
    kDebug() << "///////////////////   CONNECTING DOC TO PROJECT VIEW ////////////////";
    if (m_activeDocument) {
        if (m_activeDocument == doc) return;
        m_activeDocument->setProducers(m_projectList->producersList());
        m_activeDocument->setRenderer(NULL);
    }
    connect(trackView, SIGNAL(cursorMoved()), m_projectMonitor, SLOT(activateMonitor()));
    connect(trackView, SIGNAL(mousePosition(int)), this, SLOT(slotUpdateMousePosition(int)));
    connect(m_projectMonitor, SIGNAL(renderPosition(int)), trackView, SLOT(moveCursorPos(int)));
    connect(m_projectMonitor, SIGNAL(durationChanged(int)), trackView->projectView(), SLOT(setDuration(int)));
    connect(doc, SIGNAL(addProjectClip(DocClipBase *)), m_projectList, SLOT(slotAddClip(DocClipBase *)));
    connect(doc, SIGNAL(signalDeleteProjectClip(int)), m_projectList, SLOT(slotDeleteClip(int)));
    connect(doc, SIGNAL(updateClipDisplay(int)), m_projectList, SLOT(slotUpdateClip(int)));
    connect(doc, SIGNAL(deletTimelineClip(int)), trackView, SLOT(slotDeleteClip(int)));
    connect(doc, SIGNAL(thumbsProgress(KUrl, int)), this, SLOT(slotGotProgressInfo(KUrl, int)));

    connect(trackView, SIGNAL(clipItemSelected(ClipItem*)), effectStack, SLOT(slotClipItemSelected(ClipItem*)));
    connect(effectStack, SIGNAL(updateClipEffect(ClipItem*, QDomElement, QDomElement)), trackView->projectView(), SLOT(slotUpdateClipEffect(ClipItem*, QDomElement, QDomElement)));
    connect(effectStack, SIGNAL(removeEffect(ClipItem*, QDomElement)), trackView->projectView(), SLOT(slotDeleteEffect(ClipItem*, QDomElement)));
    connect(effectStack, SIGNAL(changeEffectState(ClipItem*, QDomElement, bool)), trackView->projectView(), SLOT(slotChangeEffectState(ClipItem*, QDomElement, bool)));
    connect(effectStack, SIGNAL(refreshEffectStack(ClipItem*)), trackView->projectView(), SLOT(slotRefreshEffects(ClipItem*)));

    m_projectList->setDocument(doc);
    m_monitorManager->setTimecode(doc->timecode());
    doc->setRenderer(m_projectMonitor->render);
    //m_undoView->setStack(0);
    m_commandStack = doc->commandStack();

    m_overView->setScene(trackView->projectScene());
    m_overView->scale(m_overView->width() / trackView->duration(), m_overView->height() / (50 * trackView->tracksNumber()));
    //m_overView->fitInView(m_overView->itemAt(0, 50), Qt::KeepAspectRatio);
    QAction *redo = m_commandStack->createRedoAction(actionCollection());
    QAction *undo = m_commandStack->createUndoAction(actionCollection());

    QWidget* w = factory()->container("mainToolBar", this);
    if (w) {
        if (actionCollection()->action("undo"))
            delete actionCollection()->action("undo");
        if (actionCollection()->action("redo"))
            delete actionCollection()->action("redo");

        actionCollection()->addAction("undo", undo);
        actionCollection()->addAction("redo", redo);
        w->addAction(undo);
        w->addAction(redo);
    }
    m_undoView->setStack(doc->commandStack());
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
        if (m_activeDocument) m_activeDocument->clipManager()->checkAudioThumbs();
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

#include "mainwindow.moc"
