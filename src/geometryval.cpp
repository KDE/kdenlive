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
#include <QMouseEvent>
#include <KDebug>

Geometryval::Geometryval(QWidget* parent): QWidget(parent) {
    ui.setupUi(this);
    QVBoxLayout* vbox = new QVBoxLayout(this);
    ui.widget->setLayout(vbox);
    QGraphicsView *view = new QGraphicsView(this);
    view->setBackgroundBrush(QBrush(QColor(0, 0, 0)));
    vbox->addWidget(view);

    ui.frame->setTickPosition(QSlider::TicksBelow);

    scene = new GraphicsSceneRectMove(this);
    scene->setTool(TITLE_SELECT);
    view->setScene(scene);
    double aspect = 4.0 / 3.0; //change to project val
    QGraphicsRectItem *m_frameBorder = new QGraphicsRectItem(QRectF(0, 0, 100.0*aspect, 100));
    m_frameBorder->setZValue(-1100);
    //m_frameBorder->setBrush(QColor(255, 255, 0, 255));
    m_frameBorder->setPen(QPen(QBrush(QColor(255, 255, 255, 255)), 1.0, Qt::DashLine));
    scene->addItem(m_frameBorder);

    paramRect = new QGraphicsRectItem(QRectF(20.0*aspect, 20, 80*aspect, 80));

    paramRect->setZValue(0);
    //m_frameBorder1->setBrush(QColor(255, 0, 0, 0));
    paramRect->setPen(QPen(QBrush(QColor(255, 0, 0, 255)), 1.0));

    scene->addItem(paramRect);

    scene->setSceneRect(-100.0*aspect, -100, 300.0*aspect, 300);
}

void Geometryval::mouseMoveEvent(QMouseEvent *event) {
    //if (event->button())
    kDebug() << paramRect->rect();
}

QDomElement Geometryval::getParamDesc() {
    return param;
}

void Geometryval::setupParam(const QDomElement& par, const QString& paramName, int minFrame, int maxFrame) {
    param = par;
    ui.frame->setRange(minFrame, maxFrame);
}
