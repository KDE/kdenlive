/*
Copyright (C) 2012  Till Theato <root@ttill.de>
This file is part of kdenlive. See www.kdenlive.org.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
*/

#include "positionbar.h"
#include "timecode.h"
#include <KLocale>
#include <KDebug>
#include <QBuffer>
#include <QPainter>
#include <QPaintEvent>


PositionBar::PositionBar(QWidget* parent) :
    QWidget(parent)
    , m_duration(0)
    , m_fps(25)
    , m_position(0)
    , m_hoverMarker(-1)
    , m_activeControl(CONTROL_NONE)
{
    setMouseTracking(true);
    m_rulerHeight = 11;
    setMinimumHeight(24);
    m_markers = QList <int>();
    m_markers << 80 << 120;
}

PositionBar::~PositionBar()
{
}

int PositionBar::position() const
{
    return m_head;
}

void PositionBar::setSeekPos(int position)
{
    m_head = position;
    update();
}

void PositionBar::setPosition(int position)
{
    if (m_activeControl != CONTROL_HEAD) {
	m_head = position;
    }
    m_position = position;
    m_pixelPosition = position * m_scale;
    update();
}

int PositionBar::duration() const
{
    return m_duration;
}

void PositionBar::setDuration(int duration, double fps)
{
    m_duration = duration;
    m_fps = fps;
    adjustScale();
}

void PositionBar::mousePressEvent(QMouseEvent* event)
{
    if (event->y() <= m_rulerHeight) {
	const int framePosition = event->x() / m_scale;
	event->accept();
	m_head = framePosition;
	m_activeControl = CONTROL_HEAD;
	emit positionChanged(framePosition);
    }
    else if (m_hoverMarker != -1) {
	if (m_hoverMarker == m_zone.x())
	    m_activeControl = CONTROL_IN;
	else if (m_hoverMarker == m_zone.y())
	    m_activeControl = CONTROL_OUT;
    }
    else m_activeControl = CONTROL_NONE;
    update();
}

void PositionBar::mouseReleaseEvent(QMouseEvent* event)
{
    event->accept();
    m_activeControl = CONTROL_NONE;
    m_hoverMarker = -1;
}

void PositionBar::slotSetThumbnail(int position, QImage img)
{
    if (img.isNull()) {
	setToolTip(Timecode(position).formatted());
	return;
    }
    QByteArray ba;
    QBuffer buffer(&ba);
    buffer.open(QIODevice::WriteOnly);
    img.save(&buffer, "PNG");
    setToolTip(QString("<p align=\"center\"><img src=\"data:image/png;base64,%1\"><br>%2").arg(QString(buffer.data().toBase64())).arg(Timecode(position).formatted()));
}

void PositionBar::mouseMoveEvent(QMouseEvent* event)
{
    event->accept();
    const int framePosition = qBound(0, event->x(), rect().width()) / m_scale;
    if (event->buttons() & Qt::LeftButton) {
	if (m_activeControl == CONTROL_NONE) return;
        m_mouseOverCursor = true;
	m_head = framePosition;
	if (m_activeControl == CONTROL_IN) {
	    m_zone.setX(framePosition);
	}
	else if (m_activeControl == CONTROL_OUT) {
	    m_zone.setY(framePosition);
	}
        emit positionChanged(framePosition);
	update();
    }
    else {
	if (!m_zone.isNull() && event->y() > m_rulerHeight && event->y() < height()) {
		int hoverMarker = -1;
		if (qAbs(event->x() - m_zone.x() * m_scale) < 6) {
			hoverMarker = m_zone.x();
		}
		else if (qAbs(event->x() - m_zone.y() * m_scale) < 6) {
			hoverMarker = m_zone.y();
		}
		if (m_hoverMarker != hoverMarker) {
		    setCursor(hoverMarker == -1 ? Qt::ArrowCursor : Qt::PointingHandCursor);
		    if (hoverMarker != -1) 
			emit requestThumb(hoverMarker);
		    else {
			// How can we clear tooltip properly?
			setToolTip(QString(" "));
		    }
		    m_hoverMarker = hoverMarker;
		    update();
		}
		return;
	}
	else {
	    if (m_hoverMarker != -1) {
		setCursor(Qt::ArrowCursor);
		m_hoverMarker = -1;
		update();
	    }
	}
        if (qAbs(event->x() - m_pixelPosition) < 6) {
            if (!m_mouseOverCursor) {
                m_mouseOverCursor = true;
                update();
            }
        } else if (m_mouseOverCursor) {
            m_mouseOverCursor = false;
            update();
        }
        //if (isActiveWindow() && m_head == m_position) emit requestThumb(framePosition);
    }
}

void PositionBar::enterEvent(QEvent* event)
{
    event->accept();
    update();
}

