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

// include files for QT
#include <qdir.h>
#include <qprinter.h>
#include <qpainter.h>

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

#define ID_STATUS_MSG 1
#define ID_EDITMODE_MSG 2
#define ID_CURTIME_MSG 2

KdenliveApp::KdenliveApp(QWidget* , const char* name):KDockMainWindow(0, name)
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
  filePrint->setEnabled(false);
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
  fileClose = KStdAction::close(this, SLOT(slotFileClose()), actionCollection());
  filePrint = KStdAction::print(this, SLOT(slotFilePrint()), actionCollection());
  fileQuit = KStdAction::quit(this, SLOT(slotFileQuit()), actionCollection());
  editCut = KStdAction::cut(this, SLOT(slotEditCut()), actionCollection());
  editCopy = KStdAction::copy(this, SLOT(slotEditCopy()), actionCollection());
  editPaste = KStdAction::paste(this, SLOT(slotEditPaste()), actionCollection());
  viewToolBar = KStdAction::showToolbar(this, SLOT(slotViewToolBar()), actionCollection());
  viewStatusBar = KStdAction::showStatusbar(this, SLOT(slotViewStatusBar()), actionCollection());
  optionsPreferences = KStdAction::preferences(this, SLOT(slotOptionsPreferences()), actionCollection());

  timelineMoveTool = new KRadioAction(i18n("Move/Resize Tool"), "moveresize.png", 0, this, SLOT(slotTimelineMoveTool()), actionCollection(),"timeline_move_tool");
  timelineRazorTool = new KRadioAction(i18n("Razor Tool"), "razor.png", 0, this, SLOT(slotTimelineRazorTool()), actionCollection(),"timeline_razor_tool");
  timelineSpacerTool = new KRadioAction(i18n("Spacing Tool"), "spacer.png", 0, this, SLOT(slotTimelineSpacerTool()), actionCollection(),"timeline_spacer_tool");
  timelineSnapToFrame = new KToggleAction(i18n("Snap To Frames"), "snaptoframe.png", 0, this, SLOT(slotTimelineSnapToFrame()), actionCollection(),"timeline_snap_frame");
  timelineSnapToBorder = new KToggleAction(i18n("Snap To Border"), "snaptoborder.png", 0, this, SLOT(slotTimelineSnapToBorder()), actionCollection(),"timeline_snap_border");

  projectAddClips = new KToggleAction(i18n("Add Clips"), "addclips.png", 0, this, SLOT(slotProjectAddClips()), actionCollection(), "project_add_clip");
  projectDeleteClips = new KToggleAction(i18n("Delete Clips"), "deleteclips.png", 0, this, SLOT(slotProjectDeleteClips()), actionCollection(), "project_delete_clip");
  projectClean = new KToggleAction(i18n("Clean Project"), "cleanproject.png", 0, this, SLOT(slotProjectClean()), actionCollection(), "project_clean");

  renderExportTimeline = new KAction(i18n("&Export Timeline"), 0, 0, this, SLOT(slotRenderExportTimeline()), actionCollection(), "render_export_timeline");

  actionSeekForwards = new KAction(i18n("Seek &Forwards"), KShortcut(), this, SLOT(slotSeekForwards()), actionCollection(), "seek_forwards");
  actionSeekBackwards = new KAction(i18n("Seek Backwards"), KShortcut(), this, SLOT(slotSeekBackwards()), actionCollection(), "seek_backwards");  
  actionTogglePlay = new KAction(i18n("Start/Stop"), KShortcut(), this, SLOT(slotTogglePlay()), actionCollection(), "toggle_play");  
  actionDeleteSelected = new KAction(i18n("Delete Selected Clips"), KShortcut(Qt::Key_Delete), this, SLOT(slotDeleteSelected()), actionCollection(), "delete_selected_clips");

  timelineMoveTool->setExclusiveGroup("timeline_tools");
  timelineRazorTool->setExclusiveGroup("timeline_tools");
  timelineSpacerTool->setExclusiveGroup("timeline_tools");  
      
  fileNew->setStatusText(i18n("Creates a new document"));
  fileOpen->setStatusText(i18n("Opens an existing document"));
  fileOpenRecent->setStatusText(i18n("Opens a recently used file"));
  fileSave->setStatusText(i18n("Saves the actual document"));
  fileSaveAs->setStatusText(i18n("Saves the actual document as..."));
  fileClose->setStatusText(i18n("Closes the actual document"));
  filePrint ->setStatusText(i18n("Prints out the actual document"));
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
  actionSeekForwards->setStatusText(i18n("Seek forward one frame"));
  actionSeekBackwards->setStatusText(i18n("Seek backwards one frame"));
  actionTogglePlay->setStatusText(i18n("Start or stop playback"));
  actionDeleteSelected->setStatusText(i18n("Delete currently selected clips"));  
  // use the absolute path to your kdenliveui.rc file for testing purpose in createGUI();
  createGUI();

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

  KDockWidget *tabGroupDock = createDockWidget(i18n("tab group"), QPixmap(), 0, i18n("tab group"));
  m_tabWidget = new KDockTabGroup(tabGroupDock);
  m_tabWidget->setTabPosition(QTabWidget::Bottom);  
  tabGroupDock->setWidget(m_tabWidget);
  tabGroupDock->setDockSite(KDockWidget::DockFullSite);    
  tabGroupDock->manualDock(mainDock, KDockWidget::DockTop);  

  widget = createDockWidget(i18n("project list"), QPixmap(), 0, i18n("project list"));
	m_projectList = new ProjectList(this, getDocument(), widget);
  widget->setWidget(m_projectList);
  m_tabWidget->addTab(widget, i18n("Project List"));      

  widget = createDockWidget(i18n("Debug"), QPixmap(), 0, i18n("Debug"));
  m_renderDebugPanel = new RenderDebugPanel(widget);
  widget->setWidget(m_renderDebugPanel);
  widget->setDockSite(KDockWidget::DockFullSite);    
  m_tabWidget->addTab(widget, i18n("Debug"));

  widget = createDockWidget(i18n("Export"), QPixmap(), 0, i18n("Export"));
  m_exportDialog = new ExportDialog(getDocument()->renderer()->fileFormats(), widget, "export");
  widget->setWidget(m_exportDialog);
  widget->setDockSite(KDockWidget::DockFullSite);
  m_tabWidget->addTab(widget, i18n("Export"));  

  KDockWidget *workspaceMonitor = createDockWidget(i18n("Workspace Monitor"), QPixmap(), 0, i18n("Workspace Monitor"));
	m_workspaceMonitor = new KMMMonitor(this, getDocument(), workspaceMonitor, i18n("Workspace Monitor"));
  workspaceMonitor->setWidget(m_workspaceMonitor);
  workspaceMonitor->setDockSite(KDockWidget::DockFullSite);    
  workspaceMonitor->manualDock(tabGroupDock, KDockWidget::DockRight);

  widget = createDockWidget(i18n("Clip Monitor"), QPixmap(), 0, i18n("Clip Monitor"));
	m_clipMonitor = new KMMMonitor(this, getDocument(), widget, i18n("Clip Monitor"));
  widget->setWidget(m_clipMonitor);
  widget->setDockSite(KDockWidget::DockFullSite);
  widget->manualDock(workspaceMonitor, KDockWidget::DockLeft);    

  setBackgroundMode(PaletteBase);

  connect(m_rulerPanel, SIGNAL(timeScaleChanged(int)), m_timeline, SLOT(setTimeScale(int)));

  connect(m_projectList, SIGNAL(dragDropOccured(QDropEvent *)), getDocument(), SLOT(slot_insertClips(QDropEvent *)));
  connect(m_timeline, SIGNAL(projectLengthChanged(int)), m_workspaceMonitor, SLOT(setClipLength(int)));

  connect(m_timeline, SIGNAL(seekPositionChanged(const GenTime &)), m_workspaceMonitor, SLOT(seek(const GenTime &)));
  connect(m_workspaceMonitor, SIGNAL(seekPositionChanged(const GenTime &)), m_timeline, SLOT(seek(const GenTime &)));
  connect(getDocument(), SIGNAL(sceneListChanged(const QDomDocument &)), m_workspaceMonitor, SLOT(setSceneList(const QDomDocument &)));

  connect(getDocument(), SIGNAL(avFileListUpdated()), m_projectList, SLOT(slot_UpdateList()));
  connect(getDocument(), SIGNAL(avFileChanged(AVFile *)), m_projectList, SLOT(slot_avFileChanged(AVFile *)));

  connect(m_workspaceMonitor, SIGNAL(seekPositionChanged(const GenTime &)), this, SLOT(slotUpdateCurrentTime(const GenTime &)));

  connect(m_renderManager, SIGNAL(recievedInfo(const QString &, const QString &)), m_renderDebugPanel, SLOT(slotPrintDebug(const QString &, const QString &)));    
  connect(m_renderManager, SIGNAL(recievedStdout(const QString &, const QString &)), m_renderDebugPanel, SLOT(slotPrintWarning(const QString &, const QString &)));
  connect(m_renderManager, SIGNAL(recievedStderr(const QString &, const QString &)), m_renderDebugPanel, SLOT(slotPrintError(const QString &, const QString &)));

  connect(m_projectList, SIGNAL(AVFileSelected(AVFile *)), this, SLOT(slotSetClipMonitorSource(AVFile *)));
  connect(getDocument(), SIGNAL(signalClipSelected(DocClipBase *)), this, SLOT(slotSetClipMonitorSource(DocClipBase *)));

  connect(m_timeline, SIGNAL(signalClipCropStartChanged(const GenTime &)), m_clipMonitor, SLOT(seek(const GenTime &)));
  connect(m_timeline, SIGNAL(signalClipCropEndChanged(const GenTime &)), m_clipMonitor, SLOT(seek(const GenTime &)));  

  makeDockInvisible(mainDock);

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
  	kdDebug() << "Opening url " << url.path() << endl;
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
  statusBar()->changeItem(i18n("Spacer tool"), ID_EDITMODE_MSG);
}

