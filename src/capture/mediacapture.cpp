/*
SPDX-FileCopyrightText: 2019 Akhil K Gangadharan <helloimakhil@gmail.com>
This file is part of Kdenlive. See www.kdenlive.org.

SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "mediacapture.h"
#include "audiomixer/iecscale.h"
#include "audiomixer/mixermanager.hpp"
#include "core.h"
#include "kdenlivesettings.h"
#include "monitor/monitor.h"
// TODO: fix video capture (Hint: QCameraInfo is not available in Qt6 anymore)
//#include <QCameraInfo>
#include <KLocalizedString>
#include <QDir>
#include <QtEndian>
#include <utility>

AudioDevInfo::AudioDevInfo(const QAudioFormat &format, QObject *parent)
    : QIODevice(parent)
    , m_format(format)
{
}

qint64 AudioDevInfo::readData(char *data, qint64 maxSize)
{
    Q_UNUSED(data)
    Q_UNUSED(maxSize)
    return 0;
}

qint64 AudioDevInfo::writeData(const char *data, qint64 len)
{
    Q_ASSERT(m_format.bytesPerSample() * 8 % 8 == 0);
    const int channelBytes = m_format.bytesPerSample();
    const int sampleBytes = m_format.channelCount() * channelBytes;
    Q_ASSERT(len % sampleBytes == 0);
    const int numSamples = len / sampleBytes;

    const unsigned char *ptr = reinterpret_cast<const unsigned char *>(data);
    QVector<double> levels;
    for (int j = 0; j < m_format.channelCount(); ++j) {
        levels << 0;
    }
    for (int i = 0; i < numSamples; ++i) {
        for (int j = 0; j < m_format.channelCount(); ++j) {
            float value = qAbs(m_format.normalizedSampleValue(ptr));
            levels[j] = qMax(value, levels.at(j));
            ptr += channelBytes;
        }
    }
    QVector<qreal> dbLevels;
    QVector<qreal> recLevels;
    for (int j = 0; j < m_format.channelCount(); ++j) {
        qreal val = 20. * log10(levels.at(j));
        recLevels << val;
        dbLevels << IEC_ScaleMax(val, 0);
    }
    Q_EMIT levelRecChanged(recLevels);
    Q_EMIT levelChanged(dbLevels);

    return len;
}

MediaCapture::MediaCapture(QObject *parent)
    : QObject(parent)
    , currentState(-1)
    , m_audioInput(nullptr)
    , m_audioInfo(nullptr)
    , m_audioDevice("default:")
    , m_path(QUrl())
    , m_recordState(0)
    , m_recOffset(0)
    , m_tid(-1)
    , m_readyForRecord(false)
{
    m_resetTimer.setInterval(5000);
    m_resetTimer.setSingleShot(true);
    connect(&m_resetTimer, &QTimer::timeout, this, &MediaCapture::resetIfUnused);
}

void MediaCapture::switchMonitorState(int tid, bool run)
{
    pCore->mixer()->monitorAudio(tid, run);
}

void MediaCapture::initializeAudioSetup()
{
    if (m_audioSource && m_audioInfo) {
        return;
    }
    setAudioCaptureDevice();
    QAudioDevice deviceInfo = QMediaDevices::defaultAudioInput();
    if (!m_audioDevice.isEmpty() && m_audioDevice != QLatin1String("default:")) {
        const auto deviceInfos = QMediaDevices::audioInputs();
        for (const QAudioDevice &devInfo : deviceInfos) {
            qDebug() << "Found device : " << devInfo.description();
            if (devInfo.description() == m_audioDevice) {
                deviceInfo = devInfo;
                qDebug() << "Monitoring using device : " << devInfo.description();
                break;
            }
        }
    }
    if (deviceInfo.isNull()) {
        return;
    }
    QAudioFormat format = deviceInfo.preferredFormat();
    format.setSampleRate(KdenliveSettings::audiocapturesamplerate());
    format.setChannelCount(KdenliveSettings::audiocapturechannels());
    format.setSampleFormat(static_cast<QAudioFormat::SampleFormat>(KdenliveSettings::audiocapturesampleformat()));
    if (!deviceInfo.isFormatSupported(format)) {
        qWarning() << "Default format not supported - trying to use preferred";
        format = deviceInfo.preferredFormat();
        pCore->displayMessage(i18n("Recording format adjusted to %1 channels, %2Hz.", format.channelCount(), format.sampleRate()), InformationMessage, 8000);
    }
    m_audioInfo.reset(new AudioDevInfo(format));
    m_audioInput.reset(new QAudioInput(deviceInfo, this));
    m_audioSource = std::make_unique<QAudioSource>(deviceInfo, format, this);
}

void MediaCapture::switchMonitorState(bool run)
{
    if (m_recordStatus == RecordBusy) {
        return;
    }
    m_recordStatus = RecordBusy;
    if (run) {
        // Start monitoring audio
        initializeAudioSetup();
        QObject::connect(m_audioInfo.data(), &AudioDevInfo::levelChanged, m_audioInput.get(), [&](const QVector<qreal> &level) {
            m_levels = level;
            if (m_recordState == QMediaRecorder::RecordingState) {
                // Get the frame number
                int currentPos = qRound(m_recTimer.elapsed() / 1000. * pCore->getCurrentFps());
                if (currentPos > m_lastPos) {
                    // Only store 1 value per frame
                    switch (level.count()) {
                    case 2:
                        for (int i = 0; i < currentPos - m_lastPos; i++) {
                            m_recLevels << qMax(level.first(), level.last());
                        }
                        break;
                    default:
                        for (int i = 0; i < currentPos - m_lastPos; i++) {
                            m_recLevels << level.first();
                        }
                        break;
                    }
                    m_lastPos = currentPos;
                    Q_EMIT recDurationChanged();
                }
            }
            Q_EMIT levelsChanged();
        });
        QObject::connect(m_audioInfo.data(), &AudioDevInfo::levelRecChanged, this, &MediaCapture::audioLevels);
#if QT_VERSION < QT_VERSION_CHECK(6, 7, 0)
        qreal linearVolume = QAudio::convertVolume(KdenliveSettings::audiocapturevolume() / 100.0, QAudio::LogarithmicVolumeScale, QAudio::LinearVolumeScale);
#else
        qreal linearVolume =
            QtAudio::convertVolume(KdenliveSettings::audiocapturevolume() / 100.0, QtAudio::LogarithmicVolumeScale, QtAudio::LinearVolumeScale);
#endif
        m_audioSource->setVolume(linearVolume);
        m_audioInfo->open(QIODevice::WriteOnly);
        m_audioSource->start(m_audioInfo.data());
        m_recordStatus = RecordMonitoring;
    } else {
        m_recordStatus = RecordReady;
        if (m_audioInfo) {
            m_audioInfo->close();
            m_audioInfo.reset();
        }
        m_audioInput.reset();
        m_audioSource->reset();
        m_audioSource.reset(nullptr);
    }
}

int MediaCapture::recDuration() const
{
    return m_recOffset + m_lastPos;
}

const QVector<double> MediaCapture::recLevels() const
{
    return m_recLevels;
}

MediaCapture::~MediaCapture()
{
    if (m_audioSource) {
        switchMonitorState(false);
    }
}

void MediaCapture::displayErrorMessage()
{
    qDebug() << " !!!!!!!!!!!!!!!! ERROR : QMediarecorder - Capture failed";
}

void MediaCapture::resetIfUnused()
{
    QMutexLocker lk(&m_recMutex);
    qDebug() << "// CLEARING REC MANAGER";
    if (m_mediaCapture && m_mediaRecorder->recorderState() == QMediaRecorder::StoppedState) {
        m_mediaCapture.reset();
    }
}

void MediaCapture::recordAudio(const QUuid &uuid, int tid, bool record)
{
    QMutexLocker lk(&m_recMutex);
    if (m_recordStatus == RecordBusy) {
        return;
    }
    m_recordStatus = RecordBusy;
    m_tid = tid;
    if (record) {
        m_recordingSequence = uuid;
        pCore->displayMessage(i18n("Monitoring audio. Press <b>Space</b> to start/pause recording, <b>Esc</b> to end."), InformationMessage, 8000);
    }
    if (!m_mediaRecorder) {
        m_mediaRecorder = std::make_unique<QMediaRecorder>(this);
        connect(m_mediaRecorder.get(), &QMediaRecorder::recorderStateChanged, this, [&, tid](QMediaRecorder::RecorderState state) {
            m_recordState = state;
            if (m_recordState == QMediaRecorder::StoppedState) {
                m_resetTimer.start();
                m_recLevels.clear();
                m_lastPos = -1;
                m_recOffset = 0;
                Q_EMIT audioLevels(QVector<qreal>());
                // m_readyForRecord is true if we were only displaying the countdown but real recording didn't start yet
                if (!m_readyForRecord) {
                    Q_EMIT pCore->finalizeRecording(m_recordingSequence, getCaptureOutputLocation().toLocalFile());
                }
                m_readyForRecord = false;
                m_recordStatus = RecordMonitoring;
            } else {
                m_recordStatus = RecordRecording;
            }
            Q_EMIT recordStateChanged(tid, m_recordState == QMediaRecorder::RecordingState);
        });
    }

    if (!m_mediaCapture) {
        m_mediaCapture = std::make_unique<QMediaCaptureSession>(this);
    }

    if (record && m_mediaRecorder->recorderState() == QMediaRecorder::StoppedState) {
        initializeAudioSetup();
        m_recTimer.invalidate();
        m_resetTimer.stop();
        m_readyForRecord = true;
        m_mediaCapture->setAudioInput(m_audioInput.get());
        m_mediaCapture->setRecorder(m_mediaRecorder.get());
        setCaptureOutputLocation();
#if QT_VERSION < QT_VERSION_CHECK(6, 7, 0)
        qreal linearVolume = QAudio::convertVolume(KdenliveSettings::audiocapturevolume() / 100.0, QAudio::LogarithmicVolumeScale, QAudio::LinearVolumeScale);
#else
        qreal linearVolume =
            QtAudio::convertVolume(KdenliveSettings::audiocapturevolume() / 100.0, QtAudio::LogarithmicVolumeScale, QtAudio::LinearVolumeScale);
#endif
        m_audioSource->setVolume(linearVolume);
        connect(m_mediaRecorder.get(), &QMediaRecorder::errorChanged, this, &MediaCapture::displayErrorMessage);

        // audioSettings.setCodec("audio/x-flac");
        int captureSampleRate = m_audioSource->format().sampleRate();
        if (captureSampleRate == 48000 || captureSampleRate == 44100) {
            m_mediaRecorder->setAudioSampleRate(captureSampleRate);
        } else {
            // Non standard sample rate, try to do our best
            m_mediaRecorder->setAudioSampleRate(-1);
        }
        int captureChannels = m_audioSource->format().channelCount();
        if (captureChannels <= 2) {
            m_mediaRecorder->setAudioChannelCount(captureChannels);
        } else {
            // Non standard channels count, try to do our best
            m_mediaRecorder->setAudioChannelCount(-1);
        }
        m_mediaRecorder->setOutputLocation(m_path);

        QMediaFormat mediaFormat(QMediaFormat::FileFormat::Wave);
        mediaFormat.setAudioCodec(QMediaFormat::AudioCodec::Wave);

        m_mediaRecorder->setMediaFormat(mediaFormat);
        m_recLevels.clear();
        m_recordStatus = RecordRecording;
    } else if (!record) {
        m_mediaRecorder->stop();
        m_recTimer.invalidate();
    } else {
        qDebug() << "::: RESTARTING RECORD\n\nBBBBBB";
        m_lastPos = -1;
        m_recTimer.start();
        m_mediaRecorder->record();
        m_recordStatus = RecordRecording;
    }
}

int MediaCapture::startCapture(bool showCountdown)
{
    if (showCountdown && !KdenliveSettings::disablereccountdown()) {
        pCore->getMonitor(Kdenlive::ProjectMonitor)->startCountDown();
        m_recordStatus = RecordShowingCountDown;
        return -1;
    }
    m_lastPos = -1;
    m_recOffset = 0;
    m_recTimer.start();
    m_mediaRecorder->record();
    m_readyForRecord = false;
    return m_tid;
}

// TODO: fix video capture

/*void MediaCapture::recordVideo(int tid, bool record)
{
    Q_UNUSED(tid)
    if (!m_videoRecorder) {
        QList<QCameraInfo> availableCameras = QCameraInfo::availableCameras();
        foreach (const QCameraInfo &cameraInfo, availableCameras) {
            if (cameraInfo == QCameraInfo::defaultCamera()) {
                m_camera = std::make_unique<QCamera>(cameraInfo, this);
                break;
            }
        }
        m_videoRecorder = std::make_unique<QMediaRecorder>(m_camera.get(), this);
    }

    if (record && m_videoRecorder->state() == QMediaRecorder::StoppedState) {
        setCaptureOutputLocation();
        m_videoRecorder->setOutputLocation(m_path);
        // TODO: Qt6
        connect(m_videoRecorder.get(), static_cast<void (QMediaRecorder::*)(QMediaRecorder::Error)>(&QAudioRecorder::error), this,
                &MediaCapture::displayErrorMessage);
        m_camera->setCaptureMode(QCamera::CaptureVideo);
        m_camera->start();
        // QString container = "video/mpeg";
        // By default, Qt chooses appropriate parameters
        m_videoRecorder->record();
    } else {
        m_videoRecorder->stop();
        m_camera->stop();
        m_videoRecorder.reset();
        m_camera.reset();
    }
}*/

