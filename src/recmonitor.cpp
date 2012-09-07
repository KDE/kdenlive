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


#include "recmonitor.h"
#include "gentime.h"
#include "mltdevicecapture.h"
#include "kdenlivesettings.h"
#include "managecapturesdialog.h"
#include "monitormanager.h"
#include "monitor.h"
#include "profilesdialog.h"

#include <KDebug>
#include <KLocale>
#include <KStandardDirs>
#include <KComboBox>
#include <KIO/NetAccess>
#include <KFileItem>
#include <KMessageBox>
#include <KApplication>
#include <KDiskFreeSpaceInfo>
#include <QMouseEvent>
#include <QMenu>
#include <QToolButton>
#include <QFile>
#include <QDir>


RecMonitor::RecMonitor(Kdenlive::MONITORID name, MonitorManager *manager, QWidget *parent) :
    AbstractMonitor(name, manager, parent),
    m_isCapturing(false),
    m_didCapture(false),
    m_isPlaying(false),
    m_captureDevice(NULL),
    m_analyse(false)
{
    setupUi(this);

    //video_frame->setAttribute(Qt::WA_PaintOnScreen);
    device_selector->setCurrentIndex(KdenliveSettings::defaultcapture());
    connect(device_selector, SIGNAL(currentIndexChanged(int)), this, SLOT(slotVideoDeviceChanged(int)));

    // Video widget holder
    QVBoxLayout *l = new QVBoxLayout;
    l->setContentsMargins(0, 0, 0, 0);
    l->setSpacing(0);
    l->addWidget(videoBox, 10);
    video_frame->setLayout(l);
    createVideoSurface();

    QToolBar *toolbar = new QToolBar(this);
    QHBoxLayout *layout = new QHBoxLayout;
    layout->setContentsMargins(0, 0, 0, 0);
    m_playIcon = KIcon("media-playback-start");
    m_pauseIcon = KIcon("media-playback-pause");

    m_discAction = toolbar->addAction(KIcon("network-connect"), i18n("Connect"));
    connect(m_discAction, SIGNAL(triggered()), this, SLOT(slotDisconnect()));

    m_rewAction = toolbar->addAction(KIcon("media-seek-backward"), i18n("Rewind"));
    connect(m_rewAction, SIGNAL(triggered()), this, SLOT(slotRewind()));

    m_playAction = toolbar->addAction(m_playIcon, i18n("Play"));
    connect(m_playAction, SIGNAL(triggered()), this, SLOT(slotStartPreview()));

    m_stopAction = toolbar->addAction(KIcon("media-playback-stop"), i18n("Stop"));
    connect(m_stopAction, SIGNAL(triggered()), this, SLOT(slotStopCapture()));
    m_stopAction->setEnabled(false);
    m_fwdAction = toolbar->addAction(KIcon("media-seek-forward"), i18n("Forward"));
    connect(m_fwdAction, SIGNAL(triggered()), this, SLOT(slotForward()));

    m_recAction = toolbar->addAction(KIcon("media-record"), i18n("Record"));
    connect(m_recAction, SIGNAL(triggered()), this, SLOT(slotRecord()));
    m_recAction->setCheckable(true);

    rec_options->setIcon(KIcon("system-run"));
    QMenu *menu = new QMenu(this);
    m_addCapturedClip = new QAction(i18n("Add Captured File to Project"), this);
    m_addCapturedClip->setCheckable(true);
    m_addCapturedClip->setChecked(true);
    menu->addAction(m_addCapturedClip);

    rec_audio->setChecked(KdenliveSettings::v4l_captureaudio());
    rec_video->setChecked(KdenliveSettings::v4l_capturevideo());

    m_previewSettings = new QAction(i18n("Recording Preview"), this);
    m_previewSettings->setCheckable(true);


    rec_options->setMenu(menu);
    menu->addAction(m_previewSettings);

    toolbar->addSeparator();

    QAction *configAction = toolbar->addAction(KIcon("configure"), i18n("Configure"));
    connect(configAction, SIGNAL(triggered()), this, SLOT(slotConfigure()));
    configAction->setCheckable(false);

    layout->addWidget(toolbar);
    layout->addWidget(&m_logger);
    layout->addWidget(&m_dvinfo);
    m_logger.setMaxCount(10);
    m_logger.setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_logger.setFrame(false);
    //m_logger.setInsertPolicy(QComboBox::InsertAtTop);

    m_freeSpace = new KCapacityBar(KCapacityBar::DrawTextInline, this);
    m_freeSpace->setMaximumWidth(150);
    QFontMetricsF fontMetrics(font());
    m_freeSpace->setMaximumHeight(fontMetrics.height() * 1.2);
    slotUpdateFreeSpace();
    layout->addWidget(m_freeSpace);
    connect(&m_spaceTimer, SIGNAL(timeout()), this, SLOT(slotUpdateFreeSpace()));
    m_spaceTimer.setInterval(30000);
    m_spaceTimer.setSingleShot(false);

    control_frame_firewire->setLayout(layout);
    m_displayProcess = new QProcess;
    m_captureProcess = new QProcess;

    connect(m_captureProcess, SIGNAL(stateChanged(QProcess::ProcessState)), this, SLOT(slotProcessStatus(QProcess::ProcessState)));
    connect(m_captureProcess, SIGNAL(readyReadStandardError()), this, SLOT(slotReadDvgrabInfo()));
    
    QString videoDriver = KdenliveSettings::videodrivername();
#if QT_VERSION >= 0x040600
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert("SDL_WINDOWID", QString::number(videoSurface->winId()));
    if (!videoDriver.isEmpty()) {
        if (videoDriver == "x11_noaccel") {
            env.insert("SDL_VIDEO_YUV_HWACCEL", "0");
            env.insert("SDL_VIDEODRIVER", "x11");
        } else env.insert("SDL_VIDEODRIVER", videoDriver);
    }
    m_displayProcess->setProcessEnvironment(env);
#else
    QStringList env = QProcess::systemEnvironment();
    env << "SDL_WINDOWID=" + QString::number(videoSurface->winId());
    if (!videoDriver.isEmpty()) {
        if (videoDriver == "x11_noaccel") {
            env << "SDL_VIDEO_YUV_HWACCEL=0";
            env << "SDL_VIDEODRIVER=x11";
        } else env << "SDL_VIDEODRIVER=" + videoDriver;
    }
    m_displayProcess->setEnvironment(env);
#endif

    setenv("SDL_VIDEO_ALLOW_SCREENSAVER", "1", 1);

    kDebug() << "/////// BUILDING MONITOR, ID: " << videoSurface->winId();
    slotVideoDeviceChanged(device_selector->currentIndex());
    m_previewSettings->setChecked(KdenliveSettings::enable_recording_preview());
    connect(m_previewSettings, SIGNAL(triggered(bool)), this, SLOT(slotChangeRecordingPreview(bool)));
}

