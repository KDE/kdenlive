/***************************************************************************
                         kdenlive.cpp  -  description
                            -------------------
   begin                : Fri Feb 15 01:46:16 GMT 2002
   copyright            : (C) 2002 by Jason Wood
   email                : jasonwood@blueyonder.co.uk
***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#define _ISOC99_SOURCE
#include <cmath>
#include <iostream>

// include files for QT
#include <qdir.h>
#include <qprinter.h>
#include <qpainter.h>
#include <qtooltip.h>
#include <qradiobutton.h>
#include <qcolor.h>
#include <qspinbox.h>
#include <qwhatsthis.h>
#include <qtoolbutton.h>
#include <qcheckbox.h>
#include <qsplitter.h>

// include files for KDE
#include <kconfig.h>
#include <kcommand.h>
#include <kdebug.h>
#include <kedittoolbar.h>
#include <kfiledialog.h>
#include <kiconloader.h>
#include <kkeydialog.h>
#include <knotifydialog.h>
#include <klocale.h>
#include <kmenubar.h>
#include <kmessagebox.h>
#include <kcombobox.h>
#include <kstatusbar.h>
#include <kstdaction.h>
#include <kcolorbutton.h>
#include <kurlrequester.h>
#include <klineedit.h>
#include <ktextedit.h>
#include <kurlrequesterdlg.h>
#include <kstandarddirs.h>
#include <kio/netaccess.h>
#include <kmdcodec.h>
#include <kfileitem.h>
#include <kinputdialog.h>
#include <kpassivepopup.h>

// application specific includes
// p.s., get the idea this class is kind, central to everything?
#include "capturemonitor.h"
#include "clipdrag.h"
#include "clippropertiesdialog.h"
#include "configureprojectdialog.h"
#include "docclipbase.h"
#include "docclipavfile.h"
#include "doccliptextfile.h"
#include "docclipref.h"
#include "docclipproject.h"
#include "documentbasenode.h"
#include "effectlistdialog.h"
#include "effectparamdialog.h"
#include "effectstackdialog.h"
#include "clippropertiesdialog.h"
#include "exportdialog.h"
#include "kaddclipcommand.h"
#include "kaddeffectcommand.h"
#include "kaddavfilecommand.h"
#include "kaddmarkercommand.h"
#include "kaddrefclipcommand.h"
#include "keditclipcommand.h"
#include "kdenlive.h"
#include "kdenlivedoc.h"
#include "kdenlivesetupdlg.h"
#include "documentmacrocommands.h"
#include "kmmmonitor.h"
#include "kmmtimeline.h"
#include "ktrackview.h"
#include "kmmeditpanel.h"
#include "kmmtrackkeyframepanel.h"
#include "kmmtracksoundpanel.h"
#include "kmmtrackvideopanel.h"
#include "kselectclipcommand.h"
#include "kprogress.h"
#include "krendermanager.h"
#include "krulertimemodel.h"
#include "projectlist.h"
#include "titlewidget.h"
#include "clipproperties.h"
#include "newproject.h"
#include "documentgroupnode.h"
#include "addtrackdialog_ui.h"
#include "createcolorclip_ui.h"
#include "createslideshowclip.h"
#include "createimageclip_ui.h"
#include "kaddtransitioncommand.h"
#include "docclipvirtual.h"

#include "trackpanelclipmovefunction.h"
#include "trackpanelrazorfunction.h"
#include "trackpanelspacerfunction.h"
#include "trackpanelclipresizefunction.h"
#include "trackpanelcliprollfunction.h"
#include "trackpanelmarkerfunction.h"
#include "trackpanelselectnonefunction.h"
#include "trackpanelkeyframefunction.h"
#include "trackpaneltransitionresizefunction.h"
#include "trackpaneltransitionmovefunction.h"



#define ID_STATUS_MSG 1
#define ID_EDITMODE_MSG 2
#define ID_TIMELINE_MSG 3
#define ID_CURTIME_MSG 4

namespace Gui {

    KdenliveApp::KdenliveApp(bool newDoc, QWidget *parent,
	const char *name):KDockMainWindow(parent, name), m_monitorManager(this),
    m_workspaceMonitor(NULL), m_captureMonitor(NULL), m_exportWidget(NULL), m_selectedFile(NULL), m_copiedClip(NULL), m_renderManager(NULL), m_doc(NULL), m_effectStackDialog(NULL), m_clipMonitor(NULL), m_projectList(NULL), m_effectListDialog(NULL), m_projectFormat(PAL_VIDEO), m_timelinePopupMenu(NULL), m_rulerPopupMenu(NULL), m_exportDvd(NULL) {
	config = kapp->config();
	QString newProjectName;
	videoProjectFormats << i18n("PAL (720x576, 25fps)") << i18n("PAL 16:9 (720x576, 25fps)");
	videoProjectFormats << i18n("NTSC (720x480, 30fps)") << i18n("NTSC 16:9 (720x480, 30fps)");

	// HDV not implemented in MLT yet...
	//newProjectDialog->video_format->insertItem(i18n("HDV-1080 (1440x1080, 25fps)"));
	//newProjectDialog->video_format->insertItem(i18n("HDV-720 (1280x720, 25fps)"));

	initStatusBar();

	int audioTracks = KdenliveSettings::audiotracks();
	int videoTracks = KdenliveSettings::videotracks();

	if (!KdenliveSettings::openlast() && !KdenliveSettings::openblank() && !newDoc) {
		slotNewProject(&newProjectName, &m_selectedFile, &videoTracks, &audioTracks);
	}
	else if (KdenliveSettings::openblank() && !newDoc) {
		slotNewProject(&newProjectName, &m_selectedFile, &videoTracks, &audioTracks, true);
	}

        QPixmap pixmap(locate("appdata", "graphics/kdenlive-splash.png"));

        if (KdenliveSettings::showsplash()) {
            splash = new KdenliveSplash(pixmap);
            splash->show();
            QTimer::singleShot(10*1000, this, SLOT(slotSplashTimeout()));
        }

	// renderer options
	m_renderManager = new KRenderManager(this);

	// call inits to invoke all other construction parts
	m_commandHistory = new KCommandHistory(actionCollection(), true);
	initActions();

	m_effectList.setAutoDelete(true);

	initEffects::initializeEffects( &m_effectList );

	// init effects menu
	audioEffectsMenu = ((QPopupMenu *) factory()->container("audio_effect", this));
	videoEffectsMenu = ((QPopupMenu *) factory()->container("video_effect", this));
	removeEffectsMenu = ((QPopupMenu *) factory()->container("remove_effect", this));
	QPtrListIterator < EffectDesc > itt(m_effectList);
	while (itt.current()) {
	    if (itt.current()->type() == "video") {
		videoEffectsMenu->insertItem( itt.current()->name() );
	    }
	    else audioEffectsMenu->insertItem( itt.current()->name() );
	    ++itt;
	}

	connect(audioEffectsMenu, SIGNAL(activated(int)), this, SLOT(slotAddAudioEffect(int)));
	connect(videoEffectsMenu, SIGNAL(activated(int)), this, SLOT(slotAddVideoEffect(int)));
	connect(removeEffectsMenu, SIGNAL(activated(int)), this, SLOT(slotRemoveEffect(int)));

	initDocument(videoTracks, audioTracks);

	initWidgets();
	readOptions();

	// disable actions at startup
	fileSave->setEnabled(false);
	//  filePrint->setEnabled(false);
	editCut->setEnabled(true);
	editCopy->setEnabled(true);
	editPaste->setEnabled(false);

	fileSaveAs->setEnabled(true);

	m_autoSaveTimer = new QTimer(this);
	connect(m_autoSaveTimer, SIGNAL(timeout()), this, SLOT(slotAutoSave()));
	if (KdenliveSettings::autosave())
	    m_autoSaveTimer->start(KdenliveSettings::autosavetime() * 60000, false);
        
	// Reopen last project if user asked it
	if (KdenliveSettings::openlast()) openLastFile();
        else if (!m_selectedFile.isEmpty()) openSelectedFile();
	else if (!newDoc || KdenliveSettings::openblank()) {
	    initView();
	    setCaption(newProjectName + ".kdenlive" + " - " + easyName(m_projectFormat), false);
	    m_doc->setProjectName( newProjectName + ".kdenlive");
	    if (KdenliveSettings::showsplash()) slotSplashTimeout();
	}

	connect(manager(), SIGNAL(change()), this, SLOT(slotUpdateLayoutState()));
	setAutoSaveSettings();
    }
    
    
    void KdenliveApp::slotSplashTimeout()
    {
        delete splash;
        splash = 0L;
    }
    
    
    KdenliveApp::~KdenliveApp() {
        if (splash) delete splash;
        if (m_renderManager) delete m_renderManager;
        delete m_transitionPanel;
        delete m_effectStackDialog;
        delete m_projectList;
        delete m_effectListDialog;
        //delete m_clipPropertyDialog;
        if (m_workspaceMonitor) delete m_workspaceMonitor;
        if (m_clipMonitor) delete m_clipMonitor;
        if (m_captureMonitor) delete m_captureMonitor;
        if (m_timeline) delete m_timeline;
        if (m_dockClipMonitor) delete m_dockClipMonitor;
        if (m_dockWorkspaceMonitor) delete m_dockWorkspaceMonitor;
        if (m_dockEffectList) delete m_dockEffectList;
        if (m_dockProjectList) delete m_dockProjectList;
        if (m_dockEffectStack) delete m_dockEffectStack;
        if (m_dockTransition) delete m_dockTransition;
	if (m_commandHistory) delete m_commandHistory;
    }
    
    const EffectDescriptionList & KdenliveApp::effectList() const
    {
	return m_effectList;
    }

    void KdenliveApp::slotAddEffect(const QString & effectName)
    {
	DocClipRef *clip = getDocument()->projectClip().selectedClip();
	if (!clip) return;
	Effect *effect = effectList().effectDescription(effectName)->createEffect(effectName);
	addCommand(Command::KAddEffectCommand::insertEffect(getDocument(), clip, clip->numEffects(), effect));
	m_effectStackDialog->slotSetEffectStack(clip);
	makeDockVisible(m_dockEffectStack);
	if (effectName == i18n("Freeze")) getDocument()->emitCurrentClipPosition();
	else getDocument()->activateSceneListGeneration(true);
    }

    void KdenliveApp::slotRemoveEffect(int ix)
    {
	kdDebug()<<"*********** REMOVING EFFECT AT: "<<ix<<endl;
	
	DocClipRef *clip = getDocument()->projectClip().selectedClip();
	if (!clip) return;
	kdDebug()<<"*********** LIST: "<<clip->clipEffectNames()<<endl;
	addCommand(Command::KAddEffectCommand::removeEffect(getDocument(), clip, ix));
	getDocument()->activateSceneListGeneration(true);
    }

    void KdenliveApp::slotAddVideoEffect(int ix)
    {
	DocClipRef *clip = getDocument()->projectClip().selectedClip();
	if (!clip) return;
	QString effectName = videoEffectsMenu->text(ix);
	Effect *effect = effectList().effectDescription(effectName)->createEffect(effectName);
	addCommand(Command::KAddEffectCommand::insertEffect(getDocument(), clip, clip->numEffects(), effect));
	m_effectStackDialog->slotSetEffectStack(clip);
	makeDockVisible(m_dockEffectStack);
	if (effectName == i18n("Freeze")) getDocument()->emitCurrentClipPosition();
	else getDocument()->activateSceneListGeneration(true);
    }

    void KdenliveApp::slotAddAudioEffect(int ix)
    {
	DocClipRef *clip = getDocument()->projectClip().selectedClip();
	if (!clip) return;
	QString effectName = audioEffectsMenu->text(ix);
	Effect *effect = effectList().effectDescription(effectName)->createEffect(effectName);
	addCommand(Command::KAddEffectCommand::insertEffect(getDocument(), clip, clip->numEffects(), effect));
	m_effectStackDialog->slotSetEffectStack(clip);
	makeDockVisible(m_dockEffectStack);
	getDocument()->activateSceneListGeneration(true);
    }

    void KdenliveApp::slotAutoSave()
    {
	if (!m_doc->isModified()) return;
	slotFileSave();
	
    }

    void KdenliveApp::openSelectedFile()
    {
        openDocumentFile(m_selectedFile);
        slotSplashTimeout();
    }

    void KdenliveApp::openLastFile()
    {
        config->setGroup("RecentFiles");
        QString Lastproject = config->readPathEntry("File1");
        if (!Lastproject.isEmpty())
            openDocumentFile(KURL(Lastproject));
        slotSplashTimeout();
    }

    void KdenliveApp::initActions() {

	setStandardToolBarMenuEnabled(true);
	createStandardStatusBarAction();

	actionCollection()->setHighlightingEnabled(true);
 	connect(actionCollection(), SIGNAL( actionStatusText( const QString & ) ),
           this, SLOT( slotStatusMsg( const QString & ) ) );
 	connect(actionCollection(), SIGNAL( clearStatusText() ),
           statusBar(), SLOT( clear() ) );

	fileNew =
	    KStdAction::openNew(this, SLOT(slotFileNew()),
	    actionCollection());
	fileOpen =
	    KStdAction::open(this, SLOT(slotFileOpen()),
	    actionCollection());
	fileOpenRecent =
	    KStdAction::openRecent(this,
	    SLOT(openDocumentFile(const KURL &)), actionCollection());
	fileSave =
	    KStdAction::save(this, SLOT(slotFileSave()),
	    actionCollection());
	fileSaveAs =
	    KStdAction::saveAs(this, SLOT(slotFileSaveAs()),
	    actionCollection());
	//fileClose = KStdAction::close(this, SLOT(slotFileClose()), actionCollection());
	//  filePrint = KStdAction::print(this, SLOT(slotFilePrint()), actionCollection());
	fileQuit =
	    KStdAction::quit(this, SLOT(slotFileQuit()),
	    actionCollection());
	editCut =
	    KStdAction::cut(this, SLOT(slotEditCut()), actionCollection());
	editCopy =
	    KStdAction::copy(this, SLOT(slotEditCopy()),
	    actionCollection());

	editPaste = KStdAction::paste(this, SLOT(slotEditPaste()),
	    actionCollection());

	editPaste = KStdAction::paste(this, SLOT(slotEditPaste()),
	    actionCollection());

	fullScreen = KStdAction::fullScreen(this, SLOT(slotFullScreen()),
	    actionCollection(), this);

	optionsPreferences =
	    KStdAction::preferences(this, SLOT(slotOptionsPreferences()),
	    actionCollection());
	KStdAction::keyBindings(this, SLOT(slotConfKeys()),
	    actionCollection());
	KStdAction::configureNotifications(this, SLOT(slotConfNotifications()),
	    actionCollection());
	configureToolbars =
	    KStdAction::configureToolbars(this, SLOT(slotConfToolbars()),
	    actionCollection());
	fitToWidth =
	    KStdAction::fitToWidth(this, SLOT(slotFitToWidth()),
	    actionCollection());
	fitToWidth->setStatusText(i18n("Zoom to display the whole project"));

	KAction *zoomIn = KStdAction::zoomIn(this, SLOT(slotZoomIn()),
	    actionCollection());
	zoomIn->setStatusText(i18n("Zoom in"));

	KAction *zoomOut = KStdAction::zoomOut(this, SLOT(slotZoomOut()),
	    actionCollection());
	zoomOut->setStatusText(i18n("Zoom out"));

        KAction *zoomRestore = new KAction(i18n("Restore Last Zoom Level"), 0, this,
        SLOT(slotRestoreZoom()), actionCollection(),
        "restore_zoom");
	zoomRestore->setStatusText(i18n("Restoring previous zoom level"));

	timelineMoveTool =
	    new KRadioAction(i18n("Move/Resize Tool"), "moveresize.png",
	    KShortcut(Qt::Key_Q), this, SLOT(slotTimelineMoveTool()),
	    actionCollection(), "timeline_move_tool");
	timelineRazorTool =
	    new KRadioAction(i18n("Razor Tool"), "razor.png",
	    KShortcut(Qt::Key_W), this, SLOT(slotTimelineRazorTool()),
	    actionCollection(), "timeline_razor_tool");
	timelineSpacerTool =
	    new KRadioAction(i18n("Spacing Tool"), "spacer.png",
	    KShortcut(Qt::Key_E), this, SLOT(slotTimelineSpacerTool()),
	    actionCollection(), "timeline_spacer_tool");
	timelineMarkerTool =
	    new KRadioAction(i18n("Marker Tool"), "marker.png",
	    KShortcut(Qt::Key_M), this, SLOT(slotTimelineMarkerTool()),
	    actionCollection(), "timeline_marker_tool");
	timelineRollTool =
	    new KRadioAction(i18n("Roll Tool"), "rolltool.png",
	    KShortcut(Qt::Key_R), this, SLOT(slotTimelineRollTool()),
	    actionCollection(), "timeline_roll_tool");

	timelineSnapToFrame =
	    new KToggleAction(i18n("Snap To Frames"), "snaptoframe.png", 0,
	    this, SLOT(slotTimelineSnapToFrame()), actionCollection(),
	    "timeline_snap_frame");
	timelineSnapToBorder =
	    new KToggleAction(i18n("Snap To Border"), "snaptoborder.png",
	    0, this, SLOT(slotTimelineSnapToBorder()), actionCollection(),
	    "timeline_snap_border");
	timelineSnapToMarker =
	    new KToggleAction(i18n("Snap To Marker"), "snaptomarker.png",
	    0, this, SLOT(slotTimelineSnapToMarker()), actionCollection(),
	    "timeline_snap_marker");

	KAction *gotoStart = new KAction(i18n("Go To Beginning"), KStdAccel::home(), this, SLOT(slotGotoStart()), actionCollection(), "timeline_go_start");
	gotoStart->setStatusText(i18n("Beginning of project"));

	KAction *gotoEnd = new KAction(i18n("Go To End"), KStdAccel::end(), this, SLOT(slotGotoEnd()), actionCollection(), "timeline_go_end");
	gotoEnd->setStatusText(i18n("End of project"));

	projectAddClips =
	    new KAction(i18n("Add Clips"), "addclips.png", 0, this,
	    SLOT(slotProjectAddClips()), actionCollection(),
	    "project_add_clip");

	projectAddColorClip =
	    new KAction(i18n("Create Color Clip"), "addclips.png", 0, this,
	    SLOT(slotProjectAddColorClip()), actionCollection(),
	    "project_add_color_clip");

	projectAddImageClip =
	    new KAction(i18n("Create Image Clip"), "addclips.png", 0, this,
	    SLOT(slotProjectAddImageClip()), actionCollection(),
	    "project_add_image_clip");

	(void) new KAction(i18n("Create Slideshow Clip"), "addclips.png", 0, this,
	    SLOT(slotProjectAddSlideshowClip()), actionCollection(),
	    "project_add_slideshow_clip");

	projectAddTextClip =
	    new KAction(i18n("Create Text Clip"), "addclips.png", 0, this,
	    SLOT(slotProjectAddTextClip()), actionCollection(),
	    "project_add_text_clip");

	(void) new KAction(i18n("Duplicate Text Clip"), "addclips.png", 0, this,
	    SLOT(slotProjectDuplicateTextClip()), actionCollection(),
	    "project_duplicate_text_clip");
	

	projectDeleteClips =
	    new KAction(i18n("Delete Clip"), "editdelete.png", 0, this,
	    SLOT(slotProjectDeleteClips()), actionCollection(),
	    "project_delete_clip");
	projectClean =
	    new KAction(i18n("Clean Project"), "cleanproject.png", 0, this,
	    SLOT(slotProjectClean()), actionCollection(), "project_clean");
/*	projectClipProperties =
	    new KAction(i18n("Clip properties"), "clipproperties.png", 0,
	    this, SLOT(slotProjectClipProperties()), actionCollection(),
	    "project_clip_properties");*/
        

	renderExportTimeline =
	    new KAction(i18n("&Export Timeline"), "exportvideo.png", 0,
	    this, SLOT(slotRenderExportTimeline()), actionCollection(),
	    "render_export_timeline");

	KAction *renderDvd = new KAction(i18n("Export to DVD"), "dvd_unmount.png", 0, this,
	    SLOT(slotRenderDvd()), actionCollection(), "render_dvd");
	renderDvd->setStatusText(i18n("Generating DVD files"));

	configureProject =
	    new KAction(i18n("&Configure Project"), "configureproject.png",
	    0, this, SLOT(slotConfigureProject()), actionCollection(),
	    "configure_project");

	actionTogglePlay =
	    new KAction(i18n("Play/Pause"), KShortcut(Qt::Key_Space), this,
	    SLOT(slotPlay()), actionCollection(), "toggle_play");

	actionStopPlay =
	    new KAction(i18n("Stop"), 0, this,
	    SLOT(slotStop()), actionCollection(), "stop_clip");
	actionStopPlay->setStatusText(i18n("Stop playing"));

	actionTogglePlaySelected =
	    new KAction(i18n("Play Selection"),
	    KShortcut(Qt::Key_Space | Qt::CTRL), this,
	    SLOT(slotTogglePlaySelected()), actionCollection(),
	    "toggle_play_selection");
	actionTogglePlaySelected->setStatusText(i18n("Play selection"));

	KAction *playPause = new KAction(i18n("Play/Pause"), KShortcut(Qt::Key_K), this,
	    SLOT(slotPlay()), actionCollection(), "play_clip");
	playPause->setStatusText(i18n("Play or pause"));

	KAction *playFwd = new KAction(i18n("Play forward"), KShortcut(Qt::Key_L), this,
	    SLOT(slotToggleForwards()), actionCollection(), "toggle_forwards");
	playFwd->setStatusText(i18n("Fast forwards playing (click several times for faster playing)"));

	KAction *playBack = new KAction(i18n("Play backward"), KShortcut(Qt::Key_J), this,
	    SLOT(slotToggleBackwards()), actionCollection(), "toggle_backwards");
	playBack->setStatusText(i18n("Fast backwards playing (click several times for faster playing)"));

	KAction *playLoop = new KAction(i18n("Loop selected zone"), 0, this,
	    SLOT(slotLoopPlay()), actionCollection(), "play_loop");
	playLoop->setStatusText(i18n("Play selected zone in loop"));

	KAction *splitAudio = new KAction(i18n("Split Audio From Selected Clip"), 0, this,
	    SLOT(slotSplitAudio()), actionCollection(), "split_audio");
	splitAudio->setStatusText(i18n("Split Audio From Selected Clip"));



	actionNextFrame =
	    new KAction(i18n("Forward one frame"),
	    KShortcut(Qt::Key_Right), this, SLOT(slotNextFrame()),
	    actionCollection(), "forward_frame");
	actionLastFrame =
	    new KAction(i18n("Back one frame"), KShortcut(Qt::Key_Left),
	    this, SLOT(slotLastFrame()), actionCollection(),
	    "backward_frame");
        actionNextSecond =
                new KAction(i18n("Forward one second"),
                            KShortcut(Qt::CTRL | Qt::Key_Right), this, SLOT(slotNextSecond()),
                            actionCollection(), "forward_second");
	actionNextSecond->setStatusText(i18n("Move cursor forward one second"));
        actionLastSecond =
                new KAction(i18n("Back one second"), KShortcut(Qt::CTRL | Qt::Key_Left),
                            this, SLOT(slotLastSecond()), actionCollection(),
                            "backward_second");
	actionLastSecond->setStatusText(i18n("Move cursor backwards one second"));

        KAction *nextSnap = new KAction(i18n("Forward to next snap point"),
                            KShortcut(Qt::ALT | Qt::Key_Right), this, SLOT(slotNextSnap()),
                            actionCollection(), "forward_snap");
	nextSnap->setStatusText(i18n("Move cursor to next snap point"));

        KAction *prevSnap = new KAction(i18n("Rewind to previous snap point"),
                            KShortcut(Qt::ALT | Qt::Key_Left), this, SLOT(slotPreviousSnap()),
                            actionCollection(), "backward_snap");
	prevSnap->setStatusText(i18n("Move cursor to previous snap point"));

	actionSetInpoint =
	    new KAction(i18n("Set Inpoint"), KShortcut(Qt::Key_I), this,
	    SLOT(slotSetInpoint()), actionCollection(), "set_inpoint");
	actionSetOutpoint =
	    new KAction(i18n("Set Outpoint"), KShortcut(Qt::Key_O), this,
	    SLOT(slotSetOutpoint()), actionCollection(), "set_outpoint");
	actionDeleteSelected =
	    new KAction(i18n("Delete Clip"),
	    KShortcut(Qt::Key_Delete), this, SLOT(slotDeleteSelected()),
	    actionCollection(), "delete_selected_clips");

	actionToggleSnapMarker =
	    new KAction(i18n("Toggle Snap Marker"),
	    KShortcut(Qt::Key_Period), this, SLOT(slotToggleSnapMarker()),
	    actionCollection(), "toggle_snap_marker");
	actionClearAllSnapMarkers =
	    new KAction(i18n("Clear All Snap Markers"), KShortcut(), this,
	    SLOT(slotClearAllSnapMarkers()), actionCollection(),
	    "clear_all_snap_markers");
	actionClearSnapMarkersFromSelected =
	    new KAction(i18n("Clear Snap Markers From Selected"),
	    KShortcut(), this, SLOT(slotClearSnapMarkersFromSelected()),
	    actionCollection(), "clear_snap_markers_from_selected");

	actionLoadLayout1 =
	    new KAction(i18n("Load Layout &1"), "loadlayout1.png",
	    KShortcut(Qt::Key_F9), this, SLOT(loadLayout1()),
	    actionCollection(), "load_layout_1");
	actionLoadLayout2 =
	    new KAction(i18n("Load Layout &2"), "loadlayout2.png",
	    KShortcut(Qt::Key_F10), this, SLOT(loadLayout2()),
	    actionCollection(), "load_layout_2");
	actionLoadLayout3 =
	    new KAction(i18n("Load Layout &3"), "loadlayout3.png",
	    KShortcut(Qt::Key_F11), this, SLOT(loadLayout3()),
	    actionCollection(), "load_layout_3");
	actionLoadLayout4 =
	    new KAction(i18n("Load Layout &4"), "loadlayout4.png",
	    KShortcut(Qt::Key_F12), this, SLOT(loadLayout4()),
	    actionCollection(), "load_layout_4");
	actionSaveLayout1 =
	    new KAction(i18n("Save Layout &1"),
	    KShortcut(Qt::Key_F9 | Qt::CTRL | Qt::SHIFT), this,
	    SLOT(saveLayout1()), actionCollection(), "save_layout_1");
	actionSaveLayout2 =
	    new KAction(i18n("Save Layout &2"),
	    KShortcut(Qt::Key_F10 | Qt::CTRL | Qt::SHIFT), this,
	    SLOT(saveLayout2()), actionCollection(), "save_layout_2");
	actionSaveLayout3 =
	    new KAction(i18n("Save Layout &3"),
	    KShortcut(Qt::Key_F11 | Qt::CTRL | Qt::SHIFT), this,
	    SLOT(saveLayout3()), actionCollection(), "save_layout_3");
	actionSaveLayout4 =
	    new KAction(i18n("Save Layout &4"),
	    KShortcut(Qt::Key_F12 | Qt::CTRL | Qt::SHIFT), this,
	    SLOT(saveLayout4()), actionCollection(), "save_layout_4");

	timelineRazorAllClips =
	    new KAction(i18n("Razor All Clips"),
	    KShortcut(Qt::Key_W | Qt::CTRL | Qt::SHIFT), this,
	    SLOT(slotRazorAllClips()), actionCollection(),
	    "razor_all_clips");
	timelineRazorSelectedClips =
	    new KAction(i18n("Razor Clip"),
	    KShortcut(Qt::Key_W | Qt::SHIFT), this,
	    SLOT(slotRazorSelectedClips()), actionCollection(),
	    "razor_selected_clips");

	KAction *addFolder = new KAction(i18n("Create Folder"), "folder_new.png", 0, this,
	    SLOT(slotProjectAddFolder()), actionCollection(),
	    "create_folder");
	addFolder->setStatusText(i18n("Add folder"));

	KAction *renameFolder = new KAction(i18n("Rename Folder"), 0, this,
	    SLOT(slotProjectRenameFolder()), actionCollection(),
	    "rename_folder");
	renameFolder->setStatusText(i18n("Rename folder"));

	KAction *deleteFolder = new KAction(i18n("Delete Folder"), "editdelete.png", 0, this,
	    SLOT(slotProjectDeleteFolder()), actionCollection(),
	    "delete_folder");
	deleteFolder->setStatusText(i18n("Delete folder"));

        KAction *addTransition = new KAction(i18n("Add Transition"), 0, this,
        SLOT(slotAddTransition()), actionCollection(),
        "add_transition");
	addTransition->setStatusText(i18n("Add transition to selected clip"));

        KAction *deleteTransition = new KAction(i18n("Delete Transition"), 0, this,
        SLOT(slotDeleteTransition()), actionCollection(),
        "del_transition");
	deleteTransition->setStatusText(i18n("Delete transition from selected clip"));

        KAction *addTrack = new KAction(i18n("Add Track"), 0, this,
        SLOT(slotAddTrack()), actionCollection(),
        "timeline_add_track");
	addTrack->setStatusText(i18n("Add track"));

	KAction *deleteTrack = new KAction(i18n("Delete Track"), 0, this,
        SLOT(slotDeleteTrack()), actionCollection(),
        "timeline_delete_track");
	deleteTrack->setStatusText(i18n("Delete track"));

	KAction *externalAudio = new KAction(i18n("Open Clip In External Editor"), 0, this,
        SLOT(slotExternalEditor()), actionCollection(),
        "external_audio");
	externalAudio->setStatusText(i18n("Open clip in an external editor"));

	KAction *saveZone = new KAction(i18n("Save Selected Zone"), 0, this,
        SLOT(slotSaveZone()), actionCollection(),
        "save_zone");
	saveZone->setStatusText(i18n("Save selected zone as playlist for future use"));

	KAction *renderZone = new KAction(i18n("Render Selected Zone"), 0, this,
        SLOT(slotRenderZone()), actionCollection(),
        "render_zone");
	renderZone->setStatusText(i18n("Render selected zone for future use"));

	KAction *virtualZone = new KAction(i18n("Create Virtual Clip"), 0, this,
        SLOT(slotVirtualZone()), actionCollection(),
        "virtual_zone");
	virtualZone->setStatusText(i18n("Create virtual clip from selected zone"));

	KAction *showVirtualZone = new KAction(i18n("Go To Virtual Zone"), 0, this,
        SLOT(slotShowVirtualZone()), actionCollection(),
        "show_virtual_zone");
	showVirtualZone->setStatusText(i18n("Go to selected clip's virtual zone"));

	KAction *addGuide = new KAction(i18n("Add Guide"), 0, this,
        SLOT(slotAddGuide()), actionCollection(),
        "timeline_add_guide");
	addGuide->setStatusText(i18n("Add guide at cursor position"));

	KAction *deleteGuide = new KAction(i18n("Delete Guide"), 0, this,
        SLOT(slotDeleteGuide()), actionCollection(),
        "timeline_delete_guide");
	deleteGuide->setStatusText(i18n("Delete guide at cursor position"));

	KAction *editGuide = new KAction(i18n("Edit Guide"), 0, this,
        SLOT(slotEditGuide()), actionCollection(),
        "timeline_edit_guide");
	editGuide->setStatusText(i18n("Edit guide at cursor position"));

        showClipMonitor = new KToggleAction(i18n("Clip Monitor"), 0, this,
	    SLOT(slotToggleClipMonitor()), actionCollection(),
	    "toggle_clip_monitor");
	showWorkspaceMonitor = new KToggleAction(i18n("Timeline Monitor"), 0, this,
	    SLOT(slotToggleWorkspaceMonitor()), actionCollection(),
	    "toggle_workspace_monitor");
        showEffectList = new KToggleAction(i18n("Effect List"), 0, this,
	    SLOT(slotToggleEffectList()), actionCollection(),
	    "toggle_effect_list");
        showEffectStack = new KToggleAction(i18n("Effect Stack"), 0, this,
	    SLOT(slotToggleEffectStack()), actionCollection(),
	    "toggle_effect_stack");
        showProjectList = new KToggleAction(i18n("Project Tree"), 0, this,
	    SLOT(slotToggleProjectList()), actionCollection(),
	    "toggle_project_list");
        showTransitions = new KToggleAction(i18n("Transitions"), 0, this,
        SLOT(slotToggleTransitions()), actionCollection(),
        "toggle_transitions");

        (void) new KAction(i18n("Focus Clip Monitor"), 0, this,
	    SLOT(slotFocusClipMonitor()), actionCollection(),
	    "focus_clip_monitor");
	(void) new KAction(i18n("Focus Timeline Monitor"), 0, this,
	    SLOT(slotFocusWorkspaceMonitor()), actionCollection(),
	    "focus_workspace_monitor");
        (void) new KAction(i18n("Focus Effect List"), 0, this,
	    SLOT(slotFocusEffectList()), actionCollection(),
	    "focus_effect_list");
        (void) new KAction(i18n("Focus Effect Stack"), 0, this,
	    SLOT(slotFocusEffectStack()), actionCollection(),
	    "focus_effect_stack");
        (void) new KAction(i18n("Focus Project Tree"), 0, this,
	    SLOT(slotFocusProjectList()), actionCollection(),
	    "focus_project_list");

        (void) new KAction(i18n("Focus Transitions"), 0, this,
        SLOT(slotFocusTransitions()), actionCollection(),
        "focus_transitions");

        (void) new KAction(i18n("Move Clip To Cursor"), KShortcut(Qt::SHIFT | Qt::Key_Return), this,
        SLOT(slotMoveClipToCurrentTime()), actionCollection(),
        "move_current");

        (void) new KAction(i18n("Move Clip Up"), KShortcut(Qt::SHIFT | Qt::Key_Up), this,
        SLOT(slotMoveClipUp()), actionCollection(),
        "move_up");

        (void) new KAction(i18n("Move Clip Down"), KShortcut(Qt::SHIFT | Qt::Key_Down), this,
        SLOT(slotMoveClipDown()), actionCollection(),
        "move_down");

        (void) new KAction(i18n("Resize Clip Start To Current Time"), KShortcut(Qt::SHIFT | Qt::Key_Left), this,
        SLOT(slotResizeClipStart()), actionCollection(),
        "resize_start");

        (void) new KAction(i18n("Resize Clip End To Current Time"), KShortcut(Qt::SHIFT | Qt::Key_Right), this,
        SLOT(slotResizeClipEnd()), actionCollection(),
        "resize_end");

        (void) new KAction(i18n("Select Next Track"), KShortcut(Qt::Key_Down), this,
        SLOT(slotSelectNextTrack()), actionCollection(),
        "next_track");

        (void) new KAction(i18n("Select Previous Track"), KShortcut(Qt::Key_Up), this,
        SLOT(slotSelectPreviousTrack()), actionCollection(),
        "prev_track");

        (void) new KAction(i18n("Select Clip Under Cursor"), 0, this,
        SLOT(selectClipUnderCursor()), actionCollection(),
        "select_current");

	clipAutoSelect = new KToggleAction(i18n("Clip Auto Selection"), 0, 0,
	    this, SLOT(slotTimelineSnapToFrame()), actionCollection(),
	    "clip_auto_select");

        KAction *editClip = new KAction(i18n("Edit Clip Properties"), 0, this,
        SLOT(slotProjectEditClip()), actionCollection(),
        "edit_clip");
	editClip->setStatusText(i18n("Edit Clip Properties"));
        
        KAction *editParentClip = new KAction(i18n("Clip properties"), 0, this,
        SLOT(slotProjectEditParentClip()), actionCollection(),
        "project_edit_clip");
	editParentClip->setStatusText(i18n("Edit clip properties"));
        
        KAction *exportCurrentFrame = new KAction(i18n("Export Current Frame"), 0, this,
        SLOT(slotExportCurrentFrame()), actionCollection(),
        "export_current_frame");
	exportCurrentFrame->setStatusText(i18n("Save current frame as image file"));

        KAction *viewSelectedClip = new KAction(i18n("Play Clip"), 0, this,
        SLOT(slotViewSelectedClip()), actionCollection(),
        "view_selected_clip");
        viewSelectedClip->setStatusText(i18n("Play clip in clip monitor"));
        

	timelineMoveTool->setExclusiveGroup("timeline_tools");
	timelineRazorTool->setExclusiveGroup("timeline_tools");
	timelineSpacerTool->setExclusiveGroup("timeline_tools");
	timelineMarkerTool->setExclusiveGroup("timeline_tools");
	timelineRollTool->setExclusiveGroup("timeline_tools");

	fileNew->setStatusText(i18n("Create a new project"));
	fileOpen->setStatusText(i18n("Open a project"));
	fileOpenRecent->setStatusText(i18n("Open a recent file"));
	fileSave->setStatusText(i18n("Save project"));
	fileSaveAs->setStatusText(i18n("Save project as..."));
	//  fileClose->setStatusText(i18n("Close document"));
	//  filePrint ->setStatusText(i18n("Print document"));
	fileQuit->setStatusText(i18n("Exit application"));
	editCut->
	    setStatusText(i18n
	    ("Cut and move selected zone to clipboard"));
	editCopy->
	    setStatusText(i18n
	    ("Copy clip to clipboard"));
	editPaste->
	    setStatusText(i18n
	    ("Paste clipboard contents to cursor position"));
	timelineMoveTool->
	    setStatusText(i18n("Move and resize clips"));
	timelineRazorTool->
	    setStatusText(i18n("Split Clip"));
	timelineSpacerTool->
	    setStatusText(i18n("Shift all clips to the right of mouse"));
	timelineMarkerTool->
	    setStatusText(i18n
	    ("Insert commented snap markers on clips (Ctrl + click to remove a marker)"));
	timelineRollTool->
	    setStatusText(i18n
	    ("Move edit point between two selected clips"));
	timelineSnapToFrame->
	    setStatusText(i18n("Align clips on nearest frame"));
	timelineSnapToBorder->
	    setStatusText(i18n
	    ("Align clips on nearest clip borders"));
	timelineSnapToMarker->
	    setStatusText(i18n
	    ("Align clips on snap markers"));
	projectAddClips->setStatusText(i18n("Add clips"));
	projectDeleteClips->
	    setStatusText(i18n("Remove clip"));
	projectClean->
	    setStatusText(i18n("Remove unused clips"));
	//projectClipProperties->setStatusText( i18n( "View the properties of the selected clip" ) );
	actionTogglePlay->setStatusText(i18n("Start or stop playback"));
	actionTogglePlay->
	    setStatusText(i18n
	    ("Start or stop playback of inpoint/outpoint selection"));
	actionNextFrame->
	    setStatusText(i18n
	    ("Move forwards one frame"));
	actionLastFrame->
	    setStatusText(i18n
	    ("Move backwards one frame"));
	actionSetInpoint->
	    setStatusText(i18n("Set inpoint"));
	actionSetOutpoint->
	    setStatusText(i18n
	    ("Set outpoint"));
	actionDeleteSelected->
	    setStatusText(i18n("Delete clip"));

	actionToggleSnapMarker->
	    setStatusText(i18n
	    ("Toggle a snap marker at the current monitor position"));
	actionClearAllSnapMarkers->
	    setStatusText(i18n
	    ("Remove all snap markers from project"));
	actionClearSnapMarkersFromSelected->
	    setStatusText(i18n
	    ("Remove snap markers from selected clips"));

	renderExportTimeline->
	    setStatusText(i18n("Render timeline to a file"));
	configureProject->
	    setStatusText(i18n("Configure project"));

	timelineRazorAllClips->
	    setStatusText(i18n
	    ("Razor all clips at cursor position"));
	timelineRazorSelectedClips->
	    setStatusText(i18n
	    ("Razor clip at cursor position"));

	// use the absolute path to your kdenliveui.rc file for testing purpose in createGUI();
	createGUI("kdenliveui.rc");

	timelineMoveTool->setChecked(true);
	timelineSnapToBorder->setChecked(true);
	timelineSnapToFrame->setChecked(true);
	timelineSnapToMarker->setChecked(true);

    }


    void KdenliveApp::initStatusBar() {
	///////////////////////////////////////////////////////////////////
	// STATUSBAR
	// TODO: add your own items you need for displaying current application status.

	statusBar()->insertItem(i18n("Ready."), ID_STATUS_MSG);

	m_statusBarProgress = new KProgress(statusBar());
	m_statusBarProgress->setMaximumWidth(100);
	m_statusBarProgress->setTotalSteps(0);
	//m_statusBarProgress->setTextEnabled(false);
	statusBar()->addWidget(m_statusBarProgress);
	m_statusBarProgress->hide();

	m_statusBarExportProgress = new KProgress(statusBar());
	m_statusBarExportProgress->setMaximumWidth(100);
	m_statusBarExportProgress->setTotalSteps(100);
	//m_statusBarExportProgress->setTextEnabled(false);
	statusBar()->addWidget(m_statusBarExportProgress);
	m_statusBarExportProgress->hide();

	statusBar()->insertItem(i18n("Move/Resize mode"), ID_EDITMODE_MSG,
	    0, true);
	statusBar()->insertItem(QString::null, ID_TIMELINE_MSG,
	    0, true);
	statusBar()->insertItem(i18n("Current Time : ") + "00:00:00.00",
	    ID_CURTIME_MSG, 0, true);
    }

    void KdenliveApp::initDocument(int vtracks, int atracks) {
	if (m_doc) delete m_doc;
	renderManager()->resetRenderers();
        m_doc = new KdenliveDoc(KdenliveSettings::defaultfps(), KdenliveSettings::defaultwidth(), KdenliveSettings::defaultheight(), this, this);
	m_doc->newDocument(vtracks, atracks);
	connect(m_doc, SIGNAL(modified(bool)), this, SLOT(documentModified(bool)));
    }


    void KdenliveApp::initWidgets() {
	view = new QWidget(this);
        m_menuPosition = QPoint();
	KDockWidget *mainDock =
	    createDockWidget("Kdenlive", QPixmap(), this,
	    i18n("Kdenlive"));
	mainDock->setWidget(view);
	mainDock->setDockSite(KDockWidget::DockFullSite);
        mainDock->setEnableDocking(KDockWidget::DockNone);
	mainDock->setToolTipString(i18n("Kdenlive"));
        //mainDock->setFocusPolicy(QWidget::WheelFocus); //QWidget::TabFocus
	setCentralWidget(mainDock);
	setMainDockWidget(mainDock);
	//setCaption(m_doc->URL().fileName(), false);

	m_timelineWidget =
	    createDockWidget("TimeLine", QPixmap(), 0,
	    i18n("TimeLine"));
	m_timeline = new KMMTimeLine(NULL, m_timelineWidget);
	m_timelineWidget->setWidget(m_timeline);
	m_timelineWidget->setDockSite(KDockWidget::DockFullSite);
	m_timelineWidget->setDockSite(KDockWidget::DockCorner);
	m_timelineWidget->manualDock(mainDock, KDockWidget::DockBottom);
	m_timelineWidget->setToolTipString(i18n("TimeLine"));

	m_dockProjectList = createDockWidget("Project List", QPixmap(), 0, i18n("Project Tree"));
	QWhatsThis::add(m_dockProjectList,
	    i18n("Video files usable in your project. "
		"Add or remove files with the contextual menu. "
		"In order to add sequences to the current video project, use the drag and drop."));
	m_dockProjectList->setDockSite(KDockWidget::DockFullSite);
	m_dockProjectList->manualDock(mainDock, KDockWidget::DockLeft);
	m_dockProjectList->setToolTipString(i18n("Project Tree"));

	m_dockTransition = createDockWidget("transition", QPixmap(), 0, i18n("Transition"));
	m_transitionPanel = new TransitionDialog(this, m_dockTransition);
	m_dockTransition->setWidget(m_transitionPanel);
	m_dockTransition->setDockSite(KDockWidget::DockFullSite);
	m_dockTransition->manualDock(m_dockProjectList, KDockWidget::DockCenter);
	m_dockTransition->setToolTipString(i18n("Transition"));
	
	m_dockEffectList = createDockWidget("Effect List", QPixmap(), 0, i18n("Effect List"));
	QToolTip::add(m_dockEffectList,
	    i18n("Current effects usable with the renderer"));
	m_dockEffectList->setDockSite(KDockWidget::DockFullSite);
	m_dockEffectList->manualDock(m_dockProjectList, KDockWidget::DockCenter);
	m_dockEffectList->setToolTipString(i18n("Effect List"));

	m_dockEffectStack = createDockWidget("Effect Stack", QPixmap(), 0, i18n("Effect Stack"));
	m_dockEffectStack->setDockSite(KDockWidget::DockFullSite);
	m_dockEffectStack->manualDock(m_dockProjectList, KDockWidget::DockCenter);
	m_dockEffectStack->setToolTipString(i18n("Effect Stack"));

	m_dockWorkspaceMonitor = createDockWidget("Workspace Monitor", QPixmap(), 0, i18n("Timeline Monitor"));

	m_dockWorkspaceMonitor->setDockSite(KDockWidget::DockFullSite);
	m_dockWorkspaceMonitor->manualDock(mainDock, KDockWidget::DockRight);
	m_dockWorkspaceMonitor->setToolTipString(i18n("Timeline Monitor"));

	m_dockClipMonitor = createDockWidget("Clip Monitor", QPixmap(), 0, i18n("Clip Monitor"));
	m_dockClipMonitor->setDockSite(KDockWidget::DockFullSite);
	m_dockClipMonitor->manualDock( m_dockWorkspaceMonitor, KDockWidget::DockCenter );
	m_dockClipMonitor->setToolTipString(i18n("Clip Monitor"));

	m_dockCaptureMonitor = createDockWidget( "Capture Monitor", QPixmap(), 0, i18n( "Capture Monitor" ) );
	m_dockCaptureMonitor->setDockSite( KDockWidget::DockFullSite );
	m_dockCaptureMonitor->manualDock( m_dockWorkspaceMonitor, KDockWidget::DockCenter );

	setBackgroundMode(PaletteBase);
	makeDockInvisible(mainDock);
	readDockConfig(config, "Default Layout");


    }


    void KdenliveApp::initView() {
	////////////////////////////////////////////////////////////////////
	// create the main widget here that is managed by KTMainWindow's view-region and
	// connect the widget to your document to display document contents.
	kdDebug()<<"****************  INIT DOCUMENT VIEW ***************"<<endl;
	if (m_exportDvd) delete m_exportDvd;
	if (m_exportWidget) delete m_exportWidget;
	if (m_projectList) delete m_projectList;
	m_exportDvd = 0;
	m_exportWidget = 0;
	m_projectList = 0;
	m_projectList = new ProjectList(this, getDocument(), m_dockProjectList);
	m_dockProjectList->setWidget(m_projectList);
	m_projectList->slot_UpdateList();
	m_projectList->show();
	m_dockProjectList->update();

	if (m_effectListDialog) delete m_effectListDialog;
	m_effectListDialog =
	    new EffectListDialog(effectList(), m_timelineWidget, "effect list");
	m_dockEffectList->setWidget(m_effectListDialog);
	m_effectListDialog->show();
	m_dockEffectList->update();
	if (m_effectStackDialog) delete m_effectStackDialog;
	m_effectStackDialog =
	    new EffectStackDialog(this, getDocument(), m_dockEffectStack,
	    "effect stack");
//      QToolTip::add( m_dockEffectStack, i18n( "All effects on selected clip." ) );
	m_dockEffectStack->setWidget(m_effectStackDialog);
	m_effectStackDialog->show();
	m_dockEffectStack->update();

	m_monitorManager.deleteMonitors();


	if (m_workspaceMonitor) delete m_workspaceMonitor;
	m_workspaceMonitor = m_monitorManager.createMonitor(getDocument(), m_dockWorkspaceMonitor, "Document");

	m_workspaceMonitor->setNoSeek(true);
	m_dockWorkspaceMonitor->setWidget(m_workspaceMonitor);
        m_workspaceMonitor->editPanel()->showLcd(KdenliveSettings::showlcd());
	m_workspaceMonitor->show();
	m_dockWorkspaceMonitor->update();

	if (m_clipMonitor) delete m_clipMonitor;
	m_clipMonitor =
	    m_monitorManager.createMonitor(getDocument(),
	    m_dockClipMonitor, "Clip Monitor");
	m_dockClipMonitor->setWidget(m_clipMonitor);
	m_clipMonitor->editPanel()->showLcd(KdenliveSettings::showlcd());
	m_clipMonitor->show();
	m_dockClipMonitor->update();

        if (m_captureMonitor) delete m_captureMonitor;
	m_captureMonitor = m_monitorManager.createCaptureMonitor( getDocument(), m_dockCaptureMonitor, "Capture Monitor" );
	m_dockCaptureMonitor->setWidget( m_captureMonitor );
	m_captureMonitor->show();
	m_dockCaptureMonitor->update();

	connect(m_workspaceMonitor,
	    SIGNAL(seekPositionChanged(const GenTime &)), m_timeline,
	    SLOT(seek(const GenTime &)));

	
	connect( m_workspaceMonitor, SIGNAL( seekPositionChanged( const GenTime & ) ), this, SLOT( slotUpdateCurrentTime( const GenTime & ) ) );
	//connect editpanel sliders with timeline sliders -reh
	connect(m_workspaceMonitor,
	    SIGNAL(inpointPositionChanged(const GenTime &)), m_timeline,
	    SLOT(setInpointTimeline(const GenTime &)));
	connect(m_workspaceMonitor,
	    SIGNAL(outpointPositionChanged(const GenTime &)), m_timeline,
	    SLOT(setOutpointTimeline(const GenTime &)));

/*	
	Don't display timeline clip in timeline monitor on single click
	connect(getDocument(), SIGNAL(signalClipSelected(DocClipRef *)),
	    this, SLOT(slotSetClipMonitorSource(DocClipRef *)));*/

	connect(getDocument(), SIGNAL(signalOpenClip(DocClipRef *)),
	    this, SLOT(slotSetClipMonitorSourceAndSeek(DocClipRef *)));
	connect(getDocument(), SIGNAL(signalClipSelected(DocClipRef *)),
	    m_effectStackDialog, SLOT(slotSetEffectStack(DocClipRef *)));
	connect(getDocument(), SIGNAL(effectStackChanged(DocClipRef *)),
	    m_effectStackDialog, SLOT(slotSetEffectStack(DocClipRef *)));
	connect(m_effectStackDialog, SIGNAL(generateSceneList()),
	    getDocument(), SLOT(hasBeenModified()));

	connect(m_effectListDialog, SIGNAL(effectSelected(const QString &)), this,
        SLOT(slotAddEffect(const QString &)));

	connect(m_effectStackDialog, SIGNAL(redrawTrack(int, GenTime, GenTime)), m_timeline,
	    SLOT(drawPartialTrack(int, GenTime, GenTime)));
        
        connect(getDocument(), SIGNAL(clipReferenceChanged()), this,
                SLOT(clipReferenceChanged()));

	connect(getDocument(), SIGNAL(clipListUpdated()), m_projectList,
	    SLOT(slot_UpdateList()));
        
        connect(getDocument(), SIGNAL(timelineClipUpdated()), m_timeline,
                SLOT(drawTrackViewBackBuffer()));

        connect(getDocument(), SIGNAL(refreshCurrentClipTrack(int, int, GenTime, GenTime)), m_timeline,
                SLOT(drawCurrentTrack(int, int, GenTime, GenTime)));

	connect(getDocument(), SIGNAL(clipChanged(DocClipRef *)),
	    m_projectList, SLOT(slot_clipChanged(DocClipRef *)));
        
	connect(getDocument(), SIGNAL(nodeDeleted(DocumentBaseNode *)),
	    m_projectList, SLOT(slot_nodeDeleted(DocumentBaseNode *)));

	connect(getDocument(), SIGNAL(documentChanged(DocClipBase *)),
	    m_workspaceMonitor, SLOT(slotSetClip(DocClipBase *)));


	connect(getDocument()->renderer(),
	    SIGNAL(effectListChanged(const QPtrList < EffectDesc > &)),
	    m_effectListDialog,
	    SLOT(setEffectList(const QPtrList < EffectDesc > &)));

	connect(getDocument()->renderer(),
	    SIGNAL(rendering(const GenTime &)), this,
	    SLOT(slotSetRenderProgress(const GenTime &)));
	connect(getDocument()->renderer(), SIGNAL(renderFinished()), this,
	    SLOT(slotSetRenderFinished()));

	connect(getDocument(), SIGNAL(trackListChanged()), this,
	    SLOT(slotSyncTimeLineWithDocument()));
	
        connect(getDocument(), SIGNAL(clipChanged(DocClipRef *)),
	    this, SLOT(refreshClipTrack(DocClipRef *)));
        
	connect(getDocument(),
	    SIGNAL(documentLengthChanged(const GenTime &)), m_timeline,
	    SLOT(slotSetProjectLength(const GenTime &)));

	connect(m_projectList, SIGNAL(clipSelected(DocClipRef *)), this,
	    SLOT(activateClipMonitor()));
	connect(m_projectList, SIGNAL(clipSelected(DocClipRef *)), this,
	    SLOT(slotSetClipMonitorSource(DocClipRef *)));

	connect(m_projectList, SIGNAL(dragDropOccured(QDropEvent *, QListViewItem *)), this,
	    SLOT(slot_insertClips(QDropEvent *, QListViewItem *)));
        
        connect(m_projectList, SIGNAL(editItem()), this, SLOT(slotProjectEditClip()));

	if (m_workspaceMonitor) {
	    connect(m_timeline,
		SIGNAL(seekPositionChanged(const GenTime &)),
		m_workspaceMonitor->editPanel(),
		SLOT(seek(const GenTime &)));
	    //connect timeline sliders with editpanel sliders -reh
	    connect(m_timeline,
		SIGNAL(inpointPositionChanged(const GenTime &)),
		m_workspaceMonitor->editPanel(),
		SLOT(setInpoint(const GenTime &)));
	    connect(m_timeline,
		SIGNAL(outpointPositionChanged(const GenTime &)),
		m_workspaceMonitor->editPanel(),
		SLOT(setOutpoint(const GenTime &)));
	}


	connect(m_timeline, SIGNAL(seekPositionChanged(const GenTime &)),
        this, SLOT(activateWorkspaceMonitor()));
        
	connect(m_timeline, SIGNAL(rightButtonPressed()), this,
	    SLOT(slotDisplayTimeLineContextMenu()));

	connect(m_timeline, SIGNAL(headerRightButtonPressed()), this,
	    SLOT(slotDisplayTrackHeaderContextMenu()));

	connect(m_timeline, SIGNAL(rulerRightButtonPressed()), this,
	    SLOT(slotDisplayRulerContextMenu()));

	m_timeline->trackView()->clearFunctions();

	m_timeline->trackView()->registerFunction("move",
	    new TrackPanelClipMoveFunction(this, m_timeline,
		getDocument()));
	TrackPanelClipResizeFunction *resizeFunction =
	    new TrackPanelClipResizeFunction(this, m_timeline,
	    getDocument());
	m_timeline->trackView()->registerFunction("resize",
	    resizeFunction);

	TrackPanelKeyFrameFunction *keyFrameFunction =
	    new TrackPanelKeyFrameFunction(this, m_timeline,
	    getDocument());
	m_timeline->trackView()->registerFunction("keyframe",
	    keyFrameFunction);
        
        TrackPanelTransitionResizeFunction *transitionResizeFunction =
                new TrackPanelTransitionResizeFunction(this, m_timeline,
                                               getDocument());
        m_timeline->trackView()->registerFunction("transitionresize",
        transitionResizeFunction);
        
        TrackPanelTransitionMoveFunction *transitionMoveFunction =
                new TrackPanelTransitionMoveFunction(this, m_timeline,
                getDocument());
        m_timeline->trackView()->registerFunction("transitionmove",
        transitionMoveFunction);

	TrackPanelClipRollFunction *rollFunction =
	    new TrackPanelClipRollFunction(this, m_timeline,
	    getDocument());

	//register roll function -reh
	m_timeline->trackView()->registerFunction("roll", rollFunction);


	// connects for clip/timeline monitor activation (i.e. making sure they are visible when needed)

	connect(m_transitionPanel, SIGNAL(transitionChanged(bool)),
                getDocument(), SLOT(activateSceneListGeneration(bool)));

	connect(m_transitionPanel, SIGNAL(transitionChanged(bool)),
	    m_timeline, SLOT(drawTrackViewBackBuffer()));

	connect(keyFrameFunction, SIGNAL(signalKeyFrameChanged(bool)),
	    getDocument(), SLOT(activateSceneListGeneration(bool)));
        
        connect(transitionMoveFunction, SIGNAL(transitionChanged(bool)),
                getDocument(), SLOT(activateSceneListGeneration(bool)));

	connect(transitionMoveFunction, SIGNAL(editTransition(Transition *)), this, SLOT(slotEditTransition(Transition *)));

        
        connect(transitionResizeFunction, SIGNAL(transitionChanged(bool)),
                getDocument(), SLOT(activateSceneListGeneration(bool)));

	connect(keyFrameFunction, SIGNAL(redrawTrack()),
	    m_effectStackDialog, SLOT(updateKeyFrames()));

	connect(resizeFunction,
	    SIGNAL(signalClipCropStartChanged(DocClipRef *)), this,
	    SLOT(slotSetClipMonitorSource(DocClipRef *)));
	connect(resizeFunction,
	    SIGNAL(signalClipCropEndChanged(DocClipRef *)), this,
	    SLOT(slotSetClipMonitorSource(DocClipRef *)));

	connect(resizeFunction,
	    SIGNAL(signalClipCropStartChanged(DocClipRef *)),
	    m_clipMonitor, SLOT(slotClipCropStartChanged(DocClipRef *)));
	connect(resizeFunction,
	    SIGNAL(signalClipCropEndChanged(DocClipRef *)), m_clipMonitor,
	    SLOT(slotClipCropEndChanged(DocClipRef *)));

	/*connect(rollFunction,
	    SIGNAL(signalClipCropStartChanged(DocClipRef *)), this,
	    SLOT(slotSetClipMonitorSource(DocClipRef *)));*/
	connect(rollFunction,
	    SIGNAL(signalClipCropEndChanged(DocClipRef *)), this,
	    SLOT(slotSetClipMonitorSource(DocClipRef *)));

	/*connect(rollFunction,
	    SIGNAL(signalClipCropStartChanged(DocClipRef *)),
	    m_clipMonitor, SLOT(slotClipCropStartChanged(DocClipRef *)));*/
	connect(rollFunction,
	    SIGNAL(signalClipCropEndChanged(DocClipRef *)), m_clipMonitor,
	    SLOT(slotClipCropEndChanged(DocClipRef *)));

	m_timeline->trackView()->registerFunction("marker",
	    new TrackPanelMarkerFunction(this, m_timeline, getDocument()));
	m_timeline->trackView()->registerFunction("spacer",
	    new TrackPanelSpacerFunction(this, m_timeline, getDocument()));

	//m_timeline->trackView()->registerFunction("effects", new TrackPanelSpacerFunction(this, m_timeline, getDocument()));

	TrackPanelRazorFunction *razorFunction =
	    new TrackPanelRazorFunction(this, m_timeline, getDocument());
	m_timeline->trackView()->registerFunction("razor", razorFunction);
	connect(razorFunction, SIGNAL(lookingAtClip(DocClipRef *, const GenTime &)), this,
	    SLOT(slotLookAtClip(DocClipRef *, const GenTime &)));
        
        connect(razorFunction, SIGNAL(sceneListChanged(bool)),
                getDocument(), SLOT(activateSceneListGeneration(bool)));


	m_timeline->trackView()->registerFunction("selectnone",
	    new TrackPanelSelectNoneFunction(this, m_timeline,
		getDocument()));
	m_timeline->trackView()->setDragFunction("move");

	m_timeline->setSnapToFrame(snapToFrameEnabled());
	m_timeline->setSnapToBorder(snapToBorderEnabled());
	m_timeline->setSnapToMarker(snapToMarkersEnabled());
	m_timeline->setEditMode("move");
	slotSyncTimeLineWithDocument();

	m_timeline->slotSetFramesPerSecond(KdenliveSettings::defaultfps());
	m_timeline->slotSetProjectLength(getDocument()->projectClip().duration());

        //QTimer::singleShot(200, getDocument(), SLOT(activateSceneListGeneration()));
	m_doc->setModified(false);
	
    }

    void KdenliveApp::focusTimelineWidget()
    {
        m_timelineWidget->setFocus();
    }
    

    void KdenliveApp::slotUpdateLayoutState() {
	showClipMonitor->setChecked(!m_dockClipMonitor->mayBeShow());
	showWorkspaceMonitor->setChecked(!m_dockWorkspaceMonitor->mayBeShow());
	showEffectList->setChecked(!m_dockEffectList->mayBeShow());
	showProjectList->setChecked(!m_dockProjectList->mayBeShow());
	showEffectStack->setChecked(!m_dockEffectStack->mayBeShow());
	showTransitions->setChecked(!m_dockTransition->mayBeShow());
    }
    
    void KdenliveApp::customEvent(QCustomEvent* e)
    {
        if( e->type() == 10000) {
            // The timeline playing position changed...
            PositionChangeEvent *ev = (PositionChangeEvent *)e;
            m_monitorManager.activeMonitor()->screen()->positionChanged(ev->position());
	    if (KdenliveSettings::autoscroll() && m_workspaceMonitor->screen()->playSpeed() > 0) m_timeline->autoScroll();
        }
        else if( e->type() == 10001) {
            // The export process progressed
            if (m_exportWidget) m_exportWidget->reportProgress(((PositionChangeEvent *)e)->position());
        }
        else if( e->type() == 10002) {
            // Timeline playing stopped
	    PositionChangeEvent *ev = (PositionChangeEvent *)e;
            m_monitorManager.activeMonitor()->screen()->positionChanged(ev->position());
            m_monitorManager.activeMonitor()->screen()->slotPlayingStopped();
        }
        else if( e->type() == 10003) {
            // export is over
            m_workspaceMonitor->screen()->slotExportStopped();
        }
	else if( e->type() == 10005) {
            // Show progress of an audio thumb
	    int val = ((ProgressEvent *)e)->value();
	    switch (val) {
	    case -1:
		// reset new thumb creation
		m_statusBarProgress->setTotalSteps(100);
		slotStatusMsg(i18n("Ready."));
		m_statusBarProgress->hide();
		break;
	    case 0:
		// thumb just finished
		slotStatusMsg(i18n("Ready."));
		m_statusBarProgress->setTotalSteps(0);
		m_statusBarProgress->setProgress(0);
		m_statusBarProgress->hide();
		getDocument()->refreshAudioThumbnails();
		break;
	    default:
		// progressing...
		slotStatusMsg(i18n("Generating audio thumb"));
		m_statusBarProgress->show();
	    	m_statusBarProgress->setProgress(val);
		break;
	    }
        }
	else if( e->type() == 10006) {
            // The export process progressed
            //if (m_exportWidget) m_exportWidget->reportProgress(((ProgressEvent *)e)->value());
        }
	else if( e->type() == 10007) {
            // Show progress of an export process
	    int val = ((ProgressEvent *)e)->value();
	    if (val == 0) {
		slotStatusMsg(i18n("Ready."));
		m_statusBarExportProgress->hide();
	    }
	    else {
		slotStatusMsg(i18n("Exporting to File"));
		m_statusBarExportProgress->show();
	    }
	    m_statusBarExportProgress->setProgress(val);
	    }
    }

    void KdenliveApp::slotEditTransition(Transition *transition) {
	m_dockTransition->makeDockVisible();
	m_transitionPanel->setTransition(transition);
        m_timeline->drawTrackViewBackBuffer();
    }

    void KdenliveApp::slotToggleClipMonitor() {
	m_dockClipMonitor->changeHideShowState();
    }

    void KdenliveApp::slotToggleWorkspaceMonitor() {
	m_dockWorkspaceMonitor->changeHideShowState();
    }

    void KdenliveApp::slotToggleEffectList() {
	m_dockEffectList->changeHideShowState();
    }

    void KdenliveApp::slotToggleEffectStack() {
	m_dockEffectStack->changeHideShowState();
    }

    void KdenliveApp::slotToggleProjectList() {
	m_dockProjectList->changeHideShowState();
    }
    
    void KdenliveApp::slotToggleTransitions() {
        m_dockTransition->changeHideShowState();
    }

    void KdenliveApp::slotFocusClipMonitor() {
	activateClipMonitor();
	//m_dockClipMonitor->makeDockVisible();
    }

    void KdenliveApp::slotFocusWorkspaceMonitor() {
	activateWorkspaceMonitor();
	//m_dockWorkspaceMonitor->makeDockVisible();
    }

    void KdenliveApp::slotFocusEffectList() {
	m_dockEffectList->makeDockVisible();
	m_effectListDialog->setFocus();
    }

    void KdenliveApp::slotFocusEffectStack() {
	m_dockEffectStack->makeDockVisible();
	m_effectStackDialog->m_effectList->setFocus();
    }

    void KdenliveApp::slotFocusProjectList() {
	m_dockProjectList->makeDockVisible();
	m_projectList->m_listView->setFocus();
    }
    
    void KdenliveApp::slotFocusTransitions() {
        m_dockTransition->makeDockVisible();
	m_dockTransition->setFocus();
    }

    void KdenliveApp::slotSelectPreviousTrack() {
	m_timeline->selectPreviousTrack();
	if (clipAutoSelect->isChecked()) selectClipUnderCursor();
    }


    void KdenliveApp::slotMoveClipToCurrentTime() {
	DocClipRef *clip = getDocument()->projectClip().selectedClip();
	if (!clip) return;
	GenTime offset = getDocument()->renderer()->seekPosition() - clip->trackStart();
	getDocument()->moveSelectedClips(offset, 0);
    }

    void KdenliveApp::slotMoveClipDown() {
	DocClipRef *clip = getDocument()->projectClip().selectedClip();
	if (!clip) return;
	getDocument()->moveSelectedClips(GenTime(0.0), 1);
    }

    void KdenliveApp::slotMoveClipUp() {
	DocClipRef *clip = getDocument()->projectClip().selectedClip();
	if (!clip) return;
	getDocument()->moveSelectedClips(GenTime(0.0), -1);
    }

    void KdenliveApp::slotResizeClipStart() {
	DocClipRef *clip = getDocument()->projectClip().selectedClip();
	if (!clip) return;
	clip->parentTrack()->resizeClipTrackStart(clip, getDocument()->renderer()->seekPosition());
    }

    void KdenliveApp::slotResizeClipEnd() {
	DocClipRef *clip = getDocument()->projectClip().selectedClip();
	if (!clip) return;
	clip->parentTrack()->resizeClipTrackEnd(clip, getDocument()->renderer()->seekPosition());
    }

    void KdenliveApp::slotSelectNextTrack() {
	m_timeline->selectNextTrack();
	if (clipAutoSelect->isChecked()) selectClipUnderCursor();
    }

    void KdenliveApp::openDocumentFile(const KURL & url) {
	if (!saveModified()) {
	    // here saving wasn't successful
	} else if (KIO::NetAccess::exists(url, true, this)) {
	    kdWarning() << "Opening url " << url.path() << endl;
            requestDocumentClose(url);
	    initView();
	    m_projectFormatManager.openDocument(url, m_doc);
	    setCaption(url.fileName() + " - " + easyName(m_projectFormat), false);
	    fileOpenRecent->addURL(m_doc->URL());
	}
	else {
	    KMessageBox::sorry(this, i18n("Cannot read file: %1").arg(url.path()));
	    slotFileNew();
	    return;
	}

/*	if (!KIO::NetAccess::exists(KURL(KdenliveSettings::currenttmpfolder()), false, this)) {
		if (KMessageBox::questionYesNo(this, i18n("Cannot write to the temporary folder:\n%1\nDo you want to create the folder ?\n Answering no will disable audio thumbnails").arg(KdenliveSettings::currentdefaultfolder())) ==  KMessageBox::No) {
			KdenliveSettings::setAudiothumbnails(false);
		}
		else {
			if (!KIO::NetAccess::exists(KURL(KdenliveSettings::defaultfolder()), false, this))
				KIO::NetAccess::mkdir(KURL(KdenliveSettings::defaultfolder()), this);
			if (!KIO::NetAccess::exists(KURL(KdenliveSettings::currentdefaultfolder()), false, this)) {
				KIO::NetAccess::mkdir(KURL(KdenliveSettings::currentdefaultfolder()), this);
				
				if (!KIO::NetAccess::exists(KURL(KdenliveSettings::currentdefaultfolder()), false, this)) {
					KMessageBox::sorry(0, i18n("Unable to create the project folder. Audio thumbnails will be disabled."));
					KdenliveSettings::setAudiothumbnails(false);
				}
			}
		}
	}
	*/
	QTimer::singleShot(500, m_timeline, SLOT(ensureCursorVisible()));
	slotStatusMsg(i18n("Ready."));
    }

    QString KdenliveApp::easyName(VIDEOFORMAT format) {
	switch (format) {
	    case HDV1080PAL_VIDEO:
		return i18n("HDV 1080");
	    case HDV720PAL_VIDEO:
		return i18n("HDV 720");
	    case NTSC_VIDEO:
		return i18n("NTSC");
	    default:
		return i18n("PAL");
	    }
    }

   uint KdenliveApp::projectVideoFormat() {
	return (uint) m_projectFormat;
   }

    GenTime KdenliveApp::inpointPosition() const {
	return m_timeline->inpointPosition();
    }

    void KdenliveApp::setInpointPosition(const GenTime in) {
	m_timeline->setInpointTimeline(in);
    }

    QDomDocument KdenliveApp::xmlGuides() {
	return m_timeline->xmlGuides();
    }

    void KdenliveApp::guidesFromXml(QDomElement doc) {
	m_timeline->guidesFromXml(doc);
    }

    QValueList <int> KdenliveApp::timelineGuides() const {
	return m_timeline->timelineGuides();
    }

    QString KdenliveApp::timelineGuidesComments() const {
	return m_timeline->timelineRulerComments().join(";");
    }

    GenTime KdenliveApp::outpointPosition() const {
	return m_timeline->outpointPosition();
    }

    void KdenliveApp::setOutpointPosition(const GenTime out) {
	m_timeline->setOutpointTimeline(out);
    }

    KdenliveDoc *KdenliveApp::getDocument() const {
	return m_doc;
    }

    void KdenliveApp::saveOptions() {
	config->setGroup("General Options");
	config->writeEntry("Geometry", size());
	config->writeEntry("TimeScaleSlider", m_timeline->getTimeScaleSliderText());
	config->writeEntry("FileDialogPath", m_fileDialogPath.path());
	writeDockConfig(config, "Default Layout");
	fileOpenRecent->saveEntries(config);
    }

    int KdenliveApp::getTimeScaleSliderText() const {
	int value = m_timeline->getTimeScaleSliderText();
	 return value;
    }

    void KdenliveApp::readOptions() {
	config->setGroup("General Options");

	//timeline slider timescale setting
	int iTimeScaleSlider =
	    (int) config->readNumEntry("TimeScaleSlider", 6);
	m_timeline->setSliderIndex(iTimeScaleSlider);

	// initialize the recent file list
	fileOpenRecent->loadEntries(config);
	// file dialog path
	m_fileDialogPath = KURL(config->readEntry("FileDialogPath", ""));

	QSize size = config->readSizeEntry("Geometry");
	if (!size.isEmpty()) {
	    resize(size);
	}

    }

    void KdenliveApp::saveProperties(KConfig * _cfg) {
	if (m_doc->URL().fileName() != i18n("Untitled")
	    && !m_doc->isModified()) {
	    // saving to tempfile not necessary

	} else {
	    KURL url = m_doc->URL();
	    _cfg->writeEntry("filename", url.url());
	    _cfg->writeEntry("modified", m_doc->isModified());
	    QString tempname = kapp->tempSaveName(url.url());
	    QString tempurl = KURL::encode_string(tempname);
	    KURL _url(tempurl);
	    m_projectFormatManager.saveDocument(_url, m_doc);
	}
    }


    void KdenliveApp::readProperties(KConfig * _cfg) {
	QString filename = _cfg->readEntry("filename", "");
	KURL url(filename);
	bool modified = _cfg->readBoolEntry("modified", false);
	if (modified) {
	    bool canRecover;
	    QString tempname =
		kapp->checkRecoverFile(filename, canRecover);
	    KURL _url(tempname);

	    if (canRecover) {
		initView();
		m_projectFormatManager.openDocument(_url, m_doc);
		m_doc->setModified(true);
	    	setCaption(url.fileName() + " - " + easyName(m_projectFormat), true);
		QFile::remove(tempname);
	    }
	} else {
	    if (!filename.isEmpty()) {
		initView();
		m_projectFormatManager.openDocument(url, m_doc);
	    	setCaption(url.fileName() + " - " + easyName(m_projectFormat), false);
	    }
	}
    }

    bool KdenliveApp::queryClose() {
	saveOptions();
	return saveModified();
    }

    bool KdenliveApp::saveModified() {
	bool completed = true;

	if (m_doc->isModified()) {
	    int want_save = KMessageBox::warningYesNoCancel(this,
		i18n("The current file has been modified.\n"
		    "Do you want to save it?"),
		i18n("Warning"));

	    switch (want_save) {
	    case KMessageBox::Yes:
		if (m_doc->URL().fileName() == i18n("Untitled")) {
		    slotFileSaveAs();
		} else {
		    m_projectFormatManager.saveDocument(m_doc->URL(), m_doc);
		};
		/*if (m_clipMonitor)
		    m_clipMonitor->slotClearClip();
		if (m_workspaceMonitor)
		    m_workspaceMonitor->slotClearClip();
		m_doc->deleteContents();*/

		completed = true;
		break;

	    case KMessageBox::No:
		completed = true;
		break;

	    case KMessageBox::Cancel:
		completed = false;
		break;

	    default:
		completed = false;
		break;
	    }

	}

	return completed;
    }

    bool KdenliveApp::queryExit() {
	//saveOptions();
	return true;
    }

    void KdenliveApp::setProjectFormat(VIDEOFORMAT vFormat) {
	    QString text;
	    switch (vFormat) {
	    case HDV1080PAL_VIDEO:
		// Not implemented in MLT yet
		putenv ("MLT_NORMALISATION=HDV1080PAL");
		text = i18n("HDV 1440x1080 25fps");
		
		break;
	    case HDV720PAL_VIDEO:
		// Not implemented in MLT yet
		putenv ("MLT_NORMALISATION=HDV720PAL");
		text = i18n("HDV 1280x720 25fps");
		break;
	    case NTSC_VIDEO:
		putenv ("MLT_NORMALISATION=NTSC");
		text = i18n("NTSC 720x480 30fps");
		break;
	    default:
		putenv ("MLT_NORMALISATION=PAL");
		text = i18n("PAL 720x576 25fps");
		break;
	    }
	if (KdenliveSettings::videoprofile() == "dv_wide") text.append(i18n(" [16:9]"));
	statusBar()->changeItem(text, ID_TIMELINE_MSG);
	m_projectFormat = vFormat;
    }

