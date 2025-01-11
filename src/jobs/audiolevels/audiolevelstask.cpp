/*
    SPDX-FileCopyrightText: 2013-2021 Meltytech LLC
    SPDX-FileCopyrightText: 2021 Jean-Baptiste Mardelle <jb@kdenlive.org>
    SPDX-FileCopyrightText: 2024 Étienne André <eti.andre@gmail.com>

    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "audiolevelstask.h"
#include "audio/audioStreamInfo.h"
#include "bin/projectclip.h"
#include "bin/projectitemmodel.h"
#include "core.h"
#include "generators.h"

#include <KLocalizedString>
#include <KMessageWidget>
#include <QElapsedTimer>
#include <QFile>
#include <QList>
#include <QMutex>
#include <QRgb>
#include <QString>
#include <QVariantList>
#include <functional>
constexpr int UPDATE_DELAY_MS = 1000;

AudioLevelsTask::AudioLevelsTask(const ObjectId &owner, QObject *object)
    : AbstractTask(owner, AbstractTask::AUDIOTHUMBJOB, object)
{
    m_description = i18n("Audio thumbs");
}

void AudioLevelsTask::start(const ObjectId &owner, QObject *object, bool force)
{
    // See if there is already a task for this MLT service and resource.
    if (pCore->taskManager.hasPendingJob(owner, AbstractTask::AUDIOTHUMBJOB)) {
        qDebug() << "AUDIO LEVELS TASK STARTED TWICE!!!!";
        return;
    }
    AudioLevelsTask *task = new AudioLevelsTask(owner, object);
    // Otherwise, start a new audio levels generation thread.
    task->m_isForce = force;
    pCore->taskManager.startTask(owner.itemId, task);
}

void AudioLevelsTask::storeLevels(const std::shared_ptr<ProjectClip> &binClip, const int stream, const QVector<int16_t> &levels)
{
    const auto producer = binClip->originalProducer();
    producer->lock();

    auto *levelsCopy = new QVector<int16_t>(levels);
    producer->set(QStringLiteral("_kdenlive:audio%1").arg(stream).toUtf8().constData(), levelsCopy, 0,
                  [](void *ptr) { delete static_cast<QVector<int16_t> *>(ptr); });

    producer->unlock();
}

void AudioLevelsTask::storeMax(const std::shared_ptr<ProjectClip> &binClip, const int stream, const QVector<int16_t> &levels)
{
    const auto max = *std::max_element(levels.constBegin(), levels.constEnd());

    const auto producer = binClip->originalProducer();
    producer->lock();
    producer->set(QStringLiteral("_kdenlive:audio_max%1").arg(stream).toUtf8().constData(), max);
    producer->unlock();
}

QVector<int16_t> AudioLevelsTask::getLevelsFromCache(const QString &cachePath)
{
    qDebug() << "Loading audio levels from cache" << cachePath;
    QFile file(cachePath);
    QVector<int16_t> levels;
    if (file.open(QIODevice::ReadOnly)) {
        QDataStream in(&file);
        in >> levels;
        file.close();
    }
    return levels;
}

void AudioLevelsTask::saveLevelsToCache(const QString &cachePath, const QVector<int16_t> &levels)
{
    qDebug() << "Saving audio levels to cache" << cachePath;
    QFile file(cachePath);
    if (file.open(QIODevice::WriteOnly)) {
        QDataStream out(&file);
        out << levels;
        file.close();
    }
}

void AudioLevelsTask::progressCallback(const std::shared_ptr<ProjectClip> &binClip, const QVector<int16_t> &levels, const int streamIdx, const int progress)
{
    if (m_progress != progress) {
        m_progress = progress;
        QMetaObject::invokeMethod(m_object, "updateJobProgress");
    }

    if (m_timer.elapsed() > UPDATE_DELAY_MS && !m_isCanceled) {
        m_timer.restart();
        storeLevels(binClip, streamIdx, levels);
        QMetaObject::invokeMethod(m_object, "updateAudioThumbnail", Q_ARG(bool, false));
    }
}

void AudioLevelsTask::run()
{
    AbstractTaskDone whenFinished(m_owner.itemId, this);
    if (m_isCanceled || pCore->taskManager.isBlocked()) {
        return;
    }
    QMutexLocker lock(&m_runMutex);
    m_running = true;
    m_timer.start();

    const auto binClip = pCore->projectItemModel()->getClipByBinID(QString::number(m_owner.itemId));
    if (binClip == nullptr) {
        // Clip was deleted
        return;
    }
    if (binClip->audioChannels() == 0 || binClip->audioThumbCreated()) {
        // nothing to do
        return;
    }

    std::shared_ptr<Mlt::Producer> producer = binClip->originalProducer();
    if ((producer == nullptr) || !producer->is_valid()) {
        QMetaObject::invokeMethod(pCore.get(), "displayBinMessage", Qt::QueuedConnection,
                                  Q_ARG(QString, i18n("Audio thumbs: cannot open file %1", QFileInfo(binClip->url()).fileName())),
                                  Q_ARG(int, int(KMessageWidget::Warning)));
        return;
    }
    const int lengthInFrames = producer->get_length();
    if (lengthInFrames == INT_MAX || lengthInFrames == 0) {
        // This is a broken file or live feed, don't attempt to generate audio thumbnails
        QMetaObject::invokeMethod(pCore.get(), "displayBinMessage", Qt::QueuedConnection,
                                  Q_ARG(QString, i18n("Audio thumbs: unknown file length for %1", QFileInfo(binClip->url()).fileName())),
                                  Q_ARG(int, int(KMessageWidget::Warning)));
        return;
    }

    QString service = producer->get("mlt_service");
    if (service == QLatin1String("avformat-novalidate")) {
        service = QStringLiteral("avformat");
    } else if (service.startsWith(QLatin1String("xml"))) {
        service = QStringLiteral("xml-nogl");
    }
    const QString res = qstrdup(producer->get("resource"));

    for (auto streamIdx : binClip->audioInfo()->streams().keys()) {
        if (m_isCanceled) {
            break;
        }

        auto clbk = [this, binClip, streamIdx](const int progress, const QVector<int16_t> &levels) { progressCallback(binClip, levels, streamIdx, progress); };

        const QString cachePath = binClip->getAudioThumbPath(streamIdx);
        QVector<int16_t> levels;
        bool skipSaving = false;
        if (!m_isCanceled && !m_isForce && QFile::exists(cachePath)) {
            // load from cache
            levels = getLevelsFromCache(cachePath);
            skipSaving = true;
        }

        if (!m_isCanceled && levels.empty() && service == QStringLiteral("avformat")) {
            // if the resource is a media file, we can use libav for speed
            const auto fps = producer->get_fps();
            levels = generateLibav(streamIdx, res, lengthInFrames, fps, clbk, m_isCanceled);
        }

        if (!m_isCanceled && levels.empty()) {
            // else, or if using libav failed, use MLT
            const int channels = binClip->audioInfo()->channelsForStream(streamIdx);
            levels = generateMLT(streamIdx, service, res, channels, clbk, m_isCanceled);
        }

        if (!m_isCanceled && !levels.empty()) {
            storeLevels(binClip, streamIdx, levels);
            storeMax(binClip, streamIdx, levels);
            if (!skipSaving) {
                saveLevelsToCache(cachePath, levels);
            }
            m_progress = 100;
            QMetaObject::invokeMethod(m_object, "updateJobProgress");
        }
    }
    QMetaObject::invokeMethod(m_object, "updateAudioThumbnail", Q_ARG(bool, true));
}
