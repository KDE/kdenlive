

#include <QTextStream>
#include <QTimer>
#include <QAction>

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

#include <mlt++/Mlt.h>

#include "mainwindow.h"
#include "kdenlivesettings.h"
#include "ui_configmisc_ui.h"
 
MainWindow::MainWindow(QWidget *parent)
    : KXmlGuiWindow(parent),
      fileName(QString()), m_activeDocument(NULL)
{
  m_timelineArea = new KTabWidget(this);
  m_timelineArea->setHoverCloseButton(true);
  m_timelineArea->setTabReorderingEnabled(true);
  connect(m_timelineArea, SIGNAL(currentChanged (int)), this, SLOT(activateDocument()));
  setCentralWidget(m_timelineArea);

  m_monitorManager = new MonitorManager();
  m_commandStack = new KUndoStack(this);

  projectListDock = new QDockWidget(i18n("Project Tree"), this);
  projectListDock->setObjectName("project_tree");
  m_projectList = new ProjectList(this);
  projectListDock->setWidget(m_projectList);
  addDockWidget(Qt::TopDockWidgetArea, projectListDock);

  effectListDock = new QDockWidget(i18n("Effect List"), this);
  effectListDock->setObjectName("effect_list");
  effectList = new KListWidget(this);
  effectListDock->setWidget(effectList);
  addDockWidget(Qt::TopDockWidgetArea, effectListDock);
  
  effectStackDock = new QDockWidget(i18n("Effect Stack"), this);
  effectStackDock->setObjectName("effect_stack");
  effectStack = new KListWidget(this);
  effectStackDock->setWidget(effectStack);
  addDockWidget(Qt::TopDockWidgetArea, effectStackDock);
  
  transitionConfigDock = new QDockWidget(i18n("Transition"), this);
  transitionConfigDock->setObjectName("transition");
  transitionConfig = new KListWidget(this);
  transitionConfigDock->setWidget(transitionConfig);
  addDockWidget(Qt::TopDockWidgetArea, transitionConfigDock);

  Mlt::Factory::init(NULL);

  clipMonitorDock = new QDockWidget(i18n("Clip Monitor"), this);
  clipMonitorDock->setObjectName("clip_monitor");
  m_clipMonitor = new Monitor("clip", m_monitorManager, this);
  clipMonitorDock->setWidget(m_clipMonitor);
  addDockWidget(Qt::TopDockWidgetArea, clipMonitorDock);
  
  projectMonitorDock = new QDockWidget(i18n("Project Monitor"), this);
  projectMonitorDock->setObjectName("project_monitor");
  m_projectMonitor = new Monitor("project", m_monitorManager, this);
  projectMonitorDock->setWidget(m_projectMonitor);
  addDockWidget(Qt::TopDockWidgetArea, projectMonitorDock);

  setupActions();
  tabifyDockWidget (projectListDock, effectListDock);
  tabifyDockWidget (projectListDock, effectStackDock);
  tabifyDockWidget (projectListDock, transitionConfigDock);
  projectListDock->raise();

  tabifyDockWidget (clipMonitorDock, projectMonitorDock);

  connect(projectMonitorDock, SIGNAL(visibilityChanged (bool)), m_projectMonitor, SLOT(refreshMonitor(bool)));
  connect(clipMonitorDock, SIGNAL(visibilityChanged (bool)), m_clipMonitor, SLOT(refreshMonitor(bool)));
  connect(m_monitorManager, SIGNAL(connectMonitors ()), this, SLOT(slotConnectMonitors()));
  connect(m_monitorManager, SIGNAL(raiseClipMonitor (bool)), this, SLOT(slotRaiseMonitor(bool)));
  m_monitorManager->initMonitors(m_clipMonitor, m_projectMonitor);

  newFile();
}


void MainWindow::slotRaiseMonitor(bool clipMonitor)
{
  if (clipMonitor) clipMonitorDock->raise();
  else projectMonitorDock->raise();
}

void MainWindow::slotConnectMonitors()
{

  m_projectList->setRenderer(m_clipMonitor->render);

  connect(m_projectList, SIGNAL(clipSelected(const QDomElement &)), m_clipMonitor, SLOT(slotSetXml(const QDomElement &)));

  connect(m_projectList, SIGNAL(getFileProperties(const QDomElement &, int)), m_clipMonitor->render, SLOT(getFileProperties(const QDomElement &, int)));

  connect(m_clipMonitor->render, SIGNAL(replyGetImage(int, int, const QPixmap &, int, int)), m_projectList, SLOT(slotReplyGetImage(int, int, const QPixmap &, int, int)));

  connect(m_clipMonitor->render, SIGNAL(replyGetFileProperties(int, const QMap < QString, QString > &, const QMap < QString, QString > &)), m_projectList, SLOT(slotReplyGetFileProperties(int, const QMap < QString, QString > &, const QMap < QString, QString > &)));

}

