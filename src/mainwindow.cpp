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

#include <stdlib.h>

#include <QTextStream>
#include <QTimer>
#include <QAction>
#include <QtTest>
#include <QtCore>
#include <QKeyEvent>

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
#include <locale.h>
#include <ktogglefullscreenaction.h>
#include <KFileItem>
#include <KNotification>
#include <KNotifyConfigWidget>

#include <mlt++/Mlt.h>

#include "mainwindow.h"
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
#include "jogshuttle.h"
#include "clipproperties.h"
#include "wizard.h"
#include "editclipcommand.h"
#include "titlewidget.h"

static const int ID_STATUS_MSG = 1;
static const int ID_EDITMODE_MSG = 2;
static const int ID_TIMELINE_MSG = 3;
static const int ID_TIMELINE_BUTTONS = 5;
static const int ID_TIMELINE_POS = 6;
static const int ID_TIMELINE_FORMAT = 7;

namespace Mlt {
class Producer;
};

EffectsList MainWindow::videoEffects;
EffectsList MainWindow::audioEffects;
EffectsList MainWindow::customEffects;
EffectsList MainWindow::transitions;

MainWindow::MainWindow(QWidget *parent)
        : KXmlGuiWindow(parent),
        m_activeDocument(NULL), m_activeTimeline(NULL), m_renderWidget(NULL), m_jogProcess(NULL), m_findActivated(false), m_initialized(false) {
    setlocale(LC_NUMERIC, "POSIX");
    setFont(KGlobalSettings::toolBarFont());
    parseProfiles();
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

    initEffects::parseEffectFiles();
    //initEffects::parseCustomEffectsFile();

    m_monitorManager = new MonitorManager();

    projectListDock = new QDockWidget(i18n("Project Tree"), this);
    projectListDock->setObjectName("project_tree");
    m_projectList = new ProjectList(this);
    projectListDock->setWidget(m_projectList);
    addDockWidget(Qt::TopDockWidgetArea, projectListDock);

    effectListDock = new QDockWidget(i18n("Effect List"), this);
    effectListDock->setObjectName("effect_list");
    m_effectList = new EffectsListView();

    //m_effectList = new KListWidget(this);
    effectListDock->setWidget(m_effectList);
    addDockWidget(Qt::TopDockWidgetArea, effectListDock);

    effectStackDock = new QDockWidget(i18n("Effect Stack"), this);
    effectStackDock->setObjectName("effect_stack");
    effectStack = new EffectStackView(this);
    effectStackDock->setWidget(effectStack);
    addDockWidget(Qt::TopDockWidgetArea, effectStackDock);

    transitionConfigDock = new QDockWidget(i18n("Transition"), this);
    transitionConfigDock->setObjectName("transition");
    transitionConfig = new TransitionSettings(this);
    transitionConfigDock->setWidget(transitionConfig);
    addDockWidget(Qt::TopDockWidgetArea, transitionConfigDock);

    KdenliveSettings::setCurrent_profile(KdenliveSettings::default_profile());
    m_fileOpenRecent = KStandardAction::openRecent(this, SLOT(openFile(const KUrl &)),
                       actionCollection());
    readOptions();

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
    connect(m_recMonitor, SIGNAL(showConfigDialog(int, int)), this, SLOT(slotPreferences(int, int)));

    undoViewDock = new QDockWidget(i18n("Undo History"), this);
    undoViewDock->setObjectName("undo_history");
    m_undoView = new QUndoView(this);
    m_undoView->setCleanIcon(KIcon("edit-clear"));
    m_undoView->setEmptyLabel(i18n("Clean"));
    undoViewDock->setWidget(m_undoView);
    m_undoView->setGroup(m_commandStack);
    addDockWidget(Qt::TopDockWidgetArea, undoViewDock);

    //overviewDock = new QDockWidget(i18n("Project Overview"), this);
    //overviewDock->setObjectName("project_overview");
    //m_overView = new CustomTrackView(NULL, NULL, this);
    //overviewDock->setWidget(m_overView);
    //addDockWidget(Qt::TopDockWidgetArea, overviewDock);

    setupActions();
    //tabifyDockWidget(projectListDock, effectListDock);
    tabifyDockWidget(projectListDock, effectStackDock);
    tabifyDockWidget(projectListDock, transitionConfigDock);
    //tabifyDockWidget(projectListDock, undoViewDock);


    tabifyDockWidget(clipMonitorDock, projectMonitorDock);
    tabifyDockWidget(clipMonitorDock, recMonitorDock);
    setCentralWidget(m_timelineArea);

    setupGUI(Default, NULL /*"kdenliveui.rc"*/);
    kDebug() << factory() << " " << factory()->container("video_effects_menu", this);

    // build effects menus
    QAction *action;
    QMenu *videoEffectsMenu = static_cast<QMenu*>(factory()->container("video_effects_menu", this));
    QStringList effects = videoEffects.effectNames();
    foreach(const QString &name, effects) {
        action = new QAction(name, this);
        action->setData(name);
        videoEffectsMenu->addAction(action);
    }
    QMenu *audioEffectsMenu = static_cast<QMenu*>(factory()->container("audio_effects_menu", this));
    effects = audioEffects.effectNames();
    foreach(const QString &name, effects) {
        action = new QAction(name, this);
        action->setData(name);
        audioEffectsMenu->addAction(action);
    }
    m_customEffectsMenu = static_cast<QMenu*>(factory()->container("custom_effects_menu", this));
    effects = customEffects.effectNames();
    if (effects.isEmpty()) m_customEffectsMenu->setEnabled(false);
    else m_customEffectsMenu->setEnabled(true);

    foreach(const QString &name, effects) {
        action = new QAction(name, this);
        action->setData(name);
        m_customEffectsMenu->addAction(action);
    }

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
    effects = transitions.effectNames();
    foreach(const QString &name, effects) {
        action = new QAction(name, this);
        action->setData(name);
        transitionsMenu->addAction(action);
    }
    connect(transitionsMenu, SIGNAL(triggered(QAction *)), this, SLOT(slotAddTransition(QAction *)));

    m_timelineContextMenu->addAction(actionCollection()->action(KStandardAction::name(KStandardAction::Paste)));

    m_timelineContextClipMenu->addAction(actionCollection()->action("delete_timeline_clip"));
    m_timelineContextClipMenu->addAction(actionCollection()->action("change_clip_speed"));
    m_timelineContextClipMenu->addAction(actionCollection()->action("cut_timeline_clip"));
    m_timelineContextClipMenu->addAction(actionCollection()->action(KStandardAction::name(KStandardAction::Copy)));
    m_timelineContextClipMenu->addAction(actionCollection()->action("paste_effects"));

    QMenu *markersMenu = (QMenu*)(factory()->container("marker_menu", this));
    m_timelineContextClipMenu->addMenu(markersMenu);
    m_timelineContextClipMenu->addMenu(transitionsMenu);
    m_timelineContextClipMenu->addMenu(videoEffectsMenu);
    m_timelineContextClipMenu->addMenu(audioEffectsMenu);
    //TODO: re-enable custom effects menu when it is implemented
    m_timelineContextClipMenu->addMenu(m_customEffectsMenu);

    m_timelineContextTransitionMenu->addAction(actionCollection()->action("delete_timeline_clip"));
    m_timelineContextTransitionMenu->addAction(actionCollection()->action(KStandardAction::name(KStandardAction::Copy)));

    connect(projectMonitorDock, SIGNAL(visibilityChanged(bool)), m_projectMonitor, SLOT(refreshMonitor(bool)));
    connect(clipMonitorDock, SIGNAL(visibilityChanged(bool)), m_clipMonitor, SLOT(refreshMonitor(bool)));
    //connect(m_monitorManager, SIGNAL(connectMonitors()), this, SLOT(slotConnectMonitors()));
    connect(m_monitorManager, SIGNAL(raiseClipMonitor(bool)), this, SLOT(slotRaiseMonitor(bool)));
    connect(m_effectList, SIGNAL(addEffect(QDomElement)), this, SLOT(slotAddEffect(QDomElement)));
    connect(m_effectList, SIGNAL(reloadEffects()), this, SLOT(slotReloadEffects()));

    m_monitorManager->initMonitors(m_clipMonitor, m_projectMonitor);
    slotConnectMonitors();

    setAutoSaveSettings();

    if (KdenliveSettings::openlastproject()) {
        openLastFile();
    } else {
        /*QList<KAutoSaveFile *> staleFiles = KAutoSaveFile::allStaleFiles();
        if (!staleFiles.isEmpty()) {
            if (KMessageBox::questionYesNo(this, i18n("Auto-saved files exist. Do you want to recover them now?"), i18n("File Recovery"), KGuiItem(i18n("Recover")), KGuiItem(i18n("Don't recover"))) == KMessageBox::Yes) {
         recoverFiles(staleFiles);
            }
            else newFile();
        }
        else*/
        newFile();
    }

    activateShuttleDevice();
    projectListDock->raise();
}

