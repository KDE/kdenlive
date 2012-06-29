/***************************************************************************
 *   Copyright (C) 2007 by Jean-Baptiste Mardelle (jb@kdenlive.org)        *
 *   Copyright (C) 2012 by Till Theato (root@ttill.de)                     *
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

#ifndef POSITIONBAR_H
#define POSITIONBAR_H

#include <QWidget>


/**
 * @class PositionBar
 * @brief Small bar with time ticks and a movable position indicator/cursor.
 */

class PositionBar : public QWidget
{
    Q_OBJECT

public:
    explicit PositionBar(QWidget* parent = 0);
    virtual ~PositionBar();

    /** @brief Returns the currently displayed position. */
    int position() const;

public slots:
    /** @brief Updates the length in frames to show. */
    void setDuration(int duration);
    /** @brief Updates the displayed position.
     * 
     * positionChanged is not emitted
     */
    void setPosition(int position);

signals:
    void positionChanged(int position);

protected:
    void mousePressEvent(QMouseEvent *event);
    void mouseMoveEvent(QMouseEvent *event);
    void leaveEvent(QEvent *event);
    void paintEvent(QPaintEvent *event);
    void resizeEvent(QResizeEvent *event);

private:
    void adjustScale();
    void updateBackground();

    int m_duration;
    int m_position;
    int m_pixelPosition;
    double m_scale;
    double m_smallMarkSteps;
    double m_mediumMarkSteps;
    /** @brief True is mouse is over the ruler cursor. */
    bool m_mouseOverCursor;
    QPixmap m_background;
};

#endif
