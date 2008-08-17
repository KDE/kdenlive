/***************************************************************************
                          transition.cpp  -  description
                             -------------------
    begin                : Tue Jan 24 2006
    copyright            : (C) 2006 by Jean-Baptiste Mardelle
    email                : jb@ader.ch
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include <QBrush>
#include <QDomElement>
#include <QPainter>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QScrollBar>
#include <QStyleOptionGraphicsItem>

#include <kdebug.h>
#include <KIcon>
#include <klocale.h>

#include "transition.h"
#include "clipitem.h"
#include "kdenlivesettings.h"
#include "customtrackscene.h"
#include "mainwindow.h"

Transition::Transition(const ItemInfo info, int transitiontrack, double fps, QDomElement params) : AbstractClipItem(info, QRectF(), fps), m_gradient(QLinearGradient(0, 0, 0, 0)) {
    setRect(0, 0, (qreal)(info.endPos - info.startPos).frames(fps) - 0.02, (qreal)(KdenliveSettings::trackheight() / 3 * 2 - 1));
    setPos((qreal) info.startPos.frames(fps), (qreal)(info.track * KdenliveSettings::trackheight() + KdenliveSettings::trackheight() / 3 * 2));

    m_singleClip = true;
    m_transitionTrack = transitiontrack;
    m_secondClip = NULL;
    m_cropStart = GenTime();
    m_maxDuration = GenTime(10000, fps);

    m_gradient.setColorAt(0, QColor(200, 200, 0, 150));
    m_gradient.setColorAt(1, QColor(200, 200, 200, 120));

    //m_referenceClip = clipa;
    if (params.isNull()) {
        m_parameters = MainWindow::transitions.getEffectByName("Luma");
    } else {
        m_parameters = params;
    }

    m_name = m_parameters.elementsByTagName("name").item(0).toElement().text();
    m_secondClip = 0;
    setFlags(QGraphicsItem::ItemClipsToShape | QGraphicsItem::ItemIsMovable | QGraphicsItem::ItemIsSelectable);
    setZValue(2);

    //m_referenceClip->addTransition(this);
}

Transition::~Transition() {
}

Transition *Transition::clone() {
    QDomElement xml = toXML().cloneNode().toElement();
    Transition *tr = new Transition(info(), transitionEndTrack(), m_fps, xml);
    return tr;
}

QString Transition::transitionName() const {
    return m_name;
}

QString Transition::transitionTag() const {
    return m_parameters.attribute("tag");
}

void Transition::setTransitionParameters(const QDomElement params) {
    m_parameters = params;
    m_name = m_parameters.elementsByTagName("name").item(0).toElement().text();
    update();
}


bool Transition::invertedTransition() const {
    return false; //m_parameters.attribute("reverse").toInt();
}

QPixmap Transition::transitionPixmap() const {
    KIcon icon;
    QString tag = transitionTag();
    if (tag == "luma") {
        if (invertedTransition()) icon = KIcon("kdenlive_trans_up");
        else icon = KIcon("kdenlive_trans_down");
    } else if (tag == "composite") {
        icon = KIcon("kdenlive_trans_wiper");
    } else if (tag == "lumafile") {
        icon = KIcon("kdenlive_trans_luma");
    } else icon = KIcon("kdenlive_trans_pip");
    return icon.pixmap(QSize(15, 15));
}


void Transition::setTransitionDirection(bool inv) {
    //m_parameters.setAttribute("reverse", inv);
}

int Transition::transitionEndTrack() const {
    return m_transitionTrack;
}

void Transition::updateTransitionEndTrack(int newtrack) {
    m_transitionTrack = newtrack;
}

void Transition::paint(QPainter *painter,
                       const QStyleOptionGraphicsItem *option,
                       QWidget *widget) {
    QRectF exposed = option->exposedRect;
    exposed.setRight(exposed.right() + 1);
    exposed.setBottom(exposed.bottom() + 1);
    painter->setClipRect(exposed);
    QRectF br = rect();
    m_gradient.setStart(0, br.y());
    m_gradient.setFinalStop(0, br.bottom());
    painter->fillRect(br, m_gradient);

    int top = (int)(br.y() + br.height() / 2 - 7);
    QPointF p1(br.x(), br.y() + br.height() / 2 - 7);
    painter->setMatrixEnabled(false);
    painter->drawPixmap(painter->matrix().map(p1) + QPointF(5, 0), transitionPixmap());
    painter->setPen(QColor(0, 0, 0, 180));
    top += painter->fontInfo().pixelSize();
    QPointF p2(br.x(), top);
    painter->drawText(painter->matrix().map(p2) + QPointF(26, 1), transitionName());
    painter->setPen(QColor(255, 255, 255, 180));
    QPointF p3(br.x(), top);
    painter->drawText(painter->matrix().map(p3) + QPointF(25, 0), transitionName());
    painter->setMatrixEnabled(true);
    QPen pen = painter->pen();
    if (isSelected()) {
        pen.setColor(Qt::red);
        //pen.setWidth(2);
    } else {
        pen.setColor(Qt::black);
        //pen.setWidth(1);
    }
    painter->setPen(pen);
    painter->drawRect(br);
}

int Transition::type() const {
    return TRANSITIONWIDGET;
}

//virtual
QVariant Transition::itemChange(GraphicsItemChange change, const QVariant &value) {
    if (change == ItemPositionChange && scene()) {
        // calculate new position.
        QPointF newPos = value.toPointF();
        int xpos = projectScene()->getSnapPointForPos((int) newPos.x(), KdenliveSettings::snaptopoints());
        xpos = qMax(xpos, 0);
        newPos.setX(xpos);
        int newTrack = newPos.y() / KdenliveSettings::trackheight();
        newTrack = qMin(newTrack, projectScene()->tracksCount() - 1);
        newTrack = qMax(newTrack, 0);
        newPos.setY((int)(newTrack * KdenliveSettings::trackheight() + KdenliveSettings::trackheight() / 3 * 2));
        // Only one clip is moving
        QRectF sceneShape = rect();
        sceneShape.translate(newPos);
        QList<QGraphicsItem*> items = scene()->items(sceneShape, Qt::IntersectsItemShape);
        items.removeAll(this);

        if (!items.isEmpty()) {
            for (int i = 0; i < items.count(); i++) {
                if (items.at(i)->type() == type()) {
                    // Collision! Don't move.
                    //kDebug()<<"/// COLLISION WITH ITEM: "<<items.at(i)->boundingRect()<<", POS: "<<items.at(i)->pos()<<", ME: "<<newPos;
                    QPointF otherPos = items.at(i)->pos();
                    if ((int) otherPos.y() != (int) pos().y()) return pos();
                    //kDebug()<<"////  CURRENT Y: "<<pos().y()<<", COLLIDING Y: "<<otherPos.y();
                    if (pos().x() < otherPos.x()) {
                        int npos = (static_cast < AbstractClipItem* >(items.at(i))->startPos() - m_cropDuration).frames(m_fps);
                        newPos.setX(npos);
                    } else {
                        // get pos just after colliding clip
                        int npos = static_cast < AbstractClipItem* >(items.at(i))->endPos().frames(m_fps);
                        newPos.setX(npos);
                    }
                    m_track = newTrack;
                    //kDebug()<<"// ITEM NEW POS: "<<newPos.x()<<", mapped: "<<mapToScene(newPos.x(), 0).x();
                    m_startPos = GenTime((int) newPos.x(), m_fps);
                    return newPos;
                }
            }
        }
        m_track = newTrack;
        m_startPos = GenTime((int) newPos.x(), m_fps);
        //kDebug()<<"// ITEM NEW POS: "<<newPos.x()<<", mapped: "<<mapToScene(newPos.x(), 0).x();
        return newPos;
    }
    return QGraphicsItem::itemChange(change, value);
}


OPERATIONTYPE Transition::operationMode(QPointF pos) {
    QRectF rect = sceneBoundingRect();
    if (qAbs((int)(pos.x() - rect.x())) < 6) return RESIZESTART;
    else if (qAbs((int)(pos.x() - (rect.right()))) < 6) return RESIZEEND;
    return MOVE;
}

bool Transition::hasClip(const ClipItem * clip) const {
    if (clip == m_secondClip) return true;
    return false;
}

bool Transition::belongsToClip(const ClipItem * clip) const {
    if (clip == m_referenceClip) return true;
    return false;
}

/*
Transition *Transition::clone() {
    return new Transition::Transition(rect(), m_referenceClip, this->toXML() , m_fps);
}*/

/*
Transition *Transition::reparent(ClipItem * clip) {
    return new Transition::Transition(rect(), clip, this->toXML(), m_fps, m_referenceClip->startPos());
}*/

bool Transition::isValid() const {
    return true; //(m_transitionDuration != GenTime());
}

const ClipItem *Transition::referencedClip() const {
    return m_referenceClip;
}

QDomElement Transition::toXML() {
    m_parameters.setAttribute("type", transitionTag());
    //m_transitionParameters.setAttribute("inverted", invertTransition());
    m_parameters.setAttribute("transition_atrack", track());
    m_parameters.setAttribute("transition_btrack", m_transitionTrack);
    m_parameters.setAttribute("start", startPos().frames(m_fps));
    m_parameters.setAttribute("end", endPos().frames(m_fps));

    if (m_secondClip) {
        m_parameters.setAttribute("clipb_starttime", m_secondClip->startPos().frames(m_referenceClip->fps()));
        m_parameters.setAttribute("clipb_track", transitionEndTrack());
    }

    return m_parameters;
}

