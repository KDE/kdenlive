/*
    SPDX-FileCopyrightText: 2011 Till Theato <root@ttill.de>
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#ifndef SIMPLEKEYFRAMEWIDGET_H
#define SIMPLEKEYFRAMEWIDGET_H

#include "utils/timecode.h"

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
