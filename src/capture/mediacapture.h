/*
SPDX-FileCopyrightText: 2019 Akhil K Gangadharan <akhilam512@gmail.com>
This file is part of Kdenlive. See www.kdenlive.org.

SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include <QAudioBuffer>
#include <QAudioRecorder>
#include <QAudioInput>
#include <QCamera>
#include <QMediaRecorder>
#include <QIODevice>
#include <QStringList>
#include <QElapsedTimer>
#include <QUrl>
#include <QTimer>
#include <QMutex>
#include <memory>

class QAudioRecorder;
class QAudioProbe;


class AudioDevInfo: public QIODevice
{
    Q_OBJECT
public:
    AudioDevInfo(const QAudioFormat &format, QObject *parent = nullptr);
    quint32 maxAmplitude = 0;

signals:
    void levelChanged(const QVector<qreal> &dbLevels);
    void levelRecChanged(const QVector<qreal> &dbLevels);

protected:
    qint64 readData(char *data, qint64 maxSize) override;
    qint64 writeData(const char *data, qint64 maxSize) override;
private:
    const QAudioFormat m_format;
};

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
    void switchMonitorState(bool run);
    /** @brief Returns true is audio monitoring is currently in progress **/
    bool isMonitoring() const;
    const QVector<double> recLevels() const;
    /** @brief Start monitoring a track **/
    Q_INVOKABLE void switchMonitorState(int tid, bool run);
    void pauseRecording();
    void resumeRecording();

public slots:
    void displayErrorMessage();
    /** @brief Sets m_volume to selected audio capture volume **/
    void setAudioVolume();

private:
    std::unique_ptr<QAudioRecorder> m_audioRecorder;
    std::unique_ptr<QAudioInput> m_audioInput;
    QScopedPointer<AudioDevInfo> m_audioInfo;
    std::unique_ptr<QMediaRecorder> m_videoRecorder;
    std::unique_ptr<QCamera> m_camera;
    QElapsedTimer m_recTimer;
    QString m_audioDevice;
    QUrl m_path;
    QVector<qreal> m_levels;
    QVector<double> m_recLevels;
    int m_recordState;
    int m_lastPos;
    QTimer m_resetTimer;
    QMutex m_recMutex;

private slots:
    void resetIfUnused();

signals:
    void levelsChanged();
    void recordStateChanged(int tid, bool recording);
    void recordDone();
    void audioLevels(QVector<qreal> levels);
};
