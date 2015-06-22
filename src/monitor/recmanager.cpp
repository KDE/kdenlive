/***************************************************************************
 *   Copyright (C) 2015 by Jean-Baptiste Mardelle (jb@kdenlive.org)        *
 *   This file is part of Kdenlive. See www.kdenlive.org.                  *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or         *
 *   modify it under the terms of the GNU General Public License as        *
 *   published by the Free Software Foundation; either version 2 of        *
 *   the License or (at your option) version 3 or any later version        *
 *   accepted by the membership of KDE e.V. (or its successor approved     *
 *   by the membership of KDE e.V.), which shall act as a proxy            *
 *   defined in Section 14 of version 3 of the license.                    *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>. *
 ***************************************************************************/

#include "recmanager.h"
#include "monitor.h"
#include "kdenlivesettings.h"
#include "capture/mltdevicecapture.h"
#include "capture/managecapturesdialog.h"
#include "dialogs/profilesdialog.h"

#include <QDebug>
#include "klocalizedstring.h"

#include <QComboBox>
#include <QToolBar>
#include <QDesktopWidget>
#include <QStandardPaths>
#include <QIcon>
#include <QDir>
#include <QFile>

RecManager::RecManager(int iconSize, Monitor *parent) :
    QObject(parent)
    , m_monitor(parent)
    , m_captureProcess(NULL)
    , m_recToolbar(new QToolBar(parent))
    , m_screenCombo(NULL)
{
    m_recToolbar->setIconSize(QSize(iconSize, iconSize));
    m_playAction = m_recToolbar->addAction(QIcon::fromTheme("media-playback-start"), i18n("Preview"));
    m_playAction->setCheckable(true);
    connect(m_playAction, &QAction::toggled, this, &RecManager::slotPreview);

    m_recAction = m_recToolbar->addAction(QIcon::fromTheme("media-record"), i18n("Record"));
    m_recAction->setCheckable(true);
    connect(m_recAction, &QAction::toggled, this, &RecManager::slotRecord);

    // Check number of monitors for FFmpeg screen capture
    int screens = QApplication::desktop()->screenCount();
    if (screens > 1) {
        m_screenCombo = new QComboBox(parent);
        for (int ix = 0; ix < screens; ix++) {
            m_screenCombo->addItem(i18n("Monitor %1", ix));
        }
        m_recToolbar->addWidget(m_screenCombo);
        // Update screen grab monitor choice in case we changed from fullscreen
        m_screenCombo->setEnabled(KdenliveSettings::grab_capture_type() == 0);
    }
    QWidget *spacer = new QWidget(parent);
    spacer->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Preferred);
    m_recToolbar->addWidget(spacer);
    m_device_selector = new QComboBox(parent);
    m_device_selector->addItems(QStringList() << i18n("Firewire") << i18n("Webcam") << i18n("Screen Grab") << i18n("Blackmagic Decklink"));
    m_device_selector->setCurrentIndex(KdenliveSettings::defaultcapture());
    connect(m_device_selector, SIGNAL(currentIndexChanged(int)), this, SLOT(slotVideoDeviceChanged(int)));
    m_recToolbar->addWidget(m_device_selector);
    QAction *configureRec = m_recToolbar->addAction(QIcon::fromTheme("configure"), i18n("Configure Recording"));
    connect(configureRec, &QAction::triggered, this, &RecManager::showRecConfig);
    m_recToolbar->addSeparator();
    m_switchRec = m_recToolbar->addAction(QIcon::fromTheme("list-add"), i18n("Show Record Control"));
    m_switchRec->setCheckable(true);
    connect(m_switchRec, &QAction::toggled, m_monitor, &Monitor::slotSwitchRec);
    m_recToolbar->setVisible(false);
    slotVideoDeviceChanged(m_device_selector->currentIndex());
}

RecManager::~RecManager()
{
}

void RecManager::showRecConfig()
{
    qDebug()<<"+ + +SHOW CONFIG + + +";
    m_monitor->showConfigDialog(4, m_device_selector->currentIndex());
}

QToolBar *RecManager::toolbar() const
{
    return m_recToolbar;
}

QAction *RecManager::switchAction() const
{
    return m_switchRec;
}

void RecManager::stopCapture()
{
    if (m_captureProcess) {
        slotRecord(false);
    }
}


