/***************************************************************************
 *   Copyright (C) 2008 by Marco Gittler (g.marco@freenet.de)              *
 *   Copyright (C) 2008 by Jean-Baptiste Mardelle (jb@kdenlive.org)        *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA          *
 ***************************************************************************/

#include "abstractclipitem.h"
#include "customtrackscene.h"
#include "kdenlivesettings.h"
#include "customtrackview.h"

#include "mlt++/Mlt.h"

#include <QDebug>
#include <klocalizedstring.h>

#include <QApplication>
#include <QPainter>
#include <QGraphicsSceneMouseEvent>
#include <QParallelAnimationGroup>
#include <QPropertyAnimation>

AbstractClipItem::AbstractClipItem(const ItemInfo &info, const QRectF& rect, double fps) :
        QObject()
        , QGraphicsRectItem(rect)
        , m_info(info)
        , m_editedKeyframe(-1)
        , m_selectedKeyframe(0)
        , m_keyframeType(KEYFRAMETYPE::NoKeyframe)
        , m_keyframeDefault(0)
        , m_keyframeMin(0)
        , m_keyframeMax(1)
        , m_keyframeFactor(1)
        , m_visibleParam(0)
        , m_fps(fps)
        , m_isMainSelectedClip(false)
{
    setFlags(QGraphicsItem::ItemIsMovable | QGraphicsItem::ItemIsSelectable);
    setFlag(QGraphicsItem::ItemSendsGeometryChanges, true);
    setFlag(QGraphicsItem::ItemUsesExtendedStyleOption, true);
    setPen(Qt::NoPen);
    m_handleSize = QFontInfo(QApplication::font()).pixelSize() * 0.7;
}

AbstractClipItem::~AbstractClipItem()
{
}

void AbstractClipItem::closeAnimation()
{
    if (!isEnabled()) return;
    setEnabled(false);
    setFlag(QGraphicsItem::ItemIsSelectable, false);
    if (QApplication::style()->styleHint(QStyle::SH_Widget_Animate, 0, QApplication::activeWindow())) {
        // animation disabled
        deleteLater();
        return;
    }
    QPropertyAnimation *closeAnimation = new QPropertyAnimation(this, "rect");
    QPropertyAnimation *closeAnimation2 = new QPropertyAnimation(this, "opacity");
    closeAnimation->setDuration(200);
    closeAnimation2->setDuration(200);
    QRectF r = rect();
    QRectF r2 = r;
    r2.setLeft(r.left() + r.width() / 2);
    r2.setTop(r.top() + r.height() / 2);
    r2.setWidth(1);
    r2.setHeight(1);
    closeAnimation->setStartValue(r);
    closeAnimation->setEndValue(r2);
    closeAnimation->setEasingCurve(QEasingCurve::InQuad);
    closeAnimation2->setStartValue(1.0);
    closeAnimation2->setEndValue(0.0);
    QParallelAnimationGroup *group = new QParallelAnimationGroup;
    connect(group, SIGNAL(finished()), this, SLOT(deleteLater()));
    group->addAnimation(closeAnimation);
    group->addAnimation(closeAnimation2);
    group->start(QAbstractAnimation::DeleteWhenStopped);
}

ItemInfo AbstractClipItem::info() const
{
    ItemInfo info = m_info;
    info.cropStart = cropStart();
    info.endPos = endPos();
    return info;
}

GenTime AbstractClipItem::endPos() const
{
    return m_info.startPos + m_info.cropDuration;
}

int AbstractClipItem::track() const
{
    return m_info.track;
}

GenTime AbstractClipItem::cropStart() const
{
    return m_info.cropStart;
}

GenTime AbstractClipItem::cropDuration() const
{
    return m_info.cropDuration;
}

void AbstractClipItem::setCropStart(const GenTime &pos)
{
    m_info.cropStart = pos;
}

void AbstractClipItem::updateItem(int track)
{
    m_info.track = track;
    m_info.startPos = GenTime((int) scenePos().x(), m_fps);
}

void AbstractClipItem::updateRectGeometry()
{
    setRect(0, 0, cropDuration().frames(m_fps) - 0.02, rect().height());
}

