/***************************************************************************
 *   Copyright (C) 2010 by Simon Andreas Eugster (simon.eu@gmail.com)      *
 *   This file is part of kdenlive. See www.kdenlive.org.                  *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 ***************************************************************************/

#ifndef FFTTOOLS_H
#define FFTTOOLS_H

#include "../../definitions.h"
#include "../external/kiss_fft/tools/kiss_fftr.h"
#include <QHash>
#include <QVector>

class FFTTools
{
public:
    FFTTools();
    ~FFTTools();

    enum WindowType { Window_Rect, Window_Triangle, Window_Hamming };

    /** Creates a vector containing the factors for the selected window functions.
        The last element in the vector (at position size+1) contains the area of
        this window function compared to the rectangular window (e.g. for a triangular
        window the factor will be 0.5).
        Knowing this factor is important for the Fourier Transformation as the
        values in the frequency domain will be scaled by this factor and need to be
        re-scaled for proper dB display.
        The additional parameter describes:
        * Nothing for the Rectangular window
        * The left and right start values for the Triangular window (i.e. ranges between param and 1;
          default is 0)
        * Nothing for the Hamming window
    */
    static const QVector<float> window(const WindowType windowType, const int size, const float param = 0);

    static const QString windowSignature(const WindowType windowType, const int size, const float param = 0);

    /** Returns a signature for a kiss_fft configuration
        used as a hash in the cache */
    static const QString cfgSignature(const int size);

    /** Calculates the Fourier Transformation of the input audio frame.
        The resulting values will be given in relative decibel: The maximum power is 0 dB, lower powers have
        negative dB values.
        * audioFrame: Interleaved format with #numChannels channels
        * freqSpectrum: Array pointer to write the data into
        * windowSize must be divisible by 2,
        * freqSpectrum has to be of size windowSize/2
        For windowType and param see the FFTTools::window() function above.
    */
    void fftNormalized(const audioShortVector &audioFrame, const uint channel, const uint numChannels, float *freqSpectrum, const WindowType windowType,
                       const uint windowSize, const float param = 0);

    /** This is linear interpolation with the special property that it preserves peaks, which is required
        for e.g. showing correct Decibel values (where the peak values are of interest because of clipping which
        may occur for too strong frequencies; The lower values are smeared by the window function anyway).
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
    static const QVector<float> interpolatePeakPreserving(const QVector<float> &in, const uint targetSize, uint left = 0, uint right = 0, float fill = 0.0);

private:
    QHash<QString, kiss_fftr_cfg> m_fftCfgs;          // FFT cfg cache
    QHash<QString, QVector<float>> m_windowFunctions; // Window function cache
};

#endif // FFTTOOLS_H
