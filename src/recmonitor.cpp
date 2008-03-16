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

#include "gentime.h"
#include "kdenlivesettings.h"
#include "recmonitor.h"

RecMonitor::RecMonitor(QString name, QWidget *parent)
        : QWidget(parent), m_name(name), m_isActive(false), m_isCapturing(false) {
    ui.setupUi(this);

    ui.video_frame->setAttribute(Qt::WA_PaintOnScreen);
    connect(ui.buttonConnect, SIGNAL(clicked()), this, SLOT(slotSwitchCapture()));
    connect(ui.buttonRec, SIGNAL(clicked()), this, SLOT(slotCapture()));
    ui.buttonRec->setCheckable(true);

    displayProcess = new QProcess;
    captureProcess = new QProcess;

    connect(displayProcess, SIGNAL(stateChanged(QProcess::ProcessState)), this, SLOT(slotProcessStatus(QProcess::ProcessState)));

    QStringList env = QProcess::systemEnvironment();
    env << "SDL_WINDOWID=" + QString::number(ui.video_frame->winId());
    displayProcess->setEnvironment(env);

    kDebug() << "/////// BUILDING MONITOR, ID: " << ui.video_frame->winId();
}

QString RecMonitor::name() const {
    return m_name;
}

void RecMonitor::slotSwitchCapture() {

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
    ui.buttonRec->setChecked(false);
    m_isCapturing = false;
    if (captureProcess->state() == QProcess::Starting) return;
    else if (captureProcess->state() == QProcess::NotRunning) {
        m_captureArgs.clear();
        m_displayArgs.clear();

        if (KdenliveSettings::defaultcapture() == 0) {
            m_captureArgs << "--format" << "hdv" << "-i" << "capture" << "-";
            m_displayArgs << "-f" << "mpegts" << "-x" << QString::number(ui.video_frame->width()) << "-y" << QString::number(ui.video_frame->height()) << "-";
        } else {
            if (!KdenliveSettings::video4adevice().isEmpty()) m_captureArgs << "-f" << KdenliveSettings::video4aformat() << "-i" << KdenliveSettings::video4adevice();
            m_captureArgs << "-f" << "video4linux2" << "-s" << QString::number(KdenliveSettings::video4width()) + "x" + QString::number(KdenliveSettings::video4height()) << "-r" << QString::number(KdenliveSettings::video4rate()) << "-i" << KdenliveSettings::video4vdevice() << "-f" << KdenliveSettings::video4vformat() << "-";
            m_displayArgs << "-f" << KdenliveSettings::video4vformat() << "-x" << QString::number(ui.video_frame->width()) << "-y" << QString::number(ui.video_frame->height()) << "-";
        }

        captureProcess->setStandardOutputProcess(displayProcess);
        if (KdenliveSettings::defaultcapture() == 0) captureProcess->start("dvgrab", m_captureArgs);
        else captureProcess->start("ffmpeg", m_captureArgs);
        displayProcess->start("ffplay", m_displayArgs);
        ui.video_frame->setText(i18n("Initialising..."));
    } else {
        // stop capture
        displayProcess->kill();
        captureProcess->kill();
    }
}

void RecMonitor::slotCapture() {

    if (captureProcess->state() == QProcess::NotRunning) {
        ui.buttonConnect->setEnabled(false);

        QString path = KdenliveSettings::capturefolder() + "/capture0000.mpg";
        int i = 1;
        while (QFile::exists(path)) {
            QString num = QString::number(i);
            if (i < 10) num.prepend("000");
            else if (i < 100) num.prepend("00");
            else num.prepend("0");
            path = KdenliveSettings::capturefolder() + "/capture" + num + ".mpg";
	    i++;
        }

        m_captureFile = KUrl(path);

        m_isCapturing = true;
        m_captureArgs.clear();
        m_displayArgs.clear();

        if (KdenliveSettings::defaultcapture() == 0) {
            m_captureArgs << "--format" << "hdv" << "-i" << "capture" << "-";
            m_displayArgs << "-f" << "mpegts" << "-x" << QString::number(ui.video_frame->width()) << "-y" << QString::number(ui.video_frame->height()) << "-";
        } else {
            if (!KdenliveSettings::video4adevice().isEmpty()) m_captureArgs << "-f" << KdenliveSettings::video4aformat() << "-i" << KdenliveSettings::video4adevice();
            m_captureArgs << "-f" << "video4linux2" << "-s" << QString::number(KdenliveSettings::video4width()) + "x" + QString::number(KdenliveSettings::video4height()) << "-r" << QString::number(KdenliveSettings::video4rate()) << "-i" << KdenliveSettings::video4vdevice() << "-y" << "-f" << KdenliveSettings::video4vformat() << m_captureFile.path() << "-f" << KdenliveSettings::video4vformat() << "-";
            m_displayArgs << "-f" << KdenliveSettings::video4vformat() << "-x" << QString::number(ui.video_frame->width()) << "-y" << QString::number(ui.video_frame->height()) << "-";
        }

        captureProcess->setStandardOutputProcess(displayProcess);
        //ui.video_frame->setScaledContents(false);
        if (KdenliveSettings::defaultcapture() == 0) captureProcess->start("dvgrab", m_captureArgs);
        else captureProcess->start("ffmpeg", m_captureArgs);
        displayProcess->start("ffplay", m_displayArgs);
        ui.video_frame->setText(i18n("Initialising..."));
        ui.buttonRec->setChecked(true);
    } else {
        // stop capture
        displayProcess->kill();
        captureProcess->kill();
        if (m_isCapturing) {
            QTimer::singleShot(1000, this, SLOT(slotSwitchCapture()));
            ui.buttonRec->setChecked(false);
            ui.buttonConnect->setEnabled(true);
            if (ui.autoaddbox->isChecked()) emit addProjectClip(m_captureFile);
        } else {
            QTimer::singleShot(1000, this, SLOT(slotCapture()));
            ui.buttonRec->setChecked(true);
        }
    }
}

void RecMonitor::slotProcessStatus(QProcess::ProcessState status) {
    if (status == QProcess::Starting) ui.buttonConnect->setText(i18n("Starting..."));
    else if (status == QProcess::NotRunning) {
        ui.buttonConnect->setText(i18n("Connect"));
        ui.video_frame->setText(i18n("Not connected"));
    } else ui.buttonConnect->setText(i18n("Disconnect"));
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