/** Called when the user activates the "Export Timeline" action */
void KdenliveApp::slotRenderExportTimeline()
{
  slotStatusMsg(i18n("Exporting Timeline..."));

  KURL url=KFileDialog::getSaveURL(QDir::currentDirPath(),
        i18n("*.dv|Raw DV Files"), this, i18n("Export Timeline To File..."));

	if(!url.isEmpty())
  {
    if(url.path().find(".") == -1) {
      url.setFileName(url.filename() + ".dv");
    }    
    doc->renderDocument(url);
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
	static QString str_inpoint="inpoint";
	static QString str_outpoint="outpoint";
	static QString str_file="file";


  kdDebug() << "Setting clip monitor source" << endl;
  
  if(!file->durationKnown()) {
    kdWarning() << "Cannot create scenelist for clip monitor - clip duration unknown" << endl;
    return;
  }
  
  QDomDocument scenelist;
  scenelist.appendChild(scenelist.createElement("scenelist"));
  
  // generate the next scene.
  QDomElement scene = scenelist.createElement("scene");
  scene.setAttribute("duration", QString::number(file->duration().seconds()));

  QDomElement sceneClip;
  sceneClip = scenelist.createElement("input");
  sceneClip.setAttribute(str_file, file->fileURL().path());
  sceneClip.setAttribute(str_inpoint, "0.0");
  sceneClip.setAttribute(str_outpoint, QString::number(file->duration().seconds()));
  scene.appendChild(sceneClip);
  scenelist.documentElement().appendChild(scene);

  m_clipMonitor->setSceneList(scenelist);
  m_clipMonitor->setClipLength(file->duration().frames(getDocument()->framesPerSecond()));
  m_clipMonitor->seek(GenTime(0.0));
}

/** Sets the clip monitor source to be the given clip. */
void KdenliveApp::slotSetClipMonitorSource(DocClipBase *clip)
{
	static QString str_inpoint="inpoint";
	static QString str_outpoint="outpoint";
	static QString str_file="file";

  kdDebug() << "Setting clip monitor source (from DocClipBase *)" << endl;

  if(!clip) {  
    kdError() << "Null clip passed, not setting monitor." << endl;
    return;
  }

  QDomDocument scenelist;
  scenelist.appendChild(scenelist.createElement("scenelist"));

  // generate the next scene.
  QDomElement scene = scenelist.createElement("scene");
  scene.setAttribute("duration", QString::number(clip->duration().seconds()));

  QDomElement sceneClip;
  sceneClip = scenelist.createElement("input");  
  sceneClip.setAttribute(str_file, clip->fileURL().path());
  sceneClip.setAttribute(str_inpoint, "0.0");
  sceneClip.setAttribute(str_outpoint, QString::number(clip->duration().seconds()));
  scene.appendChild(sceneClip);
  scenelist.documentElement().appendChild(scene);

  m_clipMonitor->setSceneList(scenelist);
  m_clipMonitor->setClipLength(clip->duration().frames(getDocument()->framesPerSecond()));
  m_clipMonitor->seek(GenTime(0.0));  
}
