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
#include <qwhatsthis.h>

// include files for KDE
#include <kiconloader.h>
#include <kmessagebox.h>
#include <kfiledialog.h>
#include <kmenubar.h>
#include <kstatusbar.h>
#include <klocale.h>
#include <kconfig.h>
#include <kstdaction.h>
#include <kcommand.h>
#include <kdebug.h>
#include <kkeydialog.h>
#include <kedittoolbar.h>

// application specific includes
#include "kdenlive.h"
#include "kdenliveview.h"
#include "kdenlivedoc.h"
#include "kdenlivesetupdlg.h"
#include "kprogress.h"
#include "krulertimemodel.h"
#include "krendermanager.h"
#include "docclipbase.h"
#include "exportdialog.h"
#include "effectlistdialog.h"
#include "effectparamdialog.h"
#include "clippropertiesdialog.h"

#define ID_STATUS_MSG 1
#define ID_EDITMODE_MSG 2
#define ID_CURTIME_MSG 2

KdenliveApp::KdenliveApp(QWidget* , const char* name):
				KDockMainWindow(0, name),
				m_monitorManager(this)
{
  config=kapp->config();

  // renderer options
  m_renderManager = new KRenderManager();
  m_renderManager->readConfig(config);  

  // call inits to invoke all other construction parts
  initStatusBar();
  m_commandHistory = new KCommandHistory(actionCollection(), true);  
  initActions();
  initDocument();
  initView();

  readOptions();

  // disable actions at startup
  fileSave->setEnabled(false);
//  filePrint->setEnabled(false);
  editCut->setEnabled(false);
  editCopy->setEnabled(false);
  editPaste->setEnabled(false);

  fileSaveAs->setEnabled(true);

  timelineMoveTool->setChecked(true);
  timelineSnapToBorder->setChecked(true);
  timelineSnapToFrame->setChecked(true);
}

KdenliveApp::~KdenliveApp()
{
  if(m_renderManager) delete m_renderManager;
  if(m_commandHistory) delete m_commandHistory;
}

