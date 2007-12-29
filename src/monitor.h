#ifndef MONITOR_H
#define MONITOR_H

#include <KListWidget>
#include "ui_monitor_ui.h"
#include "renderer.h"

class Monitor : public QWidget
{
  Q_OBJECT
  
  public:
    Monitor(QString name, QWidget *parent=0);
    Render *render;

  private:
    Ui::Monitor_UI ui;

  private slots:
    void slotPlay();
    void slotOpen();
    void slotRewind();

  public slots:
    void slotOpenFile(const QString &);
    void slotSetXml(const QDomElement &e);
};

#endif
