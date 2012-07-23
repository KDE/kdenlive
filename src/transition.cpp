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
#if QT_VERSION >= 0x040600
#include <QPropertyAnimation>
#endif

Transition::Transition(const ItemInfo &info, int transitiontrack, double fps, QDomElement params, bool automaticTransition) :
        AbstractClipItem(info, QRectF(), fps),
        m_forceTransitionTrack(false),
        m_automaticTransition(automaticTransition),
        m_secondClip(NULL),
        m_transitionTrack(transitiontrack)
{
    setZValue(3);
    m_info.cropDuration = info.endPos - info.startPos;
    setPos(info.startPos.frames(fps), (int)(info.track * KdenliveSettings::trackheight() + itemOffset() + 1));

#if QT_VERSION >= 0x040600
    if (!(KGlobalSettings::graphicEffectsLevel() & KGlobalSettings::SimpleAnimationEffects)) {
        // animation disabled
        setRect(0, 0, m_info.cropDuration.frames(fps) - 0.02, (qreal) itemHeight());
    }
    else {
        QPropertyAnimation *startAnimation = new QPropertyAnimation(this, "rect");
        startAnimation->setDuration(200);
        const QRectF r(0, 0, m_info.cropDuration.frames(fps) - 0.02, (qreal) itemHeight() / 2);
        const QRectF r2(0, 0, m_info.cropDuration.frames(fps) - 0.02, (qreal)itemHeight());
        startAnimation->setStartValue(r);
        startAnimation->setEndValue(r2);
        startAnimation->setEasingCurve(QEasingCurve::OutQuad);
        startAnimation->start(QAbstractAnimation::DeleteWhenStopped);
    }
#else
    setRect(0, 0, m_info.cropDuration.frames(fps) - 0.02, (qreal) itemHeight());
#endif

    m_info.cropStart = GenTime();
    m_maxDuration = GenTime(600);

    if (m_automaticTransition) setBrush(QColor(200, 200, 50, 180));
    else setBrush(QColor(200, 100, 50, 180));

    //m_referenceClip = clipa;
    if (params.isNull()) {
        m_parameters = MainWindow::transitions.getEffectByTag("luma", "dissolve").cloneNode().toElement();
    } else {
        m_parameters = params;
    }
    if (m_automaticTransition) m_parameters.setAttribute("automatic", 1);
    else if (m_parameters.attribute("automatic") == "1") m_automaticTransition = true;
    if (m_parameters.attribute("force_track") == "1") m_forceTransitionTrack = true;
    m_name = i18n(m_parameters.firstChildElement("name").text().toUtf8().data());
    m_secondClip = 0;

    //m_referenceClip->addTransition(this);
}

Transition::~Transition()
{
    blockSignals(true);
    if (scene()) scene()->removeItem(this);
}

Transition *Transition::clone()
{
    QDomElement xml = toXML().cloneNode().toElement();
    Transition *tr = new Transition(info(), transitionEndTrack(), m_fps, xml);
    return tr;
}

QString Transition::transitionTag() const
{
    return m_parameters.attribute("tag");
}

QStringList Transition::transitionInfo() const
{
    QStringList info;
    info << m_name << m_parameters.attribute("tag") << m_parameters.attribute("id");
    return info;
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
        setBrush(QColor(200, 200, 50, 180));
    } else {
        m_parameters.removeAttribute("automatic");
        setBrush(QColor(200, 100, 50, 180));
    }
    update();
}

void Transition::setTransitionParameters(const QDomElement params)
{
    m_parameters = params;
    if (m_parameters.attribute("force_track") == "1") setForcedTrack(true, m_parameters.attribute("transition_btrack").toInt());
    else if (m_parameters.attribute("force_track") == "0") setForcedTrack(false, m_parameters.attribute("transition_btrack").toInt());
    m_name = i18n(m_parameters.firstChildElement("name").text().toUtf8().data());
    update();
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
    QPen framePen;
    const QRectF mapped = painter->worldTransform().mapRect(br);

    painter->fillRect(exposed, brush());

    QPointF p1(br.x(), br.y() + br.height() / 2 - 7);
    painter->setWorldMatrixEnabled(false);
    const QString text = m_name + (m_forceTransitionTrack ? "|>" : QString());

    // Draw clip name
    if (isSelected() || (parentItem() && parentItem()->isSelected())) {
        framePen.setColor(Qt::red);
        framePen.setWidthF(2.0);
    }
    else {
        framePen.setColor(brush().color().darker());
    }

    const QRectF txtBounding = painter->boundingRect(mapped, Qt::AlignHCenter | Qt::AlignVCenter, ' ' + text + ' ');
    painter->setBrush(framePen.color());
    painter->setPen(Qt::NoPen);
    painter->drawRoundedRect(txtBounding, 3, 3);
    painter->setBrush(QBrush(Qt::NoBrush));

    painter->setPen(Qt::white);
    painter->drawText(txtBounding, Qt::AlignCenter, text);

    // Draw frame
    painter->setPen(framePen);
    painter->setClipping(false);
    painter->drawRect(mapped.adjusted(0, 0, -0.5, -0.5));
}

