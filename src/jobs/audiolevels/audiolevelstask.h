/*
    SPDX-FileCopyrightText: 2013-2018 Meltytech LLC
    SPDX-FileCopyrightText: 2013-2018 Dan Dennedy <dan@dennedy.org>
    SPDX-FileCopyrightText: 2021 Jean-Baptiste Mardelle <jb@kdenlive.org>
    SPDX-FileCopyrightText: 2024 Étienne André <eti.andre@gmail.com>

    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include "jobs/abstracttask.h"

#include <QObject>
#include <QRunnable>
#include <bin/projectclip.h>

class AudioLevelsTask : public AbstractTask
{
public:
    AudioLevelsTask(const ObjectId &owner, QObject *object);
    static void start(const ObjectId &owner, QObject *object, bool force = false);
    static QVector<int16_t> getLevelsFromCache(const QString &cachePath);
    static void saveLevelsToCache(const QString &cachePath, const QVector<int16_t> &levels);

protected:
    void run() override;

private:
    static void storeLevels(const std::shared_ptr<ProjectClip> &binClip, int stream, const QVector<int16_t> &levels);
    static void storeMax(const std::shared_ptr<ProjectClip> &binClip, int stream, const QVector<int16_t> &levels);
    void progressCallback(const std::shared_ptr<ProjectClip> &binClip, const QVector<int16_t> &levels, int streamIdx, int progress);
    QElapsedTimer m_timer;
};