void MainWindow::queryQuit() {
    kDebug() << "----- SAVING CONFUIG";
    if (queryClose()) kapp->quit();
}

//virtual
bool MainWindow::queryClose() {
    saveOptions();
    if (m_activeDocument && m_activeDocument->isModified()) {
        switch (KMessageBox::warningYesNoCancel(this, i18n("Save changes to document ?"))) {
        case KMessageBox::Yes :
            // save document here. If saving fails, return false;
            saveFile();
            return true;
        case KMessageBox::No :
            return true;
        default: // cancel
            return false;
        }
    }
    return true;
}

void MainWindow::saveProperties(KConfig*) {
    // save properties here,used by session management
    saveFile();
}


void MainWindow::readProperties(KConfig *config) {
    // read properties here,used by session management
    QString Lastproject = config->group("Recent Files").readPathEntry("File1", QString());
    openFile(KUrl(Lastproject));
}

void MainWindow::slotReloadEffects() {
    initEffects::parseCustomEffectsFile();
    m_customEffectsMenu->clear();
    const QStringList effects = customEffects.effectNames();
    QAction *action;
    if (effects.isEmpty()) m_customEffectsMenu->setEnabled(false);
    else m_customEffectsMenu->setEnabled(true);

    foreach(const QString &name, effects) {
        action = new QAction(name, this);
        action->setData(name);
        m_customEffectsMenu->addAction(action);
    }
    m_effectList->reloadEffectList();
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

void MainWindow::configureNotifications() {
    KNotifyConfigWidget::configure(this);
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

void MainWindow::slotSetClipDuration(const QString &id, int duration) {
    if (!m_activeDocument) return;
    m_activeDocument->setProducerDuration(id, duration);
}

void MainWindow::slotConnectMonitors() {

    m_projectList->setRenderer(m_clipMonitor->render);
    connect(m_projectList, SIGNAL(receivedClipDuration(const QString &, int)), this, SLOT(slotSetClipDuration(const QString &, int)));
    connect(m_projectList, SIGNAL(showClipProperties(DocClipBase *)), this, SLOT(slotShowClipProperties(DocClipBase *)));
    connect(m_projectList, SIGNAL(getFileProperties(const QDomElement &, const QString &)), m_clipMonitor->render, SLOT(getFileProperties(const QDomElement &, const QString &)));
    connect(m_clipMonitor->render, SIGNAL(replyGetImage(const QString &, int, const QPixmap &, int, int)), m_projectList, SLOT(slotReplyGetImage(const QString &, int, const QPixmap &, int, int)));
    connect(m_clipMonitor->render, SIGNAL(replyGetFileProperties(const QString &, Mlt::Producer*, const QMap < QString, QString > &, const QMap < QString, QString > &)), m_projectList, SLOT(slotReplyGetFileProperties(const QString &, Mlt::Producer*, const QMap < QString, QString > &, const QMap < QString, QString > &)));

    connect(m_clipMonitor->render, SIGNAL(removeInvalidClip(const QString &)), m_projectList, SLOT(slotRemoveInvalidClip(const QString &)));

    connect(m_clipMonitor, SIGNAL(refreshClipThumbnail(const QString &)), m_projectList, SLOT(slotRefreshClipThumbnail(const QString &)));

    connect(m_clipMonitor, SIGNAL(adjustMonitorSize()), this, SLOT(slotAdjustClipMonitor()));
    connect(m_projectMonitor, SIGNAL(adjustMonitorSize()), this, SLOT(slotAdjustProjectMonitor()));

    connect(m_clipMonitor, SIGNAL(saveZone(Render *, QPoint)), this, SLOT(slotSaveZone(Render *, QPoint)));
    connect(m_projectMonitor, SIGNAL(saveZone(Render *, QPoint)), this, SLOT(slotSaveZone(Render *, QPoint)));
}

void MainWindow::slotAdjustClipMonitor() {
    clipMonitorDock->updateGeometry();
    clipMonitorDock->adjustSize();
    m_clipMonitor->resetSize();
}

void MainWindow::slotAdjustProjectMonitor() {
    projectMonitorDock->updateGeometry();
    projectMonitorDock->adjustSize();
    m_projectMonitor->resetSize();
}

void MainWindow::setupActions() {

    KActionCollection* collection = actionCollection();
    m_timecodeFormat = new KComboBox(this);
    m_timecodeFormat->addItem(i18n("hh:mm:ss::ff"));
    m_timecodeFormat->addItem(i18n("Frames"));

    statusProgressBar = new QProgressBar(this);
    statusProgressBar->setMinimum(0);
    statusProgressBar->setMaximum(100);
    statusProgressBar->setMaximumWidth(150);
    statusProgressBar->setVisible(false);

    QWidget *w = new QWidget;

    QHBoxLayout *layout = new QHBoxLayout;
    w->setLayout(layout);
    layout->setContentsMargins(5, 0, 5, 0);
    QToolBar *toolbar = new QToolBar("statusToolBar", this);


    m_toolGroup = new QActionGroup(this);

    QString style1 = "QToolButton {background-color: rgba(230, 230, 230, 220); border-style: inset; border:1px solid #999999;border-radius: 3px;margin: 0px 3px;padding: 0px;} QToolButton:checked { background-color: rgba(224, 224, 0, 100); border-style: inset; border:1px solid #cc6666;border-radius: 3px;}";

    m_buttonSelectTool = toolbar->addAction(KIcon("kdenlive-select-tool"), i18n("Selection tool"));
    m_buttonSelectTool->setCheckable(true);
    m_buttonSelectTool->setChecked(true);

    m_buttonRazorTool = toolbar->addAction(KIcon("edit-cut"), i18n("Razor tool"));
    m_buttonRazorTool->setCheckable(true);
    m_buttonRazorTool->setChecked(false);

    m_toolGroup->addAction(m_buttonSelectTool);
    m_toolGroup->addAction(m_buttonRazorTool);
    m_toolGroup->setExclusive(true);
    toolbar->setToolButtonStyle(Qt::ToolButtonIconOnly);

    QWidget * actionWidget;
    actionWidget = toolbar->widgetForAction(m_buttonSelectTool);
    actionWidget->setMaximumWidth(24);
    actionWidget->setMinimumHeight(17);

    actionWidget = toolbar->widgetForAction(m_buttonRazorTool);
    actionWidget->setMaximumWidth(24);
    actionWidget->setMinimumHeight(17);

    toolbar->setStyleSheet(style1);
    connect(m_toolGroup, SIGNAL(triggered(QAction *)), this, SLOT(slotChangeTool(QAction *)));

    toolbar->addSeparator();
    m_buttonFitZoom = toolbar->addAction(KIcon("zoom-fit-best"), i18n("Fit zoom to project"));
    m_buttonFitZoom->setCheckable(false);
    connect(m_buttonFitZoom, SIGNAL(triggered()), this, SLOT(slotFitZoom()));

    actionWidget = toolbar->widgetForAction(m_buttonFitZoom);
    actionWidget->setMaximumWidth(24);
    actionWidget->setMinimumHeight(17);

    m_zoomSlider = new QSlider(Qt::Horizontal, this);
    m_zoomSlider->setMaximum(13);
    m_zoomSlider->setPageStep(1);

    m_zoomSlider->setMaximumWidth(150);
    m_zoomSlider->setMinimumWidth(100);

    const int contentHeight = QFontMetrics(w->font()).height() + 8;

    QString style = "QSlider::groove:horizontal { background-color: rgba(230, 230, 230, 220);border: 1px solid #999999;height: 8px;border-radius: 3px;margin-top:3px }";
    style.append("QSlider::handle:horizontal {  background-color: white; border: 1px solid #999999;width: 9px;margin: -2px 0;border-radius: 3px; }");

    m_zoomSlider->setStyleSheet(style);

    //m_zoomSlider->height() + 5;
    statusBar()->setMinimumHeight(contentHeight);


    toolbar->addWidget(m_zoomSlider);

    m_buttonVideoThumbs = toolbar->addAction(KIcon("kdenlive-show-videothumb"), i18n("Show video thumbnails"));
    m_buttonVideoThumbs->setCheckable(true);
    m_buttonVideoThumbs->setChecked(KdenliveSettings::videothumbnails());
    connect(m_buttonVideoThumbs, SIGNAL(triggered()), this, SLOT(slotSwitchVideoThumbs()));

    m_buttonAudioThumbs = toolbar->addAction(KIcon("kdenlive-show-audiothumb"), i18n("Show audio thumbnails"));
    m_buttonAudioThumbs->setCheckable(true);
    m_buttonAudioThumbs->setChecked(KdenliveSettings::audiothumbnails());
    connect(m_buttonAudioThumbs, SIGNAL(triggered()), this, SLOT(slotSwitchAudioThumbs()));

    m_buttonShowMarkers = toolbar->addAction(KIcon("kdenlive-show-markers"), i18n("Show markers comments"));
    m_buttonShowMarkers->setCheckable(true);
    m_buttonShowMarkers->setChecked(KdenliveSettings::showmarkers());
    connect(m_buttonShowMarkers, SIGNAL(triggered()), this, SLOT(slotSwitchMarkersComments()));

    m_buttonSnap = toolbar->addAction(KIcon("kdenlive-snap"), i18n("Snap"));
    m_buttonSnap->setCheckable(true);
    m_buttonSnap->setChecked(KdenliveSettings::snaptopoints());
    connect(m_buttonSnap, SIGNAL(triggered()), this, SLOT(slotSwitchSnap()));
    layout->addWidget(toolbar);


    actionWidget = toolbar->widgetForAction(m_buttonVideoThumbs);
    actionWidget->setMaximumWidth(24);
    actionWidget->setMinimumHeight(17);

    actionWidget = toolbar->widgetForAction(m_buttonAudioThumbs);
    actionWidget->setMaximumWidth(24);
    actionWidget->setMinimumHeight(17);

    actionWidget = toolbar->widgetForAction(m_buttonShowMarkers);
    actionWidget->setMaximumWidth(24);
    actionWidget->setMinimumHeight(17);

    actionWidget = toolbar->widgetForAction(m_buttonSnap);
    actionWidget->setMaximumWidth(24);
    actionWidget->setMinimumHeight(17);

    m_messageLabel = new StatusBarMessageLabel(this);
    m_messageLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::MinimumExpanding);

    statusBar()->addWidget(m_messageLabel, 10);
    statusBar()->addWidget(statusProgressBar, 0);
    statusBar()->insertPermanentWidget(ID_TIMELINE_BUTTONS, w);
    statusBar()->insertPermanentFixedItem("00:00:00:00", ID_TIMELINE_POS);
    statusBar()->insertPermanentWidget(ID_TIMELINE_FORMAT, m_timecodeFormat);
    statusBar()->setMaximumHeight(statusBar()->font().pointSize() * 4);
    m_messageLabel->hide();

    collection->addAction("select_tool", m_buttonSelectTool);
    collection->addAction("razor_tool", m_buttonRazorTool);

    collection->addAction("show_video_thumbs", m_buttonVideoThumbs);
    collection->addAction("show_audio_thumbs", m_buttonAudioThumbs);
    collection->addAction("show_markers", m_buttonShowMarkers);
    collection->addAction("snap", m_buttonSnap);
    collection->addAction("zoom_fit", m_buttonFitZoom);

    m_projectSearch = new KAction(KIcon("edit-find"), i18n("Find"), this);
    collection->addAction("project_find", m_projectSearch);
    connect(m_projectSearch, SIGNAL(triggered(bool)), this, SLOT(slotFind()));
    m_projectSearch->setShortcut(Qt::Key_Slash);

    m_projectSearchNext = new KAction(KIcon("go-down-search"), i18n("Find Next"), this);
    collection->addAction("project_find_next", m_projectSearchNext);
    connect(m_projectSearchNext, SIGNAL(triggered(bool)), this, SLOT(slotFindNext()));
    m_projectSearchNext->setShortcut(Qt::Key_F3);
    m_projectSearchNext->setEnabled(false);

    KAction* profilesAction = new KAction(KIcon("document-new"), i18n("Manage Profiles"), this);
    collection->addAction("manage_profiles", profilesAction);
    connect(profilesAction, SIGNAL(triggered(bool)), this, SLOT(slotEditProfiles()));

    KAction* projectAction = new KAction(KIcon("configure"), i18n("Project Settings"), this);
    collection->addAction("project_settings", projectAction);
    connect(projectAction, SIGNAL(triggered(bool)), this, SLOT(slotEditProjectSettings()));

    KAction* projectRender = new KAction(KIcon("media-record"), i18n("Render"), this);
    collection->addAction("project_render", projectRender);
    connect(projectRender, SIGNAL(triggered(bool)), this, SLOT(slotRenderProject()));

    KAction* monitorPlay = new KAction(KIcon("media-playback-start"), i18n("Play"), this);
    KShortcut playShortcut;
    playShortcut.setPrimary(Qt::Key_Space);
    playShortcut.setAlternate(Qt::Key_K);
    monitorPlay->setShortcut(playShortcut);
    collection->addAction("monitor_play", monitorPlay);
    connect(monitorPlay, SIGNAL(triggered(bool)), m_monitorManager, SLOT(slotPlay()));

    KAction *markIn = collection->addAction("mark_in");
    markIn->setText(i18n("Set In Point"));
    markIn->setShortcut(Qt::Key_I);
    connect(markIn, SIGNAL(triggered(bool)), this, SLOT(slotSetInPoint()));

    KAction *markOut = collection->addAction("mark_out");
    markOut->setText(i18n("Set Out Point"));
    markOut->setShortcut(Qt::Key_O);
    connect(markOut, SIGNAL(triggered(bool)), this, SLOT(slotSetOutPoint()));

    KAction* monitorSeekBackward = new KAction(KIcon("media-seek-backward"), i18n("Rewind"), this);
    monitorSeekBackward->setShortcut(Qt::Key_J);
    collection->addAction("monitor_seek_backward", monitorSeekBackward);
    connect(monitorSeekBackward, SIGNAL(triggered(bool)), m_monitorManager, SLOT(slotRewind()));

    KAction* monitorSeekBackwardOneFrame = new KAction(KIcon("media-skip-backward"), i18n("Rewind 1 Frame"), this);
    monitorSeekBackwardOneFrame->setShortcut(Qt::Key_Left);
    collection->addAction("monitor_seek_backward-one-frame", monitorSeekBackwardOneFrame);
    connect(monitorSeekBackwardOneFrame, SIGNAL(triggered(bool)), m_monitorManager, SLOT(slotRewindOneFrame()));

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

    KAction* projectStart = new KAction(KIcon("media-seek-backward"), i18n("Go to Project Start"), this);
    projectStart->setShortcut(Qt::CTRL + Qt::Key_Home);
    collection->addAction("seek_start", projectStart);
    connect(projectStart, SIGNAL(triggered(bool)), m_monitorManager, SLOT(slotStart()));

    KAction* projectEnd = new KAction(KIcon("media-seek-forward"), i18n("Go to Project End"), this);
    projectEnd->setShortcut(Qt::CTRL + Qt::Key_End);
    collection->addAction("seek_end", projectEnd);
    connect(projectEnd, SIGNAL(triggered(bool)), m_monitorManager, SLOT(slotEnd()));

    KAction* monitorSeekForwardOneFrame = new KAction(KIcon("media-skip-forward"), i18n("Forward 1 Frame"), this);
    monitorSeekForwardOneFrame->setShortcut(Qt::Key_Right);
    collection->addAction("monitor_seek_forward-one-frame", monitorSeekForwardOneFrame);
    connect(monitorSeekForwardOneFrame, SIGNAL(triggered(bool)), m_monitorManager, SLOT(slotForwardOneFrame()));

    KAction* monitorSeekSnapForward = new KAction(KIcon("media-seek-forward"), i18n("Go to Next Snap Point"), this);
    monitorSeekSnapForward->setShortcut(Qt::ALT + Qt::Key_Right);
    collection->addAction("monitor_seek_snap_forward", monitorSeekSnapForward);
    connect(monitorSeekSnapForward, SIGNAL(triggered(bool)), this, SLOT(slotSnapForward()));

    KAction* deleteTimelineClip = new KAction(KIcon("edit-delete"), i18n("Delete Selected Item"), this);
    deleteTimelineClip->setShortcut(Qt::Key_Delete);
    collection->addAction("delete_timeline_clip", deleteTimelineClip);
    connect(deleteTimelineClip, SIGNAL(triggered(bool)), this, SLOT(slotDeleteTimelineClip()));

    KAction* editTimelineClipSpeed = new KAction(KIcon("edit-delete"), i18n("Change Clip Speed"), this);
    collection->addAction("change_clip_speed", editTimelineClipSpeed);
    connect(editTimelineClipSpeed, SIGNAL(triggered(bool)), this, SLOT(slotChangeClipSpeed()));

    KAction* cutTimelineClip = new KAction(KIcon("edit-cut"), i18n("Cut Clip"), this);
    cutTimelineClip->setShortcut(Qt::SHIFT + Qt::Key_R);
    collection->addAction("cut_timeline_clip", cutTimelineClip);
    connect(cutTimelineClip, SIGNAL(triggered(bool)), this, SLOT(slotCutTimelineClip()));

    KAction* addClipMarker = new KAction(KIcon("bookmark-new"), i18n("Add Marker to Clip"), this);
    collection->addAction("add_clip_marker", addClipMarker);
    connect(addClipMarker, SIGNAL(triggered(bool)), this, SLOT(slotAddClipMarker()));

    KAction* deleteClipMarker = new KAction(KIcon("edit-delete"), i18n("Delete Marker from Clip"), this);
    collection->addAction("delete_clip_marker", deleteClipMarker);
    connect(deleteClipMarker, SIGNAL(triggered(bool)), this, SLOT(slotDeleteClipMarker()));

    KAction* deleteAllClipMarkers = new KAction(KIcon("edit-delete"), i18n("Delete All Markers from Clip"), this);
    collection->addAction("delete_all_clip_markers", deleteAllClipMarkers);
    connect(deleteAllClipMarkers, SIGNAL(triggered(bool)), this, SLOT(slotDeleteAllClipMarkers()));

    KAction* editClipMarker = new KAction(KIcon("document-properties"), i18n("Edit Marker"), this);
    collection->addAction("edit_clip_marker", editClipMarker);
    connect(editClipMarker, SIGNAL(triggered(bool)), this, SLOT(slotEditClipMarker()));

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
    connect(pasteEffects , SIGNAL(triggered()), this, SLOT(slotPasteEffects()));

    KStandardAction::quit(this, SLOT(queryQuit()),
                          collection);

    KStandardAction::open(this, SLOT(openFile()),
                          collection);

    m_saveAction = KStandardAction::save(this, SLOT(saveFile()),
                                         collection);

    KStandardAction::saveAs(this, SLOT(saveFileAs()),
                            collection);

    KStandardAction::openNew(this, SLOT(newFile()),
                             collection);

    KStandardAction::preferences(this, SLOT(slotPreferences()),
                                 collection);

    KStandardAction::configureNotifications(this , SLOT(configureNotifications()) , collection);

    KStandardAction::copy(this, SLOT(slotCopy()),
                          collection);

    KStandardAction::paste(this, SLOT(slotPaste()),
                           collection);

    KAction *undo = KStandardAction::undo(m_commandStack, SLOT(undo()),
                                          collection);
    undo->setEnabled(false);
    connect(m_commandStack, SIGNAL(canUndoChanged(bool)), undo, SLOT(setEnabled(bool)));

    KAction *redo = KStandardAction::redo(m_commandStack, SLOT(redo()),
                                          collection);
    redo->setEnabled(false);
    connect(m_commandStack, SIGNAL(canRedoChanged(bool)), redo, SLOT(setEnabled(bool)));

    KStandardAction::fullScreen(this, SLOT(slotFullScreen()), this, collection);

    connect(collection, SIGNAL(actionHovered(QAction*)),
            this, SLOT(slotDisplayActionMessage(QAction*)));
    //connect(collection, SIGNAL( clearStatusText() ),
    //statusBar(), SLOT( clear() ) );
}

void MainWindow::slotDisplayActionMessage(QAction *a) {
    statusBar()->showMessage(a->data().toString(), 3000);
}

void MainWindow::saveOptions() {
    KdenliveSettings::self()->writeConfig();
    KSharedConfigPtr config = KGlobal::config();
    m_fileOpenRecent->saveEntries(KConfigGroup(config, "Recent Files"));
    config->sync();
}

void MainWindow::readOptions() {
    KSharedConfigPtr config = KGlobal::config();
    m_fileOpenRecent->loadEntries(KConfigGroup(config, "Recent Files"));
    KConfigGroup initialGroup(config, "version");
    if (!initialGroup.exists()) {
        // this is our first run, show Wizard
        Wizard *w = new Wizard(this);
        if (w->exec() == QDialog::Accepted && w->isOk()) {
            w->adjustSettings();
            initialGroup.writeEntry("version", "0.7");
            delete w;
        } else {
            ::exit(1);
        }
    }
}

void MainWindow::newFile() {
    QString profileName;
    KUrl projectFolder;
    QPoint projectTracks(3, 2);
    if (m_timelineArea->count() == 0) profileName = KdenliveSettings::default_profile();
    else {
        ProjectSettings *w = new ProjectSettings;
        if (w->exec() != QDialog::Accepted) return;
        profileName = w->selectedProfile();
        projectFolder = w->selectedFolder();
        projectTracks = w->tracks();
        delete w;
    }
    KdenliveDoc *doc = new KdenliveDoc(KUrl(), projectFolder, m_commandStack, profileName, projectTracks,  this);
    doc->m_autosave = new KAutoSaveFile(KUrl(), doc);
    TrackView *trackView = new TrackView(doc, this);
    m_timelineArea->addTab(trackView, KIcon("kdenlive"), doc->description());
    if (m_timelineArea->count() == 1) {
        connectDocumentInfo(doc);
        connectDocument(trackView, doc);
    } else m_timelineArea->setTabBarHidden(false);
}

void MainWindow::activateDocument() {
    if (m_timelineArea->currentWidget() == NULL) return;
    TrackView *currentTab = (TrackView *) m_timelineArea->currentWidget();
    KdenliveDoc *currentDoc = currentTab->document();
    connectDocumentInfo(currentDoc);
    connectDocument(currentTab, currentDoc);
}

void MainWindow::closeCurrentDocument() {
    QWidget *w = m_timelineArea->currentWidget();
    // closing current document
    int ix = m_timelineArea->currentIndex() + 1;
    if (ix == m_timelineArea->count()) ix = 0;
    m_timelineArea->setCurrentIndex(ix);
    TrackView *tabToClose = (TrackView *) w;
    KdenliveDoc *docToClose = tabToClose->document();
    if (docToClose && docToClose->isModified()) {
        switch (KMessageBox::warningYesNoCancel(this, i18n("Save changes to document ?"))) {
        case KMessageBox::Yes :
            // save document here. If saving fails, return false;
            saveFile();
            break;
        case KMessageBox::Cancel :
            return;
        default:
            break;
        }
    }
    m_timelineArea->removeTab(m_timelineArea->indexOf(w));
    if (m_timelineArea->count() == 1) m_timelineArea->setTabBarHidden(true);
    delete docToClose;
    delete w;
    if (m_timelineArea->count() == 0) m_activeDocument = NULL;
}

void MainWindow::saveFileAs(const QString &outputFileName) {
    m_projectMonitor->saveSceneList(outputFileName, m_activeDocument->documentInfoXml());
    m_activeDocument->setUrl(KUrl(outputFileName));
    if (m_activeDocument->m_autosave == NULL) {
        m_activeDocument->m_autosave = new KAutoSaveFile(KUrl(outputFileName), this);
    } else m_activeDocument->m_autosave->setManagedFile(KUrl(outputFileName));
    setCaption(m_activeDocument->description());
    m_timelineArea->setTabText(m_timelineArea->currentIndex(), m_activeDocument->description());
    m_timelineArea->setTabToolTip(m_timelineArea->currentIndex(), m_activeDocument->url().path());
    m_activeDocument->setModified(false);
    m_fileOpenRecent->addUrl(KUrl(outputFileName));
}

void MainWindow::saveFileAs() {
    QString outputFile = KFileDialog::getSaveFileName(KUrl(), "*.kdenlive|Kdenlive project files (*.kdenlive)");
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
        m_activeDocument->m_autosave->resize(0);
    }
}

