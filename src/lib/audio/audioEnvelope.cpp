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
#include "kdenlive_debug.h"
#include <QImage>
#include <QTime>
#include <QtConcurrent>
#include <algorithm>
#include <cmath>

AudioEnvelope::AudioEnvelope(const QString &url, Mlt::Producer *producer, int offset, int length, int track, int startPos) :
    m_offset(offset),
    m_track(track),
    m_startpos(startPos),
    m_envelopeSize(producer->get_length())
{
    // make a copy of the producer to avoid audio playback issues
    QString path = QString::fromUtf8(producer->get("resource"));
    if (path == QLatin1String("<playlist>") || path == QLatin1String("<tractor>") || path == QLatin1String("<producer>")) {
        path = url;
    }
    m_producer.reset(new Mlt::Producer(*(producer->profile()), path.toUtf8().constData()));
    connect(&m_watcher, &QFutureWatcherBase::finished, this, [this]{ envelopeReady(this); } );
    if (!m_producer || !m_producer->is_valid()) {
        qCDebug(KDENLIVE_LOG) << "// Cannot create envelope for producer: " << path;
    }
    m_info.reset(new AudioInfo(m_producer.get()));

    Q_ASSERT(m_offset >= 0);
    if (length > 0) {
        Q_ASSERT(length + m_offset <= m_envelopeSize);
        m_envelopeSize = length;
    }
}

AudioEnvelope::~AudioEnvelope()
{
}

void AudioEnvelope::startComputeEnvelope() {
    m_audioSummary = QtConcurrent::run(this, &AudioEnvelope::loadAndNormalizeEnvelope);
    m_watcher.setFuture(m_audioSummary);
}

bool AudioEnvelope::hasComputationStarted() const {
    // An empty qFuture is canceled. QtConcurrent::run() returns a
    // future that does not support cancelation, so this is a good way
    // to check whether the computations have started.
    return !m_audioSummary.isCanceled();
}

const AudioEnvelope::AudioSummary& AudioEnvelope::audioSummary() {
    Q_ASSERT(hasComputationStarted());
    m_audioSummary.waitForFinished();
    Q_ASSERT(m_audioSummary.constBegin() != m_audioSummary.constEnd());
    // We use this instead of m_audioSummary.result() in order to return
    // a const reference instead of a copy.
    return *m_audioSummary.constBegin();
}

const std::vector<qint64>& AudioEnvelope::envelope()
{
    // Blocks until the summary is available.
    return audioSummary().audioAmplitudes;
}

AudioEnvelope::AudioSummary AudioEnvelope::loadAndNormalizeEnvelope() const {
    qCDebug(KDENLIVE_LOG) << "Loading envelope ...";

    AudioSummary summary(m_envelopeSize);

    int samplingRate = m_info->info(0)->samplingRate();
    mlt_audio_format format_s16 = mlt_audio_s16;
    int channels = 1;

    QTime t;
    t.start();
    m_producer->seek(m_offset);
    m_producer->set_speed(1.0); // This is necessary, otherwise we don't get any new frames in the 2nd run.
    for (int i = 0; i < summary.audioAmplitudes.size() ; ++i) {
        std::unique_ptr<Mlt::Frame> frame(m_producer->get_frame(i));
        qint64 position = mlt_frame_get_position(frame->get_frame());
        int samples = mlt_sample_calculator(m_producer->get_fps(), samplingRate, position);

        qint16 *data = static_cast<qint16 *>(frame->get_audio(format_s16, samplingRate, channels, samples));
        summary.audioAmplitudes[i] = 0;
        for (int k = 0; k < samples; ++k) {
          summary.audioAmplitudes[i] += abs(data[k]);
        }
    }
    qCDebug(KDENLIVE_LOG) << "Calculating the envelope (" << m_envelopeSize << " frames) took "
                          << t.elapsed() << " ms.";

    qCDebug(KDENLIVE_LOG) << "Normalizing envelope ...";
    const qint64 meanBeforeNormalization = std::accumulate(summary.audioAmplitudes.begin(),
					    summary.audioAmplitudes.end(), 0LL) /
      summary.audioAmplitudes.size();

    // Normalize the envelope.
    for (int i = 0; i < summary.audioAmplitudes.size(); ++i) {
      summary.audioAmplitudes[i] -= meanBeforeNormalization;
    }

    summary.amplitudeMax = *std::max_element(summary.audioAmplitudes.begin(),
					     summary.audioAmplitudes.end());
    return summary;
}

int AudioEnvelope::track() const
{
    return m_track;
}

int AudioEnvelope::startPos() const
{
    return m_startpos;
}

QImage AudioEnvelope::drawEnvelope()
{
  const AudioSummary& summary = audioSummary();
    QImage img(m_envelopeSize, 400, QImage::Format_ARGB32);
    img.fill(qRgb(255, 255, 255));

    if (summary.amplitudeMax == 0) {
        return img;
    }

    for (int x = 0; x < img.width(); ++x) {
        double fy = summary.audioAmplitudes[x] / double(summary.amplitudeMax) * img.height();
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
      const AudioSummary& summary = audioSummary();
      qCDebug(KDENLIVE_LOG) << "Envelope info"
			    << "\n* size = " << summary.audioAmplitudes.size()
			    << "\n* max = " << summary.amplitudeMax;
  }
}

