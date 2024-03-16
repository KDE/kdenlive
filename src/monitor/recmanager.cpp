/*
    SPDX-FileCopyrightText: 2015 Jean-Baptiste Mardelle <jb@kdenlive.org>
    This file is part of Kdenlive. See www.kdenlive.org.

    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "recmanager.h"
//#include "capture/mltdevicecapture.h"
#include "core.h"
#include "dialogs/profilesdialog.h"
#include "kdenlivesettings.h"
#include "monitor.h"

#include "klocalizedstring.h"
#include <KMessageBox>

#include <QCheckBox>
#include <QComboBox>
#include <QDir>
#include <QFile>
#include <QMenu>
#include <QScreen>
#include <QStandardPaths>
#include <QToolBar>
#include <QToolButton>
#include <QWidgetAction>

RecManager::RecManager(Monitor *parent)
    : QObject(parent)
    , m_monitor(parent)
    , m_recToolbar(new QToolBar(parent))
{
    m_playAction = m_recToolbar->addAction(QIcon::fromTheme(QStringLiteral("media-playback-start")), i18n("Preview"));
    m_playAction->setCheckable(true);
    connect(m_playAction, &QAction::toggled, this, &RecManager::slotPreview);

    m_recAction = new QAction(QIcon::fromTheme(QStringLiteral("media-record")), i18n("Record"));
    m_recAction->setCheckable(true);
    connect(m_recAction, &QAction::toggled, this, &RecManager::slotRecord);

    m_showLogAction = new QAction(i18n("Show log"), this);
    connect(m_showLogAction, &QAction::triggered, this, &RecManager::slotShowLog);

    m_recVideo = new QCheckBox(i18n("Video"));
    m_recAudio = new QCheckBox(i18n("Audio"));
    m_recToolbar->addWidget(m_recVideo);
    m_recToolbar->addWidget(m_recAudio);
    m_recAudio->setChecked(KdenliveSettings::v4l_captureaudio());
    m_recVideo->setChecked(KdenliveSettings::v4l_capturevideo());

    QWidget *spacer = new QWidget(parent);
    spacer->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Preferred);
    m_recToolbar->addWidget(spacer);

    m_audio_device = new QComboBox(parent);
    QStringList audioDevices = pCore->getAudioCaptureDevices();
    m_audio_device->addItems(audioDevices);
    connect(m_audio_device, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this, &RecManager::slotAudioDeviceChanged);
    QString selectedDevice = KdenliveSettings::defaultaudiocapture();
    int selectedIndex = m_audio_device->findText(selectedDevice);
    if (!selectedDevice.isNull() && selectedIndex > -1) {
        m_audio_device->setCurrentIndex(selectedIndex);
    }
    m_recToolbar->addWidget(m_audio_device);

    m_audioCaptureSlider = new QSlider(Qt::Vertical);
    m_audioCaptureSlider->setRange(0, 100);
    m_audioCaptureSlider->setValue(KdenliveSettings::audiocapturevolume());
    connect(m_audioCaptureSlider, &QSlider::valueChanged, this, &RecManager::slotSetVolume);
    auto *widgetslider = new QWidgetAction(parent);
    widgetslider->setText(i18n("Audio Capture Volume"));
    widgetslider->setDefaultWidget(m_audioCaptureSlider);
    auto *menu = new QMenu(parent);
    menu->addAction(widgetslider);
    m_audioCaptureButton = new QToolButton(parent);
    m_audioCaptureButton->setMenu(menu);
    m_audioCaptureButton->setToolTip(i18n("Audio Capture Volume"));
    m_audioCaptureButton->setPopupMode(QToolButton::InstantPopup);
    QIcon icon;
    if (KdenliveSettings::audiocapturevolume() == 0) {
        icon = QIcon::fromTheme(QStringLiteral("audio-volume-muted"));
    } else {
        icon = QIcon::fromTheme(QStringLiteral("audio-volume-medium"));
    }
    m_audioCaptureButton->setIcon(icon);
    m_recToolbar->addWidget(m_audioCaptureButton);
    m_recToolbar->addSeparator();

    m_device_selector = new QComboBox(parent);
    // TODO: re-implement firewire / decklink capture
    // m_device_selector->addItems(QStringList() << i18n("Firewire") << i18n("Webcam") << i18n("Screen Grab") << i18n("Blackmagic Decklink"));
    m_device_selector->addItem(i18n("Webcam"), Video4Linux);
    m_device_selector->addItem(i18n("Screen Grab"), ScreenGrab);
    selectedIndex = m_device_selector->findData(KdenliveSettings::defaultcapture());

    if (selectedIndex > -1) {
        m_device_selector->setCurrentIndex(selectedIndex);
    }
    connect(m_device_selector, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this, &RecManager::slotVideoDeviceChanged);
    m_recToolbar->addWidget(m_device_selector);

    QAction *configureRec = m_recToolbar->addAction(QIcon::fromTheme(QStringLiteral("configure")), i18n("Configure Recording"));
    connect(configureRec, &QAction::triggered, this, &RecManager::showRecConfig);
    m_recToolbar->addSeparator();
    m_switchRec = m_recToolbar->addAction(QIcon::fromTheme(QStringLiteral("list-add")), i18n("Show Record Control"));
    m_switchRec->setCheckable(true);
    connect(m_switchRec, &QAction::toggled, m_monitor, &Monitor::slotSwitchRec);
    m_recToolbar->setVisible(false);
    slotVideoDeviceChanged();
    connect(m_monitor, &Monitor::screenChanged, this, &RecManager::slotSetScreen);
}

RecManager::~RecManager() = default;

void RecManager::showRecConfig()
{
    Q_EMIT pCore->showConfigDialog(Kdenlive::PageCapture, m_device_selector->currentData().toInt());
}

QToolBar *RecManager::toolbar() const
{
    return m_recToolbar;
}

QAction *RecManager::recAction() const
{
    return m_recAction;
}

void RecManager::stopCapture()
{
    if (m_captureProcess) {
        slotRecord(false);
    } else if (pCore->getMediaCaptureState() == 1 && (m_checkAudio || m_checkVideo)) {
        // QMediaRecorder::RecordingState value is 1
        pCore->stopMediaCapture(-1, m_checkAudio, m_checkVideo);
        m_monitor->slotOpenClip(nullptr);
    }
}

void RecManager::stop()
{
    if (m_captureProcess) {
        // Don't stop screen rec when hiding rec toolbar
    } else {
        stopCapture();
        m_switchRec->setChecked(false);
    }
    toolbar()->setVisible(false);
}

void RecManager::slotRecord(bool record)
{
    /*if (m_device_selector->currentData().toInt() == Video4Linux) {
        if (record) {
            m_checkAudio = m_recAudio->isChecked();
            m_checkVideo = m_recVideo->isChecked();
            pCore->startMediaCapture(m_checkAudio, m_checkVideo);
        } else {
            stopCapture();
        }
        return;
    }*/
    if (!record) {
        if (!m_captureProcess) {
            return;
        }
        m_captureProcess->write("q");
        if (!m_captureProcess->waitForFinished()) {
            m_captureProcess->terminate();
            QTimer::singleShot(1500, m_captureProcess, &QProcess::kill);
        }
        return;
    }
    if (m_captureProcess) {
        return;
    }
    m_recError.clear();

    QString extension = KdenliveSettings::grab_extension();
    QDir captureFolder = QDir(pCore->getProjectCaptureFolderName());

    if (!captureFolder.exists()) {
        // This returns false if it fails, but we'll just let the whole recording fail instead
        captureFolder.mkpath(".");
    }

    QFileInfo checkCaptureFolder(captureFolder.absolutePath());
    if (!checkCaptureFolder.isWritable()) {
        Q_EMIT warningMessage(i18n("The directory %1, could not be created.\nPlease "
                                   "make sure you have the required permissions.",
                                   captureFolder.absolutePath()));
        m_recAction->blockSignals(true);
        m_recAction->setChecked(false);
        m_recAction->blockSignals(false);
        return;
    }

    m_captureProcess = new QProcess;
    connect(m_captureProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this, &RecManager::slotProcessStatus);
    connect(m_captureProcess, &QProcess::readyReadStandardError, this, &RecManager::slotReadProcessInfo);

    QString path = captureFolder.absoluteFilePath("capture0000." + extension);
    int i = 1;
    while (QFile::exists(path)) {
        QString num = QString::number(i).rightJustified(4, '0', false);
        path = captureFolder.absoluteFilePath("capture" + num + QLatin1Char('.') + extension);
        ++i;
    }
    m_captureFile = QUrl::fromLocalFile(path);
    QStringList captureArgs;
