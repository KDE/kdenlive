/*
    SPDX-FileCopyrightText: 2025 Kdenlive contributors
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "audiolevelconfig.h"
#include "core.h"

AudioLevelConfig &AudioLevelConfig::instance()
{
    static AudioLevelConfig instance;
    return instance;
}

AudioLevelConfig::AudioLevelConfig()
{
    m_configFile = KSharedConfig::openConfig();
    readConfig();
}

void AudioLevelConfig::readConfig()
{
    KConfigGroup scopeConfig(m_configFile, "Audio_Levels");
    m_config.levelStyle = static_cast<AudioLevel::LevelStyle>(scopeConfig.readEntry("levelStyle", static_cast<int>(AudioLevel::LevelStyle::Solid)));
    m_config.drawBlockLines = scopeConfig.readEntry("drawBlockLines", false);
    m_config.peakIndicatorStyle =
        static_cast<AudioLevel::PeakIndicatorStyle>(scopeConfig.readEntry("peakIndicatorStyle", static_cast<int>(AudioLevel::PeakIndicatorStyle::Colorful)));
}

void AudioLevelConfig::writeConfig()
{
    KConfigGroup scopeConfig(m_configFile, "Audio_Levels");
    scopeConfig.writeEntry("levelStyle", static_cast<int>(m_config.levelStyle));
    scopeConfig.writeEntry("drawBlockLines", m_config.drawBlockLines);
    scopeConfig.writeEntry("peakIndicatorStyle", static_cast<int>(m_config.peakIndicatorStyle));
    scopeConfig.sync();
}

void AudioLevelConfig::setLevelStyle(AudioLevel::LevelStyle style)
{
    if (m_config.levelStyle != style) {
        m_config.levelStyle = style;
        writeConfig();
        configChanged();
    }
}

void AudioLevelConfig::setDrawBlockLines(bool draw)
{
    if (m_config.drawBlockLines != draw) {
        m_config.drawBlockLines = draw;
        writeConfig();
        configChanged();
    }
}

void AudioLevelConfig::setPeakIndicatorStyle(AudioLevel::PeakIndicatorStyle style)
{
    if (m_config.peakIndicatorStyle != style) {
        m_config.peakIndicatorStyle = style;
        writeConfig();
        configChanged();
    }
}

void AudioLevelConfig::configChanged()
{
    Q_EMIT pCore->audioLevelsConfigChanged();
}