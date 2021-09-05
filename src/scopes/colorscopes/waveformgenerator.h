/***************************************************************************
 *   SPDX-FileCopyrightText: 2010 Simon Andreas Eugster (simon.eu@gmail.com)      *
 *   This file is part of kdenlive. See www.kdenlive.org.                  *
 *                                                                         *
 *   SPDX-License-Identifier: GPL-2.0-or-later
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
