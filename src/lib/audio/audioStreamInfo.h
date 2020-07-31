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

#include <QString>
#include <QMap>
#include <memory>
#include <mlt++/Mlt.h>

/**
  Provides easy access to properties of an audio stream.
  */
class AudioStreamInfo
{
public:
    // TODO make that access a shared ptr instead of raw
    AudioStreamInfo(const std::shared_ptr<Mlt::Producer> &producer, int audioStreamIndex);
    ~AudioStreamInfo();

    int samplingRate() const;
    int channels() const;
    /** @brief returns a list of audio stream index > stream description */
    QMap <int, QString> streams() const;
    /** @brief returns a list of audio stream index > channels per stream */
    QMap <int, int> streamChannels() const;
    /** @brief returns the channel count for a stream */
    int channelsForStream(int stream) const;
    /** @brief returns a list of audio channels per active stream */
    QList <int> activeStreamChannels() const;
    /** @brief returns a list of enabled audio stream indexes > stream description */
    QMap <int, QString> activeStreams() const;
    int bitrate() const;
    const QString &samplingFormat() const;
    int audio_index() const;
    int ffmpeg_audio_index() const;
    void dumpInfo() const;
    void setAudioIndex(const std::shared_ptr<Mlt::Producer> &producer, int ix);
    QMap<int, QString> streamInfo(Mlt::Properties sourceProperties);
    void updateActiveStreams(const QString &activeStreams);
    void renameStream(int ix, const QString streamName);

private:
    int m_audioStreamIndex;
    QMap <int, QString> m_audioStreams;
    QMap <int, int> m_audioChannels;
    QList <int> m_activeStreams;
    int m_ffmpegAudioIndex;
    int m_samplingRate;
    int m_channels;
    int m_bitRate;
    QString m_samplingFormat;
};

#endif // AUDIOSTREAMINFO_H
