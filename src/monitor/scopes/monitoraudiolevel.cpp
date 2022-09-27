/*
SPDX-FileCopyrightText: 2016 Jean-Baptiste Mardelle <jb@kdenlive.org>
This file is part of Kdenlive. See www.kdenlive.org.

SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "monitoraudiolevel.h"
#include "audiomixer/iecscale.h"
#include "core.h"
#include "profiles/profilemodel.hpp"

#include "mlt++/Mlt.h"

#include <cmath>

#include <QFont>
#include <QPaintEvent>
#include <QPainter>

MonitorAudioLevel::MonitorAudioLevel(int height, QWidget *parent)
    : ScopeWidget(parent)
    , audioChannels(2)
    , m_height(height)
    , m_channelHeight(height / 2)
    , m_channelDistance(1)
    , m_channelFillHeight(m_channelHeight)
{
    setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Preferred);
    isValid = true;
    connect(this, &MonitorAudioLevel::audioLevelsAvailable, this, &MonitorAudioLevel::setAudioValues);
}

MonitorAudioLevel::~MonitorAudioLevel() = default;

void MonitorAudioLevel::refreshScope(const QSize & /*size*/, bool /*full*/)
{
    SharedFrame sFrame;
    while (m_queue.count() > 0) {
        sFrame = m_queue.pop();
        if (sFrame.is_valid()) {
            int samples = sFrame.get_audio_samples();
            if (samples <= 0) {
                continue;
            }
            // TODO: the 200 value is aligned with the MLT audiolevel filter, but seems arbitrary.
            samples = qMin(200, samples);
            int channels = sFrame.get_audio_channels();
            QVector<double> levels;
            const int16_t *audio = sFrame.get_audio();
            for (int c = 0; c < channels; c++) {
                int16_t peak = 0;
                const int16_t *p = audio + c;
                for (int s = 0; s < samples; s++) {
                    int16_t sample = abs(*p);
                    if (sample > peak) peak = sample;
                    p += channels;
                }
                if (peak == 0) {
                    levels << -100;
                } else {
                    levels << 20 * log10((double)peak / (double)std::numeric_limits<int16_t>::max());
                }
            }
            emit audioLevelsAvailable(levels);
        }
    }
}

void MonitorAudioLevel::resizeEvent(QResizeEvent *event)
{
    drawBackground(m_peaks.size());
    ScopeWidget::resizeEvent(event);
}

void MonitorAudioLevel::refreshPixmap()
{
    drawBackground(m_peaks.size());
}

