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

#include <QPainter>
#include <QToolTip>
#include <QGraphicsSceneMouseEvent>

AbstractClipItem::AbstractClipItem(const ItemInfo info, const QRectF& rect, double fps) :
        QObject(),
        QGraphicsRectItem(rect),
        m_info(info),
        m_editedKeyframe(-1),
        m_selectedKeyframe(0),
        m_keyframeFactor(1),
        m_fps(fps)
#if QT_VERSION >= 0x040600	
	, m_closeAnimation(NULL)
#endif
{
    setFlags(QGraphicsItem::ItemIsMovable | QGraphicsItem::ItemIsSelectable);
#if QT_VERSION >= 0x040600
    setFlag(QGraphicsItem::ItemSendsGeometryChanges, true);
#endif
}

AbstractClipItem::~AbstractClipItem()
{
#if QT_VERSION >= 0x040600
    if (m_closeAnimation) delete m_closeAnimation;
#endif  
}  
 
void AbstractClipItem::closeAnimation()
{
#if QT_VERSION >= 0x040600
    if (m_closeAnimation) return;
    m_closeAnimation = new QPropertyAnimation(this, "rect");
    connect(m_closeAnimation, SIGNAL(finished()), this, SLOT(deleteLater()));
    m_closeAnimation->setDuration(200);
    QRectF r = rect();
    QRectF r2 = r;
    r2.setLeft(r.left() + r.width() / 2);
    r2.setTop(r.top() + r.height() / 2);
    r2.setWidth(1);
    r2.setHeight(1);
    m_closeAnimation->setStartValue(r);
    m_closeAnimation->setEndValue(r2);
    m_closeAnimation->setEasingCurve(QEasingCurve::InQuad);
    m_closeAnimation->start();
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

void AbstractClipItem::resizeStart(int posx)
{
    GenTime durationDiff = GenTime(posx, m_fps) - m_info.startPos;
    if (durationDiff == GenTime()) return;
    //kDebug() << "-- RESCALE DIFF=" << durationDiff.frames(25) << ", CLIP: " << startPos().frames(25) << "-" << endPos().frames(25);

    if (type() == AVWIDGET && cropStart() + durationDiff < GenTime()) {
        durationDiff = GenTime() - cropStart();
    } else if (durationDiff >= cropDuration()) {
        return;
        if (cropDuration() > GenTime(3, m_fps)) durationDiff = GenTime(3, m_fps);
        else return;
    }
    //kDebug()<<"// DURATION DIFF: "<<durationDiff.frames(25)<<", POS: "<<pos().x();
    m_info.startPos += durationDiff;

    if (type() == AVWIDGET) {
        m_info.cropStart += durationDiff;
    }
    m_info.cropDuration = m_info.cropDuration - durationDiff;
    setRect(0, 0, cropDuration().frames(m_fps) - 0.02, rect().height());
    moveBy(durationDiff.frames(m_fps), 0);
    //setPos(m_startPos.frames(m_fps), pos().y());
    if ((int) scenePos().x() != posx) {
        //kDebug()<<"//////  WARNING, DIFF IN XPOS: "<<pos().x()<<" == "<<m_startPos.frames(m_fps);
        GenTime diff = GenTime((int) pos().x() - posx, m_fps);

        if (type() == AVWIDGET) {
            m_info.cropStart += diff;
        }
        m_info.cropDuration = m_info.cropDuration - diff;
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

void AbstractClipItem::resizeEnd(int posx)
{
    GenTime durationDiff = GenTime(posx, m_fps) - endPos();
    if (durationDiff == GenTime()) return;
    if (cropDuration() + durationDiff <= GenTime()) {
        durationDiff = GenTime() - (cropDuration() - GenTime(3, m_fps));
    }

    m_info.cropDuration += durationDiff;

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
                m_info.cropDuration = diff;
                setRect(0, 0, cropDuration().frames(m_fps) - 0.02, rect().height());
                break;
            }
        }
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

void AbstractClipItem::drawKeyFrames(QPainter *painter, QRectF /*exposedRect*/)
{
    if (m_keyframes.count() < 2) return;
    QRectF br = rect();
    double maxw = br.width() / cropDuration().frames(m_fps);
    double maxh = br.height() / 100.0 * m_keyframeFactor;
    double x1;
    double y1;
    double x2;
    double y2;

    // draw line showing default value
    bool active = isSelected() || (parentItem() && parentItem()->isSelected());
    if (active) {
        x1 = br.x();
        x2 = br.right();
        y1 = br.bottom() - m_keyframeDefault * maxh;
        QLineF l(x1, y1, x2, y1);
        QLineF l2 = painter->matrix().map(l);
        painter->setPen(QColor(168, 168, 168, 180));
        painter->drawLine(l2);
        painter->setPen(QColor(108, 108, 108, 180));
        painter->drawLine(l2.translated(0, 1));
        painter->setPen(QColor(Qt::white));
    }

    // draw keyframes
    QMap<int, int>::const_iterator i = m_keyframes.constBegin();
    QColor color(Qt::blue);
    x1 = br.x() + maxw * (i.key() - cropStart().frames(m_fps));
    y1 = br.bottom() - i.value() * maxh;
    QLineF l2;
    while (i != m_keyframes.constEnd()) {
        if (i.key() == m_selectedKeyframe) color = QColor(Qt::red);
        else color = QColor(Qt::blue);
        ++i;
        if (i == m_keyframes.constEnd()) break;
        x2 = br.x() + maxw * (i.key() - cropStart().frames(m_fps));
        y2 = br.bottom() - i.value() * maxh;
        QLineF l(x1, y1, x2, y2);
        l2 = painter->matrix().map(l);
        painter->drawLine(l2);
        if (active) {
            const QRectF frame(l2.x1() - 3, l2.y1() - 3, 6, 6);
            painter->fillRect(frame, color);
        }
        x1 = x2;
        y1 = y2;
    }
    if (active) {
        const QRectF frame(l2.x2() - 3, l2.y2() - 3, 6, 6);
        painter->fillRect(frame, color);
    }
}

int AbstractClipItem::mouseOverKeyFrames(QPointF pos, double maxOffset)
{
    const QRectF br = sceneBoundingRect();
    double maxw = br.width() / cropDuration().frames(m_fps);
    double maxh = br.height() / 100.0 * m_keyframeFactor;
    if (m_keyframes.count() > 1) {
        QMap<int, int>::const_iterator i = m_keyframes.constBegin();
        double x1;
        double y1;
        while (i != m_keyframes.constEnd()) {
            x1 = br.x() + maxw * (i.key() - cropStart().frames(m_fps));
            y1 = br.bottom() - i.value() * maxh;
            if (qAbs(pos.x() - x1) < maxOffset && qAbs(pos.y() - y1) < 10) {
                setToolTip('[' + QString::number((GenTime(i.key(), m_fps) - cropStart()).seconds(), 'f', 2) + i18n("seconds") + ", " + QString::number(i.value(), 'f', 1) + "%]");
                return i.key();
            } else if (x1 > pos.x()) break;
            ++i;
        }
    }
    setToolTip(QString());
    return -1;
}

void AbstractClipItem::updateSelectedKeyFrame()
{
    if (m_editedKeyframe == -1) return;
    QRectF br = sceneBoundingRect();
    double maxw = br.width() / cropDuration().frames(m_fps);
    double maxh = br.height() / 100.0 * m_keyframeFactor;
    update(br.x() + maxw *(m_selectedKeyframe - cropStart().frames(m_fps)) - 3, br.bottom() - m_keyframes[m_selectedKeyframe] * maxh - 3, 12, 12);
    m_selectedKeyframe = m_editedKeyframe;
    update(br.x() + maxw *(m_selectedKeyframe - cropStart().frames(m_fps)) - 3, br.bottom() - m_keyframes[m_selectedKeyframe] * maxh - 3, 12, 12);
}

int AbstractClipItem::selectedKeyFramePos() const
{
    return m_editedKeyframe;
}

double AbstractClipItem::selectedKeyFrameValue() const
{
    return m_keyframes[m_editedKeyframe];
}

void AbstractClipItem::updateKeyFramePos(const GenTime pos, const double value)
{
    if (!m_keyframes.contains(m_selectedKeyframe)) return;
    int newpos = (int) pos.frames(m_fps);
    int start = cropStart().frames(m_fps);
    int end = (cropStart() + cropDuration()).frames(m_fps) - 1;
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
    m_keyframes[newpos] = (int) newval;
    m_selectedKeyframe = newpos;
    update();
}

double AbstractClipItem::keyFrameFactor() const
{
    return m_keyframeFactor;
}

void AbstractClipItem::addKeyFrame(const GenTime pos, const double value)
{
    QRectF br = sceneBoundingRect();
    double maxh = 100.0 / br.height() / m_keyframeFactor;
    int newval = (br.bottom() - value) * maxh;
    //kDebug() << "Rect: " << br << "/ SCENE: " << sceneBoundingRect() << ", VALUE: " << value << ", MAX: " << maxh << ", NEWVAL: " << newval;
    int newpos = (int) pos.frames(m_fps) ;
    m_keyframes[newpos] = newval;
    m_selectedKeyframe = newpos;
    update();
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
    if (scene()) return static_cast <CustomTrackScene*>(scene());
    return NULL;
}

void AbstractClipItem::setItemLocked(bool locked)
{
    if (locked) {
        setSelected(false);
        setFlag(QGraphicsItem::ItemIsMovable, false);
        setFlag(QGraphicsItem::ItemIsSelectable, false);
    } else {
        setFlag(QGraphicsItem::ItemIsMovable, true);
        setFlag(QGraphicsItem::ItemIsSelectable, true);
    }
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
    } else QGraphicsItem::mousePressEvent(event);
}

