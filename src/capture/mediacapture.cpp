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
#include <QAudioOutput>
// TODO: fix video capture (Hint: QCameraInfo is not available in Qt6 anymore)
//#include <QCameraInfo>
#include <QDir>
#include <QtEndian>
#include <utility>

AudioDevInfo::AudioDevInfo(const QAudioFormat &format, QObject *parent)
    : QIODevice(parent)
    , m_format(format)
{
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    switch (m_format.sampleSize()) {
    case 8:
        switch (m_format.sampleType()) {
        case QAudioFormat::UnSignedInt:
            maxAmplitude = 255;
            break;
        case QAudioFormat::SignedInt:
            maxAmplitude = 127;
            break;
        default:
            break;
        }
        break;
    case 16:
        switch (m_format.sampleType()) {
        case QAudioFormat::UnSignedInt:
            maxAmplitude = 65535;
            break;
        case QAudioFormat::SignedInt:
            maxAmplitude = 32767;
            break;
        default:
            break;
        }
        break;

    case 32:
        switch (m_format.sampleType()) {
        case QAudioFormat::UnSignedInt:
            maxAmplitude = 0xffffffff;
            break;
        case QAudioFormat::SignedInt:
            maxAmplitude = 0x7fffffff;
            break;
        case QAudioFormat::Float:
            maxAmplitude = 0x7fffffff; // Kind of
        default:
            break;
        }
        break;

    default:
        break;
    }
#else
    // TODO: qt6
#endif
}

qint64 AudioDevInfo::readData(char *data, qint64 maxSize)
{
    Q_UNUSED(data)
    Q_UNUSED(maxSize)
    return 0;
}

