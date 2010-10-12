/***************************************************************************
 *   Copyright (C) 2010 by Till Theato (root@ttill.de)                     *
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


#ifndef MONITORSCENE_H
#define MONITORSCENE_H

#include <QtCore>
#include <QGraphicsScene>

class Render;


class MonitorScene : public QGraphicsScene
{
    Q_OBJECT
public:
    MonitorScene(Render *renderer, QObject* parent = 0);

    /** @brief Sets m_view to this scenes view. */
    void setUp();

    /** @brief Enables/Disables the scene for usage (background still updated).
     * @param enabled (default = true) */
    void setEnabled(bool enabled = true);

    /** @brief Makes the background frame fit again after the profile changed (and therefore the resolution might have changed). */
    void resetProfile();

    /** @brief Adds an item to the scene and connects mouse events + change signals if it is a onmonitor item. */
    void addItem(QGraphicsItem *item);

protected:
    /** @brief Emits signal mousePressed to be used in onmonitor items. */
    virtual void mousePressEvent(QGraphicsSceneMouseEvent *event);
    /** @brief Emits signal mouseMoveEvent to be used in onmonitor items. */
    virtual void mouseMoveEvent(QGraphicsSceneMouseEvent *event);
    /** @brief Emits signal mouseReleaseEvent to be used in onmonitor items. */
    virtual void mouseReleaseEvent(QGraphicsSceneMouseEvent *event);
    /** @brief Adds a keyframe if scene is disabled. */
    virtual void mouseDoubleClickEvent(QGraphicsSceneMouseEvent *event);

public slots:
    /** @brief Sets the backgrounditem's pixmap to m_backgroundImage (only if a certain amount of time has passed since last update). */
    void slotUpdateBackground();

    /** @brief Sets the scene's zoom level.
     * @param value zoom level with 100 = show frame at original size */
    void slotZoom(int value);
    /** @brief Makes the zoom level fit the viewport's size. */
    void slotZoomFit();
    /** @brief Shows the frame at it's original size and center. */
    void slotZoomOriginal();
    /** @brief Zooms in by 1%. */
    void slotZoomIn();
    /** @brief Zooms out by 1%. */
    void slotZoomOut();

private slots:
    /** @brief Sets m_backgroundImage to @param image and requests updating the background item. */
    void slotSetBackgroundImage(const QImage &image);
    /** @brief Sets the mouse curors to @param cursor for the scene. */
    void slotSetCursor(const QCursor &cursor);

private:
    Render *m_renderer;
    QGraphicsPixmapItem *m_background;
    QGraphicsRectItem *m_frameBorder;
    QTime m_lastUpdate;
    QGraphicsView *m_view;
    QImage m_backgroundImage;
    bool m_enabled;
    qreal m_zoom;

signals:
    void actionFinished();
    void zoomChanged(int);
    void addKeyframe();
    void mouseMoved(QGraphicsSceneMouseEvent *event);
    void mousePressed(QGraphicsSceneMouseEvent *event);
    void mouseReleased(QGraphicsSceneMouseEvent *event);
};

#endif