#ifdef Q_OS_WIN
    captureArgs << QStringLiteral("-f") << QStringLiteral("gdigrab") << QStringLiteral("-framerate") << QString::number(KdenliveSettings::grab_fps())
                << QStringLiteral("-i") << QStringLiteral("desktop");
    if (KdenliveSettings::grab_parameters().contains(QLatin1String("alsa"))) {
        // Add audio device
        QString params = captureArgs.join(QLatin1Char(' '));
        params.append(QStringLiteral(" ") + KdenliveSettings::grab_parameters().simplified());
        // Use Windows dshow for audio capture
        params.replace(QLatin1String("alsa"), QStringLiteral("dshow"));
        // Remove vorbis codec
        params.replace(QLatin1String("-acodec libvorbis"), QString());

        // Find first audio device
        QProcess tst;
        tst.setProcessChannelMode(QProcess::MergedChannels);
        tst.start(KdenliveSettings::ffmpegpath(), {"-hide_banner", "-list_devices", "true", "-f", "dshow", "-i", "dummy"});
        tst.waitForStarted();
        tst.waitForFinished();
        QString dshowOutput = QString::fromUtf8(tst.readAllStandardOutput());
        // KMessageBox::information(QApplication::activeWindow(), dshowOutput);
        if (dshowOutput.contains(QLatin1String("DirectShow audio devices"))) {
            dshowOutput = dshowOutput.section(QLatin1String("DirectShow audio devices"), 1);
            qDebug() << "GOT FILTERED DSOW1: " << dshowOutput;
            // dshowOutput = dshowOutput.section(QLatin1Char('"'), 3, 3);
            if (dshowOutput.contains(QLatin1Char('"'))) {
                dshowOutput = QString("audio=\"%1\"").arg(dshowOutput.section(QLatin1Char('"'), 1, 1));
                qDebug().noquote() << "GOT FILTERED DSOW2: " << dshowOutput;
                // captureArgs << params.split(QLatin1Char(' '));
                // captureArgs.replace(captureArgs.indexOf(QLatin1String("default")), dshowOutput);
                params.replace(QLatin1String("default"), dshowOutput);
            }
        } else {
            qDebug() << KdenliveSettings::ffmpegpath() << "=== GOT DSHOW DEVICES: " << dshowOutput;
            captureArgs << params.split(QLatin1Char(' '));
        }
        params.replace(QStringLiteral("\\\\"), QStringLiteral("\\"));
        params.replace(QStringLiteral("\\\""), QStringLiteral("\""));
        qDebug().noquote() << "== STARTING WIN CAPTURE: " << params << "\n___________";
        params.append(QStringLiteral(" ") + path);
        qDebug().noquote() << "== STARTING WIN CAPTURE: " << params << "\n___________";
        m_captureProcess->setNativeArguments(params);
        m_captureProcess->start(KdenliveSettings::ffmpegpath(), {});
    } else if (!KdenliveSettings::grab_parameters().simplified().isEmpty()) {
        captureArgs << KdenliveSettings::grab_parameters().simplified().split(QLatin1Char(' '));
        captureArgs << path;
        m_captureProcess->start(KdenliveSettings::ffmpegpath(), captureArgs);
    }
