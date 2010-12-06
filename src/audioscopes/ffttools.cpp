/***************************************************************************
 *   Copyright (C) 2010 by Simon Andreas Eugster (simon.eu@gmail.com)      *
 *   This file is part of kdenlive. See www.kdenlive.org.                  *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 ***************************************************************************/

#include <math.h>

#include <QString>

#include "ffttools.h"

//#define DEBUG_FFTTOOLS
#ifdef DEBUG_FFTTOOLS
#include <QDebug>
#endif

FFTTools::FFTTools()
{
}

const QString FFTTools::windowSignature(const WindowType windowType, const int size, const float param)
{
    return QString("s%1_t%2_p%3").arg(size).arg(windowType).arg(param, 0, 'f', 3);
}

// http://cplusplus.syntaxerrors.info/index.php?title=Cannot_declare_member_function_%E2%80%98static_int_Foo::bar%28%29%E2%80%99_to_have_static_linkage
const QVector<float> FFTTools::window(const WindowType windowType, const int size, const float param)
{
    // Deliberately avoid converting size to a float
    // to keep mid an integer.
    float mid = (size-1)/2;
    float max = size-1;
    QVector<float> window;

    switch (windowType) {
    case Window_Rect:
        return QVector<float>(size+1, 1);
        break;
    case Window_Triangle:
        window = QVector<float>(size+1);

        for (int x = 0; x < mid; x++) {
            window[x] = x/mid + (mid-x)/mid*param;
        }
        for (int x = mid; x < size; x++) {
            window[x] = (x-mid)/(max-mid) * param + (max-x)/(max-mid);
        }
        window[size] = .5 + param/2;

#ifdef DEBUG_FFTTOOLS
        qDebug() << "Triangle window (factor " << window[size] << "):";
        for (int i = 0; i < size; i++) {
            qDebug() << window[i];
        }
        qDebug() << "Triangle window end.";
#endif

        return window;
        break;
    case Window_Hamming:
        // Use a quick version of the Hamming window here: Instead of
        // interpolating values between (-max/2) and (max/2)
        // we use integer values instead, ranging from -mid to (max-mid).
        window = QVector<float>(size+1);

        for (int x = 0; x < size; x++) {
            window[x] = .54 + .46 * cos( 2*M_PI*(x-mid) / size );
        }

        // Integrating the cosine over the window function results in
        // an area of 0; So only the constant factor 0.54 counts.
        window[size] = .54;

#ifdef DEBUG_FFTTOOLS
        qDebug() << "Hanning window (factor " << window[size] << "):";
        for (int i = 0; i < size; i++) {
            qDebug() << window[i];
        }
        qDebug() << "Hanning window end.";
#endif

        return window;
        break;
    }
    Q_ASSERT(false);
    return QVector<float>();
}

#ifdef DEBUG_FFTTOOLS
#undef DEBUG_FFTTOOLS
#endif
