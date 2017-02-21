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

#include <QWidget>

class SimpleTimelineWidget : public QWidget
{
    Q_OBJECT

public:
    explicit SimpleTimelineWidget(QWidget *parent = nullptr);
    void setKeyframes(const QList<int> &keyframes);
    void setDuration(int dur);

public slots:
    void slotSetPosition(int pos);
    void slotRemoveKeyframe(int pos);
    void slotAddKeyframe(int pos = - 1, int select = false);
    void slotAddRemove();
    void slotGoToNext();
    void slotGoToPrev();

protected:
    void paintEvent(QPaintEvent *event) Q_DECL_OVERRIDE;
    void mousePressEvent(QMouseEvent *event) Q_DECL_OVERRIDE;
    void mouseReleaseEvent(QMouseEvent *event) Q_DECL_OVERRIDE;
    void mouseMoveEvent(QMouseEvent *event) Q_DECL_OVERRIDE;
    void mouseDoubleClickEvent(QMouseEvent *event) Q_DECL_OVERRIDE;
    void wheelEvent(QWheelEvent *event) Q_DECL_OVERRIDE;

private:
    int m_duration;
    int m_position;
    int m_currentKeyframe;
    int m_currentKeyframeOriginal;
    int m_hoverKeyframe;
    QList<int> m_keyframes;
    int m_lineHeight;
    double m_scale;
    int m_size;

    QColor m_colSelected;
    QColor m_colKeyframe;
    QColor m_colKeyframeBg;

signals:
    void positionChanged(int pos);
    void atKeyframe(bool);

    void keyframeSelected();
    void keyframeMoving(int oldPos, int currentPos);
    void keyframeMoved(int oldPos, int newPos);
    void keyframeAdded(int pos);
    void keyframeRemoved(int pos);
};

#endif
