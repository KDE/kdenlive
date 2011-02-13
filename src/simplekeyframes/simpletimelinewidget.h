/***************************************************************************
 *   Copyright (C) 2011 by Till Theato (root@ttill.de)                     *
 *   This file is part of Kdenlive (www.kdenlive.org).                     *
 *                                                                         *
 *   Kdenlive is free software: you can redistribute it and/or modify      *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation, either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   Kdenlive is distributed in the hope that it will be useful,           *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with Kdenlive.  If not, see <http://www.gnu.org/licenses/>.     *
 ***************************************************************************/

#ifndef SIMPLETIMELINEWIDGET_H
#define SIMPLETIMELINEWIDGET_H

#include <QtCore>
#include <QWidget>

class SimpleTimelineWidget : public QWidget
{
    Q_OBJECT

public:
    SimpleTimelineWidget(QWidget* parent = 0);
    void setKeyframes(QList <int> keyframes);
    void setRange(int min, int max);

public slots:
    void slotSetPosition(int pos);
    void slotRemoveKeyframe(int pos);
    void slotAddKeyframe(int pos = - 1, int select = false);
    void slotAddRemove();
    void slotGoToNext();
    void slotGoToPrev();

protected:
    void paintEvent(QPaintEvent *event);
    void mousePressEvent(QMouseEvent *event);
    void mouseReleaseEvent(QMouseEvent *event);
    void mouseMoveEvent(QMouseEvent *event);
    void wheelEvent(QWheelEvent *event);

private:
    int m_min;
    int m_max;
    int m_position;
    int m_currentKeyframe;
    int m_currentKeyframeOriginal;
    QList <int> m_keyframes;
    int m_lineHeight;
    double m_scale;

signals:
    void positionChanged(int pos);

    void keyframeSelected();
    void keyframeMoving(int oldPos, int currentPos);
    void keyframeMoved(int oldPos, int newPos);
    void keyframeAdded(int pos);
    void keyframeRemoved(int pos);
};

#endif
