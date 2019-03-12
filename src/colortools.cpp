/***************************************************************************
 *   Copyright (C) 2010 by Simon Andreas Eugster (simon.eu@gmail.com)      *
 *   This file is part of kdenlive. See www.kdenlive.org.                  *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 ***************************************************************************/

#include "colortools.h"

#include <QColor>
#include <cmath>

//#define DEBUG_CT
#ifdef DEBUG_CT
#include "kdenlive_debug.h"
#endif

namespace {
double preventOverflow(double value)
{
    return value < 0 ? 0 : value > 255 ? 255 : value;
}
} // namespace

ColorTools::ColorTools(QObject *parent)
    : QObject(parent)
{
}

QImage ColorTools::yuvColorWheel(const QSize &size, int Y, float scaling, bool modifiedVersion, bool circleOnly)
{
    QImage wheel(size, QImage::Format_ARGB32);
    if (size.width() == 0 || size.height() == 0) {
        qCritical("ERROR: Size of the color wheel must not be 0!");
        return wheel;
    }
    if (circleOnly) {
        wheel.fill(qRgba(0, 0, 0, 0));
    }

    double dr, dg, db, dv;
    double ru, rv, rr;
    const int w = size.width();
    const int h = size.height();
    const float w2 = (float)w / 2;
    const float h2 = (float)h / 2;

    for (int u = 0; u < w; ++u) {
        // Transform u from {0,...,w} to [-1,1]
        double du = (double)2 * u / (w - 1) - 1;
        du = scaling * du;

        for (int v = 0; v < h; ++v) {
            dv = (double)2 * v / (h - 1) - 1;
            dv = scaling * dv;

            if (circleOnly) {
                // Ellipsis equation: x²/a² + y²/b² = 1
                // Here: x=ru, y=rv, a=w/2, b=h/2, 1=rr
                // For rr > 1, the point lies outside. Don't draw it.
                ru = u - double(w2);
                rv = v - double(h2);
                rr = ru * ru / (w2 * w2) + rv * rv / (h2 * h2);
                if (rr > 1) {
                    continue;
                }
            }

            // Calculate the RGB values from YUV
            dr = Y + 290.8 * dv;
            dg = Y - 100.6 * du - 148 * dv;
            db = Y + 517.2 * du;

            if (modifiedVersion) {
                // Scale the RGB values down, or up, to max 255
                const double dmax = 255 / std::max({fabs(dr), fabs(dg), fabs(db)});

                dr *= dmax;
                dg *= dmax;
                db *= dmax;
            }

            // Avoid overflows (which would generate intersecting patterns).
            // Note that not all possible (y,u,v) values with u,v \in [-1,1]
            // have a correct RGB representation, therefore some RGB values
            // may exceed {0,...,255}.
            dr = preventOverflow(dr);
            dg = preventOverflow(dg);
            db = preventOverflow(db);

            wheel.setPixel(u, (h - v - 1), qRgba(dr, dg, db, 255));
        }
    }

    emit signalYuvWheelCalculationFinished();
    return wheel;
}

QImage ColorTools::yuvVerticalPlane(const QSize &size, int angle, float scaling)
{
    QImage plane(size, QImage::Format_ARGB32);
    if (size.width() == 0 || size.height() == 0) {
        qCritical("ERROR: Size of the color plane must not be 0!");
        return plane;
    }

    double dr, dg, db, Y;
    const int w = size.width();
    const int h = size.height();
    const double uscaling = scaling * cos(M_PI * angle / 180);
    const double vscaling = scaling * sin(M_PI * angle / 180);

    for (int uv = 0; uv < w; ++uv) {
        double du = uscaling * ((double)2 * uv / w - 1); //(double)?
        double dv = vscaling * ((double)2 * uv / w - 1);

        for (int y = 0; y < h; ++y) {
            Y = (double)255 * y / h;

            // See yuv2rgb, yuvColorWheel
            dr = preventOverflow(Y + 290.8 * dv);
            dg = preventOverflow(Y - 100.6 * du - 148 * dv);
            db = preventOverflow(Y + 517.2 * du);

            plane.setPixel(uv, (h - y - 1), qRgba(dr, dg, db, 255));
        }
    }

    return plane;
}

