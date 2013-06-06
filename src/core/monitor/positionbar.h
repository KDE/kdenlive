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

    enum controls {
        CONTROL_NONE,
        CONTROL_HEAD,
        CONTROL_IN,
        CONTROL_OUT
    };

public:
    explicit PositionBar(QWidget* parent = 0);
    virtual ~PositionBar();

    /** @brief Returns the currently displayed position. */
    int position() const;
    /** @brief Returns the duration of current displayed clip. */
    int duration() const;
    /** @brief Moves playhead to requested position
     *. @param position the new position
     */
    void setSeekPos(int position);
    /** @brief Sets the ruler zone
     *. @param zone the new zone
     */
    void setZone(const QPoint &zone);

public slots:
    /** @brief Updates the length in frames to show. 
     *  @param duration clip duration
     *  @param fps current profile fps, should it be replaced by a better way to get profile info?
     */
    void setDuration(int duration, double fps);
    /** @brief Updates the displayed position of currently active control.
     * 
     * positionChanged is not emitted
     */
    void setPosition(int position);
    
    /** @brief Displays tooltip with thumbnail.
     *  @param position The frame position ot the thumbnail
     *  @param img the thumbnail image
     * 
     */
    void slotSetThumbnail(int position, const QImage &img);

signals:
    /** @brief Playhead position was changed, inform others.
     *  @param position The new frame position.
     * */
    void positionChanged(int position);
    /** @brief Request a thumbnail for position, used to display tooltips
     *  @param position the frame for which we want a thumbnail
     * */
    void requestThumb(int position);
    /** @brief Inform monitor that the zone or markers changed
     *  @param marks the list of frames with info text
     * */
    void marksChanged(const QMap <int, QString> &marks);

protected:
    void mousePressEvent(QMouseEvent *event);
    void mouseMoveEvent(QMouseEvent *event);
    void mouseReleaseEvent(QMouseEvent* event);
    void enterEvent(QEvent *event);
    void leaveEvent(QEvent *event);
    void paintEvent(QPaintEvent *event);
    void resizeEvent(QResizeEvent *event);

private:
    /** @brief Duration of the clip */
    int m_duration;
    /** @brief Fps of current profile */
    double m_fps;
    /** @brief Position of the active control */
    int m_position;
    /** @brief If we hovering a marker, keep it's position */
    int m_hoverMarker;
    /** @brief Which control is currently active (playhead, zone in / out) */
    enum controls m_activeControl;
    /** @brief Height of the ruler part */
    int m_rulerHeight;
    /** @brief Clip markers / zone, temporary until we implement better handling */
    QList <int> m_markers;
    /** @brief Holds the zone in / out points */
    QPoint m_zone;
    /** @brief Position of the playhead (in frames) */
    int m_head;
    /** @brief Position of playhead (in pixels) */
    int m_pixelPosition;
    /** @brief Scale factor for the ruler, depends on widget width and clip length */
    double m_scale;
    /** @brief Distance between 2 small time marks in ruler */
    double m_smallMarkSteps;
    /** @brief Distance between 2 medium time marks in ruler */
    double m_mediumMarkSteps;
    /** @brief True is mouse is over the ruler cursor. */
    bool m_mouseOverCursor;
    /** @brief The pixmap background image, consisting of the time marks. */
    QPixmap m_background;
    /** @brief Adjusts the ruler scale in case the position bar size or the clip duration changed */
    void adjustScale();
    /** @brief Repaint the background pixmap (containing the ruler time marks) */
    void updateBackground();
    /** @brief Zone or markers changed, inform monitor */
    void prepareMarks();
};

#endif