/////////////////////////////////////////////////////////////////////
// SLOT IMPLEMENTATION
/////////////////////////////////////////////////////////////////////

    void KdenliveApp::slotFileNew() {
	slotStatusMsg(i18n("Creating new project..."));

	if (!saveModified()) {
	    // here saving wasn't successful
	} else {
	    int videoTracks = 3;
	    int audioTracks = 2;
	    QString newProjectName;
	    m_selectedFile = NULL;
	    slotNewProject(&newProjectName, &m_selectedFile, &videoTracks, &audioTracks);
            if (!m_selectedFile.isEmpty()) {
	    	openSelectedFile();
	    }
	    else {
		requestDocumentClose();
		initView();
	    	m_doc->newDocument(videoTracks, audioTracks);
	    	setCaption(newProjectName + ".kdenlive" + " - " + easyName(m_projectFormat), false);
	    	m_doc->setProjectName( newProjectName + ".kdenlive");
	    }
	}

	slotStatusMsg(i18n("Ready."));
    }


	void KdenliveApp::slotNewProject(QString *newProjectName, KURL *fileUrl, int *videoTracks, int *audioTracks, bool byPass) {
		bool finished = false;
		QString projectFolder;
		int projectFormat, videoNum, audioNum;

		if (!byPass) {
		    // Prepare the New Project Dialog
		    QStringList recentFiles;
		    config->setGroup("RecentFiles");
		    QString Lastproject = config->readPathEntry("File1");
		    uint i = 1;
		    while (!Lastproject.isEmpty()) {
			recentFiles<<Lastproject;
			i++;
			Lastproject = config->readPathEntry("File" + QString::number(i));
		    }
		    newProject *newProjectDialog = new newProject(QDir::homeDirPath(), recentFiles, this, "new_project");
		    newProjectDialog->setCaption(i18n("Kdenlive - New Project"));
		    // Insert available video formats:
		    newProjectDialog->video_format->insertStringList(videoProjectFormats);
		    newProjectDialog->audioTracks->setValue(*audioTracks);
		    newProjectDialog->videoTracks->setValue(*videoTracks);

		    if (newProjectDialog->exec() == QDialog::Rejected) exit(1);

		    if (!newProjectDialog->isNewFile()) {
			*fileUrl = newProjectDialog->selectedFile();
			finished = true;
		    }
		    else {
			*newProjectName = newProjectDialog->projectName->text();
			projectFolder = newProjectDialog->projectFolderPath();
			projectFormat = newProjectDialog->video_format->currentItem();
			audioNum = newProjectDialog->audioTracks->value();
			videoNum = newProjectDialog->videoTracks->value();
		    }
		    delete newProjectDialog;
		}
		else {
			*newProjectName = i18n("Untitled");
			projectFolder = QDir::homeDirPath();
			projectFormat = KdenliveSettings::defaultprojectformat();
			audioNum = *audioTracks;
			videoNum = *videoTracks;
		}
		
		if (!finished) {
			KdenliveSettings::setCurrentdefaultfolder(projectFolder);

			// create a temp folder for previews & thumbnails in KDE's tmp resource dir
			if (KdenliveSettings::userdefinedtmp()) {
			    KdenliveSettings::setCurrenttmpfolder( KdenliveSettings::defaulttmpfolder() + "/kdenlive/");
			    KIO::NetAccess::mkdir(KURL(KdenliveSettings::currenttmpfolder()), 0, -1);
			}
			else KdenliveSettings::setCurrenttmpfolder(locateLocal("tmp", "kdenlive/", true));

			if (!KIO::NetAccess::exists(KdenliveSettings::currenttmpfolder(), false, this)) {
				KMessageBox::sorry(this, i18n("Unable to create a folder for temporary files.\nKdenlive will not work properly unless you choose a folder for temporary files with write access in Kdenlive Settings dialog."));
			}
			switch (projectFormat) {
				case 0:
					// PAL project
					KdenliveSettings::setDefaultheight(576);
					KdenliveSettings::setDefaultfps(25.0);
					KdenliveSettings::setAspectratio(59.0 / 54.0);
					KdenliveSettings::setVideoprofile(QString::null);
					setProjectFormat(PAL_VIDEO);
					break;
				case 1:
					// PAL WIDE project
					KdenliveSettings::setDefaultheight(576);
					KdenliveSettings::setDefaultfps(25.0);
					KdenliveSettings::setAspectratio(118.0 / 81.0);
					KdenliveSettings::setVideoprofile("dv_wide");
					setProjectFormat(PAL_VIDEO);
					break;
				case 2:
					// NTSC project
					KdenliveSettings::setDefaultheight(480);
					KdenliveSettings::setDefaultfps(30000.0 / 1001.0);
					KdenliveSettings::setAspectratio(10.0 / 11.0);
					KdenliveSettings::setVideoprofile(QString::null);
					setProjectFormat(NTSC_VIDEO);
					break;
				case 3:
					// NTSC WIDE project
					KdenliveSettings::setDefaultheight(480);
					KdenliveSettings::setDefaultfps(30000.0 / 1001.0);
					KdenliveSettings::setAspectratio(40.0 / 33.0);
					KdenliveSettings::setVideoprofile("dv_wide");
					setProjectFormat(NTSC_VIDEO);
					break;
				case 4:
					// HDV project
					KdenliveSettings::setDefaultheight(1080);
					KdenliveSettings::setDefaultwidth(1440);
					KdenliveSettings::setDefaultfps(25.0);
					KdenliveSettings::setAspectratio(1.333);
					setProjectFormat(HDV1080PAL_VIDEO);
					break;
				case 5:
					// HDV project
					KdenliveSettings::setDefaultheight(720);
					KdenliveSettings::setDefaultwidth(1280);
					KdenliveSettings::setDefaultfps(25.0);
					KdenliveSettings::setAspectratio(1.333);
					setProjectFormat(HDV720PAL_VIDEO);
					break;
			}
			*audioTracks = audioNum;
			*videoTracks = videoNum;
			}
	}


    void KdenliveApp::slotFileOpen() {
	slotStatusMsg(i18n("Opening file..."));

	if (!saveModified()) {
	    // here saving wasn't successful

	} else {
	    KURL url = KFileDialog::getOpenURL(m_fileDialogPath.path(),
		m_projectFormatManager.loadMimeTypes(),
		this,
		i18n("Open File..."));
                //requestDocumentClose(url);
	    if (!url.isEmpty()) openDocumentFile(url);
	}
	slotStatusMsg(i18n("Ready."));
    }
    
    void KdenliveApp::requestDocumentClose(KURL new_url)
    {
    m_timeline->clearGuides();
    // Check if new file is an NTSC or PAL doc and set environnement variables accordingly
    if (!new_url.isEmpty()) {
	QFile myFile(new_url.path());
	if (myFile.open(IO_ReadOnly)) {
		bool foundFormat = false;
		QDomDocument doc;
		doc.setContent(&myFile, false);
		QDomElement documentElement = doc.documentElement();
    		while (!documentElement.isNull() && documentElement.tagName() != "kdenlivedoc") {
			documentElement = documentElement.firstChild().toElement();
			kdWarning() <<
	    		"KdenliveDoc::loadFromXML() document element has unknown tagName : "
	    		<< documentElement.tagName() << endl;
    		}

    		QDomNode n = documentElement.firstChild();

    		while (!n.isNull()) {
			QDomElement e = n.toElement();
			if (!e.isNull()) {
	    		    if (e.tagName() == "properties") {
				int vFormat = e.attribute("projectvideoformat","0").toInt();
				KdenliveSettings::setVideoprofile(e.attribute("videoprofile", QString::null));
				setProjectFormat((VIDEOFORMAT) vFormat);
				foundFormat = true;
				break;
			    }
			}
			n = n.nextSibling();
		}
		if (!foundFormat) {
			setProjectFormat(PAL_VIDEO);
			KdenliveSettings::setVideoprofile(QString::null);
		}
		myFile.close();
	}
	else {
		setProjectFormat(PAL_VIDEO);
		KdenliveSettings::setVideoprofile(QString::null);
	}
    }

    if (m_effectStackDialog) m_effectStackDialog->slotSetEffectStack(0);
    m_monitorManager.resetMonitors();
    initDocument(KdenliveSettings::videotracks(), KdenliveSettings::audiotracks());
    }

    void KdenliveApp::slotEditCopy()
    {
 	if (getDocument()->projectClip().hasSelectedClips() == 0) {
            KMessageBox::sorry(this, i18n("No clip selected"));
            return;
        }
	slotStatusMsg(i18n("Copying clip %1.").arg(getDocument()->projectClip().selectedClip()->name()));
	editPaste->setEnabled(true);
	if (m_copiedClip) delete m_copiedClip; 
	m_copiedClip = getDocument()->projectClip().selectedClip()->clone(effectList(), getDocument()->clipManager());
	slotStatusMsg(i18n("Ready."));
    }

    void KdenliveApp::slotEditCut()
    {
 	if (getDocument()->projectClip().hasSelectedClips() == 0) {
            KMessageBox::sorry(this, i18n("No clip selected"));
            return;
        }
	slotStatusMsg(i18n("Cutting clip %1.").arg(getDocument()->projectClip().selectedClip()->name()));
	editPaste->setEnabled(true);
	if (m_copiedClip) delete m_copiedClip;
	m_copiedClip = getDocument()->projectClip().selectedClip()->clone(effectList(), getDocument()->clipManager());
	slotDeleteSelected();
	slotStatusMsg(i18n("Ready."));
    }

    void KdenliveApp::slotEditPaste()
    {
 	if (!m_copiedClip) {
            KMessageBox::sorry(this, i18n("No clip in clipboard"));
            return;
        }
	slotStatusMsg(i18n("Pasting clip %1.").arg(m_copiedClip->name()));
	QPoint position = mousePosition();
	int ix = m_timeline->trackView()->panelAt(m_timeline->trackView()->mapFromGlobal(position).y())->documentTrackIndex();

	GenTime insertTime = m_timeline->timeUnderMouse(m_timeline->trackView()->mapFromGlobal(position).x());

	DocClipRef *m_pastedClip = m_copiedClip->clone(effectList(), getDocument()->clipManager());

	DocClipRefList selectedClip;
	selectedClip.append(m_pastedClip);
	if (getDocument()->projectClip().canAddClipsToTracks(selectedClip, ix, insertTime)) {
		m_pastedClip->setParentTrack(getDocument()->track(ix), ix);
		m_pastedClip->moveTrackStart(insertTime);
		getDocument()->track(ix)->addClip(m_pastedClip, false);
		if (m_pastedClip->trackEnd() > getDocument()->projectClip().duration())
			 getDocument()->track(ix)->checkTrackLength();
		getDocument()->activateSceneListGeneration(true);
		slotStatusMsg(i18n("Ready."));
	}
	else {
		slotStatusMsg(i18n("Cannot past clip %1 on track %2 at %3").arg(m_pastedClip->name()).arg(ix).arg(Timecode::getEasyTimecode(insertTime, m_doc->framesPerSecond())));
	}
    }

    void KdenliveApp::slotFullScreen()
    {
	this->setWindowState(this->windowState() ^ WindowFullScreen);
    }

    void KdenliveApp::slotDeleteTrack()
    {
	QPoint position = mousePosition();
	int ix = m_timeline->selectedTrack();
	/*else {
	    KTrackPanel *panel = m_timeline->trackView()->panelAt(m_timeline->trackView()->mapFromGlobal(position).y());
	    if (!panel) {
	    	bool ok;
	    	ix = KInputDialog::getInteger(i18n("Delete Track"), i18n("Enter track number"), 0, 0, getDocument()->trackList().count() - 1, 1, 10, &ok);
	    	if (!ok) return;
	    }
	    else {
	    	ix = panel->documentTrackIndex();
	    }
	}*/
	if (KMessageBox::warningContinueCancel(this, i18n("Remove track %1 ?\nThis will remove all clips on that track.").arg(ix),i18n("Delete Track")) != KMessageBox::Continue) return;
	//kdDebug()<<"+++++++++++++++++++++  ASK TRACK DELETION: "<<ix<<endl;
	addCommand(Command::KAddRefClipCommand::deleteAllTrackClips(getDocument(), ix));
	if (m_transitionPanel->isOnTrack(ix)) m_transitionPanel->setTransition(0); 
	getDocument()->slotDeleteTrack(ix);
    }

    void KdenliveApp::slotAddTrack()
    {
	QPoint position = mousePosition();
	int ix = m_timeline->selectedTrack();
	/*else {
	    KTrackPanel *panel = m_timeline->trackView()->panelAt(m_timeline->trackView()->mapFromGlobal(position).y());
 	    if (panel) ix = panel->documentTrackIndex();
	}*/
	addTrackDialog_UI *addTrack = new addTrackDialog_UI(this);
	addTrack->setCaption(i18n("Add Track"));
	addTrack->trackNumber->setValue(ix);
	if (addTrack->exec() == QDialog::Accepted) {
		ix = addTrack->trackNumber->value();
		if (addTrack->trackPosition->currentItem() == 1) ix++;
		if (addTrack->trackVideo->isChecked())
			getDocument()->addVideoTrack(ix);
		else getDocument()->addSoundTrack(ix);
	}
    }


    void KdenliveApp::slotExternalEditor()
    {
	DocClipRef *clip = static_cast<AVListViewItem*>(m_projectList->m_listView->currentItem())->clip();
	QString externalEditor;
	if (clip->clipType() == DocClipBase::AUDIO) {
		externalEditor = KdenliveSettings::externalaudioeditor().stripWhiteSpace();	
	}
	else if (clip->clipType() == DocClipBase::IMAGE) {
		externalEditor = KdenliveSettings::externalimageeditor().stripWhiteSpace();	
	}
	else return;
	// TODO: listen to process exit, then rebuild audio thumbnail
	if (externalEditor.isEmpty()) {
	    KMessageBox::sorry(this, i18n("You didn't define any external editor for that kind of clip.\nPlease go to Kdenlive settings -> Misc to define it."));
	    return;
	}
	KProcess *p = new KProcess();
    	*p<<externalEditor;
    	*p<<clip->fileURL().path();
	p->start();
    	p->detach();
    	delete p;
    }

    void KdenliveApp::slotRenderZone()
    {
        QCheckBox * addToProject = new QCheckBox(i18n("Add new clip to project"),this);
        KFileDialog *fd = new KFileDialog(m_fileDialogPath.path(), "*.dv", this, "save_render", true,addToProject);
        fd->setOperationMode(KFileDialog::Saving);
        fd->setMode(KFile::File);
        if (fd->exec() == QDialog::Accepted) {
		if (!m_exportWidget) slotRenderExportTimeline(false);
		m_exportWidget->renderSelectedZone(fd->selectedURL().path());
	}
	delete addToProject;
	delete fd;
    }

    void KdenliveApp::slotSaveZone()
    {
        QCheckBox * addToProject = new QCheckBox(i18n("Add new clip to project"),this);
        KFileDialog *fd = new KFileDialog(m_fileDialogPath.path(), "application/vnd.westley.scenelist", this, "save_westley", true,addToProject);
        fd->setOperationMode(KFileDialog::Saving);
        fd->setMode(KFile::File);
        if (fd->exec() == QDialog::Accepted) {
		QDomDocument partial = getDocument()->projectClip().generatePartialSceneList(m_timeline->inpointPosition(), m_timeline->outpointPosition(), -1);
		QFile file(fd->selectedURL().path());
		file.open( IO_WriteOnly );
		QString save = partial.toString();
    		file.writeBlock(save.utf8(), save.length());
		file.close();
		if (addToProject->isChecked()) insertClipFromUrl(fd->selectedURL().path());
	}
	delete addToProject;
	delete fd;
    }


    void KdenliveApp::insertClipFromUrl(QString path)
    {
	addCommand(new Command::KAddClipCommand(*m_doc, m_projectList->m_listView->parentName(), path, true));
    }

    void KdenliveApp::slotVirtualZone()
    {
	bool ok;
	QString clipName = KInputDialog::getText(i18n("Create Virtual Clip"), i18n("New clip name"), i18n("Virtual Clip"), &ok);
	if (!ok) return;
	KTempFile tmp( KdenliveSettings::currenttmpfolder(), ".westley");
	QTextStream stream( tmp.file() );
    	stream << getDocument()->projectClip().generatePartialSceneList(m_timeline->inpointPosition(), m_timeline->outpointPosition(), -1).toString() << "\n";
    	tmp.close();
	addCommand(new Command::KAddClipCommand(*getDocument(), m_projectList->m_listView->parentName(), clipName, KURL(tmp.name()), m_timeline->inpointPosition(), m_timeline->outpointPosition(), QString::null, true));
    }

    void KdenliveApp::slotShowVirtualZone()
    {
	DocClipVirtual *clip = static_cast<AVListViewItem*>(m_projectList->m_listView->currentItem())->clip()->referencedClip()->toDocClipVirtual();
	if (clip) m_timeline->seek(clip->virtualStartTime());
    }

    void KdenliveApp::refreshVirtualZone()
    {
	m_timeline->slotSetVZone(getDocument()->clipManager().virtualZones());
    }

    void KdenliveApp::slotDeleteGuide()
    {
	m_timeline->slotDeleteGuide();
	if (m_exportWidget) m_exportWidget->updateGuides();
	if (m_exportDvd) m_exportDvd->fillStructure(xmlGuides());
    }

    void KdenliveApp::slotAddGuide()
    {
	m_timeline->slotAddGuide();
	if (m_exportWidget) m_exportWidget->updateGuides();
	if (m_exportDvd) m_exportDvd->fillStructure(xmlGuides());
    }	

    void KdenliveApp::insertGuides(QString guides, QString comments)
    {
	QStringList guidesList = QStringList::split(";", guides);
	QStringList commentsList = QStringList::split(";", comments);
	int i = 0;
	for (; i < guidesList.count(); i++) {
	    m_timeline->insertSilentGuide((*(guidesList.at(i))).toInt(), *(commentsList.at(i)));
	}
    }


    void KdenliveApp::slotEditGuide()
    {
	m_timeline->slotEditGuide();
	if (m_exportWidget) m_exportWidget->updateGuides();
	if (m_exportDvd) m_exportDvd->fillStructure(xmlGuides());
    }


    void KdenliveApp::slotFileSave() {
	if (m_doc->URL().isEmpty()) slotFileSaveAs();
	else {
		slotStatusMsg(i18n("Saving file..."));
		if (KIO::NetAccess::exists(m_doc->URL(), true, this)) {
		    KIO::NetAccess::copy(m_doc->URL(), KURL(m_doc->URL().path() + "~"), this);
		}
		m_projectFormatManager.saveDocument(m_doc->URL(), m_doc);
		slotStatusMsg(i18n("Ready."));
		fileOpenRecent->addURL(m_doc->URL());
	}
    }

    void KdenliveApp::slotFileSaveAs() {
	slotStatusMsg(i18n("Saving file with a new filename..."));

	KURL url = KFileDialog::getSaveURL(m_fileDialogPath.path(),
	    m_projectFormatManager.saveMimeTypes(), this, i18n("Save as..."));

	if (!url.isEmpty()) {
	    if (!url.path().endsWith(".kdenlive"))
		url.setFileName(url.filename() + ".kdenlive");

	    if (KIO::NetAccess::exists(url, true, this)) {
            	if (KMessageBox::questionYesNo(this, i18n("File already exists.\nDo you want to overwrite it ?")) ==  KMessageBox::No) return;
	    }
	    if (m_projectFormatManager.saveDocument(url, m_doc)) {
	    fileOpenRecent->addURL(url);

            setCaption(url.fileName() + " - " + easyName(m_projectFormat), m_doc->isModified());
	    m_doc->setURL(url);
	    }
	    m_fileDialogPath = url;
	    m_fileDialogPath.setFileName(QString::null);
	}

	slotStatusMsg(i18n("Ready."));
    }

    void KdenliveApp::slotFileClose() {
	slotStatusMsg(i18n("Closing file..."));
	close();
	slotStatusMsg(i18n("Ready."));
    }

    void KdenliveApp::slotFilePrint() {
	slotStatusMsg(i18n("Printing..."));

	QPrinter printer;
	if (printer.setup(this)) {
	    QPainter painter;

	    painter.begin(&printer);

	    // TODO - add Print code here.

	    painter.end();
	}

	slotStatusMsg(i18n("Ready."));
    }

    void KdenliveApp::slotFileQuit() {
	slotStatusMsg(i18n("Exiting..."));
	saveOptions();
	// close the first window, the list makes the next one the first again.
	// This ensures that queryClose() is called on each window to ask for closing
	KMainWindow *w;
	if (memberList) {
	    for (w = memberList->first(); w; w = memberList->next()) {
		// only close the window if the closeEvent is accepted. If the user presses Cancel on the saveModified() dialog,
		// the window and the application stay open.
		kdDebug() << "Closing" << w << endl;
		if (!w->close())
		    break;
	    }
	    kdDebug() << "Done" << endl;
	}
    }

    void KdenliveApp::slotStatusMsg(const QString & text) {
	///////////////////////////////////////////////////////////////////
	// change status message permanently
	//statusBar()->clear();
	statusBar()->changeItem(text, ID_STATUS_MSG);
    }