int Transition::type() const
{
    return TRANSITIONWIDGET;
}

//virtual
QVariant Transition::itemChange(GraphicsItemChange change, const QVariant &value)
{
    if (change == QGraphicsItem::ItemSelectedChange) {
        if (value.toBool()) setZValue(10);
        else setZValue(3);
    }
    if (change == ItemPositionChange && scene()) {
        // calculate new position.
        QPointF newPos = value.toPointF();
        int xpos = projectScene()->getSnapPointForPos((int) newPos.x(), KdenliveSettings::snaptopoints());
        xpos = qMax(xpos, 0);
        newPos.setX(xpos);
        int newTrack = newPos.y() / KdenliveSettings::trackheight();
        newTrack = qMin(newTrack, projectScene()->tracksCount() - 1);
        newTrack = qMax(newTrack, 0);
        newPos.setY((int)(newTrack * KdenliveSettings::trackheight() + itemOffset() + 1));
        // Only one clip is moving
        QRectF sceneShape = rect();
        sceneShape.translate(newPos);
        QList<QGraphicsItem*> items;
        // TODO: manage transitions in OVERWRITE MODE
        //if (projectScene()->editMode() == NORMALEDIT)
        items = scene()->items(sceneShape, Qt::IntersectsItemShape);
        items.removeAll(this);

        bool forwardMove = newPos.x() > pos().x();
        int offset = 0;
        if (!items.isEmpty()) {
            for (int i = 0; i < items.count(); i++) {
                if (!items.at(i)->isEnabled()) continue;
                if (items.at(i)->type() == type()) {
                    // Collision!
                    QPointF otherPos = items.at(i)->pos();
                    if ((int) otherPos.y() != (int) pos().y()) {
                        return pos();
                    }
                    if (forwardMove) {
                        offset = qMax(offset, (int)(newPos.x() - (static_cast < AbstractClipItem* >(items.at(i))->startPos() - cropDuration()).frames(m_fps)));
                    } else {
                        offset = qMax(offset, (int)((static_cast < AbstractClipItem* >(items.at(i))->endPos().frames(m_fps)) - newPos.x()));
                    }

                    if (offset > 0) {
                        if (forwardMove) {
                            sceneShape.translate(QPointF(-offset, 0));
                            newPos.setX(newPos.x() - offset);
                        } else {
                            sceneShape.translate(QPointF(offset, 0));
                            newPos.setX(newPos.x() + offset);
                        }
                        QList<QGraphicsItem*> subitems = scene()->items(sceneShape, Qt::IntersectsItemShape);
                        subitems.removeAll(this);
                        for (int j = 0; j < subitems.count(); j++) {
                            if (!subitems.at(j)->isEnabled()) continue;
                            if (subitems.at(j)->type() == type()) {
                                // move was not successful, revert to previous pos
                                m_info.startPos = GenTime((int) pos().x(), m_fps);
                                return pos();
                            }
                        }
                    }

                    m_info.track = newTrack;
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

//static
int Transition::itemHeight()
{
    return (int) (KdenliveSettings::trackheight() / 3 * 2 - 1);
}

//static
int Transition::itemOffset()
{
    return (int) (KdenliveSettings::trackheight() / 3 * 2);
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
    m_parameters.setAttribute("automatic", m_automaticTransition);

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

bool Transition::updateKeyframes()
{
    QString keyframes;
    QDomElement pa;
    bool modified = false;
    QDomNodeList namenode = m_parameters.elementsByTagName("parameter");
    for (int i = 0; i < namenode.count() ; i++) {
        pa = namenode.item(i).toElement();
        if (pa.attribute("type") == "geometry") {
            keyframes = pa.attribute("value");
            break;
        }
    }
    if (keyframes.isEmpty()) return false;
    int duration = cropDuration().frames(m_fps) - 1;
    QStringList values = keyframes.split(';');
    int frame;
    int i = 0;
    foreach(const QString &pos, values) {
        if (!pos.contains('=')) {
            i++;
            continue;
        }
        frame = pos.section('=', 0, 0).toInt();
        if (frame > duration) {
            modified = true;
            break;
        }
        i++;
    }
    if (modified) {
        if (i > 0) {
            // Check if there is a keyframe at transition end
            QString prev = values.at(i-1);
            bool done = false;
            if (prev.contains('=')) {
                int previousKeyframe = prev.section('=', 0, 0).toInt();
                if (previousKeyframe == duration) {
                    // Remove the last keyframes
                    while (values.count() > i) {
                        values.removeLast();
                    }
                    done = true;
                }
            }
            if (!done) {
                // Add new keyframe at end and remove last keyframes
                QString last = values.at(i);
                last = QString::number(duration) + '=' + last.section('=', 1);
                values[i] = last;
                while (values.count() > (i + 1)) {
                    values.removeLast();
                }
            }
        }
        pa.setAttribute("value", values.join(";"));
    }
    
    return true;
}

