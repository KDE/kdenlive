/***************************************************************************
 *   Copyright (C) 2012 by Simon Andreas Eugster (simon.eu@gmail.com)      *
 *   This file is part of kdenlive. See www.kdenlive.org.                  *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 ***************************************************************************/

#include "audioInfo.h"

#include "audioStreamInfo.h"
#include <QString>
#include <iostream>
#include <cstdlib>

AudioInfo::AudioInfo(Mlt::Producer *producer) :
m_producer(NULL)
{
    // Since we already receive an MLT producer, we do not need to initialize MLT:
    // Mlt::Factory::init(NULL);

    // Get the number of streams and add the information of each of them if it is an audio stream.
    int streams = atoi(producer->get("meta.media.nb_streams"));
    for (int i = 0; i < streams; i++) {
        QByteArray propertyName = QString("meta.media.%1.stream.type").arg(i).toLocal8Bit();

        if (strcmp("audio", producer->get(propertyName.data())) == 0) {
            m_list << new AudioStreamInfo(producer, i);
        }

    }
}

AudioInfo::~AudioInfo()
{
    foreach (AudioStreamInfo *info, m_list) {
        delete info;
    }
}

int AudioInfo::size() const
{
    return m_list.size();
}

AudioStreamInfo const* AudioInfo::info(int pos) const
{
    Q_ASSERT(pos >= 0);
    Q_ASSERT(pos <= m_list.size());

    return m_list.at(pos);
}

void AudioInfo::dumpInfo() const
{
    foreach (AudioStreamInfo *info, m_list) {
        info->dumpInfo();
    }
}
