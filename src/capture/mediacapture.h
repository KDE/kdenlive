/*
SPDX-FileCopyrightText: 2019 Akhil K Gangadharan <akhilam512@gmail.com>
This file is part of Kdenlive. See www.kdenlive.org.

SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include <QAudioBuffer>
#include <QAudioDevice>
#include <QAudioInput>
#include <QAudioSource>
#include <QCamera>
#include <QElapsedTimer>
#include <QIODevice>
#include <QMediaCaptureSession>
#include <QMediaDevices>
#include <QMediaFormat>
#include <QMediaRecorder>
#include <QMutex>
#include <QStringList>
#include <QTimer>
#include <QUrl>
#include <QUuid>
#include <memory>

class AudioDevInfo: public QIODevice
{
    Q_OBJECT
public:
    AudioDevInfo(const QAudioFormat &format, QObject *parent = nullptr);

Q_SIGNALS:
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
    Q_PROPERTY(int recDuration READ recDuration NOTIFY recDurationChanged)

public:
    enum RECORDSTATUS { RecordReady, RecordBusy, RecordMonitoring, RecordRecording, RecordShowingCountDown };

    MediaCapture(QObject *parent);
    ~MediaCapture() override;
    void recordAudio(const QUuid &uuid, int tid, bool record);
    // TODO: fix video capture
    // void recordVideo(int tid, bool /*record*/);
    /** @brief Returns the audio record status **/
    MediaCapture::RECORDSTATUS recordStatus() const;
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
    Q_INVOKABLE int recDuration() const;
    void switchMonitorState(bool run);
    const QVector<double> recLevels() const;
    /** @brief Start monitoring a track **/
    Q_INVOKABLE void switchMonitorState(int tid, bool run);
    void pauseRecording();
    void resumeRecording();
    /** @brief Start the real audio capture **/
    int startCapture(bool showCountdown);
    void initializeAudioSetup();

public Q_SLOTS:
    void displayErrorMessage();
    /** @brief Sets m_volume to selected audio capture volume **/
    void setAudioVolume();

private:
    std::unique_ptr<QMediaCaptureSession> m_mediaCapture;
    std::unique_ptr<QMediaRecorder> m_mediaRecorder;
    std::unique_ptr<QAudioSource> m_audioSource{nullptr};
    std::unique_ptr<QAudioInput> m_audioInput;
    QScopedPointer<AudioDevInfo> m_audioInfo;
    QUuid m_recordingSequence;
    std::unique_ptr<QMediaRecorder> m_videoRecorder;
    std::unique_ptr<QCamera> m_camera;
    QElapsedTimer m_recTimer;
    QString m_audioDevice;
    QUrl m_path;
    QVector<qreal> m_levels;
    QVector<double> m_recLevels;
    RECORDSTATUS m_recordStatus{RecordReady};
    int m_recordState;
    /** @brief Last recorded frame */
    int m_lastPos;
    /** @brief Duration of pre-pause recording */
    int m_recOffset;
    int m_tid;
    /** @brief true if we started the record countdown */
    bool m_readyForRecord;
    QTimer m_resetTimer;
    QMutex m_recMutex;

private Q_SLOTS:
    void resetIfUnused();

Q_SIGNALS:
    void levelsChanged();
    void recordStateChanged(int tid, bool recording);
    void recordDone();
    void audioLevels(QVector<qreal> levels);
    void recDurationChanged();
};