RecMonitor::~RecMonitor()
{
    m_spaceTimer.stop();
    delete m_captureProcess;
    delete m_displayProcess;
    if (m_captureDevice) delete m_captureDevice;
}

void RecMonitor::mouseDoubleClickEvent(QMouseEvent * event)
{
    if (!KdenliveSettings::openglmonitors() && videoBox && videoBox->isVisible()) {
        videoBox->switchFullScreen();
        event->accept();
    }
}

void RecMonitor::slotSwitchFullScreen()
{
    videoBox->switchFullScreen();
}

void RecMonitor::stop()
{
    slotStopCapture();
}

void RecMonitor::start()
{
    //slotStartPreview(true);
}

void RecMonitor::slotConfigure()
{
    emit showConfigDialog(4, device_selector->currentIndex());
}

void RecMonitor::slotUpdateCaptureFolder(const QString &currentProjectFolder)
{
    if (KdenliveSettings::capturetoprojectfolder()) m_capturePath = currentProjectFolder;
    else m_capturePath = KdenliveSettings::capturefolder();
    if (m_captureProcess) m_captureProcess->setWorkingDirectory(m_capturePath);
    if (m_captureProcess->state() != QProcess::NotRunning) {
        if (device_selector->currentIndex() == FIREWIRE)
            KMessageBox::information(this, i18n("You need to disconnect and reconnect in the capture monitor to apply your changes"), i18n("Capturing"));
        else KMessageBox::information(this, i18n("You need to stop capture before your changes can be applied"), i18n("Capturing"));
    } else slotVideoDeviceChanged(device_selector->currentIndex());

    // update free space info
    slotUpdateFreeSpace();
}

