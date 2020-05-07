/***************************************************************************
 *   Copyright (C) 2010 by Simon Andreas Eugster (simon.eu@gmail.com)      *
 *   This file is part of kdenlive. See www.kdenlive.org.                  *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 ***************************************************************************/

#include "histogramgenerator.h"
#include "colorconstants.h"

#include "klocalizedstring.h"
#include <QImage>
#include <QPainter>
#include <algorithm>
#include <cmath>

HistogramGenerator::HistogramGenerator() = default;

QImage HistogramGenerator::calculateHistogram(const QSize &paradeSize, const QImage &image, const int &components,
                                              ITURec rec, bool unscaled, bool logScale,
                                              uint accelFactor) const
{
    if (paradeSize.height() <= 0 || paradeSize.width() <= 0 || image.width() <= 0 || image.height() <= 0) {
        return QImage();
    }

    bool drawY = (components & HistogramGenerator::ComponentY) != 0;
    bool drawR = (components & HistogramGenerator::ComponentR) != 0;
    bool drawG = (components & HistogramGenerator::ComponentG) != 0;
    bool drawB = (components & HistogramGenerator::ComponentB) != 0;
    bool drawSum = (components & HistogramGenerator::ComponentSum) != 0;

    int r[256], g[256], b[256], y[256], s[766];
    // Initialize the values to zero
    std::fill(r, r + 256, 0);
    std::fill(g, g + 256, 0);
    std::fill(b, b + 256, 0);
    std::fill(y, y + 256, 0);
    std::fill(s, s + 766, 0);

    const uint ww = (uint)paradeSize.width();
    const uint wh = (uint)paradeSize.height();

    // Read the stats from the input image
    for (int Y = 0; Y < image.height(); ++Y) {
        for (int X = 0; X < image.width(); X += (int)accelFactor) {

            QRgb col = image.pixel(X, Y);
            r[qRed(col)]++;
            g[qGreen(col)]++;
            b[qBlue(col)]++;

            if (drawY) {
                // Use if branch to avoid expensive multiplication if Y disabled
                if (rec == ITURec::Rec_601) {
                    y[(int)floor(REC_601_R * (float) qRed(col) + REC_601_G * (float) qGreen(col) + REC_601_B * (float) qBlue(col))]++;
                } else {
                    y[(int)floor(REC_709_R * (float) qRed(col) + REC_709_G * (float) qGreen(col) + REC_709_B * (float) qBlue(col))]++;
                }
            }

            if (drawSum) {
                // Use an if branch here because the sum takes more operations than rgb
                s[qRed(col)]++;
                s[qGreen(col)]++;
                s[qBlue(col)]++;
            }
        }
    }

    const int nParts = (drawY ? 1 : 0) + (drawR ? 1 : 0) + (drawG ? 1 : 0) + (drawB ? 1 : 0) + (drawSum ? 1 : 0);
    if (nParts == 0) {
        // Nothing to draw
        return QImage();
    }

    // Distance for text
    const int d = 20;

    // Height of a single histogram box without text
    const int partH = int((int)wh - nParts * d) / nParts;

    // Total number of bytes of the image
    const uint byteCount = (uint) image.sizeInBytes();

    // Factor for scaling the measured value to the histogram.
    // This factor is used for linear scaling and does not depend
    // on the measured histogram values. Very large values,
    // e.g. in an image with a lot of white, are clipped.
    // Otherwise, the relatively low height of the histogram
    // would show all other values close to 0 when one bin is very high.
    float scaling = 0;
    int div = (int)byteCount >> 7;
    if (div > 0) {
        scaling = (float)partH / float(byteCount >> 7);
    }
    const int dist = 40;

    QImage histogram(paradeSize, QImage::Format_ARGB32);
    QPainter davinci(&histogram);
    davinci.setPen(QColor(220, 220, 220, 255));
    histogram.fill(qRgba(0, 0, 0, 0));

    QColor neutralColor(220, 220, 210, 255);
    QColor redColor(255, 128, 0, 255);
    QColor greenColor(128, 255, 0, 255);
    QColor blueColor(0, 128, 255, 255);

    int wy = 0; // Drawing position

    if (drawY) {
        drawComponentFull(&davinci, y, scaling, QRect(0, wy, (int)ww, partH + dist), neutralColor, dist, unscaled, logScale, 256);
        wy += partH + d;
    }

    if (drawSum) {
        drawComponentFull(&davinci, s, scaling / 3, QRect(0, wy, (int)ww, partH + dist), neutralColor, dist, unscaled, logScale, 256);
        wy += partH + d;
    }

    if (drawR) {
        drawComponentFull(&davinci, r, scaling, QRect(0, wy, (int)ww, partH + dist), redColor, dist, unscaled, logScale, 256);
        wy += partH + d;
    }

    if (drawG) {
        drawComponentFull(&davinci, g, scaling, QRect(0, wy, (int)ww, partH + dist), greenColor, dist, unscaled, logScale, 256);
        wy += partH + d;
    }

    if (drawB) {
        drawComponentFull(&davinci, b, scaling, QRect(0, wy, (int)ww, partH + dist), blueColor, dist, unscaled, logScale, 256);
    }

    return histogram;
}

QImage HistogramGenerator::drawComponent(const int *y, const QSize &size, const float &scaling, const QColor &color, bool unscaled, bool logScale, uint max)
{
    QImage component((int)max, size.height(), QImage::Format_ARGB32);
    component.fill(qRgba(0, 0, 0, 255));
    Q_ASSERT(scaling != INFINITY);

    const int partH = size.height();

    const int maxBinSize = *std::max_element(&y[0], &y[max - 1]);
    const float logScaling = float(size.height()) / log10f(float(maxBinSize + 1));

    for (uint x = 0; x < max; ++x) {

        // Calculate the height of the curve at position x
        int partY;
        if (logScale) {
            partY = int(logScaling * log10f(float(y[x] + 1)));
        } else {
            partY = int(scaling * (float)y[x]);
        }

        // Invert the y axis
        if (partY > partH - 1) {
            partY = partH - 1;
        }
        partY = partH - 1 - partY;

        for (int k = partH - 1; k >= partY; --k) {
            component.setPixel((int)x, k, color.rgba());
        }
    }
    if (unscaled && size.width() >= component.width()) {
        return component;
    }
    return component.scaled(size, Qt::IgnoreAspectRatio, Qt::FastTransformation);
}

void HistogramGenerator::drawComponentFull(QPainter *davinci, const int *y, const float &scaling, const QRect &rect, const QColor &color, int textSpace,
                                           bool unscaled, bool logScale, uint max)
{
    QImage component = drawComponent(y, rect.size() - QSize(0, textSpace), scaling, color, unscaled, logScale, max);
    davinci->drawImage(rect.topLeft(), component);

    uint min = 0;
    for (uint x = 0; x < max; ++x) {
        min = x;
        if (y[x] > 0) {
            break;
        }
    }
    int maxVal = (int)max - 1;
    for (int x = (int)max - 1; x >= 0; --x) {
        maxVal = x;
        if (y[x] > 0) {
            break;
        }
    }

    const int textY = rect.bottom() - textSpace + 15;
    const int dist = 40;
    const int cw = component.width();

    davinci->drawText(0, textY, i18n("min"));
    davinci->drawText(dist, textY, QString::number(min, 'f', 0));

    davinci->drawText(cw - dist - 30, textY, i18n("max"));
    davinci->drawText(cw - 30, textY, QString::number(maxVal, 'f', 0));
}
