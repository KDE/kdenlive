/***************************************************************************
 *   Copyright (C) 2008 by Jean-Baptiste Mardelle (jb@kdenlive.org)        *
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

#include "kdenlivesettingsdialog.h"
#include "profilesdialog.h"
#ifdef USE_V4L
#include "v4l/v4lcapture.h"
#endif
#include "encodingprofilesdialog.h"
#include "kdenlivesettings.h"
#include "renderer.h"

#include <KStandardDirs>
#include <KDebug>
#include <kopenwithdialog.h>
#include <KConfigDialogManager>
#include <kde_file.h>
#include <KIO/NetAccess>
#include <kdeversion.h>
#include <KMessageBox>

#include <QDir>
#include <QTimer>
#include <QTreeWidgetItem>
#include <QThread>

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#ifdef USE_JOGSHUTTLE
#include "jogaction.h"
#include "jogshuttleconfig.h"
#include <linux/input.h>
#endif


KdenliveSettingsDialog::KdenliveSettingsDialog(const QMap<QString, QString>& mappable_actions, QWidget * parent) :
    KConfigDialog(parent, "settings", KdenliveSettings::self()),
    m_modified(false),
    m_shuttleModified(false),
    m_mappable_actions(mappable_actions)
{
    KdenliveSettings::setV4l_format(0);
    QWidget *p1 = new QWidget;
    m_configMisc.setupUi(p1);
    m_page1 = addPage(p1, i18n("Misc"), "configure");

    // Hide multi tab option until Kdenlive really supports it
    m_configMisc.kcfg_activatetabs->setVisible(false);
    // Hide avformat-novalidate trick, causes crash (bug #2205 and #2206)
    m_configMisc.kcfg_projectloading_avformatnovalidate->setVisible(false);

    QWidget *p8 = new QWidget;
    m_configProject.setupUi(p8);
    m_page8 = addPage(p8, i18n("Project Defaults"), "document-new");
    connect(m_configProject.kcfg_generateproxy, SIGNAL(toggled(bool)), m_configProject.kcfg_proxyminsize, SLOT(setEnabled(bool)));
    m_configProject.kcfg_proxyminsize->setEnabled(KdenliveSettings::generateproxy());
    connect(m_configProject.kcfg_generateimageproxy, SIGNAL(toggled(bool)), m_configProject.kcfg_proxyimageminsize, SLOT(setEnabled(bool)));
    m_configProject.kcfg_proxyimageminsize->setEnabled(KdenliveSettings::generateimageproxy());

    QWidget *p3 = new QWidget;
    m_configTimeline.setupUi(p3);
    m_page3 = addPage(p3, i18n("Timeline"), "video-display");

    QWidget *p2 = new QWidget;
    m_configEnv.setupUi(p2);
    m_configEnv.mltpathurl->setMode(KFile::Directory);
    m_configEnv.mltpathurl->lineEdit()->setObjectName("kcfg_mltpath");
    m_configEnv.rendererpathurl->lineEdit()->setObjectName("kcfg_rendererpath");
    m_configEnv.kcfg_mltthreads->setMaximum( QThread::idealThreadCount() < 4 ? QThread::idealThreadCount() : 3 );
    m_configEnv.tmppathurl->setMode(KFile::Directory);
    m_configEnv.tmppathurl->lineEdit()->setObjectName("kcfg_currenttmpfolder");
    m_configEnv.projecturl->setMode(KFile::Directory);
    m_configEnv.projecturl->lineEdit()->setObjectName("kcfg_defaultprojectfolder");
    m_configEnv.capturefolderurl->setMode(KFile::Directory);
    m_configEnv.capturefolderurl->lineEdit()->setObjectName("kcfg_capturefolder");
    m_configEnv.capturefolderurl->setEnabled(!KdenliveSettings::capturetoprojectfolder());
    connect(m_configEnv.kcfg_capturetoprojectfolder, SIGNAL(clicked()), this, SLOT(slotEnableCaptureFolder()));
    m_page2 = addPage(p2, i18n("Environment"), "application-x-executable-script");

    QWidget *p4 = new QWidget;
    m_configCapture.setupUi(p4);

#ifdef USE_V4L

    // Video 4 Linux device detection
    for (int i = 0; i < 10; i++) {
        QString path = "/dev/video" + QString::number(i);
        if (QFile::exists(path)) {
            QStringList deviceInfo = V4lCaptureHandler::getDeviceName(path);
            if (!deviceInfo.isEmpty()) {
                m_configCapture.kcfg_detectedv4ldevices->addItem(deviceInfo.at(0), path);
                m_configCapture.kcfg_detectedv4ldevices->setItemData(m_configCapture.kcfg_detectedv4ldevices->count() - 1, deviceInfo.at(1), Qt::UserRole + 1);
            }
        }
    }
    connect(m_configCapture.kcfg_detectedv4ldevices, SIGNAL(currentIndexChanged(int)), this, SLOT(slotUpdatev4lDevice()));
    connect(m_configCapture.kcfg_v4l_format, SIGNAL(currentIndexChanged(int)), this, SLOT(slotUpdatev4lCaptureProfile()));
    connect(m_configCapture.config_v4l, SIGNAL(clicked()), this, SLOT(slotEditVideo4LinuxProfile()));

    slotUpdatev4lDevice();
#endif

    m_page4 = addPage(p4, i18n("Capture"), "media-record");
    m_configCapture.tabWidget->setCurrentIndex(KdenliveSettings::defaultcapture());
#ifdef Q_WS_MAC
    m_configCapture.tabWidget->setEnabled(false);
    m_configCapture.kcfg_defaultcapture->setEnabled(false);
    m_configCapture.label->setText(i18n("Capture is not yet available on Mac OS X."));
#endif

    QWidget *p5 = new QWidget;
    m_configShuttle.setupUi(p5);
#ifdef USE_JOGSHUTTLE
    connect(m_configShuttle.kcfg_enableshuttle, SIGNAL(stateChanged(int)), this, SLOT(slotCheckShuttle(int)));
    connect(m_configShuttle.shuttledevicelist, SIGNAL(activated(int)), this, SLOT(slotUpdateShuttleDevice(int)));
    slotCheckShuttle(KdenliveSettings::enableshuttle());
    m_configShuttle.shuttledisabled->hide();

    // Store the button pointers into an array for easier handling them in the other functions.
    m_shuttle_buttons.push_back(m_configShuttle.shuttle1);
    m_shuttle_buttons.push_back(m_configShuttle.shuttle2);
    m_shuttle_buttons.push_back(m_configShuttle.shuttle3);
    m_shuttle_buttons.push_back(m_configShuttle.shuttle4);
    m_shuttle_buttons.push_back(m_configShuttle.shuttle5);

    // populate the buttons with the current configuration. The items are sorted
    // according to the user-selected language, so they do not appear in random order.
    QList<QString> action_names = mappable_actions.keys();
    qSort(action_names);

    // Here we need to compute the action_id -> index-in-action_names. We iterate over the
    // action_names, as the sorting may depend on the user-language.
    QStringList actions_map = JogShuttleConfig::actionMap(KdenliveSettings::shuttlebuttons());
    QMap<QString, int> action_pos;
    foreach (const QString& action_id, actions_map) {
      // This loop find out at what index is the string that would map to the action_id.
      for (int i = 0; i < action_names.size(); i++) {
          if (mappable_actions[action_names[i]] == action_id) {
              action_pos[action_id] = i;
              break;
          }
      }
    }

    int i = 0;
    foreach (KComboBox* button, m_shuttle_buttons) {
      button->addItems(action_names);
      connect(button, SIGNAL(activated(int)), this, SLOT(slotShuttleModified()));
      ++i;
      if (i < actions_map.size())
        button->setCurrentIndex(action_pos[actions_map[i]]);
    }
#else /* ! USE_JOGSHUTTLE */
    m_configShuttle.kcfg_enableshuttle->hide();
    m_configShuttle.kcfg_enableshuttle->setDisabled(true);
