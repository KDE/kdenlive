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
#include "customtrackscene.h"
#include "customtrackview.h"

#include "kdenlivesettings.h"
#include "mainwindow.h"

#include "kdenlive_debug.h"

#include <QBrush>
#include <QDomElement>
#include <QPainter>
#include <QStyleOptionGraphicsItem>
#include <QPropertyAnimation>
#include <klocalizedstring.h>
#include <QMimeData>

Transition::Transition(const ItemInfo &info, int transitiontrack, double fps, const QDomElement &params, bool automaticTransition) :
    AbstractClipItem(info, QRectF(), fps),
    m_forceTransitionTrack(false),
    m_automaticTransition(automaticTransition),
    m_transitionTrack(transitiontrack)
{
    setZValue(4);
    m_info.cropDuration = info.endPos - info.startPos;
    if (QApplication::style()->styleHint(QStyle::SH_Widget_Animate, nullptr, QApplication::activeWindow())) {
        // animation disabled
        setRect(0, 0, m_info.cropDuration.frames(fps) - 0.02, (qreal) itemHeight());
    } else {
        QPropertyAnimation *startAnimation = new QPropertyAnimation(this, "rect");
        startAnimation->setDuration(200);
        const QRectF r(0, 0, m_info.cropDuration.frames(fps) - 0.02, (qreal) itemHeight() / 2);
        const QRectF r2(0, 0, m_info.cropDuration.frames(fps) - 0.02, (qreal)itemHeight());
        startAnimation->setStartValue(r);
        startAnimation->setEndValue(r2);
        startAnimation->setEasingCurve(QEasingCurve::OutQuad);
        startAnimation->start(QAbstractAnimation::DeleteWhenStopped);
    }

    m_info.cropStart = GenTime();
    m_maxDuration = GenTime(600);

    if (m_automaticTransition) {
        setBrush(QColor(200, 200, 50, 180));
    } else {
        setBrush(QColor(200, 100, 50, 180));
    }

    if (params.isNull()) {
        m_parameters = MainWindow::transitions.getEffectByTag(QStringLiteral("luma"), QStringLiteral("dissolve")).cloneNode().toElement();
    } else {
        m_parameters = params;
    }
    if (m_automaticTransition) {
        m_parameters.setAttribute(QStringLiteral("automatic"), 1);
    } else if (m_parameters.attribute(QStringLiteral("automatic")) == QLatin1String("1")) {
        m_automaticTransition = true;
    }
    if (m_parameters.attribute(QStringLiteral("force_track")) == QLatin1String("1")) {
        m_forceTransitionTrack = true;
    }
    m_name = i18n(m_parameters.firstChildElement("name").text().toUtf8().data());

    QDomNodeList namenode = m_parameters.elementsByTagName(QStringLiteral("parameter"));
    for (int i = 0; i < namenode.count(); ++i) {
        QDomElement pa = namenode.item(i).toElement();
        QString paramType = pa.attribute(QStringLiteral("type"));
        if (paramType == QLatin1String("geometry") || paramType == QLatin1String("animated") || paramType == QLatin1String("animatedrect")) {
            setAcceptDrops(true);
            break;
        }
    }
}

Transition::~Transition()
{
    blockSignals(true);
    if (scene()) {
        scene()->removeItem(this);
    }
}

Transition *Transition::clone(const ItemInfo &newInfo)
{
    const QDomElement xml = toXML().cloneNode().toElement();
    Transition *tr = new Transition(newInfo.isValid() ? newInfo : info(), transitionEndTrack(), m_fps, xml);
    return tr;
}

QString Transition::transitionTag() const
{
    return m_parameters.attribute(QStringLiteral("tag"));
}

