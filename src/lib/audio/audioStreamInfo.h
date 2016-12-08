/*
Copyright (C) 2012  Simon A. Eugster (Granjow)  <simon.eu@gmail.com>
This file is part of kdenlive. See www.kdenlive.org.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
*/

#ifndef AUDIOSTREAMINFO_H
#define AUDIOSTREAMINFO_H

#include <mlt++/Mlt.h>
#include <QString>

/**
  Provides easy access to properties of an audio stream.
  */
class AudioStreamInfo
{
public:
    AudioStreamInfo(Mlt::Producer *producer, int audioStreamIndex);
    ~AudioStreamInfo();

    int samplingRate() const;
    int channels() const;
    int bitrate() const;
    const QString &samplingFormat() const;
    int audio_index() const;
    int ffmpeg_audio_index() const;
    void dumpInfo() const;

private:
    int m_audioStreamIndex;
    int m_ffmpegAudioIndex;
    int m_samplingRate;
    int m_channels;
    int m_bitRate;
    QString m_samplingFormat;

};

#endif // AUDIOSTREAMINFO_H