void MainWindow::openFile() {
    KUrl url = KFileDialog::getOpenUrl(KUrl(), "*.kdenlive|Kdenlive project files (*.kdenlive)\n*.westley|MLT project files (*.westley)");
    if (url.isEmpty()) return;
    m_fileOpenRecent->addUrl(url);
    openFile(url);
}

void MainWindow::openLastFile() {
    KSharedConfigPtr config = KGlobal::config();
    KUrl::List urls = m_fileOpenRecent->urls();
    if (urls.isEmpty()) newFile();
    else openFile(urls.last());
}

void MainWindow::openFile(const KUrl &url) {
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
    doOpenFile(url, NULL);
}

void MainWindow::doOpenFile(const KUrl &url, KAutoSaveFile *stale) {
    KdenliveDoc *doc;
    doc = new KdenliveDoc(url, KUrl(), m_commandStack, QString(), QPoint(3, 2), this);
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
    TrackView *trackView = new TrackView(doc, this);
    m_timelineArea->setCurrentIndex(m_timelineArea->addTab(trackView, KIcon("kdenlive"), doc->description()));
    m_timelineArea->setTabToolTip(m_timelineArea->currentIndex(), doc->url().path());
    if (m_timelineArea->count() > 1) m_timelineArea->setTabBarHidden(false);
    slotGotProgressInfo(QString(), -1);
    m_clipMonitor->refreshMonitor(true);
}

