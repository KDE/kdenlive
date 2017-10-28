/***************************************************************************
 *   Copyright (C) 2010 by Simon Andreas Eugster (simon.eu@gmail.com)      *
 *   This file is part of kdenlive. See www.kdenlive.org.                  *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 ***************************************************************************/

#ifndef VECTORSCOPEGENERATOR_H
#define VECTORSCOPEGENERATOR_H

#include <QImage>
#include <QObject>

class QImage;
class QPoint;
class QPointF;
class QSize;

class VectorscopeGenerator : public QObject
{
    Q_OBJECT

public:
    enum ColorSpace { ColorSpace_YUV, ColorSpace_YPbPr };
    enum PaintMode { PaintMode_Green, PaintMode_Green2, PaintMode_Original, PaintMode_Chroma, PaintMode_YUV, PaintMode_Black };

    QImage calculateVectorscope(const QSize &vectorscopeSize, const QImage &image, const float &gain, const VectorscopeGenerator::PaintMode &paintMode,
                                const VectorscopeGenerator::ColorSpace &colorSpace, bool, uint accelFactor = 1) const;

    QPoint mapToCircle(const QSize &targetSize, const QPointF &point) const;
    static const float scaling;

signals:
    void signalCalculationFinished(const QImage &image, uint ms);
};

#endif // VECTORSCOPEGENERATOR_H
