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
#include <qdom.h>
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


Transition::Transition(const QRectF& rect , ClipItem * clipa, const QString & type, const GenTime &startTime, const GenTime &endTime, double fps, bool inverted) : AbstractClipItem(rect) {
    m_invertTransition = inverted;
    m_singleClip = true;
    m_transitionTrack = clipa->track();
    m_secondClip = NULL;
    m_transitionName = type;
    m_fps = fps;
    m_cropDuration = endTime - startTime;
    m_startPos = startTime;
    m_maxDuration = GenTime(1000000);
    // Default duration = 2.5 seconds
    GenTime defaultTransitionDuration = GenTime(2.5);

    m_referenceClip = clipa;

    if (startTime < m_referenceClip->startPos()) m_transitionStart = GenTime(0.0);
    else if (startTime > m_referenceClip->endPos()) m_transitionStart = m_referenceClip->duration() - defaultTransitionDuration;
    else m_transitionStart = startTime - m_referenceClip->startPos();

    if (m_transitionStart + m_cropDuration > m_referenceClip->duration())
        m_transitionDuration = m_referenceClip->duration() - m_transitionStart;
    else m_transitionDuration = m_cropDuration;
    m_secondClip = 0;
    setFlags(QGraphicsItem::ItemClipsToShape | QGraphicsItem::ItemIsMovable | QGraphicsItem::ItemIsSelectable);
    setZValue(2);

    QDomDocument doc;
    doc.setContent(QString("<ktransition tag=\"luma\">\n\
                           <name>Luma</name>\n\
                           <description>Luma Transitions</description>\n\
                           <properties id=\"luma\" tag=\"luma\" />\n\
                           <parameter type=\"bool\" name=\"reverse\" max=\"1\" min=\"0\" default=\"1\" factor=\"1\">\n\
                           <name>Reverse</name>\n\
                           </parameter>\n\
                           </ktransition>"));
    m_transitionParameters = doc.documentElement();

    m_referenceClip->addTransition(this);
}

// create a transition from XML
Transition::Transition(const QRectF& rect , ClipItem * clip, QDomElement transitionElement, double fps, GenTime offset) : AbstractClipItem(rect) {
    if (offset == GenTime()) offset = clip->startPos();
    m_referenceClip = clip;
    m_singleClip = true;
    m_secondClip = NULL;
    m_transitionStart = GenTime(transitionElement.attribute("start", QString::null).toInt(), m_referenceClip->fps());
    m_transitionDuration = GenTime(transitionElement.attribute("end", QString::null).toInt(), m_referenceClip->fps()) - m_transitionStart;
    m_transitionTrack = transitionElement.attribute("transition_track", "0").toInt();
    m_transitionStart = m_transitionStart - offset;

    m_invertTransition = transitionElement.attribute("inverted", "0").toInt();
    m_transitionName = transitionElement.attribute("type" , "luma");

    // load transition parameters
    typedef QMap<QString, QString> ParamMap;
    ParamMap params;
    for (QDomNode n = transitionElement.firstChild(); !n.isNull(); n = n.nextSibling()) {
        QDomElement paramElement = n.toElement();
        params[paramElement.tagName()] = paramElement.attribute("value", QString::null);
    }
    if (m_invertTransition) params["reverse"] = "1";
    //if (!params.isEmpty()) setTransitionParameters(params);

    // Check if transition is valid (not outside of clip)
    if (m_transitionStart > clip->duration())
        m_transitionDuration = GenTime();

}

Transition::~Transition() {
}

QString Transition::transitionName() const {
    return m_transitionName;
}

void Transition::setTransitionParameters(const QDomElement & elem) {
    m_transitionParameters = elem;
    m_transitionName = elem.attribute("tag");
}

bool Transition::invertTransition() const {
    if (!m_singleClip) {
        if (m_referenceClip->startPos() < m_secondClip->startPos()) return true;
        else return false;
    }
    return m_invertTransition;
}

