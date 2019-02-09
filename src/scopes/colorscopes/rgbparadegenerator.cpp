/***************************************************************************
 *   Copyright (C) 2010 by Simon Andreas Eugster (simon.eu@gmail.com)      *
 *   This file is part of kdenlive. See www.kdenlive.org.                  *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 ***************************************************************************/

#include "rgbparadegenerator.h"
#include "klocalizedstring.h"
#include <QColor>
#include <QPainter>

#define CHOP255(a) ((255) < (a) ? (255) : int(a))
#define CHOP1255(a) ((a) < (1) ? (1) : ((a) > (255) ? (255) : (a)))

const QColor RGBParadeGenerator::colHighlight(255, 245, 235, 255);
const QColor RGBParadeGenerator::colLight(200, 200, 200, 255);
const QColor RGBParadeGenerator::colSoft(150, 150, 150, 255);

const uchar RGBParadeGenerator::distRight(40);
const uchar RGBParadeGenerator::distBottom(40);

struct StructRGB
{
    uint r;
    uint g;
    uint b;
};

RGBParadeGenerator::RGBParadeGenerator() = default;

QImage RGBParadeGenerator::calculateRGBParade(const QSize &paradeSize, const QImage &image, const RGBParadeGenerator::PaintMode paintMode, bool drawAxis,
                                              bool drawGradientRef, uint accelFactor)
{
    Q_ASSERT(accelFactor >= 1);

    if (paradeSize.width() <= 0 || paradeSize.height() <= 0 || image.width() <= 0 || image.height() <= 0) {
        return QImage();
    }
    QImage parade(paradeSize, QImage::Format_ARGB32);
    parade.fill(Qt::transparent);

    QPainter davinci(&parade);

    const uint ww = (uint)paradeSize.width();
    const uint wh = (uint)paradeSize.height();
    const uint iw = (uint)image.bytesPerLine();
    const uint ih = (uint)image.height();
    const uint byteCount = iw * ih; // Note that 1 px = 4 B

    const uchar offset = 10;
    const uint partW = (ww - 2 * offset - distRight) / 3;
    const uint partH = wh - distBottom;

    // Statistics
    uchar minR = 255, minG = 255, minB = 255, maxR = 0, maxG = 0, maxB = 0, r, g, b;

    // Number of input pixels that will fall on one scope pixel.
    // Must be a float because the acceleration factor can be high, leading to <1 expected px per px.
    const float pixelDepth = (float)((byteCount >> 2) / accelFactor) / float(partW * 255);
    const float gain = 255 / (8 * pixelDepth);
    //        qCDebug(KDENLIVE_LOG) << "Pixel depth: expected " << pixelDepth << "; Gain: using " << gain << " (acceleration: " << accelFactor << "x)";

    QImage unscaled((int)ww - distRight, 256, QImage::Format_ARGB32);
    unscaled.fill(qRgba(0, 0, 0, 0));

    const float wPrediv = (float)(partW - 1) / float((int)iw - 1);

    std::vector<std::vector<StructRGB>> paradeVals(partW, std::vector<StructRGB>(256, {0, 0, 0}));

    const uchar *bits = image.bits();
    const uint stepsize = uint(uint(image.depth() / 8) * accelFactor);

    for (uint i = 0, x = 0; i < byteCount; i += stepsize) {
        auto *col = (const QRgb *)(bits);
        r = (uchar)qRed(*col);
        g = (uchar)qGreen(*col);
        b = (uchar)qBlue(*col);

        double dx = x * (double)wPrediv;

        paradeVals[(size_t)dx][r].r++;
        paradeVals[(size_t)dx][g].g++;
        paradeVals[(size_t)dx][b].b++;

        if (r < minR) {
            minR = r;
        }
        if (g < minG) {
            minG = g;
        }
        if (b < minB) {
            minB = b;
        }
        if (r > maxR) {
            maxR = r;
        }
        if (g > maxG) {
            maxG = g;
        }
        if (b > maxB) {
            maxB = b;
        }

        bits += stepsize;
        x += stepsize;
        x %= iw; // Modulo image width, to represent the current x position in the image
    }

    const int offset1 = (int)partW + (int)offset;
    const int offset2 = 2 * (int)partW + 2 * (int)offset;
    switch (paintMode) {
    case PaintMode_RGB:
        for (int i = 0; i < (int)partW; ++i) {
            for (int j = 0; j < 256; ++j) {
                unscaled.setPixel(i, j, qRgba(255, 10, 10, CHOP255(gain * (float)paradeVals[(size_t)i][(size_t)j].r)));
                unscaled.setPixel(i + offset1, j, qRgba(10, 255, 10, CHOP255(gain * (float)paradeVals[(size_t)i][(size_t)j].g)));
                unscaled.setPixel(i + offset2, j, qRgba(10, 10, 255, CHOP255(gain * (float)paradeVals[(size_t)i][(size_t)j].b)));
            }
        }
        break;
    default:
        for (int i = 0; i < (int)partW; ++i) {
            for (int j = 0; j < 256; ++j) {
                unscaled.setPixel(i, j, qRgba(255, 255, 255, CHOP255(gain * (float)paradeVals[(size_t)i][(size_t)j].r)));
                unscaled.setPixel(i + offset1, j, qRgba(255, 255, 255, CHOP255(gain * (float)paradeVals[(size_t)i][(size_t)j].g)));
                unscaled.setPixel(i + offset2, j, qRgba(255, 255, 255, CHOP255(gain * (float)paradeVals[(size_t)i][(size_t)j].b)));
            }
        }
        break;
    }

    // Scale the image to the target height. Scaling is not accomplished before because
    // there are only 255 different values which would lead to gaps if the height is not exactly 255.
    // Don't use bilinear transformation because the fast transformation meets the goal better.
    davinci.drawImage(0, 0, unscaled.mirrored(false, true).scaled(unscaled.width(), (int)partH, Qt::IgnoreAspectRatio, Qt::FastTransformation));

    if (drawAxis) {
        QRgb opx;
        for (int i = 0; i <= 10; ++i) {
            double dy = (float)i / 10. * float((int)partH - 1);
            for (int x = 0; x < (int)ww - (int)distRight; ++x) {
                opx = parade.pixel(x, dy);
                parade.setPixel(x, dy, qRgba(CHOP255(150 + qRed(opx)), 255, CHOP255(200 + qBlue(opx)), CHOP255(32 + qAlpha(opx))));
            }
        }
    }

    if (drawGradientRef) {
        davinci.setPen(colLight);
        davinci.drawLine(0, (int)partH, (int)partW, 0);
        davinci.drawLine((int)partW + (int)offset, (int)partH, 2 * (int)partW + (int)offset, 0);
        davinci.drawLine(2 * (int)partW + 2 * (int)offset, (int)partH, 3 * (int)partW + 2 * (int)offset, 0);
    }

    const int d = 50;

    // Show numerical minimum
    if (minR == 0) {
        davinci.setPen(colHighlight);
    } else {
        davinci.setPen(colSoft);
    }
    davinci.drawText(0, (int)wh, i18n("min: "));
    if (minG == 0) {
        davinci.setPen(colHighlight);
    } else {
        davinci.setPen(colSoft);
    }
    davinci.drawText((int)partW + (int)offset, (int)wh, i18n("min: "));
    if (minB == 0) {
        davinci.setPen(colHighlight);
    } else {
        davinci.setPen(colSoft);
    }
    davinci.drawText(2 * (int)partW + 2 * (int)offset, (int)wh, i18n("min: "));

    // Show numerical maximum
    if (maxR == 255) {
        davinci.setPen(colHighlight);
    } else {
        davinci.setPen(colSoft);
    }
    davinci.drawText(0, (int)wh - 20, i18n("max: "));
    if (maxG == 255) {
        davinci.setPen(colHighlight);
    } else {
        davinci.setPen(colSoft);
    }
    davinci.drawText((int)partW + (int)offset, (int)wh - 20, i18n("max: "));
    if (maxB == 255) {
        davinci.setPen(colHighlight);
    } else {
        davinci.setPen(colSoft);
    }
    davinci.drawText(2 * (int)partW + 2 * (int)offset, (int)wh - 20, i18n("max: "));

    davinci.setPen(colLight);
    davinci.drawText(d, (int)wh, QString::number(minR, 'f', 0));
    davinci.drawText((int)partW + (int)offset + d, (int)wh, QString::number(minG, 'f', 0));
    davinci.drawText(2 * (int)partW + 2 * (int)offset + d, (int)wh, QString::number(minB, 'f', 0));

    davinci.drawText(d, (int)wh - 20, QString::number(maxR, 'f', 0));
    davinci.drawText((int)partW + (int)offset + d, (int)wh - 20, QString::number(maxG, 'f', 0));
    davinci.drawText(2 * (int)partW + 2 * (int)offset + (int)d, (int)wh - 20, QString::number(maxB, 'f', 0));

    return parade;
}

#undef CHOP255