void MainWindow::recoverFiles(QList<KAutoSaveFile *> staleFiles) {
    if (!KdenliveSettings::activatetabs()) closeCurrentDocument();
    foreach(KAutoSaveFile *stale, staleFiles) {
        /*if (!stale->open(QIODevice::QIODevice::ReadOnly)) {
                  // show an error message; we could not steal the lockfile
                  // maybe another application got to the file before us?
                  delete stale;
                  continue;
        }*/
        kDebug() << "// OPENING RECOVERY: " << stale->fileName() << "\nMANAGED: " << stale->managedFile().path();
        // the stalefiles also contain ".lock" files so we must ignore them... bug in KAutoSaveFile ?
        if (!stale->fileName().endsWith(".lock")) doOpenFile(KUrl(stale->fileName()), stale);
        else KIO::NetAccess::del(KUrl(stale->fileName()), this);
    }
}


void MainWindow::parseProfiles() {
    //kdDebug()<<" + + YOUR MLT INSTALL WAS FOUND IN: "<< MLT_PREFIX <<endl;

    //KdenliveSettings::setDefaulttmpfolder();
    if (KdenliveSettings::mltpath().isEmpty()) {
        KdenliveSettings::setMltpath(QString(MLT_PREFIX) + QString("/share/mlt/profiles/"));
    }
    if (KdenliveSettings::rendererpath().isEmpty()) {
        QString inigoPath = QString(MLT_PREFIX) + QString("/bin/inigo");
        if (!QFile::exists(inigoPath))
            inigoPath = KStandardDirs::findExe("inigo");
        else KdenliveSettings::setRendererpath(inigoPath);
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
            if (getUrl->exec() == QDialog::Rejected) {
                ::exit(0);
            }
            KUrl mltPath = getUrl->selectedUrl();
            delete getUrl;
            if (mltPath.isEmpty()) ::exit(0);
            KdenliveSettings::setMltpath(mltPath.path());
            QStringList profilesList = QDir(KdenliveSettings::mltpath()).entryList(profilesFilter, QDir::Files);
        }
    }

    if (KdenliveSettings::rendererpath().isEmpty()) {
        // Cannot find the MLT inigo renderer, ask for location
        KUrlRequesterDialog *getUrl = new KUrlRequesterDialog(QString(), i18n("Cannot find the inigo program required for rendering (part of Mlt)"), this);
        if (getUrl->exec() == QDialog::Rejected) {
            ::exit(0);
        }
        KUrl rendererPath = getUrl->selectedUrl();
        delete getUrl;
        if (rendererPath.isEmpty()) ::exit(0);
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
        KdenliveSettings::setCurrent_profile(profile);
        KdenliveSettings::setProject_fps(m_activeDocument->fps());
        setCaption(m_activeDocument->description(), m_activeDocument->isModified());
        m_monitorManager->resetProfiles(m_activeDocument->timecode());
        if (m_renderWidget) m_renderWidget->setDocumentStandard(m_activeDocument->getDocumentStandard());
        m_timelineArea->setTabText(m_timelineArea->currentIndex(), m_activeDocument->description());

        // We need to desactivate & reactivate monitors to get a refresh
        m_monitorManager->switchMonitors();
    }
    delete w;
}

