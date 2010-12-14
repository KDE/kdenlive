/***************************************************************************
 *   Copyright (C) 2010 by Simon Andreas Eugster (simon.eu@gmail.com)      *
 *   This file is part of kdenlive. See www.kdenlive.org.                  *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 ***************************************************************************/

/**
   Displays a spectral power distribution of audio samples.
   The frequency distribution is calculated by means of a Fast Fourier Transformation.
   For more information see Wikipedia:FFT and the code comments.
*/

#ifndef AUDIOSPECTRUM_H
#define AUDIOSPECTRUM_H

// Enables debugging
//#define DEBUG_AUDIOSPEC

#include <QtCore>
#include <QVector>
#include <QHash>

#include "abstractaudioscopewidget.h"
#include "ui_audiospectrum_ui.h"
#include "tools/kiss_fftr.h"
#include "ffttools.h"

class AudioSpectrum_UI;
class AudioSpectrum : public AbstractAudioScopeWidget {
    Q_OBJECT

public:
    AudioSpectrum(QWidget *parent = 0);
    ~AudioSpectrum();

    // Implemented virtual methods
    QString widgetName() const;


protected:
    ///// Implemented methods /////
    QRect scopeRect();
    QImage renderHUD(uint accelerationFactor);
    QImage renderAudioScope(uint accelerationFactor, const QVector<int16_t> audioFrame, const int freq, const int num_channels, const int num_samples, const int newData);
    QImage renderBackground(uint accelerationFactor);
    bool isHUDDependingOnInput() const;
    bool isScopeDependingOnInput() const;
    bool isBackgroundDependingOnInput() const;
    virtual void readConfig();
    void writeConfig();

    virtual void handleMouseDrag(const QPoint movement, const RescaleDirection rescaleDirection, const Qt::KeyboardModifiers rescaleModifiers);

private:
    Ui::AudioSpectrum_UI *ui;

    QAction *m_aResetHz;
    QAction *m_aTrackMouse;

    FFTTools m_fftTools;
    QVector<float> m_lastFFT;
    QSemaphore m_lastFFTLock;

    /** Contains the plot only; m_scopeRect contains text and widgets as well */
    QRect m_innerScopeRect;

    /** Lower bound for the dB value to display */
    int m_dBmin;
    /** Upper bound (max: 0) */
    int m_dBmax;

    /** Maximum frequency (limited by the sampling rate if determined automatically).
        Stored for the painters. */
    uint m_freqMax;
    /** The user has chosen a custom frequency. */
    bool m_customFreq;

    /** This is linear interpolation with the special property that it preserves peaks, which is required
        for e.g. showing correct Decibel values (where the peak values are of interest).
        Consider f = {0, 100, 0}
                 x = {0.5,  1.5}: With default linear interpolation x0 and x1 would both be mapped to 50.
        This function maps x1 (the first position after the peak) to 100.

        @param in           The source vector containing the data
        @param targetSize   Number of interpolation nodes between ...
        @param left         the left array index in the in-vector and ...
        @param right        the right array index (both inclusive).
        @param fill         If right lies outside of the array bounds (which is perfectly fine here) then this value
                            will be used for filling the missing information.
        */
    static const QVector<float> interpolatePeakPreserving(const QVector<float> in, const uint targetSize, uint left = 0, uint right = 0, float fill = 0.0);

#ifdef DEBUG_AUDIOSPEC
    long m_timeTotal;
    long m_showTotal;
#endif


private slots:
    void slotResetMaxFreq();

};

#endif // AUDIOSPECTRUM_H
