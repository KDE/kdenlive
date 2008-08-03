/***************************************************************************
                          geomeytrval.cpp  -  description
                             -------------------
    begin                : 03 Aug 2008
    copyright            : (C) 2008 by Marco Gittler
    email                : g.marco@freenet.de
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/


#include "geometryval.h"
#include <QGraphicsView>
#include <QVBoxLayout>
#include <QGraphicsScene>
#include "graphicsscenerectmove.h"
#include <QGraphicsRectItem>

Geometryval::Geometryval(QWidget* parent): QWidget(parent) {
    ui.setupUi(this);
    QVBoxLayout* vbox = new QVBoxLayout(this);
    ui.widget->setLayout(vbox);
    QGraphicsView *view = new QGraphicsView(this);
    //view->setBackgroundBrush(QBrush(QColor(0,0,0)));
    vbox->addWidget(view);

    ui.frame->setTickPosition(QSlider::TicksBelow);

    scene = new GraphicsSceneRectMove(this);
    scene->setTool(TITLE_SELECT);
    view->setScene(scene);

    QGraphicsRectItem *m_frameBorder = new QGraphicsRectItem(QRectF(-40, -30, 40, 30));
    m_frameBorder->setZValue(-1100);
    m_frameBorder->setBrush(QColor(255, 255, 255, 0));

    scene->addItem(m_frameBorder);

    QGraphicsRectItem *m_frameBorder1 = new QGraphicsRectItem(QRectF(-30, -20, 30, 20));

    m_frameBorder1->setZValue(0);
    m_frameBorder1->setBrush(QColor(255, 0, 0, 0));

    scene->addItem(m_frameBorder1);

    scene->setSceneRect(-80, -60, 80, 60);
}
QDomElement Geometryval::getParamDesc() {}

void Geometryval::setupParam(const QDomElement&, const QString& paramName, int, int) {}
