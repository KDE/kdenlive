/*
Copyright (C) 2019  Akhil K Gangadharan <akhilam512@gmail.com>
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

#ifndef MEDIACAPTURE_H
#define MEDIACAPTURE_H

#include <QMediaRecorder>
#include <QAudioRecorder>
#include <QAudioEncoderSettings>
#include <QVideoEncoderSettings>
#include <QCamera>
#include <QCameraInfo>
#include <QStringList>
#include <QAudioBuffer>
#include <QUrl>
#include <memory>

class QAudioRecorder;
class QAudioProbe;

class MediaCapture : public QObject 
{
    Q_OBJECT
    Q_PROPERTY(QVector<qreal> levels READ levels NOTIFY levelsChanged)

public:
    MediaCapture(QObject* parent);
    ~MediaCapture();
    void recordAudio(bool);
    void recordVideo(bool);
    /** @brief Sets m_path to selected output location **/
    void setCaptureOutputLocation(QUrl);
    /** @brief Sets m_device to selected audio capture device **/
    void setAudioCaptureDevice(QString);
    /** @brief Sets m_volume to selected audio capture volume **/
    void setAudioVolume(qreal);
    /** @brief Returns list of audio devices available for capture **/
    QStringList getAudioCaptureDevices();
    /** @brief Returns QMediaRecorder::State value **/
    int getState();
    Q_INVOKABLE QVector<qreal> levels() const;

public slots:
    void displayErrorMessage();

private:
    std::unique_ptr<QAudioRecorder> m_audioRecorder;
    std::unique_ptr<QMediaRecorder> m_videoRecorder;
    std::unique_ptr<QCamera> m_camera;
    std::unique_ptr<QAudioProbe> m_probe;
    QString m_audioDevice;
    qreal m_volume;
    QUrl m_path;
    QVector<qreal> m_levels;

private slots:
    void processBuffer(const QAudioBuffer& buffer);
    
signals:
    void levelsChanged();
};

#endif
