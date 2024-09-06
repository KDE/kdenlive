/*
    SPDX-FileCopyrightText: 2020 Jean-Baptiste Mardelle <jb@kdenlive.org>
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "speeddialog.h"
#include "core.h"
#include "effects/effectsrepository.hpp"
#include "ui_clipspeed_ui.h"
#include "widgets/precisionspinbox.h"
#include "widgets/timecodedisplay.h"

#include <KMessageWidget>
#include <QDebug>
#include <QPushButton>
#include <QtMath>

SpeedDialog::SpeedDialog(QWidget *parent, double speed, int duration, double minSpeed, double maxSpeed, bool reversed, bool pitch_compensate,
                         ClipType::ProducerType clipType)
    : QDialog(parent)
    , ui(new Ui::ClipSpeed_UI)
    , m_durationDisplay(nullptr)
    , m_duration(duration)
{
    ui->setupUi(this);
    setWindowTitle(i18n("Clip Speed"));
    ui->speedSlider->setMinimum(0);
    ui->speedSlider->setMaximum(100);
    ui->speedSlider->setTickInterval(10);
    ui->label_dest->setVisible(false);
    ui->kurlrequester->setVisible(false);
    if (reversed) {
        ui->checkBox->setChecked(true);
    }
    ui->speedSlider->setValue(int(qLn(speed) * 12));
    ui->pitchCompensate->setChecked(pitch_compensate);
    if (!EffectsRepository::get()->exists(QStringLiteral("rbpitch"))) {
        ui->pitchCompensate->setEnabled(false);
        ui->pitchCompensate->setToolTip(i18n("MLT must be compiled with rubberband library to enable pitch correction"));
    }

    // Info widget
    if (clipType == ClipType::Playlist || clipType == ClipType::Timeline) {
        ui->infoMessage->setText(i18n("Speed effect will render better when used on single high fps clips"));
        ui->infoMessage->show();
    } else {
        ui->infoMessage->hide();
    }
    ui->precisionSpin->setSuffix(QLatin1Char('%'));
    ui->precisionSpin->setRange(minSpeed, maxSpeed, 6);
    ui->precisionSpin->setValue(speed);
    ui->precisionSpin->setFocus();
    ui->precisionSpin->selectAll();
    if (m_duration > 0) {
        ui->durationLayout->addWidget(new QLabel(i18n("Duration"), this));
        m_durationDisplay = new TimecodeDisplay(this);
        m_durationDisplay->setValue(m_duration);
        ui->durationLayout->addWidget(m_durationDisplay);
        connect(m_durationDisplay, &TimecodeDisplay::timeCodeEditingFinished, this, [this, speed, minSpeed](int value) {
            if (value < 1) {
                value = 1;
                m_durationDisplay->setValue(value);
            } else if (value > m_duration * speed / minSpeed) {
                value = int(m_duration * speed / minSpeed);
                m_durationDisplay->setValue(value);
            }
            double updatedSpeed = speed * m_duration / value;
            QSignalBlocker bk(ui->speedSlider);
            ui->speedSlider->setValue(int(qLn(updatedSpeed) * 12));
            QSignalBlocker bk2(ui->precisionSpin);
            ui->precisionSpin->setValue(updatedSpeed);
            checkSpeed(updatedSpeed);
        });
    }

    connect(ui->precisionSpin, &PrecisionSpinBox::valueChanged, this, [&, speed](double value) {
        QSignalBlocker bk(ui->speedSlider);
        ui->speedSlider->setValue(int(qLn(value) * 12));
        if (m_durationDisplay) {
            QSignalBlocker bk2(m_durationDisplay);
            int dur = qRound(m_duration * std::fabs(speed / value));
            qDebug() << "==== CALCULATED SPEED DIALOG DURATION: " << dur;
            m_durationDisplay->setValue(dur);
        }
        ui->buttonBox->button((QDialogButtonBox::Ok))->setEnabled(!qFuzzyIsNull(value));
    });
    connect(ui->speedSlider, &QSlider::valueChanged, this, [this, speed](int value) {
        double res = qExp(value / 12.);
        QSignalBlocker bk(ui->precisionSpin);
        checkSpeed(res);
        ui->precisionSpin->setValue(res);
        if (m_durationDisplay) {
            QSignalBlocker bk2(m_durationDisplay);
            m_durationDisplay->setValue(qRound(m_duration * std::fabs(speed / res)));
        }
        ui->buttonBox->button((QDialogButtonBox::Ok))->setEnabled(!qFuzzyIsNull(res));
    });
}

void SpeedDialog::checkSpeed(double res)
{
    if (res < ui->precisionSpin->min() || res > ui->precisionSpin->max()) {
        ui->infoMessage->setText(res < ui->precisionSpin->min() ? i18n("Minimum speed is %1", ui->precisionSpin->min())
                                                                : i18n("Maximum speed is %1", ui->precisionSpin->max()));
        ui->infoMessage->setCloseButtonVisible(true);
        ui->infoMessage->setMessageType(KMessageWidget::Warning);
        ui->infoMessage->animatedShow();
    }
}

SpeedDialog::~SpeedDialog()
{
    delete ui;
}

double SpeedDialog::getValue() const
{
    double val = ui->precisionSpin->value();
    if (ui->checkBox->checkState() == Qt::Checked) {
        val *= -1;
    }
    return val;
}

bool SpeedDialog::getPitchCompensate() const
{
    return ui->pitchCompensate->isChecked();
}