#endif /* USE_JOGSHUTTLE */
    m_page5 = addPage(p5, i18n("JogShuttle"), "input-mouse");

    QWidget *p6 = new QWidget;
    m_configSdl.setupUi(p6);

#ifndef USE_OPENGL
    m_configSdl.kcfg_openglmonitors->setHidden(true);
#endif

    m_page6 = addPage(p6, i18n("Playback"), "media-playback-start");

    QWidget *p7 = new QWidget;
    m_configTranscode.setupUi(p7);
    m_page7 = addPage(p7, i18n("Transcode"), "edit-copy");
    connect(m_configTranscode.button_add, SIGNAL(clicked()), this, SLOT(slotAddTranscode()));
    connect(m_configTranscode.button_delete, SIGNAL(clicked()), this, SLOT(slotDeleteTranscode()));
    connect(m_configTranscode.profiles_list, SIGNAL(itemChanged(QListWidgetItem *)), this, SLOT(slotDialogModified()));
    connect(m_configTranscode.profiles_list, SIGNAL(currentRowChanged(int)), this, SLOT(slotSetTranscodeProfile()));
    
    connect(m_configTranscode.profile_name, SIGNAL(textChanged(const QString &)), this, SLOT(slotEnableTranscodeUpdate()));
    connect(m_configTranscode.profile_description, SIGNAL(textChanged(const QString &)), this, SLOT(slotEnableTranscodeUpdate()));
    connect(m_configTranscode.profile_extension, SIGNAL(textChanged(const QString &)), this, SLOT(slotEnableTranscodeUpdate()));
    connect(m_configTranscode.profile_parameters, SIGNAL(textChanged()), this, SLOT(slotEnableTranscodeUpdate()));
    connect(m_configTranscode.profile_audioonly, SIGNAL(stateChanged(int)), this, SLOT(slotEnableTranscodeUpdate()));
    
    connect(m_configTranscode.button_update, SIGNAL(pressed()), this, SLOT(slotUpdateTranscodingProfile()));
    
    m_configTranscode.profile_parameters->setMaximumHeight(QFontMetrics(font()).lineSpacing() * 5);

    connect(m_configCapture.kcfg_rmd_capture_audio, SIGNAL(clicked(bool)), m_configCapture.audio_group, SLOT(setVisible(bool)));

    m_configCapture.audio_group->setVisible(KdenliveSettings::rmd_capture_audio());

    connect(m_configEnv.kp_image, SIGNAL(clicked()), this, SLOT(slotEditImageApplication()));
    connect(m_configEnv.kp_audio, SIGNAL(clicked()), this, SLOT(slotEditAudioApplication()));
    connect(m_configEnv.kp_player, SIGNAL(clicked()), this, SLOT(slotEditVideoApplication()));

    loadEncodingProfiles();
    checkProfile();

    slotUpdateDisplay();

    connect(m_configSdl.kcfg_audio_driver, SIGNAL(currentIndexChanged(int)), this, SLOT(slotCheckAlsaDriver()));
    initDevices();
    connect(m_configProject.kcfg_profiles_list, SIGNAL(currentIndexChanged(int)), this, SLOT(slotUpdateDisplay()));
    connect(m_configCapture.kcfg_rmd_capture_type, SIGNAL(currentIndexChanged(int)), this, SLOT(slotUpdateRmdRegionStatus()));

    slotUpdateRmdRegionStatus();
    loadTranscodeProfiles();


    //HACK: check dvgrab version, because only dvgrab >= 3.3 supports
    //   --timestamp option without bug

    if (KdenliveSettings::dvgrab_path().isEmpty() || !QFile::exists(KdenliveSettings::dvgrab_path())) {
        QString dvgrabpath = KStandardDirs::findExe("dvgrab");
        KdenliveSettings::setDvgrab_path(dvgrabpath);
    }

    // decklink profile
    m_configCapture.decklink_showprofileinfo->setIcon(KIcon("help-about"));
    m_configCapture.decklink_manageprofile->setIcon(KIcon("configure"));
    m_configCapture.decklink_parameters->setVisible(false);
    m_configCapture.decklink_parameters->setMaximumHeight(QFontMetrics(font()).lineSpacing() * 4);
    m_configCapture.decklink_parameters->setPlainText(KdenliveSettings::decklink_parameters());
    connect(m_configCapture.decklink_manageprofile, SIGNAL(clicked(bool)), this, SLOT(slotManageEncodingProfile()));
    connect(m_configCapture.kcfg_decklink_profile, SIGNAL(currentIndexChanged(int)), this, SLOT(slotUpdateDecklinkProfile()));
    connect(m_configCapture.decklink_showprofileinfo, SIGNAL(clicked(bool)), m_configCapture.decklink_parameters, SLOT(setVisible(bool)));

    // v4l profile
    m_configCapture.v4l_showprofileinfo->setIcon(KIcon("help-about"));
    m_configCapture.v4l_manageprofile->setIcon(KIcon("configure"));
    m_configCapture.v4l_parameters->setVisible(false);
    m_configCapture.v4l_parameters->setMaximumHeight(QFontMetrics(font()).lineSpacing() * 4);
    m_configCapture.v4l_parameters->setPlainText(KdenliveSettings::v4l_parameters());
    connect(m_configCapture.v4l_manageprofile, SIGNAL(clicked(bool)), this, SLOT(slotManageEncodingProfile()));
    connect(m_configCapture.kcfg_v4l_profile, SIGNAL(currentIndexChanged(int)), this, SLOT(slotUpdateV4lProfile()));
    connect(m_configCapture.v4l_showprofileinfo, SIGNAL(clicked(bool)), m_configCapture.v4l_parameters, SLOT(setVisible(bool)));

    // proxy profile stuff
    m_configProject.proxy_showprofileinfo->setIcon(KIcon("help-about"));
    m_configProject.proxy_manageprofile->setIcon(KIcon("configure"));
    m_configProject.proxyparams->setVisible(false);
    m_configProject.proxyparams->setMaximumHeight(QFontMetrics(font()).lineSpacing() * 4);
    m_configProject.proxyparams->setPlainText(KdenliveSettings::proxyparams());
    connect(m_configProject.proxy_manageprofile, SIGNAL(clicked(bool)), this, SLOT(slotManageEncodingProfile()));
    connect(m_configProject.proxy_showprofileinfo, SIGNAL(clicked(bool)), m_configProject.proxyparams, SLOT(setVisible(bool)));
    connect(m_configProject.kcfg_proxy_profile, SIGNAL(currentIndexChanged(int)), this, SLOT(slotUpdateProxyProfile()));


    slotUpdateProxyProfile(-1);
    slotUpdateV4lProfile(-1);
    slotUpdateDecklinkProfile(-1);

    Render::getBlackMagicDeviceList(m_configCapture.kcfg_decklink_capturedevice);
    if (!Render::getBlackMagicOutputDeviceList(m_configSdl.kcfg_blackmagic_output_device)) {
        // No blackmagic card found
	m_configSdl.kcfg_external_display->setEnabled(false);
    }

    double dvgrabVersion = 0;
    if (!KdenliveSettings::dvgrab_path().isEmpty()) {
        QProcess *versionCheck = new QProcess;
        versionCheck->setProcessChannelMode(QProcess::MergedChannels);
        versionCheck->start("dvgrab", QStringList() << "--version");
        if (versionCheck->waitForFinished()) {
            QString version = QString(versionCheck->readAll()).simplified();
            if (version.contains(' ')) version = version.section(' ', -1);
            dvgrabVersion = version.toDouble();

            kDebug() << "// FOUND DVGRAB VERSION: " << dvgrabVersion;
        }
        delete versionCheck;
        if (dvgrabVersion < 3.3) {
            KdenliveSettings::setFirewiretimestamp(false);
            m_configCapture.kcfg_firewiretimestamp->setEnabled(false);
        }
        m_configCapture.dvgrab_info->setText(i18n("dvgrab version %1 at %2", dvgrabVersion, KdenliveSettings::dvgrab_path()));
    } else m_configCapture.dvgrab_info->setText(i18n("<strong><em>dvgrab</em> utility not found, please install it for firewire capture</strong>"));

    if (KdenliveSettings::rmd_path().isEmpty() || !QFile::exists(KdenliveSettings::rmd_path())) {
        QString rmdpath = KStandardDirs::findExe("recordmydesktop");
        KdenliveSettings::setRmd_path(rmdpath);
    }
    if (KdenliveSettings::rmd_path().isEmpty())
        m_configCapture.rmd_info->setText(i18n("<strong><em>Recordmydesktop</em> utility not found, please install it for screen grabs</strong>"));
    else
        m_configCapture.rmd_info->setText(i18n("Recordmydesktop found at: %1", KdenliveSettings::rmd_path()));
}

