/*
    SPDX-FileCopyrightText: 2010 Simon Andreas Eugster <simon.eu@gmail.com>
    This file is part of kdenlive. See www.kdenlive.org.

SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "waveformgenerator.h"

#include <cmath>

#include <QDebug>
#include <QElapsedTimer>
#include <QImage>
#include <QPainter>
#include <QSize>
#include <vector>

#define CHOP255(a) int((255) < (a) ? (255) : (a))

WaveformGenerator::WaveformGenerator() = default;

WaveformGenerator::~WaveformGenerator() = default;

QImage WaveformGenerator::calculateWaveform(const QSize &waveformSize, const QImage &image, WaveformGenerator::PaintMode paintMode, bool drawAxis, ITURec rec,
                                            uint accelFactor)
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

    const uint ww = uint(waveformSize.width());
    const uint wh = uint(waveformSize.height());
    const uint iw = uint(image.width());
    const uint ih = uint(image.height());
    const auto totalPixels = image.width() * image.height();

    std::vector<std::vector<uint>> waveValues(size_t(waveformSize.width()), std::vector<uint>(size_t(waveformSize.height()), 0));

    // Number of input pixels that will fall on one scope pixel.
    // Must be a float because the acceleration factor can be high, leading to <1 expected px per px.
    const float pixelDepth = float(totalPixels / accelFactor) / (ww * wh);
    const float gain = 255.f / (8 * pixelDepth);
    // qCDebug(KDENLIVE_LOG) << "Pixel depth: expected " << pixelDepth << "; Gain: using " << gain << " (acceleration: " << accelFactor << "x)";

    // Subtract 1 from sizes because we start counting from 0.
    // Not doing it would result in attempts to paint outside of the image.
    const float hPrediv = (wh - 1) / 255.f;
    const float wPrediv = (ww - 1) / float(iw - 1);

    for (int i = 0; i < totalPixels; i += accelFactor) {
        const int x = i % image.width();
        const QRgb pixel = image.pixel(x, i / image.width());

        float dY, dx, dy;

        if (rec == ITURec::Rec_601) {
            // CIE 601 Luminance
            dY = REC_601_R * qRed(pixel) + REC_601_G * qGreen(pixel) + REC_601_B * qBlue(pixel);
        } else {
            // CIE 709 Luminance
            dY = REC_709_R * qRed(pixel) + REC_709_G * qGreen(pixel) + REC_709_B * qBlue(pixel);
        }
        // dY is on [0,255] now.

        dy = dY * hPrediv;
        dx = x * wPrediv;
        waveValues[size_t(dx)][size_t(dy)]++;
    }

    switch (paintMode) {
    case PaintMode_Green:
        for (int i = 0; i < waveformSize.width(); ++i) {
            for (int j = 0; j < waveformSize.height(); ++j) {
                // Logarithmic scale. Needs fine tuning by hand, but looks great.
                wave.setPixel(i, waveformSize.height() - j - 1,
                              qRgba(CHOP255(52 * logf(0.1f * gain * float(waveValues[size_t(i)][size_t(j)]))),
                                    CHOP255(52 * logf(gain * float(waveValues[size_t(i)][size_t(j)]))),
                                    CHOP255(52 * logf(.25f * gain * float(waveValues[size_t(i)][size_t(j)]))),
                                    CHOP255(64 * logf(gain * float(waveValues[size_t(i)][size_t(j)])))));
            }
        }
        break;
    case PaintMode_Yellow:
        for (int i = 0; i < waveformSize.width(); ++i) {
            for (int j = 0; j < waveformSize.height(); ++j) {
                wave.setPixel(i, waveformSize.height() - j - 1, qRgba(255, 242, 0, CHOP255(gain * float(waveValues[size_t(i)][size_t(j)]))));
            }
        }
        break;
    default:
        for (int i = 0; i < waveformSize.width(); ++i) {
            for (int j = 0; j < waveformSize.height(); ++j) {
                wave.setPixel(i, waveformSize.height() - j - 1, qRgba(255, 255, 255, CHOP255(2.f * gain * float(waveValues[size_t(i)][size_t(j)]))));
            }
        }
        break;
    }

    if (drawAxis) {
        QPainter davinci;
        bool ok = davinci.begin(&wave);
        if (!ok) {
            qDebug() << "Could not initialise QPainter for Waveform.";
            return wave;
        }

        QRgb opx;
        davinci.setPen(qRgba(150, 255, 200, 32));
        davinci.setCompositionMode(QPainter::CompositionMode_Overlay);
        for (int i = 0; i <= 10; ++i) {
            int dy = int(i / 10.f * (wh - 1));
            for (int x = 0; x < int(ww); ++x) {
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
