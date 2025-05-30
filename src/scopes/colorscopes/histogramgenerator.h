/*
    SPDX-FileCopyrightText: 2010 Simon Andreas Eugster <simon.eu@gmail.com>
    This file is part of kdenlive. See www.kdenlive.org.

SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include "colorconstants.h"
#include <QObject>
#include <QPalette>

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

    /**
     * Calculates a histogram display from the input image.
     * @param paradeSize
     * @param image
     * @param components OR-ed HistogramGenerator::Components flags and decide with components (Y, R, G, B) to paint.
     * @param rec
     * @param unscaled unscaled = true leaves the width at 256 if the widget is wider (to avoid scaling).
     * @param logScale Use a logarithmic instead of linear scale.
     * @param accelFactor
     * @param palette
     * @return
     */
    QImage calculateHistogram(const QSize &paradeSize, qreal scalingFactor, const QImage &image, const int &components, const ITURec rec, bool unscaled,
                              bool logScale, uint accelFactor = 1, const QPalette &palette = QPalette()) const;

    /**
     * Draws the histogram of a single component.
     *
     * @param y Bins containing the number of samples per value
     * @param size Desired box size of the histogram
     * @param scaling Use this scaling factor to scale the y values to the box height
     * @param color Color to use for drawing
     * @param unscaled Do not scale the width but take the number of bins instead (usually 256)
     * @param logScale Use logarithmic scale instead of linear
     * @param max Number of bins, usually 256
     */
    static QImage drawComponent(const int *y, const QSize &size, const float &scaling, const QColor &color, const QColor &backgroundColor, bool unscaled,
                                bool logScale, int max);

    static void drawComponentFull(QPainter *davinci, const int *y, const float &scaling, const QRect &rect, const QColor &color, const QColor &backgroundColor,
                                  int textSpace, bool unscaled, bool logScale, int max);

    enum Components { ComponentY = 1 << 0, ComponentR = 1 << 1, ComponentG = 1 << 2, ComponentB = 1 << 3, ComponentSum = 1 << 4 };
};
