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

#define ID_STATUS_MSG 1
#define ID_EDITMODE_MSG 2
#define ID_CURTIME_MSG 2

KdenliveApp::KdenliveApp(QWidget* , const char* name):KMainWindow(0, name)
{
  config=kapp->config();

  // renderer options
  config->setGroup("Renderer Options");
  setRenderAppPath(config->readEntry("App Path", "/usr/local/bin/piave"));
  setRenderAppPort(config->readUnsignedNumEntry("App Port", 6100));
  m_renderer = new KRender(m_renderAppPath, m_renderAppPort);  

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
  if(m_renderer) delete m_renderer;
}

void KdenliveApp::initActions()
{
  fileNewWindow = new KAction(i18n("New &Window"), 0, 0, this, SLOT(slotFileNewWindow()), actionCollection(),"file_new_window");
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

  timelineMoveTool->setExclusiveGroup("timeline_tools");
  timelineRazorTool->setExclusiveGroup("timeline_tools");
  timelineSpacerTool->setExclusiveGroup("timeline_tools");  
      
  fileNewWindow->setStatusText(i18n("Opens a new application window"));
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
  setCentralWidget(view);	
  setCaption(doc->URL().fileName(),false);
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
  config->setGroup("Renderer Options");
  config->writeEntry("App Path" , renderAppPath().path());
  config->writeEntry("App Port" , renderAppPort());    
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

void KdenliveApp::slotFileNewWindow()
{
  slotStatusMsg(i18n("Opening a new application window..."));
	
  KdenliveApp *new_window= new KdenliveApp();
  new_window->show();

  slotStatusMsg(i18n("Ready."));
}

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
    KURL url=KFileDialog::getOpenURL(QString::null,
        i18n("*.kdenlive|Kdenlive Project Files (*.kdenlive)\n*.dv|Raw DV File (*.dv)\n*|All Files"), this, i18n("Open File..."));
    if(!url.isEmpty())
    {
      doc->openDocument(url);
      setCaption(url.fileName(), false);
      fileOpenRecent->addURL( url );
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

  KURL url=KFileDialog::getSaveURL(QDir::currentDirPath(),
        i18n("*.kdenlive|Kdenlive Project Files (*.kdenlive)"), this, i18n("Save as..."));
  if(!url.isEmpty())
  {
    if(url.path().find(".") == -1) {
      url.setFileName(url.filename() + ".kdenlive");
    }
    doc->saveDocument(url);
    fileOpenRecent->addURL(url);
    setCaption(url.fileName(),doc->isModified());
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
    view->print(&printer);
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
void KdenliveApp::slotUpdateCurrentTime(GenTime time)
{
  statusBar()->changeItem(i18n("Current Time : ") + KRulerTimeModel::mapValueToText((int)round(time.frames(doc->framesPerSecond())), doc->framesPerSecond()), ID_EDITMODE_MSG);  
}

/** Add clips to the project */
void KdenliveApp::slotProjectAddClips()
{
  slotStatusMsg(i18n("Adding Clips"));

	// determine file types supported by Arts
	QString filter = "*";

	KURL::List urlList=KFileDialog::getOpenURLs(	QString::null,
							filter,
							this,
							i18n("Open File..."));

	KURL::List::Iterator it;
	KURL url;

	for(it = urlList.begin(); it != urlList.end(); it++) {
		url =  (*it);
		if(!url.isEmpty()) {
		  	doc->insertAVFile(url);
		}
	}
    
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

/** Read property of KURL m_renderAppPath. */
const KURL& KdenliveApp::renderAppPath(){
	return m_renderAppPath;
}

/** Write property of KURL m_renderAppPath. */
void KdenliveApp::setRenderAppPath( const KURL& _newVal){
	m_renderAppPath = _newVal;
}

/** Read property of unsigned int m_renderAppPort. */
const unsigned int& KdenliveApp::renderAppPort(){
	return m_renderAppPort;
}
/** Write property of unsigned int m_renderAppPort. */
void KdenliveApp::setRenderAppPort( const unsigned int& _newVal){
	m_renderAppPort = _newVal;
}

/** Returns the application-wide renderer */
KRender * KdenliveApp::renderer()
{
  return m_renderer;
}