void RecMonitor::slotVideoDeviceChanged(int ix)
{
    QString capturefile;
    QString capturename;
    m_previewSettings->setEnabled(ix == VIDEO4LINUX || ix == BLACKMAGIC);
    rec_audio->setVisible(ix == VIDEO4LINUX);
    rec_video->setVisible(ix == VIDEO4LINUX);
    m_fwdAction->setVisible(ix == FIREWIRE);
    m_discAction->setVisible(ix == FIREWIRE);
    m_rewAction->setVisible(ix == FIREWIRE);
    m_recAction->setEnabled(ix != FIREWIRE);
    m_logger.setVisible(ix == BLACKMAGIC);
    if (m_captureDevice) {
        // MLT capture still running, abort
        m_monitorManager->clearScopeSource();
        m_captureDevice->stop();
        delete m_captureDevice;
        m_captureDevice = NULL;
    }

    // The m_videoBox container has to be shown once before the MLT consumer is build, or preview will fail
    videoBox->setHidden(ix != VIDEO4LINUX && ix != BLACKMAGIC);
    videoBox->setHidden(true);
    switch (ix) {
    case SCREENGRAB:
        m_discAction->setEnabled(false);
        m_rewAction->setEnabled(false);
        m_fwdAction->setEnabled(false);
        m_stopAction->setEnabled(false);
        m_playAction->setEnabled(false);
        if (KdenliveSettings::rmd_path().isEmpty()) {
            QString rmdpath = KStandardDirs::findExe("recordmydesktop");
            if (rmdpath.isEmpty()) video_frame->setPixmap(mergeSideBySide(KIcon("dialog-warning").pixmap(QSize(50, 50)), i18n("Recordmydesktop utility not found,\n please install it for screen grabs")));
            else KdenliveSettings::setRmd_path(rmdpath);
        }
        if (!KdenliveSettings::rmd_path().isEmpty()) video_frame->setPixmap(mergeSideBySide(KIcon("video-display").pixmap(QSize(50, 50)), i18n("Press record button\nto start screen capture\nFiles will be saved in:\n%1", m_capturePath)));
        //video_frame->setText(i18n("Press record button\nto start screen capture"));
        break;
    case VIDEO4LINUX:
        m_stopAction->setEnabled(false);
        m_playAction->setEnabled(true);
        checkDeviceAvailability();
        break;
    case BLACKMAGIC:
        m_stopAction->setEnabled(false);
        m_playAction->setEnabled(true);
        capturefile = m_capturePath;
        if (!capturefile.endsWith('/')) capturefile.append('/');
        capturename = KdenliveSettings::decklink_filename();
        capturename.append("xxx.");
        capturename.append(KdenliveSettings::decklink_extension());
        capturefile.append(capturename);
        video_frame->setPixmap(mergeSideBySide(KIcon("camera-photo").pixmap(QSize(50, 50)), i18n("Plug your camcorder and\npress play button\nto start preview.\nFiles will be saved in:\n%1", capturefile)));
        break;
    default: // FIREWIRE
        m_discAction->setEnabled(true);
        m_stopAction->setEnabled(false);
        m_playAction->setEnabled(false);
        m_rewAction->setEnabled(false);
        m_fwdAction->setEnabled(false);

        // Check that dvgab is available
        if (KdenliveSettings::dvgrab_path().isEmpty()) {
            QString dvgrabpath = KStandardDirs::findExe("dvgrab");
            if (dvgrabpath.isEmpty()) video_frame->setPixmap(mergeSideBySide(KIcon("dialog-warning").pixmap(QSize(50, 50)), i18n("dvgrab utility not found,\n please install it for firewire capture")));
            else KdenliveSettings::setDvgrab_path(dvgrabpath);
        } else {
            // Show capture info
            capturefile = m_capturePath;
            if (!capturefile.endsWith('/')) capturefile.append('/');
            capturename = KdenliveSettings::dvgrabfilename();
            if (capturename.isEmpty()) capturename = "capture";
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
            capturename.append("xxx" + extension);
            capturefile.append(capturename);
            video_frame->setPixmap(mergeSideBySide(KIcon("network-connect").pixmap(QSize(50, 50)), i18n("Plug your camcorder and\npress connect button\nto initialize connection\nFiles will be saved in:\n%1", capturefile)));
        }
        break;
    }
}

void RecMonitor::slotSetInfoMessage(const QString &message)
{
    m_logger.insertItem(0, message);
}

QPixmap RecMonitor::mergeSideBySide(const QPixmap& pix, const QString &txt)
{
    QPainter p;
    QRect r = QApplication::fontMetrics().boundingRect(QRect(0, 0, video_frame->width(), video_frame->height()), Qt::AlignLeft, txt);
    int strWidth = r.width();
    int strHeight = r.height();
    int pixWidth = pix.width();
    int pixHeight = pix.height();
    QPixmap res(strWidth + 8 + pixWidth, qMax(strHeight, pixHeight));
    res.fill(Qt::transparent);
    p.begin(&res);
    p.drawPixmap(0, 0, pix);
    p.setPen(kapp->palette().text().color());
    p.drawText(QRect(pixWidth + 8, 0, strWidth, strHeight), 0, txt);
    p.end();
    return res;
}


void RecMonitor::checkDeviceAvailability()
{
    if (!KIO::NetAccess::exists(KUrl(KdenliveSettings::video4vdevice()), KIO::NetAccess::SourceSide , this)) {
        m_playAction->setEnabled(false);
        m_recAction->setEnabled(false);
        video_frame->setPixmap(mergeSideBySide(KIcon("camera-web").pixmap(QSize(50, 50)), i18n("Cannot read from device %1\nPlease check drivers and access rights.", KdenliveSettings::video4vdevice())));
    } else {
        video_frame->setPixmap(mergeSideBySide(KIcon("camera-web").pixmap(QSize(50, 50)), i18n("Press play or record button\nto start video capture\nFiles will be saved in:\n%1", m_capturePath)));
    }
}

