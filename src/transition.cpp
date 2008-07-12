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
#include <QLinearGradient>
#include <QScrollBar>
#include <QStyleOptionGraphicsItem>

#include <kdebug.h>
#include <KIcon>
#include <klocale.h>

#include "transition.h"
#include "clipitem.h"
#include "kdenlivesettings.h"
#include "mainwindow.h"

Transition::Transition(const ItemInfo info, int transitiontrack, double scale, double fps, QDomElement params) : AbstractClipItem(info, QRectF(info.startPos.frames(fps) *scale , info.track * KdenliveSettings::trackheight() + KdenliveSettings::trackheight() / 3 * 2, (info.endPos - info.startPos).frames(fps) * scale , KdenliveSettings::trackheight() / 3 * 2 - 1), fps), m_gradient(QLinearGradient(0, 0, 0, 0)) {
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

    painter->setClipRect(option->exposedRect);
    QRectF br = rect();
    QPainterPath roundRectPathUpper = upperRectPart(br), roundRectPathLower = lowerRectPart(br);
    QPainterPath resultClipPath = roundRectPathUpper.united(roundRectPathLower);


#if 0
    QRadialGradient radialGrad(QPointF(br.x() + 50, br.y() + 20), 70);
    radialGrad.setColorAt(0, QColor(200, 200, 0, 100));
    radialGrad.setColorAt(0.5, QColor(150, 150, 0, 100));
    radialGrad.setColorAt(1, QColor(100, 100, 0, 100));
    painter->fillRect(br.intersected(rectInView), QBrush(radialGrad)/*,Qt::Dense4Pattern*/);
#else
    m_gradient.setStart(0, br.y());
    m_gradient.setFinalStop(0, br.bottom());
    painter->fillPath(resultClipPath, m_gradient);
#endif

    int top = (int)(br.y() + br.height() / 2 - 7);
    painter->drawPixmap((int)(br.x() + 10), top, transitionPixmap());
    painter->setPen(QColor(0, 0, 0, 180));
    top += painter->fontInfo().pixelSize();
    painter->drawText((int)br.x() + 31, top + 1, transitionName());
    painter->setPen(QColor(255, 255, 255, 180));
    painter->drawText((int)br.x() + 30, top, transitionName());
    QPen pen = painter->pen();
    if (isSelected()) {
        pen.setColor(Qt::red);
        //pen.setWidth(2);
    } else {
        pen.setColor(Qt::black);
        //pen.setWidth(1);
    }
    painter->setPen(pen);
    painter->drawPath(resultClipPath);
}

int Transition::type() const {
    return TRANSITIONWIDGET;
}

OPERATIONTYPE Transition::operationMode(QPointF pos, double scale) {
    if (qAbs((int)(pos.x() - rect().x())) < 6) return RESIZESTART;
    else if (qAbs((int)(pos.x() - (rect().x() + rect().width()))) < 6) return RESIZEEND;
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

