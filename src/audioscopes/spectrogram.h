/***************************************************************************
 *   Copyright (C) 2010 by Simon Andreas Eugster (simon.eu@gmail.com)      *
 *   This file is part of kdenlive. See www.kdenlive.org.                  *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 ***************************************************************************/

#ifndef SPECTROGRAM_H
#define SPECTROGRAM_H

#include <QtCore>

#include "abstractaudioscopewidget.h"
#include "ui_spectrogram_ui.h"

class Spectrogram_UI;
class Spectrogram : public AbstractAudioScopeWidget {
    Q_OBJECT

public:
    Spectrogram(QWidget *parent = 0);
    ~Spectrogram();

    QString widgetName() const;

protected:
    ///// Implemented methods /////
    QRect scopeRect();
    QImage renderHUD(uint accelerationFactor);
    QImage renderAudioScope(uint accelerationFactor, const QVector<int16_t> audioFrame, const int freq, const int num_channels, const int num_samples);
    QImage renderBackground(uint accelerationFactor);
    bool isHUDDependingOnInput() const;
    bool isScopeDependingOnInput() const;
    bool isBackgroundDependingOnInput() const;
    virtual void readConfig();
    void writeConfig();

private:
    Ui::Spectrogram_UI *ui;
};

#endif // SPECTROGRAM_H
