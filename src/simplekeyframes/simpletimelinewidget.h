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
    void slotAddKeyframe(int pos = -1, int select = false);
    void slotAddRemove();
    void slotGoToNext();
    void slotGoToPrev();

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;

private:
    int m_duration{1};
    int m_position{0};
    int m_currentKeyframe{-1};
    int m_currentKeyframeOriginal{-1};
    int m_hoverKeyframe{-1};
    QList<int> m_keyframes;
    int m_lineHeight;
    double m_scale{1};
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
