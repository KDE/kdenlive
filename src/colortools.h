/***************************************************************************
 *   Copyright (C) 2010 by Simon Andreas Eugster (simon.eu@gmail.com)      *
 *   This file is part of kdenlive. See www.kdenlive.org.                  *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 ***************************************************************************/

/**
  Color tools.
 */

#ifndef COLORTOOLS_H
#define COLORTOOLS_H

#include <QImage>

class ColorTools : public QObject
{
    Q_OBJECT

public:
    ColorTools();

    /**
      @brief Draws a UV plane with given Y value.
      scaling defines how far to zoom in (or out). Lower value = zoom in.
      The modified version always scales the RGB values so that at least one of them attains 255.
      If not the full rect should be filled, set circleOnly to true.
      See also: http://en.wikipedia.org/wiki/YUV and http://de.wikipedia.org/wiki/Vektorskop
     */
    QImage yuvColorWheel(const QSize& size, const unsigned char Y, const float scaling, const bool modifiedVersion, const bool circleOnly);

signals:
    void signalWheelCalculationFinished();
};

#endif // COLORTOOLS_H
