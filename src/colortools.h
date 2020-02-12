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
    explicit ColorTools(QObject *parent = nullptr);

    // enum ColorsRGB { COL_R, COL_G, COL_B, COL_Luma, COL_A, COL_RGB };
    enum class ColorsRGB { R, G, B, Luma, A, RGB };

    enum ComponentsHSV { COM_H, COM_S, COM_V };

    /**
      @brief Draws a UV plane with given Y value.
      scaling defines how far to zoom in (or out). Lower value = zoom in.
      The modified version always scales the RGB values so that at least one of them attains 255.
      If not the full rect should be filled, set circleOnly to true.
      See also: https://en.wikipedia.org/wiki/YUV and https://de.wikipedia.org/wiki/Vektorskop
     */
    QImage yuvColorWheel(const QSize &size, int Y, float scaling, bool modifiedVersion, bool circleOnly);
    /**
      @brief Draws a UV plane with given UV angle (ratio u:v stays constant)
      scaling defines how far to zoom in (or out). Lower value = zoom in.
      angle defines the angle in a default U/V plane. A vertical plane, on which Y goes from 0 to 1,
      is then laid through the UV plane, with the defined angle.
      @see yuvColorWheel()
     */
    QImage yuvVerticalPlane(const QSize &size, int angle, float scaling);
    /**
      @brief Draws a RGB plane with two values on one axis and one on the other.
      This is e.g. useful as background for a curves dialog. On the line from bottom left to top right
      are neutral colors. The colors on the y axis show what the neutral color will look like when modifying the curve.
      color defines the color to modify on the y axis. The other two components will be increased
      in equal terms (linear as well) on the x axis.
      scaling \in ]0,1] defines the maximum variance of the selected component; Choosing a value lower than 1
      simulates the case that the curves can adjust only +- scaling*255. This mainly delivers a more constant look
      when also using the Luma component for the curves display but might not represent the actual color change!
     */
    static QImage rgbCurvePlane(const QSize &size, const ColorTools::ColorsRGB &color, float scaling = 1, const QRgb &background = QRgb());

    /**
       Same as the plane except that we vary only the vertical dimension
    */
    static QImage rgbCurveLine(const QSize &size, const ColorTools::ColorsRGB &color, const QRgb &background);
    /**
      @brief Draws a YPbPr plane with Pb on the x axis and Pr on the y axis.
      Y is the Y value to use.
      scaling defines how far to zoom in (or out). Lower value = zoom in.
      See also: https://de.wikipedia.org/wiki/YPbPr-Farbmodell and https://www.poynton.com/ColorFAQ.html
     */
    QImage yPbPrColorWheel(const QSize &size, int Y, float scaling, bool circleOnly);
    /**
     @brief Draws a HSV plane with Hue on the x axis and hue difference on the y axis.
     This is for the Bézier Curves widget which allows one to change the hue (y) of a certain hue.
     MIN/MAX give the minimum/maximum hue difference, e.g. -128,+128.
     For the value ranges see:
     https://doc.qt.io/qt-5/qcolor.html#the-hsv-color-model
     */
    static QImage hsvHueShiftPlane(const QSize &size, int S, int V, int MIN, int MAX);

    /**
      Basic HSV plane with two components varying on the x and y axis.
      If both components are the same, then the y axis will be considered.
      MIN/MAX give the minimum/maximum saturation, usually 0..255.
      Missing colour components will be taken from baseColor.
      For shear == true, the image will be sheared such that the x axis goes through (0,0) and (1,1). offsetY can additionally
      shift the whole x axis vertically.
      */
    static QImage hsvCurvePlane(const QSize &size, const QColor &baseColor, const ComponentsHSV &xVariant, const ComponentsHSV &yVariant, bool shear = false,
                                const float offsetY = 0);

signals:
    void signalYuvWheelCalculationFinished();
};

#endif // COLORTOOLS_H
