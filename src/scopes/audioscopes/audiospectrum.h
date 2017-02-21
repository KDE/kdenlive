/***************************************************************************
 *   Copyright (C) 2010 by Simon Andreas Eugster (simon.eu@gmail.com)      *
 *   This file is part of kdenlive. See www.kdenlive.org.                  *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 ***************************************************************************/

#ifndef AUDIOSPECTRUM_H
#define AUDIOSPECTRUM_H

#include "abstractaudioscopewidget.h"
#include "lib/external/kiss_fft/tools/kiss_fftr.h"
#include "lib/audio/fftTools.h"
#include "ui_audiospectrum_ui.h"

// Enables debugging
//#define DEBUG_AUDIOSPEC

// Show overmodulation
#define DETECT_OVERMODULATION

#include <QVector>
#include <QHash>

class AudioSpectrum_UI;

/**
   \brief Displays a spectral power distribution of audio samples.
   The frequency distribution is calculated by means of a Fast Fourier Transformation.
   For more information see Wikipedia:FFT and the code comments.

   \todo Currently only supports one channel. Add support for multiple channels.
*/
class AudioSpectrum : public AbstractAudioScopeWidget
{
    Q_OBJECT

public:
    explicit AudioSpectrum(QWidget *parent = nullptr);
    ~AudioSpectrum();

    // Implemented virtual methods
    QString widgetName() const Q_DECL_OVERRIDE;

protected:
    ///// Implemented methods /////
    QRect scopeRect() Q_DECL_OVERRIDE;
    QImage renderHUD(uint accelerationFactor) Q_DECL_OVERRIDE;
    QImage renderAudioScope(uint accelerationFactor, const audioShortVector &audioFrame, const int freq, const int num_channels, const int num_samples, const int newData) Q_DECL_OVERRIDE;
    QImage renderBackground(uint accelerationFactor) Q_DECL_OVERRIDE;
    void readConfig() Q_DECL_OVERRIDE;
    void writeConfig();

    void handleMouseDrag(const QPoint &movement, const RescaleDirection rescaleDirection, const Qt::KeyboardModifiers rescaleModifiers) Q_DECL_OVERRIDE;

private:
    Ui::AudioSpectrum_UI *ui;

    QAction *m_aResetHz;
    QAction *m_aTrackMouse;
    QAction *m_aShowMax;

    FFTTools m_fftTools;
    QVector<float> m_lastFFT;
    QSemaphore m_lastFFTLock;

    QVector<float> m_peaks;
    QVector<float> m_peakMap;

    /** Contains the plot only; m_scopeRect contains text and widgets as well */
    QRect m_innerScopeRect;

    /** Lower bound for the dB value to display */
    int m_dBmin;
    /** Upper bound (max: 0) */
    int m_dBmax;

    /** Maximum frequency (limited by the sampling rate if determined automatically).
        Stored for the painters. */
    int m_freqMax;
    /** The user has chosen a custom frequency. */
    bool m_customFreq;

    float colorizeFactor;

#ifdef DEBUG_AUDIOSPEC
    long m_timeTotal;
    long m_showTotal;
#endif

private slots:
    void slotResetMaxFreq();

};

#endif // AUDIOSPECTRUM_H
