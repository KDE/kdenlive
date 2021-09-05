/*
SPDX-FileCopyrightText: 2019 Akhil K Gangadharan <akhilam512@gmail.com>
This file is part of Kdenlive. See www.kdenlive.org.

SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
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