KdenliveSettingsDialog::~KdenliveSettingsDialog() {}

void KdenliveSettingsDialog::slotUpdateRmdRegionStatus()
{
    m_configCapture.region_group->setHidden(m_configCapture.kcfg_rmd_capture_type->currentIndex() != 1);
}

void KdenliveSettingsDialog::slotEnableCaptureFolder()
{
    m_configEnv.capturefolderurl->setEnabled(!m_configEnv.kcfg_capturetoprojectfolder->isChecked());
}

void KdenliveSettingsDialog::checkProfile()
{
    m_configProject.kcfg_profiles_list->clear();
    QMap <QString, QString> profilesInfo = ProfilesDialog::getProfilesInfo();
    QMapIterator<QString, QString> i(profilesInfo);
    while (i.hasNext()) {
        i.next();
        m_configProject.kcfg_profiles_list->addItem(i.key(), i.value());
    }

    if (!KdenliveSettings::default_profile().isEmpty()) {
        for (int i = 0; i < m_configProject.kcfg_profiles_list->count(); i++) {
            if (m_configProject.kcfg_profiles_list->itemData(i).toString() == KdenliveSettings::default_profile()) {
                m_configProject.kcfg_profiles_list->setCurrentIndex(i);
                KdenliveSettings::setProfiles_list(i);
                break;
            }
        }
    }
}

