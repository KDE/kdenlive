/***************************************************************************
 *   Copyright (C) 2007 by Jean-Baptiste Mardelle (jb@kdenlive.org)        *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA          *
 ***************************************************************************/


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
    bool m_isActive;

  private slots:
    void slotPlay();
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