void MainWindow::slotRenderProject() {
    if (!m_renderWidget) {
        m_renderWidget = new RenderWidget(this);
        connect(m_renderWidget, SIGNAL(doRender(const QString&, const QString&, const QStringList &, const QStringList &, bool, bool, double, double)), this, SLOT(slotDoRender(const QString&, const QString&, const QStringList &, const QStringList &, bool, bool, double, double)));
        if (m_activeDocument) m_renderWidget->setGuides(m_activeDocument->guidesXml(), m_activeDocument->projectDuration());
    }
    /*TrackView *currentTab = (TrackView *) m_timelineArea->currentWidget();
    if (currentTab) m_renderWidget->setTimeline(currentTab);
    m_renderWidget->setDocument(m_activeDocument);*/
    m_renderWidget->show();
}

void MainWindow::slotDoRender(const QString &dest, const QString &render, const QStringList &overlay_args, const QStringList &avformat_args, bool zoneOnly, bool playAfter, double guideStart, double guideEnd) {
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
        else if (guideStart != -1) {
            args << "in=" + QString::number(GenTime(guideStart).frames(m_activeDocument->fps())) << "out=" + QString::number(GenTime(guideEnd).frames(m_activeDocument->fps()));
        }
        if (!overlay_args.isEmpty()) args << "preargs=" + overlay_args.join(" ");
        QString videoPlayer = "-";
        if (playAfter) {
            videoPlayer = KdenliveSettings::defaultplayerapp();
            if (videoPlayer.isEmpty()) KMessageBox::sorry(this, i18n("Cannot play video after rendering because the default video player application is not set.\nPlease define it in Kdenlive settings dialog."));
        }
        args << KdenliveSettings::rendererpath() << m_activeDocument->profilePath() << render << videoPlayer << temp.fileName() << dest << avformat_args;
        QString renderer = QCoreApplication::applicationDirPath() + QString("/kdenlive_render");
        if (!QFile::exists(renderer)) renderer = "kdenlive_render";
        QProcess::startDetached(renderer, args);

        KNotification::event("RenderStarted", i18n("Rendering <i>%1</i> started", dest), QPixmap(), this);
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
    m_saveAction->setEnabled(modified);
    if (modified) {
        m_timelineArea->setTabTextColor(m_timelineArea->currentIndex(), palette().color(QPalette::Link));
        m_timelineArea->setTabIcon(m_timelineArea->currentIndex(), KIcon("document-save"));
    } else {
        m_timelineArea->setTabTextColor(m_timelineArea->currentIndex(), palette().color(QPalette::Text));
        m_timelineArea->setTabIcon(m_timelineArea->currentIndex(), KIcon("kdenlive"));
    }
}

void MainWindow::connectDocumentInfo(KdenliveDoc *doc) {
    if (m_activeDocument) {
        if (m_activeDocument == doc) return;
        disconnect(m_activeDocument, SIGNAL(progressInfo(const QString &, int)), this, SLOT(slotGotProgressInfo(const QString &, int)));
    }
    connect(doc, SIGNAL(progressInfo(const QString &, int)), this, SLOT(slotGotProgressInfo(const QString &, int)));
}