void KdenliveSettingsDialog::initDevices()
{
    // Fill audio drivers
    m_configSdl.kcfg_audio_driver->addItem(i18n("Automatic"), QString());
#ifndef Q_WS_MAC
    m_configSdl.kcfg_audio_driver->addItem(i18n("OSS"), "dsp");
    m_configSdl.kcfg_audio_driver->addItem(i18n("ALSA"), "alsa");
    m_configSdl.kcfg_audio_driver->addItem(i18n("PulseAudio"), "pulse");
    m_configSdl.kcfg_audio_driver->addItem(i18n("OSS with DMA access"), "dma");
    m_configSdl.kcfg_audio_driver->addItem(i18n("Esound daemon"), "esd");
    m_configSdl.kcfg_audio_driver->addItem(i18n("ARTS daemon"), "artsc");
#endif

    if (!KdenliveSettings::audiodrivername().isEmpty())
        for (int i = 1; i < m_configSdl.kcfg_audio_driver->count(); i++) {
            if (m_configSdl.kcfg_audio_driver->itemData(i).toString() == KdenliveSettings::audiodrivername()) {
                m_configSdl.kcfg_audio_driver->setCurrentIndex(i);
                KdenliveSettings::setAudio_driver((uint) i);
            }
        }

    // Fill video drivers
    m_configSdl.kcfg_video_driver->addItem(i18n("Automatic"), QString());
#ifndef Q_WS_MAC
    m_configSdl.kcfg_video_driver->addItem(i18n("XVideo"), "x11");
    m_configSdl.kcfg_video_driver->addItem(i18n("X11"), "x11_noaccel");
    m_configSdl.kcfg_video_driver->addItem(i18n("XFree86 DGA 2.0"), "dga");
    m_configSdl.kcfg_video_driver->addItem(i18n("Nano X"), "nanox");
    m_configSdl.kcfg_video_driver->addItem(i18n("Framebuffer console"), "fbcon");
    m_configSdl.kcfg_video_driver->addItem(i18n("Direct FB"), "directfb");
    m_configSdl.kcfg_video_driver->addItem(i18n("SVGAlib"), "svgalib");
    m_configSdl.kcfg_video_driver->addItem(i18n("General graphics interface"), "ggi");
    m_configSdl.kcfg_video_driver->addItem(i18n("Ascii art library"), "aalib");
#endif

    // Fill the list of audio playback devices
    m_configSdl.kcfg_audio_device->addItem(i18n("Default"), QString());
    m_configCapture.kcfg_rmd_alsa_device->addItem(i18n("Default"), QString());
    if (!KStandardDirs::findExe("aplay").isEmpty()) {
        m_readProcess.setOutputChannelMode(KProcess::OnlyStdoutChannel);
        m_readProcess.setProgram("aplay", QStringList() << "-l");
        connect(&m_readProcess, SIGNAL(readyReadStandardOutput()) , this, SLOT(slotReadAudioDevices()));
        m_readProcess.execute(5000);
    } else {
        // If aplay is not installed on the system, parse the /proc/asound/pcm file
        QFile file("/proc/asound/pcm");
        if (file.open(QIODevice::ReadOnly)) {
            QTextStream stream(&file);
            QString line;
            QString deviceId;
            while (!stream.atEnd()) {
                line = stream.readLine();
                if (line.contains("playback")) {
                    deviceId = line.section(':', 0, 0);
                    m_configSdl.kcfg_audio_device->addItem(line.section(':', 1, 1), "plughw:" + QString::number(deviceId.section('-', 0, 0).toInt()) + ',' + QString::number(deviceId.section('-', 1, 1).toInt()));
                }
                if (line.contains("capture")) {
                    deviceId = line.section(':', 0, 0);
                    m_configCapture.kcfg_rmd_alsa_device->addItem(line.section(':', 1, 1).simplified(), "plughw:" + QString::number(deviceId.section('-', 0, 0).toInt()) + ',' + QString::number(deviceId.section('-', 1, 1).toInt()));
                    m_configCapture.kcfg_v4l_alsadevice->addItem(line.section(':', 1, 1).simplified(), "hw:" + QString::number(deviceId.section('-', 0, 0).toInt()) + ',' + QString::number(deviceId.section('-', 1, 1).toInt()));
                }
            }
            file.close();
        }
    }
    
    // Add pulseaudio capture option
    m_configCapture.kcfg_v4l_alsadevice->addItem(i18n("PulseAudio"), "pulse");
    
    if (!KdenliveSettings::audiodevicename().isEmpty()) {
        // Select correct alsa device
        int ix = m_configSdl.kcfg_audio_device->findData(KdenliveSettings::audiodevicename());
        m_configSdl.kcfg_audio_device->setCurrentIndex(ix);
        KdenliveSettings::setAudio_device(ix);
    }

    if (!KdenliveSettings::rmd_alsadevicename().isEmpty()) {
        // Select correct alsa device
        int ix = m_configCapture.kcfg_rmd_alsa_device->findData(KdenliveSettings::rmd_alsadevicename());
        m_configCapture.kcfg_rmd_alsa_device->setCurrentIndex(ix);
        KdenliveSettings::setRmd_alsa_device(ix);
    }

    if (!KdenliveSettings::v4l_alsadevicename().isEmpty()) {
        // Select correct alsa device
        int ix = m_configCapture.kcfg_v4l_alsadevice->findData(KdenliveSettings::v4l_alsadevicename());
        m_configCapture.kcfg_v4l_alsadevice->setCurrentIndex(ix);
        KdenliveSettings::setV4l_alsadevice(ix);
    }

    loadCurrentV4lProfileInfo();
}


void KdenliveSettingsDialog::slotReadAudioDevices()
{
    QString result = QString(m_readProcess.readAllStandardOutput());
    kDebug() << "// / / / / / READING APLAY: ";
    kDebug() << result;
    QStringList lines = result.split('\n');
    foreach(const QString & data, lines) {
        //kDebug() << "// READING LINE: " << data;
        if (!data.startsWith(" ") && data.count(':') > 1) {
            QString card = data.section(':', 0, 0).section(' ', -1);
            QString device = data.section(':', 1, 1).section(' ', -1);
            m_configSdl.kcfg_audio_device->addItem(data.section(':', -1).simplified(), "plughw:" + card + ',' + device);
            m_configCapture.kcfg_rmd_alsa_device->addItem(data.section(':', -1).simplified(), "plughw:" + card + ',' + device);
            m_configCapture.kcfg_v4l_alsadevice->addItem(data.section(':', -1).simplified(), "hw:" + card + ',' + device);
        }
    }
}

void KdenliveSettingsDialog::showPage(int page, int option)
{
    switch (page) {
    case 1:
        setCurrentPage(m_page1);
        break;
    case 2:
        setCurrentPage(m_page2);
        break;
    case 3:
        setCurrentPage(m_page3);
        break;
    case 4:
        setCurrentPage(m_page4);
        m_configCapture.tabWidget->setCurrentIndex(option);
        break;
    case 5:
        setCurrentPage(m_page5);
        break;
    case 6:
        setCurrentPage(m_page6);
        break;
    case 7:
        setCurrentPage(m_page7);
        break;
    default:
        setCurrentPage(m_page1);
    }
}

void KdenliveSettingsDialog::slotEditVideoApplication()
{
    KService::Ptr service;
    KOpenWithDialog dlg(KUrl::List(), i18n("Select default video player"), m_configEnv.kcfg_defaultplayerapp->text(), this);
    if (dlg.exec() != QDialog::Accepted)
        return;

    service = dlg.service();
    m_configEnv.kcfg_defaultplayerapp->setText(service->exec());
}

void KdenliveSettingsDialog::slotEditAudioApplication()
{
    KService::Ptr service;
    KOpenWithDialog dlg(KUrl::List(), i18n("Select default audio editor"), m_configEnv.kcfg_defaultaudioapp->text(), this);
    if (dlg.exec() != QDialog::Accepted)
        return;

    service = dlg.service();
    m_configEnv.kcfg_defaultaudioapp->setText(service->exec());
}

void KdenliveSettingsDialog::slotEditImageApplication()
{
    KService::Ptr service;
    KOpenWithDialog dlg(KUrl::List(), i18n("Select default image editor"), m_configEnv.kcfg_defaultimageapp->text(), this);
    if (dlg.exec() != QDialog::Accepted)
        return;

    service = dlg.service();
    m_configEnv.kcfg_defaultimageapp->setText(service->exec());
}

#ifdef USE_JOGSHUTTLE
void KdenliveSettingsDialog::slotCheckShuttle(int state)
{
    m_configShuttle.config_group->setEnabled(state);
    if (m_configShuttle.shuttledevicelist->count() == 0) {
        // parse devices
        QString baseName = "/dev/input/event";
        int fd;
        for (int i = 0; i < 30; i++) {
            QString filename = baseName + QString::number(i);
            kDebug() << "/// CHECKING OFR: " << filename;

            char name[256] = "unknown";
            fd = KDE_open((char *) filename.toUtf8().data(), O_RDONLY);
            if (fd >= 0 && ioctl(fd, EVIOCGNAME(sizeof(name)), name) >= 0) {
                m_configShuttle.shuttledevicelist->addItem(name, filename);
            }
            ::close(fd);
        }
        if (KdenliveSettings::shuttledevice().isEmpty()) QTimer::singleShot(1500, this, SLOT(slotUpdateShuttleDevice()));
    }
}

