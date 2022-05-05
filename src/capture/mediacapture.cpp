/*
SPDX-FileCopyrightText: 2019 Akhil K Gangadharan <helloimakhil@gmail.com>
This file is part of Kdenlive. See www.kdenlive.org.

SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "mediacapture.h"
#include "audiomixer/mixermanager.hpp"
#include "audiomixer/iecscale.h"
#include "core.h"
#include "kdenlivesettings.h"
#include <QAudioOutput>
#include <QIODevice>
#include <QtEndian>
#include <QAudioProbe>
#include <QCameraInfo>
#include <QDir>
#include <memory>
#include <utility>


AudioDevInfo::AudioDevInfo(const QAudioFormat &format, QObject *parent)
    : QIODevice(parent)
    , m_format(format)
{
    switch (m_format.sampleSize()) {
    case 8:
        switch (m_format.sampleType()) {
        case QAudioFormat::UnSignedInt:
            m_maxAmplitude = 255;
            break;
        case QAudioFormat::SignedInt:
            m_maxAmplitude = 127;
            break;
        default:
            break;
        }
        break;
    case 16:
        switch (m_format.sampleType()) {
        case QAudioFormat::UnSignedInt:
            m_maxAmplitude = 65535;
            break;
        case QAudioFormat::SignedInt:
            m_maxAmplitude = 32767;
            break;
        default:
            break;
        }
        break;

    case 32:
        switch (m_format.sampleType()) {
        case QAudioFormat::UnSignedInt:
            m_maxAmplitude = 0xffffffff;
            break;
        case QAudioFormat::SignedInt:
            m_maxAmplitude = 0x7fffffff;
            break;
        case QAudioFormat::Float:
            m_maxAmplitude = 0x7fffffff; // Kind of
        default:
            break;
        }
        break;

    default:
        break;
    }
}

qint64 AudioDevInfo::readData(char *data, qint64 maxSize)
{
    Q_UNUSED(data)
    Q_UNUSED(maxSize)
    return 0;
}

qint64 AudioDevInfo::writeData(const char *data, qint64 len)
{
    if (m_maxAmplitude) {
        Q_ASSERT(m_format.sampleSize() % 8 == 0);
        const int channelBytes = m_format.sampleSize() / 8;
        const int sampleBytes = m_format.channelCount() * channelBytes;
        Q_ASSERT(len % sampleBytes == 0);
        const int numSamples = len / sampleBytes;

        const unsigned char *ptr = reinterpret_cast<const unsigned char *>(data);
        QVector<quint32>levels;
        for (int j = 0; j < m_format.channelCount(); ++j) {
            levels << 0;
        }
        for (int i = 0; i < numSamples; ++i) {
            for (int j = 0; j < m_format.channelCount(); ++j) {
                quint32 value = 0;
                if (m_format.sampleSize() == 8 && m_format.sampleType() == QAudioFormat::UnSignedInt) {
                    value = *reinterpret_cast<const quint8*>(ptr);
                } else if (m_format.sampleSize() == 8 && m_format.sampleType() == QAudioFormat::SignedInt) {
                    value = qAbs(*reinterpret_cast<const qint8*>(ptr));
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
                    value = qAbs(*reinterpret_cast<const float*>(ptr) * 0x7fffffff); // assumes 0-1.0
                }

                levels[j] = qMax(value, levels.at(j));
                ptr += channelBytes;
            }
        }
        QVector<qreal> dbLevels;
        for (int j = 0; j < m_format.channelCount(); ++j) {
            qreal val = qMin(levels.at(j), m_maxAmplitude);
            val = 20. * log10(val / m_maxAmplitude);
            dbLevels << IEC_ScaleMax(val, 0);
        }
        emit levelChanged(dbLevels);
    }
    return len;
}

MediaCapture::MediaCapture(QObject *parent)
    : QObject(parent)
    , currentState(-1)
    , m_audioInput(nullptr)
    , m_audioInfo(nullptr)
    , m_probe(nullptr)
    , m_audioDevice("default:")
    , m_path(QUrl())
    , m_recordState(0)
{
    m_resetTimer.setInterval(5000);
    m_resetTimer.setSingleShot(true);
    connect(&m_resetTimer, &QTimer::timeout, this, &MediaCapture::resetIfUnused);
}

void MediaCapture::switchMonitorState(bool run)
{
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
        QObject::connect(m_audioInfo.data(), &AudioDevInfo::levelChanged, m_audioInput.get(), [&] (const QVector<qreal> &level) {
            m_levels = level;
            emit levelsChanged();
        });
        m_audioInfo->open(QIODevice::WriteOnly);
        m_audioInput->start(m_audioInfo.data());
    } else {
        if (m_audioInfo) {
            m_audioInfo->close();
            m_audioInfo.reset();
        }
        m_audioInput.reset();
    }
}

MediaCapture::~MediaCapture() = default;

void MediaCapture::displayErrorMessage()
{
    qDebug() << " !!!!!!!!!!!!!!!! ERROR : QMediarecorder - Capture failed";
}

void MediaCapture::resetIfUnused()
{
    QMutexLocker lk(&m_recMutex);
    qDebug()<<"// CLEARING REC MANAGER";
    if (m_audioRecorder && m_audioRecorder->state() == QMediaRecorder::StoppedState) {
        m_audioRecorder.reset();
    }
}

void MediaCapture::recordAudio(int tid, bool record)
{
    QMutexLocker lk(&m_recMutex);
    if (!m_audioRecorder) {
        m_audioRecorder = std::make_unique<QAudioRecorder>(this);
        if (!m_probe) {
            m_probe = std::make_unique<QAudioProbe>(this);
            connect(m_probe.get(), &QAudioProbe::audioBufferProbed, this, &MediaCapture::processBuffer);
        }
        m_probe->setSource(m_audioRecorder.get());
        connect(m_audioRecorder.get(), &QAudioRecorder::stateChanged, this, [&, tid] (QMediaRecorder::State state) {
            m_recordState = state;
            if (m_recordState == QMediaRecorder::StoppedState) {
                m_resetTimer.start();
                emit audioLevels(QVector <qreal>());
                emit pCore->finalizeRecording(getCaptureOutputLocation().toLocalFile());
            }
            emit recordStateChanged(tid, m_recordState == QMediaRecorder::RecordingState);
        });
    }

    if (record && m_audioRecorder->state() == QMediaRecorder::StoppedState) {
        setAudioCaptureDevice();
        m_audioRecorder->setAudioInput(m_audioDevice);
        setCaptureOutputLocation();
        m_audioRecorder->setVolume(KdenliveSettings::audiocapturevolume()/100.0);
        //qDebug()<<"START AREC: "<<m_path<<"\n; CODECS: "<<m_audioRecorder->supportedAudioCodecs();
        connect(m_audioRecorder.get(), static_cast<void (QAudioRecorder::*)(QMediaRecorder::Error)>(&QAudioRecorder::error),
                this, &MediaCapture::displayErrorMessage);

        QAudioEncoderSettings audioSettings;
        //audioSettings.setCodec("audio/x-flac");
        audioSettings.setSampleRate(KdenliveSettings::audiocapturesamplerate());
        audioSettings.setChannelCount(KdenliveSettings::audiocapturechannels());
        m_audioRecorder->setEncodingSettings(audioSettings);
        m_audioRecorder->setOutputLocation(m_path);
        m_audioRecorder->record();
    } else if (m_audioRecorder->state() != QMediaRecorder::PausedState) {
        m_audioRecorder->stop();
    } else {
        m_audioRecorder->record();
    }
}

void MediaCapture::recordVideo(int tid, bool record)
{
    // TO DO - fix video capture
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
        connect(m_videoRecorder.get(), static_cast<void (QMediaRecorder::*)(QMediaRecorder::Error)>(&QAudioRecorder::error),
                this, &MediaCapture::displayErrorMessage);
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
}

void MediaCapture::setCaptureOutputLocation()
{
    QDir captureFolder;
    if (KdenliveSettings::capturetoprojectfolder()) {
        captureFolder = QDir(pCore->getProjectFolderName());
    } else {
        captureFolder = QDir(KdenliveSettings::capturefolder());
    }
    QString extension;
    if (m_videoRecorder.get() != nullptr) {
        extension = QStringLiteral(".mpeg");
    } else if (m_audioRecorder.get() != nullptr) {
        //extension = QStringLiteral(".flac");
        extension = QStringLiteral(".wav");
    }

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
    std::unique_ptr<QAudioRecorder> audioRecorder = std::make_unique<QAudioRecorder>(this);
    QStringList audioDevices = audioRecorder->audioInputs();
    audioRecorder.reset();
    return audioDevices;
}

void MediaCapture::setAudioCaptureDevice()
{
    QString deviceName = KdenliveSettings::defaultaudiocapture();
    if(!deviceName.isNull()) {
        m_audioDevice = deviceName;
    }
}

void MediaCapture::setAudioVolume()
{
    if (m_audioRecorder) {
        m_audioRecorder->setVolume(KdenliveSettings::audiocapturevolume()/100.0);
    }
}

int MediaCapture::getState()
{
    if (m_audioRecorder != nullptr) {
        currentState = m_audioRecorder->state();
    }
    else if (m_videoRecorder != nullptr) {
        currentState = m_videoRecorder->state();
    }
    return currentState;
}

template <class T> QVector<qreal> getBufferLevels(const T *buffer, int frames, int channels)
{
    QVector<qreal> max_values;
    max_values.fill(0, channels);

    for (int i = 0; i < frames; ++i) {
        for (int j = 0; j < channels; ++j) {
            qreal value = qAbs(qreal(buffer[i * channels + j]));
            if (value > max_values.at(j)) {
                max_values.replace(j, value);
            }
        }
    }

    return max_values;
}

// This function returns the maximum possible sample value for a given audio format
qreal getPeakValue(const QAudioFormat &format)
{
    // Note: Only the most common sample formats are supported
    if (!format.isValid()) {
        return qreal(0);
    }

    if (format.codec() != "audio/pcm") {
        return qreal(0);
    }

    switch (format.sampleType()) {
    case QAudioFormat::Unknown:
        break;
    case QAudioFormat::Float:
        if (format.sampleSize() != 32) { // other sample formats are not supported
            return qreal(0);
        }
        return qreal(1.00003);
    case QAudioFormat::SignedInt:
        if (format.sampleSize() == 32) {
            return qreal(INT_MAX);
        }
        if (format.sampleSize() == 16) {
            return qreal(SHRT_MAX);
        }
        if (format.sampleSize() == 8) {
            return qreal(CHAR_MAX);
        }
        break;
    case QAudioFormat::UnSignedInt:
        if (format.sampleSize() == 32) {
            return qreal(UINT_MAX);
        }
        if (format.sampleSize() == 16) {
            return qreal(USHRT_MAX);
        }
        if (format.sampleSize() == 8) {
            return qreal(UCHAR_MAX);
        }
        break;
    }

    return qreal(0);
}

QVector<qreal> getBufferLevels(const QAudioBuffer &buffer)
{
    QVector<qreal> values;
    if (!buffer.format().isValid() || buffer.format().byteOrder() != QAudioFormat::LittleEndian) {
        return values;
    }

    if (buffer.format().codec() != "audio/pcm") {
        return values;
    }

    int channelCount = buffer.format().channelCount();
    values.fill(0, channelCount);
    qreal peak_value = getPeakValue(buffer.format());
    if (qFuzzyCompare(peak_value, qreal(0))) {
        return values;
    }

    switch (buffer.format().sampleType()) {
    case QAudioFormat::Unknown:
    case QAudioFormat::UnSignedInt:
        if (buffer.format().sampleSize() == 32) {
            values = getBufferLevels(buffer.constData<quint32>(), buffer.frameCount(), channelCount);
        }
        if (buffer.format().sampleSize() == 16) {
            values = getBufferLevels(buffer.constData<quint16>(), buffer.frameCount(), channelCount);
        }
        if (buffer.format().sampleSize() == 8) {
            values = getBufferLevels(buffer.constData<quint8>(), buffer.frameCount(), channelCount);
        }
        break;
    case QAudioFormat::Float:
        if (buffer.format().sampleSize() == 32) {
            values = getBufferLevels(buffer.constData<float>(), buffer.frameCount(), channelCount);
        }
        break;
    case QAudioFormat::SignedInt:
        if (buffer.format().sampleSize() == 32) {
            values = getBufferLevels(buffer.constData<qint32>(), buffer.frameCount(), channelCount);
        }
        if (buffer.format().sampleSize() == 16) {
            values = getBufferLevels(buffer.constData<qint16>(), buffer.frameCount(), channelCount);
        }
        if (buffer.format().sampleSize() == 8) {
            values = getBufferLevels(buffer.constData<qint8>(), buffer.frameCount(), channelCount);
        }
        break;
    }
    std::transform(values.begin(), values.end(), values.begin(),
                       [peak_value](double val) { return (20. * log10(val / peak_value)); });
    return values;
}

void MediaCapture::processBuffer(const QAudioBuffer &buffer)
{
    QVector <qreal> levels = getBufferLevels(buffer);
    emit audioLevels(levels);
    /*for (auto &l : m_levels) {
        l = IEC_ScaleMax(l, 0);
    }
    emit levelsChanged();*/
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
    if (m_audioRecorder && m_audioRecorder->state() == QMediaRecorder::RecordingState) {
        return true;
    }
    if (m_videoRecorder && m_videoRecorder->state() == QMediaRecorder::RecordingState) {
        return true;
    }
    return false;
}
