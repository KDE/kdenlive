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
#include "monitor.h"
#include "monitormanager.h"

#include "gentime.h"
#include "kdenlivesettings.h"
#include "capture/mltdevicecapture.h"
#include "capture/managecapturesdialog.h"
#include "dialogs/profilesdialog.h"
#include "glwidget.h"
#include <config-kdenlive.h>

#include "kdenlive_debug.h"
#include "klocalizedstring.h"

#include <KComboBox>
#include <KFileItem>
#include <KMessageBox>
#include <KDiskFreeSpaceInfo>

#include <QMouseEvent>
#include <QMenu>
#include <QFile>
#include <QDir>
#include <QPainter>
#include <QDesktopWidget>
#include <QStandardPaths>

RecMonitor::RecMonitor(Kdenlive::MonitorId name, MonitorManager *manager, QWidget *parent) :
    AbstractMonitor(name, manager, parent),
    m_isCapturing(false),
    m_didCapture(false),
    m_isPlaying(false),
    m_captureDevice(nullptr),
    m_analyse(false)
{
    setupUi(this);

    device_selector->setCurrentIndex(KdenliveSettings::defaultcapture());
    connect(device_selector, SIGNAL(currentIndexChanged(int)), this, SLOT(slotVideoDeviceChanged(int)));
    // Video widget holder
    QVBoxLayout *l = new QVBoxLayout;
    l->setContentsMargins(0, 0, 0, 0);
    l->setSpacing(0);
    video_frame->setLayout(l);

    QToolBar *toolbar = new QToolBar(this);
    QHBoxLayout *hlayout = new QHBoxLayout;
    hlayout->setContentsMargins(0, 0, 0, 0);
    m_playIcon = QIcon::fromTheme(QStringLiteral("media-playback-start"));
    m_pauseIcon = QIcon::fromTheme(QStringLiteral("media-playback-pause"));

    m_discAction = toolbar->addAction(QIcon::fromTheme(QStringLiteral("network-connect")), i18n("Connect"));
    connect(m_discAction, &QAction::triggered, this, &RecMonitor::slotDisconnect);

    m_rewAction = toolbar->addAction(QIcon::fromTheme(QStringLiteral("media-seek-backward")), i18n("Rewind"));
    connect(m_rewAction, &QAction::triggered, this, &RecMonitor::slotRewind);

    m_playAction = toolbar->addAction(m_playIcon, i18n("Play"));
    connect(m_playAction, &QAction::triggered, this, &RecMonitor::slotStartPreview);

    m_stopAction = toolbar->addAction(QIcon::fromTheme(QStringLiteral("media-playback-stop")), i18n("Stop"));
    connect(m_stopAction, &QAction::triggered, this, &RecMonitor::slotStopCapture);
    m_stopAction->setEnabled(false);
    m_fwdAction = toolbar->addAction(QIcon::fromTheme(QStringLiteral("media-seek-forward")), i18n("Forward"));
    connect(m_fwdAction, &QAction::triggered, this, &RecMonitor::slotForward);

    m_recAction = toolbar->addAction(QIcon::fromTheme(QStringLiteral("media-record")), i18n("Record"));
    connect(m_recAction, &QAction::triggered, this, &RecMonitor::slotRecord);
    m_recAction->setCheckable(true);

    rec_options->setIcon(QIcon::fromTheme(QStringLiteral("system-run")));
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

    QAction *configAction = toolbar->addAction(QIcon::fromTheme(QStringLiteral("configure")), i18n("Configure"));
    connect(configAction, &QAction::triggered, this, &RecMonitor::slotConfigure);
    configAction->setCheckable(false);

    hlayout->addWidget(toolbar);
    hlayout->addWidget(&m_logger);
    hlayout->addWidget(&m_dvinfo);
    m_logger.setMaxCount(10);
    m_logger.setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_logger.setFrame(false);
    //m_logger.setInsertPolicy(KComboBox::InsertAtTop);

    m_freeSpace = new KCapacityBar(KCapacityBar::DrawTextInline, this);
    m_freeSpace->setMaximumWidth(150);
    QFontMetricsF fontMetrics(font());
    m_freeSpace->setMaximumHeight(fontMetrics.height() * 1.2);
    hlayout->addWidget(m_freeSpace);
    connect(&m_spaceTimer, &QTimer::timeout, this, &RecMonitor::slotUpdateFreeSpace);
    m_spaceTimer.setInterval(30000);
    m_spaceTimer.setSingleShot(false);

    // Check number of monitors for FFmpeg screen capture
    int screens = QApplication::desktop()->screenCount();
    if (screens > 1) {
        for (int ix = 0; ix < screens; ix++) {
            monitor_box->addItem(i18n("Monitor %1", ix));
        }
    }
    // Update screen grab monitor choice in case we changed from fullscreen
    monitor_box->setEnabled(KdenliveSettings::grab_capture_type() == 0);

    control_frame_firewire->setLayout(hlayout);
    m_displayProcess = new QProcess;
    m_captureProcess = new QProcess;

    connect(m_captureProcess, &QProcess::stateChanged, this, &RecMonitor::slotProcessStatus);
    connect(m_captureProcess, &QProcess::readyReadStandardError, this, &RecMonitor::slotReadProcessInfo);

    qputenv("SDL_VIDEO_ALLOW_SCREENSAVER", "1");

    m_infoMessage = new KMessageWidget;
    QVBoxLayout *s =  static_cast <QVBoxLayout *>(layout());
    s->insertWidget(1, m_infoMessage);
    m_infoMessage->hide();

    slotVideoDeviceChanged(device_selector->currentIndex());
    m_previewSettings->setChecked(KdenliveSettings::enable_recording_preview());
    connect(m_previewSettings, &QAction::triggered, this, &RecMonitor::slotChangeRecordingPreview);
}

