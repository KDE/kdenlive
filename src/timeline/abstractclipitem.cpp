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
        , m_keyframeFactor(1)
        , m_keyframeOffset(0)
        , m_keyframeDefault(0)
        , m_visibleParam(0)
        , m_fps(fps)
        , m_isMainSelectedClip(false)
{
    setFlags(QGraphicsItem::ItemIsMovable | QGraphicsItem::ItemIsSelectable);
    setFlag(QGraphicsItem::ItemSendsGeometryChanges, true);
    setFlag(QGraphicsItem::ItemUsesExtendedStyleOption, true);
    setPen(Qt::NoPen);
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
        /*if (cropDuration() > GenTime(3, m_fps)) durationDiff = GenTime(3, m_fps);
        else return;*/
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
        ////qDebug() << "//////  WARNING, DIFF IN XPOS: " << pos().x() << " == " << m_info.startPos.frames(m_fps);
        GenTime diff = m_info.startPos - GenTime(posx, m_fps);

        if (type() == AVWidget)
            m_info.cropStart += diff;

        m_info.cropDuration -= diff;
        setRect(0, 0, cropDuration().frames(m_fps) - 0.02, rect().height());
        ////qDebug()<<"// NEW START: "<<m_startPos.frames(25)<<", NW DUR: "<<m_cropDuration.frames(25);
    }

    // set crop from start to 0 (isn't relevant as this only happens for color clips, images)
    if (negCropStart)
        m_info.cropStart = GenTime();

    ////qDebug() << "-- NEW CLIP=" << startPos().frames(25) << '-' << endPos().frames(25);
    //setRect((double) m_startPos.frames(m_fps) * scale, rect().y(), (double) m_cropDuration.frames(m_fps) * scale, rect().height());

    /*    if (durationDiff < GenTime()) {
            QList <QGraphicsItem *> collisionList = collidingItems(Qt::IntersectsItemBoundingRect);
            for (int i = 0; i < collisionList.size(); ++i) {
                QGraphicsItem *item = collisionList.at(i);
                if (item->type() == type() && item->pos().x() < pos().x()) {
                    //qDebug() << "/////////  COLLISION DETECTED!!!!!!!!!";
                    GenTime diff = ((AbstractClipItem *)item)->endPos() + GenTime(1, m_fps) - m_startPos;
                    setRect(0, 0, (m_cropDuration - diff).frames(m_fps) - 0.02, rect().height());
                    setPos((m_startPos + diff).frames(m_fps), pos().y());
                    m_startPos += diff;
                    if (type() == AVWidget) m_cropStart += diff;
                    m_cropDuration = m_cropDuration - diff;
                    break;
                }
            }
        }*/
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
                ////qDebug() << "/////////  COLLISION DETECTED!!!!!!!!!";
                ////qDebug() << "/////////  CURRENT: " << startPos().frames(25) << 'x' << endPos().frames(25) << ", RECT: " << rect() << '-' << pos();
                ////qDebug() << "/////////  COLLISION: " << ((AbstractClipItem *)item)->startPos().frames(25) << 'x' << ((AbstractClipItem *)item)->endPos().frames(25) << ", RECT: " << ((AbstractClipItem *)item)->rect() << '-' << item->pos();
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

void AbstractClipItem::drawKeyFrames(QPainter *painter, const QTransform &transformation)
{
    Mlt::Animation* anim = m_animation.get_anim("keyframes");
    if (anim->key_count() < 1)
        return;
    QRectF br = rect();
    double maxw = br.width() / cropDuration().frames(m_fps);
    double maxh = br.height() / 100.0 * m_keyframeFactor;
    double start = cropStart().frames(m_fps);
    double x1, y1, x2, y2;
    bool antialiasing = painter->renderHints() & QPainter::Antialiasing;
    bool active = isSelected() || (parentItem() && parentItem()->isSelected());

    // draw keyframes
    // Special case: Geometry keyframes are just vertical lines
    if (m_keyframeType == GeometryKeyframe) {
        for(int i = 0; i < anim->key_count(); ++i) {
            int key = anim->key_get_frame(i);
            QColor color = (key == m_editedKeyframe) ? QColor(Qt::red) : QColor(Qt::blue);
            if (active)
                painter->setPen(color);
            x1 = br.x() + maxw * (key - start);
            QLineF line = transformation.map(QLineF(x1, br.top(), x1, br.height()));
            painter->drawLine(line);
            if (active) {
                const QRectF frame(line.x1() - 3, line.y1() + (line.y2() - line.y1()) / 2 - 3, 6, 6);
                painter->fillRect(frame, color);
            }
        }
        painter->setRenderHint(QPainter::Antialiasing, antialiasing);
        return;
    }

    // draw line showing default value
    if (active) {
        x1 = br.x();
        x2 = br.right();
        y1 = br.bottom() - (m_keyframeDefault - m_keyframeOffset) * maxh;
        QLineF line = transformation.map(QLineF(x1, y1, x2, y1));
        painter->setPen(QColor(168, 168, 168, 180));
        painter->drawLine(line);
        painter->setPen(QColor(108, 108, 108, 180));
        painter->drawLine(line.translated(0, 1));
        painter->setPen(QColor(Qt::white));
        painter->setRenderHint(QPainter::Antialiasing);
    }

    int key = anim->key_get_frame(0);
    x1 = br.x() + maxw * (key - start);
    y1 = br.bottom() - (m_animation.anim_get_double("keyframes", key) - m_keyframeOffset) * maxh;
    // make sure line starts at clip start
    if (x1 != br.x())
        painter->drawLine(transformation.map(QLineF(br.x(), y1, x1, y1)));
    
    for(int i = 1; i < anim->key_count(); ++i) {
        key = anim->key_get_frame(i);
        x2 = br.x() + maxw * (key - start);
        y2 = br.bottom() - (m_animation.anim_get_double("keyframes", key) - m_keyframeOffset) * maxh;
        QLineF line = transformation.map(QLineF(x1, y1, x2, y2));
        painter->drawLine(line);
        if (active)
            painter->fillRect(QRectF(line.x2() - 3, line.y2() - 3, 6, 6),
                              (key == m_editedKeyframe) ? QColor(Qt::red) : QColor(Qt::blue));
        x1 = x2;
        y1 = y2;
    }

    // make sure line ends at clip end
    if (x1 != br.right())
        painter->drawLine(transformation.map(QLineF(x1, y1, br.right(), y1)));

    painter->setRenderHint(QPainter::Antialiasing, antialiasing);
}

