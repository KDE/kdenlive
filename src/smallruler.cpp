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
#include <KLocalizedString>

#include <QMouseEvent>
#include <QStylePainter>

#define SEEK_INACTIVE (-1)


SmallRuler::SmallRuler(Monitor *monitor, Render *render, QWidget *parent) :
        QWidget(parent)
        ,m_cursorFramePosition(0)
        ,m_maxval(2)
        ,m_monitor(monitor)
	,m_render(render)
	,m_lastSeekPosition(SEEK_INACTIVE)
	,m_cursorColor(palette().text())
{
    m_zoneStart = 10;
    m_zoneEnd = 60;
    KSharedConfigPtr config = KSharedConfig::openConfig(KdenliveSettings::colortheme());
    m_zoneColor = KStatefulBrush(KColorScheme::View, KColorScheme::FocusColor, config).brush(this).color();
    m_zoneColor.setAlpha(180);

    setMouseTracking(true);
    setMinimumHeight(QFontInfo(font()).pixelSize());
    adjustScale(m_maxval);
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
    updatePixmap();
}

void SmallRuler::setZoneStart()
{
    int pos = m_render->requestedSeekPosition;
    if (pos == SEEK_INACTIVE) pos = m_render->seekFramePosition();
    setZone(pos, -1);
}

void SmallRuler::setZoneEnd()
{
    int pos = m_render->requestedSeekPosition;
    if (pos == SEEK_INACTIVE) pos = m_render->seekFramePosition();
    setZone(-1, pos);
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

void SmallRuler::setMarkers(const QList<CommentedTime> &list)
{
    m_markers = list;
    updatePixmap();
}

QPoint SmallRuler::zone() const
{
    return QPoint(m_zoneStart, m_zoneEnd);
}

// virtual
void SmallRuler::mousePressEvent(QMouseEvent * event)
{
    m_render->setActiveMonitor();
    const int pos = event->x() / m_scale;
    if (event->button() == Qt::RightButton) {
        // Right button clicked, move selection zone
        if (qAbs(pos - m_zoneStart) < qAbs(pos - m_zoneEnd)) m_zoneStart = pos;
        else m_zoneEnd = pos;
        emit zoneChanged(QPoint(m_zoneStart, m_zoneEnd));
        updatePixmap();

    } else if (pos != m_lastSeekPosition && pos != m_cursorFramePosition) {
	m_render->seekToFrame(pos);
	m_lastSeekPosition = pos;
	update();
    }
    event->accept();
}

// virtual
void SmallRuler::mouseReleaseEvent(QMouseEvent * event)
{
    event->accept();
}


// virtual
void SmallRuler::leaveEvent(QEvent * event)
{
    QWidget::leaveEvent(event);
    if (m_cursorColor == palette().link()) {
	m_cursorColor = palette().text();
	update();
    }
}

// virtual
void SmallRuler::mouseMoveEvent(QMouseEvent * event)
{
    const int pos = event->x() / m_scale;
    if (event->buttons() & Qt::LeftButton) {
	if (pos != m_lastSeekPosition && pos != m_cursorFramePosition) {
	    m_render->seekToFrame(pos);
	    m_lastSeekPosition = pos;
	    update();
	}
    }
    else {
	if (m_cursorColor == palette().text() && qAbs(pos - m_cursorFramePosition) * m_scale < 7) {
	    // Mouse is over cursor
	    m_cursorColor = palette().link();
	    update();
	}
	else if (m_cursorColor == palette().link() && qAbs(pos - m_cursorFramePosition) * m_scale >= 7) {
	    m_cursorColor = palette().text();
	    update();
	}
        if (qAbs((pos - m_zoneStart) * m_scale) < 4) {
            setToolTip(i18n("Zone start: %1", m_monitor->getTimecodeFromFrames(m_zoneStart)));
        } else if (qAbs((pos - m_zoneEnd) * m_scale) < 4) {
            setToolTip(i18n("Zone end: %1", m_monitor->getTimecodeFromFrames(m_zoneEnd)));
        } 
	for (int i = 0; i < m_markers.count(); ++i) {
	    if (qAbs((pos - m_markers.at(i).time().frames(m_monitor->fps())) * m_scale) < 4) {
		// We are on a marker
		QString markerxt = m_monitor->getMarkerThumb(m_markers.at(i).time());
		if (!markerxt.isEmpty()) {
		    markerxt.prepend("<img src=\"");
		    markerxt.append("\"><p align=\"center\">");
		}
		markerxt.append(m_markers.at(i).comment());
		setToolTip(markerxt);
		event->accept();
		return;
	    }
	}
	if (pos > m_zoneStart && pos < m_zoneEnd) {
            setToolTip(i18n("Zone duration: %1", m_monitor->getTimecodeFromFrames(m_zoneEnd - m_zoneStart)));
	}
	else setToolTip(i18n("Position: %1", m_monitor->getTimecodeFromFrames(pos)));
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
    if (value == m_cursorFramePosition) return false;
    if (value == m_lastSeekPosition) m_lastSeekPosition = SEEK_INACTIVE;
    m_cursorFramePosition = value;
    update();
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
    m_lastSeekPosition = SEEK_INACTIVE;
    QPainter p(&m_pixmap);
    double f, fend;

    const int zoneStart = (int)(m_zoneStart * m_scale);
    const int zoneEnd = (int)(m_zoneEnd * m_scale);
    p.fillRect(zoneStart, 0, zoneEnd - zoneStart, height(), QBrush(m_zoneColor));

    // draw ruler
    p.setPen(palette().midlight().color());
    //p.drawLine(0, 0, width(), 0);
    p.drawLine(0, height() - 1, width(), height() - 1);
    p.setPen(palette().dark().color());
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
        for (int i = 0; i < m_markers.count(); ++i) {
	    int pos = m_markers.at(i).time().frames(m_monitor->fps()) * m_scale;
	    p.setPen(CommentedTime::markerColor(m_markers.at(i).markerType()));
            p.drawLine(pos, 0, pos, 9);
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
    p.setPen(palette().shadow().color());
    //p.setRenderHint(QPainter::Antialiasing, true);
    //p.drawRoundedRect(rect().adjusted(1,1,-2,-2), 3, 3);

    int cursorPos = m_cursorFramePosition * m_scale;
    // draw pointer
    QPolygon pa(3);
    pa.setPoints(3, cursorPos - 6, height() - 1, cursorPos + 6, height() - 1, cursorPos/*+0*/, height() - 8);
    p.setBrush(m_cursorColor);
    p.setPen(Qt::NoPen);
    p.drawPolygon(pa);

    // Draw seeking pointer
    if (m_lastSeekPosition != SEEK_INACTIVE && m_lastSeekPosition != m_cursorFramePosition) {
	p.fillRect(m_lastSeekPosition * m_scale - 1, 0, 3, height(), palette().linkVisited());
    }
}

void SmallRuler::updatePalette()
{
    KSharedConfigPtr config = KSharedConfig::openConfig(KdenliveSettings::colortheme());
    m_zoneColor = KStatefulBrush(KColorScheme::View, KColorScheme::FocusColor, config).brush(this).color();
    m_zoneColor.setAlpha(180);
    updatePixmap();
}

#include "smallruler.moc"
