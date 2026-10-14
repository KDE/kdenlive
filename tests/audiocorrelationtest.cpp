/*
    SPDX-FileCopyrightText: 2026 Erich Seifert <dev@erichseifert.de>
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/
#include "catch.hpp"

#include "lib/audio/audioCorrelation.h"

#include <algorithm>
#include <vector>

namespace {
// A deterministic zero-mean envelope, like the normalized envelopes used for alignment
std::vector<qint64> envelope(size_t size, quint32 seed)
{
    std::vector<qint64> values(size);
    for (auto &v : values) {
        seed = seed * 1664525u + 1013904223u;
        v = qint64(seed >> 16) % 2001 - 1000;
    }
    return values;
}

// Returns the shift of sub relative to main with the highest correlation
qint64 bestShift(const std::vector<qint64> &main, const std::vector<qint64> &sub)
{
    std::vector<qint64> correlation(main.size() + sub.size() + 1, 0);
    AudioCorrelation::correlate(main.data(), main.size(), sub.data(), sub.size(), correlation.data());
    const auto best = std::distance(correlation.begin(), std::max_element(correlation.begin(), correlation.end()));
    return qint64(best) - qint64(sub.size());
}
} // namespace

TEST_CASE("Audio correlation of short clips", "[AudioCorrelation]")
{
    // Clips up to 200 frames are correlated directly, longer ones with FFTCorrelation
    const std::vector<qint64> main = envelope(150, 42);

    SECTION("Sub clip inside the main clip")
    {
        const std::vector<qint64> sub(main.begin() + 30, main.begin() + 70);
        CHECK(bestShift(main, sub) == 30);
    }

    SECTION("Sub clip starting before the main clip")
    {
        std::vector<qint64> sub = envelope(20, 7);
        sub.insert(sub.end(), main.begin(), main.begin() + 60);
        CHECK(bestShift(main, sub) == -20);
    }
}