void MainWindow::connectDocument(TrackView *trackView, KdenliveDoc *doc) { //changed
    //m_projectMonitor->stop();
    kDebug() << "///////////////////   CONNECTING DOC TO PROJECT VIEW ////////////////";
    if (m_activeDocument) {
        if (m_activeDocument == doc) return;
        m_activeDocument->backupMltPlaylist();
        if (m_activeTimeline) {
            disconnect(m_projectMonitor, SIGNAL(renderPosition(int)), m_activeTimeline, SLOT(moveCursorPos(int)));
            disconnect(m_projectMonitor, SIGNAL(zoneUpdated(QPoint)), trackView, SLOT(slotSetZone(QPoint)));
            disconnect(m_projectMonitor, SIGNAL(durationChanged(int)), m_activeTimeline, SLOT(setDuration(int)));
            disconnect(m_projectList, SIGNAL(projectModified()), m_activeDocument, SLOT(setModified()));
            disconnect(m_activeDocument, SIGNAL(guidesUpdated()), this, SLOT(slotGuidesUpdated()));
            disconnect(m_activeDocument, SIGNAL(addProjectClip(DocClipBase *)), m_projectList, SLOT(slotAddClip(DocClipBase *)));
            disconnect(m_activeDocument, SIGNAL(addProjectFolder(const QString, const QString &, bool, bool)), m_projectList, SLOT(slotAddFolder(const QString, const QString &, bool, bool)));
            disconnect(m_activeDocument, SIGNAL(signalDeleteProjectClip(const QString &)), m_projectList, SLOT(slotDeleteClip(const QString &)));
            disconnect(m_activeDocument, SIGNAL(updateClipDisplay(const QString &)), m_projectList, SLOT(slotUpdateClip(const QString &)));
            disconnect(m_activeDocument, SIGNAL(selectLastAddedClip(const QString &)), m_projectList, SLOT(slotSelectClip(const QString &)));
            disconnect(m_activeDocument, SIGNAL(deleteTimelineClip(const QString &)), m_activeTimeline, SLOT(slotDeleteClip(const QString &)));
            disconnect(m_activeTimeline, SIGNAL(clipItemSelected(ClipItem*)), effectStack, SLOT(slotClipItemSelected(ClipItem*)));
            disconnect(m_activeTimeline, SIGNAL(clipItemSelected(ClipItem*)), this, SLOT(slotActivateEffectStackView()));
            disconnect(m_activeTimeline, SIGNAL(transitionItemSelected(Transition*)), transitionConfig, SLOT(slotTransitionItemSelected(Transition*)));
            disconnect(m_activeTimeline, SIGNAL(transitionItemSelected(Transition*)), this, SLOT(slotActivateTransitionView()));
            disconnect(m_zoomSlider, SIGNAL(valueChanged(int)), m_activeTimeline, SLOT(slotChangeZoom(int)));
            disconnect(m_activeTimeline->projectView(), SIGNAL(displayMessage(const QString&, MessageType)), m_messageLabel, SLOT(setMessage(const QString&, MessageType)));
            disconnect(m_activeTimeline->projectView(), SIGNAL(showClipFrame(DocClipBase *, const int)), m_clipMonitor, SLOT(slotSetXml(DocClipBase *, const int)));

            disconnect(m_activeDocument, SIGNAL(docModified(bool)), this, SLOT(slotUpdateDocumentState(bool)));
            disconnect(effectStack, SIGNAL(updateClipEffect(ClipItem*, QDomElement, QDomElement, int)), m_activeTimeline->projectView(), SLOT(slotUpdateClipEffect(ClipItem*, QDomElement, QDomElement, int)));
            disconnect(effectStack, SIGNAL(removeEffect(ClipItem*, QDomElement)), m_activeTimeline->projectView(), SLOT(slotDeleteEffect(ClipItem*, QDomElement)));
            disconnect(effectStack, SIGNAL(changeEffectState(ClipItem*, int, bool)), m_activeTimeline->projectView(), SLOT(slotChangeEffectState(ClipItem*, int, bool)));
            disconnect(effectStack, SIGNAL(changeEffectPosition(ClipItem*, int, int)), trackView->projectView(), SLOT(slotChangeEffectPosition(ClipItem*, int, int)));
            disconnect(effectStack, SIGNAL(refreshEffectStack(ClipItem*)), m_activeTimeline->projectView(), SLOT(slotRefreshEffects(ClipItem*)));
            disconnect(effectStack, SIGNAL(reloadEffects()), this, SLOT(slotReloadEffects()));
            disconnect(transitionConfig, SIGNAL(transitionUpdated(Transition *, QDomElement)), trackView->projectView() , SLOT(slotTransitionUpdated(Transition *, QDomElement)));
            disconnect(transitionConfig, SIGNAL(seekTimeline(int)), trackView->projectView() , SLOT(setCursorPos(int)));
            disconnect(m_activeTimeline->projectView(), SIGNAL(activateDocumentMonitor()), m_projectMonitor, SLOT(activateMonitor()));
            disconnect(trackView, SIGNAL(zoneMoved(int, int)), m_projectMonitor, SLOT(slotZoneMoved(int, int)));
            effectStack->clear();
        }
        m_activeDocument->setRenderer(NULL);
        disconnect(m_projectList, SIGNAL(clipSelected(DocClipBase *)), m_clipMonitor, SLOT(slotSetXml(DocClipBase *)));
        m_clipMonitor->stop();
    }
    KdenliveSettings::setCurrent_profile(doc->profilePath());
    KdenliveSettings::setProject_fps(doc->fps());
    m_monitorManager->resetProfiles(doc->timecode());
    m_projectList->setDocument(doc);
    transitionConfig->updateProjectFormat(doc->mltProfile());
    connect(m_projectList, SIGNAL(clipSelected(DocClipBase *)), m_clipMonitor, SLOT(slotSetXml(DocClipBase *)));
    connect(m_projectList, SIGNAL(projectModified()), doc, SLOT(setModified()));
    connect(trackView, SIGNAL(cursorMoved()), m_projectMonitor, SLOT(activateMonitor()));
    connect(trackView, SIGNAL(mousePosition(int)), this, SLOT(slotUpdateMousePosition(int)));
    connect(m_projectMonitor, SIGNAL(renderPosition(int)), trackView, SLOT(moveCursorPos(int)));
    connect(m_projectMonitor, SIGNAL(zoneUpdated(QPoint)), trackView, SLOT(slotSetZone(QPoint)));
    connect(m_projectMonitor, SIGNAL(durationChanged(int)), trackView, SLOT(setDuration(int)));
    connect(doc, SIGNAL(addProjectClip(DocClipBase *)), m_projectList, SLOT(slotAddClip(DocClipBase *)));
    connect(doc, SIGNAL(addProjectFolder(const QString, const QString &, bool, bool)), m_projectList, SLOT(slotAddFolder(const QString, const QString &, bool, bool)));
    connect(doc, SIGNAL(signalDeleteProjectClip(const QString &)), m_projectList, SLOT(slotDeleteClip(const QString &)));
    connect(doc, SIGNAL(updateClipDisplay(const QString &)), m_projectList, SLOT(slotUpdateClip(const QString &)));
    connect(doc, SIGNAL(selectLastAddedClip(const QString &)), m_projectList, SLOT(slotSelectClip(const QString &)));

    connect(doc, SIGNAL(deleteTimelineClip(const QString &)), trackView, SLOT(slotDeleteClip(const QString &)));
    connect(doc, SIGNAL(docModified(bool)), this, SLOT(slotUpdateDocumentState(bool)));
    connect(doc, SIGNAL(guidesUpdated()), this, SLOT(slotGuidesUpdated()));


    connect(trackView, SIGNAL(clipItemSelected(ClipItem*)), effectStack, SLOT(slotClipItemSelected(ClipItem*)));
    connect(trackView, SIGNAL(clipItemSelected(ClipItem*)), this, SLOT(slotActivateEffectStackView()));
    connect(trackView, SIGNAL(transitionItemSelected(Transition*)), transitionConfig, SLOT(slotTransitionItemSelected(Transition*)));
    connect(trackView, SIGNAL(transitionItemSelected(Transition*)), this, SLOT(slotActivateTransitionView()));
    m_zoomSlider->setValue(doc->zoom());
    connect(m_zoomSlider, SIGNAL(valueChanged(int)), trackView, SLOT(slotChangeZoom(int)));
    connect(trackView->projectView(), SIGNAL(zoomIn()), this, SLOT(slotZoomIn()));
    connect(trackView->projectView(), SIGNAL(zoomOut()), this, SLOT(slotZoomOut()));
    connect(trackView->projectView(), SIGNAL(displayMessage(const QString&, MessageType)), m_messageLabel, SLOT(setMessage(const QString&, MessageType)));

    connect(trackView->projectView(), SIGNAL(showClipFrame(DocClipBase *, const int)), m_clipMonitor, SLOT(slotSetXml(DocClipBase *, const int)));


    connect(effectStack, SIGNAL(updateClipEffect(ClipItem*, QDomElement, QDomElement, int)), trackView->projectView(), SLOT(slotUpdateClipEffect(ClipItem*, QDomElement, QDomElement, int)));
    connect(effectStack, SIGNAL(removeEffect(ClipItem*, QDomElement)), trackView->projectView(), SLOT(slotDeleteEffect(ClipItem*, QDomElement)));
    connect(effectStack, SIGNAL(changeEffectState(ClipItem*, int, bool)), trackView->projectView(), SLOT(slotChangeEffectState(ClipItem*, int, bool)));
    connect(effectStack, SIGNAL(changeEffectPosition(ClipItem*, int, int)), trackView->projectView(), SLOT(slotChangeEffectPosition(ClipItem*, int, int)));
    connect(effectStack, SIGNAL(refreshEffectStack(ClipItem*)), trackView->projectView(), SLOT(slotRefreshEffects(ClipItem*)));
    connect(transitionConfig, SIGNAL(transitionUpdated(Transition *, QDomElement)), trackView->projectView() , SLOT(slotTransitionUpdated(Transition *, QDomElement)));
    connect(transitionConfig, SIGNAL(seekTimeline(int)), trackView->projectView() , SLOT(setCursorPos(int)));
    connect(effectStack, SIGNAL(reloadEffects()), this, SLOT(slotReloadEffects()));

    connect(trackView->projectView(), SIGNAL(activateDocumentMonitor()), m_projectMonitor, SLOT(activateMonitor()));
    connect(trackView, SIGNAL(zoneMoved(int, int)), m_projectMonitor, SLOT(slotZoneMoved(int, int)));

    trackView->projectView()->setContextMenu(m_timelineContextMenu, m_timelineContextClipMenu, m_timelineContextTransitionMenu);
    m_activeTimeline = trackView;
    if (m_renderWidget) m_renderWidget->setDocumentStandard(doc->getDocumentStandard());
    doc->setRenderer(m_projectMonitor->render);
    m_commandStack->setActiveStack(doc->commandStack());
    KdenliveSettings::setProject_display_ratio(doc->dar());
    m_projectList->updateAllClips();
    trackView->projectView()->updateAllThumbs();


    //m_overView->setScene(trackView->projectScene());
    //m_overView->scale(m_overView->width() / trackView->duration(), m_overView->height() / (50 * trackView->tracksNumber()));
    //m_overView->fitInView(m_overView->itemAt(0, 50), Qt::KeepAspectRatio);

    setCaption(doc->description(), doc->isModified());
    m_saveAction->setEnabled(doc->isModified());
    m_activeDocument = doc;
}

