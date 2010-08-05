/***************************************************************************
 *   Copyright (C) 2010 by Till Theato (root@ttill.de)                     *
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


#ifndef GEOMETRYWIDGET_H
#define GEOMETRYWIDGET_H

#include "ui_geometrywidget_ui.h"
#include <mlt++/Mlt.h>

#include <QWidget>

class QDomElement;
class QGraphicsRectItem;
class Monitor;
class MonitorScene;

class GeometryWidget : public QWidget
{
    Q_OBJECT
public:
    GeometryWidget(Monitor *monitor, int clipPos = 0, QWidget* parent = 0);
    virtual ~GeometryWidget();
    QString getValue() const;

public slots:
    void setupParam(const QDomElement elem, int minframe, int maxframe);

private:
    Ui::GeometryWidget_UI m_ui;
    Monitor *m_monitor;
    int m_clipPos;
    int m_inPoint;
    int m_outPoint;
    MonitorScene *m_scene;
    QGraphicsRectItem *m_rect;
    Mlt::Geometry *m_geometry;

private slots:
    void slotCheckPosition(int renderPos);
    void slotUpdateGeometry();
    void slotUpdateProperties();
    void slotSetX(int value);
    void slotSetY(int value);
    void slotSetWidth(int value);
    void slotSetHeight(int value);

signals:
    void parameterChanged();
};

#endif
