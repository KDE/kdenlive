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

#include <KDebug>
#include <KLocale>
#include <KGlobalSettings>

#include <QPainter>
#include <QToolTip>
#include <QGraphicsSceneMouseEvent>

AbstractClipItem::AbstractClipItem(const ItemInfo &info, const QRectF& rect, double fps) :
        QObject(),
        QGraphicsRectItem(rect),
        m_info(info),
        m_editedKeyframe(-1),
        m_selectedKeyframe(0),
        m_keyframeFactor(1),
        m_keyframeOffset(0),
        m_fps(fps)
{
    setFlags(QGraphicsItem::ItemIsMovable | QGraphicsItem::ItemIsSelectable);
#if QT_VERSION >= 0x040600
    setFlag(QGraphicsItem::ItemSendsGeometryChanges, true);
    setFlag(QGraphicsItem::ItemUsesExtendedStyleOption, true);
#endif
}

AbstractClipItem::~AbstractClipItem()
{
}

void AbstractClipItem::closeAnimation()
{
#if QT_VERSION >= 0x040600
    if (!isEnabled()) return;
    setEnabled(false);
    if (!(KGlobalSettings::graphicEffectsLevel() & KGlobalSettings::SimpleAnimationEffects)) {
        // animation disabled
        deleteLater();
        return;
    }
    QPropertyAnimation *closeAnimation = new QPropertyAnimation(this, "rect");
    connect(closeAnimation, SIGNAL(finished()), this, SLOT(deleteLater()));
    closeAnimation->setDuration(200);
    QRectF r = rect();
    QRectF r2 = r;
    r2.setLeft(r.left() + r.width() / 2);
    r2.setTop(r.top() + r.height() / 2);
    r2.setWidth(1);
    r2.setHeight(1);
    closeAnimation->setStartValue(r);
    closeAnimation->setEndValue(r2);
    closeAnimation->setEasingCurve(QEasingCurve::InQuad);
    closeAnimation->start(QAbstractAnimation::DeleteWhenStopped);
#endif
}

ItemInfo AbstractClipItem::info() const
{
    ItemInfo info = m_info;
    info.cropStart = cropStart();
    info.endPos = endPos();
    return info;
}

