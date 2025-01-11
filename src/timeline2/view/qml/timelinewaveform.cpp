/*
    SPDX-FileCopyrightText: 2015-2016 Meltytech LLC
    SPDX-FileCopyrightText: 2019 Jean-Baptiste Mardelle <jb@kdenlive.org>
    SPDX-FileCopyrightText: 2024 Étienne André <eti.andre@gmail.com>

    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "timelinewaveform.h"
#include "bin/projectitemmodel.h"
#include "core.h"
#include "jobs/audiolevels/audiolevelstask.h"
#include "jobs/audiolevels/generators.h"
#include "kdenlivesettings.h"

#include <QPainter>
#include <QPainterPath>
#include <doc/kdenlivedoc.h>

TimelineWaveform::TimelineWaveform(QQuickItem *parent)
    : QQuickPaintedItem(parent)
    , m_speed(1.)
{
    setAntialiasing(false);
    setOpaquePainting(m_opaquePaint);
    setEnabled(false);
    // setRenderTarget(QQuickPaintedItem::FramebufferObject);
    // setMipmap(true);
    // setTextureSize(QSize(1, 1));
    connect(this, &TimelineWaveform::needRecompute, [this] {
        m_needRecompute = true;
        update();
    });
    connect(this, &TimelineWaveform::needRedraw, &QQuickItem::update);
    connect(this, &TimelineWaveform::normalizeChanged, [this] {
        if (m_normalize) {
            m_normalizeFactor = static_cast<double>(std::numeric_limits<int16_t>::max()) / pCore->projectItemModel()->getAudioMaxLevel(m_binId, m_stream);
        } else {
            m_normalizeFactor = 1.0;
        }
        update();
    });
}

void TimelineWaveform::drawWaveformLines(QPainter *painter, const int ch, const int channels, const qreal yMiddle, const qreal channelHeight)
{
    for (int i = 0; i * channels + ch < m_audioLevels.size(); i++) {
        const auto level = m_audioLevels[i * channels + ch];
        if (level > 0) {
            const auto lineHeight = channelHeight * level * m_normalizeFactor / std::numeric_limits<int16_t>::max();
            painter->drawLine(i, yMiddle + lineHeight / 2, i, yMiddle - lineHeight / 2);
        }
    }
}

void TimelineWaveform::drawWaveformPath(QPainter *painter, const int ch, const int channels, const qreal yMiddle, const qreal channelHeight)
{
    QPainterPath path;
    path.moveTo(0, yMiddle);

    const double extraSpace = 1 / m_pointsPerPixel * AUDIOLEVELS_POINTS_PER_FRAME;

    for (int i = 0; i * channels + ch < m_audioLevels.size(); i++) {
        const auto x = i / m_pointsPerPixel;
        const auto level = m_audioLevels[i * channels + ch];
        const auto lineHeight = channelHeight * level * m_normalizeFactor / std::numeric_limits<int16_t>::max();
        path.lineTo(x, yMiddle + lineHeight / 2);
        if (x >= width() + extraSpace) {
            break;
        }
    }

    // extend and close the shape
    const auto level = m_audioLevels[(m_audioLevels.size() - 1) / channels + ch];
    const auto lineHeight = channelHeight * level * m_normalizeFactor / std::numeric_limits<int16_t>::max();
    path.lineTo(width() + extraSpace, yMiddle + lineHeight / 2);
    path.lineTo(width() + extraSpace, yMiddle);

    painter->drawPath(path);                          // draw top waveform
    const QTransform tr(1, 0, 0, -1, 0, 2 * yMiddle); // mirror it
    painter->drawPath(tr.map(path));                  // draw bottom waveform
}

void TimelineWaveform::compute()
{
    QVector<int16_t> levels;
    if (m_binId.isEmpty()) {
        return;
    }
    if (m_stream >= 0) {
        levels = pCore->projectItemModel()->getAudioLevelsByBinID(m_binId, m_stream);
        if (levels.isEmpty()) {
            return;
        }
    }

    const auto inPoint = static_cast<int>(m_inPoint);
    const auto outPoint = static_cast<int>(m_outPoint);

    if (outPoint <= inPoint) {
        return;
    }

    const auto clipLength = levels.size() / AUDIOLEVELS_POINTS_PER_FRAME / m_channels;
    if (outPoint >= clipLength) { // pad the levels so we don't read uninitialized memory
        levels.resize(outPoint * AUDIOLEVELS_POINTS_PER_FRAME * m_channels);
    }

    const double timescale = m_scale / std::abs(m_speed);
    const int length = outPoint - inPoint;
    const int inputPoints = AUDIOLEVELS_POINTS_PER_FRAME * length;
    const int outputPoints = std::round(length * timescale);

    m_pointsPerPixel = static_cast<double>(AUDIOLEVELS_POINTS_PER_FRAME) / timescale;
    const bool reverse = m_speed < 0;

    if (m_pointsPerPixel > 1) {
        // Resample the levels and store them
        m_audioLevels.resize(outputPoints * m_channels);
        computePeaks(&levels[inPoint * AUDIOLEVELS_POINTS_PER_FRAME * m_channels], m_audioLevels.data(), m_channels, inputPoints, outputPoints);
    } else {
        // Just extract the part to be displayed
        m_audioLevels = levels.mid(inPoint * AUDIOLEVELS_POINTS_PER_FRAME * m_channels, outputPoints * m_channels);
    }

    if (reverse) {
        std::reverse(m_audioLevels.begin(), m_audioLevels.end());
    }

    if (!m_separateChannels) {
        // merge all channels into one (in-place operation)
        for (int i = 0; i < outputPoints; i++) {
            int16_t maxValue = 0;
            for (int ch = 0; ch < m_channels; ch++) {
                if (i * m_channels + ch >= m_audioLevels.size()) {
                    break;
                }
                const auto val = m_audioLevels[i * m_channels + ch];
                maxValue = std::max(maxValue, val);
            }
            if (i >= m_audioLevels.size()) {
                break;
            }
            m_audioLevels[i] = maxValue;
        }
        m_audioLevels.resize(outputPoints);
    }

    m_needRecompute = false;
}
void TimelineWaveform::paint(QPainter *painter)
{
    if (m_needRecompute) {
        compute();
    }

    if (m_audioLevels.isEmpty()) {
        return;
    }

    const auto channels = m_separateChannels ? m_channels : 1;
    const QStringList channelNames{"L", "R", "C", "LFE", "BL", "BR"};

    // if the inpoint is not an integer, start drawing a bit further back so that the visible window is correct
    const double frac = (m_inPoint - std::floor(m_inPoint)) / m_pointsPerPixel * AUDIOLEVELS_POINTS_PER_FRAME;
    painter->translate(-frac, 0);

    for (int ch = 0; ch < channels; ch++) {
        const auto channelHeight = height() / channels;
        const auto yOrigin = ch * channelHeight;
        const auto fgColor = ch % 2 == 0 ? m_fgColorEven : m_fgColorOdd;
        const auto bgColor = ch % 2 == 0 ? m_bgColorEven : m_bgColorOdd;
        const auto yMiddle = yOrigin + channelHeight / 2;

        // draw background
        painter->setBrush(bgColor);
        painter->setPen(Qt::NoPen);
        painter->drawRect(0, yOrigin, width() + frac, channelHeight);

        // draw middle line
        painter->setBrush(Qt::NoBrush);
        painter->setPen(fgColor);
        painter->drawLine(0, yMiddle, width() + frac, yMiddle);

        // draw the waveform
        if (m_pointsPerPixel > 1) {
            painter->setBrush(Qt::NoBrush);
            painter->setPen(fgColor);
            drawWaveformLines(painter, ch, channels, yMiddle, channelHeight);
        } else {
            painter->setPen(Qt::NoPen);
            painter->setBrush(fgColor);
            drawWaveformPath(painter, ch, channels, yMiddle, channelHeight);
        }

        // draw channel names
        if (m_drawChannelNames && m_channels > 1 && m_channels < 7) {
            painter->setPen(fgColor);
            painter->drawText(2, yOrigin + channelHeight, channelNames[ch]);
        }
    }
}