void KdenliveApp::initActions()
{
  fileNew = KStdAction::openNew(this, SLOT(slotFileNew()), actionCollection());
  fileOpen = KStdAction::open(this, SLOT(slotFileOpen()), actionCollection());
  fileOpenRecent = KStdAction::openRecent(this, SLOT(slotFileOpenRecent(const KURL&)), actionCollection());
  fileSave = KStdAction::save(this, SLOT(slotFileSave()), actionCollection());
  fileSaveAs = KStdAction::saveAs(this, SLOT(slotFileSaveAs()), actionCollection());
  //fileClose = KStdAction::close(this, SLOT(slotFileClose()), actionCollection());
//  filePrint = KStdAction::print(this, SLOT(slotFilePrint()), actionCollection());
  fileQuit = KStdAction::quit(this, SLOT(slotFileQuit()), actionCollection());
  editCut = KStdAction::cut(this, SLOT(slotEditCut()), actionCollection());
  editCopy = KStdAction::copy(this, SLOT(slotEditCopy()), actionCollection());
  editPaste = KStdAction::paste(this, SLOT(slotEditPaste()), actionCollection());
  viewToolBar = KStdAction::showToolbar(this, SLOT(slotViewToolBar()), actionCollection());
  viewStatusBar = KStdAction::showStatusbar(this, SLOT(slotViewStatusBar()), actionCollection());
  optionsPreferences = KStdAction::preferences(this, SLOT(slotOptionsPreferences()), actionCollection());
  keyBindings = KStdAction::keyBindings(this, SLOT(slotConfKeys()), actionCollection());
  configureToolbars = KStdAction::configureToolbars(this, SLOT(slotConfToolbars()), actionCollection());

  timelineMoveTool = new KRadioAction(i18n("Move/Resize Tool"), "moveresize.png", KShortcut(Qt::Key_Q), this, SLOT(slotTimelineMoveTool()), actionCollection(),"timeline_move_tool");
  timelineRazorTool = new KRadioAction(i18n("Razor Tool"), "razor.png", KShortcut(Qt::Key_W), this, SLOT(slotTimelineRazorTool()), actionCollection(),"timeline_razor_tool");
  timelineSpacerTool = new KRadioAction(i18n("Spacing Tool"), "spacer.png", KShortcut(Qt::Key_E), this, SLOT(slotTimelineSpacerTool()), actionCollection(),"timeline_spacer_tool");
  timelineSnapToFrame = new KToggleAction(i18n("Snap To Frames"), "snaptoframe.png", 0, this, SLOT(slotTimelineSnapToFrame()), actionCollection(),"timeline_snap_frame");
  timelineSnapToBorder = new KToggleAction(i18n("Snap To Border"), "snaptoborder.png", 0, this, SLOT(slotTimelineSnapToBorder()), actionCollection(),"timeline_snap_border");

  projectAddClips = new KToggleAction(i18n("Add Clips"), "addclips.png", 0, this, SLOT(slotProjectAddClips()), actionCollection(), "project_add_clip");
  projectDeleteClips = new KToggleAction(i18n("Delete Clips"), "deleteclips.png", 0, this, SLOT(slotProjectDeleteClips()), actionCollection(), "project_delete_clip");
  projectClean = new KToggleAction(i18n("Clean Project"), "cleanproject.png", 0, this, SLOT(slotProjectClean()), actionCollection(), "project_clean");
  projectClipProperties = new KAction(i18n("Clip properties"), "clipproperties.png", 0, this, SLOT(slotProjectClipProperties()), actionCollection(), "project_clip_properties");

  renderExportTimeline = new KAction(i18n("&Export Timeline"), "exportvideo.png", 0, this, SLOT(slotRenderExportTimeline()), actionCollection(), "render_export_timeline");

  actionSeekForwards = new KAction(i18n("Seek &Forwards"), KShortcut(), this, SLOT(slotSeekForwards()), actionCollection(), "seek_forwards");
  actionSeekBackwards = new KAction(i18n("Seek Backwards"), KShortcut(), this, SLOT(slotSeekBackwards()), actionCollection(), "seek_backwards");
  actionTogglePlay = new KAction(i18n("Start/Stop"), KShortcut(Qt::Key_Space), this, SLOT(slotTogglePlay()), actionCollection(), "toggle_play");
  actionNextFrame = new KAction(i18n("Forward one frame"), KShortcut(Qt::Key_Right), this, SLOT(slotNextFrame()), actionCollection(), "forward_frame");
  actionLastFrame = new KAction(i18n("Back one frame"), KShortcut(Qt::Key_Left), this, SLOT(slotLastFrame()), actionCollection(), "backward_frame");
  actionSetInpoint = new KAction(i18n("Set inpoint"), KShortcut(Qt::Key_I), this, SLOT(slotSetInpoint()), actionCollection(), "set_inpoint");
  actionSetOutpoint = new KAction(i18n("Set outpoint"), KShortcut(Qt::Key_O), this, SLOT(slotSetOutpoint()), actionCollection(), "set_outpoint");
  actionDeleteSelected = new KAction(i18n("Delete Selected Clips"), KShortcut(Qt::Key_Delete), this, SLOT(slotDeleteSelected()), actionCollection(), "delete_selected_clips");

  actionLoadLayout1 = new KAction(i18n("Load Layout &1"), "loadlayout1.png", KShortcut(Qt::Key_F9), this, SLOT(loadLayout1()), actionCollection(), "load_layout_1");
  actionLoadLayout2 = new KAction(i18n("Load Layout &2"), "loadlayout2.png", KShortcut(Qt::Key_F10), this, SLOT(loadLayout2()), actionCollection(), "load_layout_2");
  actionLoadLayout3 = new KAction(i18n("Load Layout &3"), "loadlayout3.png", KShortcut(Qt::Key_F11), this, SLOT(loadLayout3()), actionCollection(), "load_layout_3");
  actionLoadLayout4 = new KAction(i18n("Load Layout &4"), "loadlayout4.png", KShortcut(Qt::Key_F12), this, SLOT(loadLayout4()), actionCollection(), "load_layout_4");
  actionSaveLayout1 = new KAction(i18n("Save Layout &1"), KShortcut(Qt::Key_F9 | Qt::CTRL | Qt::SHIFT), this, SLOT(saveLayout1()), actionCollection(), "save_layout_1");
  actionSaveLayout2 = new KAction(i18n("Save Layout &2"), KShortcut(Qt::Key_F10 | Qt::CTRL | Qt::SHIFT), this, SLOT(saveLayout2()), actionCollection(), "save_layout_2");
  actionSaveLayout3 = new KAction(i18n("Save Layout &3"), KShortcut(Qt::Key_F11 | Qt::CTRL | Qt::SHIFT), this, SLOT(saveLayout3()), actionCollection(), "save_layout_3");
  actionSaveLayout4 = new KAction(i18n("Save Layout &4"), KShortcut(Qt::Key_F12 | Qt::CTRL | Qt::SHIFT), this, SLOT(saveLayout4()), actionCollection(), "save_layout_4");

  timelineMoveTool->setExclusiveGroup("timeline_tools");
  timelineRazorTool->setExclusiveGroup("timeline_tools");
  timelineSpacerTool->setExclusiveGroup("timeline_tools");

  fileNew->setStatusText(i18n("Creates a new document"));
  fileOpen->setStatusText(i18n("Opens an existing document"));
  fileOpenRecent->setStatusText(i18n("Opens a recently used file"));
  fileSave->setStatusText(i18n("Saves the actual document"));
  fileSaveAs->setStatusText(i18n("Saves the actual document as..."));
//  fileClose->setStatusText(i18n("Closes the actual document"));
//  filePrint ->setStatusText(i18n("Prints out the actual document"));
  fileQuit->setStatusText(i18n("Quits the application"));
  editCut->setStatusText(i18n("Cuts the selected section and puts it to the clipboard"));
  editCopy->setStatusText(i18n("Copies the selected section to the clipboard"));
  editPaste->setStatusText(i18n("Pastes the clipboard contents to actual position"));
  viewToolBar->setStatusText(i18n("Enables/disables the toolbar"));
  viewStatusBar->setStatusText(i18n("Enables/disables the statusbar"));
  timelineMoveTool->setStatusText(i18n("Moves and resizes the document"));
  timelineRazorTool->setStatusText(i18n("Chops clips into two pieces"));
  timelineSpacerTool->setStatusText(i18n("Shifts all clips to the right of mouse"));
  timelineSnapToFrame->setStatusText(i18n("Clips will align to the nearest frame"));
  timelineSnapToBorder->setStatusText(i18n("Clips will align with the borders of other clips"));
  projectAddClips->setStatusText(i18n("Add clips to the project"));
  projectDeleteClips->setStatusText(i18n("Remove clips from the project"));
  projectClean->setStatusText(i18n("Remove unused clips from the project"));
  projectClipProperties->setStatusText(i18n("View the properties of the selected clip"));
  actionSeekForwards->setStatusText(i18n("Seek forward one frame"));
  actionSeekBackwards->setStatusText(i18n("Seek backwards one frame"));
  actionTogglePlay->setStatusText(i18n("Start or stop playback"));
  actionNextFrame->setStatusText(i18n("Move the current position forwards by one frame"));
  actionLastFrame->setStatusText(i18n("Move the current position backwards by one frame"));
  actionSetInpoint->setStatusText(i18n("Set the inpoint to the current position"));
  actionSetOutpoint->setStatusText(i18n("Set the outpoint to the current position"));
  actionDeleteSelected->setStatusText(i18n("Delete currently selected clips"));
  // use the absolute path to your kdenliveui.rc file for testing purpose in createGUI();
  createGUI("kdenliveui.rc");

}


