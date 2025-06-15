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
        QColor green;
        QColor yellow;
        QColor red;
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