/***************************************************************************
 *   Copyright (C) 2007 by Jean-Baptiste Mardelle (jb@kdenlive.org)        *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA          *
 ***************************************************************************/


#include <QMouseEvent>
#include <QStylePainter>
#include <QMenu>
#include <QToolButton>
#include <QFile>
#include <QDir>

#include <KDebug>
#include <KLocale>
#include <KStandardDirs>
#include <KComboBox>

#include "gentime.h"
#include "kdenlivesettings.h"
#include "recmonitor.h"

RecMonitor::RecMonitor(QString name, QWidget *parent)
        : QWidget(parent), m_name(name), m_isActive(false), m_isCapturing(false), m_isPlaying(false) {
    ui.setupUi(this);

    ui.video_frame->setAttribute(Qt::WA_PaintOnScreen);
    ui.device_selector->setCurrentIndex(KdenliveSettings::defaultcapture());
    connect(ui.device_selector, SIGNAL(currentIndexChanged(int)), this, SLOT(slotVideoDeviceChanged(int)));



    QToolBar *toolbar = new QToolBar(name, this);
    QHBoxLayout *layout = new QHBoxLayout;

    m_playIcon = KIcon("media-playback-start");
    m_pauseIcon = KIcon("media-playback-pause");

    m_discAction = toolbar->addAction(KIcon("network-connect"), i18n("Connect"));
    connect(m_discAction, SIGNAL(triggered()), this, SLOT(slotDisconnect()));

    m_rewAction = toolbar->addAction(KIcon("media-seek-backward"), i18n("Rewind"));
    connect(m_rewAction, SIGNAL(triggered()), this, SLOT(slotRewind()));

    m_playAction = toolbar->addAction(m_playIcon, i18n("Play"));
    connect(m_playAction, SIGNAL(triggered()), this, SLOT(slotStartCapture()));

    m_stopAction = toolbar->addAction(KIcon("media-playback-stop"), i18n("Stop"));
    connect(m_stopAction, SIGNAL(triggered()), this, SLOT(slotStopCapture()));
    m_stopAction->setEnabled(false);
    m_fwdAction = toolbar->addAction(KIcon("media-seek-forward"), i18n("Forward"));
    connect(m_fwdAction, SIGNAL(triggered()), this, SLOT(slotForward()));

    m_recAction = toolbar->addAction(KIcon("media-record"), i18n("Record"));
    connect(m_recAction, SIGNAL(triggered()), this, SLOT(slotRecord()));
    m_recAction->setCheckable(true);

    layout->addWidget(toolbar);
    ui.control_frame_firewire->setLayout(layout);

    slotVideoDeviceChanged(ui.device_selector->currentIndex());
    displayProcess = new QProcess;
    captureProcess = new QProcess;

    connect(captureProcess, SIGNAL(stateChanged(QProcess::ProcessState)), this, SLOT(slotProcessStatus(QProcess::ProcessState)));

    QStringList env = QProcess::systemEnvironment();
    env << "SDL_WINDOWID=" + QString::number(ui.video_frame->winId());
    displayProcess->setEnvironment(env);

    if (KdenliveSettings::video4capture().isEmpty()) {
        QString captureCommand;
        if (!KdenliveSettings::video4adevice().isEmpty()) captureCommand = "-f " + KdenliveSettings::video4aformat() + " -i " + KdenliveSettings::video4adevice();

        captureCommand +=  " -f " + KdenliveSettings::video4vformat() + " -s " + KdenliveSettings::video4size() + " -r " + QString::number(KdenliveSettings::video4rate()) + " -i " + KdenliveSettings::video4vdevice() + " -f " + KdenliveSettings::video4vencoding();
        KdenliveSettings::setVideo4capture(captureCommand);
    }

    if (KdenliveSettings::video4playback().isEmpty()) {
        QString playbackCommand;
        playbackCommand =  "-f " + KdenliveSettings::video4vencoding();
        KdenliveSettings::setVideo4playback(playbackCommand);
    }
    kDebug() << "/////// BUILDING MONITOR, ID: " << ui.video_frame->winId();
}

QString RecMonitor::name() const {
    return m_name;
}

