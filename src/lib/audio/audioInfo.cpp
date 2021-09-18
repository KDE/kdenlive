/*
    SPDX-FileCopyrightText: 2012 Simon Andreas Eugster (simon.eu@gmail.com)
    This file is part of kdenlive. See www.kdenlive.org.

SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "audioInfo.h"

#include "audioStreamInfo.h"
#include <QString>
#include <cstdlib>

AudioInfo::AudioInfo(const std::shared_ptr<Mlt::Producer> &producer)
{
    // Since we already receive an MLT producer, we do not need to initialize MLT:
    // Mlt::Factory::init(nullptr);
    m_list = QList<AudioStreamInfo *>();
    // Get the number of streams and add the information of each of them if it is an audio stream.
    int streams = producer->get_int("meta.media.nb_streams");
    for (int i = 0; i < streams; ++i) {
        QByteArray propertyName = QStringLiteral("meta.media.%1.stream.type").arg(i).toLocal8Bit();
        const char *streamtype = producer->get(propertyName.data());
        if ((streamtype != nullptr) && strcmp("audio", streamtype) == 0) {
            m_list << new AudioStreamInfo(producer, i);
        }
    }
}

AudioInfo::~AudioInfo()
{
    for (AudioStreamInfo *info : qAsConst(m_list)) {
        delete info;
    }
}

int AudioInfo::size() const
{
    return m_list.size();
}

AudioStreamInfo const *AudioInfo::info(int pos) const
{
    Q_ASSERT(pos >= 0);
    Q_ASSERT(pos < m_list.size());

    return m_list.at(pos);
}

void AudioInfo::dumpInfo() const
{
    for (AudioStreamInfo *info : m_list) {
        info->dumpInfo();
    }
}
