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

#include <QVector>

class FFTTools
{
public:
    FFTTools();

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
    static const QVector<float> window(const WindowType windowType, const int size, const float param);

    static const QString windowSignature(const WindowType windowType, const int size, const float param);

};

#endif // FFTTOOLS_H
