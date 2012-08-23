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

#ifndef TIMELINEPOSITIONBAR_H
#define TIMELINEPOSITIONBAR_H

#include <QWidget>

class Project;
class TimecodeFormatter;


/**
 * @class TimelinePositionBar
 * @brief Position bar/ruler used above the timeline view.
 * 
 * TODO: refactor for more dynamic zooming options
 */


class TimelinePositionBar : public QWidget
{
    Q_OBJECT

public:
    TimelinePositionBar(QWidget *parent = 0);

    /** @brief Returns the offset (number of frames in the beginning not shown) in frames. */
    int offset() const;

    /** @brief A list of scaling factors for the different zoom levels (currently 13). */
    static const int comboScale[];

public slots:
    /** @brief Connects to @param project and sets position, duration and timecode format. */
    void setProject(Project *project);
    /** @brief Redraws the cursor indicator. */
    void setCursorPosition(int position);
    /** @brief Sets the offset. */
    void setOffset(int offset);
    /** @brief Sets the duration / timeline length. */
    void setDuration(int duration);
    /** @brief Sets the zoom level. */
    void setZoomLevel(int level);

signals:
    void positionChanged(int position);
    
protected:
    void mousePressEvent(QMouseEvent *event);
    void mouseMoveEvent(QMouseEvent *event);
    void paintEvent(QPaintEvent *event);

private slots:
    void onFramerateChange();

private:
    void updateFrameSize();

    TimecodeFormatter *m_timecodeFormatter;
    int m_duration;
    int m_offset;
    int m_playbackPosition;
    double m_textSpacing;
    double m_factor;
    double m_scale;
    int m_zoomLevel;

    int m_labelSize;
    int m_bigMarkX;
    int m_middleMarkX;
    int m_smallMarkX;
    int m_bigMarkDistance;
    int m_middleMarkDistance;
    int m_smallMarkDistance;
};

#endif
