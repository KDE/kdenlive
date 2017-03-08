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

#include "smallruler.h"

#include "kdenlivesettings.h"

#include <KColorScheme>
#include "klocalizedstring.h"

#include <QMouseEvent>
#include <QPainter>

#define SEEK_INACTIVE (-1)

// Width of a letter, used for cursor width
static int FONT_WIDTH;

SmallRuler::SmallRuler(Monitor *monitor, Render *render, QWidget *parent) :
    QWidget(parent)
    , m_cursorFramePosition(0)
    , m_maxval(2)
    , m_offset(0)
    , m_monitor(monitor)
    , m_render(render)
    , m_lastSeekPosition(SEEK_INACTIVE)
    , m_hoverZone(-1)
    , m_activeControl(CONTROL_NONE)
    , m_scale(1)
{
    QFontMetricsF fontMetrics(font());
    // Define size variables
    FONT_WIDTH = fontMetrics.averageCharWidth();
    m_zoneStart = 10;
    m_zoneEnd = 60;
    setMouseTracking(true);
    m_rulerHeight = QFontInfo(font()).pixelSize();
    setMinimumHeight(m_rulerHeight * 1.5 + 3);
    adjustScale(m_maxval, m_offset);
}

void SmallRuler::adjustScale(int maximum, int offset)
{
    m_maxval = maximum;
    m_offset = offset;
    m_scale = (double) width() / (double) maximum;
    if (m_scale == 0) {
        m_scale = 1;
    }

    if (m_scale > 0.5) {
        m_small = 25;
        m_medium = 5 * 25;
    } else if (m_scale > 0.09) {
        m_small = 5 * 25;
        m_medium = 30 * 25;
    } else {
        m_small = 30 * 25;
        m_medium = 60 * 25;
    }
    updatePixmap();
}

void SmallRuler::setZoneStart()
{
    setZone(m_render->getCurrentSeekPosition(), -1);
}

void SmallRuler::setZoneEnd(bool discardLastFrame)
{
    setZone(-1, m_render->getCurrentSeekPosition() - (discardLastFrame ? 1 : 0));
}

void SmallRuler::setZone(int start, int end)
{
    if (start != -1) {
        if (end != -1 && start >= end) {
            m_zoneEnd = qMin(m_maxval, end + (start - m_zoneStart));
            m_zoneStart = start;
        } else if (end == -1 && start >= m_zoneEnd) {
            m_zoneEnd = qMin(m_maxval, m_zoneEnd + (start - m_zoneStart));
            m_zoneStart = start;
        } else {
            m_zoneStart = start;
        }
    }
    if (end != -1) {
        if (start != -1 && end <= start) {
            m_zoneStart = qMax(0, start - (m_zoneEnd - end));
            m_zoneEnd = end;
        } else if (start == -1 && end < m_zoneStart) {
            m_zoneStart = qMax(0, m_zoneStart - (m_zoneEnd - end));
            m_zoneEnd = end;
        } else {
            m_zoneEnd = end;
        }
    }
    updatePixmap();
}

void SmallRuler::setMarkers(const QList<CommentedTime> &list)
{
    m_markers = list;
    updatePixmap();
}

QString SmallRuler::markerAt(GenTime pos)
{
    CommentedTime marker(pos, QString());
    int ix = m_markers.indexOf(marker);
    if (ix == -1) {
        return QString();
    }
    return m_markers.at(ix).comment();
}

QPoint SmallRuler::zone() const
{
    return QPoint(m_zoneStart, m_zoneEnd);
}

// virtual
void SmallRuler::mousePressEvent(QMouseEvent *event)
{
    m_render->setActiveMonitor();
    if (event->y() <= m_rulerHeight) {
        const int framePosition = event->x() / m_scale + m_offset;
        if (framePosition != m_lastSeekPosition && framePosition != m_cursorFramePosition) {
            m_render->seekToFrame(framePosition);
            m_lastSeekPosition = framePosition;
            m_activeControl = CONTROL_HEAD;
        }
    } else if (m_hoverZone != -1) {
        if (m_hoverZone == m_zoneStart) {
            m_activeControl = CONTROL_IN;
        } else if (m_hoverZone == m_zoneEnd) {
            m_activeControl = CONTROL_OUT;
        }
    } else {
        m_activeControl = CONTROL_NONE;
    }
    event->accept();
    update();
}

