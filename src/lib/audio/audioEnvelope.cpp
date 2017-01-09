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
#include <cmath>

AudioEnvelope::AudioEnvelope(const QString &url, Mlt::Producer *producer, int offset, int length, int track, int startPos) :
    m_envelope(nullptr),
    m_offset(offset),
    m_length(length),
    m_track(track),
    m_startpos(startPos),
    m_envelopeSize(producer->get_length()),
    m_envelopeMax(0),
    m_envelopeMean(0),
    m_envelopeStdDev(0),
    m_envelopeStdDevCalculated(false),
    m_envelopeIsNormalized(false)
{
    // make a copy of the producer to avoid audio playback issues
    QString path = QString::fromUtf8(producer->get("resource"));
    if (path == QLatin1String("<playlist>") || path == QLatin1String("<tractor>") || path == QLatin1String("<producer>")) {
        path = url;
    }
    m_producer = new Mlt::Producer(*(producer->profile()), path.toUtf8().constData());
    connect(&m_watcher, &QFutureWatcherBase::finished, this, &AudioEnvelope::slotProcessEnveloppe);
    if (!m_producer || !m_producer->is_valid()) {
        qCDebug(KDENLIVE_LOG) << "// Cannot create envelope for producer: " << path;
    }
    m_info = new AudioInfo(m_producer);

    Q_ASSERT(m_offset >= 0);
    if (m_length > 0) {
        Q_ASSERT(m_length + m_offset <= m_envelopeSize);
        m_envelopeSize = m_length;
    }
}

AudioEnvelope::~AudioEnvelope()
{
    if (m_envelope != nullptr) {
        delete[] m_envelope;
    }
    delete m_info;
    delete m_producer;
}

const qint64 *AudioEnvelope::envelope()
{
    if (m_envelope == nullptr) {
        loadEnvelope();
    }
    return m_envelope;
}
int AudioEnvelope::envelopeSize() const
{
    return m_envelopeSize;
}

void AudioEnvelope::loadEnvelope()
{
    Q_ASSERT(m_envelope == nullptr);

    qCDebug(KDENLIVE_LOG) << "Loading envelope ...";

    int samplingRate = m_info->info(0)->samplingRate();
    mlt_audio_format format_s16 = mlt_audio_s16;
    int channels = 1;

    m_envelope = new qint64[m_envelopeSize];
    m_envelopeMax = 0;
    m_envelopeMean = 0;

    QTime t;
    t.start();
    int count = 0;
    m_producer->seek(m_offset);
    m_producer->set_speed(1.0); // This is necessary, otherwise we don't get any new frames in the 2nd run.
    for (int i = 0; i < m_envelopeSize; ++i) {
        Mlt::Frame *frame = m_producer->get_frame(i);
        qint64 position = mlt_frame_get_position(frame->get_frame());
        int samples = mlt_sample_calculator(m_producer->get_fps(), samplingRate, position);

        qint16 *data = static_cast<qint16 *>(frame->get_audio(format_s16, samplingRate, channels, samples));

        qint64 sum = 0;
        for (int k = 0; k < samples; ++k) {
            sum += abs(data[k]);
        }
        m_envelope[i] = sum;

        m_envelopeMean += sum;
        if (sum > m_envelopeMax) {
            m_envelopeMax = sum;
        }

//        qCDebug(KDENLIVE_LOG) << position << '|' << m_producer->get_playtime()
//                  << '-' << m_producer->get_in() << '+' << m_producer->get_out() << ' ';

        delete frame;

        count++;
        if (m_length > 0 && count > m_length) {
            break;
        }
    }
    m_envelopeMean /= m_envelopeSize;
    qCDebug(KDENLIVE_LOG) << "Calculating the envelope (" << m_envelopeSize << " frames) took "
                          << t.elapsed() << " ms.";
}

int AudioEnvelope::track() const
{
    return m_track;
}

int AudioEnvelope::startPos() const
{
    return m_startpos;
}

void AudioEnvelope::normalizeEnvelope(bool /*clampTo0*/)
{
    if (m_envelope == nullptr && !m_future.isRunning()) {
        m_future = QtConcurrent::run(this, &AudioEnvelope::loadEnvelope);
        m_watcher.setFuture(m_future);
    }
}

void AudioEnvelope::slotProcessEnveloppe()
{
    if (!m_envelopeIsNormalized) {

        m_envelopeMax = 0;
        qint64 newMean = 0;
        for (int i = 0; i < m_envelopeSize; ++i) {

            m_envelope[i] -= m_envelopeMean;

            /*if (clampTo0) {
                if (m_envelope[i] < 0) { m_envelope[i] = 0; }
            }*/

            if (m_envelope[i] > m_envelopeMax) {
                m_envelopeMax = m_envelope[i];
            }

            newMean += m_envelope[i];
        }
        m_envelopeMean = newMean / m_envelopeSize;

        m_envelopeIsNormalized = true;
    }
    emit envelopeReady(this);

}

QImage AudioEnvelope::drawEnvelope()
{
    if (m_envelope == nullptr) {
        loadEnvelope();
    }

    QImage img(m_envelopeSize, 400, QImage::Format_ARGB32);
    img.fill(qRgb(255, 255, 255));

    if (m_envelopeMax == 0) {
        return img;
    }

    for (int x = 0; x < img.width(); ++x) {
        double fy = m_envelope[x] / double(m_envelopeMax) * img.height();
        for (int y = img.height() - 1; y > img.height() - 1 - fy; --y) {
            img.setPixel(x, y, qRgb(50, 50, 50));
        }
    }
    return img;
}

void AudioEnvelope::dumpInfo() const
{
    if (m_envelope == nullptr) {
        qCDebug(KDENLIVE_LOG) << "Envelope not generated, no information available.";
    } else {
        qCDebug(KDENLIVE_LOG) << "Envelope info"
                              << "\n* size = " << m_envelopeSize
                              << "\n* max = " << m_envelopeMax
                              << "\n* µ = " << m_envelopeMean;
        if (m_envelopeStdDevCalculated) {
            qCDebug(KDENLIVE_LOG) << "* s = " << m_envelopeStdDev;
        }
    }
}