void MediaCapture::setCaptureOutputLocation()
{
    QDir captureFolder = QDir(pCore->getProjectCaptureFolderName());

    if (!captureFolder.exists()) {
        // This returns false if it fails, but we'll just let the whole recording fail instead
        captureFolder.mkpath(".");
    }

    QString extension;
    if (m_videoRecorder.get() != nullptr) {
        extension = QStringLiteral(".mpeg");
    } else if (m_mediaRecorder != nullptr) {
        // extension = QStringLiteral(".flac");
        extension = QStringLiteral(".wav");
    }
    const QString baseName = i18nc("capture as a prefix to the filename of a recorded audio/video file", "capture");
    QString path = captureFolder.absoluteFilePath(baseName + QStringLiteral("-0000") + extension);
    int fileCount = 1;
    while (QFile::exists(path)) {
        QString num = QString::number(fileCount).rightJustified(4, '0', false);
        path = captureFolder.absoluteFilePath(QString("%1-%2%3").arg(baseName).arg(num).arg(extension));
        ++fileCount;
    }
    m_path = QUrl::fromLocalFile(path);
}

QUrl MediaCapture::getCaptureOutputLocation()
{
    return m_path;
}

QStringList MediaCapture::getAudioCaptureDevices()
{
    QStringList audioDevices = {};
    QList<QAudioDevice> audioInputs = QMediaDevices::audioInputs();
    std::transform(audioInputs.begin(), audioInputs.end(), std::back_inserter(audioDevices),
                   [](const QAudioDevice &audioDevice) -> QString { return audioDevice.description(); });
    return audioDevices;
}

