/*
    SPDX-FileCopyrightText: 2025 Kdenlive contributors
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include "audioleveltypes.h"
#include <KConfigGroup>
#include <KSharedConfig>
#include <QColor>

class AudioLevelConfig
{
public:
    struct Config
    {
        AudioLevel::LevelStyle levelStyle = AudioLevel::LevelStyle::Solid;
        bool drawBlockLines = true;
        AudioLevel::PeakIndicatorStyle peakIndicatorStyle = AudioLevel::PeakIndicatorStyle::Colorful;
    };

    static AudioLevelConfig &instance();

    // Getters
    AudioLevel::LevelStyle levelStyle() const { return m_config.levelStyle; }
    bool drawBlockLines() const { return m_config.drawBlockLines; }
    AudioLevel::PeakIndicatorStyle peakIndicatorStyle() const { return m_config.peakIndicatorStyle; }

    // Setters
    void setLevelStyle(AudioLevel::LevelStyle style);
    void setDrawBlockLines(bool draw);
    void setPeakIndicatorStyle(AudioLevel::PeakIndicatorStyle style);

    // Signal for configuration changes
    void configChanged();

private:
    AudioLevelConfig(); // Private constructor for singleton
    void readConfig();
    void writeConfig();

    Config m_config;
    KSharedConfigPtr m_configFile;
};