void RecMonitor::slotDisconnect()
{
    if (m_captureProcess->state() == QProcess::NotRunning) {
        m_captureTime = KDateTime::currentLocalDateTime();
        kDebug() << "CURRENT TIME: " << m_captureTime.toString();       
        m_didCapture = false;
        slotStartPreview(false);
        m_discAction->setIcon(KIcon("network-disconnect"));
        m_discAction->setText(i18n("Disconnect"));
        m_recAction->setEnabled(true);
        m_stopAction->setEnabled(true);
        m_playAction->setEnabled(true);
        m_rewAction->setEnabled(true);
        m_fwdAction->setEnabled(true);
    } else {
        m_captureProcess->write("q", 1);
        QTimer::singleShot(1000, m_captureProcess, SLOT(kill()));
        if (m_didCapture) manageCapturedFiles();
        m_didCapture = false;
    }
}

void RecMonitor::slotRewind()
{
    m_captureProcess->write("a", 1);
}

void RecMonitor::slotForward()
{
    m_captureProcess->write("z", 1);
}

void RecMonitor::slotStopCapture()
{
    // stop capture
    if (!m_isCapturing && !m_isPlaying) return;
    videoBox->setHidden(true);
    rec_audio->setEnabled(true);
    rec_video->setEnabled(true);
    switch (device_selector->currentIndex()) {
    case FIREWIRE:
        m_captureProcess->write("\e", 2);
        m_playAction->setIcon(m_playIcon);
        m_isPlaying = false;
        break;
    case SCREENGRAB:
        m_captureProcess->write("q\n", 3);
        m_captureProcess->terminate();
        video_frame->setText(i18n("Encoding captured video..."));
        QTimer::singleShot(1000, m_captureProcess, SLOT(kill()));
        break;
    case VIDEO4LINUX:
    case BLACKMAGIC:
        if (m_captureDevice) {
            m_captureDevice->stop();
        }
        m_previewSettings->setEnabled(true);
        m_isCapturing = false;
        m_isPlaying = false;
        m_playAction->setEnabled(true);
        m_stopAction->setEnabled(false);
        m_recAction->setEnabled(true);
        slotSetInfoMessage(i18n("Capture stopped"));
        m_isCapturing = false;
        m_recAction->setChecked(false);
        if (m_addCapturedClip->isChecked() && !m_captureFile.isEmpty() && QFile::exists(m_captureFile.path())) {
            emit addProjectClip(m_captureFile);
            m_captureFile.clear();
        }
        break;
    default:
        break;
    }
}

void RecMonitor::slotStartPreview(bool play)
{
    if (m_captureProcess->state() != QProcess::NotRunning) {
        if (device_selector->currentIndex() == FIREWIRE) {
            videoBox->setHidden(false);
            if (m_isPlaying) {
                m_captureProcess->write("k", 1);
                //captureProcess->write("\e", 2);
                m_playAction->setIcon(m_playIcon);
                m_isPlaying = false;
            } else {
                m_captureProcess->write("p", 1);
                m_playAction->setIcon(m_pauseIcon);
                m_isPlaying = true;
            }
        }
        return;
    }
    slotActivateMonitor();
    if (m_isPlaying) return;
    m_captureArgs.clear();
    m_displayArgs.clear();
    m_isPlaying = false;
    QString capturename = KdenliveSettings::dvgrabfilename();
    QString path;
    MltVideoProfile profile;
    QString producer;
    QStringList dvargs = KdenliveSettings::dvgrabextra().simplified().split(' ', QString::SkipEmptyParts);
    int ix = device_selector->currentIndex();
    videoBox->setHidden(ix != VIDEO4LINUX && ix != BLACKMAGIC && ix != FIREWIRE);
    switch (ix) {
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
        if (!dvargs.isEmpty()) {
            m_captureArgs << dvargs;
        }
        m_captureArgs << "-i";
        if (capturename.isEmpty()) capturename = "capture";
        m_captureArgs << capturename << "-";

        m_displayArgs << "-x" << QString::number(video_frame->width()) << "-y" << QString::number(video_frame->height()) << "-noframedrop" << "-";

        m_captureProcess->setStandardOutputProcess(m_displayProcess);
        m_captureProcess->setWorkingDirectory(m_capturePath);
        kDebug() << "Capture: Running dvgrab " << m_captureArgs.join(" ");

        m_captureProcess->start(KdenliveSettings::dvgrab_path(), m_captureArgs);
        if (play) m_captureProcess->write(" ", 1);
        m_discAction->setEnabled(true);
        break;
    case VIDEO4LINUX:
        path = KStandardDirs::locateLocal("appdata", "profiles/video4linux");
        buildMltDevice(path);
        profile = ProfilesDialog::getVideoProfile(path);
        producer = getV4lXmlPlaylist(profile);

        //producer = QString("avformat-novalidate:video4linux2:%1?width:%2&height:%3&frame_rate:%4").arg(KdenliveSettings::video4vdevice()).arg(profile.width).arg(profile.height).arg((double) profile.frame_rate_num / profile.frame_rate_den);
        if (!m_captureDevice->slotStartPreview(producer, true)) {
            // v4l capture failed to start
            video_frame->setText(i18n("Failed to start Video4Linux,\ncheck your parameters..."));
            videoBox->setHidden(true);

        } else {
            m_playAction->setEnabled(false);
            m_stopAction->setEnabled(true);
            m_isPlaying = true;
        }

        break;
    case BLACKMAGIC:
        path = KdenliveSettings::current_profile();
        slotActivateMonitor();
        buildMltDevice(path);
        producer = QString("decklink:%1").arg(KdenliveSettings::decklink_capturedevice());
        if (!m_captureDevice->slotStartPreview(producer)) {
            // v4l capture failed to start
            video_frame->setText(i18n("Failed to start Decklink,\ncheck your parameters..."));
            videoBox->setHidden(true);

        } else {
            m_playAction->setEnabled(false);
            m_stopAction->setEnabled(true);
            m_isPlaying = true;
        }
        break;
    default:
        break;
    }

    rec_audio->setEnabled(false);
    rec_video->setEnabled(false);

    if (device_selector->currentIndex() == FIREWIRE) {
        kDebug() << "Capture: Running ffplay " << m_displayArgs.join(" ");
        m_displayProcess->start(KdenliveSettings::ffplaypath(), m_displayArgs);
        //video_frame->setText(i18n("Initialising..."));
    } else {
        // do something when starting screen grab
    }
}