void MainWindow::slotGuidesUpdated() {
    if (m_renderWidget) m_renderWidget->setGuides(m_activeDocument->guidesXml(), m_activeDocument->projectDuration());
}

void MainWindow::slotPreferences(int page, int option) {
    //An instance of your dialog could be already created and could be
    // cached, in which case you want to display the cached dialog
    // instead of creating another one
    if (KConfigDialog::showDialog("settings")) {
        if (page != -1) static_cast <KdenliveSettingsDialog*>(KConfigDialog::exists("settings"))->showPage(page, option);
        return;
    }

    // KConfigDialog didn't find an instance of this dialog, so lets
    // create it :
    KdenliveSettingsDialog* dialog = new KdenliveSettingsDialog(this);
    connect(dialog, SIGNAL(settingsChanged(const QString&)), this, SLOT(updateConfiguration()));
    connect(dialog, SIGNAL(doResetProfile()), m_monitorManager, SLOT(slotResetProfiles()));
    dialog->show();
    if (page != -1) dialog->showPage(page, option);
}

void MainWindow::updateConfiguration() {
    //TODO: we should apply settings to all projects, not only the current one
    if (m_activeTimeline) {
        m_activeTimeline->refresh();
        m_activeTimeline->projectView()->checkAutoScroll();
        m_activeTimeline->projectView()->checkTrackHeight();
        if (m_activeDocument) m_activeDocument->clipManager()->checkAudioThumbs();
    }
    m_buttonAudioThumbs->setChecked(KdenliveSettings::audiothumbnails());
    m_buttonVideoThumbs->setChecked(KdenliveSettings::videothumbnails());
    activateShuttleDevice();

}

void MainWindow::slotSwitchVideoThumbs() {
    KdenliveSettings::setVideothumbnails(!KdenliveSettings::videothumbnails());
    if (m_activeTimeline) {
        m_activeTimeline->refresh();
    }
    m_buttonVideoThumbs->setChecked(KdenliveSettings::videothumbnails());
}

void MainWindow::slotSwitchAudioThumbs() {
    KdenliveSettings::setAudiothumbnails(!KdenliveSettings::audiothumbnails());
    if (m_activeTimeline) {
        m_activeTimeline->refresh();
        m_activeTimeline->projectView()->checkAutoScroll();
        if (m_activeDocument) m_activeDocument->clipManager()->checkAudioThumbs();
    }
    m_buttonAudioThumbs->setChecked(KdenliveSettings::audiothumbnails());
}

void MainWindow::slotSwitchMarkersComments() {
    KdenliveSettings::setShowmarkers(!KdenliveSettings::showmarkers());
    if (m_activeTimeline) {
        m_activeTimeline->refresh();
    }
    m_buttonShowMarkers->setChecked(KdenliveSettings::showmarkers());
}

void MainWindow::slotSwitchSnap() {
    KdenliveSettings::setSnaptopoints(!KdenliveSettings::snaptopoints());
    m_buttonShowMarkers->setChecked(KdenliveSettings::snaptopoints());
}


void MainWindow::slotDeleteTimelineClip() {
    if (m_activeTimeline) {
        m_activeTimeline->projectView()->deleteSelectedClips();
    }
}

void MainWindow::slotChangeClipSpeed() {
    if (m_activeTimeline) {
        m_activeTimeline->projectView()->changeClipSpeed();
    }
}

void MainWindow::slotAddClipMarker() {
    if (m_activeTimeline) {
        m_activeTimeline->projectView()->slotAddClipMarker();
    }
}

void MainWindow::slotDeleteClipMarker() {
    if (m_activeTimeline) {
        m_activeTimeline->projectView()->slotDeleteClipMarker();
    }
}

void MainWindow::slotDeleteAllClipMarkers() {
    if (m_activeTimeline) {
        m_activeTimeline->projectView()->slotDeleteAllClipMarkers();
    }
}

void MainWindow::slotEditClipMarker() {
    if (m_activeTimeline) {
        m_activeTimeline->projectView()->slotEditClipMarker();
    }
}

void MainWindow::slotAddGuide() {
    if (m_activeTimeline)
        m_activeTimeline->projectView()->slotAddGuide();
}

void MainWindow::slotEditGuide() {
    if (m_activeTimeline)
        m_activeTimeline->projectView()->slotEditGuide();
}

void MainWindow::slotDeleteGuide() {
    if (m_activeTimeline)
        m_activeTimeline->projectView()->slotDeleteGuide();
}

void MainWindow::slotDeleteAllGuides() {
    if (m_activeTimeline)
        m_activeTimeline->projectView()->slotDeleteAllGuides();
}

void MainWindow::slotCutTimelineClip() {
    if (m_activeTimeline) {
        m_activeTimeline->projectView()->cutSelectedClips();
    }
}

void MainWindow::slotAddProjectClip(KUrl url) {
    if (m_activeDocument)
        m_activeDocument->slotAddClipFile(url, QString());
}

void MainWindow::slotAddTransition(QAction *result) {
    if (!result) return;
    QDomElement effect = transitions.getEffectByName(result->data().toString());
    if (m_activeTimeline) {
        m_activeTimeline->projectView()->slotAddTransitionToSelectedClips(effect);
    }
}

void MainWindow::slotAddVideoEffect(QAction *result) {
    if (!result) return;
    QDomElement effect = videoEffects.getEffectByName(result->data().toString());
    slotAddEffect(effect);
}