void KdenliveSettingsDialog::slotUpdateShuttleDevice(int ix)
{
    QString device = m_configShuttle.shuttledevicelist->itemData(ix).toString();
    //KdenliveSettings::setShuttledevice(device);
    m_configShuttle.kcfg_shuttledevice->setText(device);
}

#endif /* USE_JOGSHUTTLE */

void KdenliveSettingsDialog::updateWidgets()
{
    // Revert widgets to last saved state (for example when user pressed "Cancel")
    // kDebug() << "// // // KCONFIG Revert called";
#ifdef USE_JOGSHUTTLE
    // revert jog shuttle device
    if (m_configShuttle.shuttledevicelist->count() > 0) {
	for (int i = 0; i < m_configShuttle.shuttledevicelist->count(); i++) {
	    if (m_configShuttle.shuttledevicelist->itemData(i) == KdenliveSettings::shuttledevice()) {
		m_configShuttle.shuttledevicelist->setCurrentIndex(i);
		break;
	    }
	}
    }

    // Revert jog shuttle buttons
    QList<QString> action_names = m_mappable_actions.keys();
    qSort(action_names);
    QStringList actions_map = JogShuttleConfig::actionMap(KdenliveSettings::shuttlebuttons());
    QMap<QString, int> action_pos;
    foreach (const QString& action_id, actions_map) {
      // This loop find out at what index is the string that would map to the action_id.
      for (int i = 0; i < action_names.size(); i++) {
          if (m_mappable_actions[action_names[i]] == action_id) {
              action_pos[action_id] = i;
              break;
          }
      }
    }
    int i = 0;
    foreach (KComboBox* button, m_shuttle_buttons) {
      ++i;
      if (i < actions_map.size())
        button->setCurrentIndex(action_pos[actions_map[i]]);
    }
#endif /* USE_JOGSHUTTLE */
}

void KdenliveSettingsDialog::updateSettings()
{
    // Save changes to settings (for example when user pressed "Apply" or "Ok")
    // kDebug() << "// // // KCONFIG UPDATE called";
    m_defaultProfile = m_configProject.kcfg_profiles_list->currentText();
    KdenliveSettings::setDefault_profile(m_defaultPath);

    bool resetProfile = false;
    bool updateCapturePath = false;

    /*if (m_configShuttle.shuttledevicelist->count() > 0) {
	QString device = m_configShuttle.shuttledevicelist->itemData(m_configShuttle.shuttledevicelist->currentIndex()).toString();
	if (device != KdenliveSettings::shuttledevice()) KdenliveSettings::setShuttledevice(device);
    }*/

    if (m_configEnv.kcfg_capturetoprojectfolder->isChecked() != KdenliveSettings::capturetoprojectfolder()) {
        KdenliveSettings::setCapturetoprojectfolder(m_configEnv.kcfg_capturetoprojectfolder->isChecked());
        updateCapturePath = true;
    }

    if (m_configEnv.capturefolderurl->url().path() != KdenliveSettings::capturefolder()) {
        KdenliveSettings::setCapturefolder(m_configEnv.capturefolderurl->url().path());
        updateCapturePath = true;
    }

    if (m_configCapture.kcfg_dvgrabfilename->text() != KdenliveSettings::dvgrabfilename()) {
        KdenliveSettings::setDvgrabfilename(m_configCapture.kcfg_dvgrabfilename->text());
        updateCapturePath = true;
    }

    if ((uint) m_configCapture.kcfg_firewireformat->currentIndex() != KdenliveSettings::firewireformat()) {
        KdenliveSettings::setFirewireformat(m_configCapture.kcfg_firewireformat->currentIndex());
        updateCapturePath = true;
    }

    if ((uint) m_configCapture.kcfg_v4l_format->currentIndex() != KdenliveSettings::v4l_format()) {
        saveCurrentV4lProfile();
        KdenliveSettings::setV4l_format(0);
    }

    // Check encoding profiles
    QString data = m_configCapture.kcfg_v4l_profile->itemData(m_configCapture.kcfg_v4l_profile->currentIndex()).toString();
    if (!data.isEmpty() && (data.section(";", 0, 0) != KdenliveSettings::v4l_parameters() || data.section(";", 1, 1) != KdenliveSettings::v4l_extension())) {
        KdenliveSettings::setV4l_parameters(data.section(";", 0, 0));
        KdenliveSettings::setV4l_extension(data.section(";", 1, 1));
    }
    data = m_configCapture.kcfg_decklink_profile->itemData(m_configCapture.kcfg_decklink_profile->currentIndex()).toString();
    if (!data.isEmpty() && (data.section(";", 0, 0) != KdenliveSettings::decklink_parameters() || data.section(";", 1, 1) != KdenliveSettings::decklink_extension())) {
        KdenliveSettings::setDecklink_parameters(data.section(";", 0, 0));
        KdenliveSettings::setDecklink_extension(data.section(";", 1, 1));
    }
    data = m_configProject.kcfg_proxy_profile->itemData(m_configProject.kcfg_proxy_profile->currentIndex()).toString();
    if (!data.isEmpty() && (data.section(";", 0, 0) != KdenliveSettings::proxyparams() || data.section(";", 1, 1) != KdenliveSettings::proxyextension())) {
        KdenliveSettings::setProxyparams(data.section(";", 0, 0));
        KdenliveSettings::setProxyextension(data.section(";", 1, 1));
    }


    if (updateCapturePath) emit updateCaptureFolder();

    QString value = m_configCapture.kcfg_rmd_alsa_device->itemData(m_configCapture.kcfg_rmd_alsa_device->currentIndex()).toString();
    if (value != KdenliveSettings::rmd_alsadevicename()) {
        KdenliveSettings::setRmd_alsadevicename(value);
    }

    value = m_configCapture.kcfg_v4l_alsadevice->itemData(m_configCapture.kcfg_v4l_alsadevice->currentIndex()).toString();
    if (value != KdenliveSettings::v4l_alsadevicename()) {
        KdenliveSettings::setV4l_alsadevicename(value);
    }

    value = m_configCapture.kcfg_rmd_audio_freq->itemText(m_configCapture.kcfg_rmd_audio_freq->currentIndex());
    kDebug() << "// AUDIO FREQ VALUE: " << value << ", CURRENT: " << KdenliveSettings::rmd_freq() << ", IX: " << m_configCapture.kcfg_rmd_audio_freq->currentIndex();
    if (value != KdenliveSettings::rmd_freq()) {
        kDebug() << "// SETTING AUDIO FREQ TO: " << value;
        KdenliveSettings::setRmd_freq(value);
    }

    if (m_configSdl.kcfg_external_display->isChecked() != KdenliveSettings::external_display()) {
        KdenliveSettings::setExternal_display(m_configSdl.kcfg_external_display->isChecked());
        resetProfile = true;
    }

    value = m_configSdl.kcfg_audio_driver->itemData(m_configSdl.kcfg_audio_driver->currentIndex()).toString();
    if (value != KdenliveSettings::audiodrivername()) {
        KdenliveSettings::setAudiodrivername(value);
        resetProfile = true;
    }

    if (value == "alsa") {
        // Audio device setting is only valid for alsa driver
        value = m_configSdl.kcfg_audio_device->itemData(m_configSdl.kcfg_audio_device->currentIndex()).toString();
        if (value != KdenliveSettings::audiodevicename()) {
            KdenliveSettings::setAudiodevicename(value);
            resetProfile = true;
        }
    } else if (KdenliveSettings::audiodevicename().isEmpty() == false) {
        KdenliveSettings::setAudiodevicename(QString::null);
        resetProfile = true;
    }

    value = m_configSdl.kcfg_video_driver->itemData(m_configSdl.kcfg_video_driver->currentIndex()).toString();
    if (value != KdenliveSettings::videodrivername()) {
        KdenliveSettings::setVideodrivername(value);
        resetProfile = true;
    }

    if (m_configSdl.kcfg_window_background->color() != KdenliveSettings::window_background()) {
        KdenliveSettings::setWindow_background(m_configSdl.kcfg_window_background->color());
        resetProfile = true;
    }

    if (m_configSdl.kcfg_volume->value() != KdenliveSettings::volume()) {
        KdenliveSettings::setVolume(m_configSdl.kcfg_volume->value());
        resetProfile = true;
    }

    if (m_modified) {
        // The transcoding profiles were modified, save.
        m_modified = false;
        saveTranscodeProfiles();
    }

#ifdef USE_JOGSHUTTLE
    m_shuttleModified = false;

    QStringList actions;
    actions << "monitor_pause";  // the Job rest position action.
    foreach (KComboBox* button, m_shuttle_buttons) {
        actions << m_mappable_actions[button->currentText()];
    }
    QString maps = JogShuttleConfig::actionMap(actions);
    //fprintf(stderr, "Shuttle config: %s\n", JogShuttleConfig::actionMap(actions).toAscii().constData());
    if (KdenliveSettings::shuttlebuttons() != maps)
	KdenliveSettings::setShuttlebuttons(maps);
#endif

    KConfigDialog::settingsChangedSlot();
    //KConfigDialog::updateSettings();
    if (resetProfile) emit doResetProfile();
}

