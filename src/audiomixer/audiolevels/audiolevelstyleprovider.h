/*
    SPDX-FileCopyrightText: 2025 Kdenlive contributors
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include <QColor>
#include <QFont>
#include <QPalette>

class AudioLevelStyleProvider
{
public:
    struct LevelColors
    {
        QColor darkGreen;
        QColor green;
        QColor yellow;
        QColor orange;
        QColor red;
        QColor darkRed;

        // Decibel thresholds for each color transition
        // Below green is dark green
        static constexpr double greenThreshold = -18.0; // Below this is green
        static constexpr double yellowThreshold = -6.0; // Below this is yellow
        static constexpr double orangeThreshold = -4.0; // Below this is orange
        static constexpr double redThreshold = -2.0;    // Below this is red
        // Above red is dark red
    };

    static AudioLevelStyleProvider &instance();

    LevelColors getLevelsFillColors(const QPalette &palette) const;
    QColor getBorderColor(const QPalette &palette, bool isEnabled) const;
    QColor getPeakColor(const QPalette &palette, double peakValue) const;
    QColor getChannelBackgroundColor(const QPalette &palette) const;
    QLinearGradient getLevelsFillGradient(const QPalette &palette, Qt::Orientation orientation, double maxDb) const;

private:
    AudioLevelStyleProvider() = default; // Private constructor for singleton
};