void MainWindow::setupActions()
{
  KAction* clearAction = new KAction(this);
  clearAction->setText(i18n("Clear"));
  clearAction->setIcon(KIcon("document-new"));
  clearAction->setShortcut(Qt::CTRL + Qt::Key_W);
  actionCollection()->addAction("clear", clearAction);
  /*connect(clearAction, SIGNAL(triggered(bool)),
          textArea, SLOT(clear()));*/
 
  KStandardAction::quit(kapp, SLOT(quit()),
                        actionCollection());
 
  KStandardAction::open(this, SLOT(openFile()),
                        actionCollection());
 
  KStandardAction::save(this, SLOT(saveFile()),
                        actionCollection());
 
  KStandardAction::saveAs(this, SLOT(saveFileAs()),
                        actionCollection());
 
  KStandardAction::openNew(this, SLOT(newFile()),
                        actionCollection());

  /*KStandardAction::undo(this, SLOT(undo()),
                        actionCollection());

  KStandardAction::redo(this, SLOT(redo()),
                        actionCollection());*/

  KStandardAction::preferences(this, SLOT(slotPreferences()),
	    actionCollection());

  /*m_redo = m_commandStack->createRedoAction(actionCollection());
  m_undo = m_commandStack->createUndoAction(actionCollection());*/

  setupGUI();


}
 
void MainWindow::newFile()
{
  KdenliveDoc *doc = new KdenliveDoc(KUrl(), 25, 720, 576);
  TrackView *trackView = new TrackView(doc);
  m_timelineArea->addTab(trackView, "New Project");
  if (m_timelineArea->count() == 1)
    connectDocument(trackView, doc);
}

void MainWindow::activateDocument()
{
  TrackView *currentTab = (TrackView *) m_timelineArea->currentWidget();
  KdenliveDoc *currentDoc = currentTab->document();
  connectDocument(currentTab, currentDoc);
}
 
void MainWindow::saveFileAs(const QString &outputFileName)
{
  KSaveFile file(outputFileName);
  file.open();
  
  QByteArray outputByteArray;
  //outputByteArray.append(textArea->toPlainText());
  file.write(outputByteArray);
  file.finalize();
  file.close();
  
  fileName = outputFileName;
}

void MainWindow::saveFileAs()
{
  saveFileAs(KFileDialog::getSaveFileName());
}
 
void MainWindow::saveFile()
{
  if(!fileName.isEmpty())
  {
    saveFileAs(fileName);
  }
  else
  {
    saveFileAs();
  }
}
 
void MainWindow::openFile() //changed
{
  openFile(KFileDialog::getOpenFileName(KUrl(), "application/vnd.kde.kdenlive"));
}
 
void MainWindow::openFile(const QString &inputFileName) //new
{
  KdenliveDoc *doc = new KdenliveDoc(KUrl(inputFileName), 25, 720, 576);
  TrackView *trackView = new TrackView(doc);
  m_timelineArea->setCurrentIndex(m_timelineArea->addTab(trackView, QIcon(), doc->documentName()));
  connectDocument(trackView, doc);
  
}

void MainWindow::connectDocument(TrackView *trackView, KdenliveDoc *doc) //changed
{
  //m_projectMonitor->stop();
  if (m_activeDocument) {
    m_activeDocument->setProducers(m_projectList->producersList());
    m_activeDocument->setRenderer(NULL);
  }
  m_projectList->setDocument(doc);
  m_monitorManager->setTimecode(doc->timecode());
  doc->setRenderer(m_projectMonitor->render);
  m_commandStack = doc->commandStack();

  QAction *redo = m_commandStack->createRedoAction(actionCollection());
  QAction *undo = m_commandStack->createUndoAction(actionCollection());

  QWidget* w = factory()->container("mainToolBar", this);
  if(w) {
    if (actionCollection()->action("undo"))
      delete actionCollection()->action("undo");
    if(actionCollection()->action("redo"))
      delete actionCollection()->action("redo");

    actionCollection()->addAction("undo", undo);
    actionCollection()->addAction("redo", redo);
    w->addAction(undo);
    w->addAction(redo);
  }
  
  m_activeDocument = doc;
}


void MainWindow::slotPreferences()
{
  //An instance of your dialog could be already created and could be
  // cached, in which case you want to display the cached dialog
  // instead of creating another one
  if ( KConfigDialog::showDialog( "settings" ) )
    return;

  // KConfigDialog didn't find an instance of this dialog, so lets
  // create it :
  KConfigDialog* dialog = new KConfigDialog(this, "settings",
                                          KdenliveSettings::self());

  QWidget *page1 = new QWidget;
  Ui::ConfigMisc_UI* confWdg = new Ui::ConfigMisc_UI( );
  confWdg->setupUi(page1);

  dialog->addPage( page1, i18n("Misc"), "misc" );

  //User edited the configuration - update your local copies of the
  //configuration data
  connect( dialog, SIGNAL(settingsChanged()), this, SLOT(updateConfiguration()) );

  dialog->show();
}

#include "mainwindow.moc"