QImage ColorTools::rgbCurvePlane(const QSize &size, const ColorTools::ColorsRGB &color, float scaling, const QRgb &background)
{
    Q_ASSERT(scaling > 0 && scaling <= 1);

    QImage plane(size, QImage::Format_ARGB32);
    if (size.width() == 0 || size.height() == 0) {
        qCritical("ERROR: Size of the color plane must not be 0!");
        return plane;
    }

    const int w = size.width();
    const int h = size.height();

    double dcol;
    double dx, dy;

    for (int x = 0; x < w; ++x) {
        double dval = (double)255 * x / (w - 1);

        for (int y = 0; y < h; ++y) {
            dy = (double)y / (h - 1);
            dx = (double)x / (w - 1);

            if (1 - scaling < 0.0001) {
                dcol = (double)255 * dy;
            } else {
                dcol = (double)255 * (dy - (dy - dx) * (1 - scaling));
            }

            if (color == ColorTools::ColorsRGB::R) {
                plane.setPixel(x, (h - y - 1), qRgb(dcol, dval, dval));
            } else if (color == ColorTools::ColorsRGB::G) {
                plane.setPixel(x, (h - y - 1), qRgb(dval, dcol, dval));
            } else if (color == ColorTools::ColorsRGB::B) {
                plane.setPixel(x, (h - y - 1), qRgb(dval, dval, dcol));
            } else if (color == ColorTools::ColorsRGB::A) {
                plane.setPixel(x, (h - y - 1), qRgb(dcol / 255. * qRed(background), dcol / 255. * qGreen(background), dcol / 255. * qBlue(background)));
            } else {
                plane.setPixel(x, (h - y - 1), qRgb(dcol, dcol, dcol));
            }
        }
    }
    return plane;
}

QImage ColorTools::rgbCurveLine(const QSize &size, const ColorTools::ColorsRGB &color, const QRgb &background)
{

    QImage plane(size, QImage::Format_ARGB32);
    if (size.width() == 0 || size.height() == 0) {
        qCritical("ERROR: Size of the color line must not be 0!");
        return plane;
    }

    const int w = size.width();
    const int h = size.height();

    double dcol;
    double dy;

    for (int x = 0; x < w; ++x) {

        for (int y = 0; y < h; ++y) {
            dy = (double)y / (h - 1);

            dcol = (double)255 * dy;

            if (color == ColorTools::ColorsRGB::R) {
                plane.setPixel(x, (h - y - 1), qRgb(dcol, 0, 0));
            } else if (color == ColorTools::ColorsRGB::G) {
                plane.setPixel(x, (h - y - 1), qRgb(0, dcol, 0));
            } else if (color == ColorTools::ColorsRGB::B) {
                plane.setPixel(x, (h - y - 1), qRgb(0, 0, dcol));
            } else if (color == ColorTools::ColorsRGB::A) {
                plane.setPixel(x, (h - y - 1), qRgb(dcol / 255. * qRed(background), dcol / 255. * qGreen(background), dcol / 255. * qBlue(background)));
            } else {
                plane.setPixel(x, (h - y - 1), qRgb(dcol, dcol, dcol));
            }
        }
    }
    return plane;
}

QImage ColorTools::yPbPrColorWheel(const QSize &size, int Y, float scaling, bool circleOnly)
{

    QImage wheel(size, QImage::Format_ARGB32);
    if (size.width() == 0 || size.height() == 0) {
        qCritical("ERROR: Size of the color wheel must not be 0!");
        return wheel;
    }
    if (circleOnly) {
        wheel.fill(qRgba(0, 0, 0, 0));
    }

    double dr, dg, db, dpR;
    double rB, rR, rr;
    const int w = size.width();
    const int h = size.height();
    const float w2 = (float)w / 2;
    const float h2 = (float)h / 2;

    for (int b = 0; b < w; ++b) {
        // Transform pB from {0,...,w} to [-0.5,0.5]
        double dpB = (double)b / (w - 1) - .5;
        dpB = scaling * dpB;

        for (int r = 0; r < h; ++r) {
            dpR = (double)r / (h - 1) - .5;
            dpR = scaling * dpR;

            if (circleOnly) {
                // see yuvColorWheel
                rB = b - double(w2);
                rR = r - double(h2);
                rr = rB * rB / (w2 * w2) + rR * rR / (h2 * h2);
                if (rr > 1) {
                    continue;
                }
            }

            // Calculate the RGB values from YPbPr
            dr = preventOverflow(Y + 357.5 * dpR);
            dg = preventOverflow(Y - 87.75 * dpB - 182.1 * dpR);
            db = preventOverflow(Y + 451.86 * dpB);

            wheel.setPixel(b, (h - r - 1), qRgba(dr, dg, db, 255));
        }
    }

    return wheel;
}

