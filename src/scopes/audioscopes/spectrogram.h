/***************************************************************************
 *   Copyright (C) 2010 by Simon Andreas Eugster (simon.eu@gmail.com)      *
 *   This file is part of kdenlive. See www.kdenlive.org.                  *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 ***************************************************************************/

/** This Spectrogram shows the spectral power distribution of incoming audio samples
    over time. See https://en.wikipedia.org/wiki/Spectrogram.

    The Spectrogram makes use of two caches:
    * A cached image where only the most recent line needs to be appended instead of
      having to recalculate the whole image. A typical speedup factor is 10x.
    * A FFT cache storing a history of previous spectral power distributions (i.e.
      the Fourier-transformed audio signals). This is used if the user adjusts parameters
      like the maximum frequency to display or minimum/maximum signal strength in dB.
      All required information is preserved in the FFT history, which would not be the
      case for an image (consider re-sizing the widget to 100x100 px and then back to
      800x400 px -- lost is lost).
*/

#ifndef SPECTROGRAM_H
#define SPECTROGRAM_H

#include "abstractaudioscopewidget.h"
#include "lib/audio/fftTools.h"
#include "ui_spectrogram_ui.h"

class Spectrogram_UI;
class Spectrogram : public AbstractAudioScopeWidget
{
    Q_OBJECT

public:
    explicit Spectrogram(QWidget *parent = nullptr);
    ~Spectrogram() override;

    QString widgetName() const override;

protected:
    ///// Implemented methods /////
    QRect scopeRect() override;
    QImage renderHUD(uint accelerationFactor) override;
    QImage renderAudioScope(uint accelerationFactor, const audioShortVector &audioFrame, const int freq, const int num_channels, const int num_samples,
                            const int newData) override;
    QImage renderBackground(uint accelerationFactor) override;
    bool isHUDDependingOnInput() const override;
    bool isScopeDependingOnInput() const override;
    bool isBackgroundDependingOnInput() const override;
    void readConfig() override;
    void writeConfig();
    void handleMouseDrag(const QPoint &movement, const RescaleDirection rescaleDirection, const Qt::KeyboardModifiers rescaleModifiers) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    Ui::Spectrogram_UI *m_ui;
    FFTTools m_fftTools;
    QAction *m_aResetHz;
    QAction *m_aGrid;
    QAction *m_aTrackMouse;
    QAction *m_aHighlightPeaks;

    QList<QVector<float>> m_fftHistory;
    QImage m_fftHistoryImg;

    int m_dBmin{-70};
    int m_dBmax{0};

    int m_freqMax{0};
    bool m_customFreq{false};

    bool m_parameterChanged{false};

    QRect m_innerScopeRect;
    QRgb m_colorMap[256];

private slots:
    void slotResetMaxFreq();
};

#endif // SPECTROGRAM_H
