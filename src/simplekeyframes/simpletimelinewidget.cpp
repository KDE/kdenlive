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
#include "kdenlivesettings.h"

#include <QStylePainter>
#include <QMouseEvent>

#include <QFontDatabase>
#include <KColorScheme>

SimpleTimelineWidget::SimpleTimelineWidget(QWidget *parent) :
    QWidget(parent),
    m_duration(1),
    m_position(0),
    m_currentKeyframe(-1),
    m_currentKeyframeOriginal(-1),
    m_hoverKeyframe(-1),
    m_lineHeight(10),
    m_scale(1)
{
    setMouseTracking(true);
    setMinimumSize(QSize(150, 20));
    setSizePolicy(QSizePolicy(QSizePolicy::Expanding, QSizePolicy::Maximum));
    setFont(QFontDatabase::systemFont(QFontDatabase::SmallestReadableFont));
    QPalette p = palette();
    KColorScheme scheme(p.currentColorGroup(), KColorScheme::Window, KSharedConfig::openConfig(KdenliveSettings::colortheme()));
    m_colSelected = scheme.decoration(KColorScheme::HoverColor).color();
    m_colKeyframe = scheme.foreground(KColorScheme::LinkText).color();
    m_colKeyframeBg = scheme.shade(KColorScheme::MidShade);
}

void SimpleTimelineWidget::setKeyframes(const QList<int> &keyframes)
{
    m_keyframes = keyframes;
    qSort(m_keyframes);
    m_currentKeyframe = m_currentKeyframeOriginal = -1;
    emit atKeyframe(m_keyframes.contains(m_position));
    update();
}

void SimpleTimelineWidget::slotSetPosition(int pos)
{
    if (pos != m_position) {
        m_position = pos;
        emit atKeyframe(m_keyframes.contains(m_position));
        update();
    }
}

void SimpleTimelineWidget::slotAddKeyframe(int pos, int select)
{
    if (pos < 0) {
        pos = m_position;
    }

    m_keyframes.append(pos);
    qSort(m_keyframes);
    if (select) {
        m_currentKeyframe = m_currentKeyframeOriginal = pos;
    }
    update();

    emit keyframeAdded(pos);
    if (pos == m_position) {
        emit atKeyframe(true);
    }
}

void SimpleTimelineWidget::slotAddRemove()
{
    if (m_keyframes.contains(m_position)) {
        slotRemoveKeyframe(m_position);
    } else {
        slotAddKeyframe(m_position);
    }
}

void SimpleTimelineWidget::slotRemoveKeyframe(int pos)
{
    m_keyframes.removeAll(pos);
    if (m_currentKeyframe == pos) {
        m_currentKeyframe = m_currentKeyframeOriginal = -1;
    }
    update();
    emit keyframeRemoved(pos);
    if (pos == m_position) {
        emit atKeyframe(false);
    }
}

void SimpleTimelineWidget::setDuration(int dur)
{
    m_duration = dur;
}

void SimpleTimelineWidget::slotGoToNext()
{
    if (m_position == m_duration) {
        return;
    }

    foreach (const int &keyframe, m_keyframes) {
        if (keyframe > m_position) {
            slotSetPosition(keyframe);
            emit positionChanged(keyframe);
            emit atKeyframe(true);
            return;
        }
    }

    // no keyframe after current position
    slotSetPosition(m_duration);
    emit positionChanged(m_duration);
    emit atKeyframe(false);
}

void SimpleTimelineWidget::slotGoToPrev()
{
    if (m_position == 0) {
        return;
    }

    for (int i = m_keyframes.count() - 1; i >= 0; --i) {
        const int framePos(m_keyframes.at(i));
        if (framePos < m_position) {
            slotSetPosition(framePos);
            emit positionChanged(framePos);
            emit atKeyframe(true);
            return;
        }
    }

    // no keyframe before current position
    slotSetPosition(0);
    emit positionChanged(0);
    emit atKeyframe(false);
}

