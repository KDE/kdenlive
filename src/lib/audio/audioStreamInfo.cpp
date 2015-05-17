/*
Copyright (C) 2012  Simon A. Eugster (Granjow)  <simon.eu@gmail.com>
This file is part of kdenlive. See www.kdenlive.org.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
*/

#include "audioStreamInfo.h"

#include <QDebug>
#include <QString>
#include <cstdlib>

AudioStreamInfo::AudioStreamInfo(Mlt::Producer *producer, int audioStreamIndex) :
    m_audioStreamIndex(audioStreamIndex)
{
    QByteArray key;

    key = QString::fromLatin1("meta.media.%1.codec.sample_fmt").arg(audioStreamIndex).toLocal8Bit();
    m_samplingFormat = QString::fromLatin1(producer->get(key.data()));

    key = QString::fromLatin1("meta.media.%1.codec.sample_rate").arg(audioStreamIndex).toLocal8Bit();
    m_samplingRate = producer->get_int(key.data());

    key = QString::fromLatin1("meta.media.%1.codec.bit_rate").arg(audioStreamIndex).toLocal8Bit();
    m_bitRate = producer->get_int(key.data());

    key = QString::fromLatin1("meta.media.%1.codec.channels").arg(audioStreamIndex).toLocal8Bit();
    m_channels = producer->get_int(key.data());
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

void AudioStreamInfo::dumpInfo() const
{
    qDebug() << "Info for audio stream " << m_audioStreamIndex
             << "\n\tChannels: " << m_channels
             << "\n\tSampling rate: " << m_samplingRate
             << "\n\tBit rate: " << m_bitRate;
}
