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

  Vectorscope.

  The basis matrix for converting RGB to YUV is
  (in this example for calculating the YUV value of red):

mRgb2Yuv =                       r =

   0.29900   0.58700   0.11400     1.00000
  -0.14741  -0.28939   0.43680  x  0.00000
   0.61478  -0.51480  -0.09998     0.00000

  The resulting YUV value is then drawn on the circle
  using U and V as coordinate values.

  The maximum length of such an UV vector is reached
  for the colors Red and Cyan: 0.632.
  To make optimal use of space in the circle, this value
  can be used for scaling.

  As we are dealing with RGB values in a range of {0,...,255}
  and the conversion values are made for [0,1], we already
  divide the conversion values by 255 previously, e.g. in
  GNU octave.

  The basis matrix for converting RGB to YPbPr is:

mRgb2YPbPr =                        r =

   0.299000   0.587000   0.114000     1.0000
  -0.168736  -0.331264   0.500000  x  0.0000
   0.500000  -0.418688  -0.081312     0.0000

   Note that YUV and YPbPr are not the same.
   * YUV is used for analog transfer of PAL/NTSC
   * YPbPr is used for analog transfer of YCbCr sources
     like DVD/DVB.
   It does _not_ matter for the Vectorscope what color space
   the input video is; The used color space is just a tool
   to visualize the hue. The difference is merely that the
   scope looks slightly different when choosing the respectively
   other color space; The way the Vectorscope works stays the same.

   YCbCr is basically YPbPr for digital use (it is defined
   on a range of {16,219} for Y and {16,240} for Cb/Cr
   components).

  See also:
    http://www.poynton.com/ColorFAQ.html
    https://de.wikipedia.org/wiki/Vektorskop
    https://www.elektroniktutor.de/geraetetechnik/vektskop.html

 */

#include "vectorscopegenerator.h"
#include <QImage>
#include <cmath>

// The maximum distance from the center for any RGB color is 0.63, so
// no need to make the circle bigger than required.
const float SCALING = 1 / .7;

const float VectorscopeGenerator::scaling = 1 / .7;

/**
  Input point is on [-1,1]², 0 being at the center,
  and positive directions are →top/→right.

  Maps to the coordinates used in QImages with the 0 point
  at the top left corner.

 -1          +1
+1+-----------+
  |    +      |
  |  --0++    |
  |    -      |
-1+-----------+
     vvv
   mapped to
      v
  0      x
 0+------+
  |0++   |
  |-     |
  |-     |
 y+------+

  With y:
  1. Scale from [-1,1] to [0,1] with                    y01 := (y+1)/2
  2. Invert (Orientation of the y axis changes) with    y10 := 1-y01
  3. Scale from [1,0] to [height-1,0] with              yy  := (height-1) * y10
  x does not need to be inverted.

 */
QPoint VectorscopeGenerator::mapToCircle(const QSize &targetSize, const QPointF &point) const
{
    return {int((targetSize.width() - 1) * (point.x() + 1) / 2), int((targetSize.height() - 1) * (1 - (point.y() + 1) / 2))};
}