void RecMonitor::slotRecord()
{
    rec_audio->setEnabled(false);
    rec_video->setEnabled(false);

    if (m_captureProcess->state() == QProcess::NotRunning && device_selector->currentIndex() == FIREWIRE) {
        slotStartPreview();
    }
    if (m_isCapturing) {
        // User stopped capture
        slotStopCapture();
        return;
    } else if (device_selector->currentIndex() == FIREWIRE) {
        m_isCapturing = true;
        m_didCapture = true;
        m_captureProcess->write("c\n", 3);
        m_spaceTimer.start();
        return;
    }
    if (m_captureProcess->state() == QProcess::NotRunning) {
        m_logger.clear();
        m_recAction->setChecked(true);
        QString extension = "mpg";
        if (device_selector->currentIndex() == SCREENGRAB) extension = "ogv"; //KdenliveSettings::screengrabextension();
        else if (device_selector->currentIndex() == VIDEO4LINUX) {
            // TODO: when recording audio only, allow configuration?
            if (!rec_video->isChecked()) extension = "wav";
            else extension = KdenliveSettings::v4l_extension();
        }
        else if (device_selector->currentIndex() == BLACKMAGIC) extension = KdenliveSettings::decklink_extension();
        QString path = KUrl(m_capturePath).path(KUrl::AddTrailingSlash) + "capture0000." + extension;
        int i = 1;
        while (QFile::exists(path)) {
            QString num = QString::number(i).rightJustified(4, '0', false);
            path = KUrl(m_capturePath).path(KUrl::AddTrailingSlash) + "capture" + num + '.' + extension;
            i++;
        }
        m_captureFile = KUrl(path);

        m_captureArgs.clear();
        m_displayArgs.clear();
        QString args;
        QString playlist;
        QString v4lparameters;
        MltVideoProfile profile;
        bool showPreview;
        QString capturename = KdenliveSettings::dvgrabfilename();
        if (capturename.isEmpty()) capturename = "capture";

        switch (device_selector->currentIndex()) {
        case VIDEO4LINUX:
            slotActivateMonitor();
            path = KStandardDirs::locateLocal("appdata", "profiles/video4linux");
            profile = ProfilesDialog::getVideoProfile(path);
            //m_videoBox->setRatio((double) profile.display_aspect_num / profile.display_aspect_den);
            buildMltDevice(path);
            playlist = getV4lXmlPlaylist(profile);

            v4lparameters = KdenliveSettings::v4l_parameters();

            // TODO: when recording audio only, allow param configuration?
            if (!rec_video->isChecked()) v4lparameters.clear();

            // Add alsa audio capture
            if (!rec_audio->isChecked()) {
                // if we do not want audio, make sure that we don't have audio encoding parameters
                // this is required otherwise the MLT avformat consumer will not close properly
                if (v4lparameters.contains("acodec")) {
                    QString endParam = v4lparameters.section("acodec", 1);
                    int vcodec = endParam.indexOf(" vcodec");
                    int format = endParam.indexOf(" f=");
                    int cutPosition = -1;
                    if (vcodec > -1) {
                        if (format  > -1) {
                            cutPosition = qMin(vcodec, format);
                        }
                        else cutPosition = vcodec;
                    }
                    else if (format  > -1) {
                        cutPosition = format;
                    }
                    else {
                        // nothing interesting in end params
                        endParam.clear();
                    }
                    if (cutPosition > -1) {
                        endParam.remove(0, cutPosition);
                    }
                    v4lparameters = QString(v4lparameters.section("acodec", 0, 0) + "an=1 " + endParam).simplified();
                }
            }

            showPreview = m_previewSettings->isChecked();
            if (!rec_video->isChecked()) showPreview = false;

            if (m_captureDevice->slotStartCapture(v4lparameters, m_captureFile.path(), playlist, showPreview)) {
                videoBox->setHidden(false);
                m_isCapturing = true;
                m_recAction->setEnabled(false);
                m_stopAction->setEnabled(true);
                m_previewSettings->setEnabled(false);
            }
            else {
                video_frame->setText(i18n("Failed to start Video4Linux,\ncheck your parameters..."));
                videoBox->setHidden(true);
                m_isCapturing = false;
            }
            break;

        case BLACKMAGIC:
            slotActivateMonitor();
            path = KdenliveSettings::current_profile();
            profile = ProfilesDialog::getVideoProfile(path);
            //m_videoBox->setRatio((double) profile.display_aspect_num / profile.display_aspect_den);
            buildMltDevice(path);

            playlist = QString("<producer id=\"producer0\" in=\"0\" out=\"99999\"><property name=\"mlt_type\">producer</property><property name=\"length\">100000</property><property name=\"eof\">pause</property><property name=\"resource\">%1</property><property name=\"mlt_service\">decklink</property></producer>").arg(KdenliveSettings::decklink_capturedevice());

            if (m_captureDevice->slotStartCapture(KdenliveSettings::decklink_parameters(), m_captureFile.path(), QString("decklink:%1").arg(KdenliveSettings::decklink_capturedevice()), m_previewSettings->isChecked(), false)) {
                videoBox->setHidden(false);
                m_isCapturing = true;
                slotSetInfoMessage(i18n("Capturing to %1", m_captureFile.fileName()));
                m_recAction->setEnabled(false);
                m_stopAction->setEnabled(true);
                m_previewSettings->setEnabled(false);
            }
            else {
                video_frame->setText(i18n("Failed to start Decklink,\ncheck your parameters..."));
                slotSetInfoMessage(i18n("Failed to start capture"));
                videoBox->setHidden(true);
                m_isCapturing = false;
            }
            break;

        case SCREENGRAB:
            switch (KdenliveSettings::rmd_capture_type()) {
            case 0:
                // Full screen capture, nothing special to do
                break;
            default:
                // Region capture
                m_captureArgs << "--width" << QString::number(KdenliveSettings::rmd_width()) << "--height" << QString::number(KdenliveSettings::rmd_height());
                if (!KdenliveSettings::rmd_follow_mouse()) {
                    m_captureArgs << "-x" << QString::number(KdenliveSettings::rmd_offsetx()) << "-y" << QString::number(KdenliveSettings::rmd_offsety());
                } else {
                    m_captureArgs << "--follow-mouse";
                    if (KdenliveSettings::rmd_hide_frame()) m_captureArgs << "--no-frame";
                }
                break;
            }
            if (KdenliveSettings::rmd_hide_mouse()) m_captureArgs << "--no-cursor";
            m_isCapturing = true;
            if (KdenliveSettings::rmd_capture_audio()) {
                m_captureArgs << "--freq" << KdenliveSettings::rmd_freq();
                m_captureArgs << "--channels" << QString::number(KdenliveSettings::rmd_audio_channels());
                if (KdenliveSettings::rmd_use_jack()) {
                    m_captureArgs << "--use-jack";
                    QStringList ports = KdenliveSettings::rmd_jackports().split(' ', QString::SkipEmptyParts);
                    for (int i = 0; i < ports.count(); ++i) {
                        m_captureArgs << ports.at(i);
                    }
                    if (KdenliveSettings::rmd_jack_buffer() > 0.0)
                        m_captureArgs << "--ring-buffer-size" << QString::number(KdenliveSettings::rmd_jack_buffer());
                } else {
                    if (!KdenliveSettings::rmd_alsadevicename().isEmpty())
                        m_captureArgs << "--device" << KdenliveSettings::rmd_alsadevicename();
                    if (KdenliveSettings::rmd_alsa_buffer() > 0)
                        m_captureArgs << "--buffer-size" << QString::number(KdenliveSettings::rmd_alsa_buffer());
                }
            } else m_captureArgs << "--no-sound";

            if (KdenliveSettings::rmd_fullshots()) m_captureArgs << "--full-shots";
            m_captureArgs << "--v_bitrate" << QString::number(KdenliveSettings::rmd_bitrate());
            m_captureArgs << "--v_quality" << QString::number(KdenliveSettings::rmd_quality());
            m_captureArgs << "--workdir" << KdenliveSettings::currenttmpfolder();
            m_captureArgs << "--fps" << QString::number(KdenliveSettings::rmd_fps()) << "-o" << m_captureFile.path();
            m_captureProcess->start(KdenliveSettings::rmd_path(), m_captureArgs);
            kDebug() << "// RecordMyDesktop params: " << m_captureArgs;
            break;
        default:
            break;
        }


        if (device_selector->currentIndex() == FIREWIRE) {
            m_isCapturing = true;
            kDebug() << "Capture: Running ffplay " << m_displayArgs.join(" ");
            m_displayProcess->start(KdenliveSettings::ffplaypath(), m_displayArgs);
            video_frame->setText(i18n("Initialising..."));
        }
    } else {
        // stop capture
        m_displayProcess->kill();
        //captureProcess->kill();
        QTimer::singleShot(1000, this, SLOT(slotRecord()));
    }
}

