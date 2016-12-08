/***************************************************************************
 *   Copyright (C) 2010 by Simon Andreas Eugster (simon.eu@gmail.com)      *
 *   This file is part of kdenlive. See www.kdenlive.org.                  *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 ***************************************************************************/

#ifndef HISTOGRAMGENERATOR_H
#define HISTOGRAMGENERATOR_H

#include <QObject>

class QColor;
class QImage;
class QPainter;
class QRect;
class QSize;

class HistogramGenerator : public QObject
{
    Q_OBJECT
public:
    explicit HistogramGenerator();

    /** Recommendation to use.
        See http://www.poynton.com/ColorFAQ.html for details. */
    enum Rec { Rec_601, Rec_709 };

    /**
        Calculates a histogram display from the input image.
        components are OR-ed HistogramGenerator::Components flags and decide with components (Y, R, G, B) to paint.
        unscaled = true leaves the width at 256 if the widget is wider (to avoid scaling). */
    QImage calculateHistogram(const QSize &paradeSize, const QImage &image, const int &components, const HistogramGenerator::Rec rec,
                              bool unscaled, uint accelFactor = 1) const;

    QImage drawComponent(const int *y, const QSize &size, const float &scaling, const QColor &color, bool unscaled, uint max) const;

    void drawComponentFull(QPainter *davinci, const int *y, const float &scaling, const QRect &rect,
                           const QColor &color, int textSpace, bool unscaled, uint max) const;

    enum Components { ComponentY = 1 << 0, ComponentR = 1 << 1, ComponentG = 1 << 2, ComponentB = 1 << 3, ComponentSum = 1 << 4 };

};

#endif // HISTOGRAMGENERATOR_H
