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

#include "transition.h"
#include "clipitem.h"
#include "kdenlivesettings.h"
#include "customtrackscene.h"
#include "mainwindow.h"

#include <kdebug.h>
#include <KIcon>
#include <klocale.h>

#include <QBrush>
#include <QDomElement>
#include <QPainter>
#include <QStyleOptionGraphicsItem>


Transition::Transition(const ItemInfo info, int transitiontrack, double fps, QDomElement params, bool automaticTransition) :
        AbstractClipItem(info, QRectF(), fps),
        m_forceTransitionTrack(false),
        m_automaticTransition(automaticTransition),
        m_secondClip(NULL),
        m_transitionTrack(transitiontrack)
{
    setZValue(3);
    m_info.cropDuration = info.endPos - info.startPos;
    setPos(info.startPos.frames(fps), (qreal)(info.track * KdenliveSettings::trackheight() + KdenliveSettings::trackheight() / 3 * 2));

#if QT_VERSION >= 0x040600
    m_startAnimation = new QPropertyAnimation(this, "rect");
    m_startAnimation->setDuration(200);
    QRectF r(0, 0, m_info.cropDuration.frames(fps) - 0.02, 1);
    QRectF r2(0, 0, m_info.cropDuration.frames(fps) - 0.02, (qreal)(KdenliveSettings::trackheight() / 3 * 2 - 1));
    m_startAnimation->setStartValue(r);
    m_startAnimation->setEndValue(r2);
    m_startAnimation->setEasingCurve(QEasingCurve::OutQuad);
    m_startAnimation->start();
#else
    setRect(0, 0, m_info.cropDuration.frames(fps) - 0.02, (qreal)(KdenliveSettings::trackheight() / 3 * 2 - 1));
#endif

    m_info.cropStart = GenTime();
    m_maxDuration = GenTime(600);

    if (m_automaticTransition) setBrush(QColor(200, 200, 50, 100));
    else setBrush(QColor(200, 100, 50, 100));

    //m_referenceClip = clipa;
    if (params.isNull()) {
        m_parameters = MainWindow::transitions.getEffectByTag("luma", "dissolve").cloneNode().toElement();
    } else {
        m_parameters = params;
    }
    if (m_automaticTransition) m_parameters.setAttribute("automatic", 1);
    else if (m_parameters.attribute("automatic") == "1") m_automaticTransition = true;
    if (m_parameters.attribute("force_track") == "1") m_forceTransitionTrack = true;
    m_name = m_parameters.elementsByTagName("name").item(0).toElement().text();
    m_secondClip = 0;

    //m_referenceClip->addTransition(this);
}

Transition::~Transition()
{
    blockSignals(true);
#if QT_VERSION >= 0x040600
    delete m_startAnimation;
#endif
    if (scene()) scene()->removeItem(this);
}

Transition *Transition::clone()
{
    QDomElement xml = toXML().cloneNode().toElement();
    Transition *tr = new Transition(info(), transitionEndTrack(), m_fps, xml);
    return tr;
}

QString Transition::transitionName() const
{
    return m_name;
}

QString Transition::transitionTag() const
{
    return m_parameters.attribute("tag");
}

bool Transition::isAutomatic() const
{
    return m_automaticTransition;
}

void Transition::setAutomatic(bool automatic)
{
    m_automaticTransition = automatic;
    if (automatic) {
        m_parameters.setAttribute("automatic", 1);
        setBrush(QColor(200, 200, 50, 150));
    } else {
        m_parameters.removeAttribute("automatic");
        setBrush(QColor(200, 50, 50, 150));
    }
    update();
}

void Transition::setTransitionParameters(const QDomElement params)
{
    m_parameters = params;
    if (m_parameters.attribute("force_track") == "1") setForcedTrack(true, m_parameters.attribute("transition_btrack").toInt());
    else if (m_parameters.attribute("force_track") == "0") setForcedTrack(false, m_parameters.attribute("transition_btrack").toInt());
    m_name = m_parameters.elementsByTagName("name").item(0).toElement().text();
    update();
}


bool Transition::invertedTransition() const
{
    return false; //m_parameters.attribute("reverse").toInt();
}

void Transition::setTransitionDirection(bool /*inv*/)
{
    //m_parameters.setAttribute("reverse", inv);
}

int Transition::transitionEndTrack() const
{
    return m_transitionTrack;
}

void Transition::updateTransitionEndTrack(int newtrack)
{
    if (!m_forceTransitionTrack) m_transitionTrack = newtrack;
}

void Transition::setForcedTrack(bool force, int track)
{
    m_forceTransitionTrack = force;
    m_transitionTrack = track;
}

bool Transition::forcedTrack() const
{
    return m_forceTransitionTrack;
}