RecMonitor::~RecMonitor()
{
    m_spaceTimer.stop();
    delete m_captureProcess;
    delete m_displayProcess;
    delete m_captureDevice;
    delete m_infoMessage;
}

void RecMonitor::mouseDoubleClickEvent(QMouseEvent *)
{
}

void RecMonitor::slotSwitchFullScreen(bool minimizeOnly)
{
    Q_UNUSED(minimizeOnly)
}

void RecMonitor::stop()
{
    // Special case: when recording audio only, do not stop so that we can do voiceover.
    if (device_selector->currentIndex() == ScreenBag || (device_selector->currentIndex() == Video4Linux && !rec_video->isChecked())) {
        return;
    }
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
    if (KdenliveSettings::capturetoprojectfolder()) {
        m_capturePath = currentProjectFolder;
    } else {
        m_capturePath = KdenliveSettings::capturefolder();
    }
    if (m_captureProcess) {
        m_captureProcess->setWorkingDirectory(m_capturePath);
        if (m_captureProcess->state() != QProcess::NotRunning) {
            if (device_selector->currentIndex() == Firewire) {
                KMessageBox::information(this, i18n("You need to disconnect and reconnect in the capture monitor to apply your changes"), i18n("Capturing"));
            } else {
                KMessageBox::information(this, i18n("You need to stop capture before your changes can be applied"), i18n("Capturing"));
            }
        } else {
            slotVideoDeviceChanged(device_selector->currentIndex());
        }
    }
    // update free space info
    slotUpdateFreeSpace();
}

void RecMonitor::slotUpdateFullScreenGrab()
{
    // Update screen grab monitor choice in case we changed from fullscreen
    monitor_box->setEnabled(KdenliveSettings::grab_capture_type() == 0);
}

void RecMonitor::slotVideoDeviceChanged(int ix)
{
    QString capturefile;
    QString capturename;
    if (m_infoMessage->isVisible()) {
        m_infoMessage->animatedHide();
    }
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
        m_captureDevice = nullptr;
    }

    // The m_videoBox container has to be shown once before the MLT consumer is build, or preview will fail
    /*videoBox->setHidden(ix != Video4Linux && ix != BlackMagic);
    videoBox->setHidden(true);*/
    switch (ix) {
    case ScreenBag:
        m_discAction->setEnabled(false);
        m_rewAction->setEnabled(false);
        m_fwdAction->setEnabled(false);
        m_stopAction->setEnabled(false);
        m_playAction->setEnabled(false);
        if (KdenliveSettings::ffmpegpath().isEmpty()) {
            QString exepath = QStandardPaths::findExecutable(QStringLiteral("ffmpeg%1").arg(FFMPEG_SUFFIX));
            if (exepath.isEmpty()) {
                // Check for libav version
                exepath = QStandardPaths::findExecutable(QStringLiteral("avconv"));
            }
            if (exepath.isEmpty()) {
                showWarningMessage(i18n("ffmpeg or avconv not found,\n please install it for screen grabs"));
            } else {
                KdenliveSettings::setFfmpegpath(exepath);
            }
        }
        if (!KdenliveSettings::ffmpegpath().isEmpty()) {
            if (!Render::checkX11Grab()) {
                // FFmpeg does not support screen grab
                showWarningMessage(i18n("Your FFmpeg / Libav installation\n does not support screen grab"));
                m_recAction->setEnabled(false);
            } else {
                video_frame->setPixmap(mergeSideBySide(QIcon::fromTheme(QStringLiteral("video-display")).pixmap(QSize(50, 50)), i18n("Press record button\nto start screen capture\nFiles will be saved in:\n%1", m_capturePath)));
            }
        }
        //video_frame->setText(i18n("Press record button\nto start screen capture"));
        break;
    case Video4Linux:
        m_stopAction->setEnabled(false);
        m_playAction->setEnabled(true);
        checkDeviceAvailability();
        break;
    case BlackMagic:
        m_stopAction->setEnabled(false);
        m_playAction->setEnabled(true);
        capturefile = m_capturePath;
        if (!capturefile.endsWith(QLatin1Char('/'))) {
            capturefile.append(QLatin1Char('/'));
        }
        capturename = KdenliveSettings::decklink_filename();
        capturename.append(QStringLiteral("xxx."));
        capturename.append(KdenliveSettings::decklink_extension());
        capturefile.append(capturename);
        video_frame->setPixmap(mergeSideBySide(QIcon::fromTheme(QStringLiteral("camera-photo")).pixmap(QSize(50, 50)), i18n("Plug your camcorder and\npress play button\nto start preview.\nFiles will be saved in:\n%1", capturefile)));
        break;
    default: // FIREWIRE
        m_discAction->setEnabled(true);
        m_stopAction->setEnabled(false);
        m_playAction->setEnabled(false);
        m_rewAction->setEnabled(false);
        m_fwdAction->setEnabled(false);

        // Check that dvgab is available
        if (KdenliveSettings::dvgrab_path().isEmpty()) {
            QString dvgrabpath = QStandardPaths::findExecutable(QStringLiteral("dvgrab"));
            if (dvgrabpath.isEmpty()) {
                showWarningMessage(i18n("dvgrab utility not found,\n please install it for firewire capture"));
            } else {
                KdenliveSettings::setDvgrab_path(dvgrabpath);
            }
        } else {
            // Show capture info
            capturefile = m_capturePath;
            if (!capturefile.endsWith(QLatin1Char('/'))) {
                capturefile.append(QLatin1Char('/'));
            }
            capturename = KdenliveSettings::dvgrabfilename();
            if (capturename.isEmpty()) {
                capturename = QStringLiteral("capture");
            }
            QString extension;
            switch (KdenliveSettings::firewireformat()) {
            case 0:
                extension = QStringLiteral(".dv");
                break;
            case 1:
            case 2:
                extension = QStringLiteral(".avi");
                break;
            case 3:
                extension = QStringLiteral(".m2t");
                break;
            }
            capturename.append(QStringLiteral("xxx") + extension);
            capturefile.append(capturename);
            video_frame->setPixmap(mergeSideBySide(QIcon::fromTheme(QStringLiteral("network-connect")).pixmap(QSize(50, 50)), i18n("Plug your camcorder and\npress connect button\nto initialize connection\nFiles will be saved in:\n%1", capturefile)));
        }
        break;
    }
}

