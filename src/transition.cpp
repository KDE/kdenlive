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

#include <kdebug.h>
#include <KIcon>
#include <klocale.h>

#include "transition.h"
#include "clipitem.h"
#include "kdenlivesettings.h"


Transition::Transition(const QRectF& rect , ClipItem * clipa, const TRANSITIONTYPE & type, const GenTime &startTime, const GenTime &endTime, double fps, bool inverted) : AbstractClipItem(rect) {
    m_invertTransition = inverted;
    m_singleClip = true;
    m_transitionTrack = 0;
    m_secondClip = NULL;
    m_transitionType = type;
    m_transitionName = getTransitionName(m_transitionType);
    m_fps = fps;
    GenTime duration = endTime - startTime;

    // Default duration = 2.5 seconds
    GenTime defaultTransitionDuration = GenTime(2.5);

    m_referenceClip = clipa;
    if (startTime < m_referenceClip->startPos()) m_transitionStart = GenTime(0.0);
    else if (startTime > m_referenceClip->endPos()) m_transitionStart = m_referenceClip->duration() - defaultTransitionDuration;
    else m_transitionStart = startTime - m_referenceClip->startPos();

    if (m_transitionStart + duration > m_referenceClip->duration())
        m_transitionDuration = m_referenceClip->duration() - m_transitionStart;
    else m_transitionDuration = duration;
    m_secondClip = 0;
    setFlags(QGraphicsItem::ItemClipsToShape | QGraphicsItem::ItemIsMovable | QGraphicsItem::ItemIsSelectable);
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
    uint transType = transitionElement.attribute("type", "0").toInt();
    if (transType == LUMA_TRANSITION) m_transitionType = LUMA_TRANSITION;
    else if (transType == COMPOSITE_TRANSITION) m_transitionType = COMPOSITE_TRANSITION;
    else if (transType == PIP_TRANSITION) m_transitionType = PIP_TRANSITION;
    else if (transType == LUMAFILE_TRANSITION) m_transitionType = LUMAFILE_TRANSITION;
    else if (transType == MIX_TRANSITION) m_transitionType = MIX_TRANSITION;

    // load transition parameters
    typedef QMap<QString, QString> ParamMap;
    ParamMap params;
    for (QDomNode n = transitionElement.firstChild(); !n.isNull(); n = n.nextSibling()) {
        QDomElement paramElement = n.toElement();
        params[paramElement.tagName()] = paramElement.attribute("value", QString::null);
    }
    if (m_invertTransition) params["reverse"] = "1";
    if (!params.isEmpty()) setTransitionParameters(params);

    // Check if transition is valid (not outside of clip)
    if (m_transitionStart > clip->duration())
        m_transitionDuration = GenTime();

}

Transition::~Transition() {
}

void Transition::setTransitionType(TRANSITIONTYPE newType) {
    m_transitionType = newType;
    m_transitionName = getTransitionName(m_transitionType);
}

TRANSITIONTYPE Transition::transitionType() const {
    return m_transitionType;
}

QString Transition::transitionTag()const {
    switch (m_transitionType) {
    case COMPOSITE_TRANSITION:
        return "composite";
    case PIP_TRANSITION:
        return "composite";
    case MIX_TRANSITION:
        return "mix";
    default:
        return "luma";

    }
}

QString Transition::getTransitionName(const TRANSITIONTYPE & type) {
    if (type == COMPOSITE_TRANSITION) return i18n("Push");
    else if (type == PIP_TRANSITION) return i18n("Pip");
    else if (type == LUMAFILE_TRANSITION) return i18n("Wipe");
    else if (type == MIX_TRANSITION) return i18n("Audio Fade");
    return i18n("Crossfade");
}

TRANSITIONTYPE Transition::getTransitionForName(const QString & type) {
    if (type == i18n("Push")) return COMPOSITE_TRANSITION;
    else if (type == i18n("Pip")) return PIP_TRANSITION;
    else if (type == i18n("Wipe")) return LUMAFILE_TRANSITION;
    return LUMA_TRANSITION;
}


QString Transition::transitionName() const {
    return m_transitionName;
}

void Transition::setTransitionParameters(const QMap < QString, QString > parameters) {
    m_transitionParameters = parameters;
}

const QMap < QString, QString > Transition::transitionParameters() const {
    return m_transitionParameters;
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
    if (m_transitionType == LUMA_TRANSITION) {
        if (invertTransition()) icon = KIcon("kdenlive_trans_down");
        else icon = KIcon("kdenlive_trans_up");
    } else if (m_transitionType == COMPOSITE_TRANSITION) {
        icon = KIcon("kdenlive_trans_wiper");
    } else if (m_transitionType == LUMAFILE_TRANSITION) {
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
    painter->fillRect(rect(), QBrush(Qt::green));
}

int Transition::type() const {
    return 70001;
}
OPERATIONTYPE Transition::operationMode(QPointF pos, double scale) {
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
    QDomDocument doc;
    QDomElement effect = doc.createElement("ktransition");
    effect.setAttribute("type", transitionType());
    effect.setAttribute("inverted", invertTransition());
    effect.setAttribute("transition_track", m_transitionTrack);
    effect.setAttribute("start", transitionStartTime().frames(m_referenceClip->fps()));
    effect.setAttribute("end", transitionEndTime().frames(m_referenceClip->fps()));

    if (m_secondClip) {
        effect.setAttribute("clipb_starttime", m_secondClip->startPos().frames(m_referenceClip->fps()));
        effect.setAttribute("clipb_track", transitionEndTrack());
    }


    QMap<QString, QString>::Iterator it;
    for (it = m_transitionParameters.begin(); it != m_transitionParameters.end(); ++it) {
        QDomElement param = doc.createElement(it.key());
        param.setAttribute("value", it.value());
        effect.appendChild(param);
    }

    return effect;
}

GenTime Transition::startPos() const {
    return GenTime();
}

GenTime Transition::endPos() const {
    return GenTime();
}

int Transition::track() const {
    return 0;
}

GenTime Transition::duration() const {
    return GenTime();
}
