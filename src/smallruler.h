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


#ifndef SMALLRULER_H
#define SMALLRULER_H

#include <KColorScheme>
#include <QWidget>

#include "monitormanager.h"


class SmallRuler : public QWidget
{
    Q_OBJECT

public:
    explicit SmallRuler(MonitorManager *manager, QWidget *parent = 0);
    virtual void mousePressEvent(QMouseEvent * event);
    virtual void mouseMoveEvent(QMouseEvent * event);
    virtual void leaveEvent( QEvent * event );
    void adjustScale(int maximum);
    void setZone(int start, int end);
    QPoint zone();
    void setMarkers(QList < int > list);
    int position() const;
    void updatePalette();

protected:
    virtual void paintEvent(QPaintEvent *e);
    virtual void resizeEvent(QResizeEvent *);

private:
    int m_cursorPosition;
    int m_cursorFramePosition;
    double m_scale;
    int m_medium;
    int m_small;
    int m_maxval;
    int m_zoneStart;
    int m_zoneEnd;
    KStatefulBrush m_zoneBrush;
    QList <int> m_markers;
    QPixmap m_pixmap;
    MonitorManager *m_manager;
    /** @brief True is mouse is over the ruler cursor. */
    bool m_overCursor;
    void updatePixmap();

public slots:
    bool slotNewValue(int value);

signals:
    void seekRenderer(int);
    void zoneChanged(QPoint);
};

#endif
