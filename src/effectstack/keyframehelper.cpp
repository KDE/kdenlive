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

#include <QFontDatabase>
#include <KColorScheme>

#include <QMouseEvent>
#include <QApplication>
#include <QPainter>

const int margin = 5;

#define SEEK_INACTIVE (-1)

KeyframeHelper::KeyframeHelper(QWidget *parent) :
    QWidget(parent)
    , frameLength(1)
    , m_geom(nullptr)
    , m_position(0)
    , m_scale(0)
    , m_movingKeyframe(false)
    , m_movingItem()
    , m_hoverKeyframe(-1)
    , m_seekPosition(SEEK_INACTIVE)
    , m_offset(0)
{
    setFont(QFontDatabase::systemFont(QFontDatabase::SmallestReadableFont));
    setMouseTracking(true);
    QPalette p = palette();
    m_size = QFontInfo(font()).pixelSize() * 1.8;
    m_lineHeight = m_size / 2;
    setMinimumHeight(m_size);
    setMaximumHeight(m_size);
    KColorScheme scheme(p.currentColorGroup(), KColorScheme::Window, KSharedConfig::openConfig(KdenliveSettings::colortheme()));
    m_selected = scheme.decoration(KColorScheme::HoverColor).color();
    m_keyframe = scheme.foreground(KColorScheme::LinkText).color();
}

// virtual
void KeyframeHelper::mousePressEvent(QMouseEvent *event)
{
    m_hoverKeyframe = -1;
    if (event->button() != Qt::LeftButton) {
        QWidget::mousePressEvent(event);
        return;
    }
    int xPos = event->x() - margin;
    int headOffset = m_lineHeight / 1.5;
    if (m_geom != nullptr && (event->y() <= headOffset)) {
        // check if we want to move a keyframe
        int mousePos = qMax((int)(xPos / m_scale), 0) + m_offset;
        Mlt::GeometryItem item;
        if (m_geom->next_key(&item, mousePos) == 0) {
            if (qAbs((item.frame() - m_offset)* m_scale - xPos) < headOffset) {
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
                for (int i = 0; i < m_extraGeometries.count(); ++i) {
                    if (m_extraGeometries.at(i)->next_key(item, mousePos) == 0) {
                        Mlt::GeometryItem *item2 = new Mlt::GeometryItem();
                        item2->x(item.x());
                        item2->frame(item.frame());
                        m_extraMovingItems.append(item2);
                    } else {
                        m_extraMovingItems.append(nullptr);
                    }
                }
                m_dragStart = event->pos();
                return;
            }
        }
    }
    int seekRequest = xPos / m_scale;
    if (seekRequest != m_position) {
        m_seekPosition = seekRequest;
        emit requestSeek(m_seekPosition);
        update();
    }
}

void KeyframeHelper::leaveEvent(QEvent *event)
{
    Q_UNUSED(event)
    if (m_hoverKeyframe != -1) {
        m_hoverKeyframe = -1;
        update();
    }
}

