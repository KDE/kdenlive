/***************************************************************************
 *   Copyright (C) 2010 by Jean-Baptiste Mardelle (jb@kdenlive.org)        *
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

#ifndef TIMECODEWIDGET_H
#define TIMECODEWIDGET_H

#include "timecode.h"
#include <QAbstractSpinBox>

class Project;


class TimecodeWidget : public QAbstractSpinBox
{
    Q_OBJECT

public:
    explicit TimecodeWidget(QWidget* parent = 0);

    int minimum() const;
    void setMinimum(int minimum);
    int maximum() const;
    void setRange(int minimum, int maximum);

    Timecode value() const;

    void stepBy(int steps);

public slots:
    void setProject(Project *project);
    void setValue(Timecode value);
    void setMaximum(int maximum);
    void updateMask();
    void onEditingFinished();

signals:
    void valueChanged(int value);

protected:
    void keyPressEvent(QKeyEvent *event);
    void mouseReleaseEvent(QMouseEvent *event);
    QAbstractSpinBox::StepEnabled stepEnabled() const;

private:
    int m_minimum;
    int m_maximum;
    Timecode m_value;
    TimecodeFormatter *m_timecodeFormatter;
};

#endif