const QString RecMonitor::getV4lXmlPlaylist(MltVideoProfile profile) {

    QString playlist = QString("<mlt title=\"capture\" LC_NUMERIC=\"C\"><profile description=\"v4l\" width=\"%1\" height=\"%2\" progressive=\"%3\" sample_aspect_num=\"%4\" sample_aspect_den=\"%5\" display_aspect_num=\"%6\" display_aspect_den=\"%7\" frame_rate_num=\"%8\" frame_rate_den=\"%9\" colorspace=\"%10\"/>").arg(profile.width).arg(profile.height).arg(profile.progressive).arg(profile.sample_aspect_num).arg(profile.sample_aspect_den).arg(profile.display_aspect_num).arg(profile.display_aspect_den).arg(profile.frame_rate_num).arg(profile.frame_rate_den).arg(profile.colorspace);

    if (rec_video->isChecked()) {
        playlist.append(QString("<producer id=\"producer0\" in=\"0\" out=\"999999\"><property name=\"mlt_type\">producer</property><property name=\"length\">1000000</property><property name=\"eof\">loop</property><property name=\"resource\">video4linux2:%1?width:%2&amp;height:%3&amp;frame_rate:%4</property><property name=\"mlt_service\">avformat-novalidate</property></producer><playlist id=\"playlist0\"><entry producer=\"producer0\" in=\"0\" out=\"999999\"/></playlist>").arg(KdenliveSettings::video4vdevice()).arg(profile.width).arg(profile.height).arg((double) profile.frame_rate_num / profile.frame_rate_den));
    }

    if (rec_audio->isChecked()) {
        playlist.append(QString("<producer id=\"producer1\" in=\"0\" out=\"999999\"><property name=\"mlt_type\">producer</property><property name=\"length\">1000000</property><property name=\"eof\">loop</property><property name=\"resource\">alsa:%5</property><property name=\"audio_index\">0</property><property name=\"video_index\">-1</property><property name=\"mlt_service\">avformat-novalidate</property></producer><playlist id=\"playlist1\"><entry producer=\"producer1\" in=\"0\" out=\"999999\"/></playlist>").arg(KdenliveSettings::v4l_alsadevicename()));
    }
    playlist.append("<tractor id=\"tractor0\" title=\"video0\" global_feed=\"1\" in=\"0\" out=\"999999\">");
    if (rec_video->isChecked()) playlist.append("<track producer=\"playlist0\"/>");
    if (rec_audio->isChecked()) playlist.append("<track producer=\"playlist1\"/>");
    playlist.append("</tractor></mlt>");

    return playlist;
}

