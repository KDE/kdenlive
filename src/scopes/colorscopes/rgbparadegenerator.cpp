/*
    SPDX-FileCopyrightText: 2010 Simon Andreas Eugster <simon.eu@gmail.com>
    This file is part of kdenlive. See www.kdenlive.org.

SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "rgbparadegenerator.h"
#include "klocalizedstring.h"
#include <QColor>
#include <QDebug>
#include <QPainter>

#define CHOP255(a) ((255) < (a) ? (255) : int(a))
#define CHOP1255(a) ((a) < (1) ? (1) : ((a) > (255) ? (255) : (a)))

const uchar RGBParadeGenerator::distRight(31);
const uchar RGBParadeGenerator::distBottom(40);
const uchar RGBParadeGenerator::distBorder(2);

struct StructRGB
{
    uint r;
    uint g;
    uint b;
};

RGBParadeGenerator::RGBParadeGenerator() = default;

QImage RGBParadeGenerator::calculateRGBParade(const QSize &paradeSize, qreal scalingFactor, const QImage &image, const RGBParadeGenerator::PaintMode paintMode,
                                              bool drawAxis, bool drawGradientRef, uint accelFactor, const QPalette &palette)
{
    Q_ASSERT(accelFactor >= 1);

    if (paradeSize.width() <= 0 || paradeSize.height() <= 0 || image.width() <= 0 || image.height() <= 0) {
        return QImage();
    }
    QImage parade(paradeSize * scalingFactor, QImage::Format_ARGB32);
    parade.setDevicePixelRatio(scalingFactor);
    parade.fill(palette.window().color());

    QPainter davinci;
    bool ok = davinci.begin(&parade);
    if (!ok) {
        qDebug() << "Could not initialise QPainter for RGB parade.";
        return parade;
    }

    const uint ww = uint(paradeSize.width());
    const uint wh = uint(paradeSize.height());
    const uint iw = uint(image.width());
    const uint ih = uint(image.height());

    const uchar offset = 8;
    const uint partW = (ww - 2 * offset - distRight - 2 * distBorder) / 3;
    const uint partH = wh - distBottom - 2 * distBorder;

    // Statistics
    uchar minR = 255, minG = 255, minB = 255, maxR = 0, maxG = 0, maxB = 0;

    // Number of input pixels that will fall on one scope pixel.
    // Must be a float because the acceleration factor can be high, leading to <1 expected px per px.
    const float pixelDepth = float((iw * ih) / accelFactor) / (partW * 255);
    const float gain = 255 / (8 * pixelDepth);
    //        qCDebug(KDENLIVE_LOG) << "Pixel depth: expected " << pixelDepth << "; Gain: using " << gain << " (acceleration: " << accelFactor << "x)";

    QImage unscaled(int(ww) - distRight, 256, QImage::Format_ARGB32);
    unscaled.fill(qRgba(0, 0, 0, 0));

    const float wPrediv = float(partW - 1) / (iw - 1);

    std::vector<std::vector<StructRGB>> paradeVals(partW, std::vector<StructRGB>(256, {0, 0, 0}));

    const auto totalPixels = image.width() * image.height();
    for (int i = 0; i < totalPixels; i += accelFactor) {
        const auto x = i % image.width();
        const QRgb pixel = image.pixel(x, i / image.width());
        auto r = uchar(qRed(pixel));
        auto g = uchar(qGreen(pixel));
        auto b = uchar(qBlue(pixel));

        double dx = x * double(wPrediv);

        paradeVals[size_t(dx)][r].r++;
        paradeVals[size_t(dx)][g].g++;
        paradeVals[size_t(dx)][b].b++;

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
    }

    const int offset1 = int(partW + offset);
    const int offset2 = int(2 * partW + 2 * offset);

    // Fill background of the parade with "dark2" color from AbstractScopeWidget instead of themes base color.
    // Otherwise, on a light theme contrast is not great and other drawing options depend on this assumption like white paint mode.
    QColor darkParadeBackground(25, 25, 23, 255);
    // Fill color for the border around the scope as well as the area between the individual color channels
    QColor paradeBorderColor = darkParadeBackground.lighter(120);

    davinci.fillRect(QRect(0, 0, ww - distRight, partH + 2 * distBorder), paradeBorderColor);

    davinci.fillRect(QRect(distBorder, distBorder, partW, partH), darkParadeBackground);
    davinci.fillRect(QRect(offset1 + distBorder, distBorder, partW, partH), darkParadeBackground);
    davinci.fillRect(QRect(offset2 + distBorder, distBorder, partW, partH), darkParadeBackground);

    switch (paintMode) {
    case PaintMode_RGB:
        for (int i = 0; i < int(partW); ++i) {
            for (int j = 0; j < 256; ++j) {
                unscaled.setPixel(i, j, qRgba(255, 10, 10, CHOP255(gain * float(paradeVals[size_t(i)][size_t(j)].r))));
                unscaled.setPixel(i + offset1, j, qRgba(10, 255, 10, CHOP255(gain * float(paradeVals[size_t(i)][size_t(j)].g))));
                unscaled.setPixel(i + offset2, j, qRgba(10, 10, 255, CHOP255(gain * float(paradeVals[size_t(i)][size_t(j)].b))));
            }
        }
        break;
    default:
        for (int i = 0; i < int(partW); ++i) {
            for (int j = 0; j < 256; ++j) {
                unscaled.setPixel(i, j, qRgba(255, 255, 255, CHOP255(gain * float(paradeVals[size_t(i)][size_t(j)].r))));
                unscaled.setPixel(i + offset1, j, qRgba(255, 255, 255, CHOP255(gain * float(paradeVals[size_t(i)][size_t(j)].g))));
                unscaled.setPixel(i + offset2, j, qRgba(255, 255, 255, CHOP255(gain * float(paradeVals[size_t(i)][size_t(j)].b))));
            }
        }
        break;
    }

    // Scale the image to the target height. Scaling is not accomplished before because
    // there are only 255 different values which would lead to gaps if the height is not exactly 255.
    // Don't use bilinear transformation because the fast transformation meets the goal better.
    davinci.drawImage(distBorder, distBorder,
                      unscaled.mirrored(false, true).scaled(unscaled.width(), int(partH), Qt::IgnoreAspectRatio, Qt::FastTransformation));

    if (drawAxis) {
        davinci.setPen(QPen(QColor(150, 255, 200, 32), 1));
        for (int i = 0; i <= 10; ++i) {
            int dy = distBorder + i * int(partH) / 10;
            davinci.drawLine(distBorder, dy, int(ww - distRight - distBorder), dy);
        }
    }

    if (drawGradientRef) {
        davinci.setPen(QPen(QColor(150, 255, 200, 96), 1));
        davinci.drawLine(distBorder, int(partH + distBorder), int(partW + distBorder), distBorder);
        davinci.drawLine(int(partW + offset + distBorder), int(partH + distBorder), int(2 * partW + offset + distBorder), distBorder);
        davinci.drawLine(int(2 * partW + 2 * offset + distBorder), int(partH + distBorder), int(3 * partW + 2 * offset + distBorder), distBorder);
    }

    const int d = 50;

    // Show numerical minimum
    if (minR == 0) {
        davinci.setPen(palette.highlight().color());
    } else {
        davinci.setPen(palette.text().color());
    }
    davinci.drawText(distBorder, int(wh), i18n("min: "));
    if (minG == 0) {
        davinci.setPen(palette.highlight().color());
    } else {
        davinci.setPen(palette.text().color());
    }
    davinci.drawText(int(partW + offset + distBorder), int(wh), i18n("min: "));
    if (minB == 0) {
        davinci.setPen(palette.highlight().color());
    } else {
        davinci.setPen(palette.text().color());
    }
    davinci.drawText(int(2 * partW + 2 * offset + distBorder), int(wh), i18n("min: "));

    // Show numerical maximum
    if (maxR == 255) {
        davinci.setPen(palette.highlight().color());
    } else {
        davinci.setPen(palette.text().color());
    }
    davinci.drawText(distBorder, int(wh - 20), i18n("max: "));
    if (maxG == 255) {
        davinci.setPen(palette.highlight().color());
    } else {
        davinci.setPen(palette.text().color());
    }
    davinci.drawText(int(partW + offset + distBorder), int(wh) - 20, i18n("max: "));
    if (maxB == 255) {
        davinci.setPen(palette.highlight().color());
    } else {
        davinci.setPen(palette.text().color());
    }
    davinci.drawText(int(2 * partW + 2 * offset + distBorder), int(wh - 20), i18n("max: "));

    davinci.setPen(palette.text().color());
    davinci.drawText(d, int(wh), QString::number(minR, 'f', 0));
    davinci.drawText(int(partW + offset + d), int(wh), QString::number(minG, 'f', 0));
    davinci.drawText(int(2 * partW + 2 * offset + d), int(wh), QString::number(minB, 'f', 0));

    davinci.drawText(d, int(wh - 20), QString::number(maxR, 'f', 0));
    davinci.drawText(int(partW + offset + d), int(wh) - 20, QString::number(maxG, 'f', 0));
    davinci.drawText(int(2 * partW + 2 * offset + d), int(wh - 20), QString::number(maxB, 'f', 0));

    return parade;
}

#undef CHOP255