QStringList Transition::transitionInfo() const
{
    QStringList info;
    info << m_name << m_parameters.attribute(QStringLiteral("tag")) << m_parameters.attribute(QStringLiteral("id"));
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
        m_parameters.setAttribute(QStringLiteral("automatic"), 1);
        setBrush(QColor(200, 200, 50, 180));
    } else {
        m_parameters.removeAttribute(QStringLiteral("automatic"));
        setBrush(QColor(200, 100, 50, 180));
    }
    update();
}

void Transition::setTransitionParameters(const QDomElement &params)
{
    if (m_parameters != params) {
        m_parameters = params;
        if (m_parameters.attribute(QStringLiteral("force_track")) == QLatin1String("1")) {
            setForcedTrack(true, m_parameters.attribute(QStringLiteral("transition_btrack")).toInt());
        } else if (m_parameters.attribute(QStringLiteral("force_track")) == QLatin1String("0")) {
            setForcedTrack(false, m_parameters.attribute(QStringLiteral("transition_btrack")).toInt());
        }
        m_name = i18n(m_parameters.firstChildElement("name").text().toUtf8().data());
        update();
        bool hasGeometry = false;
        QDomNodeList namenode = m_parameters.elementsByTagName(QStringLiteral("parameter"));
        for (int i = 0; i < namenode.count(); ++i) {
            QDomElement pa = namenode.item(i).toElement();
            QString paramType = pa.attribute(QStringLiteral("type"));
            if (paramType == QLatin1String("geometry") || paramType == QLatin1String("animated") || paramType == QLatin1String("animatedrect")) {
                hasGeometry = true;
                break;
            }
        }
        setAcceptDrops(hasGeometry);
    }
}

int Transition::transitionEndTrack() const
{
    return m_transitionTrack;
}

void Transition::updateTransitionEndTrack(int newtrack)
{
    if (!m_forceTransitionTrack) {
        m_transitionTrack = newtrack;
    }
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
    const QRectF exposed = painter->worldTransform().mapRect(option->exposedRect);
    const QRectF br = rect();
    QPen framePen;
    framePen.setWidthF(1.2);
    const QRectF mapped = painter->worldTransform().mapRect(br);

    QPointF p1(br.x(), br.y() + br.height() / 2 - 7);
    painter->setWorldTransform(QTransform());
    QPainterPath p;
    p.addRect(exposed);

    QPainterPath q;
    if (KdenliveSettings::clipcornertype() == 0) {
        q.addRoundedRect(mapped, 3, 3);
    } else {
        q.addRect(mapped);
    }
    painter->setClipPath(p.intersected(q));
    painter->fillRect(exposed, brush());
    const QString text = m_name + (m_forceTransitionTrack ? QStringLiteral("|>") : QString());

    // Draw clip name
    if (isSelected() || (parentItem() && parentItem()->isSelected())) {
        framePen.setColor(scene()->palette().highlight().color());
        framePen.setColor(Qt::red);
    } else {
        framePen.setColor(brush().color().darker());
    }

    const QRectF txtBounding = painter->boundingRect(mapped, Qt::AlignHCenter | Qt::AlignVCenter, QLatin1Char(' ') + text + QLatin1Char(' '));
    painter->setBrush(framePen.color());
    painter->setPen(framePen.color());
    painter->drawRoundedRect(txtBounding, 3, 3);
    painter->setBrush(QBrush(Qt::NoBrush));

    painter->setPen(Qt::white);
    painter->drawText(txtBounding, Qt::AlignCenter, text);

    // Draw frame
    if (KdenliveSettings::clipcornertype() == 1) {
        framePen.setJoinStyle(Qt::MiterJoin);
    }
    painter->setPen(framePen);
    painter->setClipping(false);
    painter->setRenderHint(QPainter::Antialiasing, true);
    if (KdenliveSettings::clipcornertype() == 0) {
        painter->drawRoundedRect(mapped.adjusted(0, 0, -0.5, -0.5), 3, 3);
    } else {
        painter->drawRect(mapped.adjusted(0, 0, -0.5, -0.5));
    }
}

int Transition::type() const
{
    return TransitionWidget;
}

