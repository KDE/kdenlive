#ifndef MONITOR_H
#define MONITOR_H

#include <KIcon>

#include "ui_monitor_ui.h"
#include "renderer.h"
#include "monitormanager.h"
#include "smallruler.h"

class MonitorManager;

class Monitor : public QWidget
{
  Q_OBJECT
  
  public:
    Monitor(QString name, MonitorManager *manager, QWidget *parent=0);
    Render *render;
    virtual void resizeEvent ( QResizeEvent * event );
  protected:
    virtual void mousePressEvent ( QMouseEvent * event );
    virtual void wheelEvent ( QWheelEvent * event );

  private:
    Ui::Monitor_UI ui;
    MonitorManager *m_monitorManager;
    QString m_name;
    double m_scale;
    int m_length;
    int m_position;
    SmallRuler *m_ruler;
    KIcon m_playIcon;
    KIcon m_pauseIcon;

  private slots:
    void slotPlay();
    void slotOpen();
    void adjustRulerSize(int length);
    void seekCursor(int pos);
    void rendererStopped(int pos);
    void slotSeek(int pos);
    void slotRewindOneFrame();
    void slotForwardOneFrame();
    void slotForward();
    void slotRewind();

  public slots:
    void slotOpenFile(const QString &);
    void slotSetXml(const QDomElement &e);
    void initMonitor();
    void refreshMonitor(bool visible);
    void stop();
    void start();
};

#endif
