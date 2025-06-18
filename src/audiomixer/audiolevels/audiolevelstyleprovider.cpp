/*
    SPDX-FileCopyrightText: 2025 Kdenlive contributors
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "audiolevelstyleprovider.h"
#include "audiolevelconfig.h"
#include "audiomixer/iecscale.h"
#include <KColorScheme>
#include <QFontDatabase>

AudioLevelStyleProvider &AudioLevelStyleProvider::instance()
{
    static AudioLevelStyleProvider instance;
    return instance;
}

AudioLevelStyleProvider::LevelColors AudioLevelStyleProvider::getLevelsFillColors(const QPalette &palette) const
{
    LevelColors colors;
    colors.darkGreen = QColor(0, 135, 60);
    colors.green = QColor(20, 190, 20);
    colors.yellow = QColor(248, 204, 27);
    colors.orange = QColor(243, 115, 36);
    colors.red = QColor(225, 39, 41);
    colors.darkRed = QColor(200, 39, 41);

    return colors;
}

QColor AudioLevelStyleProvider::getBorderColor(const QPalette &palette, bool isEnabled) const
{
    KColorScheme scheme(palette.currentColorGroup(), KColorScheme::Window);

    // Get appropriate border color with good contrast
    // Use shadow color for subtle borders that work well with level colors
    QColor shadowColor = scheme.shade(KColorScheme::ShadowShade);
    QColor windowColor = palette.color(QPalette::Window);

    bool isDarkTheme = palette.color(QPalette::Window).lightness() < palette.color(QPalette::WindowText).lightness();

    // Blend shadow color with window background color
    if (isDarkTheme) {
        if (isEnabled) {
            return QColor::fromRgbF(shadowColor.redF() * 0.6 + windowColor.redF() * 0.4, shadowColor.greenF() * 0.6 + windowColor.greenF() * 0.4,
                                    shadowColor.blueF() * 0.6 + windowColor.blueF() * 0.4);
        } else {
            return QColor::fromRgbF(shadowColor.redF() * 0.3 + windowColor.redF() * 0.7, shadowColor.greenF() * 0.3 + windowColor.greenF() * 0.7,
                                    shadowColor.blueF() * 0.3 + windowColor.blueF() * 0.7);
        }
    } else {
        // Light theme: Use stronger borders for better definition
        if (isEnabled) {
            return QColor::fromRgbF(shadowColor.redF() * 0.9 + windowColor.redF() * 0.1, shadowColor.greenF() * 0.9 + windowColor.greenF() * 0.1,
                                    shadowColor.blueF() * 0.9 + windowColor.blueF() * 0.1);
        } else {
            return QColor::fromRgbF(shadowColor.redF() * 0.6 + windowColor.redF() * 0.4, shadowColor.greenF() * 0.6 + windowColor.greenF() * 0.4,
                                    shadowColor.blueF() * 0.6 + windowColor.blueF() * 0.4);
        }
    }
}

QColor AudioLevelStyleProvider::getPeakColor(const QPalette &palette, double peakValue) const
{
    if (AudioLevelConfig::instance().peakIndicatorStyle() == AudioLevel::PeakIndicatorStyle::Colorful) {
        LevelColors levelColors = getLevelsFillColors(palette);
        if (peakValue > LevelColors::yellowThreshold) {
            return levelColors.red;
        } else if (peakValue > LevelColors::greenThreshold) {
            return levelColors.yellow;
        } else {
            return levelColors.green;
        }
    } else {
        // Monochrome style - use WindowText color for best contrast
        return palette.color(QPalette::WindowText);
    }
}

QColor AudioLevelStyleProvider::getChannelBackgroundColor(const QPalette &palette) const
{
    bool isDarkTheme = palette.color(QPalette::Window).lightness() < palette.color(QPalette::WindowText).lightness();
    QColor windowColor = palette.color(QPalette::Window);

    if (isDarkTheme) {
        // Make it slightly brighter for dark themes
        return QColor::fromHslF(windowColor.hueF(), windowColor.saturationF(), qMin(windowColor.lightnessF() + 0.04, 1.0));
    } else {
        // Make it slightly darker for light themes
        return QColor::fromHslF(windowColor.hueF(), windowColor.saturationF(), qMax(windowColor.lightnessF() - 0.04, 0.0));
    }
}

QLinearGradient AudioLevelStyleProvider::getLevelsFillGradient(const QPalette &palette, Qt::Orientation orientation, double maxDb) const
{
    auto levelColors = getLevelsFillColors(palette);
    QLinearGradient gradient;

    if (orientation == Qt::Horizontal) {
        gradient = QLinearGradient(0, 0, 1, 0);
        gradient.setColorAt(0., levelColors.darkGreen);
        gradient.setColorAt(IEC_ScaleMax(LevelColors::greenThreshold, maxDb), levelColors.green);
        gradient.setColorAt(IEC_ScaleMax(LevelColors::yellowThreshold, maxDb), levelColors.yellow);
        gradient.setColorAt(IEC_ScaleMax(LevelColors::orangeThreshold, maxDb), levelColors.orange);
        gradient.setColorAt(IEC_ScaleMax(LevelColors::redThreshold, maxDb), levelColors.red);
        gradient.setColorAt(1.0, levelColors.darkRed);
    } else {
        gradient = QLinearGradient(0, 1, 0, 0);
        gradient.setColorAt(1.0, levelColors.darkRed);
        gradient.setColorAt(IEC_ScaleMax(LevelColors::redThreshold, maxDb), levelColors.red);
        gradient.setColorAt(IEC_ScaleMax(LevelColors::orangeThreshold, maxDb), levelColors.orange);
        gradient.setColorAt(IEC_ScaleMax(LevelColors::yellowThreshold, maxDb), levelColors.yellow);
        gradient.setColorAt(IEC_ScaleMax(LevelColors::greenThreshold, maxDb), levelColors.green);
        gradient.setColorAt(0., levelColors.darkGreen);
    }

    return gradient;
}