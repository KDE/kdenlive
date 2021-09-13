/*
    SPDX-FileCopyrightText: 2015 Meltytech LLC

    SPDX-License-Identifier: LicenseRef-KDE-Accepted-GPL
*/

#ifndef IECSCALE_H
#define IECSCALE_H

//----------------------------------------------------------------------------
// IEC standard dB scaling -- as borrowed from meterbridge (c) Steve Harris

static inline double IEC_Scale(double dB)
{
    double fScale = 1.0f;

    if (dB < -70.0f) {
        fScale = 0.0f;
    } else if (dB < -60.0f) {
        fScale = (dB + 70.0f) * 0.0025f;
    } else if (dB < -50.0f) {
        fScale = (dB + 60.0f) * 0.005f + 0.025f;
    } else if (dB < -40.0) {
        fScale = (dB + 50.0f) * 0.0075f + 0.075f;
    } else if (dB < -30.0f) {
        fScale = (dB + 40.0f) * 0.015f + 0.15f;
    } else if (dB < -20.0f) {
        fScale = (dB + 30.0f) * 0.02f + 0.3f;
    } else if (dB < -0.001f || dB > 0.001f) { /* if (dB < 0.0f) */
        fScale = (dB + 20.0f) * 0.025f + 0.5f;
    }

    return fScale;
}

static inline double IEC_ScaleMax(double dB, double max)
{
    return IEC_Scale(dB) / IEC_Scale(max);
}

#endif // IECSCALE_H
