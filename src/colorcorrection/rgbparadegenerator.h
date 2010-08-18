/***************************************************************************
 *   Copyright (C) 2010 by Simon Andreas Eugster (simon.eu@gmail.com)      *
 *   This file is part of kdenlive. See www.kdenlive.org.                  *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 ***************************************************************************/

#ifndef RGBPARADEGENERATOR_H
#define RGBPARADEGENERATOR_H

#include <QObject>

class QColor;
class QImage;
class QSize;
class RGBParadeGenerator : public QObject
{
public:
    enum PaintMode { PaintMode_RGB, PaintMode_RGB2 };

    RGBParadeGenerator();
    QImage calculateRGBParade(const QSize &paradeSize, const QImage &image, const RGBParadeGenerator::PaintMode paintMode,
                              const bool &drawAxis, const bool &drawGradientRef, const uint &accelFactor = 1);

    static const QColor colHighlight;
    static const QColor colLight;
    static const QColor colSoft;
};

#endif // RGBPARADEGENERATOR_H