void AbstractClipItem::resizeStart(int posx, bool hasSizeLimit, bool /*emitChange*/)
{
    GenTime durationDiff = GenTime(posx, m_fps) - m_info.startPos;
    if (durationDiff == GenTime()) return;

    if (type() == AVWidget && hasSizeLimit && (cropStart() + durationDiff < GenTime())) {
        durationDiff = GenTime() - cropStart();
    } else if (durationDiff >= cropDuration()) {
        return;
    }
    m_info.startPos += durationDiff;

    // set to true if crop from start is negative (possible for color clips, images as they have no size limit)
    bool negCropStart = false;
    if (type() == AVWidget) {
        m_info.cropStart += durationDiff;
        if (m_info.cropStart < GenTime())
            negCropStart = true;
    }

    m_info.cropDuration -= durationDiff;
    setRect(0, 0, cropDuration().frames(m_fps) - 0.02, rect().height());
    moveBy(durationDiff.frames(m_fps), 0);

    if (m_info.startPos != GenTime(posx, m_fps)) {
        GenTime diff = m_info.startPos - GenTime(posx, m_fps);

        if (type() == AVWidget)
            m_info.cropStart += diff;

        m_info.cropDuration -= diff;
        setRect(0, 0, cropDuration().frames(m_fps) - 0.02, rect().height());
    }

    // set crop from start to 0 (isn't relevant as this only happens for color clips, images)
    if (negCropStart)
        m_info.cropStart = GenTime();
}

void AbstractClipItem::resizeEnd(int posx, bool /*emitChange*/)
{
    GenTime durationDiff = GenTime(posx, m_fps) - endPos();
    if (durationDiff == GenTime()) return;
    if (cropDuration() + durationDiff <= GenTime()) {
        durationDiff = GenTime() - (cropDuration() - GenTime(3, m_fps));
    }

    m_info.cropDuration += durationDiff;
    m_info.endPos += durationDiff;

    setRect(0, 0, cropDuration().frames(m_fps) - 0.02, rect().height());
    if (durationDiff > GenTime()) {
        QList <QGraphicsItem *> collisionList = collidingItems(Qt::IntersectsItemBoundingRect);
        bool fixItem = false;
        for (int i = 0; i < collisionList.size(); ++i) {
            if (!collisionList.at(i)->isEnabled()) continue;
            QGraphicsItem *item = collisionList.at(i);
            if (item->type() == type() && item->pos().x() > pos().x()) {
                GenTime diff = static_cast<AbstractClipItem*>(item)->startPos() - startPos();
                if (fixItem == false || diff < m_info.cropDuration) {
                    fixItem = true;
                    m_info.cropDuration = diff;
                }
            }
        }
        if (fixItem) setRect(0, 0, cropDuration().frames(m_fps) - 0.02, rect().height());
    }
}

GenTime AbstractClipItem::startPos() const
{
    return m_info.startPos;
}

double AbstractClipItem::fps() const
{
    return m_fps;
}

void AbstractClipItem::updateFps(double fps)
{
    m_fps = fps;
    setPos((qreal) startPos().frames(m_fps), pos().y());
    updateRectGeometry();
}

GenTime AbstractClipItem::maxDuration() const
{
    return m_maxDuration;
}

double AbstractClipItem::keyframeUnmap(double y) {
    QRectF br = rect();
    return ((br.bottom() - y) / br.height() * (m_keyframeMax - m_keyframeMin) + m_keyframeMin) / m_keyframeFactor;
}

double AbstractClipItem::keyframeMap(double value) {
    QRectF br = rect();
    return br.bottom() - br.height() * (value * m_keyframeFactor - m_keyframeMin) / (m_keyframeMax - m_keyframeMin);
}

QPointF AbstractClipItem::keyframeMap(int frame, double value) {
    QRectF br = rect();
    return QPointF(br.x() + br.width() * (frame - cropStart().frames(m_fps)) / cropDuration().frames(m_fps),
                   br.bottom() - br.height() * (value * m_keyframeFactor - m_keyframeMin) / (m_keyframeMax - m_keyframeMin));
}

QPointF AbstractClipItem::keyframePoint(int index) {
    int frame = m_keyAnim.key_get_frame(index);
    return keyframeMap(frame, m_keyProperties.anim_get_double("keyframes", frame));
}

