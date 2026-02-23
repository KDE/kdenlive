/*
SPDX-FileCopyrightText: 2021 Jean-Baptiste Mardelle <jb@kdenlive.org>
This file is part of Kdenlive. See www.kdenlive.org.

SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include "definitions.h"
#include "abstracttask.h"
#include <mlt++/MltProducer.h>
#include <mlt++/MltProfile.h>

#include <QDomElement>
#include <QList>
#include <QObject>
#include <QRunnable>
#include <set>

class ProjectClip;

class CacheTask : public AbstractTask
{
public:
    CacheTask(const ObjectId &owner, std::set<int> frames, int thumbsCount, int in, int out, QObject *object);
    ~CacheTask() override;
    /** @brief Method to generate a fix number of thumbnails spread over the clip's duration */
    static void start(const ObjectId &owner, int thumbsCount = 30, int in = 0, int out = 0, QObject *object = nullptr, bool force = false);
    /** @brief Method to generate thumbnails for a specific list of frames */
    static void start(const ObjectId &owner, std::set<int> frames, QObject *object = nullptr, bool force = false);

protected:
    void run() override;

private:
    int m_fullWidth;
    int m_thumbsCount;
    int m_in;
    int m_out;
    std::function<void()> m_readyCallBack;
    std::set<int> m_frames;
    QString m_errorMessage;
    void generateThumbnail(std::shared_ptr<ProjectClip>binClip);
};