void KdenliveSettingsDialog::slotUpdateDisplay()
{
    QString currentProfile = m_configProject.kcfg_profiles_list->itemData(m_configProject.kcfg_profiles_list->currentIndex()).toString();
    QMap< QString, QString > values = ProfilesDialog::getSettingsFromFile(currentProfile);
    m_configProject.p_size->setText(values.value("width") + 'x' + values.value("height"));
    m_configProject.p_fps->setText(values.value("frame_rate_num") + '/' + values.value("frame_rate_den"));
    m_configProject.p_aspect->setText(values.value("sample_aspect_num") + '/' + values.value("sample_aspect_den"));
    m_configProject.p_display->setText(values.value("display_aspect_num") + '/' + values.value("display_aspect_den"));
    if (values.value("progressive").toInt() == 0)
        m_configProject.p_progressive->setText(i18n("Interlaced"));
    else
        m_configProject.p_progressive->setText(i18n("Progressive"));
    m_defaultProfile = m_configProject.kcfg_profiles_list->itemText(m_configProject.kcfg_profiles_list->currentIndex());
    m_defaultPath = currentProfile;
}

void KdenliveSettingsDialog::slotCheckAlsaDriver()
{
    QString value = m_configSdl.kcfg_audio_driver->itemData(m_configSdl.kcfg_audio_driver->currentIndex()).toString();
    m_configSdl.kcfg_audio_device->setEnabled(value == "alsa");
}

void KdenliveSettingsDialog::loadTranscodeProfiles()
{
    KSharedConfigPtr config = KSharedConfig::openConfig("kdenlivetranscodingrc");
    KConfigGroup transConfig(config, "Transcoding");
    // read the entries
    m_configTranscode.profiles_list->blockSignals(true);
    m_configTranscode.profiles_list->clear();
    QMap< QString, QString > profiles = transConfig.entryMap();
    QMapIterator<QString, QString> i(profiles);
    while (i.hasNext()) {
        i.next();
        QListWidgetItem *item = new QListWidgetItem(i.key());
        QString data = i.value();
        if (data.contains(';')) item->setToolTip(data.section(';', 1, 1));
        item->setData(Qt::UserRole, data);
        m_configTranscode.profiles_list->addItem(item);
        //item->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled | Qt::ItemIsEditable);
    }
    m_configTranscode.profiles_list->blockSignals(false);
    m_configTranscode.profiles_list->setCurrentRow(0);
}

void KdenliveSettingsDialog::saveTranscodeProfiles()
{
    KSharedConfigPtr config = KSharedConfig::openConfig("kdenlivetranscodingrc");
    //KSharedConfigPtr config = KGlobal::config();
    KConfigGroup transConfig(config, "Transcoding");
    // read the entries
    transConfig.deleteGroup();
    int max = m_configTranscode.profiles_list->count();
    for (int i = 0; i < max; i++) {
        QListWidgetItem *item = m_configTranscode.profiles_list->item(i);
        transConfig.writeEntry(item->text(), item->data(Qt::UserRole).toString());
    }
    config->sync();
}

void KdenliveSettingsDialog::slotAddTranscode()
{
    if (!m_configTranscode.profiles_list->findItems(m_configTranscode.profile_name->text(), Qt::MatchExactly).isEmpty()) {
        KMessageBox::sorry(this, i18n("A profile with that name already exists"));
        return;
    }
    QListWidgetItem *item = new QListWidgetItem(m_configTranscode.profile_name->text());
    QString data = m_configTranscode.profile_parameters->toPlainText();
    data.append(" %1." + m_configTranscode.profile_extension->text());
    data.append(";");
    if (!m_configTranscode.profile_description->text().isEmpty()) 
        data.append(m_configTranscode.profile_description->text());
    if (m_configTranscode.profile_audioonly->isChecked()) data.append(";audio");
    item->setData(Qt::UserRole, data);
    m_configTranscode.profiles_list->addItem(item);
    m_configTranscode.profiles_list->setCurrentItem(item);
    slotDialogModified();
}

void KdenliveSettingsDialog::slotUpdateTranscodingProfile()
{
    QListWidgetItem *item = m_configTranscode.profiles_list->currentItem();
    if (!item) return;
    m_configTranscode.button_update->setEnabled(false);
    item->setText(m_configTranscode.profile_name->text());
    QString data = m_configTranscode.profile_parameters->toPlainText();
    data.append(" %1." + m_configTranscode.profile_extension->text());
    data.append(";");
    if (!m_configTranscode.profile_description->text().isEmpty())
        data.append(m_configTranscode.profile_description->text());
    if (m_configTranscode.profile_audioonly->isChecked()) data.append(";audio");
    item->setData(Qt::UserRole, data);
    slotDialogModified();
}

