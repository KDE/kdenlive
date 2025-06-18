/*
    SPDX-FileCopyrightText: 2025 Kdenlive contributors
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include "audioleveltypes.h"
#include <QObject>
#include <QPainter>
#include <QPixmap>
#include <QVector>
#include <QWidget>

/**
 * @brief Encapsulates layout state and calculations for audio level widgets
 *
 * This class handles the complex logic around HideIfSpaceIsLimited mode,
 * centralizing all related calculations and state management.
 */
class AudioLevelLayoutState
{
public:
    struct Config
    {
        AudioLevel::TickLabelsMode tickLabelsMode = AudioLevel::TickLabelsMode::Show;
        Qt::Orientation orientation = Qt::Vertical;
        QSize widgetSize;
        QFontMetrics fontMetrics = QFontMetrics(QFont());
        int audioChannels = 0;
        bool isHovered = false;
    };

    explicit AudioLevelLayoutState(const Config &config);

    // Public getters for configuration properties
    QSize getWidgetSize() const { return m_config.widgetSize; }
    Qt::Orientation getOrientation() const { return m_config.orientation; }
    AudioLevel::TickLabelsMode getTickLabelsMode() const { return m_config.tickLabelsMode; }
    int getAudioChannels() const { return m_config.audioChannels; }
    bool isHovered() const { return m_config.isHovered; }

    // Main state queries
    bool shouldDrawLabels() const { return m_shouldDrawLabels; }
    bool shouldDrawTicks() const { return m_shouldDrawTicks; }
    int getOffset() const { return m_offset; }
    int getOffsetWithLabels() const { return m_offsetWithLabels; }
    int getEffectiveOffset() const;

    // Layout calculations
    int calculateSecondaryAxisLength() const;
    bool isInHoverLabelMode() const;

private:
    void calculateLayout();

    Config m_config;
    bool m_shouldDrawLabels = true;
    bool m_shouldDrawTicks = true;
    int m_offset = 0;
    int m_offsetWithLabels = 0;
};

/**
 * @brief Handles all drawing/painting operations for audio level widgets
 *
 * This class separates the rendering logic from the widget logic
 */
class AudioLevelRenderer : public QObject
{
    Q_OBJECT

public:
    struct RenderData
    {
        QVector<double> valueDecibels;
        QVector<double> peakDecibels;
        QVector<int> valuePrimaryAxisPositions;
        QVector<int> peakPrimaryAxisPositions;
        int audioChannels = 0;
        Qt::Orientation orientation = Qt::Vertical;
        int maxDb = 0;
        bool isEnabled = true;
        QPalette palette;
        QFont font;
        QFontMetrics fontMetrics = QFontMetrics(QFont());
        int primaryAxisLength = 0;
        int secondaryAxisLength = 0;

        // Layout state - replaces multiple individual parameters
        AudioLevelLayoutState layoutState;

        // Constructor to ensure layoutState is properly initialized
        RenderData(const AudioLevelLayoutState::Config &layoutConfig)
            : layoutState(layoutConfig)
        {
        }
    };

    explicit AudioLevelRenderer(QObject *parent = nullptr);
    ~AudioLevelRenderer() override = default;

    // Main drawing methods
    void drawBackground(QPainter &painter, const RenderData &data);
    void drawChannelLevels(QPainter &painter, const RenderData &data);
    void drawChannelBordersToPixmap(QPixmap &pixmap, const RenderData &data);

    // Helper methods for coordinate conversion
    static int dBToPrimaryOffset(double dB, int maxDb, int primaryLength, Qt::Orientation orientation);
    static int channelToSecondaryOffset(int channelIndex, int secondaryAxisLength, int offset, Qt::Orientation orientation);

    // Simplified calculation methods
    static int calculatePrimaryAxisLength(const QSize &widgetSize, Qt::Orientation orientation, bool drawBlockLines);

private:
    // Drawing helper methods
    void drawChannelBorders(QPainter &painter, const RenderData &data, bool fillBackground) const;
    void drawDbScale(QPainter &painter, const RenderData &data) const;
    void drawBlockLines(QPainter &painter, const RenderData &data);
    void drawChannelPeakIndicator(QPainter &painter, const RenderData &data, int channelIndex) const;
    void drawChannelLevelsSolid(QPainter &painter, const RenderData &data) const;
    void drawChannelLevelsGradient(QPainter &painter, const RenderData &data) const;

    // Helper functions for snapping to block lines
    static int snapDown(int value);
    static int snapUp(int value);
};