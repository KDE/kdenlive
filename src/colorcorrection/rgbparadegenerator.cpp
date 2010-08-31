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

#include <QColor>
#include <QPainter>
#include <QPoint>
#include <QTime>

#define CHOP255(a) ((255) < (a) ? (255) : (a))

const QColor RGBParadeGenerator::colHighlight(255, 245, 235, 255);
const QColor RGBParadeGenerator::colLight(200, 200, 200, 255);
const QColor RGBParadeGenerator::colSoft(150, 150, 150, 255);


const uchar RGBParadeGenerator::distRight(40);
const uchar RGBParadeGenerator::distBottom(40);

RGBParadeGenerator::RGBParadeGenerator()
{
}

QImage RGBParadeGenerator::calculateRGBParade(const QSize &paradeSize, const QImage &image,
                                              const RGBParadeGenerator::PaintMode paintMode, const bool &drawAxis, 
                                              const bool &drawGradientRef, const uint &accelFactor)
{
    Q_ASSERT(accelFactor >= 1);

    if (paradeSize.width() <= 0 || paradeSize.height() <= 0 || image.width() <= 0 || image.height() <= 0) {
        return QImage();

    } else {
        QImage parade(paradeSize, QImage::Format_ARGB32);
        parade.fill(qRgba(0,0,0,0));

        QRgb *col;
        QRgb paradeCol;
        QPoint paradePoint;
        QPainter davinci(&parade);

        double dx, dy;

        const uint ww = paradeSize.width();
        const uint wh = paradeSize.height();
        const uint iw = image.bytesPerLine();
        const uint ih = image.height();
        const uint byteCount = iw*ih;

        const uchar offset = 10;
        const int partW = (ww - 2*offset - distRight) / 3;
        const int partH = wh - distBottom;

        // To get constant brightness, independant of acceleration factor and input image size
        // Must be a float because the acceleration factor can be high, leading to <1 expected px per px.
        // Divide by 3 because of the 3 components.
        const float brightnessAdjustment = accelFactor * ((float) ww*wh/(byteCount>>3)) / 3;

        uchar minR = 255, minG = 255, minB = 255, maxR = 0, maxG = 0, maxB = 0, r, g, b;
//        qDebug() << "Expecting about " << avgPxPerPx << " pixels per pixel in the RGB parade. Weakening by " << weaken
//                << " with an acceleration factor of " << accelFactor;


        QImage unscaled(ww-distRight, 256, QImage::Format_ARGB32);
        unscaled.fill(qRgba(0, 0, 0, 0));

        const float wPrediv = (float)(partW-1)/(iw-1);

        const uchar *bits = image.bits();
        const uint stepsize = 4*accelFactor;

        for (uint i = 0, x = 0; i < byteCount; i += stepsize) {

            col = (QRgb *)bits;
            r = qRed(*col);
            g = qGreen(*col);
            b = qBlue(*col);

            dx = x*wPrediv;

            paradePoint = QPoint((int)dx, r);
            paradeCol = QRgb(unscaled.pixel(paradePoint));
            switch(paintMode) {
            case PaintMode_RGB:
                unscaled.setPixel(paradePoint, qRgba(255,10,10, CHOP255(brightnessAdjustment*16 + qAlpha(paradeCol))));
                break;
            default:
                unscaled.setPixel(paradePoint, qRgba(255,255,255, CHOP255(brightnessAdjustment*16 + qAlpha(paradeCol))));
                break;
            }

            paradePoint = QPoint((int) (dx + partW + offset), g);
            paradeCol = QRgb(unscaled.pixel(paradePoint));
            switch(paintMode) {
            case PaintMode_RGB:
                unscaled.setPixel(paradePoint, qRgba(10,255,10, CHOP255(brightnessAdjustment*16 + qAlpha(paradeCol))));
                break;
            default:
                unscaled.setPixel(paradePoint, qRgba(255,255,255, CHOP255(brightnessAdjustment*16 + qAlpha(paradeCol))));
                break;
            }

            paradePoint = QPoint((int) (dx + 2*partW + 2*offset), b);
            paradeCol = QRgb(unscaled.pixel(paradePoint));
            switch(paintMode) {
            case PaintMode_RGB:
                unscaled.setPixel(paradePoint, qRgba(10,10,255, CHOP255(brightnessAdjustment*16 + qAlpha(paradeCol))));
                break;
            default:
                unscaled.setPixel(paradePoint, qRgba(255,255,255, CHOP255(brightnessAdjustment*16 + qAlpha(paradeCol))));
                break;
            }


            if (r < minR) { minR = r; }
            if (g < minG) { minG = g; }
            if (b < minB) { minB = b; }
            if (r > maxR) { maxR = r; }
            if (g > maxG) { maxG = g; }
            if (b > maxB) { maxB = b; }

            bits += stepsize;
            x += stepsize;
            x %= iw; // Modulo image width, to represent the current x position in the image
        }
        // Scale the image to the target height. Scaling is not accomplished before because
        // there are only 255 different values which would lead to gaps if the height is not exactly 255.
        // Don't use bilinear transformation because the fast transformation meets the goal better.
        davinci.drawImage(0, 0, unscaled.mirrored(false, true).scaled(unscaled.width(), partH, Qt::IgnoreAspectRatio, Qt::FastTransformation));

        if (drawAxis) {
            QRgb opx;
            for (uint i = 0; i <= 10; i++) {
                dy = (float)i/10 * (partH-1);
                for (uint x = 0; x < ww-distRight; x++) {
                    opx = parade.pixel(x, dy);
                    parade.setPixel(x,dy, qRgba(CHOP255(150+qRed(opx)), 255,
                                              CHOP255(200+qBlue(opx)), CHOP255(32+qAlpha(opx))));
                }
            }
        }
        
        if (drawGradientRef) {
            davinci.setPen(colLight);
            davinci.drawLine(0                 ,partH,   partW,           0);
            davinci.drawLine(  partW +   offset,partH, 2*partW +   offset,0);
            davinci.drawLine(2*partW + 2*offset,partH, 3*partW + 2*offset,0);
        }


        const int d = 50;

        // Show numerical minimum
        if (minR == 0) { davinci.setPen(colHighlight); } else { davinci.setPen(colSoft); }
        davinci.drawText(0,                     wh, "min: ");
        if (minG == 0) { davinci.setPen(colHighlight); } else { davinci.setPen(colSoft); }
        davinci.drawText(partW + offset,        wh, "min: ");
        if (minB == 0) { davinci.setPen(colHighlight); } else { davinci.setPen(colSoft); }
        davinci.drawText(2*partW + 2*offset,    wh, "min: ");

        // Show numerical maximum
        if (maxR == 255) { davinci.setPen(colHighlight); } else { davinci.setPen(colSoft); }
        davinci.drawText(0,                     wh-20, "max: ");
        if (maxG == 255) { davinci.setPen(colHighlight); } else { davinci.setPen(colSoft); }
        davinci.drawText(partW + offset,        wh-20, "max: ");
        if (maxB == 255) { davinci.setPen(colHighlight); } else { davinci.setPen(colSoft); }
        davinci.drawText(2*partW + 2*offset,    wh-20, "max: ");

        davinci.setPen(colLight);
        davinci.drawText(d,                        wh, QString::number(minR, 'f', 0));
        davinci.drawText(partW + offset + d,       wh, QString::number(minG, 'f', 0));
        davinci.drawText(2*partW + 2*offset + d,   wh, QString::number(minB, 'f', 0));

        davinci.drawText(d,                        wh-20, QString::number(maxR, 'f', 0));
        davinci.drawText(partW + offset + d,       wh-20, QString::number(maxG, 'f', 0));
        davinci.drawText(2*partW + 2*offset + d,   wh-20, QString::number(maxB, 'f', 0));

        davinci.drawText(ww-distRight+5,            10,      "255");
        davinci.drawText(ww-distRight+5,            partH+5,  "0");



        return parade;
    }
}

#undef CHOP255