void RecMonitor::slotVideoDeviceChanged(int ix) {

    if (ix == 1) {
        m_discAction->setEnabled(false);
        m_rewAction->setEnabled(false);
        m_fwdAction->setEnabled(false);
        m_recAction->setEnabled(true);
        m_stopAction->setEnabled(false);
        m_playAction->setEnabled(true);
    } else {
        m_discAction->setEnabled(true);
        m_recAction->setEnabled(false);
        m_stopAction->setEnabled(false);
        m_playAction->setEnabled(false);
        m_rewAction->setEnabled(false);
        m_fwdAction->setEnabled(false);
    }
}

void RecMonitor::slotDisconnect() {
    if (captureProcess->state() == QProcess::NotRunning) {
        slotStartCapture(false);
        m_discAction->setIcon(KIcon("network-disconnect"));
        m_discAction->setText(i18n("Disonnect"));
        m_recAction->setEnabled(true);
        m_stopAction->setEnabled(true);
        m_playAction->setEnabled(true);
        m_rewAction->setEnabled(true);
        m_fwdAction->setEnabled(true);
    } else {
        captureProcess->write("q", 1);
        m_discAction->setIcon(KIcon("network-connect"));
        m_discAction->setText(i18n("Connect"));
        m_recAction->setEnabled(false);
        m_stopAction->setEnabled(false);
        m_playAction->setEnabled(false);
        m_rewAction->setEnabled(false);
        m_fwdAction->setEnabled(false);
    }
}

void RecMonitor::slotRewind() {
    captureProcess->write("a", 1);
}

void RecMonitor::slotForward() {
    captureProcess->write("z", 1);
}

void RecMonitor::slotStopCapture() {
    // stop capture
    if (ui.device_selector->currentIndex() == 0) {
        captureProcess->write("\e", 2);
        m_playAction->setIcon(m_playIcon);
        m_isPlaying = false;
    } else {
        if (m_isCapturing && ui.autoaddbox->isChecked()) emit addProjectClip(m_captureFile);
        captureProcess->kill();
        displayProcess->kill();
    }
}

void RecMonitor::slotStartCapture(bool play) {

    /*
    *captureProcess<<"dvgrab";

    bool isHdv = false;

    switch (m_recPanel->capture_format->currentItem()){
        case 0:
      *captureProcess<<"--format"<<"dv1";
     break;
        case 1:
      *captureProcess<<"--format"<<"dv2";
     break;
        case 3:
      *captureProcess<<"--format"<<"hdv";
     isHdv = true;
     break;
        default:
            *captureProcess<<"--format"<<"raw";
     break;
    }

    if (KdenliveSettings::autosplit()) *captureProcess<<"--autosplit";
    if (KdenliveSettings::timestamp()) *captureProcess<<"--timestamp";
    *captureProcess<<"-i"<<"capture"<<"-";*/

    /*
        QStringList captureArgs;
        captureArgs<<"--format"<<"hdv"<<"-i"<<"capture"<<"-";
        QStringList displayArgs;

        displayArgs<<"-f"<<"mpegts"<<"-x"<<QString::number(ui.video_frame->width())<<"-y"<<QString::number(ui.video_frame->height())<<"-";

        captureProcess->setStandardOutputProcess(displayProcess);
        ui.video_frame->setScaledContents(false);
        captureProcess->start("dvgrab",captureArgs);
        displayProcess->start("ffplay",  displayArgs);*/


    if (captureProcess->state() != QProcess::NotRunning) {
        if (ui.device_selector->currentIndex() == 0) {
            if (m_isPlaying) {
                captureProcess->write("k", 1);
                //captureProcess->write("\e", 2);
                m_playAction->setIcon(m_playIcon);
                m_isPlaying = false;
            } else {
                captureProcess->write("p", 1);
                m_playAction->setIcon(m_pauseIcon);
                m_isPlaying = true;
            }
        }
        return;
    }
    m_captureArgs.clear();
    m_displayArgs.clear();
    m_isPlaying = false;

    if (ui.device_selector->currentIndex() == 0) {
        m_captureArgs << "--format" << "hdv" << "-i" << "capture" << "-";
        m_displayArgs << "-f" << "mpegts" << "-x" << QString::number(ui.video_frame->width()) << "-y" << QString::number(ui.video_frame->height()) << "-";
    } else {
        m_captureArgs << KdenliveSettings::video4capture().simplified().split(' ') << "-";
        m_displayArgs << KdenliveSettings::video4playback().simplified().split(' ') << "-x" << QString::number(ui.video_frame->width()) << "-y" << QString::number(ui.video_frame->height()) << "-";
    }

    captureProcess->setStandardOutputProcess(displayProcess);
    if (ui.device_selector->currentIndex() == 0) {
        captureProcess->setWorkingDirectory(KdenliveSettings::capturefolder());
        captureProcess->start("dvgrab", m_captureArgs);
        if (play) captureProcess->write(" ", 1);
        m_discAction->setEnabled(true);
    } else captureProcess->start("ffmpeg", m_captureArgs);
    displayProcess->start("ffplay", m_displayArgs);
    ui.video_frame->setText(i18n("Initialising..."));

}