qint64 AudioDevInfo::writeData(const char *data, qint64 len)
{
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    if (maxAmplitude) {
        Q_ASSERT(m_format.sampleSize() % 8 == 0);
        const int channelBytes = m_format.sampleSize() / 8;
        const int sampleBytes = m_format.channelCount() * channelBytes;
        Q_ASSERT(len % sampleBytes == 0);
        const int numSamples = len / sampleBytes;

        const unsigned char *ptr = reinterpret_cast<const unsigned char *>(data);
        QVector<quint32> levels;
        for (int j = 0; j < m_format.channelCount(); ++j) {
            levels << 0;
        }
        for (int i = 0; i < numSamples; ++i) {
            for (int j = 0; j < m_format.channelCount(); ++j) {
                quint32 value = 0;
                if (m_format.sampleSize() == 8 && m_format.sampleType() == QAudioFormat::UnSignedInt) {
                    value = *reinterpret_cast<const quint8 *>(ptr);
                } else if (m_format.sampleSize() == 8 && m_format.sampleType() == QAudioFormat::SignedInt) {
                    value = qAbs(*reinterpret_cast<const qint8 *>(ptr));
                } else if (m_format.sampleSize() == 16 && m_format.sampleType() == QAudioFormat::UnSignedInt) {
                    if (m_format.byteOrder() == QAudioFormat::LittleEndian)
                        value = qFromLittleEndian<quint16>(ptr);
                    else
                        value = qFromBigEndian<quint16>(ptr);
                } else if (m_format.sampleSize() == 16 && m_format.sampleType() == QAudioFormat::SignedInt) {
                    if (m_format.byteOrder() == QAudioFormat::LittleEndian)
                        value = qAbs(qFromLittleEndian<qint16>(ptr));
                    else
                        value = qAbs(qFromBigEndian<qint16>(ptr));
                } else if (m_format.sampleSize() == 32 && m_format.sampleType() == QAudioFormat::UnSignedInt) {
                    if (m_format.byteOrder() == QAudioFormat::LittleEndian)
                        value = qFromLittleEndian<quint32>(ptr);
                    else
                        value = qFromBigEndian<quint32>(ptr);
                } else if (m_format.sampleSize() == 32 && m_format.sampleType() == QAudioFormat::SignedInt) {
                    if (m_format.byteOrder() == QAudioFormat::LittleEndian)
                        value = qAbs(qFromLittleEndian<qint32>(ptr));
                    else
                        value = qAbs(qFromBigEndian<qint32>(ptr));
                } else if (m_format.sampleSize() == 32 && m_format.sampleType() == QAudioFormat::Float) {
                    value = qAbs(*reinterpret_cast<const float *>(ptr) * 0x7fffffff); // assumes 0-1.0
                }
                levels[j] = qMax(value, levels.at(j));
                ptr += channelBytes;
            }
        }
        QVector<qreal> dbLevels;
        QVector<qreal> recLevels;
        for (int j = 0; j < m_format.channelCount(); ++j) {
            qreal val = qMin(levels.at(j), maxAmplitude);
            val = 20. * log10(val / maxAmplitude);
            recLevels << val;
            dbLevels << IEC_ScaleMax(val, 0);
        }
        Q_EMIT levelRecChanged(recLevels);
        Q_EMIT levelChanged(dbLevels);
    }
#else
    // TODO: qt6
#endif
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

void MediaCapture::switchMonitorState(bool run)
{
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    if (run) {
        // Start monitoring audio
        QAudioFormat format;
        format.setSampleRate(KdenliveSettings::audiocapturesamplerate());
        format.setChannelCount(KdenliveSettings::audiocapturechannels());
        format.setSampleSize(16);
        format.setSampleType(QAudioFormat::SignedInt);
        format.setByteOrder(QAudioFormat::LittleEndian);
        format.setCodec("audio/pcm");
        QAudioDeviceInfo deviceInfo = QAudioDeviceInfo::defaultInputDevice();
        if (!m_audioDevice.isEmpty()) {
            const auto deviceInfos = QAudioDeviceInfo::availableDevices(QAudio::AudioInput);
            for (const QAudioDeviceInfo &devInfo : deviceInfos) {
                qDebug() << "Device name: " << devInfo.deviceName();
                if (devInfo.deviceName() == m_audioDevice) {
                    deviceInfo = devInfo;
                    break;
                }
            }
        }
        if (!deviceInfo.isFormatSupported(format)) {
            qWarning() << "Default format not supported - trying to use nearest";
            format = deviceInfo.nearestFormat(format);
        }
        m_audioInfo.reset(new AudioDevInfo(format));
        m_audioInput.reset();
        m_audioInput = std::make_unique<QAudioInput>(deviceInfo, format, this);
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
        qreal linearVolume = QAudio::convertVolume(KdenliveSettings::audiocapturevolume() / 100.0, QAudio::LogarithmicVolumeScale, QAudio::LinearVolumeScale);
        m_audioInput->setVolume(linearVolume);
        m_audioInfo->open(QIODevice::WriteOnly);
        m_audioInput->start(m_audioInfo.data());
    } else {
        if (m_audioInfo) {
            m_audioInfo->close();
            m_audioInfo.reset();
        }
        m_audioInput.reset();
    }
#else
    // TODO: qt6
#endif
}

int MediaCapture::recDuration() const
{
    return m_recOffset + m_lastPos;
}

const QVector<double> MediaCapture::recLevels() const
{
    return m_recLevels;
}

bool MediaCapture::isMonitoring() const
{
    return m_audioInput != nullptr && !isRecording();
}

MediaCapture::~MediaCapture() = default;

void MediaCapture::displayErrorMessage()
{
    qDebug() << " !!!!!!!!!!!!!!!! ERROR : QMediarecorder - Capture failed";
}

void MediaCapture::resetIfUnused()
{
    QMutexLocker lk(&m_recMutex);
    qDebug() << "// CLEARING REC MANAGER";
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    if (m_audioRecorder && m_audioRecorder->state() == QMediaRecorder::StoppedState) {
        m_audioRecorder.reset();
    }
#else
    // TODO: Qt6
    /*if (m_audioRecorder && m_audioRecorder->state() == QMediaRecorder::StoppedState) {
        m_audioRecorder.reset();
    }*/
#endif
}

void MediaCapture::recordAudio(int tid, bool record)
{
    QMutexLocker lk(&m_recMutex);
    m_tid = tid;
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    if (!m_audioRecorder) {
        m_audioRecorder = std::make_unique<QAudioRecorder>(this);
        connect(m_audioRecorder.get(), &QAudioRecorder::stateChanged, this, [&, tid](QMediaRecorder::State state) {
            m_recordState = state;
            if (m_recordState == QMediaRecorder::StoppedState) {
                m_resetTimer.start();
                m_recLevels.clear();
                m_lastPos = -1;
                m_recOffset = 0;
                Q_EMIT audioLevels(QVector<qreal>());
                // m_readyForRecord is true if we were only displaying the countdown but real recording didn't start yet
                if (!m_readyForRecord) {
                    Q_EMIT pCore->finalizeRecording(getCaptureOutputLocation().toLocalFile());
                }
                m_readyForRecord = false;
            }
            Q_EMIT recordStateChanged(tid, m_recordState == QMediaRecorder::RecordingState);
        });
    }

    if (record && m_audioRecorder->state() == QMediaRecorder::StoppedState) {
        m_recTimer.invalidate();
        m_resetTimer.stop();
        m_readyForRecord = true;
        setAudioCaptureDevice();
        m_audioRecorder->setAudioInput(m_audioDevice);
        setCaptureOutputLocation();
        qreal linearVolume = QAudio::convertVolume(KdenliveSettings::audiocapturevolume() / 100.0, QAudio::LogarithmicVolumeScale, QAudio::LinearVolumeScale);
        m_audioRecorder->setVolume(linearVolume);
        // qDebug()<<"START AREC: "<<m_path<<"\n; CODECS: "<<m_audioRecorder->supportedAudioCodecs();
        connect(m_audioRecorder.get(), static_cast<void (QAudioRecorder::*)(QMediaRecorder::Error)>(&QAudioRecorder::error), this,
                &MediaCapture::displayErrorMessage);

        QAudioEncoderSettings audioSettings;
        // audioSettings.setCodec("audio/x-flac");
        audioSettings.setSampleRate(KdenliveSettings::audiocapturesamplerate());
        audioSettings.setChannelCount(KdenliveSettings::audiocapturechannels());
        m_audioRecorder->setEncodingSettings(audioSettings);
        m_audioRecorder->setOutputLocation(m_path);
        m_recLevels.clear();
    } else if (!record) {
        m_audioRecorder->stop();
        m_recTimer.invalidate();
    } else {
        qDebug() << "::: RESTARTING RECORD\n\nBBBBBB";
        m_lastPos = -1;
        m_recTimer.start();
        m_audioRecorder->record();
    }
#else
    // TODO: Qt6
    /*if (!m_audioRecorder) {
        m_audioRecorder = std::make_unique<QAudioRecorder>(this);
        connect(m_audioRecorder.get(), &QAudioRecorder::stateChanged, this, [&, tid](QMediaRecorder::State state) {
            m_recordState = state;
            if (m_recordState == QMediaRecorder::StoppedState) {
                m_resetTimer.start();
                m_recLevels.clear();
                m_lastPos = -1;
                m_recOffset = 0;
                Q_EMIT audioLevels(QVector<qreal>());
                // m_readyForRecord is true if we were only displaying the countdown but real recording didn't start yet
                if (!m_readyForRecord) {
                    Q_EMIT pCore->finalizeRecording(getCaptureOutputLocation().toLocalFile());
                }
                m_readyForRecord = false;
            }
            Q_EMIT recordStateChanged(tid, m_recordState == QMediaRecorder::RecordingState);
        });
    }

    if (record && m_audioRecorder->state() == QMediaRecorder::StoppedState) {
        m_recTimer.invalidate();
        m_resetTimer.stop();
        m_readyForRecord = true;
        setAudioCaptureDevice();
        m_audioRecorder->setAudioInput(m_audioDevice);
        setCaptureOutputLocation();
        qreal linearVolume = QAudio::convertVolume(KdenliveSettings::audiocapturevolume() / 100.0, QAudio::LogarithmicVolumeScale, QAudio::LinearVolumeScale);
        m_audioRecorder->setVolume(linearVolume);
        // qDebug()<<"START AREC: "<<m_path<<"\n; CODECS: "<<m_audioRecorder->supportedAudioCodecs();
        connect(m_audioRecorder.get(), static_cast<void (QAudioRecorder::*)(QMediaRecorder::Error)>(&QAudioRecorder::error), this,
                &MediaCapture::displayErrorMessage);

        QAudioEncoderSettings audioSettings;
        // audioSettings.setCodec("audio/x-flac");
        audioSettings.setSampleRate(KdenliveSettings::audiocapturesamplerate());
        audioSettings.setChannelCount(KdenliveSettings::audiocapturechannels());
        m_audioRecorder->setEncodingSettings(audioSettings);
        m_audioRecorder->setOutputLocation(m_path);
        m_recLevels.clear();
    } else if (!record) {
        m_audioRecorder->stop();
        m_recTimer.invalidate();
    } else {
        qDebug() << "::: RESTARTING RECORD\n\nBBBBBB";
        m_lastPos = -1;
        m_recTimer.start();
        m_audioRecorder->record();
    } */
#endif
}

int MediaCapture::startCapture()
{
    m_lastPos = -1;
    m_recOffset = 0;
    m_recTimer.start();
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    m_audioRecorder->record();
#else
    // TODO: Qt6
    //m_audioRecorder->record();
#endif
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
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
        connect(m_videoRecorder.get(), static_cast<void (QMediaRecorder::*)(QMediaRecorder::Error)>(&QAudioRecorder::error), this,
                &MediaCapture::displayErrorMessage);
#else
        // TODO: Qt6
        connect(m_videoRecorder.get(), static_cast<void (QMediaRecorder::*)(QMediaRecorder::Error)>(&QAudioRecorder::error), this,
                &MediaCapture::displayErrorMessage);
#endif
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
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    // TODO: Qt6
    } else if (m_audioRecorder.get() != nullptr) {
        // extension = QStringLiteral(".flac");
        extension = QStringLiteral(".wav");
    }
#else
    // TODO: Qt6
    } /*else if (m_audioRecorder.get() != nullptr) {
        // extension = QStringLiteral(".flac");
        extension = QStringLiteral(".wav");
    }*/