QPixmap Transition::transitionPixmap() const {
    KIcon icon;
    if (m_transitionName == "luma") {
        if (invertTransition()) icon = KIcon("kdenlive_trans_up");
        else icon = KIcon("kdenlive_trans_down");
    } else if (m_transitionName == "composite") {
        icon = KIcon("kdenlive_trans_wiper");
    } else if (m_transitionName == "lumafile") {
        icon = KIcon("kdenlive_trans_luma");
    } else icon = KIcon("kdenlive_trans_pip");
    return icon.pixmap(QSize(15, 15));
}

int Transition::transitionTrack() const {
    return m_transitionTrack;
}

void Transition::setTransitionTrack(int track) {
    m_transitionTrack = track;
}

void Transition::setTransitionDirection(bool inv) {
    m_invertTransition = inv;
}

int Transition::transitionStartTrack() const {
    return m_referenceClip->track();
}

int Transition::transitionEndTrack() const {
    if (!m_singleClip) return m_secondClip->track();
    return m_referenceClip->track() + 1;
    //TODO: calculate next video track
}

GenTime Transition::transitionDuration() const {
    return transitionEndTime() - transitionStartTime();
}

GenTime Transition::transitionStartTime() const {
    if (!m_singleClip) {
        GenTime startb = m_secondClip->startPos();
        GenTime starta = m_referenceClip->startPos();
        if (startb > m_referenceClip->endPos()) return m_referenceClip->endPos() - GenTime(0.12);
        if (startb > starta)
            return startb;
        return starta;
    } else return m_referenceClip->startPos() + m_transitionStart;
}


GenTime Transition::transitionEndTime() const {
    if (!m_singleClip) {
        GenTime endb = m_secondClip->endPos();
        GenTime enda = m_referenceClip->endPos();
        if (m_secondClip->startPos() > enda) return enda;
        if (endb < m_referenceClip->startPos()) return m_referenceClip->startPos() + GenTime(0.12);
        else if (endb > enda) return enda;
        else return endb;
    } else {
        if (m_transitionStart + m_transitionDuration > m_referenceClip->duration())
            return m_referenceClip->endPos();
        return m_referenceClip->startPos() + m_transitionStart + m_transitionDuration;
    }
}
void Transition::paint(QPainter *painter,
                       const QStyleOptionGraphicsItem *option,
                       QWidget *widget) {
    QRect rectInView = visibleRect();//this is the rect that is visible by the user

    if (rectInView.isNull())
        return;
    QPainterPath clippath;
    clippath.addRect(rectInView);
    QRectF br = rect();
    QPainterPath roundRectPathUpper = upperRectPart(br), roundRectPathLower = lowerRectPart(br);

    QPainterPath resultClipPath = roundRectPathUpper.united(roundRectPathLower);

    painter->setClipPath(resultClipPath.intersected(clippath), Qt::IntersectClip);
    //painter->fillPath(roundRectPath, brush()); //, QBrush(QColor(Qt::red)));
    painter->fillRect(br.intersected(rectInView), QBrush(QColor(200, 200, 0, 160)/*,Qt::Dense4Pattern*/));
    painter->setClipRect(option->exposedRect);
    painter->drawPixmap((int)(br.x() + 10), (int)(br.y() + 10), transitionPixmap());
    painter->drawPath(resultClipPath.intersected(clippath));
    painter->setPen(QColor(0, 0, 0, 180));
    painter->drawText((int)br.x() + 31, (int)br.y() + 21, transitionName());
    painter->setPen(QColor(255, 255, 255, 180));
    painter->drawText((int)br.x() + 30, (int)br.y() + 20, transitionName());
    QPen pen = painter->pen();
    if (isSelected()) {
        pen.setColor(Qt::red);
        pen.setWidth(2);
    } else {
        pen.setColor(Qt::black);
        pen.setWidth(1);
    }
    painter->setPen(pen);
    painter->setClipRect(option->exposedRect);
    painter->drawPath(resultClipPath.intersected(clippath));
}

