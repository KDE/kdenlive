/*
    SPDX-FileCopyrightText: 2015 Meltytech, LLC
    SPDX-License-Identifier: GPL-3.0-or-later
*/

#ifndef IECSCALE_H
#define IECSCALE_H

#include <cmath>

//----------------------------------------------------------------------------
// IEC standard dB scaling -- as borrowed from meterbridge (c) Steve Harris

static inline double IEC_Scale(double dB)
{
    double fScale = 1.0f;

    if (dB < -70.0f)
        fScale = 0.0f;
    else if (dB < -60.0f)
        fScale = (dB + 70.0f) * 0.0025f;
    else if (dB < -50.0f)
        fScale = (dB + 60.0f) * 0.005f + 0.025f;
    else if (dB < -40.0)
        fScale = (dB + 50.0f) * 0.0075f + 0.075f;
    else if (dB < -30.0f)
        fScale = (dB + 40.0f) * 0.015f + 0.15f;
    else if (dB < -20.0f)
        fScale = (dB + 30.0f) * 0.02f + 0.3f;
    else if (dB < -0.001f || dB > 0.001f)  /* if (dB < 0.0f) */
        fScale = (dB + 20.0f) * 0.025f + 0.5f;

    return fScale;
}

static inline double IEC_ScaleMax(double dB, double max)
{
    return IEC_Scale(dB) / IEC_Scale(max);
}

static inline int fromDB(double level)
{
    int value = 60;
    if (level > 0.) {
        // increase volume
        value = 100 - int((pow(10, 1. - level / 24) - 1) / .225);
    } else if (level < 0.) {
        value = int((10 - pow(10, 1. - level / -50)) / -0.11395) + 59;
    }
    return value;
}

#endif // IECSCALE_H

