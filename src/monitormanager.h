#ifndef MONITORMANAGER_H
#define MONITORMANAGER_H

#include "monitor.h"

class Monitor;

class MonitorManager : public QObject
{
  Q_OBJECT
  
  public:
    MonitorManager(QWidget *parent=0);

    void initMonitors(Monitor *clipMonitor, Monitor *projectMonitor);
    void activateMonitor(QString name);

  private:
    Monitor *m_clipMonitor;
    Monitor *m_projectMonitor;
    QString m_activeMonitor;

  private slots:
    void initProjectMonitor();
    void initClipMonitor();

  signals:
    void connectMonitors();
    void raiseClipMonitor(bool);

};

#endif