// virtual
void SmallRuler::mouseReleaseEvent(QMouseEvent *event)
{
    event->accept();
    if (m_activeControl == CONTROL_IN || m_activeControl == CONTROL_OUT) {
        prepareZoneUpdate();
    }
}

void SmallRuler::prepareZoneUpdate()
{
    if (m_zoneEnd < m_zoneStart) {
        int end = m_zoneEnd;
        m_zoneEnd = m_zoneStart;
        m_zoneStart = end;
        update();
    }
    emit zoneChanged(QPoint(m_zoneStart, m_zoneEnd));
}

// virtual
void SmallRuler::mouseMoveEvent(QMouseEvent *event)
{
    const int pos = event->x() / m_scale + m_offset;
    if (event->buttons() & Qt::LeftButton) {
        if (m_activeControl == CONTROL_NONE) {
            return;
        }
        if (m_activeControl == CONTROL_IN) {
            m_zoneStart = pos;
            m_render->seekToFrame(pos);
            m_lastSeekPosition = pos;
        } else if (m_activeControl == CONTROL_OUT) {
            m_zoneEnd = pos;
            m_render->seekToFrame(pos);
            m_lastSeekPosition = pos;
        } else if (pos != m_lastSeekPosition && pos != m_cursorFramePosition) {
            m_render->seekToFrame(pos);
            m_lastSeekPosition = pos;
        }
        update();
    } else {
        if (m_zoneStart != m_zoneEnd && event->y() > m_rulerHeight && event->y() < height()) {
            int hoverZone = -1;
            if (qAbs((pos - m_zoneStart) * m_scale) < 6) {
                hoverZone = m_zoneStart;
                setToolTip(i18n("Zone start: %1", m_monitor->getTimecodeFromFrames(m_zoneStart)));
            } else if (qAbs((pos - m_zoneEnd) * m_scale) < 6) {
                hoverZone = m_zoneEnd;
                setToolTip(i18n("Zone end: %1", m_monitor->getTimecodeFromFrames(m_zoneEnd)));
            }
            if (m_hoverZone != hoverZone) {
                setCursor(hoverZone == -1 ? Qt::ArrowCursor : Qt::PointingHandCursor);
                m_hoverZone = hoverZone;
                update();
            }
            return;
        } else {
            if (m_hoverZone != -1) {
                setCursor(Qt::ArrowCursor);
                m_hoverZone = -1;
                update();
            }
        }
        for (int i = 0; i < m_markers.count(); ++i) {
            if (qAbs((pos - m_markers.at(i).time().frames(m_monitor->fps())) * m_scale) < 4) {
                // We are on a marker
                QString markerxt = m_monitor->getMarkerThumb(m_markers.at(i).time());
                if (!markerxt.isEmpty()) {
                    markerxt.prepend(QStringLiteral("<img src=\""));
                    markerxt.append(QStringLiteral("\"><p align=\"center\">"));
                }
                markerxt.append(m_markers.at(i).comment());
                setToolTip(markerxt);
                event->accept();
                return;
            }
        }
        if (pos > m_zoneStart && pos < m_zoneEnd) {
            setToolTip(i18n("Zone duration: %1", m_monitor->getTimecodeFromFrames(m_zoneEnd - m_zoneStart)));
        } else {
            setToolTip(i18n("Position: %1", m_monitor->getTimecodeFromFrames(pos)));
        }
    }
    event->accept();
}

void SmallRuler::refreshRuler()
{
    m_lastSeekPosition = SEEK_INACTIVE;
    update();
}

bool SmallRuler::slotNewValue(int value)
{
    if (value == m_cursorFramePosition) {
        return false;
    }
    if (value == m_lastSeekPosition) {
        m_lastSeekPosition = SEEK_INACTIVE;
    }
    m_cursorFramePosition = value;
    update();
    return true;
}

//virtual
void SmallRuler::resizeEvent(QResizeEvent *)
{
    adjustScale(m_maxval, m_offset);
}