void KdenliveApp::initStatusBar()
{
  ///////////////////////////////////////////////////////////////////
  // STATUSBAR
  // TODO: add your own items you need for displaying current application status.
  statusBar()->insertItem(i18n("Ready."), ID_STATUS_MSG);

  m_statusBarProgress = new KProgress(statusBar());
  m_statusBarProgress->setTextEnabled(false);
  
  statusBar()->addWidget(m_statusBarProgress);

  statusBar()->insertItem(i18n("Move/Resize mode"), ID_EDITMODE_MSG, 0, true);
  statusBar()->insertItem(i18n("Current Time : ") + "00:00:00.00", ID_CURTIME_MSG, 0, true);  
}

void KdenliveApp::initDocument()
{
  doc = new KdenliveDoc(this, this);
  connect(doc, SIGNAL(modified(bool)), this, SLOT(documentModified(bool)));
  doc->newDocument();
}

void KdenliveApp::initView()
{
  ////////////////////////////////////////////////////////////////////
  // create the main widget here that is managed by KTMainWindow's view-region and
  // connect the widget to your document to display document contents.

  view = new KdenliveView(this);
  doc->addView(view);
  KDockWidget *mainDock = createDockWidget(i18n("Application"), QPixmap(), this, i18n("Application"));
  mainDock->setWidget(view);
  mainDock->setDockSite(KDockWidget::DockFullSite);
  setCentralWidget(mainDock);
  setMainDockWidget(mainDock);
  setCaption(doc->URL().fileName(),false);


  m_rulerPanel = new KMMRulerPanel(NULL, "Ruler Panel");

  KDockWidget *widget = createDockWidget(i18n("TimeLine"), QPixmap(), 0, i18n("TimeLine"));
  m_timeline = new KMMTimeLine(this, m_rulerPanel, NULL, getDocument(), widget);
  widget->setWidget(m_timeline);
  widget->setDockSite(KDockWidget::DockFullSite);  
  widget->setDockSite(KDockWidget::DockCorner);
  widget->manualDock(mainDock, KDockWidget::DockBottom);    

  KDockWidget *projectDock = createDockWidget("project list", QPixmap(), 0, i18n("project list"));
  m_projectList = new ProjectList(this, getDocument(), projectDock);
  QToolTip::add( projectDock, i18n( "This window show your video files usable in your project" ) );
  QWhatsThis::add( projectDock, i18n( "This window show your video files usable in your project. "
                                 "You can add or remove some files with the contextual menu. "
                                 "For add some sequences on your video project, use the drag and drop." ) );
  projectDock->setWidget(m_projectList);
  projectDock->setDockSite(KDockWidget::DockFullSite);
  projectDock->manualDock(mainDock, KDockWidget::DockLeft);

  widget = createDockWidget("Debug", QPixmap(), 0, i18n("Debug"));
  m_renderDebugPanel = new RenderDebugPanel(widget);
  QToolTip::add( widget, i18n( "This window show all debug messages between renderer and Kdenlive" ) );
  widget->setWidget(m_renderDebugPanel);
  widget->setDockSite(KDockWidget::DockFullSite);    
  widget->manualDock(projectDock, KDockWidget::DockCenter);
  
  widget = createDockWidget("Effect List", QPixmap(), 0, i18n("Effect List"));
  m_effectListDialog = new EffectListDialog(getDocument()->renderer()->effectList(), widget, "effect list");
  QToolTip::add( widget, i18n( "This window show all effects usable with the renderer" ) );
  widget->setWidget(m_effectListDialog);
  widget->setDockSite(KDockWidget::DockFullSite);
  widget->manualDock(projectDock, KDockWidget::DockCenter);

  widget = createDockWidget("Effect Setup", QPixmap(), 0, i18n("Effect Setup"));
  m_effectParamDialog = new EffectParamDialog(widget, "effect setup");
  QToolTip::add( widget, i18n( "This window show the effects configurations usable with the renderer" ) );
  widget->setWidget(m_effectParamDialog);
  widget->setDockSite(KDockWidget::DockFullSite);
  widget->manualDock(projectDock, KDockWidget::DockCenter);    

  m_dockWorkspaceMonitor = createDockWidget("Workspace Monitor", QPixmap(), 0, i18n("Workspace Monitor"));
  m_workspaceMonitor = m_monitorManager.createMonitor(getDocument(), m_dockWorkspaceMonitor, i18n("Workspace Monitor"));
  m_workspaceMonitor->setNoSeek(true);
  m_dockWorkspaceMonitor->setWidget(m_workspaceMonitor);
  m_dockWorkspaceMonitor->setDockSite(KDockWidget::DockFullSite);
  m_dockWorkspaceMonitor->manualDock(mainDock, KDockWidget::DockRight);

  m_dockClipMonitor = createDockWidget("Clip Monitor", QPixmap(), 0, i18n("Clip Monitor"));
  m_clipMonitor = m_monitorManager.createMonitor(getDocument(), m_dockClipMonitor, i18n("Clip Monitor"));
  m_dockClipMonitor->setWidget(m_clipMonitor);
  m_dockClipMonitor->setDockSite(KDockWidget::DockFullSite);
  m_dockClipMonitor->manualDock(m_dockWorkspaceMonitor, KDockWidget::DockCenter);

  setBackgroundMode(PaletteBase);

  connect(m_rulerPanel, SIGNAL(timeScaleChanged(int)), m_timeline, SLOT(setTimeScale(int)));

  connect(m_workspaceMonitor, SIGNAL(seekPositionChanged(const GenTime &)), m_timeline, SLOT(seek(const GenTime &)));
  connect(m_workspaceMonitor, SIGNAL(seekPositionChanged(const GenTime &)), this, SLOT(slotUpdateCurrentTime(const GenTime &)));

  connect(getDocument(), SIGNAL(signalClipSelected(DocClipBase *)), this, SLOT(slotSetClipMonitorSource(DocClipBase *)));
  connect(getDocument(), SIGNAL(avFileListUpdated()), m_projectList, SLOT(slot_UpdateList()));
  connect(getDocument(), SIGNAL(avFileChanged(AVFile *)), m_projectList, SLOT(slot_avFileChanged(AVFile *)));

  connect(getDocument(), SIGNAL(documentChanged(DocClipBase *)), 
		  m_workspaceMonitor, SLOT(slotSetClip(DocClipBase *)));

  connect(getDocument()->renderer(), SIGNAL(effectListChanged(const QPtrList<EffectDesc> &)), m_effectListDialog, SLOT(setEffectList(const QPtrList<EffectDesc> &)));
  
  connect(getDocument()->renderer(), SIGNAL(rendering(const GenTime &)), this, SLOT(slotSetRenderProgress(const GenTime &)));
  connect(getDocument()->renderer(), SIGNAL(renderFinished()), this, SLOT(slotSetRenderFinished()));

  connect(m_renderManager, SIGNAL(renderDebug(const QString &, const QString &)), m_renderDebugPanel, SLOT(slotPrintRenderDebug(const QString &, const QString &)));
  connect(m_renderManager, SIGNAL(renderWarning(const QString &, const QString &)), m_renderDebugPanel, SLOT(slotPrintRenderWarning(const QString &, const QString &)));
  connect(m_renderManager, SIGNAL(renderError(const QString &, const QString &)), m_renderDebugPanel, SLOT(slotPrintRenderError(const QString &, const QString &)));
  connect(m_renderManager, SIGNAL(recievedStdout(const QString &, const QString &)), m_renderDebugPanel, SLOT(slotPrintWarning(const QString &, const QString &)));
  connect(m_renderManager, SIGNAL(recievedStderr(const QString &, const QString &)), m_renderDebugPanel, SLOT(slotPrintError(const QString &, const QString &)));
  connect(m_renderManager, SIGNAL(error(const QString &, const QString &)), this, SLOT(slotRenderError(const QString &, const QString &)));

  connect(m_projectList, SIGNAL(AVFileSelected(AVFile *)), this, SLOT(activateClipMonitor()));
  connect(m_projectList, SIGNAL(AVFileSelected(AVFile *)), this, SLOT(slotSetClipMonitorSource(AVFile *)));
  connect(m_projectList, SIGNAL(dragDropOccured(QDropEvent *)), getDocument(), SLOT(slot_insertClips(QDropEvent *)));

  connect(m_timeline, SIGNAL(seekPositionChanged(const GenTime &)), m_workspaceMonitor, SLOT(seek(const GenTime &)));
  connect(m_timeline, SIGNAL(signalClipCropStartChanged(const GenTime &)), m_clipMonitor, SLOT(seek(const GenTime &)));
  connect(m_timeline, SIGNAL(signalClipCropEndChanged(const GenTime &)), m_clipMonitor, SLOT(seek(const GenTime &)));
  connect(m_timeline, SIGNAL(lookingAtClip(DocClipBase *, const GenTime &)), this, SLOT(slotLookAtClip(DocClipBase *, const GenTime &)));


  // connects for clip/workspace monitor activation (i.e. making sure they are visible when needed)  
  connect(m_projectList, SIGNAL(AVFileSelected(AVFile *)), this, SLOT(activateClipMonitor()));
  connect(m_timeline, SIGNAL(signalClipCropStartChanged(const GenTime &)), this, SLOT(activateClipMonitor()));
  connect(m_timeline, SIGNAL(signalClipCropEndChanged(const GenTime &)), this, SLOT(activateClipMonitor()));
  connect(m_timeline, SIGNAL(seekPositionChanged(const GenTime &)), this, SLOT(activateWorkspaceMonitor()));

  makeDockInvisible(mainDock);

  readDockConfig(config, "Default Layout");

  m_timeline->calculateProjectSize();  
}

