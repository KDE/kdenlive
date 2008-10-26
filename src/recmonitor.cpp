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
#include <QDesktopWidget>

#include <KDebug>
#include <KLocale>
#include <KStandardDirs>
#include <KComboBox>
#include <KIO/NetAccess>
#include <KFileItem>

#include "gentime.h"
#include "kdenlivesettings.h"
#include "managecapturesdialog.h"
#include "recmonitor.h"

RecMonitor::RecMonitor(QString name, QWidget *parent)
        : QWidget(parent), m_name(name), m_isActive(false), m_isCapturing(false), m_isPlaying(false), m_didCapture(false) {
    ui.setupUi(this);

    ui.video_frame->setAttribute(Qt::WA_PaintOnScreen);
    ui.device_selector->setCurrentIndex(KdenliveSettings::defaultcapture());
    connect(ui.device_selector, SIGNAL(currentIndexChanged(int)), this, SLOT(slotVideoDeviceChanged(int)));



    QToolBar *toolbar = new QToolBar(name, this);
    QHBoxLayout *layout = new QHBoxLayout;
    layout->setContentsMargins(0, 0, 0, 0);
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

    toolbar->addSeparator();

    QAction *configAction = toolbar->addAction(KIcon("configure"), i18n("Configure"));
    connect(configAction, SIGNAL(triggered()), this, SLOT(slotConfigure()));
    configAction->setCheckable(false);

    layout->addWidget(toolbar);
    ui.control_frame_firewire->setLayout(layout);

    slotVideoDeviceChanged(ui.device_selector->currentIndex());
    displayProcess = new QProcess;
    captureProcess = new QProcess;
    alsaProcess = new QProcess;

    connect(captureProcess, SIGNAL(stateChanged(QProcess::ProcessState)), this, SLOT(slotProcessStatus(QProcess::ProcessState)));

    QStringList env = QProcess::systemEnvironment();
    env << "SDL_WINDOWID=" + QString::number(ui.video_frame->winId());

    QString videoDriver = KdenliveSettings::videodrivername();
    if (!videoDriver.isEmpty()) {
        if (videoDriver == "x11_noaccel") {
            env << "SDL_VIDEO_YUV_HWACCEL=0";
            env << "SDL_VIDEODRIVER=x11";
        } else env << "SDL_VIDEODRIVER=" + videoDriver;
    }
    setenv("SDL_VIDEO_ALLOW_SCREENSAVER", "1", 1);

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

RecMonitor::~RecMonitor() {
    delete captureProcess;
    delete displayProcess;
    delete alsaProcess;
}

QString RecMonitor::name() const {
    return m_name;
}

void RecMonitor::slotConfigure() {
    emit showConfigDialog(4, ui.device_selector->currentIndex());
}

void RecMonitor::slotVideoDeviceChanged(int ix) {
    switch (ix) {
    case SCREENGRAB:
        m_discAction->setEnabled(false);
        m_rewAction->setEnabled(false);
        m_fwdAction->setEnabled(false);
        m_recAction->setEnabled(true);
        m_stopAction->setEnabled(false);
        m_playAction->setEnabled(false);
        ui.video_frame->setPixmap(mergeSideBySide(KIcon("video-display").pixmap(QSize(50, 50)), i18n("Press record button\nto start screen capture\nFiles will be saved in:\n%1", KdenliveSettings::capturefolder())));
        //ui.video_frame->setText(i18n("Press record button\nto start screen capture"));
        break;
    case VIDEO4LINUX:
        m_discAction->setEnabled(false);
        m_rewAction->setEnabled(false);
        m_fwdAction->setEnabled(false);
        m_recAction->setEnabled(true);
        m_stopAction->setEnabled(false);
        m_playAction->setEnabled(true);
        checkDeviceAvailability();
        break;
    default: // FIREWIRE
        m_discAction->setEnabled(true);
        m_recAction->setEnabled(false);
        m_stopAction->setEnabled(false);
        m_playAction->setEnabled(false);
        m_rewAction->setEnabled(false);
        m_fwdAction->setEnabled(false);
        //ui.video_frame->setText(i18n("Plug your camcorder and\npress connect button\nto initialize connection"));
        ui.video_frame->setPixmap(mergeSideBySide(KIcon("network-connect").pixmap(QSize(50, 50)), i18n("Plug your camcorder and\npress connect button\nto initialize connection\nFiles will be saved in:\n%1", KdenliveSettings::capturefolder())));
        break;
    }
}

QPixmap RecMonitor::mergeSideBySide(const QPixmap& pix, const QString txt) {
    QPainter p;
    QRect r = p.fontMetrics().boundingRect(QRect(0, 0, ui.video_frame->width(), ui.video_frame->height()), Qt::AlignLeft, txt);
    int strWidth = r.width();
    int strHeight = r.height();
    int pixWidth = pix.width();
    int pixHeight = pix.height();
    QPixmap res(strWidth + 8 + pixWidth, qMax(strHeight, pixHeight));
    res.fill(Qt::transparent);
    p.begin(&res);
    p.drawPixmap(0, 0, pix);
    p.drawText(QRect(pixWidth + 8, 0, strWidth, strHeight), 0, txt);
    p.end();
    return res;
}


void RecMonitor::checkDeviceAvailability() {
    if (!KIO::NetAccess::exists(KUrl(KdenliveSettings::video4vdevice()), KIO::NetAccess::SourceSide , this)) {
        m_playAction->setEnabled(false);
        m_recAction->setEnabled(false);
        ui.video_frame->setPixmap(mergeSideBySide(KIcon("camera-web").pixmap(QSize(50, 50)), i18n("Cannot read from device %1\nPlease check drivers and access rights.", KdenliveSettings::video4vdevice())));
        //ui.video_frame->setText(i18n("Cannot read from device %1\nPlease check drivers and access rights.", KdenliveSettings::video4vdevice()));
    } else //ui.video_frame->setText(i18n("Press play or record button\nto start video capture"));
        ui.video_frame->setPixmap(mergeSideBySide(KIcon("camera-web").pixmap(QSize(50, 50)), i18n("Press play or record button\nto start video capture\nFiles will be saved in:\n%1", KdenliveSettings::capturefolder())));
}

void RecMonitor::slotDisconnect() {
    if (captureProcess->state() == QProcess::NotRunning) {
        m_captureTime = KDateTime::currentLocalDateTime();
        kDebug() << "CURRENT TIME: " << m_captureTime.toString();
        m_didCapture = false;
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
        QTimer::singleShot(1000, captureProcess, SLOT(kill()));
        if (m_didCapture) manageCapturedFiles();
        m_didCapture = false;
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
    switch (ui.device_selector->currentIndex()) {
    case FIREWIRE:
        captureProcess->write("\e", 2);
        m_playAction->setIcon(m_playIcon);
        m_isPlaying = false;
        break;
    case VIDEO4LINUX:
        captureProcess->write("q\n", 3);
        QTimer::singleShot(1000, captureProcess, SLOT(kill()));

        break;
    case SCREENGRAB:
        captureProcess->write("q\n", 3);
        QTimer::singleShot(1000, captureProcess, SLOT(kill()));
        break;
    default:
        break;
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
        if (ui.device_selector->currentIndex() == FIREWIRE) {
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

    switch (ui.device_selector->currentIndex()) {
    case FIREWIRE:
        switch (KdenliveSettings::firewireformat()) {
        case 0:
            // RAW DV CAPTURE
            m_captureArgs << "--format" << "raw";
            m_displayArgs << "-f" << "dv";
            break;
        case 1:
            // DV type 1
            m_captureArgs << "--format" << "dv1";
            m_displayArgs << "-f" << "dv";
            break;
        case 2:
            // DV type 2
            m_captureArgs << "--format" << "dv2";
            m_displayArgs << "-f" << "dv";
            break;
        case 3:
            // HDV CAPTURE
            m_captureArgs << "--format" << "hdv";
            m_displayArgs << "-f" << "mpegts";
            break;
        }
        if (KdenliveSettings::firewireautosplit()) m_captureArgs << "--autosplit";
        if (KdenliveSettings::firewiretimestamp()) m_captureArgs << "--timestamp";
        m_captureArgs << "-i" << "capture" << "-";
        m_displayArgs << "-x" << QString::number(ui.video_frame->width()) << "-y" << QString::number(ui.video_frame->height()) << "-";

        captureProcess->setStandardOutputProcess(displayProcess);
        captureProcess->setWorkingDirectory(KdenliveSettings::capturefolder());
        captureProcess->start("dvgrab", m_captureArgs);
        if (play) captureProcess->write(" ", 1);
        m_discAction->setEnabled(true);
        break;
    case VIDEO4LINUX:
        m_captureArgs << KdenliveSettings::video4capture().simplified().split(' ') << "-";
        m_displayArgs << KdenliveSettings::video4playback().simplified().split(' ') << "-x" << QString::number(ui.video_frame->width()) << "-y" << QString::number(ui.video_frame->height()) << "-";
        captureProcess->setStandardOutputProcess(displayProcess);
        captureProcess->start("ffmpeg", m_captureArgs);
        break;
    default:
        break;
    }

    if (ui.device_selector->currentIndex() != SCREENGRAB) {
        displayProcess->start("ffplay", m_displayArgs);
        ui.video_frame->setText(i18n("Initialising..."));
    } else {
        // do something when starting screen grab
    }
}

void RecMonitor::slotRecord() {
    if (captureProcess->state() == QProcess::NotRunning && ui.device_selector->currentIndex() == FIREWIRE) {
        slotStartCapture();
    }
    if (m_isCapturing) {
        switch (ui.device_selector->currentIndex()) {
        case FIREWIRE:
            captureProcess->write("\e", 2);
            m_playAction->setIcon(m_playIcon);
            m_isCapturing = false;
            m_isPlaying = false;
            m_recAction->setChecked(false);
            break;
        case VIDEO4LINUX:
            captureProcess->terminate();
            slotStopCapture();
            //m_isCapturing = false;
            QTimer::singleShot(1000, this, SLOT(slotStartCapture()));
            break;
        case SCREENGRAB:
            captureProcess->write("q\n", 3);
            captureProcess->terminate();
            alsaProcess->terminate();
            alsaProcess->kill();
            // in case ffmpeg doesn't exit with the 'q' command, kill it one second later
            QTimer::singleShot(1000, captureProcess, SLOT(kill()));
            break;
        }
        return;
    } else if (ui.device_selector->currentIndex() == FIREWIRE) {
        m_isCapturing = true;
        m_didCapture = true;
        captureProcess->write("c\n", 3);
        return;
    }
    if (captureProcess->state() == QProcess::NotRunning) {
        m_recAction->setChecked(true);
        QString extension = "mpg";
        if (ui.device_selector->currentIndex() == SCREENGRAB) extension = KdenliveSettings::screengrabextension();
        QString path = KdenliveSettings::capturefolder() + "/capture0000." + extension;
        int i = 1;
        while (QFile::exists(path)) {
            QString num = QString::number(i).rightJustified(4, '0', false);
            path = KdenliveSettings::capturefolder() + "/capture" + num + "." + extension;
            i++;
        }

        m_captureFile = KUrl(path);

        m_captureArgs.clear();
        m_displayArgs.clear();
        QString args;

        switch (ui.device_selector->currentIndex()) {
        case FIREWIRE:
            m_captureArgs << "--format" << "hdv" << "-i" << "capture" << "-";
            m_displayArgs << "-f" << "mpegts" << "-x" << QString::number(ui.video_frame->width()) << "-y" << QString::number(ui.video_frame->height()) << "-";
            captureProcess->setStandardOutputProcess(displayProcess);
            captureProcess->start("dvgrab", m_captureArgs);
            break;
        case VIDEO4LINUX:
            m_captureArgs << KdenliveSettings::video4capture().simplified().split(' ') << "-y" << m_captureFile.path() << "-f" << KdenliveSettings::video4vencoding() << "-";
            m_displayArgs << KdenliveSettings::video4playback().simplified().split(' ') << "-x" << QString::number(ui.video_frame->width()) << "-y" << QString::number(ui.video_frame->height()) << "-";
            captureProcess->setStandardOutputProcess(displayProcess);
            captureProcess->start("ffmpeg", m_captureArgs);
            break;
        case SCREENGRAB:
            if (KdenliveSettings::fullscreengrab()) {
                const QRect rect = QApplication::desktop()->screenGeometry();
                args = KdenliveSettings::screengrabcapture().replace("%size", QString::number(rect.width()) + "x" + QString::number(rect.height())).replace("%offset", QString());
                if (KdenliveSettings::screengrabenableaudio()) {
                    // also capture audio
                    if (KdenliveSettings::useosscapture()) m_captureArgs << KdenliveSettings::screengrabosscapture().simplified().split(' ');
                    else m_captureArgs << KdenliveSettings::screengrabalsacapture2().simplified().split(' ');
                }
                m_captureArgs << args.simplified().split(' ') << KdenliveSettings::screengrabencoding().simplified().split(' ') << m_captureFile.path();
                ui.video_frame->setText(i18n("Capturing..."));
                m_isCapturing = true;
                if (KdenliveSettings::screengrabenableaudio() && !KdenliveSettings::useosscapture()) {
                    QStringList alsaArgs = KdenliveSettings::screengrabalsacapture().simplified().split(' ');
                    alsaProcess->setStandardOutputProcess(captureProcess);
                    const QString alsaBinary = alsaArgs.takeFirst();
                    alsaProcess->start(alsaBinary, alsaArgs);
                }
                captureProcess->start("ffmpeg", m_captureArgs);
            } else {
                ui.video_frame->setText(i18n("Select region..."));
                rgnGrab = new RegionGrabber();
                connect(rgnGrab, SIGNAL(regionGrabbed(const QRect&)), SLOT(slotStartGrab(const QRect &)));
            }
            break;
        default:
            break;
        }

        //ui.video_frame->setScaledContents(false);
        if (ui.device_selector->currentIndex() != SCREENGRAB) {
            m_isCapturing = true;
            displayProcess->start("ffplay", m_displayArgs);
            ui.video_frame->setText(i18n("Initialising..."));
        }
    } else {
        // stop capture
        displayProcess->kill();
        captureProcess->kill();
        alsaProcess->kill();
        QTimer::singleShot(1000, this, SLOT(slotRecord()));
    }
}

void RecMonitor::slotStartGrab(const QRect &rect) {
    rgnGrab->deleteLater();
    QApplication::restoreOverrideCursor();
    show();
    if (rect.isNull()) return;
    int width = rect.width();
    int height = rect.height();
    if (width % 2 != 0) width--;
    if (height % 2 != 0) height--;
    QString args = KdenliveSettings::screengrabcapture().replace("%size", QString::number(width) + "x" + QString::number(height)).replace("%offset", "+" + QString::number(rect.x()) + "," + QString::number(rect.y()));
    if (KdenliveSettings::screengrabenableaudio()) {
        // also capture audio
        if (KdenliveSettings::useosscapture()) m_captureArgs << KdenliveSettings::screengrabosscapture().simplified().split(' ');
        else m_captureArgs << KdenliveSettings::screengrabalsacapture2().simplified().split(' ');
    }
    m_captureArgs << args.simplified().split(' ') << KdenliveSettings::screengrabencoding().simplified().split(' ') << m_captureFile.path();
    m_isCapturing = true;
    ui.video_frame->setText(i18n("Capturing..."));
    if (KdenliveSettings::screengrabenableaudio() && !KdenliveSettings::useosscapture()) {
        QStringList alsaArgs = KdenliveSettings::screengrabalsacapture().simplified().split(' ');
        alsaProcess->setStandardOutputProcess(captureProcess);
        alsaProcess->start(alsaArgs.takeFirst(), alsaArgs);
    }
    captureProcess->start("ffmpeg", m_captureArgs);
}

void RecMonitor::slotProcessStatus(QProcess::ProcessState status) {
    if (status == QProcess::NotRunning) {
        displayProcess->kill();
        if (m_isCapturing && ui.device_selector->currentIndex() != FIREWIRE)
            if (ui.autoaddbox->isChecked() && QFile::exists(m_captureFile.path())) emit addProjectClip(m_captureFile);
        if (ui.device_selector->currentIndex() == FIREWIRE) {
            m_discAction->setIcon(KIcon("network-connect"));
            m_discAction->setText(i18n("Connect"));
            m_playAction->setEnabled(false);
            m_rewAction->setEnabled(false);
            m_fwdAction->setEnabled(false);
            m_recAction->setEnabled(false);
        }
        m_isPlaying = false;
        m_playAction->setIcon(m_playIcon);
        m_recAction->setChecked(false);
        m_stopAction->setEnabled(false);
        ui.device_selector->setEnabled(true);
        if (captureProcess && captureProcess->exitStatus() == QProcess::CrashExit) {
            ui.video_frame->setText(i18n("Capture crashed, please check your parameters"));
        } else {
            ui.video_frame->setText(i18n("Not connected"));
        }
        m_isCapturing = false;
    } else {
        if (ui.device_selector->currentIndex() != SCREENGRAB) m_stopAction->setEnabled(true);
        ui.device_selector->setEnabled(false);
    }
}

void RecMonitor::manageCapturedFiles() {
    QString extension;
    switch (KdenliveSettings::firewireformat()) {
    case 0:
        extension = ".dv";
        break;
    case 1:
    case 2:
        extension = ".avi";
        break;
    case 3:
        extension = ".m2t";
        break;
    }
    QDir dir(KdenliveSettings::capturefolder());
    QStringList filters;
    filters << "capture*" + extension;
    const QStringList result = dir.entryList(filters, QDir::Files, QDir::Time);
    KUrl::List capturedFiles;
    foreach(QString name, result) {
        KUrl url = KUrl(dir.filePath(name));
        if (KIO::NetAccess::exists(url, KIO::NetAccess::SourceSide, this)) {
            KFileItem file(KFileItem::Unknown, KFileItem::Unknown, url, true);
            if (file.time(KFileItem::ModificationTime) > m_captureTime) capturedFiles.append(url);
        }
    }
    kDebug() << "Found : " << capturedFiles.count() << " new capture files";
    kDebug() << capturedFiles;

    if (capturedFiles.count() > 0) {
        ManageCapturesDialog *d = new ManageCapturesDialog(capturedFiles, this);
        if (d->exec() == QDialog::Accepted) {
            capturedFiles = d->importFiles();
            foreach(KUrl url, capturedFiles) {
                emit addProjectClip(url);
            }
        }
        delete d;
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
