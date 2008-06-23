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

#include <KDebug>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QScrollBar>
#include <QToolTip>

#include "abstractclipitem.h"

AbstractClipItem::AbstractClipItem(const ItemInfo info, const QRectF& rect, double fps): QGraphicsRectItem(rect), m_track(0), m_fps(fps), m_editedKeyframe(-1), m_selectedKeyframe(0) {
    setFlags(QGraphicsItem::ItemClipsToShape | QGraphicsItem::ItemIsMovable | QGraphicsItem::ItemIsSelectable);
    setTrack(info.track);
    m_startPos = info.startPos;
    m_cropDuration = info.endPos - info.startPos;
}

void AbstractClipItem::moveTo(int x, double scale, int offset, int newTrack) {
    double origX = rect().x();
    double origY = rect().y();
    bool success = true;
    if (x < 0) return;
    setRect(x * scale, origY + offset, rect().width(), rect().height());
    QList <QGraphicsItem *> collisionList = collidingItems(Qt::IntersectsItemBoundingRect);
    if (collisionList.size() == 0) m_track = newTrack;
    for (int i = 0; i < collisionList.size(); ++i) {
        QGraphicsItem *item = collisionList.at(i);
        if (item->type() == type()) {
            if (offset == 0) {
                QRectF other = ((QGraphicsRectItem *)item)->rect();
                if (x < m_startPos.frames(m_fps)) {
                    kDebug() << "COLLISION, MOVING TO------";
                    m_startPos = ((AbstractClipItem *)item)->endPos();
                    origX = m_startPos.frames(m_fps) * scale;
                } else if (x > m_startPos.frames(m_fps)) {
                    //kDebug() << "COLLISION, MOVING TO+++: "<<x<<", CLIP CURR POS: "<<m_startPos.frames(m_fps)<<", COLLIDING START: "<<((AbstractClipItem *)item)->startPos().frames(m_fps);
                    m_startPos = ((AbstractClipItem *)item)->startPos() - m_cropDuration;
                    origX = m_startPos.frames(m_fps) * scale;
                }
            }
            setRect(origX, origY, rect().width(), rect().height());
            offset = 0;
            origX = rect().x();
            success = false;
            break;
        }
    }
    if (success) {
        m_track = newTrack;
        m_startPos = GenTime(x, m_fps);
    }
    /*    QList <QGraphicsItem *> childrenList = QGraphicsItem::children();
        for (int i = 0; i < childrenList.size(); ++i) {
          childrenList.at(i)->moveBy(rect().x() - origX , offset);
        }*/
}

GenTime AbstractClipItem::endPos() const {
    return m_startPos + m_cropDuration;
}

int AbstractClipItem::track() const {
    return m_track;
}

GenTime AbstractClipItem::cropStart() const {
    return m_cropStart;
}

void AbstractClipItem::setCropStart(GenTime pos) {
    m_cropStart = pos;
}

void AbstractClipItem::resizeStart(int posx, double scale) {
    GenTime durationDiff = GenTime(posx, m_fps) - m_startPos;
    if (durationDiff == GenTime()) return;
    //kDebug() << "-- RESCALE: CROP=" << m_cropStart << ", DIFF = " << durationDiff;

    if (type() == AVWIDGET && m_cropStart + durationDiff < GenTime()) {
        durationDiff = GenTime() - m_cropStart;
    } else if (durationDiff >= m_cropDuration) {
        durationDiff = m_cropDuration - GenTime(3, m_fps);
    }

    m_startPos += durationDiff;
    if (type() == AVWIDGET) m_cropStart += durationDiff;
    m_cropDuration = m_cropDuration - durationDiff;
    setRect(m_startPos.frames(m_fps) * scale, rect().y(), m_cropDuration.frames(m_fps) * scale, rect().height());
    QList <QGraphicsItem *> collisionList = collidingItems(Qt::IntersectsItemBoundingRect);
    for (int i = 0; i < collisionList.size(); ++i) {
        QGraphicsItem *item = collisionList.at(i);
        if (item->type() == type()) {
            GenTime diff = ((AbstractClipItem *)item)->endPos() + GenTime(1, m_fps) - m_startPos;
            setRect((m_startPos + diff).frames(m_fps) * scale, rect().y(), (m_cropDuration - diff).frames(m_fps) * scale, rect().height());
            m_startPos += diff;
            if (type() == AVWIDGET) m_cropStart += diff;
            m_cropDuration = m_cropDuration - diff;
            break;
        }
    }
}

