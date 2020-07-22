/*
Copyright (C) 2016  Jean-Baptiste Mardelle <jb@kdenlive.org>
This file is part of Kdenlive. See www.kdenlive.org.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of
the License or (at your option) version 3 or any later version
accepted by the membership of KDE e.V. (or its successor approved
by the membership of KDE e.V.), which shall act as a proxy
defined in Section 14 of version 3 of the license.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "monitoraudiolevel.h"
#include "core.h"
#include "profiles/profilemodel.hpp"

#include "mlt++/Mlt.h"

#include <cmath>

#include <QFont>
#include <QPaintEvent>
#include <QPainter>

const double log_factor = 1.0 / log10(1.0 / 127);

static inline double levelToDB(double level)
{
    if (level <= 0) {
        return -100;
    }
    return 100 * (1.0 - log10(level) * log_factor);
}

MonitorAudioLevel::MonitorAudioLevel(int height, QWidget *parent)
    : ScopeWidget(parent)
    , audioChannels(2)
    , m_height(height)
    , m_channelHeight(height / 2)
    , m_channelDistance(2)
    , m_channelFillHeight(m_channelHeight)
{
    setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Preferred);
    m_filter.reset(new Mlt::Filter(pCore->getCurrentProfile()->profile(), "audiolevel"));
    if (!m_filter->is_valid()) {
        isValid = false;
        return;
    }
    m_filter->set("iec_scale", 0);
    isValid = true;
}

MonitorAudioLevel::~MonitorAudioLevel()
{
}

void MonitorAudioLevel::refreshScope(const QSize & /*size*/, bool /*full*/)
{
    SharedFrame sFrame;
    while (m_queue.count() > 0) {
        sFrame = m_queue.pop();
        if (sFrame.is_valid() && sFrame.get_audio_samples() > 0) {
            mlt_audio_format format = mlt_audio_s16;
            int channels = sFrame.get_audio_channels();
            int frequency = sFrame.get_audio_frequency();
            int samples = sFrame.get_audio_samples();
            Mlt::Frame mFrame = sFrame.clone(true, false, false);
            m_filter->process(mFrame);
            mFrame.get_audio(format, frequency, channels, samples);
            if (samples == 0) {
                // There was an error processing audio from frame
                continue;
            }
            QVector<int> levels;
            for (int i = 0; i < audioChannels; i++) {
                QString s = QStringLiteral("meta.media.audio_level.%1").arg(i);
                levels << (int)levelToDB(mFrame.get_double(s.toLatin1().constData()));
            }
            QMetaObject::invokeMethod(this, "setAudioValues", Qt::QueuedConnection, Q_ARG(QVector<int>, levels));
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
    gradient.setColorAt(0.0, Qt::darkGreen);
    gradient.setColorAt(0.379, Qt::darkGreen);
    gradient.setColorAt(0.38, Qt::green); // -20db
    gradient.setColorAt(0.868, Qt::green);
    gradient.setColorAt(0.869, Qt::yellow); // -2db
    gradient.setColorAt(0.95, Qt::yellow);
    gradient.setColorAt(0.951, Qt::red); // 0db
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
    p.setOpacity(0.4);
    p.setFont(ft);
    p.fillRect(rect, QBrush(gradient));

    // Channel labels are horizontal along the bottom.
    QVector<int> dbscale;
    dbscale << 0 << -2 << -5 << -10 << -15 << -20 << -30 << -45;
    int dbLabelCount = dbscale.size();
    // dB scale is horizontal along the bottom
    int prevX = m_pixmap.width() * 2;
    int y = totalHeight + textHeight;
    for (int i = 0; i < dbLabelCount; i++) {
        int value = dbscale.at(i);
        QString label = QString::number(value);
        int labelWidth = fontMetrics().horizontalAdvance(label);
        double xf = pow(10.0, (double)dbscale.at(i) / 50.0) * m_pixmap.width() * 40.0 / 42;
        if (xf + labelWidth / 2 > m_pixmap.width()) {
            xf = width() - labelWidth / 2;
        }
        if (prevX - (xf + labelWidth / 2) >= 2) {
            p.setPen(palette().dark().color());
            p.drawLine(xf, 0, xf, totalHeight - 1);
            xf -= labelWidth / 2;
            p.setPen(palette().text().color().rgb());
            p.drawText((int)xf, y, label);
            prevX = xf;
        }
    }
    p.setOpacity(isEnabled() ? 1 : 0.5);
    p.setPen(palette().dark().color());
    // Clear space between the 2 channels
    p.setCompositionMode(QPainter::CompositionMode_Source);
    if (m_channelHeight < 4) {
        // too many audio channels, simple line between channels
        m_channelDistance = 1;
        m_channelFillHeight = m_channelHeight;
        for (int i = 0; i < channels; i++) {
            p.drawLine(0, i * (m_channelHeight + m_channelDistance), rect.width() - 1, i * (m_channelHeight + m_channelDistance));
        }
    } else {
        m_channelDistance = 2;
        m_channelFillHeight = m_channelHeight - 2;
        for (int i = 0; i < channels; i++) {
            p.drawRect(0, i * (m_channelHeight + m_channelDistance), rect.width() - 1, m_channelHeight - 1);
            if (i > 0) {
                p.fillRect(0, i * (m_channelHeight + m_channelDistance) - 2, rect.width(), 2, Qt::transparent);
            }
        }
    }
    p.end();
}

// cppcheck-suppress unusedFunction
void MonitorAudioLevel::setAudioValues(const QVector<int> &values)
{
    m_values = values;
    if (m_peaks.size() != m_values.size()) {
        m_peaks = values;
        drawBackground(values.size());
    } else {
        for (int i = 0; i < m_values.size(); i++) {
            m_peaks[i]--;
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
    p.setPen(palette().dark().color());
    p.setOpacity(0.9);
    int width = m_channelDistance == 1 ? rect.width() : rect.width() - 1;
    for (int i = 0; i < m_values.count(); i++) {
        if (m_values.at(i) >= 100) {
            continue;
        }
        int val = (50 + m_values.at(i)) / 150.0 * rect.width();
        p.fillRect(val, i * (m_channelHeight + m_channelDistance) + 1, width - val, m_channelFillHeight, palette().dark());
        p.fillRect((50 + m_peaks.at(i)) / 150.0 * rect.width(), i * (m_channelHeight + m_channelDistance) + 1, 1, m_channelFillHeight, palette().text());
    }
}