/** Alerts the App to when the document has been modified. */
    void KdenliveApp::documentModified(bool modified) {
	if (modified) {
	    fileSave->setEnabled(true);
	} else {
	    fileSave->setEnabled(false);
	}

	setCaption(m_doc->projectName() + " - " + easyName(m_projectFormat), modified);
    }

    void KdenliveApp::slotTimelineSnapToBorder() {
	m_timeline->setSnapToBorder(snapToBorderEnabled());
    }

    void KdenliveApp::slotTimelineSnapToFrame() {
	m_timeline->setSnapToFrame(snapToFrameEnabled());
    }

    void KdenliveApp::slotTimelineSnapToMarker() {
	m_timeline->setSnapToMarker(snapToMarkersEnabled());
    }

    bool KdenliveApp::snapToFrameEnabled() const {
	return timelineSnapToFrame->isChecked();
    }
    bool KdenliveApp::snapToBorderEnabled() const {
	return timelineSnapToBorder->isChecked();
    }
    bool KdenliveApp::snapToMarkersEnabled() const {
	return timelineSnapToMarker->isChecked();
    }

/** Adds a command to the command history, execute it if execute is true. */
	void KdenliveApp::addCommand(KCommand * command, bool execute) {
	m_commandHistory->addCommand(command, execute);
    }

