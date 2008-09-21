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



#include <QGraphicsView>
#include <QVBoxLayout>
#include <QGraphicsScene>
#include "graphicsscenerectmove.h"
#include <QGraphicsRectItem>
#include <QMouseEvent>
#include <KDebug>

#include "geometryval.h"

Geometryval::Geometryval(const MltVideoProfile profile, QWidget* parent): QWidget(parent), m_profile(profile) {
    ui.setupUi(this);
    QVBoxLayout* vbox = new QVBoxLayout(ui.widget);
    QGraphicsView *view = new QGraphicsView(this);
    view->setBackgroundBrush(QBrush(Qt::black));
    vbox->addWidget(view);
    vbox->setContentsMargins(0, 0, 0, 0);

    QVBoxLayout* vbox2 = new QVBoxLayout(ui.keyframeWidget);
    m_helper = new KeyframeHelper(this);
    vbox2->addWidget(m_helper);
    vbox2->setContentsMargins(0, 0, 0, 0);

    scene = new GraphicsSceneRectMove(this);
    scene->setTool(TITLE_SELECT);
    view->setScene(scene);
    double aspect = (double) profile.sample_aspect_num / profile.sample_aspect_den * profile.width / profile.height;
    QGraphicsRectItem *m_frameBorder = new QGraphicsRectItem(QRectF(0, 0, profile.width, profile.height));
    m_frameBorder->setZValue(-1100);
    m_frameBorder->setBrush(QColor(255, 255, 0, 30));
    m_frameBorder->setPen(QPen(QBrush(QColor(255, 255, 255, 255)), 1.0, Qt::DashLine));
    scene->addItem(m_frameBorder);

    //scene->setSceneRect(-100.0*aspect, -100, 300.0*aspect, 300);
    //view->fitInView(m_frameBorder, Qt::KeepAspectRatio);
    const double sc = 100.0 / profile.height;
    view->scale(sc, sc);
    view->centerOn(m_frameBorder);
    connect(scene , SIGNAL(itemMoved()) , this , SLOT(moveEvent()));
    connect(ui.buttonNext , SIGNAL(clicked()) , this , SLOT(slotNextFrame()));
    connect(ui.buttonPrevious , SIGNAL(clicked()) , this , SLOT(slotPreviousFrame()));
}

void Geometryval::slotNextFrame() {
    Mlt::GeometryItem item;
    m_geom->next_key(&item, ui.keyframeSlider->value() + 1);
    int pos = item.frame();
    ui.keyframeSlider->setValue(pos);
}

void Geometryval::slotPreviousFrame() {
    Mlt::GeometryItem item;
    m_geom->prev_key(&item, ui.keyframeSlider->value() - 1);
    int pos = item.frame();
    ui.keyframeSlider->setValue(pos);
}

void Geometryval::moveEvent() {
    //if (event->button())

    QDomNodeList namenode = param.elementsByTagName("parameter");
    QDomNode pa = namenode.item(0);
    QRectF rec = paramRect->rect().translated(paramRect->pos());
    pa.attributes().namedItem("value").setNodeValue(
        QString("%1;%2;%3;%4;%5").arg(
            (int)rec.x()).arg(
            (int)rec.y()).arg(
            (int)(rec.x() + rec.width())).arg(
            (int)(rec.y() + rec.height())).arg(
            "100"
        )
    );
    //pa.attributes().namedItem("start").setNodeValue("50");
    QString dat;
    QTextStream stre(&dat);
    param.save(stre, 2);
    kDebug() << dat;
    emit parameterChanged();
}

QDomElement Geometryval::getParamDesc() {
    return param;
}

void Geometryval::setupParam(const QDomElement& par, int minFrame, int maxFrame) {
    param = par;
    QString val = par.attribute("value");
    char *tmp = (char *) qstrdup(val.toUtf8().data());
    m_geom = new Mlt::Geometry(tmp, val.count(';') + 1, m_profile.width, m_profile.height);
    delete[] tmp;
    //read param her and set rect
    ui.keyframeSlider->setRange(0, maxFrame - minFrame);
    m_helper->setKeyGeometry(m_geom, maxFrame - minFrame);
    QDomDocument doc;
    doc.appendChild(doc.importNode(par, true));
    kDebug() << "IMPORTED TRANS: " << doc.toString();

    Mlt::GeometryItem item;
    m_geom->fetch(&item, 0);
    paramRect = new QGraphicsRectItem(QRectF(0, 0, item.w(), item.h()));
    paramRect->setPos(item.x(), item.y());
    paramRect->setZValue(0);

    paramRect->setBrush(QColor(255, 0, 0, 40));
    paramRect->setPen(QPen(QBrush(QColor(255, 0, 0, 255)), 1.0));
    scene->addItem(paramRect);
}