void Transition::paint(QPainter *painter,
                       const QStyleOptionGraphicsItem *option,
                       QWidget */*widget*/)
{
    const QRectF exposed = option->exposedRect;
    painter->setClipRect(exposed);
    const QRectF br = rect();
    const QRectF mapped = painter->matrix().mapRect(br);

    painter->fillRect(exposed, brush());

    //int top = (int)(br.y() + br.height() / 2 - 7);
    QPointF p1(br.x(), br.y() + br.height() / 2 - 7);
    painter->setMatrixEnabled(false);
    //painter->drawPixmap(painter->matrix().map(p1) + QPointF(5, 0), transitionPixmap());
    const QString text = m_name + (m_forceTransitionTrack ? "|>" : QString());

    // Draw clip name
    QColor frameColor(brush().color().darker());
    if (isSelected() || (parentItem() && parentItem()->isSelected())) {
        frameColor = QColor(Qt::red);
    }
    frameColor.setAlpha(160);

    const QRectF txtBounding = painter->boundingRect(mapped, Qt::AlignHCenter | Qt::AlignVCenter, ' ' + text + ' ');
    //painter->fillRect(txtBounding2, frameColor);
    painter->setBrush(frameColor);
    painter->setPen(Qt::NoPen);
    painter->drawRoundedRect(txtBounding, 3, 3);
    painter->setBrush(QBrush(Qt::NoBrush));

    painter->setPen(Qt::white);
    painter->drawText(txtBounding, Qt::AlignCenter, text);

    // Draw frame
    QPen pen = painter->pen();
    pen.setColor(frameColor);
    painter->setPen(pen);
    painter->setClipping(false);
    painter->drawRect(painter->matrix().mapRect(rect()));
}

int Transition::type() const
{
    return TRANSITIONWIDGET;
}

//virtual
QVariant Transition::itemChange(GraphicsItemChange change, const QVariant &value)
{
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
        QList<QGraphicsItem*> items;
        if (projectScene()->editMode() == NORMALEDIT)
            items = scene()->items(sceneShape, Qt::IntersectsItemShape);
        items.removeAll(this);

        if (!items.isEmpty()) {
            for (int i = 0; i < items.count(); i++) {
		if (!items.at(i)->isEnabled()) continue;
                if (items.at(i)->type() == type()) {
                    // Collision! Don't move.
                    //kDebug()<<"/// COLLISION WITH ITEM: "<<items.at(i)->boundingRect()<<", POS: "<<items.at(i)->pos()<<", ME: "<<newPos;
                    QPointF otherPos = items.at(i)->pos();
                    if ((int) otherPos.y() != (int) pos().y()) return pos();
                    //kDebug()<<"////  CURRENT Y: "<<pos().y()<<", COLLIDING Y: "<<otherPos.y();
                    if (pos().x() < otherPos.x()) {
                        int npos = (static_cast < AbstractClipItem* >(items.at(i))->startPos() - m_info.cropDuration).frames(m_fps);
                        newPos.setX(npos);
                    } else {
                        // get pos just after colliding clip
                        int npos = static_cast < AbstractClipItem* >(items.at(i))->endPos().frames(m_fps);
                        newPos.setX(npos);
                    }
                    m_info.track = newTrack;
                    //kDebug()<<"// ITEM NEW POS: "<<newPos.x()<<", mapped: "<<mapToScene(newPos.x(), 0).x();
                    m_info.startPos = GenTime((int) newPos.x(), m_fps);
                    return newPos;
                }
            }
        }
        m_info.track = newTrack;
        m_info.startPos = GenTime((int) newPos.x(), m_fps);
        //kDebug()<<"// ITEM NEW POS: "<<newPos.x()<<", mapped: "<<mapToScene(newPos.x(), 0).x();
        return newPos;
    }
    return QGraphicsItem::itemChange(change, value);
}


OPERATIONTYPE Transition::operationMode(QPointF pos)
{
    if (isItemLocked()) return NONE;

    const double scale = projectScene()->scale().x();
    double maximumOffset = 6 / scale;

    QRectF rect = sceneBoundingRect();
    if (qAbs((int)(pos.x() - rect.x())) < maximumOffset) return RESIZESTART;
    else if (qAbs((int)(pos.x() - (rect.right()))) < maximumOffset) return RESIZEEND;
    return MOVE;
}

bool Transition::hasClip(const ClipItem * clip) const
{
    if (clip == m_secondClip) return true;
    return false;
}

bool Transition::belongsToClip(const ClipItem * clip) const
{
    if (clip == m_referenceClip) return true;
    return false;
}

/*
Transition *Transition::clone() {
    return new Transition::Transition(rect(), m_referenceClip, toXML() , m_fps);
}*/

/*
Transition *Transition::reparent(ClipItem * clip) {
    return new Transition::Transition(rect(), clip, toXML(), m_fps, m_referenceClip->startPos());
}*/

bool Transition::isValid() const
{
    return true; //(m_transitionDuration != GenTime());
}

const ClipItem *Transition::referencedClip() const
{
    return m_referenceClip;
}

QDomElement Transition::toXML()
{
    m_parameters.setAttribute("type", transitionTag());
    //m_transitionParameters.setAttribute("inverted", invertTransition());
    m_parameters.setAttribute("transition_atrack", track());
    m_parameters.setAttribute("transition_btrack", m_transitionTrack);
    m_parameters.setAttribute("start", startPos().frames(m_fps));
    m_parameters.setAttribute("end", endPos().frames(m_fps));
    m_parameters.setAttribute("force_track", m_forceTransitionTrack);

    if (m_secondClip) {
        m_parameters.setAttribute("clipb_starttime", m_secondClip->startPos().frames(m_referenceClip->fps()));
        m_parameters.setAttribute("clipb_track", transitionEndTrack());
    }
    return m_parameters.cloneNode().toElement();
}

bool Transition::hasGeometry()
{
    QDomNodeList namenode = m_parameters.elementsByTagName("parameter");
    for (int i = 0; i < namenode.count() ; i++) {
        QDomElement pa = namenode.item(i).toElement();
        if (pa.attribute("type") == "geometry") return true;
    }
    return false;
}

int Transition::defaultZValue() const
{
    return 3;
}

