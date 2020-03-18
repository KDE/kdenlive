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

#include <QAudioBuffer>
#include <QAudioRecorder>
#include <QCamera>
#include <QMediaRecorder>
#include <QStringList>
#include <QUrl>
#include <QTimer>
#include <QMutex>
#include <memory>

class QAudioRecorder;
class QAudioProbe;

class MediaCapture : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVector<qreal> levels READ levels NOTIFY levelsChanged)
    Q_PROPERTY(int recordState READ recordState NOTIFY recordStateChanged)

public:
    MediaCapture(QObject *parent);
    ~MediaCapture() override;
    void recordAudio(int tid, bool /*record*/);
    void recordVideo(int tid, bool /*record*/);
    /** @brief Returns true if a recording is in progress **/
    bool isRecording() const;
    /** @brief Sets m_path to selected output location **/
    void setCaptureOutputLocation();
    /** @brief Returns m_path **/
    QUrl getCaptureOutputLocation();
    /** @brief Sets m_audioDevice to selected audio capture device **/
    void setAudioCaptureDevice();
    /** @brief Returns list of audio devices available for capture **/
    QStringList getAudioCaptureDevices();
    /** @brief Sets currentState to QMediaRecorder::State value and returns it **/
    int getState();
    int currentState;
    Q_INVOKABLE QVector<qreal> levels() const;
    Q_INVOKABLE int recordState() const;

public slots:
    void displayErrorMessage();
    /** @brief Sets m_volume to selected audio capture volume **/
    void setAudioVolume();

private:
    std::unique_ptr<QAudioRecorder> m_audioRecorder;
    std::unique_ptr<QMediaRecorder> m_videoRecorder;
    std::unique_ptr<QCamera> m_camera;
    std::unique_ptr<QAudioProbe> m_probe;
    QString m_audioDevice;
    QUrl m_path;
    QVector<qreal> m_levels;
    int m_recordState;
    QTimer m_resetTimer;
    QMutex m_recMutex;

private slots:
    void processBuffer(const QAudioBuffer &buffer);
    void resetIfUnused();

signals:
    void levelsChanged();
    void recordStateChanged(int tid, bool recording);
    void recordDone();
    void audioLevels(QVector<qreal> levels);
};

#endif
