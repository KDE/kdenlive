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

enum resizeModes { NoResize, TopLeft, BottomLeft, TopRight, BottomRight, Left, Right, Top, Bottom };

class MonitorScene : public QGraphicsScene
{
    Q_OBJECT
public:
    MonitorScene(Render *renderer, QObject* parent = 0);
    void setUp();
    void setEnabled(bool enabled = true);
    void resetProfile();

protected:
    virtual void mousePressEvent(QGraphicsSceneMouseEvent *event);
    virtual void mouseMoveEvent(QGraphicsSceneMouseEvent *event);
    virtual void mouseReleaseEvent(QGraphicsSceneMouseEvent *event);
    virtual void mouseDoubleClickEvent(QGraphicsSceneMouseEvent *event);

public slots:
    void slotUpdateBackground();
    void slotSetDirectUpdate(bool directUpdate);

    void slotZoom(int value);
    void slotZoomFit();
    void slotZoomOriginal();
    void slotZoomIn();
    void slotZoomOut();

private slots:
    void slotSetBackgroundImage(const QImage &image);

private:
    resizeModes getResizeMode(QGraphicsRectItem *item, QPoint pos);

    Render *m_renderer;
    QGraphicsPixmapItem *m_background;
    QGraphicsRectItem *m_frameBorder;
    QTime m_lastUpdate;
    QGraphicsView *m_view;
    QGraphicsItem *m_selectedItem;
    resizeModes m_resizeMode;
    QPointF m_clickPoint;
    QImage m_backgroundImage;
    bool m_enabled;
    bool m_modified;
    qreal m_zoom;

signals:
    void actionFinished();
    void zoomChanged(int);
    void addKeyframe();
};

#endif
