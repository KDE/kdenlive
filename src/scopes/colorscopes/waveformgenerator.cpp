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
#include "colorconstants.h"

#include <cmath>

#include <QImage>
#include <QPainter>
#include <QSize>
#include <QElapsedTimer>
#include <vector>

#define CHOP255(a) ((255) < (a) ? (255) : (a))

WaveformGenerator::WaveformGenerator() = default;

WaveformGenerator::~WaveformGenerator() = default;

QImage WaveformGenerator::calculateWaveform(const QSize &waveformSize, const QImage &image, WaveformGenerator::PaintMode paintMode, bool drawAxis,
                                            ITURec rec, uint accelFactor)
{
    Q_ASSERT(accelFactor >= 1);

    // QTime time;
    // time.start();

    QImage wave(waveformSize, QImage::Format_ARGB32);

    if (waveformSize.width() <= 0 || waveformSize.height() <= 0 || image.width() <= 0 || image.height() <= 0) {
        return QImage();
    }

    // Fill with transparent color
    wave.fill(qRgba(0, 0, 0, 0));

    const uint ww = (uint)waveformSize.width();
    const uint wh = (uint)waveformSize.height();
    const uint iw = (uint)image.bytesPerLine();
    const uint ih = (uint)image.height();
    const uint byteCount = iw * ih;

    std::vector<std::vector<uint>> waveValues((size_t)waveformSize.width(), std::vector<uint>((size_t)waveformSize.height(), 0));

    // Number of input pixels that will fall on one scope pixel.
    // Must be a float because the acceleration factor can be high, leading to <1 expected px per px.
    const float pixelDepth = (float)((byteCount >> 2) / accelFactor) / float(ww * wh);
    const float gain = 255. / (8. * pixelDepth);
    // qCDebug(KDENLIVE_LOG) << "Pixel depth: expected " << pixelDepth << "; Gain: using " << gain << " (acceleration: " << accelFactor << "x)";

    // Subtract 1 from sizes because we start counting from 0.
    // Not doing it would result in attempts to paint outside of the image.
    const float hPrediv = (float)(wh - 1) / 255.;
    const float wPrediv = (float)(ww - 1) / float(iw - 1);

    const uchar *bits = image.bits();
    const int bpp = image.depth() / 8;

    for (uint i = 0, x = 0; i < byteCount; i += (uint)bpp) {

        Q_ASSERT(bits < image.bits() + byteCount);

        double dY, dx, dy;
        auto *col = (const QRgb *)bits;

        if (rec == ITURec::Rec_601) {
            // CIE 601 Luminance
            dY = REC_601_R * float(qRed(*col)) + REC_601_G * float(qGreen(*col)) + REC_601_B * float(qBlue(*col));
        } else {
            // CIE 709 Luminance
            dY = REC_709_R * float(qRed(*col)) + REC_709_G * float(qGreen(*col)) + REC_709_B * float(qBlue(*col));
        }
        // dY is on [0,255] now.

        dy = dY * hPrediv;
        dx = (float)x * wPrediv;
        waveValues[(size_t)dx][(size_t)dy]++;

        bits += bpp;
        x += (uint)bpp;
        if (x > iw) {
            x -= iw;
            if (accelFactor > 1) {
                bits += bpp * (int)iw * ((int)accelFactor - 1);
                i += (uint)bpp * iw * (accelFactor - 1);
            }
        }
    }

    switch (paintMode) {
    case PaintMode_Green:
        for (int i = 0; i < waveformSize.width(); ++i) {
            for (int j = 0; j < waveformSize.height(); ++j) {
                // Logarithmic scale. Needs fine tuning by hand, but looks great.
                wave.setPixel(i, waveformSize.height() - j - 1,
                              qRgba(CHOP255(52 * log(0.1 * gain * (float)waveValues[(size_t)i][(size_t)j])),
                                    CHOP255(52 * std::log(gain * (float)waveValues[(size_t)i][(size_t)j])),
                                    CHOP255(52 * log(.25 * gain * (float)waveValues[(size_t)i][(size_t)j])),
                                    CHOP255(64 * std::log(gain * (float)waveValues[(size_t)i][(size_t)j]))));
            }
        }
        break;
    case PaintMode_Yellow:
        for (int i = 0; i < waveformSize.width(); ++i) {
            for (int j = 0; j < waveformSize.height(); ++j) {
                wave.setPixel(i, waveformSize.height() - j - 1, qRgba(255, 242, 0, CHOP255(gain * (float)waveValues[(size_t)i][(size_t)j])));
            }
        }
        break;
    default:
        for (int i = 0; i < waveformSize.width(); ++i) {
            for (int j = 0; j < waveformSize.height(); ++j) {
                wave.setPixel(i, waveformSize.height() - j - 1, qRgba(255, 255, 255, CHOP255(2. * gain * (float)waveValues[(size_t)i][(size_t)j])));
            }
        }
        break;
    }

    if (drawAxis) {
        QPainter davinci(&wave);
        QRgb opx;
        davinci.setPen(qRgba(150, 255, 200, 32));
        davinci.setCompositionMode(QPainter::CompositionMode_Overlay);
        for (int i = 0; i <= 10; ++i) {
            float dy = (float)i / 10. * ((int)wh - 1);
            for (int x = 0; x < (int)ww; ++x) {
                opx = wave.pixel(x, dy);
                wave.setPixel(x, dy, qRgba(CHOP255(150 + qRed(opx)), 255, CHOP255(200 + qBlue(opx)), CHOP255(32 + qAlpha(opx))));
            }
        }
    }

    // uint diff = time.elapsed();
    // emit signalCalculationFinished(wave, diff);

    return wave;
}
#undef CHOP255
