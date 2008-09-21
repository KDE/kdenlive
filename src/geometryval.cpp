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
#include <QGraphicsRectItem>
#include <QMouseEvent>
#include <QMenu>

#include <KDebug>

#include "graphicsscenerectmove.h"
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

    connect(m_helper, SIGNAL(positionChanged(int)), this, SLOT(slotPositionChanged(int)));

    scene = new GraphicsSceneRectMove(this);
    scene->setTool(TITLE_SELECT);
    view->setScene(scene);
    double aspect = (double) profile.sample_aspect_num / profile.sample_aspect_den * profile.width / profile.height;
    QGraphicsRectItem *m_frameBorder = new QGraphicsRectItem(QRectF(0, 0, profile.width, profile.height));
    m_frameBorder->setZValue(-1100);
    m_frameBorder->setBrush(QColor(255, 255, 0, 30));
    m_frameBorder->setPen(QPen(QBrush(QColor(255, 255, 255, 255)), 1.0, Qt::DashLine));
    scene->addItem(m_frameBorder);

    ui.buttonNext->setIcon(KIcon("media-skip-forward"));
    ui.buttonPrevious->setIcon(KIcon("media-skip-backward"));
    ui.buttonAdd->setIcon(KIcon("document-new"));
    ui.buttonDelete->setIcon(KIcon("edit-delete"));

    QMenu *configMenu = new QMenu(i18n("Misc..."), this);
    ui.buttonMenu->setIcon(KIcon("system-run"));
    ui.buttonMenu->setMenu(configMenu);
    ui.buttonMenu->setPopupMode(QToolButton::QToolButton::InstantPopup);

    //scene->setSceneRect(0, 0, profile.width * 2, profile.height * 2);
    //view->fitInView(m_frameBorder, Qt::KeepAspectRatio);
    const double sc = 100.0 / profile.height * 0.8;
    QRectF srect = view->sceneRect();
    view->setSceneRect(srect.x(), -srect.height() / 2, srect.width(), srect.height() * 2);
    view->scale(sc, sc);
    view->centerOn(m_frameBorder);
    connect(ui.buttonNext , SIGNAL(clicked()) , this , SLOT(slotNextFrame()));
    connect(ui.buttonPrevious , SIGNAL(clicked()) , this , SLOT(slotPreviousFrame()));
    connect(ui.buttonDelete , SIGNAL(clicked()) , this , SLOT(slotDeleteFrame()));
    connect(ui.buttonAdd , SIGNAL(clicked()) , this , SLOT(slotAddFrame()));
}

void Geometryval::slotTransparencyChanged(int transp) {
    int pos = ui.spinPos->value();
    Mlt::GeometryItem item;
    int error = m_geom->next_key(&item, pos);
    if (error || item.frame() != pos) {
        // no keyframe under cursor
        return;
    }
    item.mix(transp);
    paramRect->setBrush(QColor(255, 0, 0, transp));
    m_geom->insert(item);
    emit parameterChanged();
}

void Geometryval::slotPositionChanged(int pos) {
    ui.spinPos->setValue(pos);
    m_helper->setValue(pos);
    Mlt::GeometryItem item;
    int error = m_geom->fetch(&item, pos);
    if (error || item.key() == false) {
        // no keyframe under cursor, adjust buttons
        ui.buttonAdd->setEnabled(true);
        ui.buttonDelete->setEnabled(false);
        ui.widget->setEnabled(false);
        ui.spinTransp->setEnabled(false);
    } else {
        ui.buttonAdd->setEnabled(false);
        ui.buttonDelete->setEnabled(true);
        ui.widget->setEnabled(true);
        ui.spinTransp->setEnabled(true);
    }
    paramRect->setPos(item.x(), item.y());
    paramRect->setRect(0, 0, item.w(), item.h());
    ui.spinTransp->setValue(item.mix());
    paramRect->setBrush(QColor(255, 0, 0, item.mix()));
}

void Geometryval::slotDeleteFrame() {
    m_geom->remove(ui.spinPos->value());
    m_helper->update();
}

