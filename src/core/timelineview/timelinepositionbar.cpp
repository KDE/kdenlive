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

#include "timelinepositionbar.h"
#include "core.h"
#include "project/projectmanager.h"
#include "project/project.h"
#include "timecodeformatter.h"
#include "timecode.h"
#include "project/timeline.h"
#include "project/producerwrapper.h"
#include "monitor/monitorview.h"
#include <KGlobalSettings>
#include <KLocale>
#include <KDebug>
#include <QMouseEvent>
#include <QPainter>


const int TimelinePositionBar::comboScale[] = { 1, 2, 5, 10, 25, 50, 125, 250, 500, 750, 1500, 3000, 6000, 12000 };


TimelinePositionBar::TimelinePositionBar(QWidget *parent) :
    QWidget(parent),
    m_timecodeFormatter(NULL),
    m_duration(0),
    m_offset(0),
    m_playbackPosition(0),
    m_zoomLevel(0)
    , m_monitorId(ProjectMonitor)
{
    setFont(KGlobalSettings::toolBarFont());
    QFontMetricsF fontMetrics(font());
    m_labelSize = fontMetrics.ascent();

    setMinimumHeight(m_labelSize * 2);
    setMaximumHeight(m_labelSize * 2);

    m_bigMarkX = m_labelSize + 1;
    int mark_length = height() - m_bigMarkX;
    m_middleMarkX = m_bigMarkX + mark_length / 2;
    m_smallMarkX = m_bigMarkX + mark_length / 3;

    setMouseTracking(true);

    connect(pCore->projectManager(), SIGNAL(projectOpened(Project*)), this, SLOT(setProject(Project*)));
}

void TimelinePositionBar::setDuration(int duration)
{
    int oldduration = m_duration;
    m_duration = duration;
    update(qMin(oldduration, m_duration) * m_factor - 1 - m_offset, 0, qAbs(oldduration - m_duration) * m_factor + 2, height());
}

int TimelinePositionBar::offset() const
{
    return m_offset;
}

void TimelinePositionBar::setProject(Project* project, MONITORID id)
{
    m_monitorId = id;
    m_timecodeFormatter = project->timecodeFormatter();
    connect(m_timecodeFormatter, SIGNAL(framerateChanged()), this, SLOT(onFramerateChange()));
    connect(m_timecodeFormatter, SIGNAL(defaultFormatChanged()), this, SLOT(update()));

    m_playbackPosition = project->timelineMonitor() == NULL ? 0 : project->timelineMonitor()->position();
    if (id != ClipMonitor) connect(this, SIGNAL(positionChanged(int,MONITORID)), project->timelineMonitor(), SLOT(seek(int,MONITORID)));
    else connect(this, SIGNAL(positionChanged(int,MONITORID)), pCore->projectManager()->current()->binMonitor(), SLOT(seek(int,MONITORID)));

    setDuration(project->timeline()->duration());

    updateFrameSize();
}

void TimelinePositionBar::setOffset(int offset)
{
    m_offset = offset;
    update();
}

void TimelinePositionBar::setCursorPosition(int position, bool seeking)
{
    if (seeking) return;
    int oldPosition = m_playbackPosition;
    m_playbackPosition = position;
    //update();
    if (qAbs(oldPosition - m_playbackPosition) * m_factor > m_textSpacing) {
        update(oldPosition * m_factor - offset() - 7, m_bigMarkX - 1, 14, height() - m_bigMarkX);
    } else {
        update(qMin(oldPosition, m_playbackPosition) * m_factor - offset() - 7, m_bigMarkX - 1,
               qAbs(oldPosition - m_playbackPosition) * m_factor + 14, height() - m_bigMarkX);
    }
}

void TimelinePositionBar::setZoomLevel(int level)
{
    m_zoomLevel = level;

    m_scale = 1 / (double) comboScale[level];
    m_factor = m_scale * m_smallMarkDistance;

    double adjustedDistance = m_timecodeFormatter->framerate() * m_smallMarkDistance;
    if (level > 8) {
        m_middleMarkDistance = adjustedDistance * 60;
        m_bigMarkDistance = adjustedDistance * 300;
    } else if (level > 6) {
        m_middleMarkDistance = adjustedDistance * 10;
        m_bigMarkDistance = adjustedDistance * 30;
    } else if (level > 3) {
        m_middleMarkDistance = adjustedDistance;
        m_bigMarkDistance = adjustedDistance * 5;
    } else {
        m_middleMarkDistance = adjustedDistance;
        m_bigMarkDistance = adjustedDistance * 60;
    }

    switch (level) {
    case 0:
        m_textSpacing = m_factor;
        break;
    case 1:
        m_textSpacing = m_factor * 5;
        break;
    case 2:
    case 3:
    case 4:
        m_textSpacing = m_timecodeFormatter->framerate() * m_factor;
        break;
    case 5:
        m_textSpacing = m_timecodeFormatter->framerate() * m_factor * 5;
        break;
    case 6:
        m_textSpacing = m_timecodeFormatter->framerate() * m_factor * 10;
        break;
    case 7:
        m_textSpacing = m_timecodeFormatter->framerate() * m_factor * 30;
        break;
    case 8:
    case 9:
        m_textSpacing = m_timecodeFormatter->framerate() * m_factor * 40;
        break;
    case 10:
        m_textSpacing = m_timecodeFormatter->framerate() * m_factor * 80;
        break;
    case 11:
    case 12:
        m_textSpacing = m_timecodeFormatter->framerate() * m_factor * 400;
        break;
    case 13:
        m_textSpacing = m_timecodeFormatter->framerate() * m_factor * 800;
        break;
    }
    update();
}