// virtual
void KeyframeHelper::mouseMoveEvent(QMouseEvent *event)
{
    int xPos = event->x() - margin;
    int headOffset = m_lineHeight / 1.5;
    if (event->buttons() == Qt::NoButton) {
        int mousePos = qMax((int)(xPos / m_scale), 0) + m_offset;
        if (qAbs(m_position * m_scale - xPos) < m_lineHeight && event->y() >= m_lineHeight) {
            // Mouse over time cursor
            if (m_hoverKeyframe != -2) {
                m_hoverKeyframe = -2;
                update();
            }
            event->accept();
            return;
        }
        if (m_geom != nullptr && (event->y() < m_lineHeight)) {
            // check if we want to move a keyframe
            Mlt::GeometryItem item;
            if (m_geom->next_key(&item, mousePos) == 0) {
                if (qAbs((item.frame() - m_offset) * m_scale - xPos) < headOffset) {
                    if (m_hoverKeyframe == item.frame()) {
                        return;
                    }
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
    if (!m_dragStart.isNull() || m_movingKeyframe) {
        if (!m_movingKeyframe) {
            if ((QPoint(xPos, event->y()) - m_dragStart).manhattanLength() < QApplication::startDragDistance()) {
                return;
            }
            m_movingKeyframe = true;
            m_dragStart = QPoint();
            m_geom->remove(m_movingItem.frame());
            for (int i = 0; i < m_extraGeometries.count(); ++i) {
                m_extraGeometries[i]->remove(m_movingItem.frame());
            }
        }
        int pos = qBound(0, (int)(xPos / m_scale), frameLength);
        if (KdenliveSettings::snaptopoints() && qAbs(pos - m_position) < headOffset / m_scale) {
            pos = m_position;
        }
        m_movingItem.frame(pos + m_offset);
        for (int i = 0; i < m_extraMovingItems.count(); ++i) {
            if (m_extraMovingItems.at(i)) {
                m_extraMovingItems[i]->frame(pos);
            }
        }
        update();
        return;
    }
    m_seekPosition = (int)(xPos / m_scale);
    m_seekPosition = qMax(0, m_seekPosition);
    m_seekPosition = qMin(frameLength, m_seekPosition);
    m_hoverKeyframe = -2;
    emit requestSeek(m_seekPosition);
    update();
}

void KeyframeHelper::mouseDoubleClickEvent(QMouseEvent *event)
{
    if (m_geom != nullptr && event->button() == Qt::LeftButton) {
        // check if we want to move a keyframe
        int xPos = event->x() - margin;
        int mousePos = qMax((int)(xPos / m_scale - 5), 0) + m_offset;
        Mlt::GeometryItem item;
        if (m_geom->next_key(&item, mousePos) == 0 && item.frame() - mousePos < 10) {
            // There is already a keyframe close to mouse click
            emit removeKeyframe(item.frame());
            return;
        }
        // add new keyframe
        emit addKeyframe((int)(xPos / m_scale) + m_offset);
    }
}

// virtual
void KeyframeHelper::mouseReleaseEvent(QMouseEvent *event)
{
    setCursor(Qt::ArrowCursor);
    m_hoverKeyframe = -1;
    if (m_movingKeyframe) {
        m_geom->insert(m_movingItem);
        m_movingKeyframe = false;

        for (int i = 0; i < m_extraGeometries.count(); ++i) {
            if (m_extraMovingItems.at(i)) {
                m_extraGeometries[i]->insert(m_extraMovingItems.at(i));
            }
        }
        m_movingKeyframe = false;
        emit keyframeMoved(m_position);
        return;
    } else if (!m_dragStart.isNull()) {
        m_seekPosition = m_movingItem.frame();
        m_dragStart = QPoint();
        emit requestSeek(m_seekPosition);
    }
    QWidget::mouseReleaseEvent(event);
}

// virtual
void KeyframeHelper::wheelEvent(QWheelEvent *e)
{
    if (e->delta() < 0) {
        --m_position;
    } else {
        ++m_position;
    }
    m_position = qMax(0, m_position);
    m_position = qMin(frameLength, m_position);
    emit requestSeek(m_position);
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
    QPainter p(this);
    p.setRenderHints(QPainter::Antialiasing);
    const QRectF clipRect = e->rect();
    p.setClipRect(clipRect);
    m_scale = (double)(width() - 2 * margin) / frameLength;
    int headOffset = m_lineHeight / 1.5;
    if (m_geom != nullptr) {
        int pos = m_offset;
        Mlt::GeometryItem item;
        while (true) {
            if (m_geom->next_key(&item, pos) == 1) {
                break;
            }
            pos = item.frame();
            int offsetPos = pos - m_offset;
            if (offsetPos < 0) {
                pos++;
                continue;
            }
            int scaledPos = margin + offsetPos * m_scale;
            // draw keyframes
            if (offsetPos == m_position || offsetPos == m_hoverKeyframe) {
                // active keyframe
                p.setBrush(m_selected);
                p.setPen(m_selected);
            } else {
                p.setPen(palette().text().color());
                p.setBrush(palette().text());
            }
            p.drawLine(scaledPos, headOffset, scaledPos, m_size);
            p.drawEllipse(scaledPos - headOffset / 2, 0, headOffset, headOffset);
            pos++;
        }
        if (m_movingKeyframe) {
            p.setBrush(m_selected);
            int scaledPos = margin + (int)((m_movingItem.frame() - m_offset) * m_scale);
            // draw keyframes
            p.drawLine(scaledPos, headOffset, scaledPos, m_size);
            p.drawEllipse(scaledPos - headOffset / 2, 0, headOffset, headOffset);
            p.setBrush(m_keyframe);
        }
    }
    p.setPen(palette().text().color());
    p.drawLine(margin, m_lineHeight + (headOffset / 2), width() - margin - 1, m_lineHeight + (headOffset / 2));
    p.drawLine(margin, m_lineHeight - headOffset, margin, m_lineHeight + headOffset);
    p.drawLine(width() - margin, m_lineHeight - headOffset, width() - margin, m_lineHeight + headOffset);
    p.setPen(Qt::NoPen);
    // draw pointer
    if (m_seekPosition != SEEK_INACTIVE) {
        p.fillRect(margin + m_seekPosition * m_scale - 1, 0, 3, height(), palette().dark());
    }
    QPolygon pa(3);
    const int cursor = margin + m_position * m_scale;
    int cursorwidth = (m_size - (m_lineHeight + headOffset / 2)) / 2 + 1;
    pa.setPoints(3, cursor - cursorwidth, m_size, cursor + cursorwidth, m_size, cursor, m_lineHeight + (headOffset / 2) + 1);
    if (m_hoverKeyframe == -2) {
        p.setBrush(palette().highlight());
    } else {
        p.setBrush(palette().text());
    }
    p.drawPolygon(pa);
}

int KeyframeHelper::value() const
{
    if (m_seekPosition == SEEK_INACTIVE) {
        return m_position + m_offset;
    }
    return m_seekPosition;
}

void KeyframeHelper::setValue(const int pos)
{
    if (pos == m_position || m_geom == nullptr) {
        return;
    }
    if (pos == m_seekPosition) {
        m_seekPosition = SEEK_INACTIVE;
    }
    m_position = pos;
    update();
}

void KeyframeHelper::setKeyGeometry(Mlt::Geometry *geom, int in, int out, bool useOffset)
{
    m_geom = geom;
    m_offset = useOffset ? in : 0;
    frameLength = qMax(1, out - in);
    update();
}

void KeyframeHelper::addGeometry(Mlt::Geometry *geom)
{
    m_extraGeometries.append(geom);
}

