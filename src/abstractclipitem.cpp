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

#include <QGraphicsScene>
#include <QGraphicsView>
#include <QScrollBar>
#include <QToolTip>

#include <KDebug>
#include <KLocale>

#include "abstractclipitem.h"
#include "customtrackscene.h"
#include "kdenlivesettings.h"

AbstractClipItem::AbstractClipItem(const ItemInfo info, const QRectF& rect, double fps): QGraphicsRectItem(rect), m_track(0), m_fps(fps), m_editedKeyframe(-1), m_selectedKeyframe(0), m_keyframeFactor(1) {
    setFlags(/*QGraphicsItem::ItemClipsToShape | */QGraphicsItem::ItemIsMovable | QGraphicsItem::ItemIsSelectable);
    setTrack(info.track);
    m_startPos = info.startPos;
    m_cropDuration = info.endPos - info.startPos;
}

ItemInfo AbstractClipItem::info() const {
    ItemInfo itemInfo;
    itemInfo.startPos = startPos();
    itemInfo.endPos = endPos();
    itemInfo.cropStart = m_cropStart;
    itemInfo.track = track();
    return itemInfo;
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

GenTime AbstractClipItem::cropDuration() const {
    return m_cropDuration;
}

void AbstractClipItem::setCropStart(GenTime pos) {
    m_cropStart = pos;
}

void AbstractClipItem::updateItem() {
    m_track = (int)(scenePos().y() / KdenliveSettings::trackheight());
    m_startPos = GenTime((int) scenePos().x(), m_fps);
}

void AbstractClipItem::updateRectGeometry() {
    setRect(0, 0, cropDuration().frames(m_fps) - 0.02, rect().height());
}

void AbstractClipItem::resizeStart(int posx, double speed) {
    GenTime durationDiff = GenTime(posx, m_fps) - m_startPos;
    if (durationDiff == GenTime()) return;
    //kDebug() << "-- RESCALE DIFF=" << durationDiff.frames(25) << ", CLIP: " << startPos().frames(25) << "-" << endPos().frames(25);

    if (type() == AVWIDGET && cropStart() + durationDiff < GenTime()) {
        durationDiff = GenTime() - cropStart();
    } else if (durationDiff >= m_cropDuration) {
        return;
        if (m_cropDuration > GenTime(3, m_fps)) durationDiff = GenTime(3, m_fps);
        else return;
    }

    m_startPos += durationDiff;
    if (type() == AVWIDGET) m_cropStart += durationDiff * speed;
    m_cropDuration = m_cropDuration - durationDiff * speed;

    setRect(0, 0, cropDuration().frames(m_fps) - 0.02, rect().height());
    setPos(m_startPos.frames(m_fps), pos().y());
    if ((int) pos().x() != posx) {
        //kDebug()<<"//////  WARNING, DIFF IN XPOS: "<<pos().x()<<" == "<<m_startPos.frames(m_fps);
        GenTime diff = GenTime((int) pos().x() - posx, m_fps);
        if (type() == AVWIDGET) m_cropStart = m_cropStart + diff;
        m_cropDuration = m_cropDuration - diff;
        setRect(0, 0, cropDuration().frames(m_fps) - 0.02, rect().height());
        //kDebug()<<"// NEW START: "<<m_startPos.frames(25)<<", NW DUR: "<<m_cropDuration.frames(25);
    }


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

void AbstractClipItem::resizeEnd(int posx, double speed, bool updateKeyFrames) {
    GenTime durationDiff = GenTime(posx, m_fps) - endPos();
    if (durationDiff == GenTime()) return;
    //kDebug() << "// DUR DIFF1:" << durationDiff.frames(25) << ", ADJUSTED: " << durationDiff.frames(25) * speed << ", SPED:" << speed;
    if (cropDuration() + durationDiff <= GenTime()) {
        durationDiff = GenTime() - (cropDuration() - GenTime(3, m_fps));
    } else if (cropStart() + cropDuration() + durationDiff >= maxDuration()) {
        //kDebug() << "// MAX OVERLOAD:" << cropDuration().frames(25) << " + " << durationDiff.frames(25) << ", MAX:" << maxDuration().frames(25);
        durationDiff = maxDuration() - cropDuration() - cropStart();
    }
    //kDebug() << "// DUR DIFF2:" << durationDiff.frames(25) << ", ADJUSTED: " << durationDiff.frames(25) * speed << ", SPED:" << speed;
    m_cropDuration += durationDiff * speed;
    setRect(0, 0, cropDuration().frames(m_fps) - 0.02, rect().height());
    if (durationDiff > GenTime()) {
        QList <QGraphicsItem *> collisionList = collidingItems(Qt::IntersectsItemBoundingRect);
        for (int i = 0; i < collisionList.size(); ++i) {
            QGraphicsItem *item = collisionList.at(i);
            if (item->type() == type() && item->pos().x() > pos().x()) {
                /*kDebug() << "/////////  COLLISION DETECTED!!!!!!!!!";
                kDebug() << "/////////  CURRENT: " << startPos().frames(25) << "x" << endPos().frames(25) << ", RECT: " << rect() << "-" << pos();
                kDebug() << "/////////  COLLISION: " << ((AbstractClipItem *)item)->startPos().frames(25) << "x" << ((AbstractClipItem *)item)->endPos().frames(25) << ", RECT: " << ((AbstractClipItem *)item)->rect() << "-" << item->pos();*/
                GenTime diff = ((AbstractClipItem *)item)->startPos() - GenTime(1, m_fps) - startPos();
                m_cropDuration = diff;
                setRect(0, 0, m_cropDuration.frames(m_fps) - 0.02, rect().height());
                break;
            }
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

void AbstractClipItem::setMaxDuration(const GenTime &max) {
    m_maxDuration = max;
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
        x2 = br.right();
        y1 = br.bottom() - m_keyframeDefault * maxh;
        QLineF l(x1, y1, x2, y1);
        QLineF l2 = painter->matrix().map(l);
        painter->setPen(QColor(168, 168, 168, 180));
        painter->drawLine(l2);
        l2.translate(0, 1);
        painter->setPen(QColor(108, 108, 108, 180));
        painter->drawLine(l2);
        painter->setPen(QColor(Qt::white));
    }

    // draw keyframes
    QMap<int, double>::const_iterator i = m_keyframes.constBegin();
    QColor color(Qt::blue);
    x1 = br.x() + maxw * (i.key() - m_cropStart.frames(m_fps));
    y1 = br.bottom() - i.value() * maxh;
    QLineF l2;
    while (i != m_keyframes.constEnd()) {
        if (i.key() == m_selectedKeyframe) color = QColor(Qt::red);
        else color = QColor(Qt::blue);
        ++i;
        if (i == m_keyframes.constEnd()) break;
        x2 = br.x() + maxw * (i.key() - m_cropStart.frames(m_fps));
        y2 = br.bottom() - i.value() * maxh;
        QLineF l(x1, y1, x2, y2);
        l2 = painter->matrix().map(l);
        painter->drawLine(l2);
        if (isSelected()) {
            painter->fillRect(l2.x1() - 3, l2.y1() - 3, 6, 6, QBrush(color));
        }
        x1 = x2;
        y1 = y2;
    }
    if (isSelected()) painter->fillRect(l2.x2() - 3, l2.y2() - 3, 6, 6, QBrush(color));
}

int AbstractClipItem::mouseOverKeyFrames(QPointF pos) {
    QRectF br = sceneBoundingRect();
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
                setToolTip("[" + QString::number((GenTime(i.key(), m_fps) - m_cropStart).seconds(), 'f', 2) + i18n("seconds") + ", " + QString::number(i.value(), 'f', 1) + "%]");
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
    QRectF br = sceneBoundingRect();
    double maxw = br.width() / m_cropDuration.frames(m_fps);
    double maxh = br.height() / 100.0 * m_keyframeFactor;
    update(br.x() + maxw * (m_selectedKeyframe - m_cropStart.frames(m_fps)) - 3, br.bottom() - m_keyframes[m_selectedKeyframe] * maxh - 3, 12, 12);
    m_selectedKeyframe = m_editedKeyframe;
    update(br.x() + maxw * (m_selectedKeyframe - m_cropStart.frames(m_fps)) - 3, br.bottom() - m_keyframes[m_selectedKeyframe] * maxh - 3, 12, 12);
}

int AbstractClipItem::selectedKeyFramePos() const {
    return m_editedKeyframe;
}

double AbstractClipItem::selectedKeyFrameValue() const {
    return m_keyframes[m_editedKeyframe];
}

void AbstractClipItem::updateKeyFramePos(const GenTime pos, const double value) {
    if (!m_keyframes.contains(m_selectedKeyframe)) return;
    int newpos = (int) pos.frames(m_fps);
    int start = m_cropStart.frames(m_fps);
    int end = (m_cropStart + m_cropDuration).frames(m_fps);
    newpos = qMax(newpos, start);
    newpos = qMin(newpos, end);
    if (value < -50 && m_selectedKeyframe != start && m_selectedKeyframe != end) {
        // remove kexframe if it is dragged outside
        m_keyframes.remove(m_selectedKeyframe);
        m_selectedKeyframe = -1;
        update();
        return;
    }
    if (value > 150 && m_selectedKeyframe != start && m_selectedKeyframe != end) {
        // remove kexframe if it is dragged outside
        m_keyframes.remove(m_selectedKeyframe);
        m_selectedKeyframe = -1;
        update();
        return;
    }
    double newval = qMax(value, 0.0);
    newval = qMin(newval, 100.0);
    newval = newval / m_keyframeFactor;
    if (m_selectedKeyframe != newpos) m_keyframes.remove(m_selectedKeyframe);
    m_keyframes[newpos] = newval;
    m_selectedKeyframe = newpos;
    update();
}

double AbstractClipItem::keyFrameFactor() const {
    return m_keyframeFactor;
}

void AbstractClipItem::addKeyFrame(const GenTime pos, const double value) {
    QRectF br = sceneBoundingRect();
    double maxh = 100.0 / br.height() / m_keyframeFactor;
    double newval = (br.bottom() - value) * maxh;
    kDebug() << "Rect: " << br << "/ SCENE: " << sceneBoundingRect() << ", VALUE: " << value << ", MAX: " << maxh << ", NEWVAL: " << newval;
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

CustomTrackScene* AbstractClipItem::projectScene() {
    if (scene()) return static_cast <CustomTrackScene*>(scene());
    return NULL;
}