/** Called when the move tool is selected */
    void KdenliveApp::slotTimelineMoveTool() {
	statusBar()->changeItem(i18n("Move/Resize tool"), ID_EDITMODE_MSG);
	m_timeline->setEditMode("move");
    }

/** Called when the razor tool action is selected */
    void KdenliveApp::slotTimelineRazorTool() {
	statusBar()->changeItem(i18n("Razor tool"), ID_EDITMODE_MSG);
	m_timeline->setEditMode("razor");
    }

/** Called when the spacer tool action is selected */
    void KdenliveApp::slotTimelineSpacerTool() {
	statusBar()->changeItem(i18n("Separate tool"), ID_EDITMODE_MSG);
	m_timeline->setEditMode("spacer");
    }

/** Called when the spacer tool action is selected */
    void KdenliveApp::slotTimelineMarkerTool() {
	statusBar()->changeItem(i18n("Marker tool"), ID_EDITMODE_MSG);
	m_timeline->setEditMode("marker");
    }

/** Called when the roll tool action is selected */
    void KdenliveApp::slotTimelineRollTool() {
	statusBar()->changeItem(i18n("Roll tool"), ID_EDITMODE_MSG);
	m_timeline->setEditMode("roll");
    }

/** Called when the user activates the "Export Timeline" action */
    void KdenliveApp::slotRenderExportTimeline(bool show) {
	slotStatusMsg(i18n("Exporting Timeline..."));
	    if (!m_exportWidget) { 
            m_exportWidget=new exportWidget(m_workspaceMonitor->screen(), m_timeline, m_projectFormat, this,"exporter");
            connect(m_exportWidget,SIGNAL(exportTimeLine(QString, QString, GenTime, GenTime, QStringList)),m_workspaceMonitor->screen(),SLOT(exportTimeline(QString, QString, GenTime, GenTime, QStringList)));
            connect(m_exportWidget,SIGNAL(stopTimeLineExport()),m_workspaceMonitor->screen(),SLOT(stopTimeLineExport()));
            connect(m_workspaceMonitor->screen(),SIGNAL(exportOver()),m_exportWidget,SLOT(endExport()));
            connect(m_exportWidget,SIGNAL(exportToFirewire(QString, int, GenTime, GenTime)),m_workspaceMonitor->screen(),SLOT(exportToFirewire(QString, int, GenTime, GenTime)));
	    connect(m_exportWidget,SIGNAL(addFileToProject(const QString &)),this,SLOT(slotAddFileToProject(const QString &)));
	    }
	    if (show) {
	        if (m_exportWidget->isVisible()) m_exportWidget->hide();
	        else m_exportWidget->show();
	    }
        slotStatusMsg(i18n("Ready."));
    }