void KdenliveApp::openDocumentFile(const KURL& url)
{
  slotStatusMsg(i18n("Opening file..."));

  doc->openDocument( url);
  fileOpenRecent->addURL( url );
  slotStatusMsg(i18n("Ready."));
}


KdenliveDoc *KdenliveApp::getDocument() const
{
  return doc;
}

void KdenliveApp::saveOptions()
{
  config->setGroup("General Options");
  config->writeEntry("Geometry", size());
  config->writeEntry("Show Toolbar", viewToolBar->isChecked());
  config->writeEntry("Show Statusbar",viewStatusBar->isChecked());
  config->writeEntry("ToolBarPos", (int) toolBar("mainToolBar")->barPos());

  fileOpenRecent->saveEntries(config,"Recent Files");
  config->writeEntry("FileDialogPath", m_fileDialogPath.path());

  m_renderManager->saveConfig(config);
  writeDockConfig(config, "Default Layout");
}


void KdenliveApp::readOptions()
{
  config->setGroup("General Options");

  // bar status settings
  bool bViewToolbar = config->readBoolEntry("Show Toolbar", true);
  viewToolBar->setChecked(bViewToolbar);
  slotViewToolBar();

  bool bViewStatusbar = config->readBoolEntry("Show Statusbar", true);
  viewStatusBar->setChecked(bViewStatusbar);
  slotViewStatusBar();
 
  // bar position settings
  KToolBar::BarPosition toolBarPos;
  toolBarPos=(KToolBar::BarPosition) config->readNumEntry("ToolBarPos", KToolBar::Top);
  toolBar("mainToolBar")->setBarPos(toolBarPos);

  // initialize the recent file list
  fileOpenRecent->loadEntries(config,"Recent Files");
  // file dialog path
  m_fileDialogPath = KURL(config->readEntry("FileDialogPath", ""));  

  QSize size=config->readSizeEntry("Geometry");
  if(!size.isEmpty())
  {
    resize(size);
  }
}

