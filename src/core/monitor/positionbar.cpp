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
#include <QPainter>
#include <QPaintEvent>


PositionBar::PositionBar(QWidget* parent) :
    QWidget(parent),
    m_duration(0),
    m_position(0)
{
    setMouseTracking(true);
    setMinimumHeight(10);
}

PositionBar::~PositionBar()
{
}

int PositionBar::position() const
{
    return m_position;
}

void PositionBar::setPosition(int position)
{
    m_position = position;
    m_pixelPosition = position * m_scale;
    update();
}

void PositionBar::setDuration(int duration)
{
    m_duration = duration;
    adjustScale();
}

void PositionBar::mousePressEvent(QMouseEvent* event)
{
    const int framePosition = event->x() / m_scale;
    emit positionChanged(framePosition);
}

void PositionBar::mouseMoveEvent(QMouseEvent* event)
{
    const int framePosition = event->x() / m_scale;
    if (event->button() == Qt::NoButton) {
        if (qAbs(event->x() - m_pixelPosition) < 6) {
            if (!m_mouseOverCursor) {
                m_mouseOverCursor = true;
                update();
            }
        } else if (m_mouseOverCursor) {
            m_mouseOverCursor = false;
            update();
        }
    }
    
    if (event->buttons() & Qt::LeftButton) {
        m_mouseOverCursor = true;
        emit positionChanged(framePosition);
    } else {
        setToolTip(i18n("Position: %1", Timecode(framePosition).formatted()));
    }
}

void PositionBar::leaveEvent(QEvent* event)
{
    Q_UNUSED(event)
    if (m_mouseOverCursor) {
        m_mouseOverCursor = false;
        update();
    }
}

void PositionBar::paintEvent(QPaintEvent* event)
{
    QPainter p(this);

    p.setClipRect(event->rect());
    p.drawPixmap(QPointF(), m_background);

    QPolygon pointer(3);
    pointer.setPoints(3,
                      m_pixelPosition - 6, 10,
                      m_pixelPosition + 6, 10,
                      m_pixelPosition    , 4);
    if (m_mouseOverCursor) {
        p.setBrush(palette().highlight());
    } else {
        p.setBrush(palette().text().color());
    }

    p.setPen(Qt::NoPen);
    p.drawPolygon(pointer);
}

void PositionBar::resizeEvent(QResizeEvent* event)
{
    adjustScale();
    QWidget::resizeEvent(event);
}

void PositionBar::updateBackground()
{
    m_background = QPixmap(width(), height());
    m_background.fill(palette().window().color());

    QPainter p(&m_background);

    p.setPen(palette().text().color());

    double f;

    if (m_smallMarkSteps > 2) {
        for (f = 0; f < width(); f += m_smallMarkSteps) {
            p.drawLine(f, 0, f, 3);
        }
    }

    if (m_mediumMarkSteps > 2) {
        for (f = 0; f < width(); f += m_mediumMarkSteps) {
            p.drawLine(f, 0, f, 6);
        }
    }

    p.end();

    update();
}

void PositionBar::adjustScale()
{
    m_scale = (double) width() / (double) m_duration;
    if (m_scale == 0) {
        m_scale = 1;
    }

    QList<int> levels = QList<int>() << 25
                                     << 5 * 25
                                     << 30 * 25
                                     << 60 * 25;
    int offset;
    if (m_scale > 0.5) {
        offset = 0;
    } else if (m_scale > 0.09) {
        offset = 1;
    } else {
        offset = 2;
    }

    m_smallMarkSteps = levels.at(offset) * m_scale;
    m_mediumMarkSteps = levels.at(offset + 1) * m_scale;

    m_pixelPosition = m_position * m_scale;

    updateBackground();
}
