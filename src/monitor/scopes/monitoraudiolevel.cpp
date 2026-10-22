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
}

void MonitorAudioLevel::refreshScope(const QSize & /*size*/, bool /*full*/)
{
    SharedFrame sFrame;
    // The queue can be cleared from the GUI thread meanwhile, so don't block on pop()
    while (m_queue.tryPop(sFrame)) {
        if (sFrame.is_valid()) {
            QMap<int, QVector<double>> levelsMap;
            // Tracks levels
            for (auto &tid : audioTrackIds) {
                QVector<double> levels;
                for (int ix = 0; ix < audioChannels; ix++) {
                    const QByteArray audioKey = QStringLiteral("meta.audio.track.%1.audio_level.%2").arg(tid).arg(ix).toLatin1();
                    const double channelData = sFrame.get_double(audioKey.constData());
                    levels << audioLevelDbFromLinear(channelData);
                }
                if (tid == -1) {
                    Q_EMIT audioLevelsAvailable(levels);
                }
                levelsMap.insert(tid, levels);
            }
            Q_EMIT pCore->audioLevelsAvailable(levelsMap);
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