void KdenliveApp::saveProperties(KConfig *_cfg)
{
  if(doc->URL().fileName()!=i18n("Untitled") && !doc->isModified())
  {
    // saving to tempfile not necessary

  }
  else
  {
    KURL url=doc->URL();	
    _cfg->writeEntry("filename", url.url());
    _cfg->writeEntry("modified", doc->isModified());
    QString tempname = kapp->tempSaveName(url.url());
    QString tempurl= KURL::encode_string(tempname);
    KURL _url(tempurl);
    doc->saveDocument(_url);
  }
}


void KdenliveApp::readProperties(KConfig* _cfg)
{
  QString filename = _cfg->readEntry("filename", "");
  KURL url(filename);
  bool modified = _cfg->readBoolEntry("modified", false);
  if(modified)
  {
    bool canRecover;
    QString tempname = kapp->checkRecoverFile(filename, canRecover);
    KURL _url(tempname);
  	
    if(canRecover)
    {
      doc->openDocument(_url);
      doc->setModified(true);
      setCaption(_url.fileName(),true);
      QFile::remove(tempname);
    }
  }
  else
  {
    if(!filename.isEmpty())
    {
      doc->openDocument(url);
      setCaption(url.fileName(),false);
    }
  }
}		

bool KdenliveApp::queryClose()
{
  return doc->saveModified();
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
  slotStatusMsg(i18n("Creating new document..."));

  if(!doc->saveModified())
  {
     // here saving wasn't successful

  }
  else
  {
    doc->newDocument();
    setCaption(doc->URL().fileName(), false);
  }

  slotStatusMsg(i18n("Ready."));
}

void KdenliveApp::slotFileOpen()
{
  slotStatusMsg(i18n("Opening file..."));
	
  if(!doc->saveModified())
  {
     // here saving wasn't successful

  }
  else
  {	
    KURL url=KFileDialog::getOpenURL(m_fileDialogPath.path(),
        i18n("*.kdenlive|Kdenlive Project Files (*.kdenlive)\n*.dv|Raw DV File (*.dv)\n*|All Files"), this, i18n("Open File..."));
    if(!url.isEmpty())
    {
      doc->openDocument(url);
      setCaption(url.fileName(), false);
      fileOpenRecent->addURL( url );
      m_fileDialogPath = url;
      m_fileDialogPath.setFileName(QString::null);
    }
  }
  slotStatusMsg(i18n("Ready."));
}