#endif
    QString path = captureFolder.absoluteFilePath("capture0000" + extension);
    int fileCount = 1;
    while (QFile::exists(path)) {
        QString num = QString::number(fileCount).rightJustified(4, '0', false);
        path = captureFolder.absoluteFilePath("capture" + num + extension);
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
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    std::unique_ptr<QAudioRecorder> audioRecorder = std::make_unique<QAudioRecorder>(this);
    QStringList audioDevices = audioRecorder->audioInputs();
    audioRecorder.reset();
    return audioDevices;
#else
    // TODO: Qt6
    /*std::unique_ptr<QAudioRecorder> audioRecorder = std::make_unique<QAudioRecorder>(this);
    QStringList audioDevices = audioRecorder->audioInputs();
    audioRecorder.reset();
    return audioDevices;*/
    return {};
#endif
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
    qreal linearVolume = QAudio::convertVolume(KdenliveSettings::audiocapturevolume() / 100.0, QAudio::LogarithmicVolumeScale, QAudio::LinearVolumeScale);
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    if (m_audioRecorder) {
        m_audioRecorder->setVolume(linearVolume);
    }
    if (m_audioInput) {
        m_audioInput->setVolume(linearVolume);
    }
#else
    // TODO: Qt6
    /*if (m_audioRecorder) {
        m_audioRecorder->setVolume(linearVolume);
    }
    if (m_audioInput) {
        m_audioInput->setVolume(linearVolume);
    }*/
#endif
}

