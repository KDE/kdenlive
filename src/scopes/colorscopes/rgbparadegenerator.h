/*
    SPDX-FileCopyrightText: 2010 Simon Andreas Eugster <simon.eu@gmail.com>
    This file is part of kdenlive. See www.kdenlive.org.

SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include <QObject>

class QColor;
class QImage;
class QSize;
class RGBParadeGenerator : public QObject
{
    Q_OBJECT
public:
    enum PaintMode { PaintMode_RGB, PaintMode_White };

    RGBParadeGenerator();
    QImage calculateRGBParade(const QSize &paradeSize, const QImage &image, const RGBParadeGenerator::PaintMode paintMode, bool drawAxis, bool drawGradientRef,
                              uint accelFactor = 1);

    static const QColor colHighlight;
    static const QColor colLight;
    static const QColor colSoft;

    static const uchar distRight;
    static const uchar distBottom;
};