void KdenliveApp::slotFileOpenRecent(const KURL& url)
{
  slotStatusMsg(i18n("Opening file..."));
	
  if(!doc->saveModified())
  {
     // here saving wasn't successful
  }
  else
  {
	kdWarning() << "Opening url " << url.path() << endl;
	doc->openDocument(url);
	setCaption(url.fileName(), false);
  }

  slotStatusMsg(i18n("Ready."));
}

void KdenliveApp::slotFileSave()
{
  slotStatusMsg(i18n("Saving file..."));

  doc->saveDocument(doc->URL());

  slotStatusMsg(i18n("Ready."));
}

void KdenliveApp::slotFileSaveAs()
{
  slotStatusMsg(i18n("Saving file with a new filename..."));

  KURL url=KFileDialog::getSaveURL(m_fileDialogPath.path(),
        i18n("*.kdenlive|Kdenlive Project Files (*.kdenlive)"), this, i18n("Save as..."));
  if(!url.isEmpty())
  {
    if(url.path().find(".") == -1) {
      url.setFileName(url.filename() + ".kdenlive");
    }
    doc->saveDocument(url);
    fileOpenRecent->addURL(url);
    setCaption(url.fileName(),doc->isModified());
    m_fileDialogPath = url;
    m_fileDialogPath.setFileName(QString::null);
  }  

  slotStatusMsg(i18n("Ready."));
}

void KdenliveApp::slotFileClose()
{
  slotStatusMsg(i18n("Closing file..."));
	
  close();

  slotStatusMsg(i18n("Ready."));
}

void KdenliveApp::slotFilePrint()
{
  slotStatusMsg(i18n("Printing..."));

  QPrinter printer;
  if (printer.setup(this))
  {
    QPainter painter;

    painter.begin(&printer);

    // TODO - add Print code here.

    painter.end();
  }

  slotStatusMsg(i18n("Ready."));
}

void KdenliveApp::slotFileQuit()
{
  slotStatusMsg(i18n("Exiting..."));
  saveOptions();
  // close the first window, the list makes the next one the first again.
  // This ensures that queryClose() is called on each window to ask for closing
  KMainWindow* w;
  if(memberList)
  {
    for(w=memberList->first(); w; w = memberList->next())
    {
      // only close the window if the closeEvent is accepted. If the user presses Cancel on the saveModified() dialog,
      // the window and the application stay open.
      kdDebug() << "Closing" << w << endl;
      if(!w->close())
	break;
    }
    kdDebug() << "Done" << endl;
  }
}

void KdenliveApp::slotEditCut()
{
  slotStatusMsg(i18n("Cutting selection..."));

  slotStatusMsg(i18n("Ready."));
}

void KdenliveApp::slotEditCopy()
{
  slotStatusMsg(i18n("Copying selection to clipboard..."));

  slotStatusMsg(i18n("Ready."));
}

void KdenliveApp::slotEditPaste()
{
  slotStatusMsg(i18n("Inserting clipboard contents..."));

  slotStatusMsg(i18n("Ready."));
}

void KdenliveApp::slotViewToolBar()
{
  slotStatusMsg(i18n("Toggling toolbar..."));
  ///////////////////////////////////////////////////////////////////
  // turn Toolbar on or off
  if(!viewToolBar->isChecked())
  {
    toolBar("mainToolBar")->hide();
  }
  else
  {
    toolBar("mainToolBar")->show();
  }		

  slotStatusMsg(i18n("Ready."));
}

void KdenliveApp::slotViewStatusBar()
{
  slotStatusMsg(i18n("Toggle the statusbar..."));
  ///////////////////////////////////////////////////////////////////
  //turn Statusbar on or off
  if(!viewStatusBar->isChecked())
  {
    statusBar()->hide();
  }
  else
  {
    statusBar()->show();
  }

  slotStatusMsg(i18n("Ready."));
}


void KdenliveApp::slotStatusMsg(const QString &text)
{
  ///////////////////////////////////////////////////////////////////
  // change status message permanently
  statusBar()->clear();
  statusBar()->changeItem(text, ID_STATUS_MSG);
}

/** Alerts the App to when the document has been modified. */
void KdenliveApp::documentModified(bool modified)
{
	if(modified) {
	  fileSave->setEnabled(true);
	} else {
	  fileSave->setEnabled(false);
	}
}

/** Called whenever snapToBorder is toggled. */
void KdenliveApp::slotTimelineSnapToBorder()
{
}

/** Called whenever snaptoframe action is toggled. */
void KdenliveApp::slotTimelineSnapToFrame(){
}

/** Returns true if snapToFrame is enabled, false otherwise */
bool KdenliveApp::snapToFrameEnabled()
{
	return timelineSnapToFrame->isChecked();	
}

/** Returns true if snapToBorder is checked, false otherwise */
bool KdenliveApp::snapToBorderEnabled()
{
	return timelineSnapToBorder->isChecked();
}

/** Adds a command to the command history, execute it if execute is true. */
void KdenliveApp::addCommand(KCommand *command, bool execute)
{
	m_commandHistory->addCommand(command, execute);
}