#else
    captureArgs << QStringLiteral("-f") << QStringLiteral("x11grab");
    if (KdenliveSettings::grab_follow_mouse()) {
        captureArgs << QStringLiteral("-follow_mouse") << QStringLiteral("centered");
    }
    if (!KdenliveSettings::grab_hide_frame()) {
        captureArgs << QStringLiteral("-show_region") << QStringLiteral("1");
    }
    QString captureSize = QStringLiteral(":0.0");
    if (KdenliveSettings::grab_capture_type() == 0) {
        // Full screen capture
        QRect screenSize = QApplication::screens().at(m_screenIndex)->geometry();
        captureArgs << QStringLiteral("-s") << QString::number(screenSize.width()) + QLatin1Char('x') + QString::number(screenSize.height());
        captureSize.append(QLatin1Char('+') + QString::number(screenSize.left()) + QLatin1Char('.') + QString::number(screenSize.top()));
    } else {
        // Region capture
        captureArgs << QStringLiteral("-s")
                    << QString::number(KdenliveSettings::grab_width()) + QLatin1Char('x') + QString::number(KdenliveSettings::grab_height());
        captureSize.append(QLatin1Char('+') + QString::number(KdenliveSettings::grab_offsetx()) + QLatin1Char(',') +
                           QString::number(KdenliveSettings::grab_offsety()));
    }
    if (KdenliveSettings::grab_hide_mouse()) {
        captureSize.append(QStringLiteral("+nomouse"));
    }
    // fps
    captureArgs << QStringLiteral("-r") << QString::number(KdenliveSettings::grab_fps());
    captureArgs << QStringLiteral("-i") << captureSize;
    if (!KdenliveSettings::grab_parameters().simplified().isEmpty()) {
        captureArgs << KdenliveSettings::grab_parameters().simplified().split(QLatin1Char(' '));
    }
    qDebug() << "== STARTING X11 CAPTURE: " << captureArgs << "\n___________";
    captureArgs << path;
    m_captureProcess->start(KdenliveSettings::ffmpegpath(), captureArgs);