void RecMonitor::slotSetInfoMessage(const QString &message)
{
    m_logger.insertItem(0, message);
}

QPixmap RecMonitor::mergeSideBySide(const QPixmap &pix, const QString &txt)
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
    p.setPen(palette().text().color());
    p.drawText(QRect(pixWidth + 8, 0, strWidth, strHeight), 0, txt);
    p.end();
    return res;
}

void RecMonitor::checkDeviceAvailability()
{
    if (!QFile::exists(KdenliveSettings::video4vdevice())) {
        rec_video->setChecked(false);
        rec_video->setEnabled(false);
        video_frame->setPixmap(mergeSideBySide(QIcon::fromTheme(QStringLiteral("camera-web")).pixmap(QSize(50, 50)), i18n("Cannot read from device %1\nPlease check drivers and access rights.", KdenliveSettings::video4vdevice())));
    } else {
        rec_video->setEnabled(true);
        video_frame->setPixmap(mergeSideBySide(QIcon::fromTheme(QStringLiteral("camera-web")).pixmap(QSize(50, 50)), i18n("Press play or record button\nto start video capture\nFiles will be saved in:\n%1", m_capturePath)));
    }
}

void RecMonitor::slotDisconnect()
{
    if (m_captureProcess->state() == QProcess::NotRunning) {
        m_captureTime = QDateTime::currentDateTime();
        //qCDebug(KDENLIVE_LOG) << "CURRENT TIME: " << m_captureTime.toString();
        m_didCapture = false;
        slotStartPreview(false);
        m_discAction->setIcon(QIcon::fromTheme(QStringLiteral("network-disconnect")));
        m_discAction->setText(i18n("Disconnect"));
        m_recAction->setEnabled(true);
        m_stopAction->setEnabled(true);
        m_playAction->setEnabled(true);
        m_rewAction->setEnabled(true);
        m_fwdAction->setEnabled(true);
    } else {
        m_captureProcess->write("q", 1);
        QTimer::singleShot(1000, m_captureProcess, &QProcess::kill);
        if (m_didCapture) {
            manageCapturedFiles();
        }
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
    if (!m_isCapturing && !m_isPlaying) {
        return;
    }
    //videoBox->setHidden(true);
    control_frame->setEnabled(true);
    slotActivateMonitor();
    switch (device_selector->currentIndex()) {
    case Firewire:
        m_captureProcess->write("\e", 2);
        m_playAction->setIcon(m_playIcon);
        m_playAction->setToolTip(i18n("Play"));
        m_isPlaying = false;
        break;
    case ScreenBag:
        m_captureProcess->terminate();
        QTimer::singleShot(1500, m_captureProcess, &QProcess::kill);
        break;
    case Video4Linux:
    case BlackMagic:
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
        if (m_addCapturedClip->isChecked() && !m_captureFile.isEmpty() && QFile::exists(m_captureFile.toLocalFile())) {
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
        if (device_selector->currentIndex() == Firewire) {
            if (m_isPlaying) {
                m_captureProcess->write("k", 1);
                //captureProcess->write("\e", 2);
                m_playAction->setIcon(m_playIcon);
                m_playAction->setToolTip(i18n("Play"));
                m_isPlaying = false;
            } else {
                m_captureProcess->write("p", 1);
                m_playAction->setIcon(m_pauseIcon);
                m_playAction->setToolTip(i18n("Pause"));
                m_isPlaying = true;
            }
        }
        return;
    }
    slotActivateMonitor();
    if (m_isPlaying) {
        return;
    }
    m_captureArgs.clear();
    m_displayArgs.clear();
    m_isPlaying = false;
    QString capturename = KdenliveSettings::dvgrabfilename();
    QString path;
    MltVideoProfile profile;
    QString producer;
    QStringList dvargs = KdenliveSettings::dvgrabextra().simplified().split(' ', QString::SkipEmptyParts);
    int ix = device_selector->currentIndex();
    bool isXml;
    if (ix != Video4Linux && ix != BlackMagic && ix != Firewire) {
        // no need for sdl preview
    } else {
    }
    switch (ix) {
    case Firewire:
        switch (KdenliveSettings::firewireformat()) {
        case 0:
            // RAW DV CAPTURE
            m_captureArgs << QStringLiteral("--format") << QStringLiteral("raw");
            m_displayArgs << QStringLiteral("-f") << QStringLiteral("dv");
            break;
        case 1:
            // DV type 1
            m_captureArgs << QStringLiteral("--format") << QStringLiteral("dv1");
            m_displayArgs << QStringLiteral("-f") << QStringLiteral("dv");
            break;
        case 2:
            // DV type 2
            m_captureArgs << QStringLiteral("--format") << QStringLiteral("dv2");
            m_displayArgs << QStringLiteral("-f") << QStringLiteral("dv");
            break;
        case 3:
            // HDV CAPTURE
            m_captureArgs << QStringLiteral("--format") << QStringLiteral("hdv");
            m_displayArgs << QStringLiteral("-f") << QStringLiteral("mpegts");
            break;
        }
        if (KdenliveSettings::firewireautosplit()) {
            m_captureArgs << QStringLiteral("--autosplit");
        }
        if (KdenliveSettings::firewiretimestamp()) {
            m_captureArgs << QStringLiteral("--timestamp");
        }
        if (!dvargs.isEmpty()) {
            m_captureArgs << dvargs;
        }
        m_captureArgs << QStringLiteral("-i");
        if (capturename.isEmpty()) {
            capturename = QStringLiteral("capture");
        }
        m_captureArgs << capturename << QStringLiteral("-");

        m_displayArgs << QStringLiteral("-x") << QString::number(video_frame->width()) << QStringLiteral("-y") << QString::number(video_frame->height()) << QStringLiteral("-noframedrop") << QStringLiteral("-");

        m_captureProcess->setStandardOutputProcess(m_displayProcess);
        m_captureProcess->setWorkingDirectory(m_capturePath);
        //qCDebug(KDENLIVE_LOG) << "Capture: Running dvgrab " << m_captureArgs.join(" ");

        m_captureProcess->start(KdenliveSettings::dvgrab_path(), m_captureArgs);
        if (play) {
            m_captureProcess->write(" ", 1);
        }
        m_discAction->setEnabled(true);
        break;
    case Video4Linux:
        path = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + QStringLiteral("/profiles/video4linux");
        buildMltDevice(path);
        profile = ProfilesDialog::getVideoProfile(path);
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

        break;
    case BlackMagic:
        path = KdenliveSettings::current_profile();
        slotActivateMonitor();
        buildMltDevice(path);
        producer = QStringLiteral("decklink:%1").arg(KdenliveSettings::decklink_capturedevice());
        if (!m_captureDevice->slotStartPreview(producer)) {
            // v4l capture failed to start
            video_frame->setText(i18n("Failed to start Decklink,\ncheck your parameters..."));

        } else {
            m_playAction->setEnabled(false);
            m_stopAction->setEnabled(true);
            m_isPlaying = true;
        }
        break;
    default:
        break;
    }

    control_frame->setEnabled(false);

    if (device_selector->currentIndex() == Firewire) {
        //qCDebug(KDENLIVE_LOG) << "Capture: Running ffplay " << m_displayArgs.join(" ");
        m_displayProcess->start(KdenliveSettings::ffplaypath(), m_displayArgs);
        //video_frame->setText(i18n("Initialising..."));
    } else {
        // do something when starting screen grab
    }
}

void RecMonitor::slotRecord()
{
    m_error.clear();
    video_frame->clear();// clear error text from previous run if any.
    // ^^ This does clear the error text but the change is not visible straight way in the UI
    // it only becomes visible after the capture is complete. Vincent and JBM - how come? What can
    // we do to make the error text go way as soon as a new capture starts. It is missleading to the user
    // to have the error still display while we are happily capturing.
    if (m_captureProcess->state() == QProcess::NotRunning && device_selector->currentIndex() == Firewire) {
        slotStartPreview();
    }
    if (m_isCapturing) {
        // User stopped capture
        slotStopCapture();
        return;
    } else if (device_selector->currentIndex() == Firewire) {
        m_isCapturing = true;
        m_didCapture = true;
        m_captureProcess->write("c\n", 3);
        m_spaceTimer.start();
        return;
    }
    if (m_captureProcess->state() == QProcess::NotRunning) {
        m_logger.clear();
        m_recAction->setChecked(true);
        QString extension = QStringLiteral("mpg");
        if (device_selector->currentIndex() == ScreenBag) {
            extension = KdenliveSettings::grab_extension();
        } else if (device_selector->currentIndex() == Video4Linux) {
            // TODO: when recording audio only, allow configuration?
            if (!rec_video->isChecked()) {
                extension = QStringLiteral("wav");
            } else {
                extension = KdenliveSettings::v4l_extension();
            }
        } else if (device_selector->currentIndex() == BlackMagic) {
            extension = KdenliveSettings::decklink_extension();
        }
        QString path = QUrl(m_capturePath).toLocalFile() + QDir::separator() + QStringLiteral("capture0000.") + extension;
        int i = 1;
        while (QFile::exists(path)) {
            QString num = QString::number(i).rightJustified(4, '0', false);
            path = QUrl(m_capturePath).toLocalFile() + QDir::separator() + QStringLiteral("capture") + num + QLatin1Char('.') + extension;
            ++i;
        }
        m_captureFile = QUrl(path);

        m_captureArgs.clear();
        m_displayArgs.clear();
        QString playlist;
        QString v4lparameters;
        MltVideoProfile profile;
        bool showPreview;
        bool isXml;
        QString captureSize;
        int screen = -1;
        if (monitor_box->count() > 0) {
            // Multi monitor setup, capture monitor selected by user
            screen = monitor_box->currentIndex();
        }
        QRect screenSize = QApplication::desktop()->screenGeometry(screen);
        QString capturename = KdenliveSettings::dvgrabfilename();
        if (capturename.isEmpty()) {
            capturename = QStringLiteral("capture");
        }

        switch (device_selector->currentIndex()) {
        case Video4Linux: //ffmpeg capture
            if (rec_video->isChecked()) {
                slotActivateMonitor();
            }
            path = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + QStringLiteral("/profiles/video4linux");
            profile = ProfilesDialog::getVideoProfile(path);
            buildMltDevice(path);
            playlist = getV4lXmlPlaylist(profile, &isXml);

            v4lparameters = KdenliveSettings::v4l_parameters();

            // TODO: when recording audio only, allow param configuration?
            if (!rec_video->isChecked()) {
                v4lparameters.clear();
            }

            // Add alsa audio capture
            if (!rec_audio->isChecked()) {
                // if we do not want audio, make sure that we don't have audio encoding parameters
                // this is required otherwise the MLT avformat consumer will not close properly
                if (v4lparameters.contains(QStringLiteral("acodec"))) {
                    QString endParam = v4lparameters.section(QStringLiteral("acodec"), 1);
                    int vcodec = endParam.indexOf(QStringLiteral(" vcodec"));
                    int format = endParam.indexOf(QStringLiteral(" f="));
                    int cutPosition = -1;
                    if (vcodec > -1) {
                        if (format  > -1) {
                            cutPosition = qMin(vcodec, format);
                        } else {
                            cutPosition = vcodec;
                        }
                    } else if (format  > -1) {
                        cutPosition = format;
                    } else {
                        // nothing interesting in end params
                        endParam.clear();
                    }
                    if (cutPosition > -1) {
                        endParam.remove(0, cutPosition);
                    }
                    v4lparameters = QString(v4lparameters.section(QStringLiteral("acodec"), 0, 0) + QStringLiteral("an=1 ") + endParam).simplified();
                }
            }

            showPreview = m_previewSettings->isChecked();
            if (!rec_video->isChecked()) {
                showPreview = false;
            }

            if (m_captureDevice->slotStartCapture(v4lparameters, m_captureFile.toLocalFile(), playlist, showPreview, isXml)) {
                m_isCapturing = true;
                m_recAction->setEnabled(false);
                m_stopAction->setEnabled(true);
                m_previewSettings->setEnabled(false);
                control_frame->setEnabled(false);
            } else {
                video_frame->setText(i18n("Failed to start ffmpeg capture,\ncheck your parameters..."));
                m_recAction->blockSignals(true);
                m_recAction->setChecked(false);
                m_recAction->blockSignals(false);
                m_isCapturing = false;
            }
            break;

        case BlackMagic:// BlackMagic capture
            slotActivateMonitor();
            path = KdenliveSettings::current_profile();
            profile = ProfilesDialog::getVideoProfile(path);
            buildMltDevice(path);

            playlist = QStringLiteral("<producer id=\"producer0\" in=\"0\" out=\"99999\"><property name=\"mlt_type\">producer</property><property name=\"length\">100000</property><property name=\"eof\">pause</property><property name=\"resource\">%1</property><property name=\"mlt_service\">decklink</property></producer>").arg(KdenliveSettings::decklink_capturedevice());

            if (m_captureDevice->slotStartCapture(KdenliveSettings::decklink_parameters(), m_captureFile.toLocalFile(), QStringLiteral("decklink:%1").arg(KdenliveSettings::decklink_capturedevice()), m_previewSettings->isChecked(), false)) {
                m_isCapturing = true;
                slotSetInfoMessage(i18n("Capturing to %1", m_captureFile.fileName()));
                m_recAction->setEnabled(false);
                m_stopAction->setEnabled(true);
                m_previewSettings->setEnabled(false);
            } else {
                video_frame->setText(i18n("Failed to start Decklink,\ncheck your parameters..."));
                slotSetInfoMessage(i18n("Failed to start capture"));
                m_isCapturing = false;
            }
            break;

        case ScreenBag:// screen grab capture
            m_captureArgs << QStringLiteral("-f") << QStringLiteral("x11grab");
            if (KdenliveSettings::grab_follow_mouse()) {
                m_captureArgs << QStringLiteral("-follow_mouse") << QStringLiteral("centered");
            }
            if (!KdenliveSettings::grab_hide_frame()) {
                m_captureArgs << QStringLiteral("-show_region") << QStringLiteral("1");
            }
            captureSize = QStringLiteral(":0.0");
            if (KdenliveSettings::grab_capture_type() == 0) {
                // Full screen capture
                m_captureArgs << QStringLiteral("-s") << QString::number(screenSize.width()) + QLatin1Char('x') + QString::number(screenSize.height());
                captureSize.append(QLatin1Char('+') + QString::number(screenSize.left()) + QLatin1Char('.') + QString::number(screenSize.top()));
            } else {
                // Region capture
                m_captureArgs << QStringLiteral("-s") << QString::number(KdenliveSettings::grab_width()) + QLatin1Char('x') + QString::number(KdenliveSettings::grab_height());
                captureSize.append(QLatin1Char('+') + QString::number(KdenliveSettings::grab_offsetx()) + QLatin1Char('.') + QString::number(KdenliveSettings::grab_offsetx()));
            }
            // fps
            m_captureArgs << QStringLiteral("-r") << QString::number(KdenliveSettings::grab_fps());
            if (KdenliveSettings::grab_hide_mouse()) {
                captureSize.append(QStringLiteral("+nomouse"));
            }
            m_captureArgs << QStringLiteral("-i") << captureSize;
            if (!KdenliveSettings::grab_parameters().simplified().isEmpty()) {
                m_captureArgs << KdenliveSettings::grab_parameters().simplified().split(QLatin1Char(' '));
            }
            m_captureArgs << path;

            m_isCapturing = true;
            m_recAction->setEnabled(false);
            /*if (KdenliveSettings::rmd_capture_audio()) {
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
            } else m_captureArgs << "--no-sound";*/

            m_captureProcess->start(KdenliveSettings::ffmpegpath(), m_captureArgs);
            if (!m_captureProcess->waitForStarted()) {
                // Problem launching capture app
                showWarningMessage(i18n("Failed to start the capture application:\n%1", KdenliveSettings::ffmpegpath()));
            }
            ////qCDebug(KDENLIVE_LOG) << "// Screen grab params: " << m_captureArgs;
            break;
        default:
            break;
        }

        if (device_selector->currentIndex() == Firewire) {
            m_isCapturing = true;
            //qCDebug(KDENLIVE_LOG) << "Capture: Running ffplay " << m_displayArgs.join(" ");
            m_displayProcess->start(KdenliveSettings::ffplaypath(), m_displayArgs);
            video_frame->setText(i18n("Initialising..."));
        }
    } else {
        // stop capture
        m_displayProcess->kill();
        //captureProcess->kill();
        QTimer::singleShot(1000, this, &RecMonitor::slotRecord);
    }
}

void RecMonitor::showWarningMessage(const QString &text, bool logAction)
{
    m_infoMessage->setText(text);
    m_infoMessage->setMessageType(KMessageWidget::Warning);
    if (logAction) {
        QAction *manualAction = new QAction(i18n("Show log"), this);
        connect(manualAction, &QAction::triggered, this, &RecMonitor::slotShowLog);
        m_infoMessage->addAction(manualAction);
    }

    if (isVisible()) {
        m_infoMessage->animatedShow();
    }
}

const QString RecMonitor::getV4lXmlPlaylist(const MltVideoProfile &profile, bool *isXml)
{
    QString playlist;
    if (rec_video->isChecked() && rec_audio->isChecked()) {
        // We want to capture audio and video, use xml playlist
        *isXml = true;
        playlist = QStringLiteral("<mlt title=\"capture\" LC_NUMERIC=\"C\"><profile description=\"v4l\" width=\"%1\" height=\"%2\" progressive=\"%3\" sample_aspect_num=\"%4\" sample_aspect_den=\"%5\" display_aspect_num=\"%6\" display_aspect_den=\"%7\" frame_rate_num=\"%8\" frame_rate_den=\"%9\" colorspace=\"%10\"/>").arg(profile.width).arg(profile.height).arg(profile.progressive).arg(profile.sample_aspect_num).arg(profile.sample_aspect_den).arg(profile.display_aspect_num).arg(profile.display_aspect_den).arg(profile.frame_rate_num).arg(profile.frame_rate_den).arg(profile.colorspace);

        playlist.append(QStringLiteral("<producer id=\"producer0\" in=\"0\" out=\"999999\"><property name=\"mlt_type\">producer</property><property name=\"length\">1000000</property><property name=\"eof\">loop</property><property name=\"resource\">video4linux2:%1?width:%2&amp;height:%3&amp;frame_rate:%4</property><property name=\"mlt_service\">avformat-novalidate</property></producer><playlist id=\"playlist0\"><entry producer=\"producer0\" in=\"0\" out=\"999999\"/></playlist>").arg(KdenliveSettings::video4vdevice()).arg(profile.width).arg(profile.height).arg((double) profile.frame_rate_num / profile.frame_rate_den));

        playlist.append(QStringLiteral("<producer id=\"producer1\" in=\"0\" out=\"999999\"><property name=\"mlt_type\">producer</property><property name=\"length\">1000000</property><property name=\"resource\">alsa:%1?channels=%2</property><property name=\"audio_index\">0</property><property name=\"video_index\">-1</property><property name=\"mlt_service\">avformat-novalidate</property></producer><playlist id=\"playlist1\"><entry producer=\"producer1\" in=\"0\" out=\"999999\"/></playlist>").arg(KdenliveSettings::v4l_alsadevicename()).arg(KdenliveSettings::alsachannels()));

        playlist.append(QStringLiteral("<tractor id=\"tractor0\" title=\"video0\" global_feed=\"1\" in=\"0\" out=\"999999\">"));
        playlist.append(QStringLiteral("<track producer=\"playlist0\"/><track producer=\"playlist1\"/>"));
        playlist.append(QStringLiteral("</tractor></mlt>"));
    } else if (rec_audio->isChecked()) {
        // Audio only recording
        *isXml = false;
        playlist = QStringLiteral("alsa:%1?channels=%2").arg(KdenliveSettings::v4l_alsadevicename()).arg(KdenliveSettings::alsachannels());
    } else {
        // Video only recording
        *isXml = false;
        playlist = QStringLiteral("video4linux2:%1?width:%2&amp;height:%3&amp;frame_rate:%4").arg(KdenliveSettings::video4vdevice()).arg(profile.width).arg(profile.height).arg((double) profile.frame_rate_num / profile.frame_rate_den);

    }

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
    QString args = KdenliveSettings::screengrabcapture().replace("%size", QString::number(width) + QLatin1Char('x') + QString::number(height)).replace("%offset", "+" + QString::number(rect.x()) + QLatin1Char(',') + QString::number(rect.y()));
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
        //qCDebug(KDENLIVE_LOG) << "Capture: Running arecord " << alsaArgs.join(" ");
        alsaProcess->start("arecord", alsaArgs);
    }
    //qCDebug(KDENLIVE_LOG) << "Capture: Running ffmpeg " << m_captureArgs.join(" ");
    captureProcess->start(KdenliveSettings::ffmpegpath(), m_captureArgs);
}*/

void RecMonitor::slotProcessStatus(QProcess::ProcessState status)
{
    if (status == QProcess::NotRunning) {
        m_displayProcess->kill();
        if (m_isCapturing && device_selector->currentIndex() != Firewire)
            if (m_addCapturedClip->isChecked() && !m_captureFile.isEmpty() && QFile::exists(m_captureFile.toLocalFile())) {
                emit addProjectClip(m_captureFile);
                m_captureFile.clear();
            }
        if (device_selector->currentIndex() == Firewire) {
            m_discAction->setIcon(QIcon::fromTheme(QStringLiteral("network-connect")));
            m_discAction->setText(i18n("Connect"));
            m_playAction->setEnabled(false);
            m_rewAction->setEnabled(false);
            m_fwdAction->setEnabled(false);
            m_recAction->setEnabled(false);
        } else {
            m_recAction->setEnabled(true);
        }
        m_isPlaying = false;
        m_playAction->setIcon(m_playIcon);
        m_playAction->setToolTip(i18n("Play"));
        m_recAction->setChecked(false);
        m_stopAction->setEnabled(false);
        device_selector->setEnabled(true);
        if (m_captureProcess) {
            if (m_captureProcess->exitStatus() == QProcess::CrashExit) {
                showWarningMessage(i18n("Capture crashed, please check your parameters"), true);
            } else {
                if (device_selector->currentIndex() != ScreenBag) {
                    video_frame->setText(i18n("Not connected"));
                } else {
                    int code = m_captureProcess->exitCode();
                    if (code != 0 && code != 255) {
                        showWarningMessage(i18n("Capture crashed, please check your parameters"), true);
                    } else {
                        video_frame->setPixmap(mergeSideBySide(QIcon::fromTheme(QStringLiteral("video-display")).pixmap(QSize(50, 50)), i18n("Press record button\nto start screen capture\nFiles will be saved in:\n%1", m_capturePath)));
                    }
                }
            }
        }
        m_isCapturing = false;

        m_spaceTimer.stop();
        // update free space info
        slotUpdateFreeSpace();
    } else {
        if (device_selector->currentIndex()) {
            m_stopAction->setEnabled(true);
        }
        device_selector->setEnabled(false);
    }
}

void RecMonitor::manageCapturedFiles()
{
    QString extension;
    switch (KdenliveSettings::firewireformat()) {
    case 0:
        extension = QStringLiteral(".dv");
        break;
    case 1:
    case 2:
        extension = QStringLiteral(".avi");
        break;
    case 3:
        extension = QStringLiteral(".m2t");
        break;
    }
    QDir dir(m_capturePath);
    QStringList filters;
    QString capturename = KdenliveSettings::dvgrabfilename();
    if (capturename.isEmpty()) {
        capturename = QStringLiteral("capture");
    }
    filters << capturename + QLatin1Char('*') + extension;
    const QStringList result = dir.entryList(filters, QDir::Files, QDir::Time);
    QList<QUrl> capturedFiles;
    for (const QString &name : result) {
        QUrl url = QUrl::fromLocalFile(dir.absoluteFilePath(name));
        if (QFile::exists(url.toLocalFile())) {
            KFileItem file(url);
            file.setDelayedMimeTypes(true);
            if (file.time(KFileItem::ModificationTime) > m_captureTime) {
                // The file was captured in the last batch
                if (url.fileName().contains(QLatin1Char(':'))) {
                    // Several dvgrab options (--timecode,...) use : in the file name, which is
                    // not supported by MLT, so rename them
                    QString newUrl = url.adjusted(QUrl::RemoveFilename).toLocalFile() + QDir::separator() + url.fileName().replace(QLatin1Char(':'), '_');
                    if (QFile::rename(url.toLocalFile(), newUrl)) {
                        url = QUrl::fromLocalFile(newUrl);
                    }

                }
                capturedFiles.append(url);
            }
        }
    }
    //qCDebug(KDENLIVE_LOG) << "Found : " << capturedFiles.count() << " new capture files";
    //qCDebug(KDENLIVE_LOG) << capturedFiles;

    if (!capturedFiles.isEmpty()) {
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
    if (m_freeSpace->underMouse()) {
        slotUpdateFreeSpace();
    } else {
        QWidget::mousePressEvent(event);
    }
}

void RecMonitor::slotUpdateFreeSpace()
{
    if (m_capturePath.isEmpty()) {
        return;
    }
    KDiskFreeSpaceInfo info = KDiskFreeSpaceInfo::freeSpaceInfo(m_capturePath);
    if (info.isValid() && info.size() > 0) {
        m_freeSpace->setValue(100 * info.used() / info.size());
        m_freeSpace->setText(i18n("Free space: %1", KIO::convertSize(info.available())));
        m_freeSpace->update();
    }
}

void RecMonitor::slotPlay()
{
    /*if (m_isPlaying) slotStopCapture();
    else slotStartPreview(true);*/
}

void RecMonitor::slotReadProcessInfo()
{
    QString data = m_captureProcess->readAllStandardError().simplified();
    if (device_selector->currentIndex() == ScreenBag) {
        m_error.append(data + QLatin1Char('\n'));
    } else if (device_selector->currentIndex() == Firewire) {
        data = data.section(QLatin1Char('"'), 2, 2).simplified();
        m_dvinfo.setText(data.left(11));
        m_dvinfo.updateGeometry();
    }
}

void RecMonitor::slotShowLog()
{
    KMessageBox::information(this, m_error);
}

AbstractRender *RecMonitor::abstractRender()
{
    return m_captureDevice;
}

void RecMonitor::analyseFrames(bool analyze)
{
    m_analyse = analyze;
    if (m_captureDevice) {
        m_captureDevice->sendFrameForAnalysis = analyze;
    }
}

void RecMonitor::slotDroppedFrames(int dropped)
{
    slotSetInfoMessage(i18n("%1 dropped frames", dropped));
}

void RecMonitor::buildMltDevice(const QString &path)
{
    //TODO
    Q_UNUSED(path)
    if (m_captureDevice == nullptr) {
        m_monitorManager->updateScopeSource();
        //TODO
        /*m_captureDevice = new MltDeviceCapture(path, videoSurface, this);
        connect(m_captureDevice, &MltDeviceCapture::droppedFrames, this, &RecMonitor::slotDroppedFrames);
        m_captureDevice->sendFrameForAnalysis = m_analyse;
        */
        m_monitorManager->updateScopeSource();
    }
}

void RecMonitor::slotChangeRecordingPreview(bool enable)
{
    KdenliveSettings::setEnable_recording_preview(enable);
}

void RecMonitor::slotMouseSeek(int /*eventDelta*/, int /*fast*/)
{
}