/*
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
    video_frame->setText(i18n("Capturing..."));
    if (KdenliveSettings::screengrabenableaudio() && !KdenliveSettings::useosscapture()) {
        QStringList alsaArgs = KdenliveSettings::screengrabalsacapture().simplified().split(' ');
        alsaProcess->setStandardOutputProcess(captureProcess);
        kDebug() << "Capture: Running arecord " << alsaArgs.join(" ");
        alsaProcess->start("arecord", alsaArgs);
    }
    kDebug() << "Capture: Running ffmpeg " << m_captureArgs.join(" ");
    captureProcess->start(KdenliveSettings::ffmpegpath(), m_captureArgs);
}*/

void RecMonitor::slotProcessStatus(QProcess::ProcessState status)
{
    if (status == QProcess::NotRunning) {
        m_displayProcess->kill();
        if (m_isCapturing && device_selector->currentIndex() != FIREWIRE)
            if (m_addCapturedClip->isChecked() && !m_captureFile.isEmpty() && QFile::exists(m_captureFile.path())) {
                emit addProjectClip(m_captureFile);
                m_captureFile.clear();
            }
        if (device_selector->currentIndex() == FIREWIRE) {
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
        device_selector->setEnabled(true);
        if (m_captureProcess && m_captureProcess->exitStatus() == QProcess::CrashExit) {
            video_frame->setText(i18n("Capture crashed, please check your parameters"));
        } else {
            if (device_selector->currentIndex() != SCREENGRAB) {
                video_frame->setText(i18n("Not connected"));
            } else {
                if (m_captureProcess->exitCode() != 0) {
                    video_frame->setText(i18n("Capture crashed, please check your parameters\nRecordMyDesktop exit code: %1", QString::number(m_captureProcess->exitCode())));
                } else {
                    video_frame->setPixmap(mergeSideBySide(KIcon("video-display").pixmap(QSize(50, 50)), i18n("Press record button\nto start screen capture\nFiles will be saved in:\n%1", m_capturePath)));
                }
            }
        }
        m_isCapturing = false;

        m_spaceTimer.stop();
        // update free space info
        slotUpdateFreeSpace();
    } else {
        if (device_selector->currentIndex() != SCREENGRAB) m_stopAction->setEnabled(true);
        device_selector->setEnabled(false);
    }
}

void RecMonitor::manageCapturedFiles()
{
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
    QDir dir(m_capturePath);
    QStringList filters;
    QString capturename = KdenliveSettings::dvgrabfilename();
    if (capturename.isEmpty()) capturename = "capture";
    filters << capturename + '*' + extension;
    const QStringList result = dir.entryList(filters, QDir::Files, QDir::Time);
    KUrl::List capturedFiles;
    foreach(const QString & name, result) {
        KUrl url = KUrl(dir.filePath(name));
        if (KIO::NetAccess::exists(url, KIO::NetAccess::SourceSide, this)) {
            KFileItem file(KFileItem::Unknown, KFileItem::Unknown, url, true);
            if (file.time(KFileItem::ModificationTime) > m_captureTime) {
                // The file was captured in the last batch
                if (url.fileName().contains(':')) {
                    // Several dvgrab options (--timecode,...) use : in the file name, which is
                    // not supported by MLT, so rename them
                    QString newUrl = url.directory(KUrl::AppendTrailingSlash) + url.fileName().replace(':', '_');
                    if (QFile::rename(url.path(), newUrl)) {
                        url = KUrl(newUrl);
                    }

                }
                capturedFiles.append(url);
            }
        }
    }
    kDebug() << "Found : " << capturedFiles.count() << " new capture files";
    kDebug() << capturedFiles;

    if (capturedFiles.count() > 0) {
        QPointer<ManageCapturesDialog> d = new ManageCapturesDialog(capturedFiles, this);
        if (d->exec() == QDialog::Accepted) {
            emit addProjectClipList(d->importFiles());
        }
        delete d;
    }
}

// virtual
void RecMonitor::mousePressEvent(QMouseEvent *event)
{
    if (m_freeSpace->underMouse()) slotUpdateFreeSpace();
    else QWidget::mousePressEvent(event);//m_videoBox->mousePressEvent(event);
}

void RecMonitor::slotUpdateFreeSpace()
{
    KDiskFreeSpaceInfo info = KDiskFreeSpaceInfo::freeSpaceInfo(m_capturePath);
    if (info.isValid() && info.size() > 0) {
        m_freeSpace->setValue(100 * info.used() / info.size());
        m_freeSpace->setText(i18n("Free space: %1", KIO::convertSize(info.available())));
        m_freeSpace->update();
    }
}

void RecMonitor::refreshRecMonitor(bool visible)
{
    if (visible) {
        //if (!m_isActive) activateMonitor();

    }
}

void RecMonitor::slotPlay()
{
    /*if (m_isPlaying) slotStopCapture();
    else slotStartPreview(true);*/
}

void RecMonitor::slotReadDvgrabInfo()
{
    QString data = m_captureProcess->readAllStandardError().simplified();
    data = data.section('"', 2, 2).simplified();
    m_dvinfo.setText(data.left(11));
    m_dvinfo.updateGeometry();
}

AbstractRender *RecMonitor::abstractRender()
{
    return m_captureDevice;
}


void RecMonitor::analyseFrames(bool analyse)
{
    m_analyse = analyse;
    if (m_captureDevice) m_captureDevice->sendFrameForAnalysis = analyse;
}

void RecMonitor::slotDroppedFrames(int dropped)
{
    slotSetInfoMessage(i18n("%1 dropped frames", dropped));
}

void RecMonitor::buildMltDevice(const QString &path)
{
    if (m_captureDevice == NULL) {
	m_monitorManager->updateScopeSource();
        m_captureDevice = new MltDeviceCapture(path, videoSurface, this);
        connect(m_captureDevice, SIGNAL(droppedFrames(int)), this, SLOT(slotDroppedFrames(int)));
        m_captureDevice->sendFrameForAnalysis = m_analyse;
        m_monitorManager->updateScopeSource();
    }
}

void RecMonitor::slotChangeRecordingPreview(bool enable)
{
    KdenliveSettings::setEnable_recording_preview(enable);
}


void RecMonitor::slotMouseSeek(int /*eventDelta*/, bool /*fast*/)
{
}


#include "recmonitor.moc"

