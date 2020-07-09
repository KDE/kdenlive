/***************************************************************************
 *   Copyright (C) 2020 by Jean-Baptiste Mardelle                          *
 *   This file is part of Kdenlive. See www.kdenlive.org.                  *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) version 3 or any later version accepted by the       *
 *   membership of KDE e.V. (or its successor approved  by the membership  *
 *   of KDE e.V.), which shall act as a proxy defined in Section 14 of     *
 *   version 3 of the license.                                             *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>. *
 ***************************************************************************/

#ifndef SPEEDDIALOG_H
#define SPEEDDIALOG_H

#include <QDialog>

namespace Ui {
class ClipSpeed_UI;
}

class TimecodeDisplay;
class KMessageWidget;

class SpeedDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SpeedDialog(QWidget *parent, double speed, int duration, double minSpeed, double maxSpeed, bool reversed, bool pitch_compensate);
    ~SpeedDialog();

    double getValue() const;
    bool getPitchCompensate() const;

private:
    Ui::ClipSpeed_UI *ui;
    TimecodeDisplay *m_durationDisplay;
    int m_duration;
    void checkSpeed(KMessageWidget *infoMessage, double res);
};

#endif // SPEEDDIALOG_H