int AbstractClipItem::defaultZValue() const
{
    return 2;
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

void AbstractClipItem::setCropStart(GenTime pos)
{
    m_info.cropStart = pos;
}

void AbstractClipItem::updateItem()
{
    m_info.track = (int)(scenePos().y() / KdenliveSettings::trackheight());
    m_info.startPos = GenTime((int) scenePos().x(), m_fps);
}

void AbstractClipItem::updateRectGeometry()
{
    setRect(0, 0, cropDuration().frames(m_fps) - 0.02, rect().height());
}

void AbstractClipItem::resizeStart(int posx, bool hasSizeLimit)
{
    GenTime durationDiff = GenTime(posx, m_fps) - m_info.startPos;
    if (durationDiff == GenTime()) return;

    if (type() == AVWIDGET && hasSizeLimit && (cropStart() + durationDiff < GenTime())) {
        durationDiff = GenTime() - cropStart();
    } else if (durationDiff >= cropDuration()) {
        return;
        /*if (cropDuration() > GenTime(3, m_fps)) durationDiff = GenTime(3, m_fps);
        else return;*/
    }
    m_info.startPos += durationDiff;

    // set to true if crop from start is negative (possible for color clips, images as they have no size limit)
    bool negCropStart = false;
    if (type() == AVWIDGET) {
        m_info.cropStart += durationDiff;
        if (m_info.cropStart < GenTime())
            negCropStart = true;
    }

    m_info.cropDuration -= durationDiff;
    setRect(0, 0, cropDuration().frames(m_fps) - 0.02, rect().height());
    moveBy(durationDiff.frames(m_fps), 0);

    if (m_info.startPos != GenTime(posx, m_fps)) {
        //kDebug() << "//////  WARNING, DIFF IN XPOS: " << pos().x() << " == " << m_info.startPos.frames(m_fps);
        GenTime diff = m_info.startPos - GenTime(posx, m_fps);

        if (type() == AVWIDGET)
            m_info.cropStart += diff;

        m_info.cropDuration -= diff;
        setRect(0, 0, cropDuration().frames(m_fps) - 0.02, rect().height());
        //kDebug()<<"// NEW START: "<<m_startPos.frames(25)<<", NW DUR: "<<m_cropDuration.frames(25);
    }

    // set crop from start to 0 (isn't relevant as this only happens for color clips, images)
    if (negCropStart)
        m_info.cropStart = GenTime();

    //kDebug() << "-- NEW CLIP=" << startPos().frames(25) << "-" << endPos().frames(25);
    //setRect((double) m_startPos.frames(m_fps) * scale, rect().y(), (double) m_cropDuration.frames(m_fps) * scale, rect().height());

    /*    if (durationDiff < GenTime()) {
            QList <QGraphicsItem *> collisionList = collidingItems(Qt::IntersectsItemBoundingRect);
            for (int i = 0; i < collisionList.size(); ++i) {
                QGraphicsItem *item = collisionList.at(i);
                if (item->type() == type() && item->pos().x() < pos().x()) {
                    kDebug() << "/////////  COLLISION DETECTED!!!!!!!!!";
                    GenTime diff = ((AbstractClipItem *)item)->endPos() + GenTime(1, m_fps) - m_startPos;
                    setRect(0, 0, (m_cropDuration - diff).frames(m_fps) - 0.02, rect().height());
                    setPos((m_startPos + diff).frames(m_fps), pos().y());
                    m_startPos += diff;
                    if (type() == AVWIDGET) m_cropStart += diff;
                    m_cropDuration = m_cropDuration - diff;
                    break;
                }
            }
        }*/
}

void AbstractClipItem::resizeEnd(int posx)
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
                //kDebug() << "/////////  COLLISION DETECTED!!!!!!!!!";
                //kDebug() << "/////////  CURRENT: " << startPos().frames(25) << "x" << endPos().frames(25) << ", RECT: " << rect() << "-" << pos();
                //kDebug() << "/////////  COLLISION: " << ((AbstractClipItem *)item)->startPos().frames(25) << "x" << ((AbstractClipItem *)item)->endPos().frames(25) << ", RECT: " << ((AbstractClipItem *)item)->rect() << "-" << item->pos();
                GenTime diff = ((AbstractClipItem *)item)->startPos() - startPos();
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

void AbstractClipItem::setTrack(int track)
{
    m_info.track = track;
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

void AbstractClipItem::drawKeyFrames(QPainter *painter, bool limitedKeyFrames)
{
    if (m_keyframes.count() < 1)
        return;
    QRectF br = rect();
    double maxw = br.width() / cropDuration().frames(m_fps);
    double maxh = br.height() / 100.0 * m_keyframeFactor;
    double start = cropStart().frames(m_fps);
    double x1, y1, x2, y2;
    bool antialiasing = painter->renderHints() & QPainter::Antialiasing;

    // draw line showing default value
    bool active = isSelected() || (parentItem() && parentItem()->isSelected());
    if (active) {
        x1 = br.x();
        x2 = br.right();
        if (limitedKeyFrames) {
            QMap<int, int>::const_iterator end = m_keyframes.constEnd();
            end--;
            x2 = x1 + maxw * (end.key() - start);
            x1 += maxw * (m_keyframes.constBegin().key() - start);
        }
        y1 = br.bottom() - (m_keyframeDefault - m_keyframeOffset) * maxh;
        QLineF l(x1, y1, x2, y1);
        QLineF l2 = painter->worldTransform().map(l);
        painter->setPen(QColor(168, 168, 168, 180));
        painter->drawLine(l2);
        painter->setPen(QColor(108, 108, 108, 180));
        painter->drawLine(l2.translated(0, 1));
        painter->setPen(QColor(Qt::white));
        painter->setRenderHint(QPainter::Antialiasing);
    }

    // draw keyframes
    QMap<int, int>::const_iterator i = m_keyframes.constBegin();
    QColor color(Qt::blue);
    QLineF l2;
    x1 = br.x() + maxw * (i.key() - start);
    y1 = br.bottom() - (i.value() - m_keyframeOffset) * maxh;



    // make sure line begins with clip beginning
    if (!limitedKeyFrames && i.key() != start) {
        QLineF l(br.x(), y1, x1, y1);
        l2 = painter->worldTransform().map(l);
        painter->drawLine(l2);
    }
    while (i != m_keyframes.constEnd()) {
        if (i.key() == m_editedKeyframe)
            color = QColor(Qt::red);
        else
            color = QColor(Qt::blue);
        ++i;
        if (i == m_keyframes.constEnd() && m_keyframes.count() != 1)
            break;

        if (m_keyframes.count() == 1) {
            x2 = br.right();
            y2 = y1;
        } else {
            x2 = br.x() + maxw * (i.key() - start);
            y2 = br.bottom() - (i.value() - m_keyframeOffset) * maxh;
        }
        QLineF l(x1, y1, x2, y2);
        l2 = painter->worldTransform().map(l);
        painter->drawLine(l2);
        if (active) {
            const QRectF frame(l2.x1() - 3, l2.y1() - 3, 6, 6);
            painter->fillRect(frame, color);
        }
        x1 = x2;
        y1 = y2;
    }

    // make sure line ends at clip end
    if (!limitedKeyFrames && x1 != br.right()) {
        QLineF l(x1, y1, br.right(), y1);
        painter->drawLine(painter->worldTransform().map(l));
    }

    if (active && m_keyframes.count() > 1) {
        const QRectF frame(l2.x2() - 3, l2.y2() - 3, 6, 6);
        painter->fillRect(frame, color);
    }

    painter->setRenderHint(QPainter::Antialiasing, antialiasing);
}

int AbstractClipItem::mouseOverKeyFrames(QPointF pos, double maxOffset)
{
    const QRectF br = sceneBoundingRect();
    double maxw = br.width() / cropDuration().frames(m_fps);
    double maxh = br.height() / 100.0 * m_keyframeFactor;
    if (m_keyframes.count() > 0) {
        QMap<int, int>::const_iterator i = m_keyframes.constBegin();
        double x1;
        double y1;
        while (i != m_keyframes.constEnd()) {
            x1 = br.x() + maxw * (i.key() - cropStart().frames(m_fps));
            y1 = br.bottom() - (i.value() - m_keyframeOffset) * maxh;
            if (qAbs(pos.x() - x1) < maxOffset && qAbs(pos.y() - y1) < 10) {
                setToolTip('[' + QString::number((GenTime(i.key(), m_fps) - cropStart()).seconds(), 'f', 2) + i18n("seconds") + ", " + QString::number(i.value(), 'f', 1) + ']');
                return i.key();
            } else if (x1 > pos.x()) {
                break;
            }
            ++i;
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
    update(br.x() + maxw *(m_selectedKeyframe - cropStart().frames(m_fps)) - 3, br.bottom() - (m_keyframes.value(m_selectedKeyframe) - m_keyframeOffset) * maxh - 3, 12, 12);
    m_selectedKeyframe = m_editedKeyframe;
    update(br.x() + maxw *(m_selectedKeyframe - cropStart().frames(m_fps)) - 3, br.bottom() - (m_keyframes.value(m_selectedKeyframe) - m_keyframeOffset) * maxh - 3, 12, 12);
}

int AbstractClipItem::editedKeyFramePos() const
{
    return m_editedKeyframe;
}

double AbstractClipItem::editedKeyFrameValue() const
{
    return m_keyframes.value(m_editedKeyframe);
}

int AbstractClipItem::selectedKeyFramePos() const
{
    return m_selectedKeyframe;
}

double AbstractClipItem::selectedKeyFrameValue() const
{
    return m_keyframes.value(m_selectedKeyframe);
}

void AbstractClipItem::updateKeyFramePos(const GenTime &pos, const double value)
{
    if (!m_keyframes.contains(m_editedKeyframe))
        return;
    int newpos = (int) pos.frames(m_fps);
    int min = (int) cropStart().frames(m_fps) - 1;
    int max = (int)(cropStart() + cropDuration()).frames(m_fps);
    QMap<int, int>::const_iterator i = m_keyframes.constBegin();
    while (i.key() < m_editedKeyframe) {
        min = qMax(i.key(), min);
        ++i;
    }
    i = m_keyframes.constEnd() - 1;
    while (i.key() > m_editedKeyframe) {
        max = qMin(i.key(), max);
        --i;
    }
    if (newpos <= min)
        newpos = min + 1;
    if (newpos >= max)
        newpos = max - 1;

    double newval = qMax(value, 0.0);
    newval = qMin(newval, 100.0);
    newval = newval / m_keyframeFactor + m_keyframeOffset;
    if (m_editedKeyframe != newpos)
        m_keyframes.remove(m_editedKeyframe);
    m_keyframes[newpos] = (int) newval;
    m_editedKeyframe = newpos;
    update();
}

double AbstractClipItem::keyFrameFactor() const
{
    return m_keyframeFactor;
}

int AbstractClipItem::keyFrameNumber() const
{
    return m_keyframes.count();
}

int AbstractClipItem::addKeyFrame(const GenTime &pos, const double value)
{
    QRectF br = sceneBoundingRect();
    double maxh = 100.0 / br.height() / m_keyframeFactor;
    int newval = (br.bottom() - value) * maxh + m_keyframeOffset;
    //kDebug() << "Rect: " << br << "/ SCENE: " << sceneBoundingRect() << ", VALUE: " << value << ", MAX: " << maxh << ", NEWVAL: " << newval;
    int newpos = (int) pos.frames(m_fps) ;
    m_keyframes[newpos] = newval;
    m_selectedKeyframe = newpos;
    update();
    return newval;
}

bool AbstractClipItem::hasKeyFrames() const
{
    return !m_keyframes.isEmpty();
}

/*QRect AbstractClipItem::visibleRect() {
    QRect rectInView;
    if (scene()->views().size() > 0) {
        rectInView = scene()->views()[0]->viewport()->rect();
        rectInView.moveTo(scene()->views()[0]->horizontalScrollBar()->value(), scene()->views()[0]->verticalScrollBar()->value());
        rectInView.adjust(-10, -10, 10, 10);//make view rect 10 pixel greater on each site, or repaint after scroll event
        //kDebug() << scene()->views()[0]->viewport()->rect() << " " <<  scene()->views()[0]->horizontalScrollBar()->value();
    }
    return rectInView;
}*/

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

int AbstractClipItem::itemHeight()
{
    return 0;
}

int AbstractClipItem::itemOffset()
{
    return 0;
}