//virtual
QVariant Transition::itemChange(GraphicsItemChange change, const QVariant &value)
{
    if (change == QGraphicsItem::ItemSelectedChange) {
        if (value.toBool()) {
            setZValue(5);
        } else {
            setZValue(4);
        }
    }
    CustomTrackScene *scene = nullptr;
    if (change == ItemPositionChange) {
        scene = projectScene();
    }
    if (scene) {
        // calculate new position.
        if (scene->isZooming) {
            // For some reason, mouse wheel on selected itm sometimes triggered
            // a position change event corrupting timeline, so discard it
            return pos();
        }
        QPointF newPos = value.toPointF();
        int xpos = projectScene()->getSnapPointForPos((int) newPos.x(), KdenliveSettings::snaptopoints());
        xpos = qMax(xpos, 0);
        newPos.setX(xpos);
        int newTrack = trackForPos(newPos.y());
        QList<int> lockedTracks = property("locked_tracks").value< QList<int> >();
        if (lockedTracks.contains(newTrack)) {
            // Trying to move to a locked track
            return pos();
        }
        int maximumTrack = projectScene()->tracksCount();
        newTrack = qMin(newTrack, maximumTrack);
        newTrack = qMax(newTrack, 0);
        newPos.setY(posForTrack(newTrack) + itemOffset());

        // Only one clip is moving
        QRectF sceneShape = rect();
        sceneShape.translate(newPos);
        QList<QGraphicsItem *> items;
        // TODO: manage transitions in OVERWRITE MODE
        //if (projectScene()->editMode() == NORMALEDIT)
        items = scene->items(sceneShape, Qt::IntersectsItemShape);
        items.removeAll(this);

        bool forwardMove = newPos.x() > pos().x();
        if (!items.isEmpty()) {
            for (int i = 0; i < items.count(); ++i) {
                if (!items.at(i)->isEnabled()) {
                    continue;
                }
                if (items.at(i)->type() == type()) {
                    int offset = 0;
                    // Collision!
                    QPointF otherPos = items.at(i)->pos();
                    if ((int) otherPos.y() != (int) pos().y()) {
                        return pos();
                    }
                    if (forwardMove) {
                        offset = qMax(offset, (int)(newPos.x() - (static_cast < AbstractClipItem * >(items.at(i))->startPos() - cropDuration()).frames(m_fps)));
                    } else {
                        offset = qMax(offset, (int)((static_cast < AbstractClipItem * >(items.at(i))->endPos().frames(m_fps)) - newPos.x()));
                    }

                    if (offset > 0) {
                        if (forwardMove) {
                            sceneShape.translate(QPointF(-offset, 0));
                            newPos.setX(newPos.x() - offset);
                        } else {
                            sceneShape.translate(QPointF(offset, 0));
                            newPos.setX(newPos.x() + offset);
                        }
                        QList<QGraphicsItem *> subitems = scene->items(sceneShape, Qt::IntersectsItemShape);
                        subitems.removeAll(this);
                        for (int j = 0; j < subitems.count(); ++j) {
                            if (!subitems.at(j)->isEnabled()) {
                                continue;
                            }
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
        ////qCDebug(KDENLIVE_LOG)<<"// ITEM NEW POS: "<<newPos.x()<<", mapped: "<<mapToScene(newPos.x(), 0).x();
        return newPos;
    }
    return QGraphicsItem::itemChange(change, value);
}

OperationType Transition::operationMode(const QPointF &pos, Qt::KeyboardModifiers)
{
    if (isItemLocked()) {
        return None;
    }

    const double scale = projectScene()->scale().x();
    double maximumOffset = 6 / scale;

    QRectF rect = sceneBoundingRect();
    if (qAbs((int)(pos.x())) < maximumOffset) {
        return ResizeStart;
    } else if (qAbs((int)(pos.x() - (rect.width()))) < maximumOffset) {
        return ResizeEnd;
    }
    return MoveOperation;
}

//static
int Transition::itemHeight()
{
    return (int)(KdenliveSettings::trackheight() / 3 * 2 - 1);
}

//static
int Transition::itemOffset()
{
    return (int)(KdenliveSettings::trackheight() / 3 * 2);
}

QDomElement Transition::toXML()
{
    m_parameters.setAttribute(QStringLiteral("type"), transitionTag());
    //m_transitionParameters.setAttribute("inverted", invertTransition());
    m_parameters.setAttribute(QStringLiteral("transition_atrack"), track());
    m_parameters.setAttribute(QStringLiteral("transition_btrack"), m_transitionTrack);
    m_parameters.setAttribute(QStringLiteral("start"), startPos().frames(m_fps));
    m_parameters.setAttribute(QStringLiteral("end"), endPos().frames(m_fps));
    m_parameters.setAttribute(QStringLiteral("force_track"), m_forceTransitionTrack);
    m_parameters.setAttribute(QStringLiteral("automatic"), m_automaticTransition);
    /*QDomNodeList namenode = m_parameters.elementsByTagName(QStringLiteral("parameter"));
    for (int i = 0; i < namenode.count() ; ++i) {
        QDomElement pa = namenode.item(i).toElement();
        if (pa.attribute(QStringLiteral("type")) == QLatin1String("simplekeyframe")) {
            if (pa.attribute(QStringLiteral("keyframes")).isEmpty()) {
                // Effect has a keyframe type parameter, we need to set the values
                pa.setAttribute(QStringLiteral("keyframes"), QStringLiteral("0=") + pa.attribute(QStringLiteral("default")));
            }
        }
    }*/
    return m_parameters.cloneNode().toElement();
}

bool Transition::hasGeometry()
{
    QDomNodeList namenode = m_parameters.elementsByTagName(QStringLiteral("parameter"));
    for (int i = 0; i < namenode.count(); ++i) {
        QDomElement pa = namenode.item(i).toElement();
        QString paramType = pa.attribute(QStringLiteral("type"));
        if (paramType == QLatin1String("geometry") || paramType == QLatin1String("animated") || paramType == QLatin1String("animatedrect")) {
            return true;
        }
    }
    return false;
}

bool Transition::updateKeyframes(const ItemInfo &oldInfo, const ItemInfo &newInfo)
{
    QString keyframes;
    QDomElement pa;
    bool modified = false;
    QDomNodeList namenode = m_parameters.elementsByTagName(QStringLiteral("parameter"));
    for (int i = 0; i < namenode.count(); ++i) {
        pa = namenode.item(i).toElement();
        QString type = pa.attribute(QStringLiteral("type"));
        if (type == QLatin1String("geometry") || type == QLatin1String("animated") || type == QLatin1String("animatedrect")) {
            keyframes = pa.attribute(QStringLiteral("value"));
            break;
        }
    }
    if (keyframes.isEmpty()) {
        return false;
    }
    int duration = newInfo.cropDuration.frames(m_fps) - 1;
    int oldEnd = oldInfo.cropDuration.frames(m_fps) - 1;
    QStringList values = keyframes.split(QLatin1Char(';'));
    int frame;
    int i = 0;
    if (oldEnd < duration) {
        // Transition was expanded, check if we had a keyframe at end position
        foreach (const QString &pos, values) {
            if (!pos.contains(QLatin1Char('='))) {
                ++i;
                continue;
            }
            frame = pos.section(QLatin1Char('='), 0, 0).remove(QLatin1Char('~')).remove(QLatin1Char('|')).toInt();
            if (frame == oldEnd) {
                // Move that keyframe to new end
                QString separator = QStringLiteral("=");
                if (pos.contains(QLatin1Char('~'))) {
                    separator.prepend(QLatin1Char('~'));
                } else if (pos.contains(QLatin1Char('|'))) {
                    separator.prepend(QLatin1Char('|'));
                }
                values[i] = QString::number(duration) + separator + pos.section(QLatin1Char('='), 1);
                pa.setAttribute(QStringLiteral("value"), values.join(QLatin1Char(';')));
                return true;
            }
            ++i;
        }
        return false;
    } else if (oldEnd > duration) {
        // Transition was shortened, check for out of bounds keyframes
        foreach (const QString &pos, values) {
            if (!pos.contains(QLatin1Char('='))) {
                ++i;
                continue;
            }
            frame = pos.section(QLatin1Char('='), 0, 0).remove(QLatin1Char('~')).remove(QLatin1Char('|')).toInt();
            if (frame > duration) {
                modified = true;
                break;
            }
            ++i;
        }
    }
    if (modified) {
        if (i > 0) {
            // Check if there is a keyframe at transition end
            QString prev = values.at(i - 1);
            bool done = false;
            if (prev.contains(QLatin1Char('='))) {
                int previousKeyframe = prev.section(QLatin1Char('='), 0, 0).remove(QLatin1Char('~')).remove(QLatin1Char('|')).toInt();
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
                QString separator = QStringLiteral("=");
                if (last.contains(QLatin1Char('~'))) {
                    separator.prepend(QLatin1Char('~'));
                } else if (last.contains(QLatin1Char('|'))) {
                    separator.prepend(QLatin1Char('|'));
                }
                last = QString::number(duration) + separator + last.section(QLatin1Char('='), 1);
                values[i] = last;
                while (values.count() > (i + 1)) {
                    values.removeLast();
                }
            }
        }
        pa.setAttribute(QStringLiteral("value"), values.join(QLatin1Char(';')));
    }
    return true;
}

void Transition::updateKeyframes(const QDomElement &/*effect*/)
{
}

//virtual
void Transition::dragEnterEvent(QGraphicsSceneDragDropEvent *event)
{
    if (isItemLocked()) {
        event->setAccepted(false);
    } else if (event->mimeData()->hasFormat(QStringLiteral("kdenlive/geometry"))) {
        event->acceptProposedAction();
        m_selectionTimer.start();
    } else {
        event->setAccepted(false);
    }
}

void Transition::dragLeaveEvent(QGraphicsSceneDragDropEvent *event)
{
    Q_UNUSED(event)
    if (m_selectionTimer.isActive()) {
        m_selectionTimer.stop();
    }
}

void Transition::dragMoveEvent(QGraphicsSceneDragDropEvent *event)
{
    QGraphicsItem::dragMoveEvent(event);
    if (m_selectionTimer.isActive() && !isSelected()) {
        m_selectionTimer.start();
    }
}

//virtual
void Transition::dropEvent(QGraphicsSceneDragDropEvent *event)
{
    if (scene() && !scene()->views().isEmpty()) {
        if (m_selectionTimer.isActive()) {
            m_selectionTimer.stop();
        }
        QString geometry = QString::fromUtf8(event->mimeData()->data(QStringLiteral("kdenlive/geometry")));
        event->acceptProposedAction();
        CustomTrackView *view = static_cast<CustomTrackView *>(scene()->views().first());
        if (view) {
            view->dropTransitionGeometry(this, geometry);
        }
    }
}

bool Transition::checkKeyFrames(int width, int height, int previousDuration, int cutPos)
{
    // go through all effects this clip has
    QDomDocument doc;
    doc.appendChild(doc.importNode(m_parameters, true));
    bool clipEffectsModified = resizeGeometries(m_parameters, width, height, previousDuration, cutPos == -1 ? 0 : cutPos, cropDuration().frames(m_fps) - 1, cropStart().frames(m_fps));
    QString newAnimation = resizeAnimations(m_parameters, previousDuration, cutPos == -1 ? 0 : cutPos, cropDuration().frames(m_fps) - 1, cropStart().frames(m_fps));
    if (!newAnimation.isEmpty()) {
        clipEffectsModified = true;
    }
    return clipEffectsModified;
}

