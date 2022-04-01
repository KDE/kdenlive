/*
    SPDX-FileCopyrightText: 2015 Jean-Baptiste Mardelle <jb@kdenlive.org>
    This file is part of Kdenlive. See www.kdenlive.org.

    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include "definitions.h"

#include <QProcess>
#include <QUrl>

class Monitor;
class QAction;
class QToolBar;
class QComboBox;
class QCheckBox;
class QSlider;
class QToolButton;

namespace Mlt {
class Producer;
}

/** @class RecManager
    @brief All recording specific features are gathered here
    @author Jean-Baptiste Mardelle
 */
class RecManager : public QObject
{
    Q_OBJECT

    enum CaptureDevice {
        Video4Linux = 0,
        ScreenGrab = 1,
        // Not implemented
        Firewire = 2,
        BlackMagic = 3
    };

public:
    explicit RecManager(Monitor *parent = nullptr);
    ~RecManager() override;
    QToolBar *toolbar() const;
    void stopCapture();
    QAction *recAction() const;
    /** @brief: stop capture and hide rec panel **/
    void stop();

private:
    Monitor *m_monitor;
    QAction *m_switchRec;
    QString m_captureFolder;
    QUrl m_captureFile;
    QString m_recError;
    QProcess *m_captureProcess{nullptr};
    QAction *m_recAction;
    QAction *m_playAction;
    QAction *m_showLogAction;
    QToolBar *m_recToolbar;
    QToolButton *m_audioCaptureButton;
    QComboBox *m_device_selector;
    QComboBox *m_audio_device;
    QCheckBox *m_recVideo;
    QCheckBox *m_recAudio;
    QSlider *m_audioCaptureSlider;
    bool m_checkAudio{false};
    bool m_checkVideo{false};
    Mlt::Producer *createV4lProducer();
    int m_screenIndex{0};

private slots:
    void slotSetScreen(int ScreenIndex);
    void slotRecord(bool record);
    void slotPreview(bool record);
    void slotProcessStatus(int exitCode, QProcess::ExitStatus exitStatus);
    void slotReadProcessInfo();
    void showRecConfig();
    void slotVideoDeviceChanged(int ix = -1);
    void slotAudioDeviceChanged(int ix = -1);
    void slotShowLog();
    void slotSetVolume(int);
signals:
    void addClipToProject(const QUrl &);
    void warningMessage(const QString &, int timeout = 5000, const QList<QAction *> &actions = QList<QAction *>());
};
