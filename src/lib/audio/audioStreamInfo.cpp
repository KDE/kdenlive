/*
Copyright (C) 2012  Simon A. Eugster (Granjow)  <simon.eu@gmail.com>
This file is part of kdenlive. See www.kdenlive.org.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
*/

#include "audioStreamInfo.h"

#include "kdenlive_debug.h"
#include <QString>
#include <cstdlib>

AudioStreamInfo::AudioStreamInfo(Mlt::Producer *producer, int audioStreamIndex) :
    m_audioStreamIndex(audioStreamIndex)
    , m_ffmpegAudioIndex(0)
    , m_samplingRate(48000)
    , m_channels(2)
    , m_bitRate(0)
{
    if (audioStreamIndex > -1) {
        QByteArray key;
        key = QStringLiteral("meta.media.%1.codec.sample_fmt").arg(audioStreamIndex).toLocal8Bit();
        m_samplingFormat = QString::fromLatin1(producer->get(key.data()));

        key = QStringLiteral("meta.media.%1.codec.sample_rate").arg(audioStreamIndex).toLocal8Bit();
        m_samplingRate = producer->get_int(key.data());

        key = QStringLiteral("meta.media.%1.codec.bit_rate").arg(audioStreamIndex).toLocal8Bit();
        m_bitRate = producer->get_int(key.data());

        key = QStringLiteral("meta.media.%1.codec.channels").arg(audioStreamIndex).toLocal8Bit();
        m_channels = producer->get_int(key.data());

        int streams = producer->get_int("meta.media.nb_streams");
        QList<int> audioStreams;
        for (int i = 0; i < streams; ++i) {
            QByteArray propertyName = QStringLiteral("meta.media.%1.stream.type").arg(i).toLocal8Bit();
            QString type = producer->get(propertyName.data());
            if (type == QLatin1String("audio")) {
                audioStreams << i;
            }
        }
        if (audioStreams.count() > 1) {
            m_ffmpegAudioIndex = audioStreams.indexOf(m_audioStreamIndex);
        }
    }
}

AudioStreamInfo::~AudioStreamInfo()
{
}

int AudioStreamInfo::samplingRate() const
{
    return m_samplingRate;
}

int AudioStreamInfo::channels() const
{
    return m_channels;
}

int AudioStreamInfo::bitrate() const
{
    return m_bitRate;
}

int AudioStreamInfo::audio_index() const
{
    return m_audioStreamIndex;
}

int AudioStreamInfo::ffmpeg_audio_index() const
{
    return m_ffmpegAudioIndex;
}

void AudioStreamInfo::dumpInfo() const
{
    qCDebug(KDENLIVE_LOG) << "Info for audio stream " << m_audioStreamIndex
                          << "\n\tChannels: " << m_channels
                          << "\n\tSampling rate: " << m_samplingRate
                          << "\n\tBit rate: " << m_bitRate;
}