void PositionBar::leaveEvent(QEvent* event)
{
    event->accept();
    if (m_mouseOverCursor || m_hoverMarker != -1) {
        m_mouseOverCursor = false;
	m_hoverMarker = -1;
    }
    update();
}

void PositionBar::paintEvent(QPaintEvent* event)
{
    QPainter p(this);

    p.setClipRect(event->rect());
    p.drawPixmap(QPointF(), m_background);

    QPolygon pointer(3);
    pointer.setPoints(3,
                      m_pixelPosition - 6, m_rulerHeight,
                      m_pixelPosition + 6, m_rulerHeight,
                      m_pixelPosition    , 5);
    if (m_mouseOverCursor) {
        p.setBrush(palette().highlight());
    } else {
        p.setBrush(palette().text().color());
    }
    
    // Draw zone
    if (!m_zone.isNull()) {
	QPen pen;
	pen.setBrush(QColor(255, 90, 0));
	pen.setWidth(2);
	QPen activePen;
	activePen.setBrush(palette().highlight());
	activePen.setWidth(2);
	p.setPen(pen);
	p.fillRect(m_zone.x() * m_scale, 1, (m_zone.y() - m_zone.x()) * m_scale, m_rulerHeight, QColor(255, 90, 0, 100));
	
	if (QWidget::underMouse()) {
	    QRectF rect(0, 0, 6, 6);
	    QLineF line;
	    line.setLine(m_zone.x() * m_scale, 1, m_zone.x() * m_scale, 15);
	    if (m_hoverMarker == m_zone.x())
		p.setPen(activePen);
	    p.drawLine(line);
	    rect.moveTo(m_zone.x() * m_scale - 3, 15);
	    p.drawArc(rect, 0, 5760);
    
	    p.setPen(m_hoverMarker == m_zone.y() ? activePen : pen);
	    line.translate((m_zone.y() - m_zone.x()) * m_scale, 0);
	    p.drawLine(line);
	    rect.moveTo(m_zone.y() * m_scale - 3, 15);
	    p.drawArc(rect, 0, 5760);
	}
    }
    
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(Qt::NoPen);
    p.drawPolygon(pointer);
    p.fillRect(m_head * m_scale - 1, 0, 2, m_rulerHeight, palette().text());
}

void PositionBar::resizeEvent(QResizeEvent* event)
{
    adjustScale();
    QWidget::resizeEvent(event);
}

void PositionBar::updateBackground()
{
    m_background = QPixmap(width(), height());
    m_background.fill(Qt::transparent);//palette().alternateBase().color());
    QPainter p(&m_background);
    p.fillRect(0, 0, width(), m_rulerHeight, palette().midlight().color());
    p.setPen(palette().mid().color());
    p.drawLine(0, m_rulerHeight - 1, width(), m_rulerHeight - 1);
    p.setPen(palette().dark().color());
    p.drawLine(0, 0, width(), 0);

    double f;
    QLineF line(0, 1, 0, 4);
    if (m_smallMarkSteps > 2) {
        for (f = 0; f < width(); f += m_smallMarkSteps) {
	    line.translate(m_smallMarkSteps, 0);
            p.drawLine(line);
        }
    }
    line.setLine(0, 1, 0, 7);
    if (m_mediumMarkSteps > 2 && m_mediumMarkSteps < width() / 2.5) {
        for (f = 0; f < width(); f += m_mediumMarkSteps) {
            line.translate(m_mediumMarkSteps, 0);
            p.drawLine(line);
        }
    }
    
    p.end();

    update();
}

void PositionBar::setZone(QPoint zone)
{
    m_zone = zone;
    update();
}

void PositionBar::adjustScale()
{
    m_scale = (double) width() / (double) m_duration;
    if (m_scale == 0) {
        m_scale = 1;
    }

    QList<double> levels = QList<double>() << 1
				     << m_fps
                                     << 5 * m_fps
                                     << 30 * m_fps
                                     << 60 * m_fps;
    int offset;
    if (m_scale > 5) {
	offset = 0;
    }
    else if (m_scale > 1) {
	offset = -1;
    }
    else if (m_scale > 0.5) {
        offset = 1;
    } else if (m_scale > 0.09) {
        offset = 2;
    } else {
        offset = 3;
    }

    // Don't show small marks when zooming a lot
    if (offset < 0) {
	m_smallMarkSteps = -1;
	m_mediumMarkSteps = levels.at(-offset) * m_scale;
    }
    else {
	m_smallMarkSteps = levels.at(offset) * m_scale;
	m_mediumMarkSteps = levels.at(offset + 1) * m_scale;
    }

    m_pixelPosition = m_position * m_scale;

    updateBackground();
}