void MonitorAudioLevel::drawBackground(int channels)
{
    if (height() == 0) {
        return;
    }
    QSize newSize = QWidget::size();
    if (!newSize.isValid()) {
        return;
    }
    QFont ft = font();
    ft.setPixelSize(newSize.height() / 3);
    setFont(ft);
    int textHeight = fontMetrics().ascent();
    newSize.setHeight(newSize.height() - textHeight);
    QLinearGradient gradient(0, 0, newSize.width(), 0);
    double gradientVal = 0.;
    gradient.setColorAt(gradientVal, Qt::darkGreen);
    gradientVal = IEC_ScaleMax(-12, m_maxDb);
    gradient.setColorAt(gradientVal - 0.001, Qt::darkGreen);
    gradient.setColorAt(gradientVal, Qt::green); // -12db
    gradientVal = IEC_ScaleMax(-6, m_maxDb);
    gradient.setColorAt(gradientVal - 0.001, Qt::green);
    gradient.setColorAt(gradientVal, Qt::yellow); // -6db
    gradientVal = IEC_ScaleMax(0, m_maxDb);
    gradient.setColorAt(gradientVal - 0.001, Qt::yellow);
    gradient.setColorAt(gradientVal, Qt::red); // 0db
    m_pixmap = QPixmap(QWidget::size());
    if (m_pixmap.isNull()) {
        return;
    }
    m_pixmap.fill(Qt::transparent);
    int totalHeight;
    if (channels < 2) {
        m_channelHeight = newSize.height() / 2;
        totalHeight = m_channelHeight;
    } else {
        m_channelHeight = (newSize.height() - (channels - 1)) / channels;
        totalHeight = channels * m_channelHeight + (channels - 1);
    }
    QRect rect(0, 0, newSize.width(), totalHeight);
    QPainter p(&m_pixmap);
    p.setOpacity(0.6);
    p.setFont(ft);
    p.fillRect(rect, QBrush(gradient));
    // Channel labels are horizontal along the bottom.
    QVector<int> dbscale;
    dbscale << 0 << -5 << -10 << -15 << -20 << -25 << -30 << -35 << -40 << -50;
    int dbLabelCount = dbscale.size();
    m_maxDb = dbscale.first();
    // dB scale is horizontal along the bottom
    int y = totalHeight + textHeight;
    int prevX = -1;
    int x = 0;
    for (int i = 0; i < dbLabelCount; i++) {
        int value = dbscale[i];
        QString label = QString::asprintf("%d", value);
        int labelWidth = fontMetrics().horizontalAdvance(label);
        x = IEC_ScaleMax(value, m_maxDb) * m_pixmap.width();
        p.setPen(palette().window().color());
        p.drawLine(x, 0, x, totalHeight);
        x -= qRound(labelWidth / 2.);
        if (x + labelWidth > m_pixmap.width()) {
            x = m_pixmap.width() - labelWidth;
        }
        if (prevX < 0 || prevX - (x + labelWidth) > 2) {
            p.setPen(palette().text().color().rgb());
            p.drawText(x, y, label);
            prevX = x;
        }
    }
    p.setOpacity(1);
    p.setPen(palette().window().color());
    // Clear space between the 2 channels
    p.setCompositionMode(QPainter::CompositionMode_Source);
    m_channelDistance = 1;
    m_channelFillHeight = m_channelHeight;
    for (int i = 1; i < channels; i++) {
        p.drawLine(0, i * (m_channelHeight + m_channelDistance) - 1, rect.width() - 1, i * (m_channelHeight + m_channelDistance) - 1);
    }
    p.end();
}

// cppcheck-suppress unusedFunction
void MonitorAudioLevel::setAudioValues(const QVector<double> &values)
{
    m_values = values;
    if (m_peaks.size() != m_values.size()) {
        m_peaks = values;
        drawBackground(values.size());
    } else {
        for (int i = 0; i < m_values.size(); i++) {
            m_peaks[i] -= 0.2;
            if (m_values.at(i) > m_peaks.at(i)) {
                m_peaks[i] = m_values.at(i);
            }
        }
    }
    update();
}

void MonitorAudioLevel::setVisibility(bool enable)
{
    if (enable) {
        setVisible(true);
        setFixedHeight(m_height);
    } else {
        // set height to 0 so the toolbar layout is not affected
        setFixedHeight(0);
        setVisible(false);
    }
}

void MonitorAudioLevel::paintEvent(QPaintEvent *pe)
{
    if (!isVisible()) {
        return;
    }
    QPainter p(this);
    p.setClipRect(pe->rect());
    QRect rect(0, 0, width(), height());
    if (m_values.isEmpty()) {
        p.setOpacity(0.2);
        p.drawPixmap(rect, m_pixmap);
        return;
    }
    p.drawPixmap(rect, m_pixmap);
    p.setOpacity(0.9);
    int width = m_channelDistance == 1 ? rect.width() : rect.width() - 1;
    for (int i = 0; i < m_values.count(); i++) {
        if (m_values.at(i) >= 100) {
            continue;
        }
        int val = IEC_ScaleMax(m_values.at(i), m_maxDb) * width;
        int peak = IEC_ScaleMax(m_peaks.at(i), m_maxDb) * width;
        p.fillRect(val, i * (m_channelHeight + m_channelDistance), width - val, m_channelFillHeight, palette().window());
        p.fillRect(peak, i * (m_channelHeight + m_channelDistance), 1, m_channelFillHeight, palette().text());
    }
}
