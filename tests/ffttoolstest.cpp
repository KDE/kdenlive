/*
    SPDX-FileCopyrightText: 2026 Erich Seifert <dev@erichseifert.de>
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/
#include "catch.hpp"

#include "lib/audio/fftTools.h"

#include <QtMath>
#include <algorithm>
#include <vector>

namespace {
// A mono sine wave of the given length that falls exactly on FFT bin @p bin of a window of size @p windowSize
audioShortVector sine(int length, int bin, int windowSize)
{
    audioShortVector samples(length);
    for (int i = 0; i < length; ++i) {
        samples[i] = qint16(16000 * qSin(2 * M_PI * bin * i / windowSize));
    }
    return samples;
}

int peakBin(const std::vector<float> &spectrum)
{
    return int(std::distance(spectrum.begin(), std::max_element(spectrum.begin(), spectrum.end())));
}
} // namespace

TEST_CASE("FFT of audio frames", "[FFT]")
{
    FFTTools fftTools;
    const int windowSize = 512;
    std::vector<float> spectrum(windowSize / 2);

    SECTION("Peak of a full window")
    {
        fftTools.fftNormalized(sine(windowSize, 32, windowSize), 0, 1, spectrum.data(), FFTTools::Window_Rect, windowSize);
        CHECK(peakBin(spectrum) == 32);
    }

    SECTION("Frames shorter than the window are padded with silence")
    {
        // The audio frame of one video frame is often shorter than the FFT window
        fftTools.fftNormalized(sine(windowSize / 2, 64, windowSize), 0, 1, spectrum.data(), FFTTools::Window_Hamming, windowSize);
        CHECK(peakBin(spectrum) == 64);
    }
}