/** Called when the move tool is selected */
void KdenliveApp::slotTimelineMoveTool()
{
  statusBar()->changeItem(i18n("Move/Resize tool"), ID_EDITMODE_MSG);
}

/** Called when the razor tool action is selected */
void KdenliveApp::slotTimelineRazorTool()
{
  statusBar()->changeItem(i18n("Razor tool"), ID_EDITMODE_MSG);
}

/** Called when the spacer tool action is selected */
void KdenliveApp::slotTimelineSpacerTool()
{
  statusBar()->changeItem(i18n("Separate tool"), ID_EDITMODE_MSG);
}

/** Called when the user activates the "Export Timeline" action */
void KdenliveApp::slotRenderExportTimeline()
{
  slotStatusMsg(i18n("Exporting Timeline..."));

  if(getDocument()->renderer()->rendererOk()) {
    ExportDialog exportDialog(getDocument()->renderer()->fileFormats(), this, "export dialog");
    if(QDialog::Accepted == exportDialog.exec()) {
      if(!exportDialog.url().isEmpty()) {
        doc->renderDocument(exportDialog.url());    
      }
    }
  } else {
    KMessageBox::sorry(this, i18n("The renderer is not available. Please check your settings."), i18n("Cannot Export Timeline"));
  }

  slotStatusMsg(i18n("Ready."));
}

void KdenliveApp::slotOptionsPreferences()
{
  slotStatusMsg(i18n("Editing Preferences"));

  KdenliveSetupDlg dialog(this, this, "setupdlg");
  dialog.exec();

  slotStatusMsg(i18n("Ready."));    
}

/** Returns the editing mode that the timeline should operate with */
KdenliveApp::TimelineEditMode KdenliveApp::timelineEditMode()
{
	if(timelineMoveTool->isChecked()) return Move;
	if(timelineRazorTool->isChecked()) return Razor;
	if(timelineSpacerTool->isChecked()) return Spacer;

	// fallback in case something is wrong.
	kdWarning() << "No timeline tool enabled, returning default" << endl;
	return Move;
}

/** Updates the current time in the status bar. */
void KdenliveApp::slotUpdateCurrentTime(const GenTime &time)
{
  statusBar()->changeItem(i18n("Current Time : ") + KRulerTimeModel::mapValueToText((int)floor(time.frames(doc->framesPerSecond()) + 0.5), doc->framesPerSecond()), ID_EDITMODE_MSG);  
}

/** Add clips to the project */
void KdenliveApp::slotProjectAddClips()
{
  slotStatusMsg(i18n("Adding Clips"));

	// determine file types supported by Arts
	QString filter = "*";

	KURL::List urlList=KFileDialog::getOpenURLs(	m_fileDialogPath.path(),
							filter,
							this,
							i18n("Open File..."));

	KURL::List::Iterator it;
	KURL url;

	for(it = urlList.begin(); it != urlList.end(); it++) {
		url =  (*it);
		if(!url.isEmpty()) {
      doc->insertAVFile(url);
      m_fileDialogPath = url;
		}
	}

  m_fileDialogPath.setFileName(QString::null);
  
  slotStatusMsg(i18n("Ready."));     
}

/** Remove clips from the project */
void KdenliveApp::slotProjectDeleteClips()
{
  slotStatusMsg(i18n("Removing Clips"));
  slotStatusMsg(i18n("Ready."));     
}

/** Cleans the project of unwanted clips */
void KdenliveApp::slotProjectClean()
{
  slotStatusMsg(i18n("Cleaning Project"));

  if(KMessageBox::warningContinueCancel(this,
			 i18n("Clean Project removes files from the project that are unused.\
			 Are you sure you want to do this?")) == KMessageBox::Continue) {
		doc->cleanAVFileList();
	}
  
  slotStatusMsg(i18n("Ready."));
}

void KdenliveApp::slotProjectClipProperties()
{
  slotStatusMsg(i18n("Viewing clip properties"));

  ClipPropertiesDialog dialog;
  dialog.setClip(m_projectList->currentSelection());
  dialog.exec();
  
  slotStatusMsg(i18n("Ready."));  
}

void KdenliveApp::slotSeekForwards()            
{
  slotStatusMsg(i18n("Seeking Forwards one frame"));
  slotStatusMsg(i18n("Ready."));  
}

void KdenliveApp::slotSeekBackwards()
{
  slotStatusMsg(i18n("Seeking Backwards One Frame"));
  slotStatusMsg(i18n("Ready."));
}

void KdenliveApp::slotTogglePlay()
{
	slotStatusMsg(i18n("Starting/stopping playback"));
	if(m_monitorManager.hasActiveMonitor()) {
 		m_monitorManager.activeMonitor()->togglePlay();
	}
	slotStatusMsg(i18n("Ready."));
}

void KdenliveApp::slotNextFrame()
{
	slotStatusMsg(i18n("Moving forward one frame"));
	if(m_monitorManager.hasActiveMonitor()) {
		m_monitorManager.activeMonitor()->seek(m_monitorManager.activeMonitor()->seekPosition() + GenTime(1, getDocument()->framesPerSecond()));
	}
	slotStatusMsg(i18n("Ready."));
}

