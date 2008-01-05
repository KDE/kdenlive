#ifndef MAINWINDOW_H
#define MAINWINDOW_H
 
#include <QDockWidget>

#include <KXmlGuiWindow>
#include <KTextEdit>
#include <KListWidget>
#include <KTabWidget>
#include <KUndoStack>

#include "projectlist.h"
#include "monitor.h"
#include "monitormanager.h"
#include "kdenlivedoc.h"
#include "trackview.h"

class MainWindow : public KXmlGuiWindow
{
  Q_OBJECT
  
  public:
    MainWindow(QWidget *parent=0);
    void openFile(const QString &inputFileName);
  
  private:
    KTabWidget* m_timelineArea;
    void setupActions();
    QString fileName;
    KdenliveDoc *m_activeDocument;
    MonitorManager *m_monitorManager;

    QDockWidget *projectListDock;
    ProjectList *m_projectList;

    QDockWidget *effectListDock;
    KListWidget *effectList;

    QDockWidget *effectStackDock;
    KListWidget *effectStack;

    QDockWidget *transitionConfigDock;
    KListWidget *transitionConfig;

    QDockWidget *clipMonitorDock;
    Monitor *m_clipMonitor;

    QDockWidget *projectMonitorDock;
    Monitor *m_projectMonitor;

    KUndoStack *m_commandStack;
 
  private slots:
    void newFile();
    void activateDocument();
    void connectDocument(TrackView*, KdenliveDoc*);
    void openFile();
    void saveFile();
    void saveFileAs();
    void saveFileAs(const QString &outputFileName);
    void slotPreferences();
    void slotConnectMonitors();
    void slotRaiseMonitor(bool clipMonitor);
};
 
#endif
