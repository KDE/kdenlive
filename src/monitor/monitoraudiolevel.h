/*
Copyright (C) 2016 Jean-Baptiste Mardelle <jb@kdenlive.org>
This file is part of Kdenlive. See www.kdenlive.org.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of
the License or (at your option) version 3 or any later version
accepted by the membership of KDE e.V. (or its successor approved
by the membership of KDE e.V.), which shall act as a proxy 
defined in Section 14 of version 3 of the license.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/


#ifndef MONITORAUDIOLEVEL_H
#define MONITORAUDIOLEVEL_H

#include "definitions.h"

#include <QProgressBar>
#include <QLinearGradient>

class MyProgressBar : public QProgressBar
{
    Q_OBJECT
public:
    explicit MyProgressBar(int height, QWidget *parent = 0);
    void setAudioValue(int value);

protected:
    void paintEvent(QPaintEvent*) Q_DECL_OVERRIDE;

private:
    QLinearGradient m_gradient;
    int m_peak;
};


class MonitorAudioLevel : public QObject
{
    Q_OBJECT
public:
    explicit MonitorAudioLevel(QObject *parent = 0);
    QWidget *createProgressBar(int height, QWidget *parent);
    void setMonitorVisible(bool visible);

public slots:
    void slotAudioLevels(const audioLevelVector &dbLevels);

private:
    MyProgressBar *m_pBar1;
    MyProgressBar *m_pBar2;
    audioLevelVector m_levels;
    audioLevelVector m_peaks;

};

#endif
