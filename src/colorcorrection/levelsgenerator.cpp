/***************************************************************************
 *   Copyright (C) 2010 by Simon Andreas Eugster (simon.eu@gmail.com)      *
 *   This file is part of kdenlive. See www.kdenlive.org.                  *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 ***************************************************************************/

#include <algorithm>
#include <math.h>
#include <QDebug>
#include <QImage>
#include <QPainter>
#include "levelsgenerator.h"

LevelsGenerator::LevelsGenerator()
{
}

QImage LevelsGenerator::calculateLevels(const QSize &paradeSize, const QImage &image, const int &components, const uint &accelFactor) const
{
    qDebug() << "Levels rect size is: " << paradeSize.width() << "/" << paradeSize.height();
    if (paradeSize.height() <= 0 || paradeSize.width() <= 0) {
        return QImage();
    }

    bool drawY = (components & LevelsGenerator::ComponentY) != 0;
    bool drawR = (components & LevelsGenerator::ComponentR) != 0;
    bool drawG = (components & LevelsGenerator::ComponentG) != 0;
    bool drawB = (components & LevelsGenerator::ComponentB) != 0;

    int r[256], g[256], b[256], y[256];
    // Initialize the values to zero
    std::fill(r, r+255, 0);
    std::fill(g, g+255, 0);
    std::fill(b, b+255, 0);
    std::fill(y, y+255, 0);

    const uint iw = image.bytesPerLine();
    const uint ih = image.height();
    const uint ww = paradeSize.width();
    const uint wh = paradeSize.height();
    const uint byteCount = iw*ih;
    const uint stepsize = 4*accelFactor;

    const uchar *bits = image.bits();
    QRgb *col;


    // Read the stats from the input image
    for (uint i = 0; i < byteCount; i += stepsize) {
        col = (QRgb *)bits;

        r[qRed(*col)]++;
        g[qGreen(*col)]++;
        b[qBlue(*col)]++;
        y[(int)floor(.299*qRed(*col) + .587*qGreen(*col) + .114*qBlue(*col))]++;

        bits += stepsize;
    }


    const int nParts = (drawY ? 1 : 0) + (drawR ? 1 : 0) + (drawG ? 1 : 0) + (drawB ? 1 : 0);
    if (nParts == 0) {
        // Nothing to draw
        return QImage();
    }

    const int d = 20; // Distance for text
    const int partH = (wh-4*d)/nParts;
    const float scaling = (float)partH/(byteCount >> 7);

    int wy = 0; // Drawing position
    int partY; // Vertical position for the dots

    QImage levels(paradeSize, QImage::Format_ARGB32);
    QImage component(256, partH, QImage::Format_ARGB32);
    QPainter davinci(&levels);
    levels.fill(qRgba(0, 0, 0, 0));

    if (drawY) {
        qDebug() << "Drawing Y at " << wy << " with height " << partH;
        component.fill(qRgba(0, 0, 0, 0));

        for (int x = 0; x < 256; x++) {
            // Calculate the height of the curve at position x
            partY = scaling*y[x];

            // Invert the y axis
            if (partY > partH-1) { partY = partH-1; }
            partY = partH-1 - partY;

            for (int k = partH-1; k >= partY; k--) {
                component.setPixel(x, k, qRgba(220, 220, 210, 255));
            }
        }

        davinci.drawImage(0, wy, component.scaled(ww, component.height(), Qt::IgnoreAspectRatio, Qt::FastTransformation));

        wy += partH + d;
    }

    if (drawR) {
        qDebug() << "Drawing R at " << wy << " with height " << partH;
        component.fill(qRgba(0, 0, 0, 0));

        for (int x = 0; x < 256; x++) {
            // Calculate the height of the curve at position x
            partY = scaling*r[x];

            // Invert the y axis
            if (partY > partH-1) { partY = partH-1; }
            partY = partH-1 - partY;

            for (int k = partH-1; k >= partY; k--) {
                component.setPixel(x, k, qRgba(255, 128, 0, 255));
            }
        }

        davinci.drawImage(0, wy, component.scaled(ww, component.height(), Qt::IgnoreAspectRatio, Qt::FastTransformation));

        wy += partH + d;
    }

    if (drawG) {
        qDebug() << "Drawing G at " << wy << " with height " << partH;
        component.fill(qRgba(0, 0, 0, 0));

        for (int x = 0; x < 256; x++) {
            // Calculate the height of the curve at position x
            partY = scaling*g[x];

            // Invert the y axis
            if (partY > partH-1) { partY = partH-1; }
            partY = partH-1 - partY;

            for (int k = partH-1; k >= partY; k--) {
                component.setPixel(x, k, qRgba(128, 255, 0, 255));
            }
        }

        davinci.drawImage(0, wy, component.scaled(ww, component.height(), Qt::IgnoreAspectRatio, Qt::FastTransformation));

        wy += partH + d;
    }

    if (drawB) {
        qDebug() << "Drawing B at " << wy << " with height " << partH;
        component.fill(qRgba(0, 0, 0, 0));

        for (int x = 0; x < 256; x++) {
            // Calculate the height of the curve at position x
            partY = scaling*b[x];

            // Invert the y axis
            if (partY > partH-1) { partY = partH-1; }
            partY = partH-1 - partY;

            for (int k = partH-1; k >= partY; k--) {
                component.setPixel(x, k, qRgba(0, 128, 255, 255));
            }
        }

        davinci.drawImage(0, wy, component.scaled(ww, component.height(), Qt::IgnoreAspectRatio, Qt::FastTransformation));

        wy += partH + d;
    }

    return levels;
}