void AbstractClipItem::drawKeyFrames(QPainter *painter, const QTransform &transformation)
{
    if (m_keyframeType == NoKeyframe || m_keyAnim.key_count() < 1)
        return;
    painter->save();
    bool active = isSelected() || (parentItem() && parentItem()->isSelected());
    QRectF br = rect();
    QPointF h(m_handleSize, m_handleSize);

    // draw keyframes
    // Special case: Geometry keyframes are just vertical lines
    if (m_keyframeType == GeometryKeyframe) {
        for(int i = 0; i < m_keyAnim.key_count(); ++i) {
            int key = m_keyAnim.key_get_frame(i);
            QColor color = (key == m_editedKeyframe) ? QColor(Qt::red) : QColor(Qt::blue);
            if (active)
                painter->setPen(color);
            QPointF k = keyframePoint(i);
            painter->drawLine(transformation.map(QLineF(k.x(), br.top(), k.x(), br.height())));
            if (active) {
                k.setY(br.top() + br.height()/2);
                painter->setBrush((m_keyAnim.key_get_frame(i) == m_editedKeyframe) ? QColor(Qt::red) : QColor(Qt::blue));
                painter->drawEllipse(QRectF(transformation.map(k) - h/2, transformation.map(k) + h/2));
            }
        }
        painter->restore();
        return;
    }

    // draw line showing default value
    if (active) {
        QColor col(Qt::black);
        col.setAlpha(140);
        painter->fillRect(QRectF(transformation.map(br.topLeft()), transformation.map(br.bottomRight())), col);
        double y = keyframeMap(m_keyframeDefault);
        QLineF line = transformation.map(QLineF(br.x(), y, br.right(), y));
        painter->setPen(QColor(168, 168, 168, 180));
        painter->drawLine(line);
        painter->setPen(QColor(108, 108, 108, 180));
        painter->drawLine(line.translated(0, 1));
        painter->setPen(QColor(Qt::white));
        painter->setRenderHint(QPainter::Antialiasing);
    }

    QPointF start = keyframePoint(0);
    QPainterPath path;
    path.moveTo(br.x(), br.bottom());
    path.lineTo(br.x(), start.y());
    path.lineTo(start);
    for(int i = 0; i < m_keyAnim.key_count(); ++i) {
        if (active) {
            painter->setBrush((m_keyAnim.key_get_frame(i) == m_editedKeyframe) ? QColor(Qt::red) : QColor(Qt::blue));
            painter->drawEllipse(QRectF(transformation.map(start) - h/2, transformation.map(start) + h / 2));
        }
        if (i + 1 < m_keyAnim.key_count()) {
            QPointF end = keyframePoint(i + 1);
            switch (m_keyAnim.key_get_type(i)) {
                case mlt_keyframe_discrete:
                    path.lineTo(end.x(), start.y());
                    path.lineTo(end);
                    break;
                case mlt_keyframe_linear:
                    path.lineTo(end);
                    break;
                case mlt_keyframe_smooth:
                    QPointF pre = keyframePoint(qMax(i - 1, 0));
                    QPointF post = keyframePoint(qMin(i + 2, m_keyAnim.key_count() - 1));
                    QPointF c1 = (end - pre) / 6.0; // + start
                    QPointF c2 = (start - post) / 6.0; // + end
                    double mid = (end.x() - start.x()) / 2;
                    if (c1.x() >  mid) c1 = c1 * mid / c1.x(); // scale down tangent vector to not go beyond middle
                    if (c2.x() < -mid) c2 = c2 * -mid / c2.x();
                    path.cubicTo(start + c1, end + c2, end);
                    break;
            }
            start = end;
        } else {
            path.lineTo(br.right(), start.y());
        }
    }
    path.lineTo(br.right(), br.bottom());
    QColor col(Qt::white);//QApplication::palette().highlight().color());
    col.setAlpha(active ? 120 : 80);
    painter->setBrush(col);
    painter->drawPath(transformation.map(path));
    painter->restore();
}

int AbstractClipItem::mouseOverKeyFrames(QPointF pos, double maxOffset, double scale)
{
    pos.setX(pos.x()*scale);
    for(int i = 0; i < m_keyAnim.key_count(); ++i) {
        int key = m_keyAnim.key_get_frame(i);
        double value = m_keyProperties.anim_get_double("keyframes", key);
        QPointF p = keyframeMap(key, value);
        p.setX(p.x()*scale);
        if (m_keyframeType == GeometryKeyframe)
            p.setY(rect().bottom() - rect().height() / 2);
        if ((pos - p).manhattanLength() < maxOffset) {
            setToolTip('[' + QString::number((GenTime(key, m_fps) - cropStart()).seconds(), 'f', 2)
                       + i18n("seconds") + ", " + QString::number(value, 'f', 1) + ']');
            return key;
        } else if (p.x() > pos.x() + maxOffset) {
            break;
        }
    }
    setToolTip(QString());
    return -1;
}

void AbstractClipItem::updateSelectedKeyFrame()
{
    if (m_editedKeyframe == -1)
        return;
    QPointF h(12, 12);
    QPointF p = keyframeMap(m_selectedKeyframe, m_keyProperties.anim_get_double("keyframes", m_selectedKeyframe));
    update(QRectF(p - h/2, p + h/2));
    m_selectedKeyframe = m_editedKeyframe;
    p = keyframeMap(m_selectedKeyframe, m_keyProperties.anim_get_double("keyframes", m_selectedKeyframe));
    update(QRectF(p - h/2, p + h/2));
}

int AbstractClipItem::editedKeyFramePos() const
{
    return m_editedKeyframe;
}

double AbstractClipItem::editedKeyFrameValue()
{
    return m_keyProperties.anim_get_double("keyframes", m_editedKeyframe);
}

