/*
    SPDX-FileCopyrightText: 2020 Jean-Baptiste Mardelle
    SPDX-License-Identifier: LicenseRef-KDE-Accepted-GPL
*/

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
    ~SpeedDialog() override;

    double getValue() const;
    bool getPitchCompensate() const;

private:
    Ui::ClipSpeed_UI *ui;
    TimecodeDisplay *m_durationDisplay;
    int m_duration;
    void checkSpeed(KMessageWidget *infoMessage, double res);
};

#endif // SPEEDDIALOG_H