void KdenliveSettingsDialog::slotDeleteTranscode()
{
    QListWidgetItem *item = m_configTranscode.profiles_list->currentItem();
    if (item == NULL) return;
    delete item;
    slotDialogModified();
}

void KdenliveSettingsDialog::slotEnableTranscodeUpdate()
{
    if (!m_configTranscode.profile_box->isEnabled()) return;
    bool allow = true;
    if (m_configTranscode.profile_name->text().isEmpty() || m_configTranscode.profile_extension->text().isEmpty()) allow = false;
    m_configTranscode.button_update->setEnabled(allow);
}

void KdenliveSettingsDialog::slotSetTranscodeProfile()
{
    m_configTranscode.profile_box->setEnabled(false);
    m_configTranscode.button_update->setEnabled(false);
    m_configTranscode.profile_name->clear();
    m_configTranscode.profile_description->clear();
    m_configTranscode.profile_extension->clear();
    m_configTranscode.profile_parameters->clear();
    m_configTranscode.profile_audioonly->setChecked(false);
    QListWidgetItem *item = m_configTranscode.profiles_list->currentItem();
    if (!item) {
        return;
    }
    m_configTranscode.profile_name->setText(item->text());
    QString data = item->data(Qt::UserRole).toString();
    if (data.contains(';')) {
        m_configTranscode.profile_description->setText(data.section(';', 1, 1));
        if (data.section(';', 2, 2) == "audio") m_configTranscode.profile_audioonly->setChecked(true);
        data = data.section(';', 0, 0).simplified();
    }
    m_configTranscode.profile_extension->setText(data.section('.', -1));
    m_configTranscode.profile_parameters->setPlainText(data.section(' ', 0, -2));
    m_configTranscode.profile_box->setEnabled(true);
}

void KdenliveSettingsDialog::slotShuttleModified()
{
#ifdef USE_JOGSHUTTLE
    QStringList actions;
    actions << "monitor_pause";  // the Job rest position action.
    foreach (KComboBox* button, m_shuttle_buttons) {
        actions << m_mappable_actions[button->currentText()];
    }
    QString maps = JogShuttleConfig::actionMap(actions);
    m_shuttleModified = KdenliveSettings::shuttlebuttons() != maps;
#endif
    KConfigDialog::updateButtons();
}

void KdenliveSettingsDialog::slotDialogModified()
{
    m_modified = true;
    KConfigDialog::updateButtons();
}

//virtual
bool KdenliveSettingsDialog::hasChanged()
{
    if (m_modified || m_shuttleModified) return true;
    return KConfigDialog::hasChanged();
}

void KdenliveSettingsDialog::slotUpdatev4lDevice()
{
    QString device = m_configCapture.kcfg_detectedv4ldevices->itemData(m_configCapture.kcfg_detectedv4ldevices->currentIndex()).toString();
    if (!device.isEmpty()) m_configCapture.kcfg_video4vdevice->setText(device);
    QString info = m_configCapture.kcfg_detectedv4ldevices->itemData(m_configCapture.kcfg_detectedv4ldevices->currentIndex(), Qt::UserRole + 1).toString();

    m_configCapture.kcfg_v4l_format->blockSignals(true);
    m_configCapture.kcfg_v4l_format->clear();

    QString vl4ProfilePath = KStandardDirs::locateLocal("appdata", "profiles/video4linux");
    if (QFile::exists(vl4ProfilePath)) {
        m_configCapture.kcfg_v4l_format->addItem(i18n("Current settings"));
    }

    QStringList pixelformats = info.split(">", QString::SkipEmptyParts);
    QString itemSize;
    QString pixelFormat;
    QStringList itemRates;
    for (int i = 0; i < pixelformats.count(); i++) {
        QString format = pixelformats.at(i).section(':', 0, 0);
        QStringList sizes = pixelformats.at(i).split(":", QString::SkipEmptyParts);
        pixelFormat = sizes.takeFirst();
        for (int j = 0; j < sizes.count(); j++) {
            itemSize = sizes.at(j).section("=", 0, 0);
            itemRates = sizes.at(j).section("=", 1, 1).split(",", QString::SkipEmptyParts);
            for (int k = 0; k < itemRates.count(); k++) {
                m_configCapture.kcfg_v4l_format->addItem("[" + format + "] " + itemSize + " (" + itemRates.at(k) + ")", QStringList() << format << itemSize.section('x', 0, 0) << itemSize.section('x', 1, 1) << itemRates.at(k).section('/', 0, 0) << itemRates.at(k).section('/', 1, 1));
            }
        }
    }
    m_configCapture.kcfg_v4l_format->blockSignals(false);
    slotUpdatev4lCaptureProfile();
}

void KdenliveSettingsDialog::slotUpdatev4lCaptureProfile()
{
    QStringList info = m_configCapture.kcfg_v4l_format->itemData(m_configCapture.kcfg_v4l_format->currentIndex(), Qt::UserRole).toStringList();
    if (info.isEmpty()) {
        // No auto info, display the current ones
        loadCurrentV4lProfileInfo();
        return;
    }
    m_configCapture.p_size->setText(info.at(1) + 'x' + info.at(2));
    m_configCapture.p_fps->setText(info.at(3) + '/' + info.at(4));
    m_configCapture.p_aspect->setText("1/1");
    m_configCapture.p_display->setText(info.at(1) + '/' + info.at(2));
    m_configCapture.p_colorspace->setText(ProfilesDialog::getColorspaceDescription(601));
    m_configCapture.p_progressive->setText(i18n("Progressive"));

    QString vl4ProfilePath = KStandardDirs::locateLocal("appdata", "profiles/video4linux");
    if (!QFile::exists(vl4ProfilePath)) saveCurrentV4lProfile();
}

void KdenliveSettingsDialog::loadCurrentV4lProfileInfo()
{
    QString vl4ProfilePath = KStandardDirs::locateLocal("appdata", "profiles/video4linux");
    MltVideoProfile prof;
    if (!QFile::exists(vl4ProfilePath)) {
        // No default formats found, build one
        prof.width = 320;
        prof.height = 200;
        prof.frame_rate_num = 15;
        prof.frame_rate_den = 1;
        prof.display_aspect_num = 4;
        prof.display_aspect_den = 3;
        prof.sample_aspect_num = 1;
        prof.sample_aspect_den = 1;
        prof.progressive = 1;
        prof.colorspace = 601;
        ProfilesDialog::saveProfile(prof, vl4ProfilePath);
    }
    else prof = ProfilesDialog::getVideoProfile(vl4ProfilePath);
    m_configCapture.p_size->setText(QString::number(prof.width) + 'x' + QString::number(prof.height));
    m_configCapture.p_fps->setText(QString::number(prof.frame_rate_num) + '/' + QString::number(prof.frame_rate_den));
    m_configCapture.p_aspect->setText(QString::number(prof.sample_aspect_num) + '/' + QString::number(prof.sample_aspect_den));
    m_configCapture.p_display->setText(QString::number(prof.display_aspect_num) + '/' + QString::number(prof.display_aspect_den));
    m_configCapture.p_colorspace->setText(ProfilesDialog::getColorspaceDescription(prof.colorspace));
    if (prof.progressive) m_configCapture.p_progressive->setText(i18n("Progressive"));
}

