/***************************************************************************
 *   Copyright (C) 2010 by Simon Andreas Eugster (simon.eu@gmail.com)      *
 *   This file is part of kdenlive. See www.kdenlive.org.                  *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 ***************************************************************************/

#include "waveformgenerator.h"

#include <cmath>

#include <QImage>
#include <QPainter>
#include <QSize>
#include <QTime>

#define CHOP255(a) ((255) < (a) ? (255) : (a))

WaveformGenerator::WaveformGenerator()
{
}

WaveformGenerator::~WaveformGenerator()
{
}

QImage WaveformGenerator::calculateWaveform(const QSize &waveformSize, const QImage &image, WaveformGenerator::PaintMode paintMode,
        bool drawAxis, WaveformGenerator::Rec rec, uint accelFactor)
{
    Q_ASSERT(accelFactor >= 1);

    //QTime time;
    //time.start();

    QImage wave(waveformSize, QImage::Format_ARGB32);

    if (waveformSize.width() <= 0 || waveformSize.height() <= 0 || image.width() <= 0 || image.height() <= 0) {
        return QImage();

    } else {

        // Fill with transparent color
        wave.fill(qRgba(0, 0, 0, 0));

        const uint ww = waveformSize.width();
        const uint wh = waveformSize.height();
        const uint iw = image.bytesPerLine();
        const uint ih = image.height();
        const uint byteCount = iw * ih;

        uint waveValues[waveformSize.width()][waveformSize.height()];
        for (int i = 0; i < waveformSize.width(); ++i) {
            for (int j = 0; j < waveformSize.height(); ++j) {
                waveValues[i][j] = 0;
            }
        }

        // Number of input pixels that will fall on one scope pixel.
        // Must be a float because the acceleration factor can be high, leading to <1 expected px per px.
        const float pixelDepth = (float)((byteCount >> 2) / accelFactor) / (ww * wh);
        const float gain = 255 / (8 * pixelDepth);
        //qCDebug(KDENLIVE_LOG) << "Pixel depth: expected " << pixelDepth << "; Gain: using " << gain << " (acceleration: " << accelFactor << "x)";

        // Subtract 1 from sizes because we start counting from 0.
        // Not doing it would result in attempts to paint outside of the image.
        const float hPrediv = (float)(wh - 1) / 255;
        const float wPrediv = (float)(ww - 1) / (iw - 1);

        const uchar *bits = image.bits();
        const int bpp = image.depth() / 8;

        for (uint i = 0, x = 0; i < byteCount; i += bpp) {

            Q_ASSERT(bits < image.bits() + byteCount);

            double dY, dx, dy;
            QRgb *col = (QRgb *)bits;

            if (rec == WaveformGenerator::Rec_601) {
                // CIE 601 Luminance
                dY = .299 * qRed(*col) + .587 * qGreen(*col) + .114 * qBlue(*col);
            } else {
                // CIE 709 Luminance
                dY = .2125 * qRed(*col) + .7154 * qGreen(*col) + .0721 * qBlue(*col);
            }
            // dY is on [0,255] now.

            dy = dY * hPrediv;
            dx = x * wPrediv;
            waveValues[(int)dx][(int)dy]++;

            bits += bpp;
            x += bpp;
            if (x > iw) {
                x -= iw;
                if (accelFactor > 1) {
                    bits += bpp * iw * (accelFactor - 1);
                    i += bpp * iw * (accelFactor - 1);
                }
            }
        }

        switch (paintMode) {
        case PaintMode_Green:
            for (int i = 0; i < waveformSize.width(); ++i) {
                for (int j = 0; j < waveformSize.height(); ++j) {
                    // Logarithmic scale. Needs fine tuning by hand, but looks great.
                    wave.setPixel(i, waveformSize.height() - j - 1, qRgba(CHOP255(52 * log(0.1 * gain * waveValues[i][j])),
                                  CHOP255(52 * log(gain * waveValues[i][j])),
                                  CHOP255(52 * log(.25 * gain * waveValues[i][j])),
                                  CHOP255(64 * log(gain * waveValues[i][j]))));
                }
            }
            break;
        case PaintMode_Yellow:
            for (int i = 0; i < waveformSize.width(); ++i) {
                for (int j = 0; j < waveformSize.height(); ++j) {
                    wave.setPixel(i, waveformSize.height() - j - 1, qRgba(255, 242, 0,   CHOP255(gain * waveValues[i][j])));
                }
            }
            break;
        default:
            for (int i = 0; i < waveformSize.width(); ++i) {
                for (int j = 0; j < waveformSize.height(); ++j) {
                    wave.setPixel(i, waveformSize.height() - j - 1, qRgba(255, 255, 255, CHOP255(2 * gain * waveValues[i][j])));
                }
            }
            break;
        }

        if (drawAxis) {
            QPainter davinci(&wave);
            QRgb opx;
            davinci.setPen(qRgba(150, 255, 200, 32));
            davinci.setCompositionMode(QPainter::CompositionMode_Overlay);
            for (uint i = 0; i <= 10; ++i) {
                float dy = (float)i / 10 * (wh - 1);
                for (uint x = 0; x < ww; ++x) {
                    opx = wave.pixel(x, dy);
                    wave.setPixel(x, dy, qRgba(CHOP255(150 + qRed(opx)), 255,
                                               CHOP255(200 + qBlue(opx)), CHOP255(32 + qAlpha(opx))));
                }
            }
        }

    }

    //uint diff = time.elapsed();
    //emit signalCalculationFinished(wave, diff);

    return wave;
}
#undef CHOP255

