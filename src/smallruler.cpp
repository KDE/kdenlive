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

#include <KDebug>
#include <KColorScheme>

#include <QMouseEvent>
#include <QStylePainter>


SmallRuler::SmallRuler(MonitorManager *manager, QWidget *parent) :
        QWidget(parent),
        m_scale(1),
        m_maxval(25),
        m_manager(manager)
{
    m_zoneStart = 10;
    m_zoneEnd = 60;
    m_zoneColor = KStatefulBrush(KColorScheme::View, KColorScheme::PositiveBackground, KSharedConfig::openConfig(KdenliveSettings::colortheme())).brush(this).color();
    setMouseTracking(true);
    setMinimumHeight(10);
}

void SmallRuler::adjustScale(int maximum)
{
    m_maxval = maximum;
    m_scale = (double) width() / (double) maximum;
    if (m_scale == 0) m_scale = 1;

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
    m_cursorPosition = m_cursorFramePosition * m_scale;
    updatePixmap();
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
        } else m_zoneStart = start;
    }
    if (end != -1) {
        if (start != -1 && end <= start) {
            m_zoneStart = qMax(0, start - (m_zoneEnd - end));
            m_zoneEnd = end;
        } else if (start == -1 && end <= m_zoneStart) {
            m_zoneStart = qMax(0, m_zoneStart - (m_zoneEnd - end));
            m_zoneEnd = end;
        } else m_zoneEnd = end;
    }
    updatePixmap();
}

void SmallRuler::setMarkers(QList < int > list)
{
    m_markers = list;
    updatePixmap();
}

QPoint SmallRuler::zone()
{
    return QPoint(m_zoneStart, m_zoneEnd);
}

// virtual
void SmallRuler::mousePressEvent(QMouseEvent * event)
{
    const int pos = event->x() / m_scale;
    if (event->button() == Qt::RightButton) {
        // Right button clicked, move selection zone
        if (qAbs(pos - m_zoneStart) < qAbs(pos - m_zoneEnd)) m_zoneStart = pos;
        else m_zoneEnd = pos;
        emit zoneChanged(QPoint(m_zoneStart, m_zoneEnd));
        updatePixmap();

    } else emit seekRenderer((int) pos);
}

// virtual
void SmallRuler::mouseMoveEvent(QMouseEvent * event)
{
    const int pos = event->x() / m_scale;
    if (event->buttons() & Qt::LeftButton) emit seekRenderer((int) pos);
    else {
        if (qAbs((pos - m_zoneStart) * m_scale) < 4) {
            setToolTip(i18n("Zone start: %1", m_manager->timecode().getTimecodeFromFrames(m_zoneStart)));
        } else if (qAbs((pos - m_zoneEnd) * m_scale) < 4) {
            setToolTip(i18n("Zone end: %1", m_manager->timecode().getTimecodeFromFrames(m_zoneEnd)));
        } else if (pos > m_zoneStart && pos < m_zoneEnd) {
            setToolTip(i18n("Zone duration: %1", m_manager->timecode().getTimecodeFromFrames(m_zoneEnd - m_zoneStart)));
        } else setToolTip(i18n("Position: %1", m_manager->timecode().getTimecodeFromFrames(pos)));
    }
}

bool SmallRuler::slotNewValue(int value)
{
    if (value == m_cursorFramePosition) return false;
    m_cursorFramePosition = value;
    int oldPos = m_cursorPosition;
    m_cursorPosition = value * m_scale;
    const int offset = 6;
    const int x = qMin(oldPos, m_cursorPosition);
    const int w = qAbs(oldPos - m_cursorPosition);
    update(x - offset, 4, w + 2 * offset, 6);
    return true;
}

//virtual
void SmallRuler::resizeEvent(QResizeEvent *)
{
    adjustScale(m_maxval);
}

void SmallRuler::updatePixmap()
{
    m_pixmap = QPixmap(width(), height());
    m_pixmap.fill(palette().window().color());
    QPainter p(&m_pixmap);
    double f, fend;

    const int zoneStart = (int)(m_zoneStart * m_scale);
    const int zoneEnd = (int)(m_zoneEnd * m_scale);
    p.fillRect(zoneStart, height() / 2 - 1, zoneEnd - zoneStart, height() / 2, m_zoneColor);

    // draw ruler
    p.setPen(palette().text().color());
    // draw the little marks
    fend = m_scale * m_small;
    if (fend > 2) for (f = 0; f < width(); f += fend) {
        p.drawLine((int)f, 0, (int)f, 3);
    }

    // draw medium marks
    fend = m_scale * m_medium;
    if (fend > 2) for (f = 0; f < width(); f += fend) {
	p.drawLine((int)f, 0, (int)f, 6);
    }
    // draw markers
    if (!m_markers.isEmpty()) {
        p.setPen(Qt::red);
        for (int i = 0; i < m_markers.count(); i++) {
            p.drawLine(m_markers.at(i) * m_scale, 0, m_markers.at(i) * m_scale, 9);
        }
    }
    p.end();
    update();
}

// virtual
void SmallRuler::paintEvent(QPaintEvent *e)
{

    QPainter p(this);
    QRect r = e->rect();
    p.setClipRect(r);
    p.drawPixmap(QPointF(), m_pixmap);

    // draw pointer
    QPolygon pa(3);
    pa.setPoints(3, m_cursorPosition - 5, 10, m_cursorPosition + 5, 10, m_cursorPosition/*+0*/, 5);
    p.setBrush(palette().text().color());
    p.setPen(Qt::NoPen);
    p.drawPolygon(pa);
}

#include "smallruler.moc"
