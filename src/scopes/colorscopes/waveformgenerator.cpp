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
#include <QPalette>
#include <QSize>
#include <vector>

#define CHOP255(a) int((255) < (a) ? (255) : (a))

WaveformGenerator::WaveformGenerator() = default;

WaveformGenerator::~WaveformGenerator() = default;

QImage WaveformGenerator::calculateWaveform(const QSize &waveformSize, qreal scalingFactor, const QImage &image, const WaveformGenerator::PaintMode paintMode,
                                            bool drawAxis, ITURec rec, uint accelFactor, const QPalette &palette)
{
    Q_ASSERT(accelFactor >= 1);

    // QTime time;
    // time.start();

    QSize scaledWaveformSize = waveformSize * scalingFactor;
    QImage wave(scaledWaveformSize, QImage::Format_ARGB32);
    wave.setDevicePixelRatio(scalingFactor);

    if (scaledWaveformSize.width() <= 0 || scaledWaveformSize.height() <= 0 || image.width() <= 0 || image.height() <= 0) {
        return QImage();
    }

    wave.fill(palette.base().color());

    const uint ww = uint(scaledWaveformSize.width());
    const uint wh = uint(scaledWaveformSize.height());
    const uint iw = uint(image.width());
    const auto totalPixels = image.width() * image.height();

    std::vector<std::vector<uint>> waveValues(size_t(scaledWaveformSize.width()), std::vector<uint>(size_t(scaledWaveformSize.height()), 0));

    // Number of input pixels that will fall on one scope pixel.
    // Must be a float because the acceleration factor can be high, leading to <1 expected px per px.
    const float pixelDepth = float(totalPixels / accelFactor) / (ww * wh);
    const float gain = 255.f / (8 * pixelDepth);

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

    // Fill background of the parade with "dark2" color from AbstractScopeWidget instead of themes base color as the different paint modes are optimized
    // for a dark background.
    QRgb darkBackground = qRgb(25, 25, 23);

    switch (paintMode) {
    case PaintMode_Green:
        for (int i = 0; i < scaledWaveformSize.width(); ++i) {
            for (int j = 0; j < scaledWaveformSize.height(); ++j) {
                // Logarithmic scale. Needs fine tuning by hand, but looks great.
                float value = gain * float(waveValues[size_t(i)][size_t(j)]);
                float logValue = value > 0.0f ? logf(value) : 0.0f;

                float rValue = 0.1f * value;
                float gValue = value;
                float bValue = 0.25f * value;

                float logR = rValue > 0.0f ? logf(rValue) : 0.0f;
                float logG = gValue > 0.0f ? logf(gValue) : 0.0f;
                float logB = bValue > 0.0f ? logf(bValue) : 0.0f;

                int alpha = CHOP255(64 * logValue);
                int inv_alpha = 255 - alpha;
                wave.setPixel(i, scaledWaveformSize.height() - j - 1,
                              qRgba(CHOP255((qRed(darkBackground) * inv_alpha + 52 * logR * alpha) / 255),
                                    CHOP255((qGreen(darkBackground) * inv_alpha + 52 * logG * alpha) / 255),
                                    CHOP255((qBlue(darkBackground) * inv_alpha + 52 * logB * alpha) / 255), 255));
            }
        }
        break;
    case PaintMode_Yellow:
        for (int i = 0; i < scaledWaveformSize.width(); ++i) {
            for (int j = 0; j < scaledWaveformSize.height(); ++j) {
                int alpha = CHOP255(gain * float(waveValues[size_t(i)][size_t(j)]));
                int inv_alpha = 255 - alpha;
                wave.setPixel(i, scaledWaveformSize.height() - j - 1,
                              qRgba(CHOP255((qRed(darkBackground) * inv_alpha + 255 * alpha) / 255),
                                    CHOP255((qGreen(darkBackground) * inv_alpha + 242 * alpha) / 255),
                                    CHOP255((qBlue(darkBackground) * inv_alpha + 0 * alpha) / 255), 255));
            }
        }
        break;
    default: // White mode
        for (int i = 0; i < scaledWaveformSize.width(); ++i) {
            for (int j = 0; j < scaledWaveformSize.height(); ++j) {
                int alpha = CHOP255(2.f * gain * float(waveValues[size_t(i)][size_t(j)]));
                int inv_alpha = 255 - alpha;
                wave.setPixel(i, scaledWaveformSize.height() - j - 1,
                              qRgba(CHOP255((qRed(darkBackground) * inv_alpha + 255 * alpha) / 255),
                                    CHOP255((qGreen(darkBackground) * inv_alpha + 255 * alpha) / 255),
                                    CHOP255((qBlue(darkBackground) * inv_alpha + 255 * alpha) / 255), 255));
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
        // inverted background color for good contrast
        QRgb penRgb = qRgba(255 - qRed(darkBackground), 255 - qGreen(darkBackground), 255 - qBlue(darkBackground), 32);

        davinci.setPen(penRgb);
        davinci.setCompositionMode(QPainter::CompositionMode_Overlay);
        for (int i = 0; i <= 10; ++i) {
            int dy = int(i / 10.f * (wh - 1));
            for (int x = 0; x < int(ww); ++x) {
                opx = wave.pixel(x, dy);
                wave.setPixel(x, dy, qRgba(CHOP255(qRed(penRgb) + qRed(opx)), 255, CHOP255(penRgb + qBlue(opx)), CHOP255(qAlpha(penRgb) + qAlpha(opx))));
            }
        }
    }

    // uint diff = time.elapsed();
    // Q_EMIT signalCalculationFinished(wave, diff);

    return wave;
}
#undef CHOP255
