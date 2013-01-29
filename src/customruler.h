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

/**
 * @class CustomRuler
 * @author Jean-Baptiste Mardelle
 * @brief Manages the timeline ruler.
 */

#ifndef CUSTOMRULER_H
#define CUSTOMRULER_H

#include <QWidget>

#include "customtrackview.h"
#include "timecode.h"

enum RULER_MOVE { RULER_CURSOR = 0, RULER_START = 1, RULER_MIDDLE = 2, RULER_END = 3 };
enum MOUSE_MOVE { NO_MOVE = 0, HORIZONTAL_MOVE = 1, VERTICAL_MOVE = 2 };

class CustomRuler : public QWidget
{
    Q_OBJECT

public:
    CustomRuler(Timecode tc, CustomTrackView *parent);
    void setPixelPerMark(int rate);
    static const int comboScale[];
    int outPoint() const;
    int inPoint() const;
    void setDuration(int d);
    void setZone(QPoint p);
    int offset() const;
    void updateProjectFps(Timecode t);
    void updateFrameSize();
    void updatePalette();
    
protected:
    virtual void paintEvent(QPaintEvent * /*e*/);
    virtual void wheelEvent(QWheelEvent * e);
    virtual void mousePressEvent(QMouseEvent * event);
    virtual void mouseReleaseEvent(QMouseEvent * event);
    virtual void mouseMoveEvent(QMouseEvent * event);
    virtual void leaveEvent(QEvent * event);

private:
    Timecode m_timecode;
    CustomTrackView *m_view;
    int m_zoneStart;
    int m_zoneEnd;
    int m_duration;
    QColor m_zoneColor;
    double m_textSpacing;
    double m_factor;
    double m_scale;
    int m_offset;
    int m_lastSeekPosition;
    RULER_MOVE m_moveCursor;
    QMenu *m_contextMenu;
    QAction *m_editGuide;
    QAction *m_deleteGuide;
    int m_clickedGuide;
    /** Used for zooming through vertical move */
    QPoint m_clickPoint;
    int m_rate;
    int m_startRate;
    MOUSE_MOVE m_mouseMove;
    QMenu *m_goMenu;
    QBrush m_cursorColor;


public slots:
    void slotMoveRuler(int newPos);
    void slotCursorMoved(int oldpos, int newpos);
    void updateRuler();

private slots:
    void slotEditGuide();
    void slotDeleteGuide();
    void slotGoToGuide(QAction *act);

signals:
    void zoneMoved(int, int);
    void adjustZoom(int);
};

#endif
