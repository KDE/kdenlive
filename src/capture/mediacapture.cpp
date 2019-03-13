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

MediaCapture::MediaCapture(QObject* parent)
    : QObject(parent)
{
}

MediaCapture::~MediaCapture()
{
}

void MediaCapture::displayErrorMessage()
{
    qDebug() << " !!!!!!!!!!!!!!!! ERROR : QMediarecorder - Capture failed";
}

void MediaCapture::recordAudio(bool record)
{
    if (!m_audioRecorder) {
        m_audioRecorder.reset(new QAudioRecorder(this));
    }

    if (record && m_audioRecorder->state() == QMediaRecorder::StoppedState) {
        m_audioRecorder->setAudioInput(m_audioDevice);
        m_audioRecorder->setVolume(m_volume);
        m_audioRecorder->setOutputLocation(m_path);
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
        foreach (const QCameraInfo& cameraInfo, availableCameras) {
            if (cameraInfo == QCameraInfo::defaultCamera()) {
                m_camera.reset(new QCamera(cameraInfo, this));
                break;
            }
        }
        m_videoRecorder.reset(new QMediaRecorder(m_camera.get(), this));
    }

    if (record && m_videoRecorder->state() == QMediaRecorder::StoppedState) {
        connect(m_videoRecorder.get(), SIGNAL(error(QMediaRecorder::Error)), this, SLOT(displayErrorMessage()));

        m_videoRecorder->setOutputLocation(m_path);
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

void MediaCapture::setCaptureOutputLocation(QUrl path)
{
    m_path = path;
}

QStringList MediaCapture::getAudioCaptureDevices()
{
    m_audioRecorder.reset(new QAudioRecorder(this));
    QStringList audioDevices = m_audioRecorder->audioInputs();
    m_audioRecorder.reset();
    return audioDevices;
}

void MediaCapture::setAudioCaptureDevice(QString audioDevice)
{
    m_audioDevice = audioDevice;
}

void MediaCapture::setAudioVolume(qreal volume)
{
    m_volume = volume;
}

int MediaCapture::getState()
{
    if (m_audioRecorder.get() != nullptr) {
        return m_audioRecorder->state();
    } else if (m_videoRecorder.get() != nullptr) {
        return m_videoRecorder->state();
    }
    return -1;
}