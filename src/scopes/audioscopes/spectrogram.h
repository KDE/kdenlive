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
    over time. See http://en.wikipedia.org/wiki/Spectrogram.

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
#include "ui_spectrogram_ui.h"
#include "lib/audio/fftTools.h"

class Spectrogram_UI;
class Spectrogram : public AbstractAudioScopeWidget
{
    Q_OBJECT

public:
    explicit Spectrogram(QWidget *parent = nullptr);
    ~Spectrogram();

    QString widgetName() const Q_DECL_OVERRIDE;

protected:
    ///// Implemented methods /////
    QRect scopeRect() Q_DECL_OVERRIDE;
    QImage renderHUD(uint accelerationFactor) Q_DECL_OVERRIDE;
    QImage renderAudioScope(uint accelerationFactor, const audioShortVector &audioFrame, const int freq, const int num_channels, const int num_samples, const int newData) Q_DECL_OVERRIDE;
    QImage renderBackground(uint accelerationFactor) Q_DECL_OVERRIDE;
    bool isHUDDependingOnInput() const Q_DECL_OVERRIDE;
    bool isScopeDependingOnInput() const Q_DECL_OVERRIDE;
    bool isBackgroundDependingOnInput() const Q_DECL_OVERRIDE;
    void readConfig() Q_DECL_OVERRIDE;
    void writeConfig();
    void handleMouseDrag(const QPoint &movement, const RescaleDirection rescaleDirection, const Qt::KeyboardModifiers rescaleModifiers) Q_DECL_OVERRIDE;
    void resizeEvent(QResizeEvent *event) Q_DECL_OVERRIDE;

private:
    Ui::Spectrogram_UI *ui;
    FFTTools m_fftTools;
    QAction *m_aResetHz;
    QAction *m_aGrid;
    QAction *m_aTrackMouse;
    QAction *m_aHighlightPeaks;

    QList<QVector<float> > m_fftHistory;
    QImage m_fftHistoryImg;

    int m_dBmin;
    int m_dBmax;

    int m_freqMax;
    bool m_customFreq;

    bool m_parameterChanged;

    QRect m_innerScopeRect;
    QRgb m_colorMap[256];

private slots:
    void slotResetMaxFreq();

};

#endif // SPECTROGRAM_H