QImage ColorTools::hsvHueShiftPlane(const QSize &size, int S, int V, int MIN, int MAX)
{
    Q_ASSERT(size.width() > 0);
    Q_ASSERT(size.height() > 0);
    Q_ASSERT(MAX > MIN);

    QImage plane(size, QImage::Format_ARGB32);

#ifdef DEBUG_CT
    qCDebug(KDENLIVE_LOG) << "Requested: Saturation " << S << ", Value " << V;
    QColor colTest(-1, 256, 257);
    qCDebug(KDENLIVE_LOG) << "-1 mapped to " << colTest.red() << ", 256 to " << colTest.green() << ", 257 to " << colTest.blue();
#endif

    QColor col(0, 0, 0);

    const int hueValues = MAX - MIN;

    float huediff;
    int newhue;
    for (int x = 0; x < size.width(); ++x) {
        float hue = x / (size.width() - 1.0) * 359;
        for (int y = 0; y < size.height(); ++y) {
            huediff = (1.0f - y / (size.height() - 1.0)) * hueValues + MIN;
            //            qCDebug(KDENLIVE_LOG) << "hue: " << hue << ", huediff: " << huediff;

            newhue = hue + huediff + 360; // Avoid negative numbers. Rest (>360) will be mapped correctly.

            col.setHsv(newhue, S, V);
            plane.setPixel(x, y, col.rgba());
        }
    }

    return plane;
}

QImage ColorTools::hsvCurvePlane(const QSize &size, const QColor &baseColor, const ComponentsHSV &xVariant, const ComponentsHSV &yVariant, bool shear,
                                 const float offsetY)
{
    Q_ASSERT(size.width() > 0);
    Q_ASSERT(size.height() > 0);

    /*int xMax, yMax;

    switch(xVariant) {
    case COM_H:
        xMax = 360;
        break;
    case COM_S:
    case COM_V:
        xMax = 256;
        break;
    }

    switch (yVariant) {
    case COM_H:
        yMax = 360;
        break;
    case COM_S:
    case COM_V:
        yMax = 256;
        break;
    }*/

    QImage plane(size, QImage::Format_ARGB32);

    QColor col(0, 0, 0);

    float hue, sat, val;
    hue = baseColor.hueF();
    sat = baseColor.saturationF();
    val = baseColor.valueF();

    for (int x = 0; x < size.width(); ++x) {
        switch (xVariant) {
        case COM_H:
            hue = x / (size.width() - 1.0);
            break;
        case COM_S:
            sat = x / (size.width() - 1.0);
            break;
        case COM_V:
            val = x / (size.width() - 1.0);
            break;
        }
        for (int y = 0; y < size.height(); ++y) {
            switch (yVariant) {
            case COM_H:
                hue = 1.0 - y / (size.height() - 1.0);
                break;
            case COM_S:
                sat = 1.0 - y / (size.height() - 1.0);
                break;
            case COM_V:
                val = 1.0 - y / (size.height() - 1.0);
                break;
            }

            col.setHsvF(hue, sat, val);

            if (!shear) {
                plane.setPixel(x, y, col.rgba());
            } else {
                plane.setPixel(x, (2 * size.height() + y - x * size.width() / size.height() - int(offsetY) * size.height()) % size.height(), col.rgba());
            }
        }
    }

    return plane;
}