void KdenliveApp::slotAddFileToProject(const QString &url) {
    addCommand(new Command::KAddClipCommand(*m_doc, m_projectList->m_listView->parentName(), url, true));
}


    void KdenliveApp::slotRenderDvd() {
	if (!m_exportWidget) slotRenderExportTimeline(false);
	if (!m_exportDvd) m_exportDvd = new ExportDvdDialog(&getDocument()->projectClip(), m_exportWidget, m_projectFormat, this, "dvd");
	m_exportDvd->fillStructure(xmlGuides());
	m_exportDvd->show();
    }

    void KdenliveApp::slotOptionsPreferences() {
	slotStatusMsg(i18n("Editing Preferences"));
	m_autoSaveTimer->stop();
	KdenliveSetupDlg *dialog =
	    new KdenliveSetupDlg(this, this, "setupdlg");
	connect(dialog, SIGNAL(settingsChanged()), this,
	    SLOT(updateConfiguration()));
	dialog->exec();
	delete dialog;

	if (KdenliveSettings::autosave())
	    m_autoSaveTimer->start(KdenliveSettings::autosavetime() * 60000, false);

	slotStatusMsg(i18n("Ready."));
    }


/** Updates widgets according to the new preferences. */
    void KdenliveApp::updateConfiguration() {
// redraw timeline in case size or colors changed.
        m_clipMonitor->editPanel()->showLcd(KdenliveSettings::showlcd());
        m_workspaceMonitor->editPanel()->showLcd(KdenliveSettings::showlcd());
	slotSyncTimeLineWithDocument();
    }