void MediaCapture::setAudioCaptureDevice()
{
    QString deviceName = KdenliveSettings::defaultaudiocapture();
    if (!deviceName.isNull()) {
        m_audioDevice = deviceName;
    }
}

void MediaCapture::setAudioVolume()
{
#if QT_VERSION < QT_VERSION_CHECK(6, 7, 0)
    qreal linearVolume = QAudio::convertVolume(KdenliveSettings::audiocapturevolume() / 100.0, QAudio::LogarithmicVolumeScale, QAudio::LinearVolumeScale);
#else
    qreal linearVolume = QtAudio::convertVolume(KdenliveSettings::audiocapturevolume() / 100.0, QtAudio::LogarithmicVolumeScale, QtAudio::LinearVolumeScale);
#endif
    m_audioSource->setVolume(linearVolume);
}

int MediaCapture::getState()
{
    if (m_mediaCapture != nullptr) {
        currentState = m_mediaRecorder->recorderState();
    } /*} else if (m_videoRecorder != nullptr) {
        currentState = m_videoRecorder->state();
    }*/
    return currentState;
}

QVector<qreal> MediaCapture::levels() const
{
    return m_levels;
}

int MediaCapture::recordState() const
{
    return m_recordState;
}

MediaCapture::RECORDSTATUS MediaCapture::recordStatus() const
{
    return m_recordStatus;
}

void MediaCapture::pauseRecording()
{
    m_mediaRecorder->pause();
    if (m_mediaRecorder->recorderState() == QMediaRecorder::RecordingState) {
        // Pause is not supported on this platform
        qDebug() << ":::: PAUSING FAILED!!!!";
        m_mediaRecorder->stop();
    } else {
        qDebug() << ":::: MEDIA CAPTURE PAUSED!!!!";
    }
}

void MediaCapture::resumeRecording()
{
    if (m_mediaRecorder->recorderState() == QMediaRecorder::PausedState) {
        m_recOffset += m_lastPos;
        m_lastPos = -1;
        m_recTimer.start();
        m_mediaRecorder->record();
    }
}