QImage VectorscopeGenerator::calculateVectorscope(const QSize &vectorscopeSize, const QImage &image, const float &gain,
                                                  const VectorscopeGenerator::PaintMode &paintMode, const VectorscopeGenerator::ColorSpace &colorSpace, bool,
                                                  uint accelFactor) const
{
    if (vectorscopeSize.width() <= 0 || vectorscopeSize.height() <= 0 || image.width() <= 0 || image.height() <= 0) {
        // Invalid size
        return QImage();
    }

    // Prepare the vectorscope data
    const int cw = (vectorscopeSize.width() < vectorscopeSize.height()) ? vectorscopeSize.width() : vectorscopeSize.height();
    QImage scope = QImage(cw, cw, QImage::Format_ARGB32);
    scope.fill(qRgba(0, 0, 0, 0));

    const uchar *bits = image.bits();

    double dy, dr, dg, db, dmax;
    double /*y,*/ u, v;
    QPoint pt;
    QRgb px;

    const int stepsize = int(uint(image.depth() / 8) * accelFactor);

    // Just an average for the number of image pixels per scope pixel.
    // NOTE: byteCount() has to be replaced by (img.bytesPerLine()*img.height()) for Qt 4.5 to compile, see:
    // https://doc.qt.io/qt-5/qimage.html#bytesPerLine
    double avgPxPerPx = (double)image.depth() / 8 * (image.bytesPerLine() * image.height()) / scope.size().width() / scope.size().height() / accelFactor;

    for (int i = 0; i < (image.bytesPerLine() * image.height()); i += stepsize) {
        auto *col = (const QRgb *)(bits);

        int r = qRed(*col);
        int g = qGreen(*col);
        int b = qBlue(*col);

        switch (colorSpace) {
        case VectorscopeGenerator::ColorSpace_YUV:
            //             y = (double)  0.001173 * r +0.002302 * g +0.0004471* b;
            u = (double)-0.0005781 * r - 0.001135 * g + 0.001713 * b;
            v = (double)0.002411 * r - 0.002019 * g - 0.0003921 * b;
            break;
        case VectorscopeGenerator::ColorSpace_YPbPr:
        default:
            //             y = (double)  0.001173 * r +0.002302 * g +0.0004471* b;
            u = (double)-0.0006671 * r - 0.001299 * g + 0.0019608 * b;
            v = (double)0.001961 * r - 0.001642 * g - 0.0003189 * b;
            break;
        }

        pt = mapToCircle(vectorscopeSize, QPointF(SCALING * gain * u, SCALING * gain * v));

        if (pt.x() >= scope.width() || pt.x() < 0 || pt.y() >= scope.height() || pt.y() < 0) {
            // Point lies outside (because of scaling), don't plot it

        } else {

            // Draw the pixel using the chosen draw mode.
            switch (paintMode) {
            case PaintMode_YUV:
                // see yuvColorWheel
                dy = 128; // Default Y value. Lower = darker.

                // Calculate the RGB values from YUV/YPbPr
                switch (colorSpace) {
                case VectorscopeGenerator::ColorSpace_YUV:
                    dr = dy + 290.8 * v;
                    dg = dy - 100.6 * u - 148 * v;
                    db = dy + 517.2 * u;
                    break;
                case VectorscopeGenerator::ColorSpace_YPbPr:
                default:
                    dr = dy + 357.5 * v;
                    dg = dy - 87.75 * u - 182 * v;
                    db = dy + 451.9 * u;
                    break;
                }

                if (dr < 0) {
                    dr = 0;
                }
                if (dg < 0) {
                    dg = 0;
                }
                if (db < 0) {
                    db = 0;
                }
                if (dr > 255) {
                    dr = 255;
                }
                if (dg > 255) {
                    dg = 255;
                }
                if (db > 255) {
                    db = 255;
                }

                scope.setPixel(pt, qRgba(dr, dg, db, 255));
                break;

            case PaintMode_Chroma:
                dy = 200; // Default Y value. Lower = darker.

                // Calculate the RGB values from YUV/YPbPr
                switch (colorSpace) {
                case VectorscopeGenerator::ColorSpace_YUV:
                    dr = dy + 290.8 * v;
                    dg = dy - 100.6 * u - 148 * v;
                    db = dy + 517.2 * u;
                    break;
                case VectorscopeGenerator::ColorSpace_YPbPr:
                default:
                    dr = dy + 357.5 * v;
                    dg = dy - 87.75 * u - 182 * v;
                    db = dy + 451.9 * u;
                    break;
                }

                // Scale the RGB values back to max 255
                dmax = dr;
                if (dg > dmax) {
                    dmax = dg;
                }
                if (db > dmax) {
                    dmax = db;
                }
                dmax = 255 / dmax;

                dr *= dmax;
                dg *= dmax;
                db *= dmax;

                scope.setPixel(pt, qRgba(dr, dg, db, 255));
                break;
            case PaintMode_Original:
                scope.setPixel(pt, *col);
                break;
            case PaintMode_Green:
                px = scope.pixel(pt);
                scope.setPixel(pt, qRgba(qRed(px) + (255 - qRed(px)) / (3 * avgPxPerPx), qGreen(px) + 20 * (255 - qGreen(px)) / (avgPxPerPx),
                                         qBlue(px) + (255 - qBlue(px)) / (avgPxPerPx), qAlpha(px) + (255 - qAlpha(px)) / (avgPxPerPx)));
                break;
            case PaintMode_Green2:
                px = scope.pixel(pt);
                scope.setPixel(pt,
                               qRgba(qRed(px) + ceil((255 - (float)qRed(px)) / (4 * avgPxPerPx)), 255,
                                     qBlue(px) + ceil((255 - (float)qBlue(px)) / (avgPxPerPx)), qAlpha(px) + ceil((255 - (float)qAlpha(px)) / (avgPxPerPx))));
                break;
            case PaintMode_Black:
                px = scope.pixel(pt);
                scope.setPixel(pt, qRgba(0, 0, 0, qAlpha(px) + (255 - qAlpha(px)) / 20));
                break;
            }
        }

        bits += stepsize;
    }
    return scope;
}
