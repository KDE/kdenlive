/***************************************************************************
 *   Copyright (C) 2011 by Till Theato (root@ttill.de)                     *
 *   This file is part of Kdenlive (www.kdenlive.org).                     *
 *                                                                         *
 *   Kdenlive is free software: you can redistribute it and/or modify      *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation, either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   Kdenlive is distributed in the hope that it will be useful,           *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with Kdenlive.  If not, see <http://www.gnu.org/licenses/>.     *
 ***************************************************************************/

#include "simpletimelinewidget.h"

#include <QPainter>
#include <QMouseEvent>


SimpleTimelineWidget::SimpleTimelineWidget(QWidget* parent) :
        QWidget(parent),
        m_duration(1),
        m_position(0),
        m_currentKeyframe(-1),
        m_currentKeyframeOriginal(-1),
        m_lineHeight(10),
        m_scale(1)
{
    setMouseTracking(true);
    setMinimumSize(QSize(150, 20));
    setSizePolicy(QSizePolicy(QSizePolicy::Expanding, QSizePolicy::Maximum));
}

void SimpleTimelineWidget::setKeyframes(QList <int> keyframes)
{
    m_keyframes = keyframes;
    qSort(m_keyframes);
    m_currentKeyframe = m_currentKeyframeOriginal = -1;
    update();
}

void SimpleTimelineWidget::slotSetPosition(int pos)
{
    m_position = pos;
    update();
}

void SimpleTimelineWidget::slotAddKeyframe(int pos, int select)
{
    if (pos < 0)
        pos = m_position;

    m_keyframes.append(pos);
    qSort(m_keyframes);
    if (select)
        m_currentKeyframe = m_currentKeyframeOriginal = pos;
    update();

    emit keyframeAdded(pos);
}

void SimpleTimelineWidget::slotAddRemove()
{
    if (m_keyframes.contains(m_position))
        slotRemoveKeyframe(m_position);
    else
        slotAddKeyframe(m_position);
}

void SimpleTimelineWidget::slotRemoveKeyframe(int pos)
{
    m_keyframes.removeAll(pos);
    if (m_currentKeyframe == pos)
        m_currentKeyframe = m_currentKeyframeOriginal = -1;
    update();
    emit keyframeRemoved(pos);
}

void SimpleTimelineWidget::setDuration(int dur)
{
    m_duration = dur;
}

void SimpleTimelineWidget::slotGoToNext()
{
    foreach (const int &keyframe, m_keyframes) {
        if (keyframe > m_position) {
            slotSetPosition(keyframe);
            emit positionChanged(keyframe);
            return;
        }
    }

    // no keyframe after current position
    slotSetPosition(m_duration);
    emit positionChanged(m_duration);
}

void SimpleTimelineWidget::slotGoToPrev()
{
    for (int i = m_keyframes.count() - 1; i >= 0; --i) {
        if (m_keyframes.at(i) < m_position) {
            slotSetPosition(m_keyframes.at(i));
            emit positionChanged(m_keyframes.at(i));
            return;
        }
    }

    // no keyframe before current position
    slotSetPosition(0);
    emit positionChanged(0);
}

void SimpleTimelineWidget::mousePressEvent(QMouseEvent* event)
{
    int pos = (event->x() - 5) / m_scale;
    if (qAbs(event->y() - m_lineHeight) < m_lineHeight / 5. && event->button() == Qt::LeftButton)  {
        foreach(const int &keyframe, m_keyframes) {
            if (qAbs(keyframe - pos) < 5) {
                m_currentKeyframeOriginal = keyframe;
                m_currentKeyframe = pos;
                update();
                return;
            }
        }
    }

    // no keyframe next to mouse
    m_currentKeyframe = m_currentKeyframeOriginal = -1;
    m_position = pos;
    emit positionChanged(pos);
    update();
}

void SimpleTimelineWidget::mouseMoveEvent(QMouseEvent* event)
{
    if (event->buttons() & Qt::LeftButton) {
        int pos = qBound(0, (int)((event->x() - 5) / m_scale), m_duration);
        if (m_currentKeyframe >= 0) {
            m_currentKeyframe = pos;
            emit keyframeMoving(m_currentKeyframeOriginal, m_currentKeyframe);
        } else {
            m_position = pos;
            emit positionChanged(pos);
        }
        update();
        return;
    }

    // cursor
}

void SimpleTimelineWidget::mouseReleaseEvent(QMouseEvent* event)
{
    if (m_currentKeyframe > 0) {
        emit keyframeMoved(m_currentKeyframeOriginal, m_currentKeyframe);
    }
}

void SimpleTimelineWidget::wheelEvent(QWheelEvent* event)
{
    int change = event->delta() < 0 ? -1 : 1;
    if (m_currentKeyframe > 0) {
        m_currentKeyframe = qBound(0, m_currentKeyframe + change, m_duration);
        emit keyframeMoved(m_currentKeyframeOriginal, m_currentKeyframe);
    } else {
        m_position = qBound(0, m_position + change, m_duration);
        emit positionChanged(m_position);
    }
    update();
}

void SimpleTimelineWidget::paintEvent(QPaintEvent* event)
{
    QPainter p(this);
    int min = 5;
    int max = width() - 6;
    m_scale = (max - min) / (double)(m_duration);
    p.translate(min, m_lineHeight);

    p.setPen(QPen(palette().foreground().color(), 1, Qt::SolidLine));

    /*
     * Time-"line"
     */
    p.drawLine(0, 0, max, 0);

    /*
     * current position
     */
    p.fillRect(QRectF(-1, -10, 3, 20).translated(m_position * m_scale, 0), QBrush(palette().foreground().color(), Qt::SolidPattern));

    /*
     * keyframes
     */
    p.setPen(QPen(palette().highlight().color(), 1, Qt::SolidLine));
    p.setBrush(Qt::NoBrush);
    QPolygonF keyframe = QPolygonF() << QPointF(0, -4) << QPointF(-4, 0) << QPointF(0, 4) << QPointF(4, 0);
    QPolygonF tmp;
    foreach (const int &pos, m_keyframes) {
        tmp = keyframe;
        tmp.translate(pos * m_scale, 0);
        if (pos == m_currentKeyframe)
            p.setBrush(QBrush(palette().highlight().color(), Qt::SolidPattern));

        p.drawConvexPolygon(tmp);

        if (pos == m_currentKeyframe)
            p.setBrush(Qt::NoBrush);
    }
}

#include "simpletimelinewidget.moc"