#endif

    if (!m_captureProcess->waitForStarted()) {
        // Problem launching capture app
        Q_EMIT warningMessage(i18n("Failed to start the capture application:\n%1", KdenliveSettings::ffmpegpath()));
        // delete m_captureProcess;
    }
}

void RecManager::slotProcessStatus(int exitCode, QProcess::ExitStatus exitStatus)
{
    m_recAction->setEnabled(true);
    m_recAction->setChecked(false);
    m_device_selector->setEnabled(true);
    if (exitStatus == QProcess::CrashExit) {
        Q_EMIT warningMessage(i18n("Capture crashed, please check your parameters"), -1, QList<QAction *>() << m_showLogAction);
    } else {
        if (exitCode != 0 && exitCode != 255) {
            Q_EMIT warningMessage(i18n("Capture crashed, please check your parameters"), -1, QList<QAction *>() << m_showLogAction);
        } else {
            // Capture successful, add clip to project
            Q_EMIT addClipToProject(m_captureFile);
        }
    }
    if (m_captureProcess) {
        delete m_captureProcess;
        m_captureProcess = nullptr;
    }
}

void RecManager::slotReadProcessInfo()
{
    QString data = m_captureProcess->readAllStandardError().simplified();
    m_recError.append(data + QLatin1Char('\n'));
}

void RecManager::slotAudioDeviceChanged(int)
{
    QString currentDevice = m_audio_device->currentText();
    KdenliveSettings::setDefaultaudiocapture(currentDevice);
}