void RecManager::slotRecord(bool record)
{
    if (!record) {
        if (!m_captureProcess) return;
        m_captureProcess->terminate();
        QTimer::singleShot(1500, m_captureProcess, SLOT(kill()));
        return;
    }
    if (m_captureProcess) return;
    m_recError.clear();
    m_captureProcess = new QProcess;
    connect(m_captureProcess, &QProcess::stateChanged, this, &RecManager::slotProcessStatus);
    connect(m_captureProcess, &QProcess::readyReadStandardError, this, &RecManager::slotReadProcessInfo);

    QString extension = KdenliveSettings::grab_extension();
    QString capturePath = KdenliveSettings::capturefolder();
    QString path = QUrl(capturePath).path() + QDir::separator() + "capture0000." + extension;
    int i = 1;
    while (QFile::exists(path)) {
        QString num = QString::number(i).rightJustified(4, '0', false);
        path = QUrl(capturePath).path() + QDir::separator() + "capture" + num + '.' + extension;
        ++i;
    }
    m_captureFile = QUrl(path);
    QString args;
    QString captureSize;
    int screen = -1;
    if (m_screenCombo) {
        // Multi monitor setup, capture monitor selected by user
        screen = m_screenCombo->currentIndex();
    }
    QRect screenSize = QApplication::desktop()->screenGeometry(screen);
    QStringList captureArgs;
    captureArgs << "-f" << "x11grab";
    if (KdenliveSettings::grab_follow_mouse()) captureArgs << "-follow_mouse" << "centered";
    if (!KdenliveSettings::grab_hide_frame()) captureArgs << "-show_region" << "1";
    captureSize = ":0.0";
    if (KdenliveSettings::grab_capture_type() == 0) {
        // Full screen capture
        captureArgs << "-s" << QString::number(screenSize.width()) + 'x' + QString::number(screenSize.height());
        captureSize.append('+' + QString::number(screenSize.left()) + '.' + QString::number(screenSize.top()));
    } else {
        // Region capture
        captureArgs << "-s" << QString::number(KdenliveSettings::grab_width()) + 'x' + QString::number(KdenliveSettings::grab_height());
        captureSize.append('+' + QString::number(KdenliveSettings::grab_offsetx()) + '.' + QString::number(KdenliveSettings::grab_offsetx()));
    }
    // fps
    captureArgs << "-r" << QString::number(KdenliveSettings::grab_fps());
    if (KdenliveSettings::grab_hide_mouse()) captureSize.append("+nomouse");
    captureArgs << "-i" << captureSize;
    if (!KdenliveSettings::grab_parameters().simplified().isEmpty())
    captureArgs << KdenliveSettings::grab_parameters().simplified().split(' ');
    captureArgs << path;

    m_captureProcess->start(KdenliveSettings::ffmpegpath(), captureArgs);
    if (!m_captureProcess->waitForStarted()) {
        // Problem launching capture app
        emit warningMessage(i18n("Failed to start the capture application:\n%1", KdenliveSettings::ffmpegpath()));
        //delete m_captureProcess;
    }
}


void RecManager::slotProcessStatus(QProcess::ProcessState status)
{
    if (status == QProcess::NotRunning) {
        m_recAction->setEnabled(true);
        m_recAction->setChecked(false);
        m_device_selector->setEnabled(true);
        if (m_captureProcess) {
            if (m_captureProcess->exitStatus() == QProcess::CrashExit) {
                warningMessage(i18n("Capture crashed, please check your parameters"));
            } else {
                if (true) {
                    int code = m_captureProcess->exitCode();
                    if (code != 0 && code != 255) {
                        emit warningMessage(i18n("Capture crashed, please check your parameters"));
                    }
                    else {
                        // Capture successfull, add clip to project
                        emit addClipToProject(m_captureFile);
                    }
                }
            }
        }
        delete m_captureProcess;
        m_captureProcess = NULL;
    }
}

void RecManager::slotReadProcessInfo()
{
    QString data = m_captureProcess->readAllStandardError().simplified();
    m_recError.append(data + '\n');
}


void RecManager::slotVideoDeviceChanged(int ix)
{
    switch (ix) {
      case Video4Linux:
        m_playAction->setEnabled(true);
        break;
      case BlackMagic:
        m_playAction->setEnabled(false);
        break;
      default:
        m_playAction->setEnabled(false);
        break;
    }
    /*
    m_previewSettings->setEnabled(ix == Video4Linux || ix == BlackMagic);
    control_frame->setVisible(ix == Video4Linux);
    monitor_box->setVisible(ix == ScreenBag && monitor_box->count() > 0);
    m_playAction->setVisible(ix != ScreenBag);
    m_fwdAction->setVisible(ix == Firewire);
    m_discAction->setVisible(ix == Firewire);
    m_rewAction->setVisible(ix == Firewire);
    m_recAction->setEnabled(ix != Firewire);
    m_logger.setVisible(ix == BlackMagic);
    if (m_captureDevice) {
        // MLT capture still running, abort
        m_monitorManager->clearScopeSource();
        m_captureDevice->stop();
        delete m_captureDevice;
        m_captureDevice = NULL;
    }

    // The m_videoBox container has to be shown once before the MLT consumer is build, or preview will fail
    switch (ix) {
    case ScreenBag:
    }
    */
}

void RecManager::slotPreview(bool record)
{
    //TODO:
    if (m_device_selector->currentIndex() == Video4Linux) {
        QString path = QStandardPaths::writableLocation(QStandardPaths::DataLocation) + "/profiles/video4linux";
        MltVideoProfile profile = ProfilesDialog::getVideoProfile(path);
        QString playlist =QString("video4linux2:%1?width:%2&amp;height:%3&amp;frame_rate:%4").arg(KdenliveSettings::video4vdevice()).arg(profile.width).arg(profile.height).arg((double) profile.frame_rate_num / profile.frame_rate_den);
        m_monitor->updateClipProducer(playlist);
    }
      
      
      
     /*   
        buildMltDevice(path);
        
        bool isXml;
        producer = getV4lXmlPlaylist(profile, &isXml);

        //producer = QString("avformat-novalidate:video4linux2:%1?width:%2&height:%3&frame_rate:%4").arg(KdenliveSettings::video4vdevice()).arg(profile.width).arg(profile.height).arg((double) profile.frame_rate_num / profile.frame_rate_den);
        if (!m_captureDevice->slotStartPreview(producer, isXml)) {
            // v4l capture failed to start
            video_frame->setText(i18n("Failed to start Video4Linux,\ncheck your parameters..."));

        } else {
            m_playAction->setEnabled(false);
            m_stopAction->setEnabled(true);
            m_isPlaying = true;
        }
    }*/
}