int Transition::type() const {
    return TRANSITIONWIDGET;
}
OPERATIONTYPE Transition::operationMode(QPointF pos, double scale) {
    if (abs((int)(pos.x() - rect().x())) < 6) return RESIZESTART;
    else if (abs((int)(pos.x() - (rect().x() + rect().width()))) < 6) return RESIZEEND;
    return MOVE;
}
void Transition::resizeTransitionStart(GenTime time) {
    if (!m_singleClip) return; //cannot resize automatic transitions
    if (time < m_referenceClip->startPos()) time = m_referenceClip->startPos();
    // Transitions shouldn't be shorter than 3 frames, about 0.12 seconds
    if (transitionEndTime().ms() - time.ms() < 120.0) time = transitionEndTime() - GenTime(0.12);
    m_transitionDuration = m_transitionDuration - (time - m_referenceClip->startPos() - m_transitionStart);
    m_transitionStart = time - m_referenceClip->startPos();
}

void Transition::resizeTransitionEnd(GenTime time) {
    if (!m_singleClip) return; //cannot resize automatic transitions
    if (time > m_referenceClip->endPos()) time = m_referenceClip->endPos();
    // Transitions shouldn't be shorter than 3 frames, about 0.12 seconds
    if (time.ms() - transitionStartTime().ms() < 120.0) time = transitionStartTime() + GenTime(0.12);
    m_transitionDuration = time - (m_referenceClip->startPos() + m_transitionStart);
}

void Transition::moveTransition(GenTime time) {
    if (!m_singleClip) return; //cannot move automatic transitions
    if (m_transitionStart + time < GenTime(0.0)) m_transitionStart = GenTime(0.0);
    else if (m_transitionStart + time > m_referenceClip->duration() - m_transitionDuration)
        m_transitionStart = m_referenceClip->duration() - m_transitionDuration;
    else m_transitionStart = m_transitionStart + time;
    if (m_transitionStart < GenTime(0.0)) m_transitionStart = GenTime(0.0);
}

bool Transition::hasClip(const ClipItem * clip) const {
    if (clip == m_secondClip) return true;
    return false;
}

bool Transition::belongsToClip(const ClipItem * clip) const {
    if (clip == m_referenceClip) return true;
    return false;
}

Transition *Transition::clone() {
    return new Transition::Transition(rect(), m_referenceClip, this->toXML() , m_fps);
    /*if (m_singleClip || m_secondClip == 0)
        return new Transition::Transition(m_referenceClip);
    else
        //return new Transition::Transition(m_referenceClip, m_secondClip);
    return new Transition::Transition(m_referenceClip, this->toXML());
    */
}

Transition *Transition::reparent(ClipItem * clip) {
    return new Transition::Transition(rect(), clip, this->toXML(), m_fps, m_referenceClip->startPos());
}

bool Transition::isValid() const {
    return (m_transitionDuration != GenTime());
}

const ClipItem *Transition::referencedClip() const {
    return m_referenceClip;
}

QDomElement Transition::toXML() {
    m_transitionParameters.setAttribute("type", m_transitionName);
    //m_transitionParameters.setAttribute("inverted", invertTransition());
    m_transitionParameters.setAttribute("transition_track", m_transitionTrack);
    m_transitionParameters.setAttribute("start", transitionStartTime().frames(m_referenceClip->fps()));
    m_transitionParameters.setAttribute("end", transitionEndTime().frames(m_referenceClip->fps()));

    if (m_secondClip) {
        m_transitionParameters.setAttribute("clipb_starttime", m_secondClip->startPos().frames(m_referenceClip->fps()));
        m_transitionParameters.setAttribute("clipb_track", transitionEndTrack());
    }

    return m_transitionParameters;
}

