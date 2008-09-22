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


#include <QMouseEvent>
#include <QStylePainter>

#include <KDebug>

#include "smallruler.h"


SmallRuler::SmallRuler(QWidget *parent)
        : QWidget(parent), m_scale(1), m_maxval(25) {
    m_zoneStart = 10;
    m_zoneEnd = 60;
}

void SmallRuler::adjustScale(int maximum) {
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
    update();
}

void SmallRuler::setZone(int start, int end) {
    if (start != -1) {
        if (end != -1 && start >= end) return;
        else if (end == -1 && start >= m_zoneEnd) return;
        m_zoneStart = start;
    }
    if (end != -1) {
        if (start != -1 && end <= start) end = m_zoneEnd;
        else if (start == -1 && end <= m_zoneStart) end = m_zoneEnd;
        m_zoneEnd = end;
    }
    update();
}

QPoint SmallRuler::zone() {
    return QPoint(m_zoneStart, m_zoneEnd);
}

// virtual
void SmallRuler::mousePressEvent(QMouseEvent * event) {
    const int pos = event->x() / m_scale;
    emit seekRenderer((int) pos);
}

// virtual
void SmallRuler::mouseMoveEvent(QMouseEvent * event) {
    const int pos = event->x() / m_scale;
    emit seekRenderer((int) pos);
}

void SmallRuler::slotNewValue(int value) {
    m_cursorFramePosition = value;
    int oldPos = m_cursorPosition;
    m_cursorPosition = value * m_scale;
    if (oldPos == m_cursorPosition) return;
    const int offset = 6;
    const int x = qMin(oldPos, m_cursorPosition);
    const int w = qAbs(oldPos - m_cursorPosition);
    update(x - offset, 9, w + 2 * offset, 6);
}

//virtual
void SmallRuler::resizeEvent(QResizeEvent *) {
    adjustScale(m_maxval);
}

// virtual
void SmallRuler::paintEvent(QPaintEvent *e) {

    QPainter p(this);
    QRect r = e->rect();
    p.setClipRect(r);

    double f, fend;
    p.setPen(palette().dark().color());

    const int zoneStart = (int)(m_zoneStart * m_scale);
    const int zoneEnd = (int)(m_zoneEnd * m_scale);

    p.fillRect(QRect(zoneStart, height() / 2, zoneEnd - zoneStart, height() / 2), QBrush(QColor(133, 255, 143)));

    if (r.top() < 9) {
        // draw the little marks
        fend = m_scale * m_small;
        if (fend > 2) for (f = 0; f < width(); f += fend) {
                p.drawLine((int)f, 1, (int)f, 3);
            }

        // draw medium marks
        fend = m_scale * m_medium;
        if (fend > 2) for (f = 0; f < width(); f += fend) {
                p.drawLine((int)f, 1, (int)f, 5);
            }
    }

    // draw pointer
    QPolygon pa(3);
    pa.setPoints(3, m_cursorPosition - 5, 14, m_cursorPosition + 5, 14, m_cursorPosition/*+0*/, 9);
    p.setBrush(palette().dark().color());
    p.drawPolygon(pa);
}

#include "smallruler.moc"