void AbstractClipItem::resizeEnd(int posx, double scale) {
    GenTime durationDiff = GenTime(posx, m_fps) - endPos();
    if (durationDiff == GenTime()) return;
    //kDebug() << "-- RESCALE: CROP=" << m_cropStart << ", DIFF = " << durationDiff;
    if (m_cropDuration + durationDiff <= GenTime()) {
        durationDiff = GenTime() - (m_cropDuration - GenTime(3, m_fps));
    } else if (m_cropStart + m_cropDuration + durationDiff >= m_maxDuration) {
        durationDiff = m_maxDuration - m_cropDuration - m_cropStart;
    }
    m_cropDuration += durationDiff;
    setRect(m_startPos.frames(m_fps) * scale, rect().y(), m_cropDuration.frames(m_fps) * scale, rect().height());
    QList <QGraphicsItem *> collisionList = collidingItems(Qt::IntersectsItemBoundingRect);
    for (int i = 0; i < collisionList.size(); ++i) {
        QGraphicsItem *item = collisionList.at(i);
        if (item->type() == type()) {
            GenTime diff = ((AbstractClipItem *)item)->startPos() - GenTime(1, m_fps) - startPos();
            m_cropDuration = diff;
            setRect(m_startPos.frames(m_fps) * scale, rect().y(), (m_cropDuration.frames(m_fps)) * scale, rect().height());
            break;
        }
    }
}

GenTime AbstractClipItem::duration() const {
    return m_cropDuration;
}

GenTime AbstractClipItem::startPos() const {
    return m_startPos;
}

void AbstractClipItem::setTrack(int track) {
    m_track = track;
}

double AbstractClipItem::fps() const {
    return m_fps;
}

GenTime AbstractClipItem::maxDuration() const {
    return m_maxDuration;
}

QPainterPath AbstractClipItem::upperRectPart(QRectF br) {
    QPainterPath roundRectPathUpper;
    double roundingY = 20;
    double roundingX = 20;
    double offset = 1;

    while (roundingX > br.width() / 2) {
        roundingX = roundingX / 2;
        roundingY = roundingY / 2;
    }
    int br_endx = (int)(br.x() + br .width() - offset);
    int br_startx = (int)(br.x() + offset);
    int br_starty = (int)(br.y());
    int br_halfy = (int)(br.y() + br.height() / 2 - offset);
    int br_endy = (int)(br.y() + br.height());

    roundRectPathUpper.moveTo(br_endx  , br_halfy);
    roundRectPathUpper.arcTo(br_endx - roundingX , br_starty , roundingX, roundingY, 0.0, 90.0);
    roundRectPathUpper.lineTo(br_startx + roundingX , br_starty);
    roundRectPathUpper.arcTo(br_startx , br_starty , roundingX, roundingY, 90.0, 90.0);
    roundRectPathUpper.lineTo(br_startx , br_halfy);

    return roundRectPathUpper;
}

QPainterPath AbstractClipItem::lowerRectPart(QRectF br) {
    QPainterPath roundRectPathLower;
    double roundingY = 20;
    double roundingX = 20;
    double offset = 1;

    int br_endx = (int)(br.x() + br .width() - offset);
    int br_startx = (int)(br.x() + offset);
    int br_starty = (int)(br.y());
    int br_halfy = (int)(br.y() + br.height() / 2 - offset);
    int br_endy = (int)(br.y() + br.height() - 1);

    while (roundingX > br.width() / 2) {
        roundingX = roundingX / 2;
        roundingY = roundingY / 2;
    }
    roundRectPathLower.moveTo(br_startx, br_halfy);
    roundRectPathLower.arcTo(br_startx , br_endy - roundingY , roundingX, roundingY, 180.0, 90.0);
    roundRectPathLower.lineTo(br_endx - roundingX  , br_endy);
    roundRectPathLower.arcTo(br_endx - roundingX , br_endy - roundingY, roundingX, roundingY, 270.0, 90.0);
    roundRectPathLower.lineTo(br_endx  , br_halfy);
    return roundRectPathLower;
}

void AbstractClipItem::drawKeyFrames(QPainter *painter, QRectF exposedRect) {
    if (m_keyframes.count() < 2) return;
    QRectF br = rect();
    double maxw = br.width() / m_cropDuration.frames(m_fps);
    double maxh = br.height() / 100.0 * m_keyframeFactor;
    double x1;
    double y1;
    double x2;
    double y2;

    // draw line showing default value
    if (isSelected()) {
        x1 = br.x();
        x1 = br.right();
        y1 = br.bottom() - m_keyframeDefault * maxh;
        QLineF l(x1, y1, x2, y1);
        painter->setPen(QColor(168, 168, 168, 180));
        painter->drawLine(l);
        l.translate(0, 1);
        painter->setPen(QColor(108, 108, 108, 180));
        painter->drawLine(l);
        painter->setPen(QColor(Qt::white));
    }

    // draw keyframes
    QMap<int, double>::const_iterator i = m_keyframes.constBegin();
    QColor color(Qt::blue);
    x1 = br.x() + maxw * (i.key() - m_cropStart.frames(m_fps));
    y1 = br.bottom() - i.value() * maxh;
    while (i != m_keyframes.constEnd()) {
        if (i.key() == m_selectedKeyframe) color = QColor(Qt::red);
        else color = QColor(Qt::blue);
        ++i;
        if (i == m_keyframes.constEnd()) break;
        x2 = br.x() + maxw * (i.key() - m_cropStart.frames(m_fps));
        y2 = br.bottom() - i.value() * maxh;
        QLineF l(x1, y1, x2, y2);
        painter->drawLine(l);
        if (isSelected()) {
            painter->fillRect(x1 - 3, y1 - 3, 6, 6, QBrush(color));
        }
        x1 = x2;
        y1 = y2;
    }
    if (isSelected()) painter->fillRect(x1 - 3, y1 - 3, 6, 6, QBrush(color));
}

