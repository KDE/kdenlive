/*
    this file is part of Kdenlive, the Libre Video Editor by KDE
    SPDX-FileCopyrightText: 2024 Darby Johnston <darbyjohnston@yahoo.com>

    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "otioutil.h"

#include "core.h"

#include <QMap>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
}

namespace {

const QMap<std::string, QColor> otioMarkerColors = {{"PINK", QColor(255, 192, 203)}, {"RED", QColor(255, 0, 0)},       {"ORANGE", QColor(255, 120, 0)},
                                                    {"YELLOW", QColor(255, 255, 0)}, {"GREEN", QColor(0, 255, 0)},     {"CYAN", QColor(0, 255, 255)},
                                                    {"BLUE", QColor(0, 0, 255)},     {"PURPLE", QColor(160, 32, 240)}, {"MAGENTA", QColor(255, 0, 255)},
                                                    {"BLACK", QColor(0, 0, 0)},      {"WHITE", QColor(255, 255, 255)}};

int closestColor(const QColor &color, const QList<QColor> &colors)
{
    int out = -1;
    const float colorHue = color.hueF();
    float closestHue = 1.F;
    for (auto i = colors.begin(); i != colors.end(); ++i) {
        const float otioHue = i->hueF();
        const float diff = qAbs(colorHue - otioHue);
        if (diff < closestHue) {
            out = i - colors.begin();
            closestHue = diff;
        }
    }
    return out;
}

} // namespace

int fromOtioMarkerColor(const std::string &otioColor)
{
    int markerType = -1;
    QList<int> ids;
    QList<QColor> colors;
    for (auto i = pCore->markerTypes.begin(); i != pCore->markerTypes.end(); ++i) {
        ids.push_back(i.key());
        colors.push_back(i->color);
    }
    auto j = otioMarkerColors.find(otioColor);
    if (j != otioMarkerColors.end()) {
        const int index = closestColor(*j, colors);
        if (index != -1) {
            markerType = ids[index];
        }
    }
    return markerType;
}

std::string toOtioMarkerColor(int markerType)
{
    std::string out = "RED";
    const auto i = pCore->markerTypes.find(markerType);
    if (i != pCore->markerTypes.end()) {
        QColor color = i->color;
        const int index = closestColor(color, otioMarkerColors.values());
        if (index != -1) {
            out = otioMarkerColors.key(otioMarkerColors.values()[index]);
        }
    }
    return out;
}

namespace {
class FFmpegInfo
{
public:
    FFmpegInfo(const QString &fileName);

    ~FFmpegInfo();

    const QSize &getVideoSize() const { return m_videoSize; }
    const QString &getTimecode() const { return m_timecode; }

private:
    int getStream(int streamType) const;
    QString getTimecode(int stream) const;

    AVFormatContext *m_context = nullptr;
    QSize m_videoSize;
    QString m_timecode;
};

FFmpegInfo::FFmpegInfo(const QString &fileName)
{
    int r = avformat_open_input(&m_context, fileName.toLocal8Bit().data(), nullptr, nullptr);
    if (r >= 0) {
        r = avformat_find_stream_info(m_context, nullptr);
        if (r >= 0) {
            // Get the size and timecode from the video stream.
            int stream = getStream(AVMEDIA_TYPE_VIDEO);
            if (stream != -1) {
                m_videoSize = QSize(m_context->streams[stream]->codecpar->width, m_context->streams[stream]->codecpar->height);
                m_timecode = getTimecode(stream);
            }

            // If the video stream did not have timecode check the data stream.
            if (m_timecode.isEmpty()) {
                stream = getStream(AVMEDIA_TYPE_DATA);
                if (stream != -1) {
                    m_timecode = getTimecode(stream);
                }
            }
        }
    }
}

FFmpegInfo::~FFmpegInfo()
{
    if (m_context) {
        avformat_close_input(&m_context);
    }
}

int FFmpegInfo::getStream(int streamType) const
{
    int stream = -1;
    // Check if there is a default stream to use.
    for (unsigned int i = 0; i < m_context->nb_streams; ++i) {
        if (streamType == m_context->streams[i]->codecpar->codec_type && AV_DISPOSITION_DEFAULT == m_context->streams[i]->disposition) {
            stream = i;
            break;
        }
    }
    // If there is no default stream, use the first stream.
    if (-1 == stream) {
        for (unsigned int i = 0; i < m_context->nb_streams; ++i) {
            if (streamType == m_context->streams[i]->codecpar->codec_type) {
                stream = i;
                break;
            }
        }
    }
    return stream;
}

QString FFmpegInfo::getTimecode(int stream) const
{
    QString out;
    AVDictionaryEntry *tag = nullptr;
    while ((tag = av_dict_get(m_context->streams[stream]->metadata, "", tag, AV_DICT_IGNORE_SUFFIX))) {
        if (0 == QString(tag->key).compare("timecode", Qt::CaseInsensitive)) {
            out = tag->value;
        }
    }
    return out;
}
} // namespace

QSize getVideoSize(const QString &fileName)
{
    return FFmpegInfo(fileName).getVideoSize();
}

QString getTimecode(const QString &fileName)
{
    return FFmpegInfo(fileName).getTimecode();
}