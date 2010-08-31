/***************************************************************************
 *   Copyright (C) 2010 by Simon Andreas Eugster (simon.eu@gmail.com)      *
 *   This file is part of kdenlive. See www.kdenlive.org.                  *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 ***************************************************************************/

#include <QColor>
#include <QImage>
#include <QPainter>
#include <QSize>
#include <QTime>

#include "waveformgenerator.h"

#define CHOP255(a) ((255) < (a) ? (255) : (a))

WaveformGenerator::WaveformGenerator()
{
}

WaveformGenerator::~WaveformGenerator()
{
}

QImage WaveformGenerator::calculateWaveform(const QSize &waveformSize, const QImage &image, WaveformGenerator::PaintMode paintMode,
                                            const bool &drawAxis, WaveformGenerator::Rec rec, const uint &accelFactor)
{
    Q_ASSERT(accelFactor >= 1);

    QTime time;
    time.start();

    QImage wave(waveformSize, QImage::Format_ARGB32);

    if (waveformSize.width() <= 0 || waveformSize.height() <= 0 || image.width() <= 0 || image.height() <= 0) {
        return QImage();

    } else {

        // Fill with transparent color
        wave.fill(qRgba(0,0,0,0));

        QRgb *col;
        QRgb waveCol;
        QPoint wavePoint;

        double dY, dx, dy;

        const uint ww = waveformSize.width();
        const uint wh = waveformSize.height();
        const uint iw = image.bytesPerLine();
        const uint ih = image.height();
        const uint byteCount = iw*ih;

        // Subtract 1 from sizes because we start counting from 0.
        // Not doing it would result in attempts to paint outside of the image.
        const float hPrediv = (float)(wh-1)/255;
        const float wPrediv = (float)(ww-1)/(iw-1);

        const float brightnessAdjustment = accelFactor * ((float) ww*wh/(byteCount>>3));

        const uchar *bits = image.bits();
        const uint stepsize = 4*accelFactor;

        for (uint i = 0, x = 0; i < byteCount; i += stepsize) {

            col = (QRgb *)bits;

            if (rec == WaveformGenerator::Rec_601) {
                // CIE 601 Luminance
                dY = .299*qRed(*col) + .587*qGreen(*col) + .114*qBlue(*col);
            } else {
                // CIE 709 Luminance
                dY = .2125*qRed(*col) + .7154*qGreen(*col) + .0721*qBlue(*col);
            }
            // dY is on [0,255] now.

            dy = dY*hPrediv;
            dx = x*wPrediv;
            wavePoint = QPoint((int)dx, (int)(wh-1 - dy));

            waveCol = QRgb(wave.pixel(wavePoint));
            switch (paintMode) {
            case PaintMode_Green:
                wave.setPixel(wavePoint, qRgba(CHOP255(9 + qRed(waveCol)), CHOP255(36 + qGreen(waveCol)),
                                               CHOP255(18 + qBlue(waveCol)), 255));
                break;
            case PaintMode_Yellow:
                wave.setPixel(wavePoint, qRgba(255, 242,
                                               0, CHOP255(brightnessAdjustment*10+qAlpha(waveCol))));
                break;
            default:
                wave.setPixel(wavePoint, qRgba(255,255,255,
                                               CHOP255(brightnessAdjustment*32+qAlpha(waveCol))));
                break;
            }

            bits += stepsize;
            x += stepsize;
            x %= iw; // Modulo image width, to represent the current x position in the image
        }

        if (drawAxis) {
            QPainter davinci(&wave);
            QRgb opx;
            davinci.setPen(qRgba(150,255,200,32));
            davinci.setCompositionMode(QPainter::CompositionMode_Overlay);
            for (uint i = 0; i <= 10; i++) {
                dy = (float)i/10 * (wh-1);
                for (uint x = 0; x < ww; x++) {
                    opx = wave.pixel(x, dy);
                    wave.setPixel(x,dy, qRgba(CHOP255(150+qRed(opx)), 255,
                                              CHOP255(200+qBlue(opx)), CHOP255(32+qAlpha(opx))));
                }
            }
        }

    }

    uint diff = time.elapsed();
    emit signalCalculationFinished(wave, diff);

    return wave;
}
#undef CHOP255