void RecManager::slotSetVolume(int volume)
{
    KdenliveSettings::setAudiocapturevolume(volume);
    QIcon icon;

    if (volume == 0) {
        icon = QIcon::fromTheme(QStringLiteral("audio-volume-muted"));
    } else {
        icon = QIcon::fromTheme(QStringLiteral("audio-volume-medium"));
    }
    m_audioCaptureButton->setIcon(icon);
}
void RecManager::slotVideoDeviceChanged(int)
{
    int currentItem = m_device_selector->currentData().toInt();
    KdenliveSettings::setDefaultcapture(currentItem);
    switch (currentItem) {
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
        m_captureDevice = nullptr;
    }

    // The m_videoBox container has to be shown once before the MLT consumer is build, or preview will fail
    switch (ix) {
    case ScreenBag:
    }
    */
}

Mlt::Producer *RecManager::createV4lProducer()
{
    QString profilePath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + QStringLiteral("/profiles/video4linux");
    Mlt::Profile *vidProfile = new Mlt::Profile(profilePath.toUtf8().constData());
    bool profileUsed = false;
    Mlt::Producer *prod = nullptr;
    if (m_recVideo->isChecked()) {
        prod = new Mlt::Producer(*vidProfile, QStringLiteral("video4linux2:%1").arg(KdenliveSettings::video4vdevice()).toUtf8().constData());
        if ((prod == nullptr) || !prod->is_valid()) {
            return nullptr;
        }
        prod->set("width", vidProfile->width());
        prod->set("height", vidProfile->height());
        prod->set("framerate", vidProfile->fps());
        /*p->set("standard", ui->v4lStandardCombo->currentText().toLatin1().constData());
        p->set("channel", ui->v4lChannelSpinBox->value());
        p->set("audio_ix", ui->v4lAudioComboBox->currentIndex());*/
        prod->set("force_seekable", 0);
        profileUsed = true;
    }
    if (m_recAudio->isChecked() && (prod != nullptr) && prod->is_valid()) {
        // Add audio track
        Mlt::Producer *audio = new Mlt::Producer(
            *vidProfile,
            QStringLiteral("alsa:%1?channels=%2").arg(KdenliveSettings::v4l_alsadevicename()).arg(KdenliveSettings::alsachannels()).toUtf8().constData());
        audio->set("mlt_service", "avformat-novalidate");
        audio->set("audio_index", 0);
        audio->set("video_index", -1);
        auto *tractor = new Mlt::Tractor(*vidProfile);
        tractor->set_track(*prod, 0);
        delete prod;
        tractor->set_track(*audio, 1);
        delete audio;
        prod = new Mlt::Producer(tractor->get_producer());
        delete tractor;
        profileUsed = true;
    }
    if (!profileUsed) {
        delete vidProfile;
    }
    return prod;
}

void RecManager::slotSetScreen(int screenIndex)
{
    m_screenIndex = screenIndex;
}

void RecManager::slotPreview(bool preview)
{
    if (m_device_selector->currentData().toInt() == Video4Linux) {
        if (preview) {
            std::shared_ptr<Mlt::Producer> prod(createV4lProducer());
            if (prod && prod->is_valid()) {
                m_monitor->updateClipProducer(prod);
            } else {
                Q_EMIT warningMessage(i18n("Capture crashed, please check your parameters"));
            }
        } else {
            m_monitor->slotOpenClip(nullptr);
        }
    }

    /*
       buildMltDevice(path);

       bool isXml;
       producer = getV4lXmlPlaylist(profile, &isXml);

       //producer =
    QString("avformat-novalidate:video4linux2:%1?width:%2&height:%3&frame_rate:%4").arg(KdenliveSettings::video4vdevice()).arg(profile.width).arg(profile.height).arg((double)
    profile.frame_rate_num / profile.frame_rate_den);
       if (!m_captureDevice->slotStartPreview(producer, isXml)) {
           // v4l capture failed to start
           video_frame->setText(i18n("Failed to start Video4Linux,\ncheck your parameters…"));

       } else {
           m_playAction->setEnabled(false);
           m_stopAction->setEnabled(true);
           m_isPlaying = true;
       }
    }*/
}

void RecManager::slotShowLog()
{
    KMessageBox::information(QApplication::activeWindow(), m_recError);
}