/** Updates the current time in the status bar. */
    void KdenliveApp::slotUpdateCurrentTime(const GenTime & time) {
#warning The following line is broken - since frames per second is rounded to the nearest int, krulerTimeModel
#warning would never map the correct value to text if the frames per second is wrong.
	statusBar()->changeItem(i18n("Current Time : ") +
	    KRulerTimeModel::mapValueToText((int) floor(time.frames(m_doc->
			framesPerSecond()) + 0.5), m_doc->framesPerSecond()),
	    ID_CURTIME_MSG);
    }


    void KdenliveApp::slotProjectRenameFolder(QString message) {
	if (!m_projectList->m_listView->currentItem() || static_cast<AVListViewItem *>(m_projectList->m_listView->currentItem())->clip()) return;
	QString folderName = KInputDialog::getText(i18n("Rename Folder"), message + i18n("Enter new folder name: "), m_projectList->m_listView->currentItem()->text(1));
	if (folderName.isEmpty()) return;
	// check folder name doesn't exist
        QListViewItemIterator it( m_projectList->m_listView );
        while ( it.current() ) {
            if (it.current()->text(1) == folderName && (!static_cast<AVListViewItem *>(it.current())->clip())) {
		slotProjectRenameFolder(i18n("Folder %1 already exists\n").arg(folderName));
		return;
	    }
            ++it;
        }

	DocumentGroupNode *folder = getDocument()->findClipNode(m_projectList->m_listView->currentItem()->text(1))->asGroupNode();
	folder->rename(folderName);
    }

    void KdenliveApp::slotProjectDeleteFolder() {
	if (!m_projectList->m_listView->currentItem() || static_cast<AVListViewItem *>(m_projectList->m_listView->currentItem())->clip()) return;
	if (m_projectList->m_listView->currentItem()->childCount()>0)
	if (KMessageBox::questionYesNo(this, i18n("Deleting this folder will remove all reference to its clips in your project.\nDelete this folder ?")) ==  KMessageBox::No) return;
	QString folderName = m_projectList->m_listView->currentItem()->text(1);
	QListViewItem * myChild = m_projectList->m_listView->currentItem();
        while( myChild->firstChild() ) {
            m_projectList->m_listView->setCurrentItem( myChild->firstChild() );
	    slotProjectDeleteClips(false);
	    myChild = m_projectList->m_listView->findItem(folderName, 1, Qt::CaseSensitive);
        }
	getDocument()->deleteGroupNode(folderName);
	getDocument()->activateSceneListGeneration(true);
    }

    void KdenliveApp::slotProjectAddFolder(QString message) {
	QString folderName = KInputDialog::getText(i18n("New Folder"), message + i18n("Enter new folder name: "));
	if (folderName.isEmpty()) return;
	// check folder name doesn't exist
        QListViewItemIterator it( m_projectList->m_listView );
        while ( it.current() ) {
            if (it.current()->text(1) == folderName && (!static_cast<AVListViewItem *>(it.current())->clip())) {
		slotProjectAddFolder(i18n("Folder %1 already exists\n").arg(folderName));
		return;
	    }
            ++it;
        }

	DocumentGroupNode *nFolder = new DocumentGroupNode(0, folderName);
	//AVListViewItem *item = new AVListViewItem(getDocument(), m_projectList->m_listView->firstChild(), nFolder);
	//item->setExpandable(true);
	getDocument()->addClipNode(i18n("Clips"), nFolder);
    }

/** Add clips to the project */
    void KdenliveApp::slotProjectAddClips() {
	slotStatusMsg(i18n("Adding Clips"));

	// Make a reasonable filter for video / audio files.
	QString filter = "application/vnd.kde.kdenlive application/vnd.westley.scenelist application/flv application/vnd.rn-realmedia video/x-dv video/x-msvideo video/mpeg video/x-ms-wmv audio/x-mp3 audio/x-wav application/ogg *.m2t";
	KURL::List urlList =
	    KFileDialog::getOpenURLs(m_fileDialogPath.path(), filter, this,
        i18n("Open File..."));

	KURL::List::Iterator it;
	KURL url;
	
	KMacroCommand *macroCommand = new KMacroCommand(i18n("Add Clips"));
	for (it = urlList.begin(); it != urlList.end(); it++) {
	    url = (*it);
	    if (!url.isEmpty()) {
		if (m_doc->URL() == url) {
			KMessageBox::sorry(this, i18n("You cannot include the current Kdenlive document in itself."));
		}
		else if (getDocument()->clipManager().findClip(url)) KMessageBox::sorry(this, i18n("The clip %1 is already present in this project").arg(url.filename()));
		else { 
			Command::KAddClipCommand * command;
			command = new Command::KAddClipCommand(*m_doc, m_projectList->m_listView->parentName(), url, true);
			macroCommand->addCommand(command);
		}
		m_fileDialogPath = url;
	    }
	}
	addCommand(macroCommand, true);

	m_fileDialogPath.setFileName(QString::null);

	slotStatusMsg(i18n("Ready."));
    }


/**  Create a new color clip (color, text or image) */
    void KdenliveApp::slotProjectAddColorClip() {
	slotStatusMsg(i18n("Adding Clips"));
        KDialogBase *dia = new KDialogBase(  KDialogBase::Swallow, i18n("Create Color Clip"), KDialogBase::Ok | KDialogBase::Cancel, KDialogBase::Ok, this, "create_clip", true);
	createColorClip_UI *clipChoice = new createColorClip_UI(dia);
	clipChoice->edit_name->setText(i18n("Color Clip"));
        clipChoice->edit_duration->setText(KdenliveSettings::colorclipduration());
	dia->setMainWidget(clipChoice);
	dia->adjustSize();
	if (dia->exec() == QDialog::Accepted) {
	    QString color = clipChoice->button_color->color().name();
	    color = color.replace(0, 1, "0x") + "ff";
            
            QString dur = clipChoice->edit_duration->text();
            int frames = (int) ((dur.section(":",0,0).toInt()*3600 + dur.section(":",1,1).toInt()*60 + dur.section(":",2,2).toInt()) * KdenliveSettings::defaultfps() + dur.section(":",3,3).toInt());
            
            GenTime duration(frames , KdenliveSettings::defaultfps());
            
	    KCommand *command =
		new Command::KAddClipCommand(*m_doc, m_projectList->m_listView->parentName(), color, duration,
		clipChoice->edit_name->text(),
		clipChoice->edit_description->text(), true);
	    addCommand(command, true);
	}
	delete dia;
	slotStatusMsg(i18n("Ready."));
    }

    void KdenliveApp::slotProjectAddImageClip() {
	slotStatusMsg(i18n("Adding Clips"));
        KDialogBase *dia = new KDialogBase(  KDialogBase::Swallow, i18n("Create Image Clip"), KDialogBase::Ok | KDialogBase::Cancel, KDialogBase::Ok, this, "create_clip", true);
        createImageClip_UI *clipChoice = new createImageClip_UI(dia);
	clipChoice->imageExtension->hide();
	dia->setMainWidget(clipChoice);
	// Filter for the image producer
	QString filter = "image/gif image/jpeg image/png image/x-bmp image/svg+xml";
	clipChoice->url_image->setFilter(filter);
        clipChoice->edit_duration->setText(KdenliveSettings::colorclipduration());
	dia->adjustSize();
	if (dia->exec() == QDialog::Accepted) {
	    QString url = clipChoice->url_image->url();
            
            QString dur = clipChoice->edit_duration->text();
            int frames = (int) ((dur.section(":",0,0).toInt()*3600 + dur.section(":",1,1).toInt()*60 + dur.section(":",2,2).toInt()) * KdenliveSettings::defaultfps() + dur.section(":",3,3).toInt());
            
            GenTime duration(frames , KdenliveSettings::defaultfps());
	    KCommand *command =
		new Command::KAddClipCommand(*m_doc, m_projectList->m_listView->parentName(), KURL(url), duration, clipChoice->edit_description->text(), clipChoice->transparentBg->isChecked(), true);
	    addCommand(command, true);
	}
	delete dia;
	slotStatusMsg(i18n("Ready."));
    }


void KdenliveApp::slotProjectAddSlideshowClip() {
	slotStatusMsg(i18n("Adding Clips"));

        createSlideshowClip *slideDialog = new createSlideshowClip(this, "slide");
	if (slideDialog->exec() == QDialog::Accepted) {
	    QString extension = slideDialog->selectedExtension();
	    QString url = slideDialog->selectedFolder() + "/.all." + extension;
	    
	    int ttl = slideDialog->ttl();

	    KCommand *command =
		new Command::KAddClipCommand(*m_doc, m_projectList->m_listView->parentName(), KURL(url), extension, ttl, slideDialog->hasCrossfade(), slideDialog->duration(), slideDialog->description(), slideDialog->isTransparent(), true);
	    addCommand(command, true);
	}
	delete slideDialog;
	slotStatusMsg(i18n("Ready."));
    }


