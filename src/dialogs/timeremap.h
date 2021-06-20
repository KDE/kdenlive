/***************************************************************************
 *   Copyright (C) 2021 by Jean-Baptiste Mardelle (jb@kdenlive.org)        *
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

#ifndef TIMEREMAP_H
#define TIMEREMAP_H

#include "ui_timeremap_ui.h"

#include "definitions.h"
#include "timecode.h"
#include "timecodedisplay.h"

#include <QWidget>

class ProjectClip;


/**
 * @class RemapView
 * @brief A widget to view remap keyframes.
 * @author Jean-Baptiste Mardelle
 */

class RemapView : public QWidget
{
public:
    explicit RemapView(QWidget *parent = nullptr);
    void setDuration(int duration);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    int m_duration;
    double m_scale;
    QColor m_colSelected;
    QColor m_colKeyframe;
    int m_lineHeight;
    double m_zoomFactor;
    double m_zoomStart;
};

 /**
 * @class TimeRemap
 * @brief A dialog for editing time remap effect.
 * @author Jean-Baptiste Mardelle
 */
class TimeRemap : public QWidget, public Ui::TimeRemap_UI
{
    Q_OBJECT

public:
    explicit TimeRemap(QWidget *parent = nullptr);
    ~TimeRemap() override;
    void setClip(std::shared_ptr<ProjectClip> clip, int in = -1, int out = -1);

private:
    std::shared_ptr<ProjectClip> m_clip;
    TimecodeDisplay *m_in;
    TimecodeDisplay *m_out;
    RemapView *m_view;
};

#endif
