
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
#include <kstatusbar.h>
#include <kstdaction.h>
#include <kcolorbutton.h>
#include <kurlrequester.h>
#include <klineedit.h>
#include <kurlrequesterdlg.h>

// application specific includes
// p.s., get the idea this class is kind, central to everything?
#include "capturemonitor.h"
#include "clipdrag.h"
#include "clippropertiesdialog.h"
#include "configureprojectdialog.h"
#include "docclipbase.h"
#include "docclipavfile.h"
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
#include "renderdebugpanel.h"


#include "trackpanelclipmovefunction.h"
#include "trackpanelrazorfunction.h"
#include "trackpanelspacerfunction.h"
#include "trackpanelclipresizefunction.h"
#include "trackpanelcliprollfunction.h"
#include "trackpanelmarkerfunction.h"
#include "trackpanelselectnonefunction.h"
#include "trackpanelkeyframefunction.h"

#define ID_STATUS_MSG 1
#define ID_EDITMODE_MSG 2
#define ID_CURTIME_MSG 3

namespace Gui
{

KdenliveApp::KdenliveApp( QWidget* , const char* name ) :
		KDockMainWindow( 0, name ),
		m_monitorManager( this ),
		m_workspaceMonitor( NULL ),
		m_captureMonitor( NULL )
{
	config = kapp->config();

	// renderer options
	m_renderManager = new KRenderManager();
	m_renderManager->readConfig( config );

	// call inits to invoke all other construction parts
	initStatusBar();
	m_commandHistory = new KCommandHistory( actionCollection(), true );
	initActions();
	initDocument();
	initView();
	readOptions();

	// disable actions at startup
	fileSave->setEnabled( false );
	//  filePrint->setEnabled(false);
	editCut->setEnabled( false );
	editCopy->setEnabled( false );
	editPaste->setEnabled( false );

	fileSaveAs->setEnabled( true );

	// Reopen last project if user asked it
	if (KdenliveSettings::openlast())
	{
	config->setGroup("RecentFiles");
	QString Lastproject = config->readPathEntry("File1");
	if (!Lastproject.isEmpty()) slotFileOpenRecent(KURL(Lastproject));
	}
}

KdenliveApp::~KdenliveApp()
{
	if ( m_renderManager ) delete m_renderManager;
	if ( m_commandHistory ) delete m_commandHistory;
}

void KdenliveApp::initActions()
{
	fileNew = KStdAction::openNew( this, SLOT( slotFileNew() ), actionCollection() );
	fileOpen = KStdAction::open( this, SLOT( slotFileOpen() ), actionCollection() );
	fileOpenRecent = KStdAction::openRecent( this, SLOT( slotFileOpenRecent( const KURL& ) ), actionCollection() );
	fileSave = KStdAction::save( this, SLOT( slotFileSave() ), actionCollection() );
	fileSaveAs = KStdAction::saveAs( this, SLOT( slotFileSaveAs() ), actionCollection() );
	//fileClose = KStdAction::close(this, SLOT(slotFileClose()), actionCollection());
	//  filePrint = KStdAction::print(this, SLOT(slotFilePrint()), actionCollection());
	fileQuit = KStdAction::quit( this, SLOT( slotFileQuit() ), actionCollection() );
	editCut = KStdAction::cut( this, SLOT( slotEditCut() ), actionCollection() );
	editCopy = KStdAction::copy( this, SLOT( slotEditCopy() ), actionCollection() );
	editPaste = KStdAction::paste( this, SLOT( slotEditPaste() ), actionCollection() );
	viewToolBar = KStdAction::showToolbar( this, SLOT( slotViewToolBar() ), actionCollection() );
	viewStatusBar = KStdAction::showStatusbar( this, SLOT( slotViewStatusBar() ), actionCollection() );
	optionsPreferences = KStdAction::preferences( this, SLOT( slotOptionsPreferences() ), actionCollection() );
	keyBindings = KStdAction::keyBindings( this, SLOT( slotConfKeys() ), actionCollection() );
	configureToolbars = KStdAction::configureToolbars( this, SLOT( slotConfToolbars() ), actionCollection() );
	fitToWidth = KStdAction::fitToWidth( this, SLOT( slotFitToWidth() ), actionCollection() );

	timelineMoveTool = new KRadioAction( i18n( "Move/Resize Tool" ), "moveresize.png", KShortcut( Qt::Key_Q ), this, SLOT( slotTimelineMoveTool() ), actionCollection(), "timeline_move_tool" );
	timelineRazorTool = new KRadioAction( i18n( "Razor Tool" ), "razor.png", KShortcut( Qt::Key_W ), this, SLOT( slotTimelineRazorTool() ), actionCollection(), "timeline_razor_tool" );
	timelineSpacerTool = new KRadioAction( i18n( "Spacing Tool" ), "spacer.png", KShortcut( Qt::Key_E ), this, SLOT( slotTimelineSpacerTool() ), actionCollection(), "timeline_spacer_tool" );
	timelineMarkerTool = new KRadioAction( i18n( "Marker Tool" ), "marker.png", KShortcut( Qt::Key_M ), this, SLOT( slotTimelineMarkerTool() ), actionCollection(), "timeline_marker_tool" );
	timelineRollTool = new KRadioAction( i18n( "Roll Tool" ), "rolltool.png", KShortcut( Qt::Key_R ), this, SLOT( slotTimelineRollTool() ), actionCollection(), "timeline_roll_tool" );

	timelineSnapToFrame = new KToggleAction( i18n( "Snap To Frames" ), "snaptoframe.png", 0, this, SLOT( slotTimelineSnapToFrame() ), actionCollection(), "timeline_snap_frame" );
	timelineSnapToBorder = new KToggleAction( i18n( "Snap To Border" ), "snaptoborder.png", 0, this, SLOT( slotTimelineSnapToBorder() ), actionCollection(), "timeline_snap_border" );
	timelineSnapToMarker = new KToggleAction( i18n( "Snap To Marker" ), "snaptomarker.png", 0, this, SLOT( slotTimelineSnapToMarker() ), actionCollection(), "timeline_snap_marker" );

	projectAddClips = new KAction( i18n( "Add Clips" ), "addclips.png", 0, this, SLOT( slotProjectAddClips() ), actionCollection(), "project_add_clip" );

	projectAddColorClip = new KAction( i18n( "Create Color Clip" ), "addclips.png", 0, this, SLOT( slotProjectAddColorClip() ), actionCollection(), "project_add_color_clip" );

	projectAddImageClip = new KAction( i18n( "Create Image Clip" ), "addclips.png", 0, this, SLOT( slotProjectAddImageClip() ), actionCollection(), "project_add_image_clip" );

	projectAddImageClip = new KAction( i18n( "Create Text Clip" ), "addclips.png", 0, this, SLOT( slotProjectAddTextClip() ), actionCollection(), "project_add_text_clip" );


	projectDeleteClips = new KAction( i18n( "Delete Clips" ), "deleteclips.png", 0, this, SLOT( slotProjectDeleteClips() ), actionCollection(), "project_delete_clip" );
	projectClean = new KAction( i18n( "Clean Project" ), "cleanproject.png", 0, this, SLOT( slotProjectClean() ), actionCollection(), "project_clean" );
	projectClipProperties = new KAction( i18n( "Clip properties" ), "clipproperties.png", 0, this, SLOT( slotProjectClipProperties() ), actionCollection(), "project_clip_properties" );

	renderExportTimeline = new KAction( i18n( "&Export Timeline" ), "exportvideo.png", 0, this, SLOT( slotRenderExportTimeline() ), actionCollection(), "render_export_timeline" );
	configureProject = new KAction( i18n( "&Configure Project" ), "configureproject.png", 0, this, SLOT( slotConfigureProject() ), actionCollection(), "configure_project" );

	actionSeekForwards = new KAction( i18n( "Seek &Forwards" ), KShortcut(), this, SLOT( slotSeekForwards() ), actionCollection(), "seek_forwards" );
	actionSeekBackwards = new KAction( i18n( "Seek Backwards" ), KShortcut(), this, SLOT( slotSeekBackwards() ), actionCollection(), "seek_backwards" );
	actionTogglePlay = new KAction( i18n( "Start/Stop" ), KShortcut( Qt::Key_Space ), this, SLOT( slotTogglePlay() ), actionCollection(), "toggle_play" );
	actionTogglePlaySelected = new KAction( i18n( "Play Selection" ), KShortcut( Qt::Key_Space | Qt::CTRL), this, SLOT( slotTogglePlaySelected() ), actionCollection(), "toggle_play_selection" );
	actionNextFrame = new KAction( i18n( "Forward one frame" ), KShortcut( Qt::Key_Right ), this, SLOT( slotNextFrame() ), actionCollection(), "forward_frame" );
	actionLastFrame = new KAction( i18n( "Back one frame" ), KShortcut( Qt::Key_Left ), this, SLOT( slotLastFrame() ), actionCollection(), "backward_frame" );
	actionSetInpoint = new KAction( i18n( "Set inpoint" ), KShortcut( Qt::Key_I ), this, SLOT( slotSetInpoint() ), actionCollection(), "set_inpoint" );
	actionSetOutpoint = new KAction( i18n( "Set outpoint" ), KShortcut( Qt::Key_O ), this, SLOT( slotSetOutpoint() ), actionCollection(), "set_outpoint" );
	actionDeleteSelected = new KAction( i18n( "Delete Selected Clips" ), KShortcut( Qt::Key_Delete ), this, SLOT( slotDeleteSelected() ), actionCollection(), "delete_selected_clips" );

	actionToggleSnapMarker  = new KAction( i18n( "Toggle Snap Marker" ), KShortcut( Qt::Key_Period ), this, SLOT( slotToggleSnapMarker() ), actionCollection(), "toggle_snap_marker" );
	actionClearAllSnapMarkers = new KAction( i18n( "Clear All Snap Markers" ), KShortcut(), this, SLOT( slotClearAllSnapMarkers()), actionCollection(), "clear_all_snap_markers" );
	actionClearSnapMarkersFromSelected = new KAction( i18n( "Clear Snap Markers From Selected" ), KShortcut(), this, SLOT( slotClearSnapMarkersFromSelected() ), actionCollection(), "clear_snap_markers_from_selected" );

	actionLoadLayout1 = new KAction( i18n( "Load Layout &1" ), "loadlayout1.png", KShortcut( Qt::Key_F9 ), this, SLOT( loadLayout1() ), actionCollection(), "load_layout_1" );
	actionLoadLayout2 = new KAction( i18n( "Load Layout &2" ), "loadlayout2.png", KShortcut( Qt::Key_F10 ), this, SLOT( loadLayout2() ), actionCollection(), "load_layout_2" );
	actionLoadLayout3 = new KAction( i18n( "Load Layout &3" ), "loadlayout3.png", KShortcut( Qt::Key_F11 ), this, SLOT( loadLayout3() ), actionCollection(), "load_layout_3" );
	actionLoadLayout4 = new KAction( i18n( "Load Layout &4" ), "loadlayout4.png", KShortcut( Qt::Key_F12 ), this, SLOT( loadLayout4() ), actionCollection(), "load_layout_4" );
	actionSaveLayout1 = new KAction( i18n( "Save Layout &1" ), KShortcut( Qt::Key_F9 | Qt::CTRL | Qt::SHIFT ), this, SLOT( saveLayout1() ), actionCollection(), "save_layout_1" );
	actionSaveLayout2 = new KAction( i18n( "Save Layout &2" ), KShortcut( Qt::Key_F10 | Qt::CTRL | Qt::SHIFT ), this, SLOT( saveLayout2() ), actionCollection(), "save_layout_2" );
	actionSaveLayout3 = new KAction( i18n( "Save Layout &3" ), KShortcut( Qt::Key_F11 | Qt::CTRL | Qt::SHIFT ), this, SLOT( saveLayout3() ), actionCollection(), "save_layout_3" );
	actionSaveLayout4 = new KAction( i18n( "Save Layout &4" ), KShortcut( Qt::Key_F12 | Qt::CTRL | Qt::SHIFT ), this, SLOT( saveLayout4() ), actionCollection(), "save_layout_4" );

	timelineRazorAllClips  = new KAction( i18n( "Razor All Clips" ), KShortcut( Qt::Key_W | Qt::CTRL | Qt::SHIFT), this, SLOT( slotRazorAllClips() ), actionCollection(), "razor_all_clips" );
	timelineRazorSelectedClips  = new KAction( i18n( "Razor Selected Clips" ), KShortcut( Qt::Key_W | Qt::SHIFT), this, SLOT( slotRazorSelectedClips() ), actionCollection(), "razor_selected_clips" );

	(void) new KAction( i18n( "Clip Monitor" ), 0, this, SLOT( slotToggleClipMonitor() ), actionCollection(), "toggle_clip_monitor" );
	(void) new KAction( i18n( "Workspace Monitor" ), 0, this, SLOT( slotToggleWorkspaceMonitor() ), actionCollection(), "toggle_workspace_monitor" );
	(void) new KAction( i18n( "Effect List" ), 0, this, SLOT( slotToggleEffectList() ), actionCollection(), "toggle_effect_list" );
	(void) new KAction( i18n( "Effect Stack" ), 0, this, SLOT( slotToggleEffectStack() ), actionCollection(), "toggle_effect_stack" );
	(void) new KAction( i18n( "Project List" ), 0, this, SLOT( slotToggleProjectList() ), actionCollection(), "toggle_project_list" );

	timelineMoveTool->setExclusiveGroup( "timeline_tools" );
	timelineRazorTool->setExclusiveGroup( "timeline_tools" );
	timelineSpacerTool->setExclusiveGroup( "timeline_tools" );
	timelineMarkerTool->setExclusiveGroup( "timeline_tools" );
	timelineRollTool->setExclusiveGroup( "timeline_tools" );

	fileNew->setStatusText( i18n( "Creates a new document" ) );
	fileOpen->setStatusText( i18n( "Opens an existing document" ) );
	fileOpenRecent->setStatusText( i18n( "Opens a recently used file" ) );
	fileSave->setStatusText( i18n( "Saves the actual document" ) );
	fileSaveAs->setStatusText( i18n( "Saves the actual document as..." ) );
	//  fileClose->setStatusText(i18n("Closes the actual document"));
	//  filePrint ->setStatusText(i18n("Prints out the actual document"));
	fileQuit->setStatusText( i18n( "Quits the application" ) );
	editCut->setStatusText( i18n( "Cuts the selected section and puts it to the clipboard" ) );
	editCopy->setStatusText( i18n( "Copies the selected section to the clipboard" ) );
	editPaste->setStatusText( i18n( "Pastes the clipboard contents to actual position" ) );
	viewToolBar->setStatusText( i18n( "Enables/disables the toolbar" ) );
	viewStatusBar->setStatusText( i18n( "Enables/disables the statusbar" ) );
	timelineMoveTool->setStatusText( i18n( "Moves and resizes the document" ) );
	timelineRazorTool->setStatusText( i18n( "Chops clips into two pieces" ) );
	timelineSpacerTool->setStatusText( i18n( "Shifts all clips to the right of mouse" ) );
	timelineMarkerTool->setStatusText( i18n( "Place snap marks on clips and the timeline to aid accurate positioning of clips" ) );
	timelineRollTool->setStatusText( i18n( "Move edit point between two selected clips" ) );
	timelineSnapToFrame->setStatusText( i18n( "Clips will align to the nearest frame" ) );
	timelineSnapToBorder->setStatusText( i18n( "Clips will align with the borders of other clips" ) );
	timelineSnapToMarker->setStatusText( i18n( "Clips will align with user-defined snap markers" ) );
	projectAddClips->setStatusText( i18n( "Add clips to the project" ) );
	projectDeleteClips->setStatusText( i18n( "Remove clips from the project" ) );
	projectClean->setStatusText( i18n( "Remove unused clips from the project" ) );
	//projectClipProperties->setStatusText( i18n( "View the properties of the selected clip" ) );
	actionSeekForwards->setStatusText( i18n( "Seek forward one frame" ) );
	actionSeekBackwards->setStatusText( i18n( "Seek backwards one frame" ) );
	actionTogglePlay->setStatusText( i18n( "Start or stop playback" ) );
	actionTogglePlay->setStatusText( i18n( "Start or stop playback of inpoint/outpoint selection" ) );
	actionNextFrame->setStatusText( i18n( "Move the current position forwards by one frame" ) );
	actionLastFrame->setStatusText( i18n( "Move the current position backwards by one frame" ) );
	actionSetInpoint->setStatusText( i18n( "Set the inpoint to the current position" ) );
	actionSetOutpoint->setStatusText( i18n( "Set the outpoint to the current position" ) );
	actionDeleteSelected->setStatusText( i18n( "Delete currently selected clips" ) );

	actionToggleSnapMarker->setStatusText( i18n("Toggle a snap marker at the current monitor position"));
	actionClearAllSnapMarkers ->setStatusText( i18n("Toggle a snap marker at the current monitor position"));
	actionClearSnapMarkersFromSelected->setStatusText( i18n("Toggle a snap marker at the current monitor position"));

	renderExportTimeline->setStatusText( i18n( "Render the timeline to a file" ) );
	configureProject->setStatusText( i18n( "Configure the format for this project" ) );

	timelineRazorAllClips->setStatusText( i18n("Razor all clips on the timeline at the current seek position"));
	timelineRazorSelectedClips->setStatusText( i18n("Razor all selected clips on the timeline at the current seek position"));

	// use the absolute path to your kdenliveui.rc file for testing purpose in createGUI();
	createGUI( "kdenliveui.rc" );

	timelineMoveTool->setChecked( true );
	timelineSnapToBorder->setChecked( true );
	timelineSnapToFrame->setChecked( true );
	timelineSnapToMarker->setChecked( true );
}


void KdenliveApp::initStatusBar()
{
	///////////////////////////////////////////////////////////////////
	// STATUSBAR
	// TODO: add your own items you need for displaying current application status.
	statusBar() ->insertItem( i18n( "Ready." ), ID_STATUS_MSG );

	m_statusBarProgress = new KProgress( statusBar() );
	m_statusBarProgress->setTextEnabled( false );

	statusBar() ->addWidget( m_statusBarProgress );

	statusBar() ->insertItem( i18n( "Move/Resize mode" ), ID_EDITMODE_MSG, 0, true );
	statusBar() ->insertItem( i18n( "Current Time : " ) + "00:00:00.00", ID_CURTIME_MSG, 0, true );
}

void KdenliveApp::initDocument()
{
	doc = new KdenliveDoc( this, this );
	connect( doc, SIGNAL( modified( bool ) ), this, SLOT( documentModified( bool ) ) );
	doc->newDocument();
}

void KdenliveApp::initView()
{
	////////////////////////////////////////////////////////////////////
	// create the main widget here that is managed by KTMainWindow's view-region and
	// connect the widget to your document to display document contents.

	view = new QWidget( this );
	KDockWidget *mainDock = createDockWidget( "Application", QPixmap(), this, i18n( "Application" ) );
	mainDock->setWidget( view );
	mainDock->setDockSite( KDockWidget::DockFullSite );
	setCentralWidget( mainDock );
	setMainDockWidget( mainDock );
	setCaption( doc->URL().fileName(), false );

	KDockWidget *widget = createDockWidget( i18n( "TimeLine" ), QPixmap(), 0, i18n( "TimeLine" ) );
	m_timeline = new KMMTimeLine( NULL, widget );
	widget->setWidget( m_timeline );
	widget->setDockSite( KDockWidget::DockFullSite );
	widget->setDockSite( KDockWidget::DockCorner );
	widget->manualDock( mainDock, KDockWidget::DockBottom );

	m_dockProjectList = createDockWidget( "Project List", QPixmap(), 0, i18n( "Project List" ) );
	m_projectList = new ProjectList( this, getDocument(), m_dockProjectList );
	//QToolTip::add( m_dockProjectList, i18n( "Video files usable in current project" ) );
	QWhatsThis::add( m_dockProjectList, i18n( "Video files usable in your project. "
		                                    "Add or remove files with the contextual menu. "
		                                    "In order to add sequences to the current video project, use the drag and drop." ) );
	m_dockProjectList->setWidget( m_projectList );
	m_dockProjectList->setDockSite( KDockWidget::DockFullSite );
	m_dockProjectList->manualDock( mainDock, KDockWidget::DockLeft );

	widget = createDockWidget( "Debug", QPixmap(), 0, i18n( "Debug" ) );
	m_renderDebugPanel = new RenderDebugPanel( widget );
	QToolTip::add( widget, i18n( "Current debug messages between renderer and Kdenlive" ) );
	widget->setWidget( m_renderDebugPanel );
	widget->setDockSite( KDockWidget::DockFullSite );
	widget->manualDock( m_dockProjectList, KDockWidget::DockCenter );

	m_dockEffectList = createDockWidget( "Effect List", QPixmap(), 0, i18n( "Effect List" ) );
	m_effectListDialog = new EffectListDialog( getDocument() ->renderer() ->effectList(), widget, "effect list" );
	QToolTip::add( m_dockEffectList, i18n( "Current effects usable with the renderer" ) );
	m_dockEffectList->setWidget( m_effectListDialog );
	m_dockEffectList->setDockSite( KDockWidget::DockFullSite );
	m_dockEffectList->manualDock( m_dockProjectList, KDockWidget::DockCenter );

	widget = createDockWidget( "Effect Setup", QPixmap(), 0, i18n( "Effect Setup" ) );
	m_effectParamDialog = new EffectParamDialog( this, getDocument(), widget, "effect setup" );
	QToolTip::add( widget, i18n( "Edit the parameters of the currently selected effect." ) );
	widget->setWidget( m_effectParamDialog );
	widget->setDockSite( KDockWidget::DockFullSite );
	widget->manualDock( m_dockProjectList, KDockWidget::DockCenter );

	clipWidget = createDockWidget( "Clip Properties", QPixmap(), 0, i18n( "Clip Properties" ) );
	m_clipPropertyDialog = new ClipPropertiesDialog( clipWidget, "clip properties" );
	QToolTip::add( clipWidget, i18n( "Properties of the currently selected clip." ) );
	clipWidget->setWidget( m_clipPropertyDialog );
	clipWidget->setDockSite( KDockWidget::DockFullSite );
	clipWidget->manualDock( m_dockProjectList, KDockWidget::DockCenter );

	m_dockEffectStack = createDockWidget( "Effect Stack", QPixmap(), 0, i18n( "Effect Stack" ) );
	m_effectStackDialog = new EffectStackDialog( this, getDocument(), m_dockEffectStack, "effect stack" );
	QToolTip::add( m_dockEffectStack, i18n( "All effects on the currently selected clip." ) );
	m_dockEffectStack->setWidget( m_effectStackDialog );
	m_dockEffectStack->setDockSite( KDockWidget::DockFullSite );
	m_dockEffectStack->manualDock( m_dockProjectList, KDockWidget::DockCenter );

	m_dockWorkspaceMonitor = createDockWidget( "Workspace Monitor", QPixmap(), 0, i18n( "Workspace Monitor" ) );
	m_workspaceMonitor = m_monitorManager.createMonitor( getDocument(), m_dockWorkspaceMonitor,  "Workspace Monitor" );
	m_workspaceMonitor->setNoSeek( true );
	m_dockWorkspaceMonitor->setWidget( m_workspaceMonitor );
	m_dockWorkspaceMonitor->setDockSite( KDockWidget::DockFullSite );
	m_dockWorkspaceMonitor->manualDock( mainDock, KDockWidget::DockRight );

	m_dockClipMonitor = createDockWidget( "Clip Monitor", QPixmap(), 0, i18n( "Clip Monitor" ) );
	m_clipMonitor = m_monitorManager.createMonitor( getDocument(), m_dockClipMonitor, "Clip Monitor" );
	m_dockClipMonitor->setWidget( m_clipMonitor );
	m_dockClipMonitor->setDockSite( KDockWidget::DockFullSite );
	//m_dockClipMonitor->manualDock( m_dockWorkspaceMonitor, KDockWidget::DockCenter );

//	m_dockCaptureMonitor = createDockWidget( "Capture Monitor", QPixmap(), 0, i18n( "Capture Monitor" ) );
//	m_captureMonitor = m_monitorManager.createCaptureMonitor( getDocument(), m_dockCaptureMonitor, i18n( "Capture Monitor" ) );
//	m_dockCaptureMonitor->setWidget( m_captureMonitor );
//	m_dockCaptureMonitor->setDockSite( KDockWidget::DockFullSite );
//	m_dockCaptureMonitor->manualDock( m_dockWorkspaceMonitor, KDockWidget::DockCenter );

	setBackgroundMode( PaletteBase );


	connect( m_workspaceMonitor, SIGNAL( seekPositionChanged( const GenTime & ) ), m_timeline, SLOT( seek( const GenTime & ) ) );

	//  COMMENTED BECAUSE IT CAUSES RANDOM CRASHES
	//connect( m_workspaceMonitor, SIGNAL( seekPositionChanged( const GenTime & ) ), this, SLOT( slotUpdateCurrentTime( const GenTime & ) ) );
	//connect editpanel sliders with timeline sliders -reh
	connect( m_workspaceMonitor, SIGNAL( inpointPositionChanged( const GenTime & ) ), m_timeline, SLOT( setInpointTimeline( const GenTime & ) ) );
	connect( m_workspaceMonitor, SIGNAL( outpointPositionChanged( const GenTime & ) ), m_timeline, SLOT( setOutpointTimeline( const GenTime & ) ) );
	

	connect( getDocument(), SIGNAL( signalClipSelected( DocClipRef *) ), this, SLOT( slotSetClipMonitorSource( DocClipRef * ) ) );
	connect( getDocument(), SIGNAL( signalClipSelected( DocClipRef *) ), m_effectStackDialog, SLOT( slotSetEffectStack( DocClipRef * ) ) );
	connect( getDocument(), SIGNAL( effectStackChanged( DocClipRef *) ), m_effectStackDialog, SLOT( slotSetEffectStack( DocClipRef * ) ) );
	connect( m_effectStackDialog, SIGNAL( generateSceneList() ), getDocument(), SLOT( hasBeenModified() ) );

	connect(m_effectStackDialog, SIGNAL( effectSelected(DocClipRef *, Effect *) ), m_effectParamDialog, SLOT( slotSetEffect(DocClipRef *, Effect *)));

	connect( getDocument(), SIGNAL( clipListUpdated() ), m_projectList, SLOT( slot_UpdateList() ) );
	connect( getDocument(), SIGNAL( clipChanged( DocClipRef * ) ), m_projectList, SLOT( slot_clipChanged( DocClipRef * ) ) );
	connect( getDocument(), SIGNAL( nodeDeleted( DocumentBaseNode * ) ), m_projectList, SLOT( slot_nodeDeleted( DocumentBaseNode * ) ) );

	connect( getDocument(), SIGNAL( documentChanged( DocClipBase * ) ), m_workspaceMonitor, SLOT( slotSetClip( DocClipBase * ) ) );


	connect( getDocument() ->renderer(), SIGNAL( effectListChanged( const QPtrList<EffectDesc> & ) ), m_effectListDialog, SLOT( setEffectList( const QPtrList<EffectDesc> & ) ) );

	connect( getDocument() ->renderer(), SIGNAL( rendering( const GenTime & ) ), this, SLOT( slotSetRenderProgress( const GenTime & ) ) );
	connect( getDocument() ->renderer(), SIGNAL( renderFinished() ), this, SLOT( slotSetRenderFinished() ) );

	connect(getDocument(), SIGNAL(trackListChanged()), this, SLOT(slotSyncTimeLineWithDocument()));
	connect(getDocument(), SIGNAL(clipChanged(DocClipRef* )), m_timeline, SLOT(invalidateBackBuffer()));
	connect(getDocument(), SIGNAL(documentLengthChanged(const GenTime& )), m_timeline, SLOT(slotSetProjectLength(const GenTime& )));

	connect( m_renderManager, SIGNAL( renderDebug( const QString &, const QString & ) ), m_renderDebugPanel, SLOT( slotPrintRenderDebug( const QString &, const QString & ) ) );
	connect( m_renderManager, SIGNAL( renderWarning( const QString &, const QString & ) ), m_renderDebugPanel, SLOT( slotPrintRenderWarning( const QString &, const QString & ) ) );
	connect( m_renderManager, SIGNAL( renderError( const QString &, const QString & ) ), m_renderDebugPanel, SLOT( slotPrintRenderError( const QString &, const QString & ) ) );
	connect( m_renderManager, SIGNAL( recievedStdout( const QString &, const QString & ) ), m_renderDebugPanel, SLOT( slotPrintWarning( const QString &, const QString & ) ) );
	connect( m_renderManager, SIGNAL( recievedStderr( const QString &, const QString & ) ), m_renderDebugPanel, SLOT( slotPrintError( const QString &, const QString & ) ) );
	connect( m_renderManager, SIGNAL( error( const QString &, const QString & ) ), this, SLOT( slotRenderError( const QString &, const QString & ) ) );

	connect( m_renderDebugPanel, SIGNAL(debugVemlSendRequest(const QString &, const QString &)),
			m_renderManager, SLOT(sendDebugCommand(const QString &, const QString &)));

	connect( m_projectList, SIGNAL( clipSelected( DocClipRef * ) ), this, SLOT( activateClipMonitor() ) );
	connect( m_projectList, SIGNAL( clipSelected( DocClipRef * ) ), this, SLOT( slotSetClipMonitorSource( DocClipRef * ) ) );
	connect( m_projectList, SIGNAL( clipSelected( DocClipRef *) ), this, SLOT( slotProjectClipProperties( DocClipRef * ) ) );
	connect( m_projectList, SIGNAL( dragDropOccured( QDropEvent * ) ), this, SLOT( slot_insertClips( QDropEvent * ) ) );

	if(m_workspaceMonitor) {
		connect( m_timeline, SIGNAL( seekPositionChanged( const GenTime & ) ), m_workspaceMonitor->editPanel(), SLOT( seek( const GenTime & ) ) );
		//connect timeline sliders with editpanel sliders -reh
		connect( m_timeline, SIGNAL( inpointPositionChanged( const GenTime & ) ), m_workspaceMonitor->editPanel(), SLOT( setInpoint( const GenTime & ) ) );
		connect( m_timeline, SIGNAL( outpointPositionChanged( const GenTime & ) ), m_workspaceMonitor->editPanel(), SLOT( setOutpoint( const GenTime & ) ) );
	}


	connect( m_timeline, SIGNAL( seekPositionChanged( const GenTime & ) ), this, SLOT( activateWorkspaceMonitor() ) );

	connect( m_timeline, SIGNAL( rightButtonPressed() ), this, SLOT(slotDisplayTimeLineContextMenu()));

	connect( m_effectListDialog, SIGNAL( effectSelected(const EffectDesc& )), m_effectParamDialog, SLOT(slotSetEffectDescription(const EffectDesc& )));

	makeDockInvisible( mainDock );

	readDockConfig( config, "Default Layout" );

	slotSyncTimeLineWithDocument();

	m_timeline->trackView()->registerFunction("move", new TrackPanelClipMoveFunction(this, m_timeline, getDocument()));
	TrackPanelClipResizeFunction *resizeFunction = new TrackPanelClipResizeFunction(this, m_timeline, getDocument());
	m_timeline->trackView()->registerFunction("resize", resizeFunction);

	TrackPanelKeyFrameFunction *keyFrameFunction = new TrackPanelKeyFrameFunction(this, m_timeline, getDocument());
	m_timeline->trackView()->registerFunction("keyframe", keyFrameFunction);
	// connects for clip/workspace monitor activation (i.e. making sure they are visible when needed)

	connect(keyFrameFunction, SIGNAL( signalKeyFrameChanged(bool) ), getDocument(), SLOT( activateSceneListGeneration(bool) ) );

	connect(keyFrameFunction, SIGNAL( redrawTrack() ), m_timeline, SLOT( drawTrackViewBackBuffer() ) );
	
	connect(resizeFunction, SIGNAL( signalClipCropStartChanged( DocClipRef * ) ), this, SLOT( activateClipMonitor() ) );
	connect(resizeFunction, SIGNAL( signalClipCropEndChanged( DocClipRef * ) ), this, SLOT( activateClipMonitor() ) );
	connect(resizeFunction, SIGNAL(signalClipCropStartChanged(DocClipRef* )), m_clipMonitor, SLOT(slotClipCropStartChanged( DocClipRef * )));
	connect(resizeFunction, SIGNAL(signalClipCropEndChanged(DocClipRef* )), m_clipMonitor, SLOT(slotClipCropEndChanged( DocClipRef * )));

	m_timeline->trackView()->registerFunction("marker", new TrackPanelMarkerFunction(this, m_timeline, getDocument()));
	m_timeline->trackView()->registerFunction("spacer", new TrackPanelSpacerFunction(this, m_timeline, getDocument()));

	//m_timeline->trackView()->registerFunction("effects", new TrackPanelSpacerFunction(this, m_timeline, getDocument()));

	TrackPanelRazorFunction *razorFunction = new TrackPanelRazorFunction(this, m_timeline, getDocument());
	m_timeline->trackView()->registerFunction("razor", razorFunction);
	connect( razorFunction, SIGNAL( lookingAtClip( DocClipRef *, const GenTime & ) ), this, SLOT( slotLookAtClip( DocClipRef *, const GenTime & ) ) );
	//register roll function -reh
	m_timeline->trackView()->registerFunction("roll", new TrackPanelClipRollFunction(this, m_timeline, getDocument()));

	m_timeline->trackView()->registerFunction("selectnone", new TrackPanelSelectNoneFunction(this, m_timeline, getDocument()));
	m_timeline->trackView()->setDragFunction("move");

	m_timeline->setSnapToFrame(snapToFrameEnabled());
	m_timeline->setSnapToBorder(snapToBorderEnabled());
	m_timeline->setSnapToMarker(snapToMarkersEnabled());
	m_timeline->setEditMode("move");
}


void KdenliveApp::slotToggleClipMonitor()
{
	m_dockClipMonitor->changeHideShowState();
}

void KdenliveApp::slotToggleWorkspaceMonitor()
{
	m_dockWorkspaceMonitor->changeHideShowState();
}

void KdenliveApp::slotToggleEffectList()
{
	m_dockEffectList->changeHideShowState();
}

void KdenliveApp::slotToggleEffectStack()
{
	m_dockEffectStack->changeHideShowState();
}

void KdenliveApp::slotToggleProjectList()
{
	m_dockProjectList->changeHideShowState();
}

void KdenliveApp::openDocumentFile( const KURL& url )
{
	slotStatusMsg( i18n( "Opening file..." ) );

	m_projectFormatManager.openDocument( url, doc );
	slotStatusMsg( i18n( "Ready." ) );
}


KdenliveDoc *KdenliveApp::getDocument() const
{
	return doc;
}

void KdenliveApp::saveOptions()
{
	config->setGroup( "General Options" );
	config->writeEntry( "Geometry", size() );
	config->writeEntry( "Show Toolbar", viewToolBar->isChecked() );
	config->writeEntry( "Show Statusbar", viewStatusBar->isChecked() );
	config->writeEntry( "ToolBarPos", ( int ) toolBar( "mainToolBar" ) ->barPos() );
	config->writeEntry( "TimeScaleSlider", m_timeline->getTimeScaleSliderValue() );

	fileOpenRecent->saveEntries( config );
	config->writeEntry( "FileDialogPath", m_fileDialogPath.path() );

	m_renderManager->saveConfig( config );
	writeDockConfig( config, "Default Layout" );

	config->setGroup( "Debug Options" );
	config->writeEntry( "Ignore Rendering Messages", m_renderDebugPanel->ignoreMessages() );
}
QString KdenliveApp::getTimeScaleSliderText() const
{
	QString value = m_timeline->getTimeScaleSliderText();
	return value;
}

void KdenliveApp::readOptions()
{
	config->setGroup( "General Options" );

	// bar status settings
	bool bViewToolbar = config->readBoolEntry( "Show Toolbar", true );
	viewToolBar->setChecked( bViewToolbar );
	slotViewToolBar();

	bool bViewStatusbar = config->readBoolEntry( "Show Statusbar", true );
	viewStatusBar->setChecked( bViewStatusbar );
	slotViewStatusBar();

	// bar position settings
	KToolBar::BarPosition toolBarPos;
	toolBarPos = ( KToolBar::BarPosition ) config->readNumEntry( "ToolBarPos", KToolBar::Top );
	toolBar( "mainToolBar" ) ->setBarPos( toolBarPos );

	//timeline slider timescale setting
	int iTimeScaleSlider = (int) config->readNumEntry( "TimeScaleSlider", 6 );
	m_timeline->setSliderIndex( iTimeScaleSlider );

	// initialize the recent file list
	fileOpenRecent->loadEntries( config );
	// file dialog path
	m_fileDialogPath = KURL( config->readEntry( "FileDialogPath", "" ) );

	QSize size = config->readSizeEntry( "Geometry" );
	if ( !size.isEmpty() ) {
		resize( size );
	}

	config->setGroup( "Debug Options" );
	bool ignoreMessages = config->readBoolEntry( "Ignore Rendering Messages", true );
	m_renderDebugPanel->setIgnoreMessages( ignoreMessages );
}

void KdenliveApp::saveProperties( KConfig *_cfg )
{
	if ( doc->URL().fileName() != i18n( "Untitled" ) && !doc->isModified() ) {
		// saving to tempfile not necessary

	} else {
		KURL url = doc->URL();
		_cfg->writeEntry( "filename", url.url() );
		_cfg->writeEntry( "modified", doc->isModified() );
		QString tempname = kapp->tempSaveName( url.url() );
		QString tempurl = KURL::encode_string( tempname );
		KURL _url( tempurl );
		m_projectFormatManager.saveDocument( _url, doc );
	}
}


void KdenliveApp::readProperties( KConfig* _cfg )
{
	QString filename = _cfg->readEntry( "filename", "" );
	KURL url( filename );
	bool modified = _cfg->readBoolEntry( "modified", false );
	if ( modified ) {
		bool canRecover;
		QString tempname = kapp->checkRecoverFile( filename, canRecover );
		KURL _url( tempname );

		if ( canRecover ) {
			m_projectFormatManager.openDocument( _url, doc );
			doc->setModified( true );
			setCaption( _url.fileName(), true );
			QFile::remove( tempname );
		}
	} else {
		if ( !filename.isEmpty() ) {
			m_projectFormatManager.openDocument( url, doc );
			setCaption( url.fileName(), false );
		}
	}
}

bool KdenliveApp::queryClose()
{
	return saveModified();
}

bool KdenliveApp::saveModified()
{
	bool completed = true;

	if ( doc->isModified() ) {
		int want_save = KMessageBox::warningYesNoCancel( this,
		                i18n( "The current file has been modified.\n"
		                      "Do you want to save it?" ),
		                i18n( "Warning" ) );

		switch ( want_save ) {
			case KMessageBox::Yes:
			if ( doc->URL().fileName() == i18n( "Untitled" ) ) {
				slotFileSaveAs();
			} else {
				m_projectFormatManager.saveDocument( doc->URL(), doc );
			};
			if (m_clipMonitor)
				m_clipMonitor->slotClearClip();
			if (m_workspaceMonitor)
			m_workspaceMonitor->slotClearClip();
			doc->deleteContents();

			completed = true;
			break;

			case KMessageBox::No:
			if (m_clipMonitor)m_clipMonitor->slotClearClip();
			if (m_workspaceMonitor)m_workspaceMonitor->slotClearClip();
			doc->newDocument();
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

bool KdenliveApp::queryExit()
{
	saveOptions();
	return true;
}

/////////////////////////////////////////////////////////////////////
// SLOT IMPLEMENTATION
/////////////////////////////////////////////////////////////////////

void KdenliveApp::slotFileNew()
{
	slotStatusMsg( i18n( "Creating new document..." ) );

	if ( !saveModified() ) {
		// here saving wasn't successful

	} else {
		doc->newDocument();
		setCaption( doc->URL().fileName(), false );
	}

	slotStatusMsg( i18n( "Ready." ) );
}

void KdenliveApp::slotFileOpen()
{
	slotStatusMsg( i18n( "Opening file..." ) );

	if ( !saveModified() ) {
		// here saving wasn't successful

	} else {
		KURL url = KFileDialog::getOpenURL( m_fileDialogPath.path(),
							m_projectFormatManager.loadMimeTypes(),
							this,
							i18n( "Open File..." ));
		if ( !url.isEmpty() ) {
			if (!m_projectFormatManager.openDocument( url, doc ))
			{
				 KMessageBox::sorry(this, i18n("Could not read project file: %1").arg(url.prettyURL()));
				 return;
			}

			setCaption( url.fileName(), false );
			fileOpenRecent->addURL( url );
			m_fileDialogPath = url;
			m_fileDialogPath.setFileName( QString::null );
		}
	}
	slotStatusMsg( i18n( "Ready." ) );
}

void KdenliveApp::slotFileOpenRecent( const KURL& url )
{
	slotStatusMsg( i18n( "Opening file..." ) );

	if ( !saveModified() ) {
		// here saving wasn't successful
	} else {
		kdWarning() << "Opening url " << url.path() << endl;
		m_projectFormatManager.openDocument( url , doc);
		setCaption( url.fileName(), false );
	}

	slotStatusMsg( i18n( "Ready." ) );
}

void KdenliveApp::slotFileSave()
{
	slotStatusMsg( i18n( "Saving file..." ) );

	m_projectFormatManager.saveDocument( doc->URL(), doc );

	slotStatusMsg( i18n( "Ready." ) );
}

void KdenliveApp::slotFileSaveAs()
{
	slotStatusMsg( i18n( "Saving file with a new filename..." ) );

	KURL url = KFileDialog::getSaveURL(	m_fileDialogPath.path(),
								m_projectFormatManager.saveMimeTypes(),
	                                    			/* i18n( "*.kdenlive|Kdenlive Project Files (*.kdenlive)" ),*/
					    			this,
					     			i18n( "Save as..." ) );
	if ( !url.isEmpty() ) {
		if ( url.path().find( "." ) == -1 ) {
			url.setFileName( url.filename() + ".kdenlive" );
		}
		m_projectFormatManager.saveDocument( url, doc );
		fileOpenRecent->addURL( url );
		setCaption( url.fileName(), doc->isModified() );
		m_fileDialogPath = url;
		m_fileDialogPath.setFileName( QString::null );
	}

	slotStatusMsg( i18n( "Ready." ) );
}

void KdenliveApp::slotFileClose()
{
	slotStatusMsg( i18n( "Closing file..." ) );

	close();

	slotStatusMsg( i18n( "Ready." ) );
}

void KdenliveApp::slotFilePrint()
{
	slotStatusMsg( i18n( "Printing..." ) );

	QPrinter printer;
	if ( printer.setup( this ) ) {
		QPainter painter;

		painter.begin( &printer );

		// TODO - add Print code here.

		painter.end();
	}

	slotStatusMsg( i18n( "Ready." ) );
}

void KdenliveApp::slotFileQuit()
{
	slotStatusMsg( i18n( "Exiting..." ) );
	saveOptions();
	// close the first window, the list makes the next one the first again.
	// This ensures that queryClose() is called on each window to ask for closing
	KMainWindow* w;
	if ( memberList ) {
		for ( w = memberList->first(); w; w = memberList->next() ) {
			// only close the window if the closeEvent is accepted. If the user presses Cancel on the saveModified() dialog,
			// the window and the application stay open.
			kdDebug() << "Closing" << w << endl;
			if ( !w->close() )
				break;
		}
		kdDebug() << "Done" << endl;
	}
}

void KdenliveApp::slotEditCut()
{
	slotStatusMsg( i18n( "Cutting selection..." ) );

	slotStatusMsg( i18n( "Ready." ) );
}

void KdenliveApp::slotEditCopy()
{
	slotStatusMsg( i18n( "Copying selection to clipboard..." ) );

	slotStatusMsg( i18n( "Ready." ) );
}

void KdenliveApp::slotEditPaste()
{
	slotStatusMsg( i18n( "Inserting clipboard contents..." ) );

	slotStatusMsg( i18n( "Ready." ) );
}

void KdenliveApp::slotViewToolBar()
{
	slotStatusMsg( i18n( "Toggling toolbar..." ) );
	///////////////////////////////////////////////////////////////////
	// turn Toolbar on or off
	if ( !viewToolBar->isChecked() ) {
		toolBar( "mainToolBar" ) ->hide();
	} else {
		toolBar( "mainToolBar" ) ->show();
	}

	slotStatusMsg( i18n( "Ready." ) );
}

void KdenliveApp::slotViewStatusBar()
{
	slotStatusMsg( i18n( "Toggle the statusbar..." ) );
	///////////////////////////////////////////////////////////////////
	//turn Statusbar on or off
	if ( !viewStatusBar->isChecked() ) {
		statusBar() ->hide();
	} else {
		statusBar() ->show();
	}

	slotStatusMsg( i18n( "Ready." ) );
}


void KdenliveApp::slotStatusMsg( const QString &text )
{
	///////////////////////////////////////////////////////////////////
	// change status message permanently
	statusBar() ->clear();
	statusBar() ->changeItem( text, ID_STATUS_MSG );
}

/** Alerts the App to when the document has been modified. */
void KdenliveApp::documentModified( bool modified )
{
	if ( modified ) {
		fileSave->setEnabled( true );
	} else {
		fileSave->setEnabled( false );
	}
	setCaption( doc->URL().fileName(), modified );
}

void KdenliveApp::slotTimelineSnapToBorder()
{
	m_timeline->setSnapToBorder(snapToBorderEnabled());
}

void KdenliveApp::slotTimelineSnapToFrame()
{
	m_timeline->setSnapToFrame(snapToFrameEnabled());
}

void KdenliveApp::slotTimelineSnapToMarker()
{
	m_timeline->setSnapToMarker(snapToMarkersEnabled());
}

bool KdenliveApp::snapToFrameEnabled() const
{
	return timelineSnapToFrame->isChecked();
}

bool KdenliveApp::snapToBorderEnabled() const
{
	return timelineSnapToBorder->isChecked();
}

bool KdenliveApp::snapToMarkersEnabled() const
{
	return timelineSnapToMarker->isChecked();
}

/** Adds a command to the command history, execute it if execute is true. */
void KdenliveApp::addCommand( KCommand *command, bool execute )
{
	m_commandHistory->addCommand( command, execute );
}

/** Called when the move tool is selected */
void KdenliveApp::slotTimelineMoveTool()
{
	statusBar() ->changeItem( i18n( "Move/Resize tool" ), ID_EDITMODE_MSG );
	m_timeline->setEditMode("move");
}

/** Called when the razor tool action is selected */
void KdenliveApp::slotTimelineRazorTool()
{
	statusBar() ->changeItem( i18n( "Razor tool" ), ID_EDITMODE_MSG );
	m_timeline->setEditMode("razor");
}

/** Called when the spacer tool action is selected */
void KdenliveApp::slotTimelineSpacerTool()
{
	statusBar() ->changeItem( i18n( "Separate tool" ), ID_EDITMODE_MSG );
	m_timeline->setEditMode("spacer");
}

/** Called when the spacer tool action is selected */
void KdenliveApp::slotTimelineMarkerTool()
{
	statusBar() ->changeItem( i18n( "Marker tool" ), ID_EDITMODE_MSG );
	m_timeline->setEditMode("marker");
}

/** Called when the roll tool action is selected */
void KdenliveApp::slotTimelineRollTool()
{
	statusBar() ->changeItem( i18n( "Roll tool" ), ID_EDITMODE_MSG );
	m_timeline->setEditMode("roll");
}

/** Called when the user activates the "Export Timeline" action */
void KdenliveApp::slotRenderExportTimeline()
{
	slotStatusMsg( i18n( "Exporting Timeline..." ) );

	if ( getDocument() ->renderer() ->rendererOk() ) {
		ExportDialog exportDialog( getDocument() ->renderer() ->fileFormats(), this, "export dialog" );
		if ( QDialog::Accepted == exportDialog.exec() ) {
			if ( !exportDialog.url().isEmpty() ) {
				doc->renderDocument( exportDialog.url() );
			}
		}
	} else {
		KMessageBox::sorry( this, i18n( "The renderer is not available. Please check your settings." ), i18n( "Cannot Export Timeline" ) );
	}

	slotStatusMsg( i18n( "Ready." ) );
}


void KdenliveApp::slotOptionsPreferences()
{
	slotStatusMsg( i18n( "Editing Preferences" ) );

	KdenliveSetupDlg *dialog = new KdenliveSetupDlg( this, this, "setupdlg" );
	connect( dialog, SIGNAL(settingsChanged()), this, SLOT(updateConfiguration()) );
	dialog->exec();
	delete dialog;

	slotStatusMsg( i18n( "Ready." ) );
}


/** Updates widgets according to the new preferences. */
void KdenliveApp::updateConfiguration()
{
// redraw timeline in case size or colors changed.
slotSyncTimeLineWithDocument();
}


/** Updates the current time in the status bar. */
void KdenliveApp::slotUpdateCurrentTime( const GenTime &time )
{
#warning The following line is broken - since frames per second is rounded to the nearest int, krulerTimeModel
#warning would never map the correct value to text if the frames per second is wrong.
	statusBar() ->changeItem( i18n( "Current Time : " ) + KRulerTimeModel::mapValueToText( ( int ) floor( time.frames( doc->framesPerSecond() ) + 0.5 ), doc->framesPerSecond() ), ID_CURTIME_MSG );
}

/** Add clips to the project */
void KdenliveApp::slotProjectAddClips()
{
	slotStatusMsg( i18n( "Adding Clips" ) );

	// Make a reasonable filter for video / audio files.
	QString filter = "*.dv video/x-msvideo video/mpeg audio/x-mp3 audio/x-wav application/ogg";

	KURL::List urlList = KFileDialog::getOpenURLs( m_fileDialogPath.path(),
	                     filter,
	                     this,
	                     i18n( "Open File..." ) );
	

	KURL::List::Iterator it;
	KURL url;

	KMacroCommand *macroCommand = new KMacroCommand( i18n( "Add Clips" ) );
	for ( it = urlList.begin(); it != urlList.end(); it++ ) {
		url = ( *it );
		if ( !url.isEmpty() ) {
			Command::KAddClipCommand * command;
			command = new Command::KAddClipCommand( *doc, url, true );
			macroCommand->addCommand( command );
			m_fileDialogPath = url;
		}
	}
	addCommand( macroCommand, true );

	m_fileDialogPath.setFileName( QString::null );

	slotStatusMsg( i18n( "Ready." ) );
}


/**  Create a new color clip (color, text or image) */
void KdenliveApp::slotProjectAddColorClip()
{
	slotStatusMsg( i18n( "Adding Clips" ) );

	KDialogBase *dia = new KDialogBase(this,"create_clip",true,i18n("Create New Color Clip"),KDialogBase::Ok|KDialogBase::Cancel);
	createColorClip_UI *clipChoice = new createColorClip_UI(dia);
	clipChoice->edit_name->setText(i18n("Color Clip"));
	dia->setMainWidget(clipChoice);
	dia->adjustSize();
	if (dia->exec() == QDialog::Accepted){
		QString color = clipChoice->button_color->color().name();
		color = color.replace(0,1,"0x")+"ff";
		GenTime duration(clipChoice->text_duration->value());
		KCommand *command = new Command::KAddClipCommand( *doc, color, duration, clipChoice->edit_name->text(), clipChoice->edit_description->text(), true );
		addCommand( command, true );
		}
	delete dia;
	slotStatusMsg( i18n( "Ready." ) );
}

void KdenliveApp::slotProjectAddImageClip()
{
	slotStatusMsg( i18n( "Adding Clips" ) );

	KDialogBase *dia = new KDialogBase(this,"create_clip",true,i18n("Create New Image Clip"),KDialogBase::Ok|KDialogBase::Cancel);
	createImageClip_UI *clipChoice = new createImageClip_UI(dia);
	dia->setMainWidget(clipChoice);
	// Filter for the GDK pixbuf producer
	QString filter = "image/gif image/jpeg image/png image/x-bmp";
	clipChoice->url_image->setFilter(filter);
	dia->adjustSize();
	if (dia->exec() == QDialog::Accepted){
		QString url = clipChoice->url_image->url();
		QString extension = QString::null;
		int ttl = 0;
		GenTime duration(clipChoice->text_duration->value());
		KCommand *command = new Command::KAddClipCommand( *doc, KURL(url), extension, ttl, duration,clipChoice->edit_description->text(), true );
		addCommand( command, true );
		}
	delete dia;
	slotStatusMsg( i18n( "Ready." ) );
}

/* Create text clip */
void KdenliveApp::slotProjectAddTextClip()
{
// # To be written
}

void KdenliveApp::slotProjectEditClip()
{
	slotStatusMsg( i18n( "Editing Clips" ) );
	DocClipRef *refClip = m_projectList->currentSelection();
	if ( refClip ) {
		DocClipBase * clip = refClip->referencedClip();

		if (refClip->clipType() == DocClipBase::COLOR) {
			KDialogBase *dia = new KDialogBase(this,"edit_clip",true,i18n("Edit Color Clip"),KDialogBase::Ok|KDialogBase::Cancel);
			createColorClip_UI *clipChoice = new createColorClip_UI(dia);
			QString color = dynamic_cast<DocClipAVFile*>(clip)->color();
			color = color.replace(0,2,"#");
			color = color.left(7);
			clipChoice->button_color->setColor(color);
			clipChoice->edit_name->setText(clip->name());
			clipChoice->edit_description->setText(clip->description());
			clipChoice->text_duration->setValue((int) (refClip->duration().ms()/1000));
			dia->setMainWidget(clipChoice);
			dia->adjustSize();
			if (dia->exec() == QDialog::Accepted){
			QString color = clipChoice->button_color->color().name();
			color = color.replace(0,1,"0x")+"ff";
			GenTime duration(clipChoice->text_duration->value());
			KCommand *command = new Command::KEditClipCommand( *doc, refClip, color, duration, clipChoice->edit_name->text(), clipChoice->edit_description->text());
			//addCommand( command, true );

			//m_projectList->updateListItem();
			}
			delete dia;
		}

		else if (refClip->clipType() == DocClipBase::IMAGE) {
			KDialogBase *dia = new KDialogBase(this,"create_clip",true,i18n("Edit Image Clip"),KDialogBase::Ok|KDialogBase::Cancel);
			createImageClip_UI *clipChoice = new createImageClip_UI(dia);

			clipChoice->edit_description->setText(clip->description());
			clipChoice->text_duration->setValue((int) (refClip->duration().ms()/1000));
			clipChoice->url_image->setURL(refClip->fileURL().path());
			dia->setMainWidget(clipChoice);
			dia->adjustSize();
			if (dia->exec() == QDialog::Accepted){
			QString url = clipChoice->url_image->url();
			QString extension = QString::null;
			int ttl = 0;			
			GenTime duration(clipChoice->text_duration->value());
			KCommand *command = new Command::KEditClipCommand( *doc, refClip, KURL(url), extension, ttl, duration, clipChoice->edit_description->text());
			//addCommand( command, true );

			//m_projectList->updateListItem();
			}
			delete dia;
		}

		else {  // audio or video clip
		KURL url = KURLRequesterDlg::getURL(refClip->fileURL().path(),this,i18n("Enter New URL"));
		if (url.path()!= refClip->fileURL().path())
		{
		KCommand *command = new Command::KEditClipCommand( *doc, refClip, url);
		//m_projectList->updateListItem();
		}
		}
	}
	slotStatusMsg( i18n( "Ready." ) );
}



/** Remove clips from the project */
void KdenliveApp::slotProjectDeleteClips()
{
	slotStatusMsg( i18n( "Removing Clips" ) );

	DocClipRef *refClip = m_projectList->currentSelection();
	if ( refClip ) {
		DocClipBase * clip = refClip->referencedClip();
		if ( KMessageBox::warningContinueCancel( this,
		        i18n( "This will remove all clips on the timeline that are currently using this clip. Are you sure you want to do this?" ) ) == KMessageBox::Continue ) {

			// Create a macro command that will delete all clips from the timeline involving this
			// avfile. Then, delete it.
			KMacroCommand * macroCommand = new KMacroCommand( i18n( "Delete Clip" ) );

			DocClipRefList list = doc->referencedClips( m_projectList->currentSelection() ->referencedClip() );

			QPtrListIterator<DocClipRef> itt( list );

			while ( itt.current() ) {
				Command::KAddRefClipCommand * command = new Command::KAddRefClipCommand( doc->effectDescriptions(), doc->clipManager(), &( doc->projectClip() ), itt.current(), false );
				macroCommand->addCommand( command );
				++itt;
			}

			// NOTE - we clear the monitors of the clip here - this does _not_ go into the macro
			// command.
			m_monitorManager.clearClip( clip );

			DocumentBaseNode *node = doc->findClipNode( refClip->name() );

			macroCommand->addCommand( new Command::KAddClipCommand( *doc, node->name(), clip, node->parent(), false ) );

			addCommand( macroCommand, true );
		}
	}

	slotStatusMsg( i18n( "Ready." ) );
}

/** Cleans the project of unwanted clips */
void KdenliveApp::slotProjectClean()
{
	slotStatusMsg( i18n( "Cleaning Project" ) );

	if ( KMessageBox::warningContinueCancel( this,
	        i18n( "Clean Project removes files from the project that are unused.\
	              Are you sure you want to do this?" ) ) == KMessageBox::Continue ) {

		KCommand *command = Command::KAddClipCommand::clearProject(*doc);

		addCommand( command, true );
	}

	slotStatusMsg( i18n( "Ready." ) );
}

void KdenliveApp::slotProjectClipProperties()
{
	slotStatusMsg( i18n( "Viewing clip properties" ) );
	m_clipPropertyDialog->setClip( m_projectList->currentSelection() );
	makeDockVisible( clipWidget );
	slotStatusMsg( i18n( "Ready." ) );
}

void KdenliveApp::slotProjectClipProperties( DocClipRef *clip )
{
	slotStatusMsg( i18n( "Viewing clip properties" ) );
	m_clipPropertyDialog->setClip( m_projectList->currentSelection() );
	slotStatusMsg( i18n( "Ready." ) );
}

void KdenliveApp::slotSeekForwards()
{
	slotStatusMsg( i18n( "Seeking Forwards one frame" ) );
	slotStatusMsg( i18n( "Ready." ) );
}

void KdenliveApp::slotSeekBackwards()
{
	slotStatusMsg( i18n( "Seeking Backwards One Frame" ) );
	slotStatusMsg( i18n( "Ready." ) );
}

void KdenliveApp::slotTogglePlay()
{
	slotStatusMsg( i18n( "Starting/stopping playback" ) );
	if ( m_monitorManager.hasActiveMonitor() ) {
		m_monitorManager.activeMonitor() ->editPanel()->togglePlay();
	}
	slotStatusMsg( i18n( "Ready." ) );
}

void KdenliveApp::slotTogglePlaySelected()
{
	slotStatusMsg( i18n( "Starting/stopping playback of inpoint/outpoint section" ) );
	if ( m_monitorManager.hasActiveMonitor() ) {
		m_monitorManager.activeMonitor() ->editPanel()->togglePlaySelected();
	}
	slotStatusMsg( i18n( "Ready." ) );
}

void KdenliveApp::slotNextFrame()
{
	slotStatusMsg( i18n( "Moving forward one frame" ) );
	if ( m_monitorManager.hasActiveMonitor() ) {
		m_monitorManager.activeMonitor() ->editPanel()->seek( m_monitorManager.activeMonitor() ->screen()->seekPosition() + GenTime( 1, getDocument() ->framesPerSecond() ) );
	}
	slotStatusMsg( i18n( "Ready." ) );
}

void KdenliveApp::slotLastFrame()
{
	slotStatusMsg( i18n( "Moving backwards one frame" ) );
	if ( m_monitorManager.hasActiveMonitor() ) {
		m_monitorManager.activeMonitor() ->editPanel()->seek( m_monitorManager.activeMonitor() ->screen()->seekPosition() - GenTime( 1, getDocument() ->framesPerSecond() ) );
	}
	slotStatusMsg( i18n( "Ready." ) );
}

void KdenliveApp::slotSetInpoint()
{
	slotStatusMsg( i18n( "Setting Inpoint" ) );
	if ( m_monitorManager.hasActiveMonitor() ) {
		m_monitorManager.activeMonitor() ->editPanel()->setInpoint();
	}
	slotStatusMsg( i18n( "Ready." ) );
}

void KdenliveApp::slotSetOutpoint()
{
	slotStatusMsg( i18n( "Setting outpoint" ) );
	if ( m_monitorManager.hasActiveMonitor() ) {
		m_monitorManager.activeMonitor() ->editPanel()->setOutpoint();
	}
	slotStatusMsg( i18n( "Ready." ) );
}

void KdenliveApp::slotDeleteSelected()
{
	slotStatusMsg( i18n( "Deleting Selected Clips" ) );
	addCommand( Command::KAddRefClipCommand::deleteSelectedClips(getDocument()), true );
	slotStatusMsg( i18n( "Ready." ) );
}

/** Returns the render manager. */
KRenderManager * KdenliveApp::renderManager()
{
	return m_renderManager;
}

/** Sets the clip monitor source to be the given clip. */
void KdenliveApp::slotSetClipMonitorSource( DocClipRef *clip )
{
	//activateClipMonitor();
	m_clipMonitor->slotSetClip( clip );
}

void KdenliveApp::loadLayout1()
{
	setUpdatesEnabled( FALSE );
	readDockConfig( config, "Layout 1" );
	setUpdatesEnabled( TRUE );
}

void KdenliveApp::loadLayout2()
{
	setUpdatesEnabled( FALSE );
	readDockConfig( config, "Layout 2" );
	setUpdatesEnabled( TRUE );
}

void KdenliveApp::loadLayout3()
{
	setUpdatesEnabled( FALSE );
	readDockConfig( config, "Layout 3" );
	setUpdatesEnabled( TRUE );
}

void KdenliveApp::loadLayout4()
{
	setUpdatesEnabled( FALSE );
	readDockConfig( config, "Layout 4" );
	setUpdatesEnabled( TRUE );
}

void KdenliveApp::saveLayout1()
{
	writeDockConfig( config, "Layout 1" );
}

void KdenliveApp::saveLayout2()
{
	writeDockConfig( config, "Layout 2" );
}

void KdenliveApp::saveLayout3()
{
	writeDockConfig( config, "Layout 3" );
}

void KdenliveApp::saveLayout4()
{
	writeDockConfig( config, "Layout 4" );
}


void KdenliveApp::activateClipMonitor()
{
	m_dockClipMonitor->makeDockVisible();
	m_monitorManager.activateMonitor( m_clipMonitor );
}

void KdenliveApp::activateMonitor(KMonitor *monitor)
{
m_monitorManager.activateMonitor(monitor);
}

void KdenliveApp::activateWorkspaceMonitor()
{
	m_dockWorkspaceMonitor->makeDockVisible();
	m_monitorManager.activateMonitor( m_workspaceMonitor );
}

/** Selects a clip into the clip monitor and seeks to the given time. */
void KdenliveApp::slotLookAtClip( DocClipRef *clip, const GenTime &time )
{
	slotSetClipMonitorSource( clip );
	m_clipMonitor->editPanel()->seek( time );
}


void KdenliveApp::slotRenderError( const QString &name, const QString &message )
{
	KMessageBox::sorry( this, message, name );
}

void KdenliveApp::slotSetRenderProgress( const GenTime &time )
{
	m_statusBarProgress->setPercentageVisible( true );
	m_statusBarProgress->setTotalSteps( ( int ) m_timeline->projectLength().frames( getDocument() ->framesPerSecond() ) );
	m_statusBarProgress->setProgress( ( int ) time.frames( getDocument() ->framesPerSecond() ) );
}

void KdenliveApp::slotSetRenderFinished()
{
	std::cerr << "FINISHED RENDERING!" << std::endl;
	m_statusBarProgress->setPercentageVisible( false );
	m_statusBarProgress->setProgress( m_statusBarProgress->totalSteps() );
}

void KdenliveApp::slotConfKeys()
{
  KKeyDialog::configure( actionCollection(), this, true );
}

void KdenliveApp::slotConfToolbars()
{
	saveMainWindowSettings( KGlobal::config(), "General Options" );
	KEditToolbar *dlg = new KEditToolbar( actionCollection(), "kdenliveui.rc" );
	if ( dlg->exec() ) {
		createGUI( "kdenliveui.rc" );
		applyMainWindowSettings( KGlobal::config(), "General Options" );
	}
	delete dlg;
}

void KdenliveApp::slotConfigureProject()
{
	ConfigureProjectDialog configDialog( getDocument() ->renderer() ->fileFormats(), this, "configure project dialog" );
	if ( QDialog::Accepted == configDialog.exec() ) {}
}


void KdenliveApp::slot_insertClips( QDropEvent *event )
{
	// sanity check.
	if ( !ClipDrag::canDecode( event ) ) return ;

	DocClipRefList clips = ClipDrag::decode( getDocument()->effectDescriptions(), getDocument() ->clipManager(), event );

	clips.setAutoDelete( true );

	QPtrListIterator<DocClipRef> itt( clips );

	KMacroCommand *macroCommand = new KMacroCommand( i18n( "Add Clips" ) );

	while ( itt.current() ) {
		Command::KAddClipCommand * command = new Command::KAddClipCommand( *getDocument(), "TBD - give proper name", itt.current() ->referencedClip(), getDocument() ->clipHierarch(), true );
		macroCommand->addCommand( command );
		++itt;
	}

	addCommand( macroCommand, true );
}

void KdenliveApp::slotFitToWidth()
{
	m_timeline->fitToWidth();
}


void KdenliveApp::slotToggleSnapMarker()
{
	slotStatusMsg( i18n( "Toggling snap marker" ) );
	if ( m_monitorManager.hasActiveMonitor() ) {
		m_monitorManager.activeMonitor() ->slotToggleSnapMarker();
	}
	slotStatusMsg( i18n( "Ready." ) );
}

void KdenliveApp::slotClearAllSnapMarkers()
{
	slotStatusMsg( i18n( "Clearing snap markers" ) );

	KMacroCommand *macroCommand = new KMacroCommand( i18n( "Clear all snap markers" ) );

	DocClipProject &clip = getDocument()->projectClip();

	populateClearSnapMarkers(macroCommand, clip, false);
	populateClearSnapMarkers(macroCommand, clip, true);

	addCommand(macroCommand, true);

	slotStatusMsg( i18n( "Ready." ) );
}

void KdenliveApp::slotClearSnapMarkersFromSelected()
{
	slotStatusMsg( i18n( "Clearing snap markers" ) );

	KMacroCommand *macroCommand = new KMacroCommand( i18n( "Clear selected snap markers" ) );

	DocClipProject &clip = getDocument()->projectClip();
	populateClearSnapMarkers(macroCommand, clip, true);

	addCommand(macroCommand, true);

	slotStatusMsg( i18n( "Ready." ) );
}

void KdenliveApp::populateClearSnapMarkers(KMacroCommand *macroCommand, DocClipProject &clip, bool selectedClips)
{
	// Hmmm, I wonder if this should scan *into* project clips?For the moment I will leave it as scanning only those cliprefs in the outermost
	// proejct clip - jmw, 16/12/2003

	for(uint track = 0; track<clip.numTracks(); ++track)
	{
		QPtrListIterator<DocClipRef> itt(clip.track(track)->firstClip(selectedClips));

		while(itt.current()) {
			QValueVector<GenTime> times = itt.current()->snapMarkers();

			for(QValueVector<GenTime>::iterator timeItt = times.begin(); timeItt != times.end(); ++timeItt) {
				Command::KAddMarkerCommand *command = new Command::KAddMarkerCommand(*getDocument(), itt.current(), *timeItt, false);
				macroCommand->addCommand(command);
			}

			++itt;
		}
	}
}

void KdenliveApp::slotDisplayTimeLineContextMenu()
{
	QPopupMenu *menu = (QPopupMenu *)factory()->container("timeline_context", this);
	if(menu) {
		menu->popup(QCursor::pos());
	}
}


/** At least one track within the project have been added or removed.
*
* The timeline needs to be updated to show these changes. */
void KdenliveApp::slotSyncTimeLineWithDocument()
{
	unsigned int index = 0;

	QPtrListIterator<DocTrackBase> trackItt(getDocument()->trackList());

	// Store the state of each track to restore it when rebuilding the tracks
	QPtrListIterator<KTrackPanel> panelList(m_timeline->trackList());
	uint tracksCount = trackItt.count()*2;  // double because of keyframe tracks
	bool collapsedState[tracksCount];
	
	uint i = 0;

	while ( i<tracksCount)
	{
		if (panelList.current()) collapsedState[i] = panelList.current()->isTrackCollapsed();
		else collapsedState[i] = false;
		i++;
		++panelList;
	}

	m_timeline->clearTrackList();


	while(trackItt.current()) {
		disconnect( trackItt.current(), SIGNAL( clipLayoutChanged() ), m_timeline, SLOT( drawTrackViewBackBuffer() ) );
		disconnect( trackItt.current(), SIGNAL( clipSelectionChanged() ), m_timeline, SLOT( drawTrackViewBackBuffer() ) );
		connect( trackItt.current(), SIGNAL( clipLayoutChanged() ), m_timeline, SLOT( drawTrackViewBackBuffer() ) );
		connect( trackItt.current(), SIGNAL( clipSelectionChanged() ), m_timeline, SLOT( drawTrackViewBackBuffer() ) );

		if(trackItt.current()->clipType() == "Video") {
			m_timeline->insertTrack(index, new KMMTrackVideoPanel(this, m_timeline, getDocument(), (dynamic_cast<DocTrackVideo *>(trackItt.current())), collapsedState[index]));
			++index;

			m_timeline->insertTrack(index, new KMMTrackKeyFramePanel(m_timeline, getDocument(), (*trackItt), collapsedState[index], "alphablend", 0, "fade", KEYFRAMETRACK));
			++index;
		} else if(trackItt.current()->clipType() == "Sound") {
			m_timeline->insertTrack(index, new KMMTrackSoundPanel(this, m_timeline, getDocument(), (dynamic_cast<DocTrackSound *>(trackItt.current())), collapsedState[index]));
			++index;
			m_timeline->insertTrack(index, new KMMTrackKeyFramePanel(m_timeline, getDocument(), (*trackItt), collapsedState[index], "alphablend", 0, "fade", KEYFRAMETRACK));
			++index;
		} else {
			kdWarning() << "Sync failed" << endl;
		}

		++trackItt;
	}

	m_timeline->resizeTracks();
}

void KdenliveApp::slotRazorAllClips()
{
	addCommand(Command::DocumentMacroCommands::razorAllClipsAt(getDocument(), m_timeline->seekPosition()), true);
}

void KdenliveApp::slotRazorSelectedClips()
{
	addCommand(Command::DocumentMacroCommands::razorSelectedClipsAt(getDocument(), m_timeline->seekPosition()), true);
}

} // namespace Gui
