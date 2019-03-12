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

#ifndef SIMPLEKEYFRAMEWIDGET_H
#define SIMPLEKEYFRAMEWIDGET_H

#include "timecode.h"

#include <QWidget>

class SimpleTimelineWidget;
class TimecodeDisplay;
class QToolButton;

class SimpleKeyframeWidget : public QWidget
{
    Q_OBJECT

public:
    explicit SimpleKeyframeWidget(const Timecode &t, int duration, QWidget *parent = nullptr);
    ~SimpleKeyframeWidget() override;

    int getPosition() const;
    void setKeyframes(const QList<int> &keyframes);
    void addKeyframe(int pos = -1);

    void updateTimecodeFormat();

public slots:
    void slotSetPosition(int pos = -1, bool update = true);

private slots:
    void slotAtKeyframe(bool atKeyframe);

signals:
    void positionChanged(int pos);
    void keyframeAdded(int pos);
    void keyframeRemoved(int pos);
    void keyframeMoved(int oldPos, int newPos);

private:
    SimpleTimelineWidget *m_timeline;
    QToolButton *m_buttonAddDelete;
    QToolButton *m_buttonPrevious;
    QToolButton *m_buttonNext;
    TimecodeDisplay *m_time;
};

#endif
