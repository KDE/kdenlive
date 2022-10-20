/*
    SPDX-FileCopyrightText: 2013-2021 Meltytech LLC
    SPDX-FileCopyrightText: 2021 Jean-Baptiste Mardelle <jb@kdenlive.org>

    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "audiolevelstask.h"
#include "audio/audioStreamInfo.h"
#include "bin/projectclip.h"
#include "bin/projectitemmodel.h"
#include "core.h"

#include <KLocalizedString>
#include <KMessageWidget>
#include <QElapsedTimer>
#include <QFile>
#include <QImage>
#include <QList>
#include <QMutex>
#include <QRgb>
#include <QString>
#include <QThreadPool>
#include <QTime>
#include <QVariantList>

static QList<AudioLevelsTask *> tasksList;
static QMutex tasksListMutex;

static void deleteQVariantList(QVector<uint8_t> *list)
{
    delete list;
}

AudioLevelsTask::AudioLevelsTask(const ObjectId &owner, QObject *object)
    : AbstractTask(owner, AbstractTask::AUDIOTHUMBJOB, object)
{
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
    pCore->taskManager.startTask(owner.second, task);
}

void AudioLevelsTask::run()
{
    if (m_isCanceled || pCore->taskManager.isBlocked()) {
        pCore->taskManager.taskDone(m_owner.second, this);
        return;
    }
    QMutexLocker lock(&m_runMutex);
    m_running = true;
    // 2 channels interleaved of uchar values
    auto binClip = pCore->projectItemModel()->getClipByBinID(QString::number(m_owner.second));
    if (binClip == nullptr) {
        // Clip was deleted
        pCore->taskManager.taskDone(m_owner.second, this);
        return;
    }
    if (binClip->audioChannels() == 0 || binClip->audioThumbCreated()) {
        // nothing to do
        pCore->taskManager.taskDone(m_owner.second, this);
        return;
    }
    std::shared_ptr<Mlt::Producer> producer = binClip->originalProducer();
    if ((producer == nullptr) || !producer->is_valid()) {
        QMetaObject::invokeMethod(pCore.get(), "displayBinMessage", Qt::QueuedConnection,
                                  Q_ARG(QString, i18n("Audio thumbs: cannot open file %1", QFileInfo(binClip->url()).fileName())),
                                  Q_ARG(int, int(KMessageWidget::Warning)));
        pCore->taskManager.taskDone(m_owner.second, this);
        return;
    }
    int lengthInFrames = producer->get_length(); // Multiply this if we want more than 1 sample per frame
    if (lengthInFrames == INT_MAX || lengthInFrames == 0) {
        // This is a broken file or live feed, don't attempt to generate audio thumbnails
        QMetaObject::invokeMethod(pCore.get(), "displayBinMessage", Qt::QueuedConnection,
                                  Q_ARG(QString, i18n("Audio thumbs: unknown file length for %1", QFileInfo(binClip->url()).fileName())),
                                  Q_ARG(int, int(KMessageWidget::Warning)));
        pCore->taskManager.taskDone(m_owner.second, this);
        return;
    }
    int frequency = binClip->audioInfo()->samplingRate();
    frequency = frequency <= 0 ? 48000 : frequency;

    int channels = binClip->audioInfo()->channels();
    channels = channels <= 0 ? 2 : channels;

    QMap<int, QString> streams = binClip->audioInfo()->streams();
    QMap<int, int> audioChannels = binClip->audioInfo()->streamChannels();
    QMapIterator<int, QString> st(streams);
    bool audioCreated = false;
    while (st.hasNext() && !m_isCanceled) {
        st.next();
        int stream = st.key();
        if (audioChannels.contains(stream)) {
            channels = audioChannels.value(stream);
        }
        // Generate one thumb per stream
        QString cachePath = binClip->getAudioThumbPath(stream);
        QVector<uint8_t> mltLevels;
        if (!m_isForce && QFile::exists(cachePath)) {
            // Audio thumb already exists
            QImage image(cachePath);
            if (!m_isCanceled && !image.isNull()) {
                // convert cached image
                int n = image.width() * image.height();
                for (int i = 0; n > 1 && i < n; i++) {
                    QRgb p = image.pixel(i / channels, i % channels);
                    mltLevels << qRed(p);
                    mltLevels << qGreen(p);
                    mltLevels << qBlue(p);
                    mltLevels << qAlpha(p);
                }
                if (mltLevels.size() > 0) {
                    QVector<uint8_t> *levelsCopy = new QVector<uint8_t>(mltLevels);
                    producer->lock();
                    QString key = QString("_kdenlive:audio%1").arg(stream);
                    producer->set(key.toUtf8().constData(), levelsCopy, 0, (mlt_destructor)deleteQVariantList);
                    producer->unlock();
                    continue;
                }
            }
        }
        QString service = producer->get("mlt_service");
        if (service == QLatin1String("avformat-novalidate")) {
            service = QStringLiteral("avformat");
        } else if (service.startsWith(QLatin1String("xml"))) {
            service = QStringLiteral("xml-nogl");
        }
        QScopedPointer<Mlt::Producer> audioProducer(new Mlt::Producer(*producer->profile(), service.toUtf8().constData(), producer->get("resource")));
        if (!audioProducer->is_valid()) {
            QMetaObject::invokeMethod(pCore.get(), "displayBinMessage", Qt::QueuedConnection,
                                      Q_ARG(QString, i18n("Audio thumbs: cannot open file %1", producer->get("resource"))),
                                      Q_ARG(int, int(KMessageWidget::Warning)));
            pCore->taskManager.taskDone(m_owner.second, this);
            return;
        }
        audioProducer->set("video_index", "-1");
        audioProducer->set("audio_index", stream);
        Mlt::Filter chans(*producer->profile(), "audiochannels");
        Mlt::Filter converter(*producer->profile(), "audioconvert");
        Mlt::Filter levels(*producer->profile(), "audiolevel");
        audioProducer->attach(chans);
        audioProducer->attach(converter);
        audioProducer->attach(levels);

        double framesPerSecond = audioProducer->get_fps();
        mlt_audio_format audioFormat = mlt_audio_s16;
        QStringList keys;
        keys.reserve(channels);
        for (int i = 0; i < channels; i++) {
            keys << "meta.media.audio_level." + QString::number(i);
        }
        uint maxLevel = 1;
        QElapsedTimer updateTime;
        updateTime.start();
        for (int z = 0; z < lengthInFrames && !m_isCanceled; ++z) {
            int val = int(100.0 * z / lengthInFrames);
            if (m_progress != val) {
                m_progress = val;
                QMetaObject::invokeMethod(m_object, "updateJobProgress");
            }
            QScopedPointer<Mlt::Frame> mltFrame(audioProducer->get_frame());
            if ((mltFrame != nullptr) && mltFrame->is_valid() && (mltFrame->get_int("test_audio") == 0)) {
                int samples = mlt_audio_calculate_frame_samples(float(framesPerSecond), frequency, z);
                mltFrame->get_audio(audioFormat, frequency, channels, samples);
                for (int channel = 0; channel < channels; ++channel) {
                    uint lev = 256 * qMin(mltFrame->get_double(keys.at(channel).toUtf8().constData()) * 0.9, 1.0);
                    mltLevels << lev;
                    // double lev = mltFrame->get_double(keys.at(channel).toUtf8().constData());
                    // mltLevels << lev;
                    maxLevel = qMax(lev, maxLevel);
                }
            } else if (!mltLevels.isEmpty()) {
                for (int channel = 0; channel < channels; channel++) {
                    mltLevels << mltLevels.last();
                }
            }
            // Incrementally update the audio levels every 3 seconds.
            if (updateTime.elapsed() > 3000 && !m_isCanceled) {
                updateTime.restart();
                QVector<uint8_t> *levelsCopy = new QVector<uint8_t>(mltLevels);
                producer->lock();
                QString key = QString("_kdenlive:audio%1").arg(stream);
                producer->set(key.toUtf8().constData(), levelsCopy, 0, (mlt_destructor)deleteQVariantList);
                producer->unlock();
                QMetaObject::invokeMethod(m_object, "updateAudioThumbnail", Q_ARG(bool, false));
            }
        }

        /*// Normalize
        for (double &v : mltLevels) {
            m_audioLevels << uchar(255 * v / maxLevel);
        }*/
        if (m_isCanceled) {
            mltLevels.clear();
            m_progress = 100;
            QMetaObject::invokeMethod(m_object, "updateJobProgress");
        }
        if (mltLevels.size() > 0) {
            QVector<uint8_t> *levelsCopy = new QVector<uint8_t>(mltLevels);
            producer->lock();
            QString key = QString("_kdenlive:audio%1").arg(stream);
            QString key2 = QString("kdenlive:audio_max%1").arg(stream);
            producer->set(key2.toUtf8().constData(), int(maxLevel));
            producer->set(key.toUtf8().constData(), levelsCopy, 0, (mlt_destructor)deleteQVariantList);
            producer->unlock();
            // qDebug()<<"=== FINISHED PRODUCING AUDIO FOR: "<<key<<", SIZE: "<<levelsCopy->size();
            m_progress = 100;
            QMetaObject::invokeMethod(m_object, "updateJobProgress");
            // Put into an image for caching.
            int count = mltLevels.size();
            QImage image((count + 3) / 4 / channels, channels, QImage::Format_ARGB32);
            int n = image.width() * image.height();
            for (int i = 0; i < n; i++) {
                QRgb p;
                if ((4 * i + 3) < count) {
                    p = qRgba(mltLevels.at(4 * i), mltLevels.at(4 * i + 1), mltLevels.at(4 * i + 2), mltLevels.at(4 * i + 3));
                } else {
                    int last = mltLevels.last();
                    int r = (4 * i + 0) < count ? mltLevels.at(4 * i + 0) : last;
                    int g = (4 * i + 1) < count ? mltLevels.at(4 * i + 1) : last;
                    int b = (4 * i + 2) < count ? mltLevels.at(4 * i + 2) : last;
                    int a = last;
                    p = qRgba(r, g, b, a);
                }
                image.setPixel(i / channels, i % channels, p);
            }
            image.save(cachePath);
            audioCreated = true;
            QMetaObject::invokeMethod(m_object, "updateAudioThumbnail", Q_ARG(bool, false));
        }
    }
    if (!audioCreated && !m_isCanceled) {
        // Audio was cached, ensure the bin thumbnail is loaded
        QMetaObject::invokeMethod(m_object, "updateAudioThumbnail", Q_ARG(bool, true));
    }
    pCore->taskManager.taskDone(m_owner.second, this);
    QMetaObject::invokeMethod(m_object, "updateJobProgress");
}
