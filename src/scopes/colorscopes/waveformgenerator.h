/***************************************************************************
 *   Copyright (C) 2010 by Simon Andreas Eugster (simon.eu@gmail.com)      *
 *   This file is part of kdenlive. See www.kdenlive.org.                  *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 ***************************************************************************/

#ifndef WAVEFORMGENERATOR_H
#define WAVEFORMGENERATOR_H

#include <QObject>
#include "colorconstants.h"

class QImage;
class QSize;

class WaveformGenerator : public QObject
{
    Q_OBJECT

public:
    enum PaintMode { PaintMode_Green, PaintMode_Yellow, PaintMode_White };

    WaveformGenerator();
    ~WaveformGenerator() override;

    QImage calculateWaveform(const QSize &waveformSize, const QImage &image, WaveformGenerator::PaintMode paintMode, bool drawAxis,
                             const ITURec rec, uint accelFactor = 1);
};

#endif // WAVEFORMGENERATOR_H