int AbstractClipItem::selectedKeyFramePos() const
{
    return m_selectedKeyframe;
}

void AbstractClipItem::updateKeyFramePos(int frame, const double y)
{
    if (!m_keyProperties.get_anim("keyframes")->is_key(m_editedKeyframe))
        return;
    int prev = m_keyAnim.previous_key(m_editedKeyframe - 1) + 1;
    int next = m_keyAnim.next_key(m_editedKeyframe + 1) - 1;
    if (next <= 0) next = m_editedKeyframe;
    int newpos = qBound(prev, frame, next);
    double newval = keyframeUnmap(y);
    m_keyProperties.anim_set("keyframes", newval, newpos, 0, m_keyAnim.keyframe_type(m_editedKeyframe));
    if (m_editedKeyframe != newpos)
        m_keyAnim.remove(m_editedKeyframe);
    m_editedKeyframe = newpos;
    update();
}

int AbstractClipItem::keyFrameNumber()
{
    return m_keyAnim.key_count();
}

int AbstractClipItem::checkForSingleKeyframe()
{
    // Check if we have only one keyframe
    int start = cropStart().frames(m_fps);
    if (m_keyAnim.key_count() == 1 && m_keyAnim.is_key(start)) {
        double value = m_keyProperties.anim_get_double("keyframes", start);
        // Add keyframe at end of clip to allow inserting a new keframe in between
        m_keyProperties.anim_set("keyframes", value, cropDuration().frames(m_fps));
        return value;
    }
    return -1;
}

double AbstractClipItem::addKeyFrame(const GenTime &pos, const double y)
{
    double newval = keyframeUnmap(y);
    qDebug()<<"NEW VAL; "<<newval<<" / "<<y;
    /*m_selectedKeyframe = pos.frames(m_fps);
    m_keyProperties.anim_set("keyframes", newval, m_selectedKeyframe, 0, m_keyAnim.keyframe_type(m_selectedKeyframe));
    update();*/
    return newval;
}

bool AbstractClipItem::hasKeyFrames()
{
    return m_keyAnim.key_count() > 0;
}

CustomTrackScene* AbstractClipItem::projectScene()
{
    if (scene())
        return static_cast <CustomTrackScene*>(scene());
    return NULL;
}

void AbstractClipItem::setItemLocked(bool locked)
{
    if (locked)
        setSelected(false);

    setFlag(QGraphicsItem::ItemIsMovable, !locked);
    setFlag(QGraphicsItem::ItemIsSelectable, !locked);
}

bool AbstractClipItem::isItemLocked() const
{
    return !(flags() & (QGraphicsItem::ItemIsSelectable));
}

// virtual
void AbstractClipItem::mousePressEvent(QGraphicsSceneMouseEvent * event)
{
    if (event->modifiers() & Qt::ShiftModifier) {
        // User want to do a rectangle selection, so ignore the event to pass it to the view
        event->ignore();
    } else {
        QGraphicsItem::mousePressEvent(event);
    }
}

// virtual
void AbstractClipItem::mouseReleaseEvent(QGraphicsSceneMouseEvent * event)
{
    if (event->modifiers() & Qt::ControlModifier) {
        // User want to do a rectangle selection, so ignore the event to pass it to the view
        event->ignore();
    } else {
        QGraphicsItem::mouseReleaseEvent(event);
    }
}

void AbstractClipItem::mouseMoveEvent(QGraphicsSceneMouseEvent * event)
{
    if (event->buttons() !=  Qt::LeftButton || event->modifiers() & Qt::ControlModifier) {
        // User want to do a rectangle selection, so ignore the event to pass it to the view
        event->ignore();
    } else {
        QGraphicsItem::mouseMoveEvent(event);
    }
}

int AbstractClipItem::itemHeight()
{
    return 0;
}

int AbstractClipItem::itemOffset()
{
    return 0;
}

void AbstractClipItem::setMainSelectedClip(bool selected)
{
    if (selected == m_isMainSelectedClip) return;
    m_isMainSelectedClip = selected;
    update();
}

bool AbstractClipItem::isMainSelectedClip()
{
    return m_isMainSelectedClip;
}

int AbstractClipItem::trackForPos(int position)
{
    int track = 1;
    if (!scene() || scene()->views().isEmpty()) return track;
    CustomTrackView *view = static_cast<CustomTrackView*>(scene()->views()[0]);
    if (view) {
	track = view->getTrackFromPos(position);
    }
    return track;
}

int AbstractClipItem::posForTrack(int track)
{
    int pos = 0;
    if (!scene() || scene()->views().isEmpty()) return pos;
    CustomTrackView *view = static_cast<CustomTrackView*>(scene()->views()[0]);
    if (view) {
	pos = view->getPositionFromTrack(track) + 1;
    }
    return pos;
}