void TimelinePositionBar::onFramerateChange()
{
    setZoomLevel(m_zoomLevel);
}

void TimelinePositionBar::updateFrameSize()
{
    m_smallMarkDistance = 90; // m_view->getFrameWidth();
    setZoomLevel(m_zoomLevel);
}

void TimelinePositionBar::mousePressEvent(QMouseEvent* event)
{
    int framePosition = qRound((event->x() + m_offset) / m_factor);

    setFocus(Qt::MouseFocusReason);
//     m_view->activateMonitor();

    emit positionChanged(framePosition, m_monitorId);
    setCursorPosition(framePosition);
    kDebug()<<"/ // REQUEST POS: "<<framePosition<<" / "<<event->x()<<" / "<<m_factor;

    // update ?
}

void TimelinePositionBar::mouseMoveEvent(QMouseEvent* event)
{
    int framePosition = qRound((event->x() + m_offset) / m_factor);

    if (event->buttons() & Qt::LeftButton) {
        emit positionChanged(framePosition, m_monitorId);
	setCursorPosition(framePosition);
    } else {
        if (m_timecodeFormatter) {
            setToolTip(i18n("Position: %1", Timecode(framePosition, m_timecodeFormatter).formatted()));
        }
    }
}

void TimelinePositionBar::paintEvent(QPaintEvent* event)
{
    if (!m_timecodeFormatter) {
        return;
    }

    QPainter painter(this);
    painter.setClipRect(event->rect());

    const int maxval = (event->rect().right() + m_offset) / m_smallMarkDistance + 1;

    double f, fend;
    const int offsetmax = maxval * m_smallMarkDistance;
    int offsetmin;

    painter.setPen(palette().text().color());

    // draw time labels
    if (event->rect().y() < m_labelSize) {
        offsetmin = (event->rect().left() + m_offset) / m_textSpacing;
        offsetmin *= m_textSpacing;
        for (f = offsetmin; f < offsetmax; f += m_textSpacing) {
            painter.drawText(f - m_offset + 2, m_labelSize, Timecode(static_cast<int>(f / m_factor + 0.5), m_timecodeFormatter).formatted());
        }
    }

    // draw the little marks
    fend = m_scale * m_smallMarkDistance;
    if (fend > 5) {
        offsetmin = (event->rect().left() + m_offset) / m_smallMarkDistance;
        offsetmin *= m_smallMarkDistance;
        for (f = offsetmin - m_offset; f < offsetmax - m_offset; f += fend)
            painter.drawLine(f, m_smallMarkX, f, height());
    }

    // draw medium marks
    fend = m_scale * m_middleMarkDistance;
    if (fend > 5) {
        offsetmin = (event->rect().left() + m_offset) / m_middleMarkDistance;
        offsetmin *= m_middleMarkDistance;
        for (f = offsetmin - m_offset - fend; f < offsetmax - m_offset + fend; f += fend)
            painter.drawLine(f, m_middleMarkX, f, height());
    }

    // draw big marks
    fend = m_scale * m_bigMarkDistance;
    if (fend > 5) {
        offsetmin = (event->rect().left() + m_offset) / m_bigMarkDistance;
        offsetmin *= m_bigMarkDistance;
        for (f = offsetmin - m_offset; f < offsetmax - m_offset; f += fend)
            painter.drawLine(f, m_bigMarkX, f, height());
    }

    // draw pointer
    painter.setRenderHint(QPainter::Antialiasing);
    int position = m_playbackPosition * m_factor - m_offset;
    QPolygon pointer(3);
    pointer.setPoints(3, position - 6, m_bigMarkX,
                         position + 6, m_bigMarkX,
                         position,     height() - 1);
    painter.setBrush(palette().highlight());
    painter.drawPolygon(pointer);
}

#include "timelinepositionbar.moc"
