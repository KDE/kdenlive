#include <QObject>
#include <QTimer>

#include "monitormanager.h"

MonitorManager::MonitorManager(QWidget *parent)
    : QObject(parent)
{


}

void MonitorManager::initMonitors(Monitor *clipMonitor, Monitor *projectMonitor)
{
  m_clipMonitor = clipMonitor;
  m_projectMonitor = projectMonitor;
  QTimer::singleShot(750, this, SLOT(initClipMonitor()));
}

void MonitorManager::initClipMonitor()
{
  m_clipMonitor->initMonitor();
  QTimer::singleShot(1500, this, SLOT(initProjectMonitor()));
}

void MonitorManager::initProjectMonitor()
{
  m_clipMonitor->stop();
  m_projectMonitor->initMonitor();
  activateMonitor("project");
  emit connectMonitors();
}

void MonitorManager::activateMonitor(QString name)
{
  if (m_activeMonitor == name) return;
  if (name == "clip") {
    m_projectMonitor->stop();
    m_clipMonitor->start();
    emit raiseClipMonitor(true);
  }
  else {
    m_clipMonitor->stop();
    m_projectMonitor->start();
    m_projectMonitor->raise();
    emit raiseClipMonitor(false);
  }
  m_activeMonitor = name;
}

#include "monitormanager.moc"
