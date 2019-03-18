/*
Copyright (C) 2019  Akhil K Gangadharan <helloimakhil@gmail.com>
This file is part of Kdenlive. See www.kdenlive.org.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of
the License or (at your option) version 3 or any later version
accepted by the membership of KDE e.V. (or its successor approved
by the membership of KDE e.V.), which shall act as a proxy
defined in Section 14 of version 3 of the license.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "mediacapture.h"
#include "kdenlivesettings.h"
#include "core.h"
#include <QAudioProbe>
#include <QDir>
#include <memory>
#include <utility>

MediaCapture::MediaCapture(QObject *parent)
    : QObject(parent)
    , m_volume(1.)
    , currentState(-1)
    , m_audioDevice("default:")
    , m_path(QUrl())
{
    m_probe = std::make_unique<QAudioProbe>(this);
    connect(m_probe.get(), &QAudioProbe::audioBufferProbed, this, &MediaCapture::processBuffer);
}

MediaCapture::~MediaCapture() = default;

void MediaCapture::displayErrorMessage()
{
    qDebug() << " !!!!!!!!!!!!!!!! ERROR : QMediarecorder - Capture failed";
}

void MediaCapture::recordAudio(bool record)
{
    if (!m_audioRecorder) {
        m_audioRecorder = std::make_unique<QAudioRecorder>(this);
    }
    m_probe->setSource(m_audioRecorder.get());

    if (record && m_audioRecorder->state() == QMediaRecorder::StoppedState) {
        setAudioCaptureDevice();
        m_audioRecorder->setAudioInput(m_audioDevice);
        setCaptureOutputLocation();
        m_audioRecorder->setOutputLocation(m_path);
        setAudioVolume();
        m_audioRecorder->setVolume(m_volume);

        connect(m_audioRecorder.get(), SIGNAL(error(QMediaRecorder::Error)), this, SLOT(displayErrorMessage()));

        QAudioEncoderSettings audioSettings;
        audioSettings.setBitRate(48000);
        QString container = "audio/x-wav";
        m_audioRecorder->setEncodingSettings(audioSettings, QVideoEncoderSettings(), container);
        m_audioRecorder->record();
    } else if (m_audioRecorder->state() != QMediaRecorder::PausedState) {
        m_audioRecorder->stop();
        m_audioRecorder.reset();
    } else {
        m_audioRecorder->record();
    }
}

void MediaCapture::recordVideo(bool record)
{
    // TO DO - fix video capture
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
        connect(m_videoRecorder.get(), SIGNAL(error(QMediaRecorder::Error)), this, SLOT(displayErrorMessage()));
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
        extension = QStringLiteral("mpeg");
    } else if (m_audioRecorder.get() != nullptr) {
        extension = QStringLiteral("wav");
    }

    QString path = captureFolder.absoluteFilePath("capture0000." + extension);
    int fileCount = 1;
    while (QFile::exists(path)) {
        QString num = QString::number(fileCount).rightJustified(4, '0', false);
        path = captureFolder.absoluteFilePath("capture" + num + QLatin1Char('.') + extension);
        ++fileCount;
    }
    m_path = std::move(QUrl::fromLocalFile(path));
}

QUrl MediaCapture::getCaptureOutputLocation()
{
    return m_path;
}
QStringList MediaCapture::getAudioCaptureDevices()
{
    m_audioRecorder = std::make_unique<QAudioRecorder>(this);
    QStringList audioDevices = m_audioRecorder->audioInputs();
    m_audioRecorder.reset();
    return audioDevices;
}

void MediaCapture::setAudioCaptureDevice()
{
    QStringList audioDevices = getAudioCaptureDevices();
    int deviceIndex = KdenliveSettings::defaultaudiocapture();
    m_audioDevice = std::move(audioDevices[deviceIndex]);
}

void MediaCapture::setAudioVolume()
{
    m_volume = KdenliveSettings::audiocapturevolume()/100.0;
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
        for (double &value : values) {
            value = qAbs(value - peak_value / 2) / (peak_value / 2);
        }
        break;
    case QAudioFormat::Float:
        if (buffer.format().sampleSize() == 32) {
            values = getBufferLevels(buffer.constData<float>(), buffer.frameCount(), channelCount);
            for (double &value : values) {
                value /= peak_value;
            }
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
        for (double &value : values) {
            value /= peak_value;
        }
        break;
    }

    return values;
}

void MediaCapture::processBuffer(const QAudioBuffer &buffer)
{
    m_levels = getBufferLevels(buffer);
    emit levelsChanged();
}

QVector<qreal> MediaCapture::levels() const
{
    return m_levels;
}