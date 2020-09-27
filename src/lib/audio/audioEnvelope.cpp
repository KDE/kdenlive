/***************************************************************************
 *   Copyright (C) 2012 by Simon Andreas Eugster (simon.eu@gmail.com)      *
 *   This file is part of kdenlive. See www.kdenlive.org.                  *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 ***************************************************************************/

#include "audioEnvelope.h"
#include "audioStreamInfo.h"
#include "bin/bin.h"
#include "bin/projectclip.h"
#include "core.h"
#include "kdenlive_debug.h"
#include <QImage>
#include <QElapsedTimer>
#include <QtConcurrent>
#include <KLocalizedString>
#include <algorithm>
#include <cmath>
#include <memory>

AudioEnvelope::AudioEnvelope(const QString &binId, int clipId, size_t offset, size_t length, size_t startPos)
    : m_offset(offset)
    , m_clipId(clipId)
    , m_startpos(startPos)
{
    std::shared_ptr<ProjectClip> clip = pCore->bin()->getBinClip(binId);
    m_producer = clip->cloneProducer();
    if (length > 2000) {
        // Analyse on timeline clip zone only
        m_offset = 0;
        m_producer->set_in_and_out((int) offset, (int) (offset + length));
    }
    m_envelopeSize = (size_t)m_producer->get_playtime();

    m_producer->set("set.test_image", 1);
    connect(&m_watcher, &QFutureWatcherBase::finished, this, [this] { emit envelopeReady(this); });
    if (!m_producer || !m_producer->is_valid()) {
        qCDebug(KDENLIVE_LOG) << "// Cannot create envelope for producer: " << binId;
    } else {
        m_info = std::make_unique<AudioInfo>(m_producer);
    }
}

AudioEnvelope::~AudioEnvelope()
{
    if (hasComputationStarted()) {
        // This is better than nothing, but does not seem enough to
        // guarantee safe deletion of the AudioEnvelope while the
        // computations are running: if the computations have just
        // finished, m_watcher might be finished, but the signal
        // 'envelopeReady' might still be pending while AudioEnvelope is
        // being deleted, which can cause a crash according to
        // https://doc.qt.io/qt-5/qobject.html#dtor.QObject.
        m_audioSummary.waitForFinished();
        m_watcher.waitForFinished();
    }
}

void AudioEnvelope::startComputeEnvelope()
{
    m_audioSummary = QtConcurrent::run(this, &AudioEnvelope::loadAndNormalizeEnvelope);
    m_watcher.setFuture(m_audioSummary);
}

bool AudioEnvelope::hasComputationStarted() const
{
    // An empty qFuture is canceled. QtConcurrent::run() returns a
    // future that does not support cancelation, so this is a good way
    // to check whether the computations have started.
    return !m_audioSummary.isCanceled();
}

const AudioEnvelope::AudioSummary &AudioEnvelope::audioSummary()
{
    Q_ASSERT(hasComputationStarted());
    m_audioSummary.waitForFinished();
    Q_ASSERT(m_audioSummary.constBegin() != m_audioSummary.constEnd());
    // We use this instead of m_audioSummary.result() in order to return
    // a const reference instead of a copy.
    return *m_audioSummary.constBegin();
}

size_t AudioEnvelope::offset()
{
    return m_offset;
}

const std::vector<qint64> &AudioEnvelope::envelope()
{
    // Blocks until the summary is available.
    return audioSummary().audioAmplitudes;
}

AudioEnvelope::AudioSummary AudioEnvelope::loadAndNormalizeEnvelope() const
{
    qCDebug(KDENLIVE_LOG) << "Loading envelope ...";
    AudioSummary summary(m_envelopeSize);
    if (!m_info || m_info->size() < 1) {
        return summary;
    }
    int samplingRate = m_info->info(0)->samplingRate();
    mlt_audio_format format_s16 = mlt_audio_s16;
    int channels = 1;

    QElapsedTimer t;
    t.start();
    m_producer->seek(0);
    size_t max = summary.audioAmplitudes.size();
    for (size_t i = 0; i < max; ++i) {
        std::unique_ptr<Mlt::Frame> frame(m_producer->get_frame((int)i));
        qint64 position = mlt_frame_get_position(frame->get_frame());
        int samples = mlt_sample_calculator(m_producer->get_fps(), samplingRate, position);
        auto *data = static_cast<qint16 *>(frame->get_audio(format_s16, samplingRate, channels, samples));

        summary.audioAmplitudes[i] = 0;
        for (int k = 0; k < samples; ++k) {
            summary.audioAmplitudes[i] += abs(data[k]);
        }
        pCore->displayMessage(i18n("Processing data analysis"), ProcessingJobMessage, (int) (100 * i / max));
    }
    qCDebug(KDENLIVE_LOG) << "Calculating the envelope (" << m_envelopeSize << " frames) took " << t.elapsed() << " ms.";
    qCDebug(KDENLIVE_LOG) << "Normalizing envelope ...";
    const qint64 meanBeforeNormalization =
        std::accumulate(summary.audioAmplitudes.begin(), summary.audioAmplitudes.end(), 0LL) / (qint64)summary.audioAmplitudes.size();

    // Normalize the envelope.
    summary.amplitudeMax = 0;
    for (size_t i = 0; i < max; ++i) {
        summary.audioAmplitudes[i] -= meanBeforeNormalization;
        summary.amplitudeMax = std::max(summary.amplitudeMax, qAbs(summary.audioAmplitudes[i]));
    }
    pCore->displayMessage(i18n("Audio analysis finished"), OperationCompletedMessage, 300);
    return summary;
}

int AudioEnvelope::clipId() const
{
    return m_clipId;
}

size_t AudioEnvelope::startPos() const
{
    return m_startpos;
}

QImage AudioEnvelope::drawEnvelope()
{
    const AudioSummary &summary = audioSummary();

    QImage img((int)m_envelopeSize, 400, QImage::Format_ARGB32);
    img.fill(qRgb(255, 255, 255));

    if (summary.amplitudeMax == 0) {
        return img;
    }

    for (int x = 0; x < img.width(); ++x) {
        double fy = (double)summary.audioAmplitudes[(size_t)x] / double(summary.amplitudeMax) * (double)img.height();
        for (int y = img.height() - 1; y > img.height() - 1 - fy; --y) {
            img.setPixel(x, y, qRgb(50, 50, 50));
        }
    }
    return img;
}

void AudioEnvelope::dumpInfo()
{
    if (!m_audioSummary.isFinished()) {
        qCDebug(KDENLIVE_LOG) << "Envelope not yet generated, no information available.";
    } else {
        const AudioSummary &summary = audioSummary();
        qCDebug(KDENLIVE_LOG) << "Envelope info"
                              << "\n* size = " << summary.audioAmplitudes.size() << "\n* max = " << summary.amplitudeMax;
    }
}
