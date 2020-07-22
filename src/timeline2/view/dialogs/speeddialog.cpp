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


#include "speeddialog.h"
#include "ui_clipspeed_ui.h"
#include "effects/effectsrepository.hpp"
#include "timecodedisplay.h"
#include "core.h"

#include <QPushButton>
#include <QDebug>
#include <KMessageWidget>
#include <QtMath>

SpeedDialog::SpeedDialog(QWidget *parent, double speed, int duration, double minSpeed, double maxSpeed, bool reversed, bool pitch_compensate)
    : QDialog(parent)
    , ui(new Ui::ClipSpeed_UI)
    , m_durationDisplay(nullptr)
    , m_duration(duration)
{
    ui->setupUi(this);
    setWindowTitle(i18n("Clip Speed"));
    ui->speedSpin->setDecimals(2);
    ui->speedSpin->setMinimum(minSpeed);
    ui->speedSpin->setMaximum(maxSpeed);
    ui->speedSlider->setMinimum(0);
    ui->speedSlider->setMaximum(100);
    ui->speedSlider->setTickInterval(10);
    ui->label_dest->setVisible(false);
    ui->kurlrequester->setVisible(false);
    ui->toolButton->setVisible(false);
    if (reversed) {
        ui->checkBox->setChecked(true);
    }
    ui->speedSpin->setValue(speed);
    ui->speedSlider->setValue(qLn(speed) * 12);
    ui->pitchCompensate->setChecked(pitch_compensate);
    if (!EffectsRepository::get()->exists(QStringLiteral("rbpitch"))) {
        ui->pitchCompensate->setEnabled(false);
        ui->pitchCompensate->setToolTip(i18n("MLT must be compiled with rubberband library to enable pitch correction"));
    }

    // Info widget
    KMessageWidget *infoMessage = new KMessageWidget(this);
    ui->infoLayout->addWidget(infoMessage);
    infoMessage->hide();
    ui->speedSpin->setFocus();
    ui->speedSpin->selectAll();
    if (m_duration > 0) {
        ui->durationLayout->addWidget(new QLabel(i18n("Duration"), this));
        m_durationDisplay = new TimecodeDisplay(pCore->timecode());
        m_durationDisplay->setValue(m_duration);
        ui->durationLayout->addWidget(m_durationDisplay);
        connect(m_durationDisplay, &TimecodeDisplay::timeCodeEditingFinished, this, [this, infoMessage, speed, minSpeed](int value) {
            if (value < 1) {
                value = 1;
                m_durationDisplay->setValue(value);
            } else if (value > m_duration * speed / minSpeed) {
                value = m_duration * speed / minSpeed;
                m_durationDisplay->setValue(value);
            }
            double updatedSpeed = speed * m_duration / value;
            QSignalBlocker bk(ui->speedSlider);
            ui->speedSlider->setValue(qLn(updatedSpeed) * 12);
            QSignalBlocker bk2(ui->speedSpin);
            ui->speedSpin->setValue(updatedSpeed);
            checkSpeed(infoMessage, updatedSpeed);
            
        });
    }

    connect(ui->speedSpin, static_cast<void (QDoubleSpinBox::*)(double)>(&QDoubleSpinBox::valueChanged), this, [&, speed] (double value) {
        QSignalBlocker bk(ui->speedSlider);
        ui->speedSlider->setValue(qLn(value) * 12);
        if (m_durationDisplay) {
            QSignalBlocker bk2(m_durationDisplay);
            int dur = qRound(m_duration * std::fabs(speed / value));
            qDebug()<<"==== CALCULATED SPEED DIALOG DURATION: "<<dur;
            m_durationDisplay->setValue(dur);
        }
        ui->buttonBox->button((QDialogButtonBox::Ok))->setEnabled(!qFuzzyIsNull(value));
    });
    connect(ui->speedSlider, &QSlider::valueChanged, this, [this, infoMessage, speed] (int value) {
        double res = qExp(value / 12.);
        QSignalBlocker bk(ui->speedSpin);
        checkSpeed(infoMessage, res);
        ui->speedSpin->setValue(res);
        if (m_durationDisplay) {
            QSignalBlocker bk2(m_durationDisplay);
            m_durationDisplay->setValue(qRound(m_duration * std::fabs(speed / res)));
        }
        ui->buttonBox->button((QDialogButtonBox::Ok))->setEnabled(!qFuzzyIsNull(ui->speedSpin->value()));
    });
}


void SpeedDialog::checkSpeed(KMessageWidget *infoMessage, double res)
{
    if (res < ui->speedSpin->minimum() || res > ui->speedSpin->maximum()) {
        infoMessage->setText(res < ui->speedSpin->minimum() ? i18n("Minimum speed is %1", ui->speedSpin->minimum()) : i18n("Maximum speed is %1", ui->speedSpin->maximum()));
        infoMessage->setCloseButtonVisible(true);
        infoMessage->setMessageType(KMessageWidget::Warning);
        infoMessage->animatedShow();
    }
}

SpeedDialog::~SpeedDialog()
{
    delete ui;
}

double SpeedDialog::getValue() const
{
    double val = ui->speedSpin->value();
    if (ui->checkBox->checkState() == Qt::Checked) {
        val *= -1;
    }
    return val;
}

bool SpeedDialog::getPitchCompensate() const
{
    return ui->pitchCompensate->isChecked();
}