void KdenliveApp::slotLastFrame()
{
	slotStatusMsg(i18n("Moving backwards one frame"));
	if(m_monitorManager.hasActiveMonitor()) {
		m_monitorManager.activeMonitor()->seek(m_monitorManager.activeMonitor()->seekPosition() - GenTime(1, getDocument()->framesPerSecond()));
	}
	slotStatusMsg(i18n("Ready."));
}

void KdenliveApp::slotSetInpoint()
{
	slotStatusMsg(i18n("Setting Inpoint"));
	if(m_monitorManager.hasActiveMonitor()) {
		m_monitorManager.activeMonitor()->slotSetInpoint();
	}
	slotStatusMsg(i18n("Ready."));
}

void KdenliveApp::slotSetOutpoint()
{
	slotStatusMsg(i18n("Setting outpoint"));
	if(m_monitorManager.hasActiveMonitor()) {
		m_monitorManager.activeMonitor()->slotSetOutpoint();
	}
	slotStatusMsg(i18n("Ready."));
}

void KdenliveApp::slotDeleteSelected()
{
  slotStatusMsg(i18n("Deleting Selected Clips"));
  addCommand(m_timeline->createAddClipsCommand(false), true);
//  m_workspaceMonitor->swapScreens(m_clipMonitor);
  slotStatusMsg(i18n("Ready."));
}

/** Returns the render manager. */
KRenderManager * KdenliveApp::renderManager()
{
  return m_renderManager;
}

/** Set the source of the clip monitor to the spectified AVFile. */
void KdenliveApp::slotSetClipMonitorSource(AVFile *file)
{
	m_clipMonitor->slotSetClip(file);
	activateClipMonitor();
}

/** Sets the clip monitor source to be the given clip. */
void KdenliveApp::slotSetClipMonitorSource(DocClipBase *clip)
{
	m_clipMonitor->slotSetClip(clip);
	activateClipMonitor();
}

void KdenliveApp::loadLayout1()
{
  setUpdatesEnabled(FALSE);
  readDockConfig(config, "Layout 1");
  setUpdatesEnabled(TRUE);  
}

void KdenliveApp::loadLayout2()
{
  setUpdatesEnabled(FALSE);
  readDockConfig(config, "Layout 2");
  setUpdatesEnabled(TRUE);
}

void KdenliveApp::loadLayout3()
{
  setUpdatesEnabled(FALSE);
  readDockConfig(config, "Layout 3");
  setUpdatesEnabled(TRUE);
}

void KdenliveApp::loadLayout4()
{
  setUpdatesEnabled(FALSE);
  readDockConfig(config, "Layout 4");
  setUpdatesEnabled(TRUE);
}

void KdenliveApp::saveLayout1()
{
  writeDockConfig(config, "Layout 1");
}

void KdenliveApp::saveLayout2()
{
  writeDockConfig(config, "Layout 2");
}

void KdenliveApp::saveLayout3()
{
  writeDockConfig(config, "Layout 3");
}

void KdenliveApp::saveLayout4()
{
  writeDockConfig(config, "Layout 4");
}


void KdenliveApp::activateClipMonitor()
{
  m_dockClipMonitor->makeDockVisible();
  m_monitorManager.activateMonitor(m_clipMonitor);
}

void KdenliveApp::activateWorkspaceMonitor()
{
  m_dockWorkspaceMonitor->makeDockVisible();
  m_monitorManager.activateMonitor(m_workspaceMonitor);
}

/** Selects a clip into the clip monitor and seeks to the given time. */
void KdenliveApp::slotLookAtClip(DocClipBase *clip, const GenTime &time)
{
  std::cerr << "Looking at clip " << clip << " with time " << time.seconds() << std::endl;
  slotSetClipMonitorSource(clip);
  m_clipMonitor->seek(time);
}


void KdenliveApp::slotRenderError(const QString &name, const QString &message)
{
  KMessageBox::sorry(this, message, name);
}

void KdenliveApp::slotSetRenderProgress(const GenTime &time)
{
	m_statusBarProgress->setPercentageVisible(true);
	m_statusBarProgress->setTotalSteps(m_timeline->projectLength().frames(getDocument()->framesPerSecond()));
	m_statusBarProgress->setProgress(time.frames(getDocument()->framesPerSecond()));
}

void KdenliveApp::slotSetRenderFinished()
{
	std::cerr << "FINISHED RENDERING!" << std::endl;
	m_statusBarProgress->setPercentageVisible(false);
	m_statusBarProgress->setProgress(m_statusBarProgress->totalSteps());
}

void KdenliveApp::slotConfKeys()
{
	    KKeyDialog::configureKeys(actionCollection(), xmlFile(), true, this);
}

void KdenliveApp::slotConfToolbars()
{
	saveMainWindowSettings(KGlobal::config(), "General Options");
	KEditToolbar *dlg = new KEditToolbar(actionCollection(), "kdenliveui.rc");
	if (dlg->exec()) {
		createGUI("kdenliveui.rc");
		applyMainWindowSettings(KGlobal::config(), "General Options");
	}
	delete dlg;
}

