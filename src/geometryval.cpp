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

Geometryval::Geometryval(QWidget* parent): QWidget(parent) {
    ui.setupUi(this);
    QVBoxLayout* vbox = new QVBoxLayout(this);
    ui.widget->setLayout(vbox);
    QGraphicsView *view = new QGraphicsView(this);
    vbox->addWidget(view);
    //scene= new QGraphicsScene;

    scene = new GraphicsSceneRectMove(this);
    view->setScene(scene);
}
QDomElement Geometryval::getParamDesc() {}

void Geometryval::setupParam(const QDomElement&, const QString& paramName, int, int) {}
