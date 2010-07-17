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

    enum ColorsRGB { COL_R, COL_G, COL_B };

    /**
      @brief Draws a UV plane with given Y value.
      scaling defines how far to zoom in (or out). Lower value = zoom in.
      The modified version always scales the RGB values so that at least one of them attains 255.
      If not the full rect should be filled, set circleOnly to true.
      See also: http://en.wikipedia.org/wiki/YUV and http://de.wikipedia.org/wiki/Vektorskop
     */
    QImage yuvColorWheel(const QSize& size, const unsigned char &Y, const float &scaling, const bool &modifiedVersion, const bool &circleOnly);
    /**
      @brief Draws a UV plane with given UV angle (ratio u:v stays constant)
      scaling defines how far to zoom in (or out). Lower value = zoom in.
      angle defines the angle in a default U/V plane. A vertical plane, on which Y goes from 0 to 1,
      is then laid through the UV plane, with the defined angle.
      @see yuvColorWheel()
     */
    QImage yuvVerticalPlane(const QSize &size, const float &angle, const float &scaling);
    /**
      @brief Draws a RGB plane with two values on one axis and one on the other.
      This is e.g. useful as background for a curves dialog. On the line from bottom left to top right
      are neutral colors. The colors on the y axis show what the neutral color will look like when modifying the curve.
      color defines the color to modify on the y axis. The other two components will be increased
      in equal terms (linear as well) on the x axis.
     */
    QImage rgbCurvePlane(const QSize &size, const ColorTools::ColorsRGB &color);
    /**
      @brief Draws a YPbPr plane with Pb on the x axis and Pr on the y axis.
      Y is the Y value to use.
      scaling defines how far to zoom in (or out). Lower value = zoom in.
      See also: http://de.wikipedia.org/wiki/YPbPr-Farbmodell and http://www.poynton.com/ColorFAQ.html
     */
    QImage yPbPrColorWheel(const QSize &size, const unsigned char &Y, const float &scaling, const bool &circleOnly);

signals:
    void signalYuvWheelCalculationFinished();
};

#endif // COLORTOOLS_H
