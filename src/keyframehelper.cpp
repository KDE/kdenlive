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
#include "kdenlivesettings.h"
#include "definitions.h"

#include <KDebug>
#include <KGlobalSettings>
#include <KColorScheme>

#include <QMouseEvent>
#include <QStylePainter>
#include <QApplication>

const int margin = 5;
const int cursorWidth = 6;

KeyframeHelper::KeyframeHelper(QWidget *parent) :
        QWidget(parent),
        m_geom(NULL),
        m_position(0),
        m_scale(0),
        m_movingKeyframe(false),
        m_lineHeight(9),
        m_drag(false),
        m_hoverKeyframe(-1)
{
    setFont(KGlobalSettings::toolBarFont());
    setMouseTracking(true);
    QPalette p = palette();
    KColorScheme scheme(p.currentColorGroup(), KColorScheme::Window, KSharedConfig::openConfig(KdenliveSettings::colortheme()));
    m_selected = scheme.decoration(KColorScheme::HoverColor).color();
    m_keyframe = scheme.foreground(KColorScheme::LinkText).color();
    m_keyframebg = scheme.shade(KColorScheme::MidShade);
}

// virtual
void KeyframeHelper::mousePressEvent(QMouseEvent * event)
{
    m_hoverKeyframe = -1;
    if (event->button() != Qt::LeftButton) {
	QWidget::mousePressEvent(event);
	return;
    }
    int xPos = event->x() - margin;
    if (m_geom != NULL && (event->y() < m_lineHeight)) {
        // check if we want to move a keyframe
        int mousePos = qMax((int)(xPos / m_scale), 0);
        Mlt::GeometryItem item;
        if (m_geom->next_key(&item, mousePos) == 0) {
            if (qAbs(item.frame() * m_scale - xPos) < 4) {
                m_drag = true;
                m_movingItem.x(item.x());
                m_movingItem.y(item.y());
                m_movingItem.w(item.w());
                m_movingItem.h(item.h());
                m_movingItem.mix(item.mix());
                m_movingItem.frame(item.frame());

                while (!m_extraMovingItems.isEmpty()) {
                    Mlt::GeometryItem *gitem = m_extraMovingItems.takeFirst();
                    delete gitem;
                }
                for (int i = 0; i < m_extraGeometries.count(); i++) {
                    Mlt::GeometryItem *item2 = new Mlt::GeometryItem();
                    if (m_extraGeometries.at(i)->next_key(item, mousePos) == 0) {
                        item2->x(item.x());
                        item2->frame(item.frame());
                        m_extraMovingItems.append(item2);
                    } else {
                        delete(item2);
                    }
                }
                
                m_dragStart = event->pos();
                m_movingKeyframe = true;
                return;
            }
        }
    }
    if (event->y() >= m_lineHeight && event->y() < height()) {
        m_drag = true;
        m_position = xPos / m_scale;
        emit positionChanged(m_position);
        update();
    }
}

void KeyframeHelper::leaveEvent( QEvent * event )
{
    Q_UNUSED(event);
    if (m_hoverKeyframe != -1) {
        m_hoverKeyframe = -1;
        update();
    }
}

// virtual
void KeyframeHelper::mouseMoveEvent(QMouseEvent * event)
{
    int xPos = event->x() - margin;
    if (!m_drag) {
        int mousePos = qMax((int)(xPos / m_scale), 0);
        if (qAbs(m_position * m_scale - xPos) < cursorWidth && event->y() >= m_lineHeight) {
            // Mouse over time cursor
            if (m_hoverKeyframe != -2) {
                m_hoverKeyframe = -2;
                update();
            }
            event->accept();
            return;
        }
        if (m_geom != NULL && (event->y() < m_lineHeight)) {
            // check if we want to move a keyframe
            Mlt::GeometryItem item;
            if (m_geom->next_key(&item, mousePos) == 0) {
                if (qAbs(item.frame() * m_scale - xPos) < 4) {
                    if (m_hoverKeyframe == item.frame()) return;
                    m_hoverKeyframe = item.frame();
                    setCursor(Qt::PointingHandCursor);
                    update();
                    event->accept();
                    return;
                }
            }
        }
        if (m_hoverKeyframe != -1) {
            m_hoverKeyframe = -1;
            setCursor(Qt::ArrowCursor);
            update();
        }
        event->accept();
	return;
    }
    if (m_movingKeyframe) {
        if (!m_dragStart.isNull()) {
            if ((QPoint(xPos, event->y()) - m_dragStart).manhattanLength() < QApplication::startDragDistance()) return;
            m_dragStart = QPoint();
            m_geom->remove(m_movingItem.frame());
            for (int i = 0; i < m_extraGeometries.count(); i++)
                m_extraGeometries[i]->remove(m_movingItem.frame());
        }
        int pos = qBound(0, (int)(xPos / m_scale), frameLength);
        if (KdenliveSettings::snaptopoints() && qAbs(pos - m_position) < 5) pos = m_position;
        m_movingItem.frame(pos);
        for (int i = 0; i < m_extraMovingItems.count(); i++) {
            m_extraMovingItems[i]->frame(pos);
        }
        update();
        return;
    }
    m_position = xPos / m_scale;
    m_position = qMax(0, m_position);
    m_position = qMin(frameLength, m_position);
    m_hoverKeyframe = -2;
    emit positionChanged(m_position);
    update();
}