/* Create text clip */
    void KdenliveApp::slotProjectAddTextClip() {
        slotStatusMsg(i18n("Adding Clips"));
	int width = m_doc->projectClip().videoWidth();
	if (KdenliveSettings::videoprofile() == "dv_wide") width = width * 4 / 3;
        activateWorkspaceMonitor();
        titleWidget *txtWidget=new titleWidget(m_workspaceMonitor->screen(), width, m_doc->projectClip().videoHeight(), NULL, this,"titler",Qt::WStyle_StaysOnTop | Qt::WType_Dialog | Qt::WDestructiveClose);
        txtWidget->titleName->setText(i18n("Text Clip"));
        txtWidget->edit_duration->setText(KdenliveSettings::textclipduration());
        if (txtWidget->exec() == QDialog::Accepted) {
            QString dur = txtWidget->edit_duration->text();
            int frames = (int) ((dur.section(":",0,0).toInt()*3600 + dur.section(":",1,1).toInt()*60 + dur.section(":",2,2).toInt()) * KdenliveSettings::defaultfps() + dur.section(":",3,3).toInt());
            
            GenTime duration(frames , KdenliveSettings::defaultfps());
            QPixmap thumb = txtWidget->thumbnail(50, 40);
            QDomDocument xml = txtWidget->toXml();
            
            KCommand *command =
                    new Command::KAddClipCommand(*m_doc, m_projectList->m_listView->parentName(), duration, txtWidget->titleName->text(),QString::null, xml , txtWidget->previewFile(), thumb, txtWidget->transparentTitle->isChecked(), true);
            addCommand(command, true);
        }
        m_workspaceMonitor->screen()->restoreProducer();
        slotStatusMsg(i18n("Ready."));
    }
    
    void KdenliveApp::slotViewSelectedClip() {
	DocClipRef *clip = getDocument()->projectClip().selectedClip();
	if (clip) slotSetClipMonitorSourceAndSeek(clip);
    }

    
    void KdenliveApp::slotExportCurrentFrame() {
        if (m_monitorManager.hasActiveMonitor()) {
            QString filter = "image/png";
            QCheckBox * addToProject = new QCheckBox(i18n("Add image to project"),this);
            KFileDialog *fd = new KFileDialog(m_fileDialogPath.path(), filter, this, "save_frame", true,addToProject);
            fd->setOperationMode(KFileDialog::Saving);
            fd->setMode(KFile::File);
            if (fd->exec() == QDialog::Accepted) {
                m_monitorManager.activeMonitor()->exportCurrentFrame(fd->selectedURL());
                //KMacroCommand *macroCommand = new KMacroCommand(i18n("Add Clips"));
                if (addToProject->isChecked()) {
                    QString dur = KdenliveSettings::colorclipduration();
                    int frames = (int) ((dur.section(":",0,0).toInt()*3600 + dur.section(":",1,1).toInt()*60 + dur.section(":",2,2).toInt()) * KdenliveSettings::defaultfps() + dur.section(":",3,3).toInt());
                    GenTime duration(frames , KdenliveSettings::defaultfps());
                    KCommand *command =
                            new Command::KAddClipCommand(*m_doc, m_projectList->m_listView->parentName(), fd->selectedURL(), duration, QString::null, false, true);
                    addCommand(command, true);
                }
            }
	    delete addToProject;
            delete fd;
        }
        else KMessageBox::sorry(this, i18n("Please activate a monitor if you want to save a frame"));
    }

    
    
    /* Edit parent clip of the selected timeline clip*/
    void KdenliveApp::slotProjectEditParentClip() {
        m_projectList->selectClip(getDocument()->projectClip().selectedClip()->referencedClip());
        slotProjectEditClip();
    }
    
    /* Duplicate text clip */
    void KdenliveApp::slotProjectDuplicateTextClip() {
	DocClipRef *refClip = m_projectList->currentSelection();
	if (refClip) {
	    DocClipBase *clip = refClip->referencedClip();
            
            if (refClip->clipType() != DocClipBase::TEXT) return;
	    QString clipName = clip->name();
	    int revision = clipName.section("#", -1).toInt();
	    if (revision > 0) { 
		revision++;
		clipName = clipName.section("#", 0, 0).append("#"+QString::number(revision));
	    }
	    else clipName.append("#2");
	 

	    KTempFile tmp(KdenliveSettings::currenttmpfolder(),".png");
	    QPixmap thumb = clip->thumbnail();
            KCommand *command =
                    new Command::KAddClipCommand(*m_doc, m_projectList->m_listView->parentName(), refClip->duration(), clip->name(), QString::null, clip->toDocClipTextFile()->textClipXml() , KURL(tmp.name()), thumb, clip->toDocClipTextFile()->isTransparent(), true);
            addCommand(command, true);
	}
	slotStatusMsg(i18n("Ready."));
    }

    /* Edit existing clip */
    void KdenliveApp::slotProjectEditClip() {
	slotStatusMsg(i18n("Editing Clips"));
	DocClipRef *refClip = m_projectList->currentSelection();
	if (refClip) {
	    DocClipBase *clip = refClip->referencedClip();
            
            if (refClip->clipType() == DocClipBase::TEXT) {
                activateWorkspaceMonitor();
		int width = m_doc->projectClip().videoWidth();
		if (KdenliveSettings::videoprofile() == "dv_wide") width = width * 4 / 3;
                titleWidget *txtWidget=new titleWidget(m_workspaceMonitor->screen(), width, m_doc->projectClip().videoHeight(), clip->fileURL(), this,"titler",Qt::WStyle_StaysOnTop | Qt::WType_Dialog | Qt::WDestructiveClose);
                Timecode tcode;
                txtWidget->edit_duration->setText(tcode.getTimecode(refClip->duration(), KdenliveSettings::defaultfps()));

                txtWidget->setXml(clip->toDocClipTextFile()->textClipXml());
                txtWidget->titleName->setText(clip->name());
                txtWidget->transparentTitle->setChecked(clip->toDocClipTextFile()->isTransparent());
                if (txtWidget->exec() == QDialog::Accepted) {
                    QString dur = txtWidget->edit_duration->text();
                    int frames = (int) ((dur.section(":",0,0).toInt()*3600 + dur.section(":",1,1).toInt()*60 + dur.section(":",2,2).toInt()) * KdenliveSettings::defaultfps() + dur.section(":",3,3).toInt());
            
                    GenTime duration(frames , KdenliveSettings::defaultfps());
                    QPixmap thumb = txtWidget->thumbnail(50, 40);
                    QDomDocument xml = txtWidget->toXml();
                    KCommand *command = new Command::KEditClipCommand(*m_doc, refClip, duration,                           txtWidget->titleName->text(),QString::null, xml , txtWidget->previewFile(), thumb, txtWidget->transparentTitle->isChecked());

		    if (refClip->numReferences() > 0) getDocument()->activateSceneListGeneration(true);
            	}
                m_workspaceMonitor->screen()->restoreProducer();
            }
            else {
                ClipProperties *dia = new ClipProperties(refClip, getDocument()); 
                if (dia->exec() == QDialog::Accepted) {
                    if (refClip->clipType() == DocClipBase::COLOR) {
                        KCommand *command =
                                new Command::KEditClipCommand(*m_doc, refClip, dia->color(),
                                dia->duration(), dia->name(), dia->description());
                    }
                    else if (refClip->clipType() == DocClipBase::IMAGE) {
			QString url = dia->url();
                        KCommand *command =
                                new Command::KEditClipCommand(*m_doc, refClip, url, dia->duration(), dia->description(), dia->transparency());
		    }
		    else if (refClip->clipType() == DocClipBase::SLIDESHOW) {
			QString url = dia->url() + "/.all." + dia->extension();
                        KCommand *command =
                                new Command::KEditClipCommand(*m_doc, refClip, url, "",dia->ttl(), dia->crossfading(), 
                                dia->duration(), dia->description(), dia->transparency());
                    }
                    else { // Video clip
                        KCommand *command = new Command::KEditClipCommand(*m_doc, refClip, dia->url(),dia->description());
                    }
		if (refClip->numReferences() > 0) getDocument()->activateSceneListGeneration(true);
                }
            }
	}
	slotStatusMsg(i18n("Ready."));
    }



/** Remove clips from the project */
    void KdenliveApp::slotProjectDeleteClips(bool confirm) {
	slotStatusMsg(i18n("Removing Clips"));

	DocClipRef *refClip = m_projectList->currentSelection();
	if (refClip) {
	    DocClipBase *clip = refClip->referencedClip();
	    if (!confirm || KMessageBox::warningContinueCancel(this,
		    i18n
		    ("This will remove all clips on timeline that are currently using this clip. Are you sure you want to do this?"))
		== KMessageBox::Continue) {
		// Create a macro command that will delete all clips from the timeline involving this
		// avfile. Then, delete it.
		KMacroCommand *macroCommand =
		    new KMacroCommand(i18n("Delete Clip"));

		// NOTE - we clear the monitors of the clip here - this does _not_ go into the macro
		// command.
		int id = clip->getId();
		m_monitorManager.clearClip(clip);

		DocClipRefList list = m_doc->referencedClips(clip);
		QPtrListIterator < DocClipRef > itt(list);

		while (itt.current()) {
		    Command::KAddRefClipCommand * command =
			new Command::KAddRefClipCommand(effectList(), m_doc->clipManager(),
			&(m_doc->projectClip()), itt.current(), false);
			if (m_transitionPanel->belongsToClip(itt.current())) m_transitionPanel->setTransition(0);
		    macroCommand->addCommand(command);
		    ++itt;
		}

		// remove audio thumbnail and tmp files
		clip->removeTmpFile();

		DocumentBaseNode *node = m_doc->findClipNodeById(id);
		if (!node) kdDebug()<<"++++++  CANNOT FIND NODE: "<<id<<endl;
		macroCommand->addCommand(new Command::KAddClipCommand(*m_doc,
			node->name(), clip, node->parent(), false));
		addCommand(macroCommand, true);

		if (confirm) {
		    getDocument()->activateSceneListGeneration(true);
		}

	    }
	}
	else if (confirm) slotProjectDeleteFolder();
	slotStatusMsg(i18n("Ready."));
    }

/** Cleans the project of unwanted clips */
    void KdenliveApp::slotProjectClean() {
	slotStatusMsg(i18n("Cleaning Project"));

	if (KMessageBox::warningContinueCancel(this,
		i18n("Clean Project removes unused files.\
	              Are you sure you want to do this?")) ==
	    KMessageBox::Continue) {

	    KCommand *command =
		Command::KAddClipCommand::clearProject(*m_doc);

	    addCommand(command, true);
	}

	slotStatusMsg(i18n("Ready."));
    }

/*
    void KdenliveApp::slotProjectClipProperties() {
	slotStatusMsg(i18n("Viewing clip properties"));
	m_clipPropertyDialog->setClip(m_projectList->currentSelection());
	makeDockVisible(clipWidget);
	slotStatusMsg(i18n("Ready."));
    }

    void KdenliveApp::slotProjectClipProperties(DocClipRef * ) {
	slotStatusMsg(i18n("Viewing clip properties"));
	m_clipPropertyDialog->setClip(m_projectList->currentSelection());
	slotStatusMsg(i18n("Ready."));
    }
*/


    void KdenliveApp::slotPlay() {
	slotStatusMsg(i18n("Play / Pause"));
	if (m_monitorManager.hasActiveMonitor()) {
	    m_monitorManager.activeMonitor()->editPanel()->play();
	}
	slotStatusMsg(i18n("Ready."));
    }

    void KdenliveApp::slotStop() {
	slotStatusMsg(i18n("Stop"));
	if (m_monitorManager.hasActiveMonitor()) {
	    m_monitorManager.activeMonitor()->editPanel()->stop();
	}
	slotStatusMsg(i18n("Ready."));
    }

    void KdenliveApp::slotTogglePlaySelected() {
	slotStatusMsg(i18n
	    ("Starting/stopping playback of inpoint/outpoint section"));
	if (m_monitorManager.hasActiveMonitor()) {
	    m_monitorManager.activeMonitor()->editPanel()->
		togglePlaySelected();
	}
	slotStatusMsg(i18n("Ready."));
    }


    void KdenliveApp::slotToggleForwards() {
	slotStatusMsg(i18n("Playing Forwards"));
	if (m_monitorManager.hasActiveMonitor()) {
	    m_monitorManager.activeMonitor()->editPanel()->toggleForward();
	}
    }

    void KdenliveApp::slotToggleBackwards() {
	slotStatusMsg(i18n("Playing Backwards"));
	if (m_monitorManager.hasActiveMonitor()) {
	    m_monitorManager.activeMonitor()->editPanel()->toggleRewind();
	}
    }

    void KdenliveApp::slotLoopPlay() {
	slotStatusMsg(i18n("Play Loop"));
	if (m_monitorManager.hasActiveMonitor()) {
	    m_monitorManager.activeMonitor()->editPanel()->loopSelected();
	}
    }

    void KdenliveApp::slotSplitAudio() {
	DocClipRef *clip = getDocument()->projectClip().selectedClip();
	if (!clip || clip->audioChannels() == 0) return;

	int ix = clip->trackNum() + 1;
	DocClipRefList list;
	list.append(clip);
	bool found = false;
	DocTrackBase *track = getDocument()->track( ix);
	DocClipRef *clip2;
	while (track) {
	    if (track->clipType() == "Sound") {
		if (getDocument()->projectClip().canAddClipsToTracks(list, ix, clip->trackStart())) {
		// create a copy of original clip
		clip2 = clip->clone(effectList(), getDocument()->clipManager());

		// remove all effects & transitions
		EffectStack emptyEffect;
		clip2->clearVideoEffects();
		clip2->deleteTransitions();
		// Insert it in timeline
		getDocument()->track(ix)->addClip(clip2, false);
		found = true;
		break;
	        }
	    }
	    ix++;
	    track = getDocument()->track( ix);
	}
	if (!found) {
	    KMessageBox::sorry(this, i18n("Cannot find track to insert audio clip."));
	    return;
	}
	QString effectName = i18n("Mute");
	if (clip->clipEffectNames().findIndex(effectName) == -1) {
	    Effect *effect = effectList().effectDescription(effectName)->createEffect(effectName);
	    addCommand(Command::KAddEffectCommand::insertEffect(getDocument(), clip, clip->numEffects(), effect));
	}
	else clip2->deleteEffect(clip2->clipEffectNames().findIndex(effectName));
    }

    void KdenliveApp::slotSeekTo(GenTime time) {
	if (m_monitorManager.hasActiveMonitor()) {
	    m_monitorManager.activeMonitor()->editPanel()->
		seek(time);
	    //m_timeline->ensureCursorVisible();
	}
    }

    void KdenliveApp::slotNextFrame() {
	slotStatusMsg(i18n("Seeking Forwards one frame"));
	if (m_monitorManager.hasActiveMonitor()) {
	    m_monitorManager.activeMonitor()->editPanel()->
		seek(m_monitorManager.activeMonitor()->screen()->
		seekPosition() + GenTime(1,
		    getDocument()->framesPerSecond()));
	    m_timeline->ensureCursorVisible();
	    if (clipAutoSelect->isChecked()) selectClipUnderCursor();
	}
    }

    void KdenliveApp::slotLastFrame() {
	slotStatusMsg(i18n("Seeking Backwards one frame"));
	if (m_monitorManager.hasActiveMonitor()) {
	    m_monitorManager.activeMonitor()->editPanel()->
		seek(m_monitorManager.activeMonitor()->screen()->
		seekPosition() - GenTime(1,
		    getDocument()->framesPerSecond()));
	    m_timeline->ensureCursorVisible();
	    if (clipAutoSelect->isChecked()) selectClipUnderCursor();
	}
    }
    
    void KdenliveApp::slotNextSnap() {
        if (m_monitorManager.hasActiveMonitor()) {
            m_monitorManager.activeMonitor()->editPanel()->
                    seek(m_doc->toSnapTime(m_monitorManager.activeMonitor()->screen()->
                    seekPosition()));
	    m_timeline->ensureCursorVisible();
	    if (clipAutoSelect->isChecked()) selectClipUnderCursor();
        }
    }

    void KdenliveApp::slotPreviousSnap() {
        if (m_monitorManager.hasActiveMonitor()) {
            m_monitorManager.activeMonitor()->editPanel()->
                    seek(m_doc->toSnapTime(m_monitorManager.activeMonitor()->screen()->
                    seekPosition(), false));
	    m_timeline->ensureCursorVisible();
	    if (clipAutoSelect->isChecked()) selectClipUnderCursor();
        }
    }

    void KdenliveApp::slotNextSecond() {
        if (m_monitorManager.hasActiveMonitor()) {
            m_monitorManager.activeMonitor()->editPanel()->
                    seek(m_monitorManager.activeMonitor()->screen()->
                    seekPosition() + GenTime((int) getDocument()->framesPerSecond(),
            getDocument()->framesPerSecond()));
	    m_timeline->ensureCursorVisible();
	    if (clipAutoSelect->isChecked()) selectClipUnderCursor();
        }
    }

    void KdenliveApp::slotLastSecond() {
        if (m_monitorManager.hasActiveMonitor()) {
            m_monitorManager.activeMonitor()->editPanel()->
                    seek(m_monitorManager.activeMonitor()->screen()->
                    seekPosition() - GenTime((int) getDocument()->framesPerSecond(),
            getDocument()->framesPerSecond()));
	    m_timeline->ensureCursorVisible();
	    if (clipAutoSelect->isChecked()) selectClipUnderCursor();
        }
    }

    void KdenliveApp::slotGotoStart() {
        if (m_monitorManager.hasActiveMonitor()) {
            m_monitorManager.activeMonitor()->editPanel()->seek(GenTime(0.0));
	    m_timeline->ensureCursorVisible();
	    if (clipAutoSelect->isChecked()) selectClipUnderCursor();
	}
    }

    void KdenliveApp::slotGotoEnd() {
        if (m_monitorManager.hasActiveMonitor()) {
            m_monitorManager.activeMonitor()->editPanel()->seek(m_doc->projectClip().duration());
	    m_timeline->ensureCursorVisible();
	    if (clipAutoSelect->isChecked()) selectClipUnderCursor();
	}
    }

    void KdenliveApp::slotSetInpoint() {
	slotStatusMsg(i18n("Setting Inpoint"));
	if (m_monitorManager.hasActiveMonitor()) {
	    m_monitorManager.activeMonitor()->editPanel()->setInpoint();
	}
	slotStatusMsg(i18n("Ready."));
    }

    void KdenliveApp::slotSetOutpoint() {
	slotStatusMsg(i18n("Setting outpoint"));
	if (m_monitorManager.hasActiveMonitor()) {
	    m_monitorManager.activeMonitor()->editPanel()->setOutpoint();
	}
	slotStatusMsg(i18n("Ready."));
    }

    void KdenliveApp::slotDeleteSelected() {
	slotStatusMsg(i18n("Deleting Selected Clips"));
	m_effectStackDialog->slotSetEffectStack(0);
	DocClipRefList list = getDocument()->listSelected();
	QPtrListIterator < DocClipRef > itt(list);
	while (itt.current()) {
		if (m_transitionPanel->belongsToClip(itt.current())) m_transitionPanel->setTransition(0);
	        ++itt;
	}

	addCommand(Command::KAddRefClipCommand::
	    deleteSelectedClips(getDocument()), true);
	getDocument()->activateSceneListGeneration(true);
	slotStatusMsg(i18n("Ready."));
    }
    
    void KdenliveApp::clipReferenceChanged() {
        m_projectList->refresh();
    }


    void KdenliveApp::selectClipUnderCursor() {
	// select clip under cursor
	DocTrackBase *track = getDocument()->track(m_timeline->selectedTrack());
	if (!track) return;
	DocClipRef *clip = track->getClipAt(getDocument()->renderer()->seekPosition());
	if (!clip) return;
	if (clip == getDocument()->projectClip().selectedClip()) return;
	KMacroCommand *macroCommand = new KMacroCommand(i18n("Select Clip"));
	macroCommand->addCommand(Command::KSelectClipCommand::selectNone(getDocument()));
	macroCommand->addCommand(new Command::KSelectClipCommand(getDocument(), clip, true));
	addCommand(macroCommand, true);
    }

/** Returns the render manager. */
    KRenderManager *KdenliveApp::renderManager() {
	return m_renderManager;
    }

