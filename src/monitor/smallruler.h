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

#include "monitormanager.h"

#include <KColorScheme>
#include <QWidget>

class SmallRuler : public QWidget
{
    Q_OBJECT

    enum controls {
        CONTROL_NONE,
        CONTROL_HEAD,
        CONTROL_IN,
        CONTROL_OUT
    };

public:
    explicit SmallRuler(Monitor *manager, Render *render, QWidget *parent = nullptr);
    void adjustScale(int maximum, int offset);
    void setZone(int start, int end);
    void setZoneStart();
    void setZoneEnd(bool discardLastFrame = false);
    QPoint zone() const;
    void setMarkers(const QList< CommentedTime > &list);
    QString markerAt(GenTime pos);
    void updatePalette();
    void refreshRuler();

protected:
    void paintEvent(QPaintEvent *e) Q_DECL_OVERRIDE;
    void resizeEvent(QResizeEvent *) Q_DECL_OVERRIDE;
    void enterEvent(QEvent *event) Q_DECL_OVERRIDE;
    void leaveEvent(QEvent *event) Q_DECL_OVERRIDE;
    void mousePressEvent(QMouseEvent *event) Q_DECL_OVERRIDE;
    void mouseMoveEvent(QMouseEvent *event) Q_DECL_OVERRIDE;
    void mouseReleaseEvent(QMouseEvent *event) Q_DECL_OVERRIDE;

private:
    int m_cursorFramePosition;
    int m_maxval;
    int m_offset;
    Monitor *m_monitor;
    Render *m_render;
    int m_lastSeekPosition;
    int m_hoverZone;
    enum controls m_activeControl;
    double m_scale;
    int m_zoneStart;
    int m_zoneEnd;
    int m_rulerHeight;
    double m_smallMarkSteps;
    double m_mediumMarkSteps;
    int m_medium;
    int m_small;
    QList<CommentedTime> m_markers;
    QPixmap m_pixmap;
    void prepareZoneUpdate();
    void updatePixmap();

public slots:
    bool slotNewValue(int value);

signals:
    void zoneChanged(const QPoint &);
};

#endif