void KeyframeHelper::mouseDoubleClickEvent(QMouseEvent * event)
{
    if (m_geom != NULL && event->button() == Qt::LeftButton) {
        // check if we want to move a keyframe
        int xPos = event->x() - margin;
        int mousePos = qMax((int)(xPos / m_scale - 5), 0);
        Mlt::GeometryItem item;
        if (m_geom->next_key(&item, mousePos) == 0 && item.frame() - mousePos < 10) {
            // There is already a keyframe close to mouse click
            emit removeKeyframe(item.frame());
            return;
        }
        // add new keyframe
        emit addKeyframe((int)(xPos / m_scale));
    }
}

// virtual
void KeyframeHelper::mouseReleaseEvent(QMouseEvent * event)
{
    m_drag = false;
    setCursor(Qt::ArrowCursor);
    m_hoverKeyframe = -1;
    if (m_movingKeyframe) {
        m_geom->insert(m_movingItem);
        m_movingKeyframe = false;

        for (int i = 0; i < m_extraGeometries.count(); i++) {
            m_extraGeometries[i]->insert(m_extraMovingItems.at(i));
        }
        
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
    m_scale = (double) (width() - 2 * margin) / frameLength;
    if (m_geom != NULL) {
        int pos = 0;
        p.setPen(m_keyframe);
        p.setBrush(m_keyframebg);
        Mlt::GeometryItem item;
        while (true) {
            if (m_geom->next_key(&item, pos) == 1) break;
            pos = item.frame();
            if (pos == m_hoverKeyframe) {
                p.setBrush(m_selected);
            }
            int scaledPos = margin + pos * m_scale;
            // draw keyframes
            p.drawLine(scaledPos, 9, scaledPos, 15);
            // draw pointer
            QPolygon pa(4);
            pa.setPoints(4,
                         scaledPos,     0,
                         scaledPos - 4, 4,
                         scaledPos,     8,
                         scaledPos + 4, 4);
            p.drawPolygon(pa);
            //p.drawEllipse(scaledPos - 4, 0, 8, 8);
            if (pos == m_hoverKeyframe) {
                p.setBrush(m_keyframebg);
            }
            //p.fillRect(QRect(scaledPos - 1, 0, 2, 15), QBrush(QColor(255, 20, 20)));
            pos++;
        }
        
        if (m_movingKeyframe) {
            p.setBrush(m_selected);
            int scaledPos = margin + (int)(m_movingItem.frame() * m_scale);
            // draw keyframes
            p.drawLine(scaledPos, 9, scaledPos, 15);
            // draw pointer
            QPolygon pa(5);
            pa.setPoints(4,
                         scaledPos,     0,
                         scaledPos - 4, 4,
                         scaledPos,     8,
                         scaledPos + 4, 4);
            p.drawPolygon(pa);
            p.setBrush(m_keyframe);
        }
    }
    p.setPen(palette().dark().color());
    p.drawLine(margin, m_lineHeight, width() - margin - 1, m_lineHeight);
    p.drawLine(margin, m_lineHeight - 3, margin, m_lineHeight + 3);
    p.drawLine(width() - margin - 1, m_lineHeight - 3, width() - margin - 1, m_lineHeight + 3);

    // draw pointer
    QPolygon pa(3);
    const int cursor = margin + m_position * m_scale;
    pa.setPoints(3, cursor - cursorWidth, 16, cursor + cursorWidth, 16, cursor, 10);
    if (m_hoverKeyframe == -2)
        p.setBrush(palette().highlight());
    else
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
    while (!m_extraGeometries.isEmpty()) {
        Mlt::Geometry *geom = m_extraGeometries.takeFirst();
        delete geom;
    }
    update();
}

void KeyframeHelper::addGeometry(Mlt::Geometry *geom)
{
    m_extraGeometries.append(geom);
}

#include "keyframehelper.moc"
