/*
    SPDX-FileCopyrightText: 2025 Kdenlive contributors
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "audiolevelrenderer.hpp"
#include "audiolevelconfig.h"
#include "audiolevelstyleprovider.h"
#include "audiomixer/iecscale.h"
#include "utils/painterutils.h"

#include <KLocalizedString>
#include <QPainter>
#include <array>

// Drawing constants
constexpr int CHANNEL_BORDER_WIDTH = 1;      // px for channel border
constexpr int BLOCK_LINE_SPACING = 4;        // px between each block line. Block line is a line perpendicular to the primary axis.
constexpr double BLOCK_LINE_GAP_FACTOR = 0.; // gap between block line and channel border/separator. multiplied by secondary axis length
constexpr int BLOCK_LINE_WIDTH = 1;          // pen size for block lines
constexpr int PEAK_INDICATOR_WIDTH = 2;      // px for peak indicator
constexpr int TICK_MARK_LENGTH = 2;          // px for tick mark
constexpr int TICK_MARK_THICKNESS = 1;
constexpr int MARGIN_BETWEEN_LABEL_AND_LEVELS = TICK_MARK_LENGTH + 2; // px between decibels scale labels and audio levels

constexpr int MINIMUM_SECONDARY_AXIS_LENGTH = 3; // minimum height/width for audio level channels
constexpr int MAXIMUM_SECONDARY_AXIS_LENGTH = 7; // maximum height/width for audio level channels

constexpr int NO_AUDIO_DB = -100;
constexpr int NO_AUDIO_PRIMARY_AXIS_POSITION = -1;

constexpr qreal HIDPI_OFFSET_ADJUSTMENT = 0.5;
constexpr qreal HIDPI_LENGTH_ADJUSTMENT = 1.0;

// AudioLevelLayoutState implementation
AudioLevelLayoutState::AudioLevelLayoutState(const Config &config)
    : m_config(config)
{
    calculateLayout();
}

void AudioLevelLayoutState::calculateLayout()
{
    m_borderOffset = 0;
    m_borderOffsetWithLabels = 0;
    m_shouldDrawTicks = (m_config.tickLabelsMode != AudioLevel::TickLabelsMode::Hide);
    m_shouldDrawLabels = (m_config.tickLabelsMode != AudioLevel::TickLabelsMode::Hide);

    if (m_config.tickLabelsMode == AudioLevel::TickLabelsMode::Hide) {
        return;
    }

    // Calculate space needed for labels
    if (m_config.orientation == Qt::Horizontal) {
        m_borderOffsetWithLabels = m_config.fontMetrics.height() + MARGIN_BETWEEN_LABEL_AND_LEVELS;
    } else {
        m_borderOffsetWithLabels = m_config.fontMetrics.boundingRect(QStringLiteral("-45")).width() + MARGIN_BETWEEN_LABEL_AND_LEVELS;
    }

    if (m_config.tickLabelsMode == AudioLevel::TickLabelsMode::HideIfSpaceIsLimited) {
        // Calculate if we have enough space for labels
        int fullSize = m_config.orientation == Qt::Horizontal ? m_config.widgetSize.height() : m_config.widgetSize.width();
        int drawableSize = fullSize - m_borderOffsetWithLabels;
        int channelSize = (drawableSize - (m_config.audioChannels + 1) * CHANNEL_BORDER_WIDTH) / m_config.audioChannels;

        m_shouldDrawLabels = channelSize >= MINIMUM_SECONDARY_AXIS_LENGTH;

        if (!m_shouldDrawLabels) {
            m_borderOffset = TICK_MARK_LENGTH; // Reserve space just for tick marks
        } else {
            m_borderOffset = m_borderOffsetWithLabels;
        }
    } else {
        m_borderOffset = m_borderOffsetWithLabels;
    }
}

int AudioLevelLayoutState::getEffectiveBorderOffset() const
{
    if (isInHoverLabelMode()) {
        return m_borderOffsetWithLabels;
    }
    return m_borderOffset;
}

int AudioLevelLayoutState::calculateSecondaryAxisLength() const
{
    int fullSize = m_config.orientation == Qt::Horizontal ? m_config.widgetSize.height() : m_config.widgetSize.width();
    int drawableSize = fullSize - m_borderOffset;
    int channelSize = (drawableSize - (m_config.audioChannels + 1) * CHANNEL_BORDER_WIDTH) / m_config.audioChannels;

    // When hovering in HideIfSpaceIsLimited mode, force minimum size
    if (isInHoverLabelMode()) {
        return MINIMUM_SECONDARY_AXIS_LENGTH;
    }

    if (channelSize < 1) channelSize = 1;
    return channelSize;
}

bool AudioLevelLayoutState::isInHoverLabelMode() const
{
    return m_config.isHovered && m_config.tickLabelsMode == AudioLevel::TickLabelsMode::HideIfSpaceIsLimited && !m_shouldDrawLabels;
}

// AudioLevelRenderer implementation
AudioLevelRenderer::AudioLevelRenderer(QObject *parent)
    : QObject(parent)
{
}

// Helper functions for snapping to block lines
int AudioLevelRenderer::snapDown(int value)
{
    return value - (value % BLOCK_LINE_SPACING);
}

int AudioLevelRenderer::snapUp(int value)
{
    int rem = value % BLOCK_LINE_SPACING;
    return rem == 0 ? value : value - rem + BLOCK_LINE_SPACING;
}

int AudioLevelRenderer::dBToPrimaryOffset(double dB, int maxDb, int primaryLength, Qt::Orientation orientation)
{
    if (orientation == Qt::Horizontal) {
        return qRound(IEC_ScaleMax(dB, maxDb) * primaryLength);
    } else {
        int totalHeight = CHANNEL_BORDER_WIDTH + primaryLength;
        return totalHeight - qRound(IEC_ScaleMax(dB, maxDb) * primaryLength);
    }
}

int AudioLevelRenderer::channelToSecondaryOffset(int channelIndex, int secondaryAxisLength, int borderOffset, Qt::Orientation orientation)
{
    if (orientation == Qt::Horizontal) {
        // Start at top, do not add offset
        return CHANNEL_BORDER_WIDTH + channelIndex * (secondaryAxisLength + CHANNEL_BORDER_WIDTH);
    } else {
        return borderOffset + CHANNEL_BORDER_WIDTH + channelIndex * (secondaryAxisLength + CHANNEL_BORDER_WIDTH);
    }
}

int AudioLevelRenderer::calculatePrimaryAxisLength(const QSize &widgetSize, Qt::Orientation orientation, bool drawBlockLines)
{
    if (!drawBlockLines) {
        // Use full width/height minus borders
        if (orientation == Qt::Horizontal) {
            return widgetSize.width() - 2 * CHANNEL_BORDER_WIDTH;
        } else {
            return widgetSize.height() - 2 * CHANNEL_BORDER_WIDTH;
        }
    }
    if (orientation == Qt::Horizontal) {
        int baseLength = widgetSize.width() - CHANNEL_BORDER_WIDTH;
        int fullBlocks = baseLength / BLOCK_LINE_SPACING;
        return fullBlocks * BLOCK_LINE_SPACING - CHANNEL_BORDER_WIDTH;
    } else {
        int baseLength = widgetSize.height() - CHANNEL_BORDER_WIDTH;
        int fullBlocks = baseLength / BLOCK_LINE_SPACING;
        return fullBlocks * BLOCK_LINE_SPACING - CHANNEL_BORDER_WIDTH;
    }
}

void AudioLevelRenderer::drawBackground(QPainter &painter, const RenderData &data)
{
    if (data.layoutState.getWidgetSize().height() == 0 || data.layoutState.getWidgetSize().width() == 0 || data.audioChannels == 0 ||
        data.valueDecibels.isEmpty()) {
        return;
    }

    const QVector<int> dbscale = {0, -6, -12, -18, -24, -30, -36, -42, -48, -54};

    // Draw channel borders (background rectangle and separators)
    drawChannelBorders(painter, data, true);
    // Draw dB scale (labels and lines)
    drawDbScale(painter, data);
}

void AudioLevelRenderer::drawChannelBordersToPixmap(QPixmap &pixmap, const RenderData &data)
{
    pixmap.fill(Qt::transparent);
    QPainter p(&pixmap);
    drawChannelBorders(p, data, false);
    p.end();
}

void AudioLevelRenderer::drawChannelBorders(QPainter &painter, const RenderData &data, bool fillBackground) const
{
    painter.save();
    int effectiveBorderOffset = data.layoutState.getEffectiveBorderOffset();

    if (data.orientation == Qt::Horizontal && data.layoutState.isInHoverLabelMode()) {
        // In horizontal mode, clip to prevent drawing over labels in hovering mode
        QRect clipRect(0, 0, data.layoutState.getWidgetSize().width(), data.layoutState.getWidgetSize().height() - effectiveBorderOffset);
        painter.setClipRect(clipRect);
    }

    QPen pen = painter.pen();
    pen.setColor(AudioLevelStyleProvider::instance().getBorderColor(data.palette, data.isEnabled));
    pen.setWidthF(CHANNEL_BORDER_WIDTH);
    painter.setPen(pen);

    QColor channelBackgroundColor = AudioLevelStyleProvider::instance().getChannelBackgroundColor(data.palette);

    // Draw the main background rectangle for the levels area and channel separators (lines between channels)
    if (data.orientation == Qt::Horizontal) {
        qreal totalHeight = data.audioChannels * data.secondaryAxisLength + (data.audioChannels + 1) * CHANNEL_BORDER_WIDTH;
        qreal totalWidth = data.primaryAxisLength + 2 * CHANNEL_BORDER_WIDTH;
        QRectF drawingRect = PainterUtils::adjustedForPen(QRectF(0, 0, totalWidth, totalHeight), pen.widthF());
        if (fillBackground && channelBackgroundColor.isValid()) {
            painter.fillRect(drawingRect, channelBackgroundColor);
        }
        painter.drawRect(drawingRect);
        for (int i = 1; i < data.audioChannels; i++) {
            qreal y = i * (data.secondaryAxisLength + CHANNEL_BORDER_WIDTH);
            painter.drawLine(PainterUtils::adjustedHorizontalLine(y, CHANNEL_BORDER_WIDTH, data.primaryAxisLength + 1, pen.widthF()));
        }
    } else {
        qreal totalWidth = data.audioChannels * data.secondaryAxisLength + (data.audioChannels + 1) * CHANNEL_BORDER_WIDTH;
        qreal totalHeight = data.primaryAxisLength + 2 * CHANNEL_BORDER_WIDTH;
        QRectF drawingRect = PainterUtils::adjustedForPen(QRectF(effectiveBorderOffset, 0, totalWidth, totalHeight), pen.widthF());
        if (fillBackground && channelBackgroundColor.isValid()) {
            painter.fillRect(drawingRect, channelBackgroundColor);
        }
        painter.drawRect(drawingRect);
        qreal channelWidth = data.secondaryAxisLength;
        for (int i = 1; i < data.audioChannels; ++i) {
            qreal x = effectiveBorderOffset + i * (channelWidth + CHANNEL_BORDER_WIDTH);
            painter.drawLine(PainterUtils::adjustedVerticalLine(x, CHANNEL_BORDER_WIDTH, data.primaryAxisLength + 1, pen.widthF()));
        }
    }
    painter.restore();
}

void AudioLevelRenderer::drawDbScale(QPainter &painter, const RenderData &data) const
{
    if (!data.layoutState.shouldDrawTicks()) {
        return;
    }

    const QVector<int> dbscale = {0, -6, -12, -18, -24, -30, -36, -42, -48, -54};
    int effectiveBorderOffset = data.layoutState.getEffectiveBorderOffset();

    // Fill background with Window color in hovering/hidding mode. We're potentially drawing over previous levels, so we need to fill the background.
    if (data.layoutState.isInHoverLabelMode()) {
        QColor bgColor = data.palette.color(QPalette::Window);
        if (data.orientation == Qt::Horizontal) {
            qreal y = data.audioChannels * data.secondaryAxisLength + (data.audioChannels + 1) * CHANNEL_BORDER_WIDTH;
            painter.fillRect(QRectF(0, y, data.layoutState.getWidgetSize().width(), data.layoutState.getWidgetSize().height() - y), bgColor);
        } else {
            painter.fillRect(QRectF(0, 0, effectiveBorderOffset, data.layoutState.getWidgetSize().height()), bgColor);
        }
    }

    int dbLabelCount = dbscale.size();

    QColor textColor = data.isEnabled ? data.palette.color(QPalette::Text) : data.palette.color(QPalette::Disabled, QPalette::Text);
    QPen pen = textColor;
    pen.setWidthF(TICK_MARK_THICKNESS);
    painter.setPen(pen);
    painter.setOpacity(0.8); // Using a constant value since we removed kLabelOpacity
    painter.setFont(data.font);
    int labelMargin = 2;
    if (data.orientation == Qt::Horizontal) {
        int labelHeight = data.fontMetrics.ascent();
        int prevX = -1;
        int x = 0;
        // y in regular mode bottom of the channels rectangle, in hovering mode we're clipping so have to use the effective offset
        qreal y = qMin(data.audioChannels * data.secondaryAxisLength + (data.audioChannels + 1) * CHANNEL_BORDER_WIDTH,
                       data.layoutState.getWidgetSize().height() - effectiveBorderOffset);
        int spaceForTwoLabels = 2 * data.fontMetrics.boundingRect(QStringLiteral("-45")).width() + labelMargin;
        bool drawLabels =
            (data.layoutState.shouldDrawLabels() || data.layoutState.isInHoverLabelMode()) && data.layoutState.getWidgetSize().width() >= spaceForTwoLabels;
        for (int i = 0; i < dbLabelCount; i++) {
            int value = dbscale[i];
            x = dBToPrimaryOffset(value, data.maxDb, data.primaryAxisLength, data.orientation);
            // Draw tick mark
            painter.drawLine(PainterUtils::adjustedVerticalLine(x, y, y + TICK_MARK_LENGTH, pen.widthF()));

            if (drawLabels) {
                const QString label = QString::asprintf("%d", value);
                int labelWidth = data.fontMetrics.horizontalAdvance(label);
                // Center the label relative to the tick mark
                x -= qRound(labelWidth / 2.0);
                // Ensure the label is not drawn off the widget
                if (x + labelWidth > data.layoutState.getWidgetSize().width()) {
                    x = data.layoutState.getWidgetSize().width() - labelWidth;
                } else if (x < 0) {
                    x = 0;
                }
                // Draw the label if it is not overlapping with the previous label
                if (prevX < 0 || prevX - (x + labelWidth) > labelMargin) {
                    painter.drawText(x, y + MARGIN_BETWEEN_LABEL_AND_LEVELS + labelHeight, label);
                    prevX = x;
                }
            }
        }
    } else {
        int labelHeight = data.fontMetrics.height();
        int prevY = -1;
        int y = 0;
        qreal x = effectiveBorderOffset - TICK_MARK_LENGTH;
        int spaceForTwoLabels = 2 * labelHeight;
        bool drawLabels =
            (data.layoutState.shouldDrawLabels() || data.layoutState.isInHoverLabelMode()) && data.layoutState.getWidgetSize().height() >= spaceForTwoLabels;
        for (int i = 0; i < dbLabelCount; i++) {
            int value = dbscale.at(i);
            y = dBToPrimaryOffset(value, data.maxDb, data.primaryAxisLength, data.orientation);
            // Draw tick mark
            painter.drawLine(PainterUtils::adjustedHorizontalLine(y, x, x + TICK_MARK_LENGTH, pen.widthF()));

            if (drawLabels) {
                // Center the label relative to the tick mark
                y -= qRound(labelHeight / 2.0);
                // Ensure the label is not drawn off the widget
                if (y + labelHeight > data.layoutState.getWidgetSize().height()) {
                    y = data.layoutState.getWidgetSize().height() - labelHeight;
                } else if (y < 0) {
                    y = 0;
                }
                // Draw the label if it is not overlapping with the previous label
                if (prevY < 0 || y - (prevY + labelHeight) > labelMargin) {
                    const QString label = QString::asprintf("%d", value);
                    painter.drawText(QRectF(0, y, effectiveBorderOffset - MARGIN_BETWEEN_LABEL_AND_LEVELS, labelHeight), label, QTextOption(Qt::AlignRight));
                    prevY = y;
                }
            }
        }
    }
    painter.setOpacity(1.0);
}

void AudioLevelRenderer::drawBlockLines(QPainter &painter, const RenderData &data)
{
    if (!AudioLevelConfig::instance().drawBlockLines()) {
        return;
    }
    QPen pen(AudioLevelStyleProvider::instance().getBorderColor(data.palette, data.isEnabled));
    pen.setWidthF(BLOCK_LINE_WIDTH);
    painter.setPen(pen);
    int secondaryLen = data.secondaryAxisLength;
    int blockLineGap = qRound(secondaryLen * BLOCK_LINE_GAP_FACTOR);
    int blockLineLength = secondaryLen - 2 * blockLineGap;
    int effectiveBorderOffset = data.layoutState.getEffectiveBorderOffset();

    if (data.orientation == Qt::Horizontal) {
        // block lines for the value
        for (int x = BLOCK_LINE_SPACING; x < data.primaryAxisLength; x = x + BLOCK_LINE_SPACING) {
            for (int i = 0; i < data.audioChannels; i++) {
                // Don't draw block lines for channels with no audio data
                if (data.valuePrimaryAxisPositions.at(i) == NO_AUDIO_PRIMARY_AXIS_POSITION) {
                    continue;
                }
                // Only draw block lines from left up to the value position
                if (x > snapUp(data.valuePrimaryAxisPositions.at(i))) {
                    continue;
                }
                // Draw only in the meter area (not in label area)
                qreal borderOffset = CHANNEL_BORDER_WIDTH + i * (secondaryLen + CHANNEL_BORDER_WIDTH);
                qreal lineStart = borderOffset + blockLineGap - HIDPI_OFFSET_ADJUSTMENT;
                qreal lineEnd = lineStart + blockLineLength + HIDPI_LENGTH_ADJUSTMENT;
                if (lineStart < lineEnd) {
                    painter.drawLine(PainterUtils::adjustedVerticalLine(x, lineStart, lineEnd, pen.widthF()));
                }
            }
        }
        // block lines for the peak
        for (int i = 0; i < data.audioChannels; i++) {
            if (data.peakPrimaryAxisPositions.at(i) == NO_AUDIO_PRIMARY_AXIS_POSITION) {
                continue;
            }
            int x = snapDown(data.peakPrimaryAxisPositions.at(i));
            qreal borderOffset = CHANNEL_BORDER_WIDTH + i * (secondaryLen + CHANNEL_BORDER_WIDTH);
            qreal lineStart = borderOffset + blockLineGap - HIDPI_OFFSET_ADJUSTMENT;
            qreal lineEnd = lineStart + blockLineLength + HIDPI_LENGTH_ADJUSTMENT;
            if (lineStart < lineEnd) {
                painter.drawLine(PainterUtils::adjustedVerticalLine(x, lineStart, lineEnd, pen.widthF()));
                painter.drawLine(PainterUtils::adjustedVerticalLine(x + BLOCK_LINE_SPACING, lineStart, lineEnd, pen.widthF()));
            }
        }
    } else {
        // block lines for the value
        for (int y = CHANNEL_BORDER_WIDTH + data.primaryAxisLength - BLOCK_LINE_SPACING; y > CHANNEL_BORDER_WIDTH; y -= BLOCK_LINE_SPACING) {
            for (int i = 0; i < data.audioChannels; ++i) {
                // Don't draw block lines for channels with no audio data
                if (data.valuePrimaryAxisPositions.at(i) == NO_AUDIO_PRIMARY_AXIS_POSITION) {
                    continue;
                }
                // Only draw block lines from bottom up to the value position
                if (y < snapDown(data.valuePrimaryAxisPositions.at(i))) {
                    continue;
                }
                // Draw line perpendicular to primary axis for this channel.
                qreal borderOffset = effectiveBorderOffset + CHANNEL_BORDER_WIDTH + i * (secondaryLen + CHANNEL_BORDER_WIDTH);
                qreal lineStart = borderOffset + blockLineGap - HIDPI_OFFSET_ADJUSTMENT;
                qreal lineEnd = lineStart + blockLineLength + HIDPI_LENGTH_ADJUSTMENT;
                if (lineStart < lineEnd) {
                    painter.drawLine(PainterUtils::adjustedHorizontalLine(y, lineStart, lineEnd, pen.widthF()));
                }
            }
        }
        // block lines for the peak
        for (int i = 0; i < data.audioChannels; i++) {
            if (data.peakPrimaryAxisPositions.at(i) == NO_AUDIO_PRIMARY_AXIS_POSITION) {
                continue;
            }
            int y = snapUp(data.peakPrimaryAxisPositions.at(i));
            qreal borderOffset = effectiveBorderOffset + CHANNEL_BORDER_WIDTH + i * (secondaryLen + CHANNEL_BORDER_WIDTH);
            qreal lineStart = borderOffset + blockLineGap - HIDPI_OFFSET_ADJUSTMENT;
            qreal lineEnd = lineStart + blockLineLength + HIDPI_LENGTH_ADJUSTMENT;
            if (lineStart < lineEnd) {
                painter.drawLine(PainterUtils::adjustedHorizontalLine(y, lineStart, lineEnd, pen.widthF()));
                painter.drawLine(PainterUtils::adjustedHorizontalLine(y - BLOCK_LINE_SPACING, lineStart, lineEnd, pen.widthF()));
            }
        }
    }
}

void AudioLevelRenderer::drawChannelPeakIndicator(QPainter &painter, const RenderData &data, int channelIndex) const
{
    bool drawPeak = data.peakPrimaryAxisPositions.at(channelIndex) != NO_AUDIO_PRIMARY_AXIS_POSITION;
    if (!drawPeak) return;

    QColor peakColor = AudioLevelStyleProvider::instance().getPeakColor(data.palette, data.peakDecibels.at(channelIndex));

    // To fix glitches in HiDPI with fractional scaling lets slightly enlarge the drawing area as we could otherwise end up with a gap of 1px between the levels
    // fill and the border. We'll have to redraw the borders to ensure they are not covered by the levels.
    qreal secondaryOffset =
        channelToSecondaryOffset(channelIndex, data.secondaryAxisLength, data.layoutState.getBorderOffset(), data.orientation) - HIDPI_OFFSET_ADJUSTMENT;
    qreal secondaryLength = data.secondaryAxisLength + HIDPI_LENGTH_ADJUSTMENT;

    if (data.orientation == Qt::Horizontal) {
        qreal peakX;
        qreal peakWidth;
        if (AudioLevelConfig::instance().drawBlockLines()) {
            peakX = snapDown(data.peakPrimaryAxisPositions[channelIndex]);
            peakWidth = BLOCK_LINE_SPACING;
        } else {
            peakX = data.peakPrimaryAxisPositions[channelIndex];
            peakWidth = PEAK_INDICATOR_WIDTH;
            if (peakX + peakWidth > data.primaryAxisLength) {
                peakX = data.primaryAxisLength - peakWidth;
            }
        }
        painter.fillRect(QRectF(peakX, secondaryOffset, peakWidth, secondaryLength), peakColor);
    } else {
        qreal peakY;
        qreal peakHeight;
        if (AudioLevelConfig::instance().drawBlockLines()) {
            peakY = snapUp(data.peakPrimaryAxisPositions[channelIndex]);
            peakHeight = BLOCK_LINE_SPACING;
        } else {
            peakY = data.peakPrimaryAxisPositions[channelIndex];
            peakHeight = PEAK_INDICATOR_WIDTH;
            if (peakY - peakHeight < 0) {
                peakY = peakHeight;
            }
        }
        painter.fillRect(QRectF(secondaryOffset, peakY - peakHeight, secondaryLength, peakHeight), peakColor);
    }
}

void AudioLevelRenderer::drawChannelLevelsSolid(QPainter &painter, const RenderData &data) const
{
    auto levelColors = AudioLevelStyleProvider::instance().getLevelsFillColors(data.palette);

    for (int i = 0; i < data.audioChannels; i++) {
        double value = data.valueDecibels.at(i);
        // To fix glitches in HiDPI with fractional scaling lets slightly enlarge the drawing area as we could otherwise end up with a gap of 1px between the
        // levels fill and the border. We'll have to redraw the borders to ensure they are not covered by the levels.
        qreal secondaryOffset =
            channelToSecondaryOffset(i, data.secondaryAxisLength, data.layoutState.getBorderOffset(), data.orientation) - HIDPI_OFFSET_ADJUSTMENT;
        qreal secondaryLength = data.secondaryAxisLength + HIDPI_LENGTH_ADJUSTMENT;
        bool drawLevels = data.valueDecibels.at(i) != NO_AUDIO_DB;
        if (drawLevels) {
            bool drawGreen = true;
            bool drawYellow = value >= AudioLevelStyleProvider::LevelColors::greenThreshold;
            bool drawRed = value > AudioLevelStyleProvider::LevelColors::yellowThreshold;
            int valuePrimaryAxisPosition = data.valuePrimaryAxisPositions[i];
            qreal segStart;
            qreal segEnd;
            if (data.orientation == Qt::Horizontal) {
                if (drawGreen) {
                    segStart = CHANNEL_BORDER_WIDTH;
                    segEnd =
                        qMin(dBToPrimaryOffset(AudioLevelStyleProvider::LevelColors::greenThreshold, data.maxDb, data.primaryAxisLength, data.orientation) - 1,
                             valuePrimaryAxisPosition);
                    if (AudioLevelConfig::instance().drawBlockLines()) segEnd = snapUp(segEnd);
                    painter.fillRect(QRectF(segStart, secondaryOffset, segEnd - segStart, secondaryLength), levelColors.green);
                }
                if (drawYellow) {
                    segStart = segEnd;
                    segEnd =
                        qMin(dBToPrimaryOffset(AudioLevelStyleProvider::LevelColors::yellowThreshold, data.maxDb, data.primaryAxisLength, data.orientation) - 1,
                             valuePrimaryAxisPosition);
                    if (AudioLevelConfig::instance().drawBlockLines()) segEnd = snapUp(segEnd);
                    painter.fillRect(QRectF(segStart, secondaryOffset, segEnd - segStart, secondaryLength), levelColors.yellow);
                }
                if (drawRed) {
                    segStart = segEnd;
                    segEnd = valuePrimaryAxisPosition;
                    if (AudioLevelConfig::instance().drawBlockLines()) segEnd = snapUp(segEnd);
                    painter.fillRect(QRectF(segStart, secondaryOffset, segEnd - segStart, secondaryLength), levelColors.red);
                }
            } else { // Vertical orientation
                if (drawGreen) {
                    segStart = CHANNEL_BORDER_WIDTH + data.primaryAxisLength;
                    segEnd =
                        qMax(dBToPrimaryOffset(AudioLevelStyleProvider::LevelColors::greenThreshold, data.maxDb, data.primaryAxisLength, data.orientation) - 1,
                             valuePrimaryAxisPosition);
                    if (AudioLevelConfig::instance().drawBlockLines()) segEnd = snapDown(segEnd);
                    painter.fillRect(QRectF(secondaryOffset, segEnd, secondaryLength, segStart - segEnd), levelColors.green);
                }
                if (drawYellow) {
                    segStart = segEnd;
                    segEnd =
                        qMax(dBToPrimaryOffset(AudioLevelStyleProvider::LevelColors::yellowThreshold, data.maxDb, data.primaryAxisLength, data.orientation) - 1,
                             valuePrimaryAxisPosition);
                    if (AudioLevelConfig::instance().drawBlockLines()) segEnd = snapDown(segEnd);
                    painter.fillRect(QRectF(secondaryOffset, segEnd, secondaryLength, segStart - segEnd), levelColors.yellow);
                }
                if (drawRed) {
                    segStart = segEnd;
                    segEnd = valuePrimaryAxisPosition;
                    if (AudioLevelConfig::instance().drawBlockLines()) segEnd = snapDown(segEnd);
                    segEnd += CHANNEL_BORDER_WIDTH;
                    painter.fillRect(QRectF(secondaryOffset, segEnd, secondaryLength, segStart - segEnd), levelColors.red);
                }
            }
        }
        drawChannelPeakIndicator(painter, data, i);
    }
}

void AudioLevelRenderer::drawChannelLevelsGradient(QPainter &painter, const RenderData &data) const
{
    for (int i = 0; i < data.audioChannels; i++) {
        // To fix glitches in HiDPI with fractional scaling lets slightly enlarge the drawing area as we could otherwise end up with a gap of 1px between the
        // levels fill and the border. We'll have to redraw the borders to ensure they are not covered by the levels.
        qreal secondaryOffset =
            channelToSecondaryOffset(i, data.secondaryAxisLength, data.layoutState.getBorderOffset(), data.orientation) - HIDPI_OFFSET_ADJUSTMENT;
        qreal secondaryLength = data.secondaryAxisLength + HIDPI_LENGTH_ADJUSTMENT;
        int valuePrimaryOffset = data.valuePrimaryAxisPositions[i];
        bool drawLevels = data.valueDecibels.at(i) != NO_AUDIO_DB;
        if (drawLevels) {
            QColor bgColor = AudioLevelStyleProvider::instance().getChannelBackgroundColor(data.palette);
            painter.setOpacity(1.0);
            if (data.orientation == Qt::Horizontal) {
                QLinearGradient gradient = AudioLevelStyleProvider::instance().getLevelsFillGradient(data.palette, data.orientation, data.maxDb);
                gradient.setStart(CHANNEL_BORDER_WIDTH, 0);
                gradient.setFinalStop(data.primaryAxisLength, 0);
                painter.fillRect(QRectF(CHANNEL_BORDER_WIDTH, secondaryOffset, data.primaryAxisLength - CHANNEL_BORDER_WIDTH, secondaryLength), gradient);
                if (valuePrimaryOffset < data.primaryAxisLength) {
                    painter.fillRect(QRectF(valuePrimaryOffset, secondaryOffset, data.primaryAxisLength - valuePrimaryOffset, secondaryLength), bgColor);
                }
            } else {
                QLinearGradient gradient = AudioLevelStyleProvider::instance().getLevelsFillGradient(data.palette, data.orientation, data.maxDb);
                gradient.setStart(0, CHANNEL_BORDER_WIDTH + data.primaryAxisLength);
                gradient.setFinalStop(0, CHANNEL_BORDER_WIDTH);
                painter.fillRect(QRectF(secondaryOffset, CHANNEL_BORDER_WIDTH, secondaryLength, data.primaryAxisLength), gradient);
                if (valuePrimaryOffset > CHANNEL_BORDER_WIDTH) {
                    painter.fillRect(QRectF(secondaryOffset, CHANNEL_BORDER_WIDTH, secondaryLength, valuePrimaryOffset - CHANNEL_BORDER_WIDTH), bgColor);
                }
            }
        }
        drawChannelPeakIndicator(painter, data, i);
    }
}

void AudioLevelRenderer::drawChannelLevels(QPainter &painter, const RenderData &data)
{
    if (AudioLevelConfig::instance().levelStyle() == AudioLevel::LevelStyle::Solid) {
        drawChannelLevelsSolid(painter, data);
    } else if (AudioLevelConfig::instance().levelStyle() == AudioLevel::LevelStyle::Gradient) {
        drawChannelLevelsGradient(painter, data);
    }
    drawBlockLines(painter, data);
}
