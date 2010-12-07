/***************************************************************************
 *   Copyright (C) 2010 by Simon Andreas Eugster (simon.eu@gmail.com)      *
 *   This file is part of kdenlive. See www.kdenlive.org.                  *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 ***************************************************************************/

#include "spectrogram.h"

Spectrogram::Spectrogram(QWidget *parent) :
        AbstractAudioScopeWidget(false, parent)
{
    ui = new Ui::Spectrogram_UI;
    ui->setupUi(this);
    AbstractScopeWidget::init();
}

Spectrogram::~Spectrogram()
{
    writeConfig();
}

void Spectrogram::readConfig()
{
    AbstractScopeWidget::readConfig();
}
void Spectrogram::writeConfig() {}

QString Spectrogram::widgetName() const { return QString("Spectrogram"); }

QRect Spectrogram::scopeRect() { return AbstractScopeWidget::rect(); }

QImage Spectrogram::renderHUD(uint) { return QImage(); }
QImage Spectrogram::renderAudioScope(uint accelerationFactor, const QVector<int16_t> audioFrame, const int freq, const int num_channels, const int num_samples) {
    return QImage();
}
QImage Spectrogram::renderBackground(uint) { return QImage(); }

bool Spectrogram::isHUDDependingOnInput() const { return false; }
bool Spectrogram::isScopeDependingOnInput() const { return false; }
bool Spectrogram::isBackgroundDependingOnInput() const { return false; }