void SmallRuler::updatePixmap()
{
    m_pixmap = QPixmap(width(), height());
    m_lastSeekPosition = SEEK_INACTIVE;
    m_pixmap.fill(Qt::transparent);//palette().alternateBase().color());
    QPainter p(&m_pixmap);
    p.fillRect(0, 0, width(), m_rulerHeight, palette().midlight().color());
    p.setPen(palette().dark().color());
    p.drawLine(0, m_rulerHeight, width(), m_rulerHeight);
    p.drawLine(0, 0, width(), 0);
    double f;
    p.setPen(palette().dark().color());
    m_smallMarkSteps = m_scale * m_small;
    m_mediumMarkSteps = m_scale * m_medium;
    QLineF line(0, 1, 0, m_rulerHeight / 3);
    if (m_smallMarkSteps > 2) {
        for (f = 0; f < width(); f += m_smallMarkSteps) {
            line.translate(m_smallMarkSteps, 0);
            p.drawLine(line);
        }
    }
    line.setLine(0, 1, 0, m_rulerHeight / 2);
    if (m_mediumMarkSteps > 2 && m_mediumMarkSteps < width() / 2.5) {
        for (f = 0; f < width(); f += m_mediumMarkSteps) {
            line.translate(m_mediumMarkSteps, 0);
            p.drawLine(line);
        }
    }
    foreach (const CommentedTime &marker, m_markers) {
        double pos = (marker.time().frames(m_monitor->fps()) - m_offset) * m_scale;
        p.setPen(CommentedTime::markerColor(marker.markerType()));
        line.setLine(pos, 1, pos, m_rulerHeight - 1);
        p.drawLine(line);
    }

    p.end();
    update();
}

// virtual
void SmallRuler::paintEvent(QPaintEvent *event)
{
    QPainter p(this);
    p.setClipRect(event->rect());
    p.drawPixmap(QPointF(), m_pixmap);
    QPolygon pointer(3);
    int cursorPos = (m_cursorFramePosition - m_offset) * m_scale;
    pointer.setPoints(3,
                      cursorPos - FONT_WIDTH, m_rulerHeight,
                      cursorPos + FONT_WIDTH, m_rulerHeight,
                      cursorPos, FONT_WIDTH);
    p.setBrush(palette().text().color());
    // Draw zone
    if (m_zoneStart != m_zoneEnd) {
        QPen pen;
        pen.setBrush(palette().text());
        //pen.setWidth(2);
        QPen activePen;
        QColor select = palette().highlight().color();
        select.setAlpha(100);
        activePen.setBrush(palette().highlight());
        //activePen.setWidth(2);
        p.setPen(pen);
        const int zoneStart = (int)((m_zoneStart - m_offset) * m_scale);
        const int zoneEnd = (int)((m_zoneEnd - m_offset) * m_scale);
        if (zoneStart > 0 || zoneEnd < width()) {
            p.fillRect(zoneStart, 1, zoneEnd - zoneStart, m_rulerHeight - 1, select);
        }
        if (QWidget::underMouse()) {
            QRectF rect(0, 0, m_rulerHeight / 2, m_rulerHeight / 2);
            QLineF line;
            line.setLine(zoneStart, 1, zoneStart, m_rulerHeight + 1);
            if (m_hoverZone == m_zoneStart || m_activeControl == CONTROL_IN) {
                p.setPen(activePen);
            }
            p.drawLine(line);
            rect.moveTo(zoneStart - m_rulerHeight / 4, m_rulerHeight + 2);
            p.drawArc(rect, 0, 5760);
            p.setPen(m_hoverZone == m_zoneEnd || m_activeControl == CONTROL_OUT ? activePen : pen);
            line.translate(zoneEnd - zoneStart, 0);
            p.drawLine(line);
            rect.moveTo(zoneEnd - m_rulerHeight / 4, m_rulerHeight + 2);
            p.drawArc(rect, 0, 5760);
        }
    }
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(Qt::NoPen);
    p.drawPolygon(pointer);

    // Draw seeking pointer
    if (m_lastSeekPosition != SEEK_INACTIVE && m_lastSeekPosition != m_cursorFramePosition) {
        p.fillRect((m_lastSeekPosition - m_offset) * m_scale - 1, 0, 3, m_rulerHeight, palette().linkVisited());
    }
}

void SmallRuler::enterEvent(QEvent *event)
{
    event->accept();
    update();
}

void SmallRuler::leaveEvent(QEvent *event)
{
    event->accept();
    if (m_activeControl == CONTROL_IN) {
        m_zoneStart = qMax(0, m_zoneStart);
    }
    if (m_activeControl == CONTROL_OUT) {
        m_zoneStart = qMin(m_maxval, m_zoneStart);
    }
    if (m_activeControl == CONTROL_IN || m_activeControl == CONTROL_OUT) {
        prepareZoneUpdate();
        setCursor(Qt::ArrowCursor);
    }
    m_hoverZone = -1;
    m_activeControl = CONTROL_NONE;
    update();
}

void SmallRuler::updatePalette()
{
    updatePixmap();
}