void MainWindow::slotAddAudioEffect(QAction *result) {
    if (!result) return;
    QDomElement effect = audioEffects.getEffectByName(result->data().toString());
    slotAddEffect(effect);
}

void MainWindow::slotAddCustomEffect(QAction *result) {
    if (!result) return;
    if (result->data().toString().isEmpty()) return;
    QDomElement effect = customEffects.getEffectByName(result->data().toString());
    slotAddEffect(effect);
}

void MainWindow::slotZoomIn() {
    m_zoomSlider->setValue(m_zoomSlider->value() - 1);
}

void MainWindow::slotZoomOut() {
    m_zoomSlider->setValue(m_zoomSlider->value() + 1);
}

void MainWindow::slotFitZoom() {
    if (m_activeTimeline) {
        m_zoomSlider->setValue(m_activeTimeline->fitZoom());
    }
}

void MainWindow::slotGotProgressInfo(const QString &message, int progress) {
    statusProgressBar->setValue(progress);
    if (progress >= 0) {
        if (!message.isEmpty()) m_messageLabel->setMessage(message, InformationMessage);//statusLabel->setText(message);
        statusProgressBar->setVisible(true);
    } else {
        m_messageLabel->setMessage(QString(), DefaultMessage);
        statusProgressBar->setVisible(false);
    }
}

void MainWindow::slotShowClipProperties(DocClipBase *clip) {
    if (clip->clipType() == TEXT) {
        QString titlepath = m_activeDocument->projectFolder().path() + "/titles/";
        QString path = clip->getProperty("resource");
        TitleWidget *dia_ui = new TitleWidget(KUrl(), titlepath, m_projectMonitor->render, this);
        QDomDocument doc;
        doc.setContent(clip->getProperty("xmldata"));
        dia_ui->setXml(doc);
        if (dia_ui->exec() == QDialog::Accepted) {
            QPixmap pix = dia_ui->renderedPixmap();
            pix.save(path);
            //slotAddClipFile(KUrl("/tmp/kdenlivetitle.png"), QString(), -1);
            //m_clipManager->slotEditTextClipFile(id, dia_ui->xml().toString());
            QMap <QString, QString> newprops;
            newprops.insert("xmldata", dia_ui->xml().toString());
            EditClipCommand *command = new EditClipCommand(m_projectList, clip->getId(), clip->properties(), newprops, true);
            m_activeDocument->commandStack()->push(command);
            m_clipMonitor->refreshMonitor(true);
            m_activeDocument->setModified(true);
        }
        delete dia_ui;

        //m_activeDocument->editTextClip(clip->getProperty("xml"), clip->getId());
        return;
    }
    ClipProperties dia(clip, m_activeDocument->timecode(), m_activeDocument->fps(), this);
    connect(&dia, SIGNAL(addMarker(const QString &, GenTime, QString)), m_activeTimeline->projectView(), SLOT(slotAddClipMarker(const QString &, GenTime, QString)));
    if (dia.exec() == QDialog::Accepted) {
        EditClipCommand *command = new EditClipCommand(m_projectList, dia.clipId(), clip->properties(), dia.properties(), true);
        m_activeDocument->commandStack()->push(command);

        //m_projectList->slotUpdateClipProperties(dia.clipId(), dia.properties());
        if (dia.needsTimelineRefresh()) {
            // update clip occurences in timeline
            m_activeTimeline->projectView()->slotUpdateClip(dia.clipId());
        }
    }
}

void MainWindow::customEvent(QEvent* e) {
    if (e->type() == QEvent::User) {
        // The timeline playing position changed...
        kDebug() << "RECIEVED JOG EVEMNT!!!";
    }
}
void MainWindow::slotActivateEffectStackView() {
    effectStack->raiseWindow(effectStackDock);
}

void MainWindow::slotActivateTransitionView() {
    transitionConfig->raiseWindow(transitionConfigDock);
}

void MainWindow::slotSnapRewind() {
    if (m_projectMonitor->isActive()) {
        if (m_activeTimeline)
            m_activeTimeline->projectView()->slotSeekToPreviousSnap();
    }
}

void MainWindow::slotSnapForward() {
    if (m_projectMonitor->isActive()) {
        if (m_activeTimeline)
            m_activeTimeline->projectView()->slotSeekToNextSnap();
    }
}

void MainWindow::slotClipStart() {
    if (m_projectMonitor->isActive()) {
        if (m_activeTimeline)
            m_activeTimeline->projectView()->clipStart();
    }
}

void MainWindow::slotClipEnd() {
    if (m_projectMonitor->isActive()) {
        if (m_activeTimeline)
            m_activeTimeline->projectView()->clipEnd();
    }
}

void MainWindow::slotChangeTool(QAction * action) {
    if (action == m_buttonSelectTool) slotSetTool(SELECTTOOL);
    else if (action == m_buttonRazorTool) slotSetTool(RAZORTOOL);
}

void MainWindow::slotSetTool(PROJECTTOOL tool) {
    if (m_activeDocument && m_activeTimeline) {
        //m_activeDocument->setTool(tool);
        m_activeTimeline->projectView()->setTool(tool);
    }
}

void MainWindow::slotCopy() {
    if (!m_activeDocument || !m_activeTimeline) return;
    m_activeTimeline->projectView()->copyClip();
}

void MainWindow::slotPaste() {
    if (!m_activeDocument || !m_activeTimeline) return;
    m_activeTimeline->projectView()->pasteClip();
}

void MainWindow::slotPasteEffects() {
    if (!m_activeDocument || !m_activeTimeline) return;
    m_activeTimeline->projectView()->pasteClipEffects();
}

void MainWindow::slotFind() {
    if (!m_activeDocument || !m_activeTimeline) return;
    m_projectSearch->setEnabled(false);
    m_findActivated = true;
    m_findString = QString();
    m_activeTimeline->projectView()->initSearchStrings();
    statusBar()->showMessage(i18n("Starting -- find text as you type"));
    m_findTimer.start(5000);
    qApp->installEventFilter(this);
}

void MainWindow::slotFindNext() {
    if (m_activeTimeline && m_activeTimeline->projectView()->findNextString(m_findString)) {
        statusBar()->showMessage(i18n("Found : %1", m_findString));
    } else {
        statusBar()->showMessage(i18n("Reached end of project"));
    }
    m_findTimer.start(4000);
}

void MainWindow::findAhead() {
    if (m_activeTimeline && m_activeTimeline->projectView()->findString(m_findString)) {
        m_projectSearchNext->setEnabled(true);
        statusBar()->showMessage(i18n("Found : %1", m_findString));
    } else {
        m_projectSearchNext->setEnabled(false);
        statusBar()->showMessage(i18n("Not found : %1", m_findString));
    }
}

void MainWindow::findTimeout() {
    m_projectSearchNext->setEnabled(false);
    m_findActivated = false;
    m_findString = QString();
    statusBar()->showMessage(i18n("Find stopped"), 3000);
    if (m_activeTimeline) m_activeTimeline->projectView()->clearSearchStrings();
    m_projectSearch->setEnabled(true);
    removeEventFilter(this);
}

void MainWindow::keyPressEvent(QKeyEvent *ke) {
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

bool MainWindow::eventFilter(QObject *obj, QEvent *event) {
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

void MainWindow::slotSaveZone(Render *render, QPoint zone) {
    KDialog *dialog = new KDialog(this);
    dialog->setCaption("Save clip zone");
    dialog->setButtons(KDialog::Ok | KDialog::Cancel);

    QWidget *widget = new QWidget(dialog);
    dialog->setMainWidget(widget);

    QVBoxLayout *vbox = new QVBoxLayout(widget);
    QLabel *label1 = new QLabel(i18n("Save clip zone as:"), this);
    QString path = m_activeDocument->projectFolder().path();
    path.append("/");
    path.append("untitled.westley");
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

void MainWindow::slotSetInPoint() {
    if (m_clipMonitor->isActive()) {
        m_clipMonitor->slotSetZoneStart();
    } else m_activeTimeline->projectView()->setInPoint();
}

void MainWindow::slotSetOutPoint() {
    if (m_clipMonitor->isActive()) {
        m_clipMonitor->slotSetZoneEnd();
    } else m_activeTimeline->projectView()->setOutPoint();
}

#include "mainwindow.moc"