void KdenliveSettingsDialog::saveCurrentV4lProfile()
{
    MltVideoProfile profile;
    profile.description = "Video4Linux capture";
    profile.colorspace = ProfilesDialog::getColorspaceFromDescription(m_configCapture.p_colorspace->text());
    profile.width = m_configCapture.p_size->text().section('x', 0, 0).toInt();
    profile.height = m_configCapture.p_size->text().section('x', 1, 1).toInt();
    profile.sample_aspect_num = m_configCapture.p_aspect->text().section('/', 0, 0).toInt();
    profile.sample_aspect_den = m_configCapture.p_aspect->text().section('/', 1, 1).toInt();
    profile.display_aspect_num = m_configCapture.p_display->text().section('/', 0, 0).toInt();
    profile.display_aspect_den = m_configCapture.p_display->text().section('/', 1, 1).toInt();
    profile.frame_rate_num = m_configCapture.p_fps->text().section('/', 0, 0).toInt();
    profile.frame_rate_den = m_configCapture.p_fps->text().section('/', 1, 1).toInt();
    profile.progressive = m_configCapture.p_progressive->text() == i18n("Progressive");
    QString vl4ProfilePath = KStandardDirs::locateLocal("appdata", "profiles/video4linux");
    ProfilesDialog::saveProfile(profile, vl4ProfilePath);
}

void KdenliveSettingsDialog::slotManageEncodingProfile()
{
    int type = 0;
    if (currentPage() == m_page4) {
        if (m_configCapture.tabWidget->currentIndex() == 1) type = 1;
        else if (m_configCapture.tabWidget->currentIndex() == 3) type = 2;
    }
    EncodingProfilesDialog *d = new EncodingProfilesDialog(type);
    d->exec();
    delete d;
    loadEncodingProfiles();
}

void KdenliveSettingsDialog::loadEncodingProfiles()
{
    KConfig conf("encodingprofiles.rc", KConfig::FullConfig, "appdata");

    // Load v4l profiles
    m_configCapture.kcfg_v4l_profile->blockSignals(true);
    QString currentItem = m_configCapture.kcfg_v4l_profile->currentText();
    m_configCapture.kcfg_v4l_profile->clear();
    KConfigGroup group(&conf, "video4linux");
    QMap< QString, QString > values = group.entryMap();
    QMapIterator<QString, QString> i(values);
    while (i.hasNext()) {
        i.next();
        if (!i.key().isEmpty()) m_configCapture.kcfg_v4l_profile->addItem(i.key(), i.value());
    }
    m_configCapture.kcfg_v4l_profile->blockSignals(false);
    if (!currentItem.isEmpty()) m_configCapture.kcfg_v4l_profile->setCurrentIndex(m_configCapture.kcfg_v4l_profile->findText(currentItem));

    // Load Decklink profiles
    m_configCapture.kcfg_decklink_profile->blockSignals(true);
    currentItem = m_configCapture.kcfg_decklink_profile->currentText();
    m_configCapture.kcfg_decklink_profile->clear();
    KConfigGroup group2(&conf, "decklink");
    values = group2.entryMap();
    QMapIterator<QString, QString> j(values);
    while (j.hasNext()) {
        j.next();
        if (!j.key().isEmpty()) m_configCapture.kcfg_decklink_profile->addItem(j.key(), j.value());
    }
    m_configCapture.kcfg_decklink_profile->blockSignals(false);
    if (!currentItem.isEmpty()) m_configCapture.kcfg_decklink_profile->setCurrentIndex(m_configCapture.kcfg_decklink_profile->findText(currentItem));

    // Load Proxy profiles
    m_configProject.kcfg_proxy_profile->blockSignals(true);
    currentItem = m_configProject.kcfg_proxy_profile->currentText();
    m_configProject.kcfg_proxy_profile->clear();
    KConfigGroup group3(&conf, "proxy");
    values = group3.entryMap();
    QMapIterator<QString, QString> k(values);
    while (k.hasNext()) {
        k.next();
        if (!k.key().isEmpty()) m_configProject.kcfg_proxy_profile->addItem(k.key(), k.value());
    }
    if (!currentItem.isEmpty()) m_configProject.kcfg_proxy_profile->setCurrentIndex(m_configProject.kcfg_proxy_profile->findText(currentItem));
    m_configProject.kcfg_proxy_profile->blockSignals(false);
    slotUpdateProxyProfile();
    
}

void KdenliveSettingsDialog::slotUpdateDecklinkProfile(int ix)
{
    if (ix == -1) ix = KdenliveSettings::decklink_profile();
    else ix = m_configCapture.kcfg_decklink_profile->currentIndex();
    QString data = m_configCapture.kcfg_decklink_profile->itemData(ix).toString();
    if (data.isEmpty()) return;
    m_configCapture.decklink_parameters->setPlainText(data.section(";", 0, 0));
    //
}

void KdenliveSettingsDialog::slotUpdateV4lProfile(int ix)
{
    if (ix == -1) ix = KdenliveSettings::v4l_profile();
    else ix = m_configCapture.kcfg_v4l_profile->currentIndex();
    QString data = m_configCapture.kcfg_v4l_profile->itemData(ix).toString();
    if (data.isEmpty()) return;
    m_configCapture.v4l_parameters->setPlainText(data.section(";", 0, 0));
    //
}

void KdenliveSettingsDialog::slotUpdateProxyProfile(int ix)
{
    if (ix == -1) ix = KdenliveSettings::v4l_profile();
    else ix = m_configProject.kcfg_proxy_profile->currentIndex();
    QString data = m_configProject.kcfg_proxy_profile->itemData(ix).toString();
    if (data.isEmpty()) return;
    m_configProject.proxyparams->setPlainText(data.section(";", 0, 0));
    //
}

void KdenliveSettingsDialog::slotEditVideo4LinuxProfile()
{
    QString vl4ProfilePath = KStandardDirs::locateLocal("appdata", "profiles/video4linux");
    ProfilesDialog *w = new ProfilesDialog(vl4ProfilePath);
    if (w->exec() == QDialog::Accepted) {
        // save and update profile
        loadCurrentV4lProfileInfo();
    }
    delete w;
}


#include "kdenlivesettingsdialog.moc"