int AbstractClipItem::mouseOverKeyFrames(QPointF pos) {
    QRectF br = rect();
    double maxw = br.width() / m_cropDuration.frames(m_fps);
    double maxh = br.height() / 100.0 * m_keyframeFactor;
    if (m_keyframes.count() > 1) {
        QMap<int, double>::const_iterator i = m_keyframes.constBegin();
        double x1;
        double y1;
        while (i != m_keyframes.constEnd()) {
            x1 = br.x() + maxw * (i.key() - m_cropStart.frames(m_fps));
            y1 = br.bottom() - i.value() * maxh;
            if (qAbs(pos.x() - x1) < 6 && qAbs(pos.y() - y1) < 6) {
                setToolTip("[" + QString::number(i.key()) + " frames, " + QString::number(i.value(), 'f', 1) + "%]");
                return i.key();
            } else if (x1 > pos.x()) break;
            ++i;
        }
    }
    setToolTip(QString());
    return -1;
}

void AbstractClipItem::updateSelectedKeyFrame() {
    if (m_editedKeyframe == -1) return;
    QRectF br = rect();
    double maxw = br.width() / m_cropDuration.frames(m_fps);
    double maxh = br.height() / 100.0 * m_keyframeFactor;
    update(br.x() + maxw * (m_selectedKeyframe - m_cropStart.frames(m_fps)) - 3, br.bottom() - m_keyframes[m_selectedKeyframe] * maxh - 3, 12, 12);
    m_selectedKeyframe = m_editedKeyframe;
    update(br.x() + maxw * (m_selectedKeyframe - m_cropStart.frames(m_fps)) - 3, br.bottom() - m_keyframes[m_selectedKeyframe] * maxh - 3, 12, 12);
}

void AbstractClipItem::updateKeyFramePos(const GenTime pos, const int value) {
    if (!m_keyframes.contains(m_selectedKeyframe)) return;
    QRectF br = rect();
    double maxh = 100.0 / br.height();
    double newval = (br.bottom() - value) * maxh;
    int newpos = (int) pos.frames(m_fps);
    int start = m_cropStart.frames(m_fps);
    int end = (m_cropStart + m_cropDuration).frames(m_fps);
    newpos = qMax(newpos, start);
    newpos = qMin(newpos, end);
    if (newval < -50 && m_selectedKeyframe != start && m_selectedKeyframe != end) {
        // remove kexframe if it is dragged outside
        m_keyframes.remove(m_selectedKeyframe);
        m_selectedKeyframe = -1;
        update();
        return;
    }
    if (newval > 150 && m_selectedKeyframe != start && m_selectedKeyframe != end) {
        // remove kexframe if it is dragged outside
        m_keyframes.remove(m_selectedKeyframe);
        m_selectedKeyframe = -1;
        update();
        return;
    }
    newval = qMax(newval, 0.0);
    newval = qMin(newval, 100.0);
    newval = newval / m_keyframeFactor;
    if (m_selectedKeyframe != newpos) m_keyframes.remove(m_selectedKeyframe);
    m_keyframes[newpos] = newval;
    m_selectedKeyframe = newpos;
    update();
}

void AbstractClipItem::addKeyFrame(const GenTime pos, const int value) {
    QRectF br = rect();
    double maxh = 100.0 / br.height() / m_keyframeFactor;
    double newval = (br.bottom() - value) * maxh;
    int newpos = (int) pos.frames(m_fps) ;
    m_keyframes[newpos] = newval;
    m_selectedKeyframe = newpos;
    update();
}

bool AbstractClipItem::hasKeyFrames() const {
    return !m_keyframes.isEmpty();
}

QRect AbstractClipItem::visibleRect() {
    QRect rectInView;
    if (scene()->views().size() > 0) {
        rectInView = scene()->views()[0]->viewport()->rect();
        rectInView.moveTo(scene()->views()[0]->horizontalScrollBar()->value(), scene()->views()[0]->verticalScrollBar()->value());
        rectInView.adjust(-10, -10, 10, 10);//make view rect 10 pixel greater on each site, or repaint after scroll event
        //kDebug() << scene()->views()[0]->viewport()->rect() << " " <<  scene()->views()[0]->horizontalScrollBar()->value();
    }
    return rectInView;
}