void SimpleTimelineWidget::mousePressEvent(QMouseEvent *event)
{
    int pos = event->x() / m_scale;
    if (event->y() < m_lineHeight && event->button() == Qt::LeftButton)  {
        foreach (const int &keyframe, m_keyframes) {
            if (qAbs(keyframe - pos) < 5) {
                m_currentKeyframeOriginal = keyframe;
                m_keyframes[m_keyframes.indexOf(keyframe)] = pos;
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
    emit atKeyframe(m_keyframes.contains(pos));
    update();
}

void SimpleTimelineWidget::mouseMoveEvent(QMouseEvent *event)
{
    int pos = qBound(0, (int)(event->x() / m_scale), m_duration);
    if (event->buttons() & Qt::LeftButton) {
        if (m_currentKeyframe >= 0) {
            if (!m_keyframes.contains(pos)) {
                // snap to position cursor
                if (KdenliveSettings::snaptopoints() && qAbs(pos - m_position) < 5 && !m_keyframes.contains(m_position)) {
                    pos = m_position;
                }
                // should we maybe sort here?
                m_keyframes[m_keyframes.indexOf(m_currentKeyframe)] = pos;
                m_currentKeyframe = pos;
                emit keyframeMoving(m_currentKeyframeOriginal, m_currentKeyframe);
                emit atKeyframe(m_keyframes.contains(m_position));
            }
        } else {
            m_position = pos;
            emit positionChanged(pos);
            emit atKeyframe(m_keyframes.contains(pos));
        }
        update();
        return;
    } else {
        if (event->y() < m_lineHeight) {
            foreach (const int &keyframe, m_keyframes) {
                if (qAbs(keyframe - pos) < 5) {
                    m_hoverKeyframe = keyframe;
                    setCursor(Qt::PointingHandCursor);
                    update();
                    return;
                }
            }
        }

        if (m_hoverKeyframe != -1) {
            m_hoverKeyframe = -1;
            setCursor(Qt::ArrowCursor);
            update();
        }
    }
}

void SimpleTimelineWidget::mouseReleaseEvent(QMouseEvent *event)
{
    Q_UNUSED(event)

    if (m_currentKeyframe >= 0) {
        qSort(m_keyframes);
        emit keyframeMoved(m_currentKeyframeOriginal, m_currentKeyframe);
    }
}

void SimpleTimelineWidget::mouseDoubleClickEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && event->y() < m_lineHeight) {
        int pos = qBound(0, (int)(event->x() / m_scale), m_duration);
        foreach (const int &keyframe, m_keyframes) {
            if (qAbs(keyframe - pos) < 5) {
                m_keyframes.removeAll(keyframe);
                if (keyframe == m_currentKeyframe) {
                    m_currentKeyframe = m_currentKeyframeOriginal = -1;
                }
                emit keyframeRemoved(keyframe);
                if (keyframe == m_position) {
                    emit atKeyframe(false);
                }
                return;
            }
        }

        // add new keyframe
        m_keyframes.append(pos);
        qSort(m_keyframes);
        emit keyframeAdded(pos);
        if (pos == m_position) {
            emit atKeyframe(true);
        }
    } else {
        QWidget::mouseDoubleClickEvent(event);
    }
}

void SimpleTimelineWidget::wheelEvent(QWheelEvent *event)
{
    int change = event->delta() < 0 ? -1 : 1;
    /*if (m_currentKeyframe > 0) {
        m_currentKeyframe = qBound(0, m_currentKeyframe + change, m_duration);
        emit keyframeMoved(m_currentKeyframeOriginal, m_currentKeyframe);
    } else { */
    m_position = qBound(0, m_position + change, m_duration);
    emit positionChanged(m_position);
//     }
    emit atKeyframe(m_keyframes.contains(m_position));
    update();
}

void SimpleTimelineWidget::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)

    QStylePainter p(this);
    m_scale = width() / (double)(m_duration);
    p.translate(0, m_lineHeight);

    p.setPen(m_colKeyframe);
    p.setBrush(m_colKeyframeBg);

    /*
     * keyframes
     */
    QPolygonF keyframe = QPolygonF() << QPointF(0, -10) << QPointF(-4, -6) << QPointF(0, -2) << QPointF(4, -6);
    QPolygonF tmp;
    foreach (const int &pos, m_keyframes) {
        tmp = keyframe;
        tmp.translate(pos * m_scale, 0);
        if (pos == m_currentKeyframe || pos == m_hoverKeyframe) {
            p.setBrush(m_colSelected);
        }

        p.drawConvexPolygon(tmp);
        p.drawLine(QLineF(0, -1, 0, 5).translated(pos * m_scale, 0));

        if (pos == m_currentKeyframe || pos == m_hoverKeyframe) {
            p.setBrush(m_colKeyframeBg);
        }
    }

    p.setPen(palette().dark().color());

    /*
     * Time-"line"
     */
    p.drawLine(0, 0, width(), 0);

    /*
     * current position
     */
    QPolygonF position = QPolygonF() << QPointF(0, 1) << QPointF(5, 6) << QPointF(-5, 6);
    position.translate(m_position * m_scale, 0);
    p.setBrush(palette().dark().color());
    p.drawConvexPolygon(position);
}

