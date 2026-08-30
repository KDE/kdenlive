/*
SPDX-FileCopyrightText: 2016 Jean-Baptiste Mardelle <jb@kdenlive.org>
This file is part of Kdenlive. See www.kdenlive.org.

SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "monitoraudiolevel.h"
#include "core.h"

#include <cmath>

#include <QFont>
#include <QPaintEvent>
#include <QPainter>
#include <QVBoxLayout>

MonitorAudioLevel::MonitorAudioLevel(QWidget *parent)
    : ScopeWidget(parent)
    , audioChannels(2)
{
    setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding);
    setFixedHeight(height()); // Fixed height for QToolbar to prevent expansion
    isValid = true;
    m_audioLevelWidget = std::make_unique<AudioLevelWidget>(this, Qt::Horizontal, AudioLevel::TickLabelsMode::HideIfSpaceIsLimited, true);
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_audioLevelWidget.get());
    connect(this, &MonitorAudioLevel::audioLevelsAvailable, this, &MonitorAudioLevel::setAudioValues);
}

MonitorAudioLevel::~MonitorAudioLevel() = default;

double MonitorAudioLevel::audioLevelDbFromLinear(double dbValue)
{
    if (dbValue <= 0.0) return -100.0;

    double level = 20.0 * log10(qMin(dbValue, 1.0));
    return qRound(level * 10.0) / 10.0;
    // return log10(dbValue / 1.18) * 20;
}

void MonitorAudioLevel::refreshScope(const QSize & /*size*/, bool /*full*/)
{
    SharedFrame sFrame;
    while (m_queue.count() > 0) {
        sFrame = m_queue.pop();
        if (sFrame.is_valid()) {
            QMap<int, QVector<double>> levelsMap;
            // Tracks levels
            for (auto &tid : audioTrackIds) {
                QVector<double> levels;
                for (int ix = 0; ix < audioChannels; ix++) {
                    const QByteArray audioKey = QStringLiteral("meta.audio.track.%1.audio_level.%2").arg(tid).arg(ix).toLatin1();
                    const double channelData = sFrame.get_double(audioKey.constData());
                    qDebug() << ":: CHANNEL DATA: " << channelData;
                    levels << audioLevelDbFromLinear(channelData);
                }
                if (tid == -1) {
                    Q_EMIT audioLevelsAvailable(levels);
                }
                qDebug() << ":::: CHECKING AUDIO LEVELS FOR: " << tid << " = " << levels;
                levelsMap.insert(tid, levels);
            }
            Q_EMIT pCore->audioLevelsAvailable(levelsMap);

            /*int samples = sFrame.get_audio_samples();
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
            Q_EMIT audioLevelsAvailable(levels);*/
        }
    }
}

void MonitorAudioLevel::refreshPixmap()
{
    if (m_audioLevelWidget) m_audioLevelWidget->refreshPixmap();
}

void MonitorAudioLevel::setAudioValues(const QVector<double> &values)
{
    if (m_audioLevelWidget) m_audioLevelWidget->setAudioValues(values);
}

QSize MonitorAudioLevel::sizeHint() const
{
    if (!isVisible()) {
        return QSize(0, 0);
    }
    if (m_audioLevelWidget) return m_audioLevelWidget->sizeHint();
    return QWidget::sizeHint();
}

void MonitorAudioLevel::setVisibility(bool enable)
{
    setVisible(enable);
    if (m_audioLevelWidget) {
        m_audioLevelWidget->setVisible(enable);
    }
}