int AbstractClipItem::mouseOverKeyFrames(QPointF pos, double maxOffset)
{
    const QRectF br = sceneBoundingRect();
    double maxw = br.width() / cropDuration().frames(m_fps);
    double maxh = br.height() / 100.0 * m_keyframeFactor;
    Mlt::Animation *anim = m_animation.get_anim("keyframes");
    for(int i = 0; i < anim->key_count(); ++i) {
        int key = anim->key_get_frame(i);
        int value = m_animation.anim_get_double("keyframes", key);
        double x1 = br.x() + maxw * (key - cropStart().frames(m_fps));
        double y1;
        if (m_keyframeType == GeometryKeyframe)
            y1 = br.bottom() - (br.height() /2);
        else
            y1 = br.bottom() - (value - m_keyframeOffset) * maxh;
        if (qAbs(pos.x() - x1) < maxOffset && qAbs(pos.y() - y1) < maxOffset) {
            setToolTip('[' + QString::number((GenTime(key, m_fps) - cropStart()).seconds(), 'f', 2)
                       + i18n("seconds") + ", " + QString::number(value, 'f', 1) + ']');
            return key;
        } else if (x1 > pos.x()) {
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
    QRectF br = sceneBoundingRect();
    double maxw = br.width() / cropDuration().frames(m_fps);
    double maxh = br.height() / 100.0 * m_keyframeFactor;
    update(br.x() + maxw *(m_selectedKeyframe - cropStart().frames(m_fps)) - 3, br.bottom() - (m_animation.anim_get_double("keyframes", m_selectedKeyframe) - m_keyframeOffset) * maxh - 3, 12, 12);
    m_selectedKeyframe = m_editedKeyframe;
    update(br.x() + maxw *(m_selectedKeyframe - cropStart().frames(m_fps)) - 3, br.bottom() - (m_animation.anim_get_double("keyframes", m_selectedKeyframe) - m_keyframeOffset) * maxh - 3, 12, 12);
}

int AbstractClipItem::editedKeyFramePos() const
{
    return m_editedKeyframe;
}

double AbstractClipItem::editedKeyFrameValue()
{
    return m_animation.anim_get_double("keyframes", m_editedKeyframe);
}

int AbstractClipItem::selectedKeyFramePos() const
{
    return m_selectedKeyframe;
}

void AbstractClipItem::updateKeyFramePos(const GenTime &pos, const double value)
{
    Mlt::Animation *anim = m_animation.get_anim("keyframes");
    if (!anim->is_key(m_editedKeyframe))
        return;
    int newpos = qBound(anim->previous_key(m_editedKeyframe - 1) + 1,
                    (int)pos.frames(m_fps),
                    anim->next_key(m_editedKeyframe + 1) - 1);
    double newval = qBound(0.0, value, 100.0) / m_keyframeFactor + m_keyframeOffset;
    if (m_editedKeyframe != newpos)
        m_animation.get_anim("keyframes")->remove(m_editedKeyframe);
    m_animation.anim_set("keyframes", newval, newpos);
    m_editedKeyframe = newpos;
    update();
}

int AbstractClipItem::keyFrameNumber()
{
    return m_animation.get_anim("keyframes")->key_count();
}

int AbstractClipItem::checkForSingleKeyframe()
{
    // Check if we have only one keyframe
    Mlt::Animation *anim = m_animation.get_anim("keyframes");
    int start = cropStart().frames(m_fps);
    if (anim->key_count() == 1 && anim->is_key(start)) {
        double value = m_animation.anim_get_double("keyframes", start);
        // Add keyframe at end of clip to allow inserting a new keframe in between
        m_animation.anim_set("keyframes", value, (cropStart() + cropDuration()).frames(m_fps) - 1);
        return value;
    }
    return -1;
}

int AbstractClipItem::addKeyFrame(const GenTime &pos, const double value)
{
    QRectF br = sceneBoundingRect();
    double maxh = 100.0 / br.height() / m_keyframeFactor;
    //TODO: apply transformation when drawing, not in stored data
    double newval = (br.bottom() - value) * maxh + m_keyframeOffset;
    m_selectedKeyframe = pos.frames(m_fps);
    m_animation.anim_set("keyframes", newval, m_selectedKeyframe);
    update();
    return newval;
}

bool AbstractClipItem::hasKeyFrames()
{
    return m_animation.get_anim("keyframes")->key_count() > 0;
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

