
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
#define ID_CURTIME_MSG 3

namespace Gui {

    KdenliveApp::KdenliveApp(bool newDoc, QWidget *parent,
	const char *name):KDockMainWindow(parent, name), m_monitorManager(this),
    m_workspaceMonitor(NULL), m_captureMonitor(NULL), m_exportWidget(NULL), m_selectedFile(NULL), m_copiedClip(NULL), m_renderManager(NULL), m_doc(NULL), m_effectStackDialog(NULL), m_clipMonitor(NULL), m_projectList(NULL), m_effectListDialog(NULL), isNtscProject(false), m_timelinePopupMenu(NULL), m_rulerPopupMenu(NULL) {
	config = kapp->config();
	QString newProjectName;
	int audioTracks = 2;
	int videoTracks = 3;
	if (!KdenliveSettings::openlast() && !newDoc) {
		slotNewProject(&newProjectName, &m_selectedFile, &videoTracks, &audioTracks);
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
	initStatusBar();
	m_commandHistory = new KCommandHistory(actionCollection(), true);
	initActions();

	m_effectList.setAutoDelete(true);

	initEffects::initializeEffects( &m_effectList );

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

        
	// Reopen last project if user asked it
	if (KdenliveSettings::openlast()) openLastFile();
        else if (!m_selectedFile.isEmpty()) openSelectedFile();
	else if (!newDoc) {
	    initView();
	    QString projectType;
	    if (isNtscProject) projectType = i18n("NTSC");
	    else projectType = i18n("PAL");
	    setCaption(newProjectName + ".kdenlive" + " - " + projectType, false);
	    m_doc->setURL(KURL(KdenliveSettings::currentdefaultfolder() + "/" + newProjectName + ".kdenlive"));
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

	optionsPreferences =
	    KStdAction::preferences(this, SLOT(slotOptionsPreferences()),
	    actionCollection());
	keyBindings =
	    KStdAction::keyBindings(this, SLOT(slotConfKeys()),
	    actionCollection());
	configureToolbars =
	    KStdAction::configureToolbars(this, SLOT(slotConfToolbars()),
	    actionCollection());
	fitToWidth =
	    KStdAction::fitToWidth(this, SLOT(slotFitToWidth()),
	    actionCollection());

	(void) KStdAction::zoomIn(this, SLOT(slotZoomIn()),
	    actionCollection());
	(void) KStdAction::zoomOut(this, SLOT(slotZoomOut()),
	    actionCollection());
        
        (void) new KAction(i18n("Restore Last Zoom"), 0, this,
        SLOT(slotRestoreZoom()), actionCollection(),
        "restore_zoom");

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
	    new KAction(i18n("Delete Clips"), "editdelete.png", 0, this,
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
	configureProject =
	    new KAction(i18n("&Configure Project"), "configureproject.png",
	    0, this, SLOT(slotConfigureProject()), actionCollection(),
	    "configure_project");

	actionTogglePlay =
	    new KAction(i18n("Start/Stop"), KShortcut(Qt::Key_Space), this,
	    SLOT(slotTogglePlay()), actionCollection(), "toggle_play");
	actionTogglePlaySelected =
	    new KAction(i18n("Play Selection"),
	    KShortcut(Qt::Key_Space | Qt::CTRL), this,
	    SLOT(slotTogglePlaySelected()), actionCollection(),
	    "toggle_play_selection");

	(void) new KAction(i18n("Play/Pause"), KShortcut(Qt::Key_K), this,
	    SLOT(slotPlay()), actionCollection(), "play_clip");

	(void) new KAction(i18n("Forward"), KShortcut(Qt::Key_L), this,
	    SLOT(slotToggleForwards()), actionCollection(), "toggle_forwards");

	(void) new KAction(i18n("Backward"), KShortcut(Qt::Key_J), this,
	    SLOT(slotToggleBackwards()), actionCollection(), "toggle_backwards");


	actionNextFrame =
	    new KAction(i18n("Forward one frame"),
	    KShortcut(Qt::Key_Right), this, SLOT(slotNextFrame()),
	    actionCollection(), "forward_frame");
	actionLastFrame =
	    new KAction(i18n("Back one frame"), KShortcut(Qt::Key_Left),
	    this, SLOT(slotLastFrame()), actionCollection(),
	    "backward_frame");
        actionNextSecond =
                new KAction(i18n("Forward 1 second"),
                            KShortcut(Qt::CTRL | Qt::Key_Right), this, SLOT(slotNextSecond()),
                            actionCollection(), "forward_second");
        actionLastSecond =
                new KAction(i18n("Back one second"), KShortcut(Qt::CTRL | Qt::Key_Left),
                            this, SLOT(slotLastSecond()), actionCollection(),
                            "backward_second");

        (void) new KAction(i18n("Forward to next snap point"),
                            KShortcut(Qt::ALT | Qt::Key_Right), this, SLOT(slotNextSnap()),
                            actionCollection(), "forward_snap");

        (void) new KAction(i18n("Rewind to previous snap point"),
                            KShortcut(Qt::ALT | Qt::Key_Left), this, SLOT(slotPreviousSnap()),
                            actionCollection(), "rewind_snap");

	actionSetInpoint =
	    new KAction(i18n("Set inpoint"), KShortcut(Qt::Key_I), this,
	    SLOT(slotSetInpoint()), actionCollection(), "set_inpoint");
	actionSetOutpoint =
	    new KAction(i18n("Set outpoint"), KShortcut(Qt::Key_O), this,
	    SLOT(slotSetOutpoint()), actionCollection(), "set_outpoint");
	actionDeleteSelected =
	    new KAction(i18n("Delete Selected Clips"),
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
	    new KAction(i18n("Razor Selected Clips"),
	    KShortcut(Qt::Key_W | Qt::SHIFT), this,
	    SLOT(slotRazorSelectedClips()), actionCollection(),
	    "razor_selected_clips");

	(void) new KAction(i18n("Create Folder"), "folder_new.png", 0, this,
	    SLOT(slotProjectAddFolder()), actionCollection(),
	    "create_folder");

	(void) new KAction(i18n("Rename Folder"), 0, this,
	    SLOT(slotProjectRenameFolder()), actionCollection(),
	    "rename_folder");

	(void) new KAction(i18n("Delete Folder"), "editdelete.png", 0, this,
	    SLOT(slotProjectDeleteFolder()), actionCollection(),
	    "delete_folder");


        (void) new KAction(i18n("Add Transition"), 0, this,
        SLOT(addTransition()), actionCollection(),
        "add_transition");
        
        (void) new KAction(i18n("Delete Transition"), 0, this,
        SLOT(deleteTransition()), actionCollection(),
        "del_transition");

        (void) new KAction(i18n("Add Track"), 0, this,
        SLOT(addTrack()), actionCollection(),
        "timeline_add_track");

	(void) new KAction(i18n("Delete Track"), 0, this,
        SLOT(deleteTrack()), actionCollection(),
        "timeline_delete_track");

        (void) new KAction(i18n("Add Guide"), 0, this,
        SLOT(addGuide()), actionCollection(),
        "timeline_add_guide");

	(void) new KAction(i18n("Delete Guide"), 0, this,
        SLOT(deleteGuide()), actionCollection(),
        "timeline_delete_guide");

	(void) new KAction(i18n("Edit Guide"), 0, this,
        SLOT(editGuide()), actionCollection(),
        "timeline_edit_guide");

        showClipMonitor = new KToggleAction(i18n("Clip Monitor"), 0, this,
	    SLOT(slotToggleClipMonitor()), actionCollection(),
	    "toggle_clip_monitor");
	showWorkspaceMonitor = new KToggleAction(i18n("Workspace Monitor"), 0, this,
	    SLOT(slotToggleWorkspaceMonitor()), actionCollection(),
	    "toggle_workspace_monitor");
        showEffectList = new KToggleAction(i18n("Effect List"), 0, this,
	    SLOT(slotToggleEffectList()), actionCollection(),
	    "toggle_effect_list");
        showEffectStack = new KToggleAction(i18n("Effect Stack"), 0, this,
	    SLOT(slotToggleEffectStack()), actionCollection(),
	    "toggle_effect_stack");
        showProjectList = new KToggleAction(i18n("Project List"), 0, this,
	    SLOT(slotToggleProjectList()), actionCollection(),
	    "toggle_project_list");
        showTransitions = new KToggleAction(i18n("Transitions"), 0, this,
        SLOT(slotToggleTransitions()), actionCollection(),
        "toggle_transitions");

        (void) new KAction(i18n("Focus Clip Monitor"), 0, this,
	    SLOT(slotFocusClipMonitor()), actionCollection(),
	    "focus_clip_monitor");
	(void) new KAction(i18n("Focus Workspace Monitor"), 0, this,
	    SLOT(slotFocusWorkspaceMonitor()), actionCollection(),
	    "focus_workspace_monitor");
        (void) new KAction(i18n("Focus Effect List"), 0, this,
	    SLOT(slotFocusEffectList()), actionCollection(),
	    "focus_effect_list");
        (void) new KAction(i18n("Focus Effect Stack"), 0, this,
	    SLOT(slotFocusEffectStack()), actionCollection(),
	    "focus_effect_stack");
        (void) new KAction(i18n("Focus Project List"), 0, this,
	    SLOT(slotFocusProjectList()), actionCollection(),
	    "focus_project_list");
        (void) new KAction(i18n("Focus Transitions"), 0, this,
        SLOT(slotFocusTransitions()), actionCollection(),
        "focus_transitions");


        (void) new KAction(i18n("Edit Clip"), 0, this,
        SLOT(slotProjectEditClip()), actionCollection(),
        "edit_clip");
        
        (void) new KAction(i18n("Edit Parent Clip"), 0, this,
        SLOT(slotProjectEditParentClip()), actionCollection(),
        "project_edit_clip");
        
        (void) new KAction(i18n("Export Current Frame"), 0, this,
        SLOT(slotExportCurrentFrame()), actionCollection(),
        "export_current_frame");

        (void) new KAction(i18n("View Selected Clip"), 0, this,
        SLOT(slotViewSelectedClip()), actionCollection(),
        "view_selected_clip");
        
        

	timelineMoveTool->setExclusiveGroup("timeline_tools");
	timelineRazorTool->setExclusiveGroup("timeline_tools");
	timelineSpacerTool->setExclusiveGroup("timeline_tools");
	timelineMarkerTool->setExclusiveGroup("timeline_tools");
	timelineRollTool->setExclusiveGroup("timeline_tools");

	fileNew->setStatusText(i18n("Creates a new document"));
	fileOpen->setStatusText(i18n("Opens an existing document"));
	fileOpenRecent->setStatusText(i18n("Opens a recently used file"));
	fileSave->setStatusText(i18n("Saves the actual document"));
	fileSaveAs->setStatusText(i18n("Saves the actual document as..."));
	//  fileClose->setStatusText(i18n("Closes the actual document"));
	//  filePrint ->setStatusText(i18n("Prints out the actual document"));
	fileQuit->setStatusText(i18n("Quits the application"));
	editCut->
	    setStatusText(i18n
	    ("Cuts the selected section and puts it to the clipboard"));
	editCopy->
	    setStatusText(i18n
	    ("Copies the selected section to the clipboard"));
	editPaste->
	    setStatusText(i18n
	    ("Pastes the clipboard contents to actual position"));
	timelineMoveTool->
	    setStatusText(i18n("Moves and resizes the document"));
	timelineRazorTool->
	    setStatusText(i18n("Chops clips into two pieces"));
	timelineSpacerTool->
	    setStatusText(i18n("Shifts all clips to the right of mouse"));
	timelineMarkerTool->
	    setStatusText(i18n
	    ("Place snap marks on clips and the timeline to aid accurate positioning of clips"));
	timelineRollTool->
	    setStatusText(i18n
	    ("Move edit point between two selected clips"));
	timelineSnapToFrame->
	    setStatusText(i18n("Clips will align to the nearest frame"));
	timelineSnapToBorder->
	    setStatusText(i18n
	    ("Clips will align with the borders of other clips"));
	timelineSnapToMarker->
	    setStatusText(i18n
	    ("Clips will align with user-defined snap markers"));
	projectAddClips->setStatusText(i18n("Add clips to the project"));
	projectDeleteClips->
	    setStatusText(i18n("Remove clips from the project"));
	projectClean->
	    setStatusText(i18n("Remove unused clips from the project"));
	//projectClipProperties->setStatusText( i18n( "View the properties of the selected clip" ) );
	actionTogglePlay->setStatusText(i18n("Start or stop playback"));
	actionTogglePlay->
	    setStatusText(i18n
	    ("Start or stop playback of inpoint/outpoint selection"));
	actionNextFrame->
	    setStatusText(i18n
	    ("Move the current position forwards by one frame"));
	actionLastFrame->
	    setStatusText(i18n
	    ("Move the current position backwards by one frame"));
	actionSetInpoint->
	    setStatusText(i18n("Set the inpoint to the current position"));
	actionSetOutpoint->
	    setStatusText(i18n
	    ("Set the outpoint to the current position"));
	actionDeleteSelected->
	    setStatusText(i18n("Delete currently selected clips"));

	actionToggleSnapMarker->
	    setStatusText(i18n
	    ("Toggle a snap marker at the current monitor position"));
	actionClearAllSnapMarkers->
	    setStatusText(i18n
	    ("Toggle a snap marker at the current monitor position"));
	actionClearSnapMarkersFromSelected->
	    setStatusText(i18n
	    ("Toggle a snap marker at the current monitor position"));

	renderExportTimeline->
	    setStatusText(i18n("Render the timeline to a file"));
	configureProject->
	    setStatusText(i18n("Configure the format for this project"));

	timelineRazorAllClips->
	    setStatusText(i18n
	    ("Razor all clips on the timeline at the current seek position"));
	timelineRazorSelectedClips->
	    setStatusText(i18n
	    ("Razor all selected clips on the timeline at the current seek position"));

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
	    createDockWidget("Application", QPixmap(), this,
	    i18n("Application"));
	mainDock->setWidget(view);
	mainDock->setDockSite(KDockWidget::DockFullSite);
        mainDock->setEnableDocking(KDockWidget::DockNone);
        //mainDock->setFocusPolicy(QWidget::WheelFocus); //QWidget::TabFocus
	setCentralWidget(mainDock);
	setMainDockWidget(mainDock);
	//setCaption(m_doc->URL().fileName(), false);

	m_timelineWidget =
	    createDockWidget(i18n("TimeLine"), QPixmap(), 0,
	    i18n("TimeLine"));
	m_timeline = new KMMTimeLine(NULL, m_timelineWidget);
	m_timelineWidget->setWidget(m_timeline);
	m_timelineWidget->setDockSite(KDockWidget::DockFullSite);
	m_timelineWidget->setDockSite(KDockWidget::DockCorner);
	m_timelineWidget->manualDock(mainDock, KDockWidget::DockBottom);

	m_dockProjectList = createDockWidget("Project List", QPixmap(), 0, i18n("Project List"));
	QWhatsThis::add(m_dockProjectList,
	    i18n("Video files usable in your project. "
		"Add or remove files with the contextual menu. "
		"In order to add sequences to the current video project, use the drag and drop."));
	m_dockProjectList->setDockSite(KDockWidget::DockFullSite);
	m_dockProjectList->manualDock(mainDock, KDockWidget::DockLeft);

	m_dockTransition = createDockWidget("transition", QPixmap(), 0, i18n("Transitions"));
	m_transitionPanel = new TransitionDialog(this, m_dockTransition);
	m_dockTransition->setWidget(m_transitionPanel);
	m_dockTransition->setDockSite(KDockWidget::DockFullSite);
	m_dockTransition->manualDock(m_dockProjectList, KDockWidget::DockCenter);
	
	m_dockEffectList = createDockWidget("Effect List", QPixmap(), 0, i18n("Effect List"));
	QToolTip::add(m_dockEffectList,
	    i18n("Current effects usable with the renderer"));
	m_dockEffectList->setDockSite(KDockWidget::DockFullSite);
	m_dockEffectList->manualDock(m_dockProjectList,
	    KDockWidget::DockCenter);
	m_dockEffectStack = createDockWidget("Effect Stack", QPixmap(), 0, i18n("Effect Stack"));
	m_dockEffectStack->setDockSite(KDockWidget::DockFullSite);
	m_dockEffectStack->manualDock(m_dockProjectList,
	    KDockWidget::DockCenter);

	m_dockWorkspaceMonitor = createDockWidget("Workspace Monitor", QPixmap(), 0, i18n("Workspace Monitor"));

	m_dockWorkspaceMonitor->setDockSite(KDockWidget::DockFullSite);
	m_dockWorkspaceMonitor->manualDock(mainDock, KDockWidget::DockRight);

	m_dockClipMonitor = createDockWidget("Clip Monitor", QPixmap(), 0, i18n("Clip Monitor"));
	m_dockClipMonitor->setDockSite(KDockWidget::DockFullSite);
	m_dockClipMonitor->manualDock( m_dockWorkspaceMonitor, KDockWidget::DockCenter );

	setBackgroundMode(PaletteBase);
	makeDockInvisible(mainDock);
	readDockConfig(config, "Default Layout");


    }


    void KdenliveApp::initView() {
	////////////////////////////////////////////////////////////////////
	// create the main widget here that is managed by KTMainWindow's view-region and
	// connect the widget to your document to display document contents.
	kdDebug()<<"****************  INIT DOCUMENT VIEW ***************"<<endl;

	if (m_projectList) delete m_projectList;
	m_projectList = new ProjectList(this, getDocument(), m_dockProjectList);
	m_dockProjectList->setWidget(m_projectList);
	m_projectList->slot_UpdateList();
	m_projectList->show();
	m_dockProjectList->update();

	if (m_effectListDialog) delete m_effectListDialog;
	m_effectListDialog =
	    new EffectListDialog(getDocument()->renderer()->effectList(),
	    m_timelineWidget, "effect list");
	m_dockEffectList->setWidget(m_effectListDialog);
	m_effectListDialog->show();
	m_dockEffectList->update();

	if (m_effectStackDialog) delete m_effectStackDialog;
	m_effectStackDialog =
	    new EffectStackDialog(this, getDocument(), m_dockEffectStack,
	    "effect stack");
//      QToolTip::add( m_dockEffectStack, i18n( "All effects on the currently selected clip." ) );
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

//	m_dockCaptureMonitor = createDockWidget( "Capture Monitor", QPixmap(), 0, i18n( "Capture Monitor" ) );
//	m_captureMonitor = m_monitorManager.createCaptureMonitor( getDocument(), m_dockCaptureMonitor, "Capture Monitor" );
//	m_dockCaptureMonitor->setWidget( m_captureMonitor );
//	m_dockCaptureMonitor->setDockSite( KDockWidget::DockFullSite );
//	m_dockCaptureMonitor->manualDock( m_dockWorkspaceMonitor, KDockWidget::DockCenter );
//	m_dockCaptureMonitor->editPanel()->showLcd(KdenliveSettings::showlcd());

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
	Don't display timeline clip in workspace monitor on single click
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

	/*connect(m_effectStackDialog, SIGNAL(effectSelected(DocClipRef *,
		    Effect *)), m_effectParamDialog,
        SLOT(slotSetEffect(DocClipRef *, Effect *)));*/

	connect(m_effectStackDialog, SIGNAL(redrawTracks()), m_timeline,
	    SLOT(invalidateBackBuffer()));
        
        connect(&(getDocument()->projectClip()), SIGNAL(clipReferenceChanged()), this,
                SLOT(clipReferenceChanged()));

	connect(getDocument(), SIGNAL(clipListUpdated()), m_projectList,
	    SLOT(slot_UpdateList()));
        
        connect(getDocument(), SIGNAL(timelineClipUpdated()), m_timeline,
                SLOT(invalidateBackBuffer()));
        
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
	    m_timeline, SLOT(invalidateBackBuffer()));
        
	connect(getDocument(),
	    SIGNAL(documentLengthChanged(const GenTime &)), m_timeline,
	    SLOT(slotSetProjectLength(const GenTime &)));

	connect(m_projectList, SIGNAL(clipSelected(DocClipRef *)), this,
	    SLOT(activateClipMonitor()));
	connect(m_projectList, SIGNAL(clipSelected(DocClipRef *)), this,
	    SLOT(slotSetClipMonitorSource(DocClipRef *)));
	/*connect(m_projectList, SIGNAL(clipSelected(DocClipRef *)), this,
	    SLOT(slotProjectClipProperties(DocClipRef *)));*/

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

	/*connect(m_effectListDialog,
	    SIGNAL(effectSelected(const EffectDesc &)),
	    m_effectParamDialog,
        SLOT(slotSetEffectDescription(const EffectDesc &)));*/

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
	// connects for clip/workspace monitor activation (i.e. making sure they are visible when needed)

	connect(m_transitionPanel, SIGNAL(transitionChanged(bool)),
                getDocument(), SLOT(activateSceneListGeneration(bool)));

	connect(m_transitionPanel, SIGNAL(transitionChanged(bool)),
	    m_timeline, SLOT(invalidateBackBuffer()));

	connect(keyFrameFunction, SIGNAL(signalKeyFrameChanged(bool)),
	    getDocument(), SLOT(activateSceneListGeneration(bool)));
        
        connect(transitionMoveFunction, SIGNAL(transitionChanged(bool)),
                getDocument(), SLOT(activateSceneListGeneration(bool)));

	connect(transitionMoveFunction, SIGNAL(editTransition(Transition *)), this, SLOT(slotEditTransition(Transition *)));

        
        connect(transitionResizeFunction, SIGNAL(transitionChanged(bool)),
                getDocument(), SLOT(activateSceneListGeneration(bool)));

/*	connect(keyFrameFunction, SIGNAL(redrawTrack()), m_timeline,
        SLOT(drawTrackViewBackBuffer()));*/
	connect(keyFrameFunction, SIGNAL(redrawTrack()),
	    m_effectStackDialog, SLOT(updateKeyFrames()));

	/*connect(resizeFunction,
	    SIGNAL(signalClipCropStartChanged(DocClipRef *)), this,
	    SLOT(activateClipMonitor()));
	connect(resizeFunction,
	    SIGNAL(signalClipCropEndChanged(DocClipRef *)), this,
	    SLOT(activateClipMonitor()));*/
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

	//register roll function -reh
	m_timeline->trackView()->registerFunction("roll",
	    new TrackPanelClipRollFunction(this, m_timeline,
		getDocument()));

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
            m_workspaceMonitor->screen()->slotPlayingStopped();
        }
        else if( e->type() == 10003) {
            // export is over
            m_workspaceMonitor->screen()->slotExportStopped();
        }
	else if( e->type() == 10005) {
            // Show progress of an audio thumb
	    int val = ((ProgressEvent *)e)->value();
	    if (val == -1) {
		m_statusBarProgress->setTotalSteps(m_statusBarProgress->totalSteps() + 100);		slotStatusMsg(i18n("Generating audio thumb"));
		m_statusBarProgress->show();
	    }
	    else {
		slotStatusMsg(i18n("Generating audio thumb"));
		m_statusBarProgress->show();
		if (m_statusBarProgress->progress() + val == m_statusBarProgress->totalSteps()) {
			slotStatusMsg(i18n("Ready."));
			val = 0;
			m_statusBarProgress->setTotalSteps(0);
			m_statusBarProgress->setProgress(0);
			m_statusBarProgress->hide();
	    	}
	    	m_statusBarProgress->setProgress(m_statusBarProgress->progress() + val);
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
        m_timeline->invalidateBackBuffer(); 
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


    void KdenliveApp::openDocumentFile(const KURL & url) {
	if (!saveModified()) {
	    // here saving wasn't successful
	} else if (KIO::NetAccess::exists(url, true, this)) {
	    kdWarning() << "Opening url " << url.path() << endl;
            requestDocumentClose(url);
	    initView();
	    m_projectFormatManager.openDocument(url, m_doc);
	    QString projectType;
	    if (isNtscProject) projectType = i18n("NTSC");
	    else projectType = i18n("PAL");
	    setCaption(url.fileName() + " - " + projectType, false);
	    fileOpenRecent->addURL(m_doc->URL());
	}
	else {
	    KMessageBox::sorry(this, i18n("Cannot read file: %1").arg(url.path()));
	    slotFileNew();
	    return;
	}

	if (!KIO::NetAccess::exists(KURL(KdenliveSettings::currentdefaultfolder()), false, this)) {
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
	slotStatusMsg(i18n("Ready."));
    }

    GenTime KdenliveApp::inpointPosition() const {
	return m_timeline->inpointPosition();
    }

    void KdenliveApp::setInpointPosition(const GenTime in) {
	m_timeline->setInpointTimeline(in);
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
		QString projectType;
		if (isNtscProject) projectType = i18n("NTSC");
	    	else projectType = i18n("PAL");
	    	setCaption(url.fileName() + " - " + projectType, true);
		QFile::remove(tempname);
	    }
	} else {
	    if (!filename.isEmpty()) {
		initView();
		m_projectFormatManager.openDocument(url, m_doc);
		QString projectType;
		if (isNtscProject) projectType = i18n("NTSC");
	    	else projectType = i18n("PAL");
	    	setCaption(url.fileName() + " - " + projectType, false);
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

    void KdenliveApp::setProjectNtsc(bool isNtsc) {
	if (isNtsc) putenv ("MLT_NORMALISATION=NTSC");
	else putenv ("MLT_NORMALISATION=PAL");
	isNtscProject = isNtsc;
    }

/////////////////////////////////////////////////////////////////////
// SLOT IMPLEMENTATION
/////////////////////////////////////////////////////////////////////

    void KdenliveApp::slotFileNew() {
	slotStatusMsg(i18n("Creating new document..."));

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
		QString projectType;
		if (isNtscProject) projectType = i18n("NTSC");
	    	else projectType = i18n("PAL");
	    	setCaption(newProjectName + ".kdenlive" + " - " + projectType, false);
	    	m_doc->setURL(KURL(KdenliveSettings::currentdefaultfolder() + "/" + newProjectName + ".kdenlive"));
	    }
	}

	slotStatusMsg(i18n("Ready."));
    }


	void KdenliveApp::slotNewProject(QString *newProjectName, KURL *fileUrl, int *videoTracks, int *audioTracks) {
    		int i = 1;
		QStringList recentFiles;
		config->setGroup("RecentFiles");
		QString Lastproject = config->readPathEntry("File1");
		while (!Lastproject.isEmpty()) {
			recentFiles<<Lastproject;
			i++;
			Lastproject = config->readPathEntry("File" + QString::number(i));
		}
		newProject *newProjectDialog = new newProject(KdenliveSettings::defaultfolder(), recentFiles, this, "new_project");
		newProjectDialog->setCaption(i18n("Kdenlive - New Project"));
		if (newProjectDialog->exec() == QDialog::Rejected) exit(1);
		else {
			if (newProjectDialog->isNewFile()) {
				*newProjectName = newProjectDialog->projectName->text();
				KdenliveSettings::setCurrentdefaultfolder(newProjectDialog->projectFolderPath());
				KdenliveSettings::setCurrenttmpfolder(KdenliveSettings::currentdefaultfolder() + "/tmp/");
				if (newProjectDialog->video_format->currentItem() == 0) {
					// PAL project
					KdenliveSettings::setDefaultheight(576);
					KdenliveSettings::setDefaultfps(25.0);
					KdenliveSettings::setAspectratio(1.09259);
					setProjectNtsc(false);
				}
				else {
					// NTSC project
					KdenliveSettings::setDefaultheight(480);
					KdenliveSettings::setDefaultfps(30.0);
					KdenliveSettings::setAspectratio(0.909);
					setProjectNtsc(true);
				}

				*audioTracks = newProjectDialog->audioTracks->value();
				*videoTracks = newProjectDialog->videoTracks->value();
			}
			else {
				*fileUrl = newProjectDialog->selectedFile();
			}
		}
		delete newProjectDialog;
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
    // Check if new file is an NTSC or PAL doc and set environnement variables accordingly
    if (!new_url.isEmpty()) {
	QFile myFile(new_url.path());
	if (myFile.open(IO_ReadOnly)) {
		QTextStream stream( &myFile );
		QString fileToText = stream.read();
		if (fileToText.find("projectheight=\"480\"") != -1) {
			kdDebug()<<"--------   KDENLIVE OPENING NTSC FILE ----------------"<<endl;
			setProjectNtsc(true);
		}
		else setProjectNtsc(false);
	myFile.close();
	}
    }
    if (isNtscProject) putenv ("MLT_NORMALISATION=NTSC");
    else putenv ("MLT_NORMALISATION=PAL");
    if (m_effectStackDialog) m_effectStackDialog->slotSetEffectStack(0);
    m_monitorManager.resetMonitors();
    initDocument(3,2);
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
	m_copiedClip = getDocument()->projectClip().selectedClip()->clone(getDocument()->effectDescriptions(), getDocument()->clipManager());
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
	m_copiedClip = getDocument()->projectClip().selectedClip()->clone(getDocument()->effectDescriptions(), getDocument()->clipManager());
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

	DocClipRef *m_pastedClip = m_copiedClip->clone(getDocument()->effectDescriptions(), getDocument()->clipManager());

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

    void KdenliveApp::deleteTrack()
    {
	QPoint position = mousePosition();
	int ix = m_timeline->trackView()->panelAt(m_timeline->trackView()->mapFromGlobal(position).y())->documentTrackIndex();
	if (KMessageBox::warningContinueCancel(this, i18n("Remove track %1 ?\nThis will remove all clips on that track.").arg(ix),i18n("Delete Track")) != KMessageBox::Continue) return;
	//kdDebug()<<"+++++++++++++++++++++  ASK TRACK DELETION: "<<ix<<endl;
	addCommand(Command::KAddRefClipCommand::deleteAllTrackClips(getDocument(), ix));
	getDocument()->deleteTrack(ix);
    }

    void KdenliveApp::addTrack()
    {
	QPoint position = mousePosition();
	int ix = m_timeline->trackView()->panelAt(m_timeline->trackView()->mapFromGlobal(position).y())->documentTrackIndex();
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

    void KdenliveApp::deleteGuide()
    {
	m_timeline->deleteGuide();
	if (m_exportWidget) m_exportWidget->updateGuides();
    }

    void KdenliveApp::addGuide()
    {
	m_timeline->addGuide();
	if (m_exportWidget) m_exportWidget->updateGuides();
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


    void KdenliveApp::editGuide()
    {
	m_timeline->editGuide();
	if (m_exportWidget) m_exportWidget->updateGuides();
    }


    void KdenliveApp::slotFileSave() {
	if (m_doc->URL().fileName() == i18n("Untitled")) slotFileSaveAs();
	else {
		slotStatusMsg(i18n("Saving file..."));
		m_projectFormatManager.saveDocument(m_doc->URL(), m_doc);
		slotStatusMsg(i18n("Ready."));
		fileOpenRecent->addURL(m_doc->URL());
	}
    }

    void KdenliveApp::slotFileSaveAs() {
	slotStatusMsg(i18n("Saving file with a new filename..."));

	KURL url = KFileDialog::getSaveURL(m_fileDialogPath.path(),
	    m_projectFormatManager.saveMimeTypes(),
	    /* i18n( "*.kdenlive|Kdenlive Project Files (*.kdenlive)" ), */
	    this,
	    i18n("Save as..."));

	if (!url.isEmpty()) {
	    if (url.path().find(".") == -1) {
		url.setFileName(url.filename() + ".kdenlive");
	    }
	    if (m_projectFormatManager.saveDocument(url, m_doc)) {
	    fileOpenRecent->addURL(url);

	    QString projectType;
	    if (isNtscProject) projectType = i18n("NTSC");
	    else projectType = i18n("PAL");
            setCaption(url.fileName() + " - " + projectType, m_doc->isModified());
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
	statusBar()->clear();
	statusBar()->changeItem(text, ID_STATUS_MSG);
    }

/** Alerts the App to when the document has been modified. */
    void KdenliveApp::documentModified(bool modified) {
	if (modified) {
	    fileSave->setEnabled(true);
	} else {
	    fileSave->setEnabled(false);
	}
	QString projectType;
	if (isNtscProject) projectType = i18n("NTSC");
	else projectType = i18n("PAL");

	setCaption(m_doc->URL().filename() + " - " + projectType, modified);
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
    void KdenliveApp::slotRenderExportTimeline() {
	slotStatusMsg(i18n("Exporting Timeline..."));
	    if (!m_exportWidget) { 
            m_exportWidget=new exportWidget(m_workspaceMonitor->screen(), m_timeline, this,"exporter");
            connect(m_exportWidget,SIGNAL(exportTimeLine(QString, QString, GenTime, GenTime, QStringList)),m_workspaceMonitor->screen(),SLOT(exportTimeline(QString, QString, GenTime, GenTime, QStringList)));
            connect(m_exportWidget,SIGNAL(stopTimeLineExport()),m_workspaceMonitor->screen(),SLOT(stopTimeLineExport()));
            connect(m_workspaceMonitor->screen(),SIGNAL(exportOver()),m_exportWidget,SLOT(endExport()));
            connect(m_exportWidget,SIGNAL(exportToFirewire(QString, int, GenTime, GenTime)),m_workspaceMonitor->screen(),SLOT(exportToFirewire(QString, int, GenTime, GenTime)));
	    }
	    if (m_exportWidget->isVisible()) m_exportWidget->hide();
	    else m_exportWidget->show();
        
        //delete m_exportWidget;
        //m_exportWidget = 0;
	slotStatusMsg(i18n("Ready."));
    }


    void KdenliveApp::slotOptionsPreferences() {
	slotStatusMsg(i18n("Editing Preferences"));

	KdenliveSetupDlg *dialog =
	    new KdenliveSetupDlg(this, this, "setupdlg");
	connect(dialog, SIGNAL(settingsChanged()), this,
	    SLOT(updateConfiguration()));
	dialog->exec();
	delete dialog;

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
	QString filter = "application/flv application/vnd.rn-realmedia video/x-dv video/x-msvideo video/mpeg audio/x-mp3 audio/x-wav application/ogg";
	KURL::List urlList =
	    KFileDialog::getOpenURLs(m_fileDialogPath.path(), filter, this,
        i18n("Open File..."));

	KURL::List::Iterator it;
	KURL url;

	
	KMacroCommand *macroCommand = new KMacroCommand(i18n("Add Clips"));
	for (it = urlList.begin(); it != urlList.end(); it++) {
	    url = (*it);
	    if (!url.isEmpty()) {
		if (getDocument()->clipManager().findClip(url)) KMessageBox::sorry(this, i18n("The clip %1 is already present in this project").arg(url.filename()));
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
        KDialogBase *dia = new KDialogBase(  KDialogBase::Swallow, i18n("Create New Color Clip"), KDialogBase::Ok | KDialogBase::Cancel, KDialogBase::Ok, this, "create_clip", true);
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
        KDialogBase *dia = new KDialogBase(  KDialogBase::Swallow, i18n("Create New Image Clip"), KDialogBase::Ok | KDialogBase::Cancel, KDialogBase::Ok, this, "create_clip", true);
        createImageClip_UI *clipChoice = new createImageClip_UI(dia);
	clipChoice->imageExtension->hide();
	dia->setMainWidget(clipChoice);
	// Filter for the image producer
	QString filter = "image/gif image/jpeg image/png image/x-bmp";
	clipChoice->url_image->setFilter(filter);
        clipChoice->edit_duration->setText(KdenliveSettings::colorclipduration());
	dia->adjustSize();
	if (dia->exec() == QDialog::Accepted) {
	    QString url = clipChoice->url_image->url();
	    QString extension = QString::null;
	    int ttl = 0;
            
            QString dur = clipChoice->edit_duration->text();
            int frames = (int) ((dur.section(":",0,0).toInt()*3600 + dur.section(":",1,1).toInt()*60 + dur.section(":",2,2).toInt()) * KdenliveSettings::defaultfps() + dur.section(":",3,3).toInt());
            
            GenTime duration(frames , KdenliveSettings::defaultfps());
	    KCommand *command =
		new Command::KAddClipCommand(*m_doc, m_projectList->m_listView->parentName(), KURL(url), extension,
                                              ttl, duration, clipChoice->edit_description->text(), clipChoice->transparentBg->isChecked(), true);
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
		new Command::KAddClipCommand(*m_doc, m_projectList->m_listView->parentName(), KURL(url), extension, ttl, slideDialog->duration(), slideDialog->description(), slideDialog->isTransparent(), true);
	    addCommand(command, true);
	}
	delete slideDialog;
	slotStatusMsg(i18n("Ready."));
    }


/* Create text clip */
    void KdenliveApp::slotProjectAddTextClip() {
        slotStatusMsg(i18n("Adding Clips"));
        activateWorkspaceMonitor();
        titleWidget *txtWidget=new titleWidget(m_workspaceMonitor->screen(), m_doc->projectClip().videoWidth(), m_doc->projectClip().videoHeight(), this,"titler",Qt::WStyle_StaysOnTop | Qt::WType_Dialog | Qt::WDestructiveClose);
        connect(txtWidget->canview,SIGNAL(showPreview(QString)),m_workspaceMonitor->screen(),SLOT(setTitlePreview(QString)));
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
                            new Command::KAddClipCommand(*m_doc, m_projectList->m_listView->parentName(), fd->selectedURL(), QString::null,
                            0, duration, QString::null, false, true);
                    addCommand(command, true);
                }
            }
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
	 

	    KTempFile tmp(KdenliveSettings::currentdefaultfolder(),".png");
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
                titleWidget *txtWidget=new titleWidget(m_workspaceMonitor->screen(), m_doc->projectClip().videoWidth(), m_doc->projectClip().videoHeight(), this,"titler",Qt::WStyle_StaysOnTop | Qt::WType_Dialog | Qt::WDestructiveClose);
                /*connect(txtWidget->canview,SIGNAL(showPreview(QString)),m_workspaceMonitor->screen(),SLOT(setTitlePreview(QString)));*/
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
			if (dia->ttl() != 0) url = url + "/.all." + dia->extension();
                        KCommand *command =
                                new Command::KEditClipCommand(*m_doc, refClip, url, "",dia->ttl(),
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
		    ("This will remove all clips on the timeline that are currently using this clip. Are you sure you want to do this?"))
		== KMessageBox::Continue) {
		// Create a macro command that will delete all clips from the timeline involving this
		// avfile. Then, delete it.
		KMacroCommand *macroCommand =
		    new KMacroCommand(i18n("Delete Clip"));

		DocClipRefList list =
		    m_doc->referencedClips(m_projectList->
		    currentSelection()->referencedClip());

		QPtrListIterator < DocClipRef > itt(list);

		while (itt.current()) {
		    Command::KAddRefClipCommand * command =
			new Command::KAddRefClipCommand(m_doc->
			effectDescriptions(), m_doc->clipManager(),
			&(m_doc->projectClip()), itt.current(), false);
		    macroCommand->addCommand(command);
		    ++itt;
		}

		// NOTE - we clear the monitors of the clip here - this does _not_ go into the macro
		// command.
		int id = clip->getId();

		m_monitorManager.clearClip(clip);

		// remove thumbnail file
		if (clip->clipType() == DocClipBase::AUDIO  || clip->clipType() == DocClipBase::VIDEO) {
		KMD5 context ((KFileItem(clip->fileURL(),"text/plain", S_IFREG).timeString() + clip->fileURL().fileName()).ascii());
		KIO::NetAccess::del(KURL(KdenliveSettings::currentdefaultfolder() + "/" + context.hexDigest().data() + ".thumb"), this);
		}

		DocumentBaseNode *node =
		    m_doc->findClipNode(refClip->name());

		macroCommand->addCommand(new Command::KAddClipCommand(*m_doc,
			node->name(), clip, node->parent(), false));
		addCommand(macroCommand, true);

		getDocument()->clipManager().removeClip(id);
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
		i18n("Clean Project removes files from the project that are unused.\
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


    void KdenliveApp::slotTogglePlay() {
	slotStatusMsg(i18n("Starting/stopping playback"));
	if (m_monitorManager.hasActiveMonitor()) {
	    m_monitorManager.activeMonitor()->editPanel()->togglePlay();
	}
	slotStatusMsg(i18n("Ready."));
    }

    void KdenliveApp::slotPlay() {
	slotStatusMsg(i18n("Play at normal speed"));
	if (m_monitorManager.hasActiveMonitor()) {
	    m_monitorManager.activeMonitor()->editPanel()->play();
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

    void KdenliveApp::slotNextFrame() {
	slotStatusMsg(i18n("Seeking Forwards one frame"));
	if (m_monitorManager.hasActiveMonitor()) {
	    m_monitorManager.activeMonitor()->editPanel()->
		seek(m_monitorManager.activeMonitor()->screen()->
		seekPosition() + GenTime(1,
		    getDocument()->framesPerSecond()));
	    m_timeline->ensureCursorVisible();
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
	}
    }
    
    void KdenliveApp::slotNextSnap() {
        if (m_monitorManager.hasActiveMonitor()) {
            m_monitorManager.activeMonitor()->editPanel()->
                    seek(m_doc->toSnapTime(m_monitorManager.activeMonitor()->screen()->
                    seekPosition()));
	    m_timeline->ensureCursorVisible();
        }
    }

    void KdenliveApp::slotPreviousSnap() {
        if (m_monitorManager.hasActiveMonitor()) {
            m_monitorManager.activeMonitor()->editPanel()->
                    seek(m_doc->toSnapTime(m_monitorManager.activeMonitor()->screen()->
                    seekPosition(), false));
	    m_timeline->ensureCursorVisible();
        }
    }

    void KdenliveApp::slotNextSecond() {
        if (m_monitorManager.hasActiveMonitor()) {
            m_monitorManager.activeMonitor()->editPanel()->
                    seek(m_monitorManager.activeMonitor()->screen()->
                    seekPosition() + GenTime((int) getDocument()->framesPerSecond(),
            getDocument()->framesPerSecond()));
	    m_timeline->ensureCursorVisible();
        }
    }

    void KdenliveApp::slotLastSecond() {
        if (m_monitorManager.hasActiveMonitor()) {
            m_monitorManager.activeMonitor()->editPanel()->
                    seek(m_monitorManager.activeMonitor()->screen()->
                    seekPosition() - GenTime((int) getDocument()->framesPerSecond(),
            getDocument()->framesPerSecond()));
	    m_timeline->ensureCursorVisible();
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
	addCommand(Command::KAddRefClipCommand::
	    deleteSelectedClips(getDocument()), true);
	getDocument()->activateSceneListGeneration(true);
	slotStatusMsg(i18n("Ready."));
    }
    
    void KdenliveApp::clipReferenceChanged() {
        m_projectList->updateReference();
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
	KdenliveSettings::setCurrenttmpfolder(KdenliveSettings::currentdefaultfolder() + "/tmp/");
	}
    }

    void KdenliveApp::slot_moveClips(QDropEvent * event, QListViewItem * parent) {
	DocClipRefList clips =
	    ClipDrag::decode(getDocument()->effectDescriptions(),
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
	    ClipDrag::decode(getDocument()->effectDescriptions(),
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
		QValueVector < GenTime > times =
		    itt.current()->snapMarkers();

		for (QValueVector < GenTime >::iterator timeItt =
		    times.begin(); timeItt != times.end(); ++timeItt) {
		    Command::KAddMarkerCommand * command =
			new Command::KAddMarkerCommand(*getDocument(),
			itt.current(), *timeItt, false);
		    macroCommand->addCommand(command);
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
	m_rulerPopupMenu = (QPopupMenu *) factory()->container("ruler_context", this);
	QStringList guides = m_timeline->timelineRulerComments();
	m_rulerPopupMenu->removeItemAt(3);
	if (!guides.isEmpty()) {
	    QPopupMenu *gotoGuide = new QPopupMenu;
	    uint ct = 100;
	    for ( QStringList::Iterator it = guides.begin(); it != guides.end(); ++it ) {
		gotoGuide->insertItem(*it, ct);
		ct++;
	    }
	    connect(gotoGuide, SIGNAL(activated(int)), m_timeline, SLOT(gotoGuide(int)));
	    m_rulerPopupMenu->insertItem(i18n("Go To"), gotoGuide);
	}
        // display menu
        m_rulerPopupMenu->popup(QCursor::pos());
    }

    void KdenliveApp::slotDisplayTimeLineContextMenu() {

	int ix = m_timeline->trackView()->panelAt(m_timeline->trackView()->mapFromGlobal(QCursor::pos()).y())->documentTrackIndex();

	DocTrackBase *track = getDocument()->track(ix);
	GenTime mouseTime;
	mouseTime = m_timeline->timeUnderMouse(m_timeline->trackView()->mapFromGlobal(QCursor::pos()).x());

	DocClipRef *clip = track->getClipAt(mouseTime);
	if (clip) {
          // select clip under mouse
          addCommand(Command::KSelectClipCommand::selectNone(getDocument()), true);
          addCommand(Command::KSelectClipCommand::selectClipAt(getDocument(), *track, mouseTime),        true);
          //track->selectClip(clip, true);
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


    void KdenliveApp::addTransition() {
        if (getDocument()->projectClip().hasSelectedClips() == 0) {
            KMessageBox::sorry(this, i18n("Please select a clip to apply transition"));
            return;
        }
        GenTime mouseTime;
	QPoint position = mousePosition();
        mouseTime = m_timeline->timeUnderMouse(m_timeline->trackView()->mapFromGlobal(position).x());
	addCommand(Command::KAddTransitionCommand::appendTransition(getDocument()->projectClip().selectedClip(), mouseTime), true);
	getDocument()->indirectlyModified();
    }
    
    void KdenliveApp::deleteTransition() {
        if (getDocument()->projectClip().hasSelectedClips() == 0) {
            KMessageBox::sorry(this, i18n("Please select a clip to delete transition"));
            return;
        }
	GenTime mouseTime;
	QPoint position = mousePosition();
	mouseTime = m_timeline->timeUnderMouse(m_timeline->trackView()->mapFromGlobal(position).x());
	addCommand(Command::KAddTransitionCommand::removeTransition(getDocument()->projectClip().selectedClip(), mouseTime), true);
	getDocument()->indirectlyModified();
    }

    void KdenliveApp::switchTransition() {
        GenTime mouseTime;
        mouseTime = m_timeline->timeUnderMouse(m_timeline->trackView()->mapFromGlobal(QCursor::pos()).x());
        getDocument()->projectClip().switchTransition(mouseTime);
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
	    disconnect(trackItt.current(), SIGNAL(clipLayoutChanged()),
		m_timeline, SLOT(drawTrackViewBackBuffer()));
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

            connect(trackItt.current(), SIGNAL(clipLayoutChanged()),
                    m_timeline, SLOT(drawTrackViewBackBuffer()));
            connect(trackItt.current(), SIGNAL(clipSelectionChanged()),
                    m_timeline, SLOT(drawTrackViewBackBuffer()));

	    ++trackItt;
	}
        if (KdenliveSettings::videothumbnails()) getDocument()->updateTracksThumbnails();
	getDocument()->refreshAudioThumbnails();
	//m_timeline->resizeTracks();
    }

    void KdenliveApp::slotRazorAllClips() {
	addCommand(Command::DocumentMacroCommands::
	    razorAllClipsAt(getDocument(), m_timeline->seekPosition()),
	    true);
    }

    void KdenliveApp::slotRazorSelectedClips() {
	addCommand(Command::DocumentMacroCommands::
	    razorSelectedClipsAt(getDocument(),
		m_timeline->seekPosition()), true);
    }

}				// namespace Gui