/** Sets the clip monitor source to be the given clip. */
    void KdenliveApp::slotSetClipMonitorSource(DocClipRef * clip) {
        if (clip) {
           activateClipMonitor();
	   m_clipMonitor->slotSetClip(clip);
        }
        else activateWorkspaceMonitor();
    }
    
            /** Sets the clip monitor source to be the given clip. */
    void KdenliveApp::slotSetClipMonitorSourceAndSeek(DocClipRef * clip) {
        if (clip) {
            GenTime value = getDocument()->renderer()->seekPosition();
            GenTime trackStart = clip->trackStart();
            GenTime trackEnd = clip->trackEnd();
            activateClipMonitor();
            m_clipMonitor->slotSetClip(clip);
            if (value > trackStart && value < trackEnd) {
                m_clipMonitor->editPanel()->seek(clip->cropStartTime() + value - trackStart);
            }
        }
        else activateWorkspaceMonitor();
    }

    void KdenliveApp::loadLayout1() {
	setUpdatesEnabled(FALSE);
	readDockConfig(config, "Layout 1");
	setUpdatesEnabled(TRUE);
    }

    void KdenliveApp::loadLayout2() {
	setUpdatesEnabled(FALSE);
	readDockConfig(config, "Layout 2");
	setUpdatesEnabled(TRUE);
    }

    void KdenliveApp::loadLayout3() {
	setUpdatesEnabled(FALSE);
	readDockConfig(config, "Layout 3");
	setUpdatesEnabled(TRUE);
    }

    void KdenliveApp::loadLayout4() {
	setUpdatesEnabled(FALSE);
	readDockConfig(config, "Layout 4");
	setUpdatesEnabled(TRUE);
    }

    void KdenliveApp::saveLayout1() {
	writeDockConfig(config, "Layout 1");
    }

    void KdenliveApp::saveLayout2() {
	writeDockConfig(config, "Layout 2");
    }

    void KdenliveApp::saveLayout3() {
	writeDockConfig(config, "Layout 3");
    }

    void KdenliveApp::saveLayout4() {
	writeDockConfig(config, "Layout 4");
    }


    void KdenliveApp::activateClipMonitor() {
	m_dockClipMonitor->makeDockVisible();
	m_monitorManager.activateMonitor(m_clipMonitor);
    }

    void KdenliveApp::activateMonitor(KMonitor * monitor) {
	m_monitorManager.activateMonitor(monitor);
    }

    void KdenliveApp::activateWorkspaceMonitor() {
	m_dockWorkspaceMonitor->makeDockVisible();
	m_monitorManager.activateMonitor(m_workspaceMonitor);
    }

/** Selects a clip into the clip monitor and seeks to the given time. */
    void KdenliveApp::slotLookAtClip(DocClipRef * clip,
	const GenTime & time) {
	slotSetClipMonitorSource(clip);
	m_clipMonitor->editPanel()->seek(time);
    }

    void KdenliveApp::refreshClipTrack(DocClipRef * clip) {
	m_timeline->drawCurrentTrack(clip->trackNum());
    }


   void KdenliveApp::slotSetRenderProgress(const GenTime & time) {
	m_statusBarProgress->setPercentageVisible(true);
	m_statusBarProgress->setTotalSteps((int) m_timeline->
	    projectLength().frames(getDocument()->framesPerSecond()));
	m_statusBarProgress->setProgress((int) time.frames(getDocument()->
		framesPerSecond()));
    }

    void KdenliveApp::slotSetRenderFinished() {
	std::cerr << "FINISHED RENDERING!" << std::endl;
	m_statusBarProgress->setPercentageVisible(false);
	m_statusBarProgress->setProgress(m_statusBarProgress->
	    totalSteps());
    }

    void KdenliveApp::slotConfKeys() {
	KKeyDialog::configure(actionCollection(), this, true);
    }

    void KdenliveApp::slotConfNotifications() {
	KNotifyDialog::configure(this, "notify_dialog");
    }

    void KdenliveApp::slotConfToolbars() {
	saveMainWindowSettings(KGlobal::config(), "General Options");
	KEditToolbar *dlg =
	    new KEditToolbar(actionCollection(), "kdenliveui.rc", true, this);
	if (dlg->exec()) {
	    createGUI("kdenliveui.rc");
	    applyMainWindowSettings(KGlobal::config(), "General Options");
	}
	delete dlg;
    }

    void KdenliveApp::slotConfigureProject() {
	ConfigureProjectDialog configDialog(getDocument()->renderer()->
	    fileFormats(), this, "configure project dialog");
        configDialog.setValues(m_doc->framesPerSecond(), m_doc->projectClip().videoWidth(), m_doc->projectClip().videoHeight(), KdenliveSettings::currentdefaultfolder());
	if (QDialog::Accepted == configDialog.exec()) {
	KdenliveSettings::setCurrentdefaultfolder(configDialog.projectFolder().url());
	}
    }

    void KdenliveApp::slot_moveClips(QDropEvent * event, QListViewItem * parent) {
	DocClipRefList clips =
	    ClipDrag::decode(effectList(),
	    getDocument()->clipManager(), event);

	clips.setAutoDelete(true);
	QPtrListIterator < DocClipRef > itt(clips);
	DocumentBaseNode *parentNode;
	// find folder on which the item was dropped
	if (parent) {
		//kdDebug()<<"+++++++ dropped on: "<<parent->text(1)<<endl;
		if (static_cast<AVListViewItem *>(parent)->clip() == 0) {
			parentNode = getDocument()->findClipNode(parent->text(1));
		}
		else if (parent->parent() && (static_cast<AVListViewItem *>(parent->parent()))->clip() == 0) { 
			parentNode = getDocument()->findClipNode(parent->parent()->text(1));
		}
		else parentNode = getDocument()->clipHierarch();
	}
	else parentNode = getDocument()->clipHierarch();

	// reparent the item
	while (itt.current()) {
		DocumentBaseNode *node = m_doc->findClipNode(itt.current()->name());
		if (node->hasParent() && node->parent()->name() != parentNode->name()) {
			DocumentBaseNode *oldParentNode = node->parent();
			oldParentNode->removeChild(node);
			node->reParent(parentNode);
			parentNode->addChild(node);
		}
	        ++itt;
	}
	m_projectList->slot_UpdateList();
    }

    void KdenliveApp::slot_insertClips(QDropEvent * event, QListViewItem * parent) {
	// sanity check.
	if (!ClipDrag::canDecode(event, true)) {
	    slot_moveClips(event, parent);
	    return;
	}
	DocClipRefList clips =
	    ClipDrag::decode(effectList(),
	    getDocument()->clipManager(), event);

	clips.setAutoDelete(true);

	QPtrListIterator < DocClipRef > itt(clips);
	DocumentBaseNode *parentNode;
	if (parent) {
		if (parent->pixmap(0) == 0) parentNode = getDocument()->findClipNode(parent->text(1));
		else if (parent->parent() && parent->parent()->pixmap(0) == 0) 
			parentNode = getDocument()->findClipNode(parent->parent()->text(1));
		else parentNode = getDocument()->clipHierarch();
	}
	else parentNode = getDocument()->clipHierarch();
	
	KMacroCommand *macroCommand = new KMacroCommand(i18n("Add Clips"));

	while (itt.current()) {
	    	Command::KAddClipCommand * command =
		new Command::KAddClipCommand(*getDocument(),
		"TBD - give proper name", itt.current()->referencedClip(),
		parentNode, true);
		//getDocument()->clipHierarch(), true);
	    	macroCommand->addCommand(command);
	        ++itt;
	}

	addCommand(macroCommand, true);
    }

    void KdenliveApp::slotZoomIn() {
	m_timeline->zoomTimeline(true);
    }

    void KdenliveApp::slotZoomOut() {
	m_timeline->zoomTimeline(false);
    }

    void KdenliveApp::slotFitToWidth() {
	m_timeline->fitToWidth(false);
    }
    
    void KdenliveApp::slotRestoreZoom() {
        m_timeline->fitToWidth(true);
    }


    void KdenliveApp::slotToggleSnapMarker() {
	slotStatusMsg(i18n("Toggling snap marker"));
	if (m_monitorManager.hasActiveMonitor()) {
	    m_monitorManager.activeMonitor()->slotToggleSnapMarker();
	}
	slotStatusMsg(i18n("Ready."));
    }

    void KdenliveApp::slotClearAllSnapMarkers() {
	slotStatusMsg(i18n("Clearing snap markers"));

	KMacroCommand *macroCommand =
	    new KMacroCommand(i18n("Clear all snap markers"));

	DocClipProject & clip = getDocument()->projectClip();

	populateClearSnapMarkers(macroCommand, clip, false);
	populateClearSnapMarkers(macroCommand, clip, true);

	addCommand(macroCommand, true);
	slotStatusMsg(i18n("Ready."));
    }

    void KdenliveApp::slotClearSnapMarkersFromSelected() {
	slotStatusMsg(i18n("Clearing snap markers"));

	KMacroCommand *macroCommand =
	    new KMacroCommand(i18n("Clear selected snap markers"));

	DocClipProject & clip = getDocument()->projectClip();
	populateClearSnapMarkers(macroCommand, clip, true);

	addCommand(macroCommand, true);

	slotStatusMsg(i18n("Ready."));
    }

    void KdenliveApp::populateClearSnapMarkers(KMacroCommand *
	macroCommand, DocClipProject & clip, bool selectedClips) {
	// Hmmm, I wonder if this should scan *into* project clips?For the moment I will leave it as scanning only those cliprefs in the outermost
	// proejct clip - jmw, 16/12/2003

	for (uint track = 0; track < clip.numTracks(); ++track) {
	    QPtrListIterator < DocClipRef >
		itt(clip.track(track)->firstClip(selectedClips));

	    while (itt.current()) {
	    QValueVector < CommentedTime > markers = itt.current()->commentedSnapMarkers();
		QValueVector < CommentedTime >::iterator markerItt = markers.begin();
		while (markerItt != markers.end()) {
		    Command::KAddMarkerCommand * command =
			new Command::KAddMarkerCommand(*getDocument(),
			itt.current(), (*markerItt).time(), (*markerItt).comment(), false);
		    macroCommand->addCommand(command);
		    ++markerItt;
		}
		++itt;
	    }
	}
    }

    void KdenliveApp::slotDisplayTrackHeaderContextMenu() {
	m_timelinePopupMenu = (QPopupMenu *) factory()->container("timeline_header_context", this);
	m_menuPosition = QCursor::pos();
	connect(m_timelinePopupMenu, SIGNAL(aboutToHide()), this, SLOT(hideTimelineMenu()));
	m_timelinePopupMenu->popup(QCursor::pos());
    }

    void KdenliveApp::slotDisplayRulerContextMenu() {
	m_rulerPopupMenu = (QPopupMenu *) factory()->container("go_to", this);
	QStringList guides = m_timeline->timelineRulerComments();

	m_rulerPopupMenu->clear();
	if (!guides.isEmpty()) {
	    uint ct = 100;
	    for ( QStringList::Iterator it = guides.begin(); it != guides.end(); ++it ) {
		m_rulerPopupMenu->insertItem(*it, ct);
		ct++;
	    }
	    connect(m_rulerPopupMenu, SIGNAL(activated(int)), m_timeline, SLOT(gotoGuide(int)));
	}
        // display menu
        ((QPopupMenu *) factory()->container("ruler_context", this))->popup(QCursor::pos());
    }

    void KdenliveApp::slotDisplayTimeLineContextMenu() {
	int ix = m_timeline->trackView()->panelAt(m_timeline->trackView()->mapFromGlobal(QCursor::pos()).y())->documentTrackIndex();
	DocTrackBase *track = getDocument()->track(ix);
	GenTime mouseTime;
	mouseTime = m_timeline->timeUnderMouse(m_timeline->trackView()->mapFromGlobal(QCursor::pos()).x());
	DocClipRef *clip = track->getClipAt(mouseTime);
	if (clip) {
          // select clip under mouse
	  if (clip != getDocument()->projectClip().selectedClip()) {
	  	KMacroCommand *macroCommand = new KMacroCommand(i18n("Select Clip"));
	  	macroCommand->addCommand(Command::KSelectClipCommand::selectNone(getDocument()));
	  	macroCommand->addCommand(new Command::KSelectClipCommand(getDocument(), clip, true));
	  	addCommand(macroCommand, true);
	  }
	  removeEffectsMenu->clear();
	  QStringList clipEffects = clip->clipEffectNames();
	  uint ix = 0;
	  for (QStringList::Iterator it = clipEffects.begin(); it != clipEffects.end(); ++it) {
	      removeEffectsMenu->insertItem(*it, ix);
	      ix++;
	  }
          m_timelinePopupMenu = (QPopupMenu *) factory()->container("timeline_clip_context", this);
	}
	else m_timelinePopupMenu = (QPopupMenu *) factory()->container("timeline_context", this);

	if (m_timelinePopupMenu) {
            // store the mouse click position
            m_menuPosition = QCursor::pos();
	    connect(m_timelinePopupMenu, SIGNAL(aboutToHide()), this, SLOT(hideTimelineMenu()));
            // display menu
	    m_timelinePopupMenu->popup(QCursor::pos());
	}
	else m_menuPosition = QPoint();
    }

    void KdenliveApp::resetTimelineMenuPosition() {
	m_menuPosition = QPoint();
    }

    void KdenliveApp::hideTimelineMenu() {
	// #hack: wait until the menu closes and its action is called, the reset the menu position.
	QTimer::singleShot(500, this, SLOT(resetTimelineMenuPosition()));
    }

    QPoint KdenliveApp::mousePosition() {
	// Try to return the best mouse position: If a context menu was displayed, return position of the menu, 
	// otherwise return current mouse position. 
	if (m_menuPosition.isNull()) return QCursor::pos();
	else return m_menuPosition; 
    }


    void KdenliveApp::slotAddTransition() {
        if (getDocument()->projectClip().hasSelectedClips() == 0) {
            KMessageBox::sorry(this, i18n("Please select a clip to apply transition"));
            return;
        }
        GenTime mouseTime;
	QPoint position = mousePosition();
        mouseTime = m_timeline->timeUnderMouse(m_timeline->trackView()->mapFromGlobal(position).x());
	int b_track = getDocument()->projectClip().selectedClip()->trackNum() + 1;
	DocClipRef *b_clip = getDocument()->projectClip().getClipAt(b_track, mouseTime);
	if (b_clip)
	    addCommand(Command::KAddTransitionCommand::appendTransition(getDocument()->projectClip().selectedClip(), b_clip), true);
	else addCommand(Command::KAddTransitionCommand::appendTransition(getDocument()->projectClip().selectedClip(), mouseTime), true);
	getDocument()->indirectlyModified();
    }
    
    void KdenliveApp::slotDeleteTransition() {
        if (getDocument()->projectClip().hasSelectedClips() == 0) {
            KMessageBox::sorry(this, i18n("Please select a clip to delete transition"));
            return;
        }
	GenTime mouseTime;
	QPoint position = mousePosition();
	mouseTime = m_timeline->timeUnderMouse(m_timeline->trackView()->mapFromGlobal(position).x());
	Transition *transit = getDocument()->projectClip().selectedClip()->transitionAt(mouseTime);
	if (transit) {
	    if (m_transitionPanel->isActiveTransition(transit)) m_transitionPanel->setTransition(0); 
	    addCommand(Command::KAddTransitionCommand::removeTransition(getDocument()->projectClip().selectedClip(), transit), true);
	    getDocument()->indirectlyModified();
	}
    }



/** At least one track within the project have been added or removed.
*
* The timeline needs to be updated to show these changes. */
    void KdenliveApp::slotSyncTimeLineWithDocument() {
	unsigned int index = 0;

	QPtrListIterator < DocTrackBase >
	    trackItt(getDocument()->trackList());

	// Store the state of each track to restore it when rebuilding the tracks
	QPtrListIterator < KTrackPanel >
	    panelList(m_timeline->trackList());
	uint tracksCount = trackItt.count() * 2;	// double because of keyframe tracks
	bool collapsedState[tracksCount];

	uint i = 0;

	while (i < tracksCount) {
	    if (panelList.current())
		collapsedState[i] =
		    panelList.current()->isTrackCollapsed();
	    else
		collapsedState[i] = false;
	    i++;
	    ++panelList;
	}

	m_timeline->clearTrackList();


	while (trackItt.current()) {
	    disconnect(trackItt.current(), SIGNAL(redrawSection(int, GenTime, GenTime)), m_timeline,
	    SLOT(drawPartialTrack(int, GenTime, GenTime)));
	    disconnect(trackItt.current(), SIGNAL(clipLayoutChanged(int)),
		m_timeline, SLOT(drawCurrentTrack(int)));
	    disconnect(trackItt.current(), SIGNAL(clipSelectionChanged()),
		m_timeline, SLOT(drawTrackViewBackBuffer()));

	    if (trackItt.current()->clipType() == "Video") {
		m_timeline->insertTrack(index, new KMMTrackVideoPanel(this,
			m_timeline, getDocument(),
			(dynamic_cast <
			    DocTrackVideo * >(trackItt.current())),
			collapsedState[index]));
		++index;

		m_timeline->insertTrack(index,
		    new KMMTrackKeyFramePanel(m_timeline, getDocument(),
			(*trackItt), collapsedState[index], KEYFRAMETRACK));
		++index;
	    } else if (trackItt.current()->clipType() == "Sound") {
		m_timeline->insertTrack(index, new KMMTrackSoundPanel(this,
			m_timeline, getDocument(),
			(dynamic_cast <
			    DocTrackSound * >(trackItt.current())),
			collapsedState[index]));
		++index;
		m_timeline->insertTrack(index,
		    new KMMTrackKeyFramePanel(m_timeline, getDocument(),
			(*trackItt), collapsedState[index], KEYFRAMETRACK));
		++index;
	    } else {
		kdWarning() << "Sync failed" << endl;
	    }
	    connect(trackItt.current(), SIGNAL(redrawSection(int, GenTime, GenTime)), m_timeline,
	    SLOT(drawPartialTrack(int, GenTime, GenTime)));
            connect(trackItt.current(), SIGNAL(clipLayoutChanged(int)),
                    m_timeline, SLOT(drawCurrentTrack(int)));
            connect(trackItt.current(), SIGNAL(clipSelectionChanged()),
                    m_timeline, SLOT(drawTrackViewBackBuffer()));

	    ++trackItt;
	}
	kdDebug()<<" +  ++ + ++ ++ PREPARE VID THUMB"<<endl;
        if (KdenliveSettings::videothumbnails()) getDocument()->updateTracksThumbnails();
	kdDebug()<<" +  ++ + ++ ++ PREPARE AUDIO THUMB"<<endl;
	getDocument()->refreshAudioThumbnails();
	//m_timeline->resizeTracks();
    }

    void KdenliveApp::slotRazorAllClips() {
	addCommand(Command::DocumentMacroCommands::
	    razorAllClipsAt(getDocument(), m_timeline->seekPosition()),
	    true);
	getDocument()->activateSceneListGeneration(true);
    }

    void KdenliveApp::slotRazorSelectedClips() {
	addCommand(Command::DocumentMacroCommands::
	    razorSelectedClipsAt(getDocument(),
		m_timeline->seekPosition()), true);
	getDocument()->activateSceneListGeneration(true);
    }

}				// namespace Gui
