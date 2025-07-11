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

const uchar WaveformGenerator::distBorder(2);

WaveformGenerator::WaveformGenerator() = default;

WaveformGenerator::~WaveformGenerator() = default;

QImage WaveformGenerator::calculateWaveform(const QSize &waveformSize, qreal scalingFactor, const QImage &image, const WaveformGenerator::PaintMode paintMode,
                                            bool drawAxis, ITURec rec, uint accelFactor)
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

    QPainter davinci;
    bool ok = davinci.begin(&wave);
    if (!ok) {
        qDebug() << "Could not initialise QPainter for Waveform.";
        return wave;
    }

    const uint ww = uint(scaledWaveformSize.width());
    const uint wh = uint(scaledWaveformSize.height());
    const uint iw = uint(image.width());
    const auto totalPixels = image.width() * image.height();

    // Calculate the actual scope area dimensions (excluding borders)
    const uint scopeW = ww - 2 * (distBorder * scalingFactor);
    const uint scopeH = wh - 2 * (distBorder * scalingFactor);
    const uint scopeWLogicalPixels = waveformSize.width() - 2 * distBorder;
    const uint scopeHLogicalPixels = waveformSize.height() - 2 * distBorder;

    std::vector<std::vector<uint>> waveValues(size_t(scopeW), std::vector<uint>(size_t(scopeH), 0));

    // Number of input pixels that will fall on one scope pixel.
    // Must be a float because the acceleration factor can be high, leading to <1 expected px per px.
    const float pixelDepth = float(totalPixels / accelFactor) / (scopeW * scopeH);
    const float gain = 255.f / (8 * pixelDepth);

    // Subtract 1 from sizes because we start counting from 0.
    // Not doing it would result in attempts to paint outside of the image.
    const float hPrediv = (scopeH - 1) / 255.f;
    const float wPrediv = (scopeW - 1) / float(iw - 1);

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
    QColor darkBackground(25, 25, 23, 255);
    // Fill color for the border around the scope
    QColor waveformBorderColor = darkBackground.lighter(120);

    // Fill the entire area with border color first
    wave.fill(waveformBorderColor);
    // Fill the actual scope area with dark background
    davinci.fillRect(QRect(distBorder, distBorder, scopeWLogicalPixels, scopeHLogicalPixels), darkBackground);

    QRgb darkBackgroundRgb = darkBackground.rgb();

    switch (paintMode) {
    case PaintMode_Green:
        for (int i = 0; i < int(scopeW); ++i) {
            for (int j = 0; j < int(scopeH); ++j) {
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
                wave.setPixel(i + distBorder, int(scopeH + distBorder) - j - 1,
                              qRgba(CHOP255((qRed(darkBackgroundRgb) * inv_alpha + 52 * logR * alpha) / 255),
                                    CHOP255((qGreen(darkBackgroundRgb) * inv_alpha + 52 * logG * alpha) / 255),
                                    CHOP255((qBlue(darkBackgroundRgb) * inv_alpha + 52 * logB * alpha) / 255), 255));
            }
        }
        break;
    case PaintMode_Yellow:
        for (int i = 0; i < int(scopeW); ++i) {
            for (int j = 0; j < int(scopeH); ++j) {
                int alpha = CHOP255(gain * float(waveValues[size_t(i)][size_t(j)]));
                int inv_alpha = 255 - alpha;
                wave.setPixel(i + distBorder, int(scopeH + distBorder) - j - 1,
                              qRgba(CHOP255((qRed(darkBackgroundRgb) * inv_alpha + 255 * alpha) / 255),
                                    CHOP255((qGreen(darkBackgroundRgb) * inv_alpha + 242 * alpha) / 255),
                                    CHOP255((qBlue(darkBackgroundRgb) * inv_alpha + 0 * alpha) / 255), 255));
            }
        }
        break;
    default: // White mode
        for (int i = 0; i < int(scopeW); ++i) {
            for (int j = 0; j < int(scopeH); ++j) {
                int alpha = CHOP255(2.f * gain * float(waveValues[size_t(i)][size_t(j)]));
                int inv_alpha = 255 - alpha;
                wave.setPixel(i + distBorder, int(scopeH + distBorder) - j - 1,
                              qRgba(CHOP255((qRed(darkBackgroundRgb) * inv_alpha + 255 * alpha) / 255),
                                    CHOP255((qGreen(darkBackgroundRgb) * inv_alpha + 255 * alpha) / 255),
                                    CHOP255((qBlue(darkBackgroundRgb) * inv_alpha + 255 * alpha) / 255), 255));
            }
        }
        break;
    }

    if (drawAxis) {
        if (paintMode != PaintMode_Green) {
            davinci.setPen(QPen(QColor(150, 255, 200, 32), 1));
        } else {
            // opposite color of the greenish base color we're using above (#005200);
            davinci.setPen(QPen(QColor(255, 255 - 52, 255, 32), 1));
        }
        for (int i = 0; i <= 10; ++i) {
            int dy = distBorder + int(i / 10.f * (scopeHLogicalPixels - 1));
            davinci.drawLine(distBorder, dy, scopeWLogicalPixels, dy);
        }
    }

    // uint diff = time.elapsed();
    // Q_EMIT signalCalculationFinished(wave, diff);

    return wave;
}
#undef CHOP255