void Geometryval::slotAddFrame() {
    int pos = ui.spinPos->value();
    Mlt::GeometryItem item;
    item.frame(pos);
    item.x(paramRect->pos().x());
    item.y(paramRect->pos().y());
    item.w(paramRect->rect().width());
    item.h(paramRect->rect().height());
    item.mix(ui.spinTransp->value());
    m_geom->insert(item);
    ui.buttonAdd->setEnabled(false);
    ui.buttonDelete->setEnabled(true);
    ui.widget->setEnabled(true);
    ui.spinTransp->setEnabled(true);
    m_helper->update();
}

void Geometryval::slotNextFrame() {
    Mlt::GeometryItem item;
    int error = m_geom->next_key(&item, m_helper->value() + 1);
    kDebug() << "// SEEK TO NEXT KFR: " << error;
    if (error) return;
    int pos = item.frame();
    ui.spinPos->setValue(pos);
}

void Geometryval::slotPreviousFrame() {
    Mlt::GeometryItem item;
    int error = m_geom->prev_key(&item, m_helper->value() - 1);
    kDebug() << "// SEEK TO NEXT KFR: " << error;
    if (error) return;
    int pos = item.frame();
    ui.spinPos->setValue(pos);
}


QDomElement Geometryval::getParamDesc() {
    param.setAttribute("value", m_geom->serialise());
    kDebug() << " / / UPDATING TRANSITION VALUE: " << param.attribute("value");
    return param;
}

void Geometryval::setupParam(const QDomElement& par, int minFrame, int maxFrame) {
    param = par;
    QString val = par.attribute("value");
    char *tmp = (char *) qstrdup(val.toUtf8().data());
    m_geom = new Mlt::Geometry(tmp, maxFrame - minFrame, m_profile.width, m_profile.height);
    delete[] tmp;
    kDebug() << " / / UPDATING TRANSITION VALUE: " << m_geom->serialise();
    //read param her and set rect
    m_helper->setKeyGeometry(m_geom, maxFrame - minFrame - 1);
    QDomDocument doc;
    doc.appendChild(doc.importNode(par, true));
    kDebug() << "IMPORTED TRANS: " << doc.toString();
    ui.spinPos->setMaximum(maxFrame - minFrame - 1);
    Mlt::GeometryItem item;
    m_path = new QGraphicsPathItem();
    m_path->setPen(QPen(Qt::red));
    updateTransitionPath();

    connect(scene, SIGNAL(actionFinished()), this, SLOT(slotUpdateTransitionProperties()));

    m_geom->fetch(&item, 0);
    paramRect = new QGraphicsRectItem(QRectF(0, 0, item.w(), item.h()));
    paramRect->setPos(item.x(), item.y());
    paramRect->setZValue(0);

    paramRect->setPen(QPen(QBrush(QColor(255, 0, 0, 255)), 1.0));
    scene->addItem(paramRect);
    scene->addItem(m_path);
    slotPositionChanged(0);
    connect(ui.spinPos, SIGNAL(valueChanged(int)), this , SLOT(slotPositionChanged(int)));
    connect(ui.spinTransp, SIGNAL(valueChanged(int)), this , SLOT(slotTransparencyChanged(int)));
}

void Geometryval::updateTransitionPath() {
    Mlt::GeometryItem item;
    int pos = 0;
    int counter = 0;
    QPainterPath path;
    while (true) {
        if (m_geom->next_key(&item, pos) == 1) break;
        pos = item.frame();
        if (counter == 0) {
            path.moveTo(item.x() + item.w() / 2, item.y() + item.h() / 2);
        } else {
            path.lineTo(item.x() + item.w() / 2, item.y() + item.h() / 2);
        }
        counter++;
        pos++;
    }
    m_path->setPath(path);
}

void Geometryval::slotUpdateTransitionProperties() {
    int pos = ui.spinPos->value();
    Mlt::GeometryItem item;
    int error = m_geom->next_key(&item, pos);
    if (error || item.frame() != pos) {
        // no keyframe under cursor
        return;
    }
    item.x(paramRect->pos().x());
    item.y(paramRect->pos().y());
    item.w(paramRect->rect().width());
    item.h(paramRect->rect().height());
    m_geom->insert(item);
    updateTransitionPath();
    emit parameterChanged();
}