void RecMonitor::slotRecord() {
    if (captureProcess->state() == QProcess::NotRunning && ui.device_selector->currentIndex() == 0) {
        slotStartCapture();
    }
    if (m_isCapturing) {
        if (ui.device_selector->currentIndex() == 0) {
            captureProcess->write("\e", 2);
            m_playAction->setIcon(m_playIcon);
            m_isCapturing = false;
            m_isPlaying = false;
            m_recAction->setChecked(false);
        } else {
            slotStopCapture();
            QTimer::singleShot(1000, this, SLOT(slotStartCapture()));
        }
        return;
    } else if (ui.device_selector->currentIndex() == 0) {
        m_isCapturing = true;
        captureProcess->write("c\n", 3);
        return;
    }
    if (captureProcess->state() == QProcess::NotRunning) {
        m_recAction->setChecked(true);

        QString path = KdenliveSettings::capturefolder() + "/capture0000.mpg";
        int i = 1;
        while (QFile::exists(path)) {
            QString num = QString::number(i).rightJustified(4, '0', false);
            path = KdenliveSettings::capturefolder() + "/capture" + num + ".mpg";
            i++;
        }

        m_captureFile = KUrl(path);

        m_isCapturing = true;
        m_captureArgs.clear();
        m_displayArgs.clear();

        if (ui.device_selector->currentIndex() == 0) {
            m_captureArgs << "--format" << "hdv" << "-i" << "capture" << "-";
            m_displayArgs << "-f" << "mpegts" << "-x" << QString::number(ui.video_frame->width()) << "-y" << QString::number(ui.video_frame->height()) << "-";
        } else {
            m_captureArgs << KdenliveSettings::video4capture().simplified().split(' ') << "-y" << m_captureFile.path() << "-f" << KdenliveSettings::video4vencoding() << "-";
            m_displayArgs << KdenliveSettings::video4playback().simplified().split(' ') << "-x" << QString::number(ui.video_frame->width()) << "-y" << QString::number(ui.video_frame->height()) << "-";
        }

        captureProcess->setStandardOutputProcess(displayProcess);
        //ui.video_frame->setScaledContents(false);
        if (ui.device_selector->currentIndex() == 0) captureProcess->start("dvgrab", m_captureArgs);
        else captureProcess->start("ffmpeg", m_captureArgs);
        displayProcess->start("ffplay", m_displayArgs);
        ui.video_frame->setText(i18n("Initialising..."));
    } else {
        // stop capture
        displayProcess->kill();
        captureProcess->kill();
        QTimer::singleShot(1000, this, SLOT(slotRecord()));
    }
}

void RecMonitor::slotProcessStatus(QProcess::ProcessState status) {
    if (status == QProcess::NotRunning) {
        m_isCapturing = false;
        m_isPlaying = false;
        m_playAction->setIcon(m_playIcon);
        m_recAction->setChecked(false);
        m_stopAction->setEnabled(false);
        ui.device_selector->setEnabled(true);
        ui.video_frame->setText(i18n("Not connected"));
    } else {
        m_stopAction->setEnabled(true);
        ui.device_selector->setEnabled(false);
    }
}

// virtual
void RecMonitor::mousePressEvent(QMouseEvent * event) {
    slotPlay();
}

void RecMonitor::activateRecMonitor() {
    //if (!m_isActive) m_monitorManager->activateRecMonitor(m_name);
}

void RecMonitor::stop() {
    m_isActive = false;

}

void RecMonitor::start() {
    m_isActive = true;

}

void RecMonitor::refreshRecMonitor(bool visible) {
    if (visible) {
        //if (!m_isActive) m_monitorManager->activateRecMonitor(m_name);

    }
}

void RecMonitor::slotPlay() {

    //if (!m_isActive) m_monitorManager->activateRecMonitor(m_name);

}


#include "recmonitor.moc"
