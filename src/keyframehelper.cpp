/***************************************************************************
 *   Copyright (C) 2007 by Jean-Baptiste Mardelle (jb@kdenlive.org)        *
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


#include "keyframehelper.h"
#include "definitions.h"

#include <KDebug>
#include <KGlobalSettings>

#include <QMouseEvent>
#include <QStylePainter>
#include <QApplication>


KeyframeHelper::KeyframeHelper(QWidget *parent) :
        QWidget(parent),
        m_geom(NULL),
        m_position(0),
        m_scale(0),
        m_movingKeyframe(false),
        m_lineHeight(9)
{
    setFont(KGlobalSettings::toolBarFont());
}

// virtual
void KeyframeHelper::mousePressEvent(QMouseEvent * event)
{
    if (m_geom != NULL && (event->y() < m_lineHeight) && event->button() == Qt::LeftButton) {
        // check if we want to move a keyframe
        int mousePos = qMax((int)(event->x() / m_scale), 0);
        Mlt::GeometryItem item;
        if (m_geom->next_key(&item, mousePos) == 0) {
            if (qAbs(item.frame() * m_scale - (int)(event->x())) < 3) {
                m_movingItem.x(item.x());
                m_movingItem.y(item.y());
                m_movingItem.w(item.w());
                m_movingItem.h(item.h());
                m_movingItem.mix(item.mix());
                m_movingItem.frame(item.frame());
                m_dragStart = event->pos();
                m_movingKeyframe = true;
                return;
            }
        }
    }
    m_position = event->x() / m_scale;
    emit positionChanged(m_position);
    update();
}

// virtual
void KeyframeHelper::mouseMoveEvent(QMouseEvent * event)
{
    if (m_movingKeyframe) {
        if (!m_dragStart.isNull()) {
            if ((event->pos() - m_dragStart).manhattanLength() < QApplication::startDragDistance()) return;
            m_dragStart = QPoint();
            m_geom->remove(m_movingItem.frame());
        }
        int pos = qMax(0, (int)(event->x() / m_scale));
        pos = qMin(frameLength, pos);
        m_movingItem.frame(pos);
        update();
        return;
    }
    m_position = event->x() / m_scale;
    m_position = qMax(0, m_position);
    m_position = qMin(frameLength, m_position);
    emit positionChanged(m_position);
    update();
}

void KeyframeHelper::mouseDoubleClickEvent(QMouseEvent * event)
{
    if (m_geom != NULL && event->button() == Qt::LeftButton) {
        // check if we want to move a keyframe
        int mousePos = qMax((int)(event->x() / m_scale - 5), 0);
        Mlt::GeometryItem item;
        if (m_geom->next_key(&item, mousePos) == 0 && item.frame() - mousePos < 10) {
            // There is already a keyframe close to mouse click
            emit removeKeyframe(item.frame());
            return;
        }
        // add new keyframe
        emit addKeyframe((int)(event->x() / m_scale));
    }
}

// virtual
void KeyframeHelper::mouseReleaseEvent(QMouseEvent * event)
{
    if (m_movingKeyframe) {
        m_geom->insert(m_movingItem);
        m_movingKeyframe = false;
        emit keyframeMoved(m_position);
        return;
    }
    QWidget::mouseReleaseEvent(event);
}

// virtual
void KeyframeHelper::wheelEvent(QWheelEvent * e)
{
    if (e->delta() < 0)
        --m_position;
    else
        ++m_position;
    m_position = qMax(0, m_position);
    m_position = qMin(frameLength, m_position);
    emit positionChanged(m_position);
    update();
    /*    int delta = 1;
        if (e->modifiers() == Qt::ControlModifier) delta = m_timecode.fps();
        if (e->delta() < 0) delta = 0 - delta;
        m_view->moveCursorPos(delta);
    */
}

// virtual
void KeyframeHelper::paintEvent(QPaintEvent *e)
{
    QStylePainter p(this);
    const QRectF clipRect = e->rect();
    p.setClipRect(clipRect);
    m_scale = (double) width() / frameLength;
    if (m_geom != NULL) {
        int pos = 0;
        p.setPen(QColor(255, 20, 20));
        p.setBrush(QColor(255, 20, 20));
        Mlt::GeometryItem item;
        while (true) {
            if (m_geom->next_key(&item, pos) == 1) break;
            pos = item.frame();
            int scaledPos = pos * m_scale;
            // draw keyframes
            p.drawLine(scaledPos, 10, scaledPos, 14);
            // draw pointer
            QPolygon pa(4);
            pa.setPoints(4,
                         scaledPos,     0,
                         scaledPos - 4, 4,
                         scaledPos,     8,
                         scaledPos + 4, 4);
            p.drawPolygon(pa);
            //p.fillRect(QRect(scaledPos - 1, 0, 2, 15), QBrush(QColor(255, 20, 20)));
            pos++;
        }
        if (m_movingKeyframe) {
            int scaledPos = (int)(m_movingItem.frame() * m_scale);
            // draw keyframes
            p.drawLine(scaledPos, 10, scaledPos, 14);
            // draw pointer
            QPolygon pa(5);
            pa.setPoints(4,
                         scaledPos,     0,
                         scaledPos - 4, 4,
                         scaledPos,     8,
                         scaledPos + 4, 4);
            p.drawPolygon(pa);
        }
    }
    p.setPen(palette().dark().color());
    p.drawLine(clipRect.x(), m_lineHeight, clipRect.right(), m_lineHeight);

    // draw pointer
    QPolygon pa(3);
    const int cursor = m_position * m_scale;
    pa.setPoints(3, cursor - 5, 15, cursor + 5, 15, cursor, 10);
    p.setBrush(palette().dark().color());
    p.drawPolygon(pa);


}

int KeyframeHelper::value() const
{
    return m_position;
}

void KeyframeHelper::setValue(const int pos)
{
    if (pos == m_position || m_geom == NULL) return;
    m_position = pos;
    update();
}

void KeyframeHelper::setKeyGeometry(Mlt::Geometry *geom, const int length)
{
    m_geom = geom;
    frameLength = length;
    update();
}

#include "keyframehelper.moc"