int MediaCapture::getState()
{
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    if (m_audioRecorder != nullptr) {
        currentState = m_audioRecorder->state();
    } else if (m_videoRecorder != nullptr) {
        currentState = m_videoRecorder->state();
    }
#else
    // TODO: Qt6
    /*if (m_audioRecorder != nullptr) {
        currentState = m_audioRecorder->state();
    } else if (m_videoRecorder != nullptr) {
        currentState = m_videoRecorder->state();
    }*/
#endif
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

bool MediaCapture::isRecording() const
{
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    if (m_readyForRecord || (m_audioRecorder && m_audioRecorder->state() != QMediaRecorder::StoppedState)) {
        return true;
    }
    if (m_videoRecorder && m_videoRecorder->state() != QMediaRecorder::StoppedState) {
        return true;
    }
#else
    //TODO: Qt6
    /*if (m_readyForRecord || (m_audioRecorder && m_audioRecorder->state() != QMediaRecorder::StoppedState)) {
        return true;
    }
    if (m_videoRecorder && m_videoRecorder->state() != QMediaRecorder::StoppedState) {
        return true;
    }*/
#endif

    return false;
}

void MediaCapture::pauseRecording()
{
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    m_audioRecorder->pause();
    if (m_audioRecorder->state() == QMediaRecorder::RecordingState) {
        // Pause is not supported on this platform
        qDebug() << ":::: PAUSING FAILED!!!!";
        m_audioRecorder->stop();
    } else {
        qDebug() << ":::: MEDIA CAPTURE PAUSED!!!!";
    }
#else
    // TODO: Qt6
    /*m_audioRecorder->pause();
    if (m_audioRecorder->state() == QMediaRecorder::RecordingState) {
        // Pause is not supported on this platform
        qDebug() << ":::: PAUSING FAILED!!!!";
        m_audioRecorder->stop();
    } else {
        qDebug() << ":::: MEDIA CAPTURE PAUSED!!!!";
    }*/
#endif
}

void MediaCapture::resumeRecording()
{
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)

    if (m_audioRecorder->state() == QMediaRecorder::PausedState) {
        m_recOffset += m_lastPos;
        m_lastPos = -1;
        m_recTimer.start();
        m_audioRecorder->record();
    }
#else
    // TODO: Qt6
    /*if (m_audioRecorder->state() == QMediaRecorder::PausedState) {
        m_recOffset += m_lastPos;
        m_lastPos = -1;
        m_recTimer.start();
        m_audioRecorder->record();
    }*/
#endif
}
