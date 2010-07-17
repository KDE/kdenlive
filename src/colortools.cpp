/***************************************************************************
 *   Copyright (C) 2010 by Simon Andreas Eugster (simon.eu@gmail.com)      *
 *   This file is part of kdenlive. See www.kdenlive.org.                  *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 ***************************************************************************/

#include <math.h>
#include "colortools.h"

ColorTools::ColorTools()
{
}



QImage ColorTools::yuvColorWheel(const QSize &size, const unsigned char &Y, const float &scaling, const bool &modifiedVersion, const bool &circleOnly)
{
    QImage wheel(size, QImage::Format_ARGB32);
    if (size.width() == 0 || size.height() == 0) {
        qCritical("ERROR: Size of the color wheel must not be 0!");
        return wheel;
    }
    wheel.fill(qRgba(0,0,0,0));

    double dr, dg, db, du, dv, dmax;
    double ru, rv, rr;
    const int w = size.width();
    const int h = size.height();
    const float w2 = (float)w/2;
    const float h2 = (float)h/2;

    for (int u = 0; u < w; u++) {
        // Transform u from {0,...,w} to [-1,1]
        du = (double) 2*u/(w-1) - 1;
        du = scaling*du;

        for (int v = 0; v < h; v++) {
            dv = (double) 2*v/(h-1) - 1;
            dv = scaling*dv;

            if (circleOnly) {
                // Ellipsis equation: x²/a² + y²/b² = 1
                // Here: x=ru, y=rv, a=w/2, b=h/2, 1=rr
                // For rr > 1, the point lies outside. Don't draw it.
                ru = u - w2;
                rv = v - h2;
                rr = ru*ru/(w2*w2) + rv*rv/(h2*h2);
                if (rr > 1) {
                    continue;
                }
            }

            // Calculate the RGB values from YUV
            dr = Y + 290.8*dv;
            dg = Y - 100.6*du - 148*dv;
            db = Y + 517.2*du;

            if (modifiedVersion) {
                // Scale the RGB values down, or up, to max 255
                dmax = fabs(dr);
                if (fabs(dg) > dmax) dmax = fabs(dg);
                if (fabs(db) > dmax) dmax = fabs(db);
                dmax = 255/dmax;

                dr *= dmax;
                dg *= dmax;
                db *= dmax;
            }

            // Avoid overflows (which would generate intersting patterns).
            // Note that not all possible (y,u,v) values with u,v \in [-1,1]
            // have a correct RGB representation, therefore some RGB values
            // may exceed {0,...,255}.
            if (dr < 0) dr = 0;
            if (dg < 0) dg = 0;
            if (db < 0) db = 0;
            if (dr > 255) dr = 255;
            if (dg > 255) dg = 255;
            if (db > 255) db = 255;

            wheel.setPixel(u, (h-v-1), qRgba(dr, dg, db, 255));
        }
    }

    emit signalWheelCalculationFinished();
    return wheel;
}

QImage ColorTools::yuvVerticalPlane(const QSize &size, const float &angle, const float &scaling)
{
    QImage plane(size, QImage::Format_ARGB32);
    if (size.width() == 0 || size.height() == 0) {
        qCritical("ERROR: Size of the color plane must not be 0!");
        return plane;
    }

    double dr, dg, db, du, dv, Y;
    const int w = size.width();
    const int h = size.height();
    const double uscaling = scaling*cos(M_PI*angle/180);
    const double vscaling = scaling*sin(M_PI*angle/180);

    for (int uv = 0; uv < w; uv++) {
        du = uscaling*((double)2*uv/w - 1);//(double)?
        dv = vscaling*((double)2*uv/w - 1);

        for (int y = 0; y < h; y++) {
            Y = (double)255*y/h;

            // See yuv2rgb, yuvColorWheel
            dr = Y + 290.8*dv;
            dg = Y - 100.6*du - 148*dv;
            db = Y + 517.2*du;
            if (dr < 0) dr = 0;
            if (dg < 0) dg = 0;
            if (db < 0) db = 0;
            if (dr > 255) dr = 255;
            if (dg > 255) dg = 255;
            if (db > 255) db = 255;

            plane.setPixel(uv, (h-y-1), qRgba(dr, dg, db, 255));

        }
    }


    return plane;

}

QImage ColorTools::rgbCurvePlane(const QSize &size, const ColorsRGB &color)
{
    QImage plane(size, QImage::Format_ARGB32);
    if (size.width() == 0 || size.height() == 0) {
        qCritical("ERROR: Size of the color plane must not be 0!");
        return plane;
    }

    const int w = size.width();
    const int h = size.height();

    double dcol, dval;

    for (int x = 0; x < w; x++) {
        dval = (double)255*x/w;

        for (int y = 0; y < h; y++) {
            dcol = (double)255*y/h;

            if (color == ColorTools::COL_R) {
                plane.setPixel(x, (h-y-1), qRgb(dcol, dval, dval));
            } else if (color == ColorTools::COL_G) {
                plane.setPixel(x, (h-y-1), qRgb(dval, dcol, dval));
            } else {
                plane.setPixel(x, (h-y-1), qRgb(dval, dval, dcol));
            }

        }
    }
    return plane;
}








