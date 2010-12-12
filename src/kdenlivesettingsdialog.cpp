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
#include "v4l/v4lcapture.h"
#include "blackmagic/devices.h"
#include "kdenlivesettings.h"

#include <KStandardDirs>
#include <KDebug>
#include <kopenwithdialog.h>
#include <KConfigDialogManager>
#include <kde_file.h>
#include <KIO/NetAccess>
#include <kdeversion.h>

#include <QDir>
#include <QTimer>
#include <QTreeWidgetItem>

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#ifndef NO_JOGSHUTTLE
#include "jogaction.h"
#include "jogshuttleconfig.h"
#include <linux/input.h>
#endif /* NO_JOGSHUTTLE */


KdenliveSettingsDialog::KdenliveSettingsDialog(const QMap<QString, QString>& mappable_actions, QWidget * parent) :
    KConfigDialog(parent, "settings", KdenliveSettings::self()),
    m_modified(false),
    m_shuttleModified(false),
    m_mappable_actions(mappable_actions)
{

    QWidget *p1 = new QWidget;
    m_configMisc.setupUi(p1);
    m_page1 = addPage(p1, i18n("Misc"), "configure");

    // Hide multi tab option until Kdenlive really supports it
    m_configMisc.kcfg_activatetabs->setVisible(false);

    QWidget *p8 = new QWidget;
    m_configProject.setupUi(p8);
    m_page8 = addPage(p8, i18n("Project Defaults"), "document-new");

    QWidget *p3 = new QWidget;
    m_configTimeline.setupUi(p3);
    m_page3 = addPage(p3, i18n("Timeline"), "video-display");

    QWidget *p2 = new QWidget;
    m_configEnv.setupUi(p2);
    m_configEnv.mltpathurl->setMode(KFile::Directory);
    m_configEnv.mltpathurl->lineEdit()->setObjectName("kcfg_mltpath");
    m_configEnv.rendererpathurl->lineEdit()->setObjectName("kcfg_rendererpath");
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

#if !defined(Q_WS_MAC) && !defined(Q_OS_FREEBSD)
    V4lCaptureHandler v4l(NULL);
    // Video 4 Linux device detection
    for (int i = 0; i < 10; i++) {
        QString path = "/dev/video" + QString::number(i);
        if (QFile::exists(path)) {
            QStringList deviceInfo = v4l.getDeviceName(path);
            if (!deviceInfo.isEmpty()) {
                m_configCapture.kcfg_detectedv4ldevices->addItem(deviceInfo.at(0), path);
                m_configCapture.kcfg_detectedv4ldevices->setItemData(m_configCapture.kcfg_detectedv4ldevices->count() - 1, deviceInfo.at(1), Qt::UserRole + 1);
            }
        }
    }
    connect(m_configCapture.kcfg_detectedv4ldevices, SIGNAL(currentIndexChanged(int)), this, SLOT(slotUpdatev4lDevice()));
#endif

    m_page4 = addPage(p4, i18n("Capture"), "media-record");
    m_configCapture.tabWidget->setCurrentIndex(KdenliveSettings::defaultcapture());
#ifdef Q_WS_MAC
    m_configCapture.tabWidget->setEnabled(false);
    m_configCapture.kcfg_defaultcapture->setEnabled(false);
    m_configCapture.label->setText(i18n("Capture is not yet available on OS X."));
#endif

    QWidget *p5 = new QWidget;
    m_configShuttle.setupUi(p5);
#ifdef NO_JOGSHUTTLE
    m_configShuttle.kcfg_enableshuttle->hide();
    m_configShuttle.kcfg_enableshuttle->setDisabled(true);
#else
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
#endif /* NO_JOGSHUTTLE */
    m_page5 = addPage(p5, i18n("JogShuttle"), "input-mouse");

    QWidget *p6 = new QWidget;
    m_configSdl.setupUi(p6);

#if not defined(Q_WS_MAC) && not defined(USE_OPEN_GL)
    m_configSdl.kcfg_openglmonitors->setHidden(true);
#endif

    m_page6 = addPage(p6, i18n("Playback"), "media-playback-start");

    QWidget *p7 = new QWidget;
    m_configTranscode.setupUi(p7);
    m_page7 = addPage(p7, i18n("Transcode"), "edit-copy");
    connect(m_configTranscode.button_add, SIGNAL(clicked()), this, SLOT(slotAddTranscode()));
    connect(m_configTranscode.button_delete, SIGNAL(clicked()), this, SLOT(slotDeleteTranscode()));
    connect(m_configTranscode.profiles_list, SIGNAL(itemChanged(QTreeWidgetItem *, int)), this, SLOT(slotDialogModified()));
    
    connect(m_configCapture.kcfg_video4vdevice, SIGNAL(editingFinished()), this, SLOT(rebuildVideo4Commands()));
    connect(m_configCapture.kcfg_video4adevice, SIGNAL(editingFinished()), this, SLOT(rebuildVideo4Commands()));
    connect(m_configCapture.kcfg_video4vcodec, SIGNAL(editingFinished()), this, SLOT(rebuildVideo4Commands()));
    connect(m_configCapture.kcfg_video4acodec, SIGNAL(editingFinished()), this, SLOT(rebuildVideo4Commands()));
    connect(m_configCapture.kcfg_video4vformat, SIGNAL(editingFinished()), this, SLOT(rebuildVideo4Commands()));
    connect(m_configCapture.kcfg_video4aformat, SIGNAL(editingFinished()), this, SLOT(rebuildVideo4Commands()));
    connect(m_configCapture.kcfg_video4size, SIGNAL(editingFinished()), this, SLOT(rebuildVideo4Commands()));
    connect(m_configCapture.kcfg_video4rate, SIGNAL(editingFinished()), this, SLOT(rebuildVideo4Commands()));

    connect(m_configCapture.kcfg_rmd_capture_audio, SIGNAL(clicked(bool)), m_configCapture.audio_group, SLOT(setVisible(bool)));

    m_configCapture.audio_group->setVisible(KdenliveSettings::rmd_capture_audio());

    connect(m_configEnv.kp_image, SIGNAL(clicked()), this, SLOT(slotEditImageApplication()));
    connect(m_configEnv.kp_audio, SIGNAL(clicked()), this, SLOT(slotEditAudioApplication()));
    connect(m_configEnv.kp_player, SIGNAL(clicked()), this, SLOT(slotEditVideoApplication()));

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


    BMInterface::getBlackMagicDeviceList(m_configCapture.kcfg_hdmi_capturedevice, m_configCapture.kcfg_hdmi_capturemode);
    connect(m_configCapture.kcfg_hdmi_capturedevice, SIGNAL(currentIndexChanged(int)), this, SLOT(slotUpdateHDMIModes()));

    if (BMInterface::getBlackMagicOutputDeviceList(m_configSdl.kcfg_blackmagic_output_device)) {
        // Found blackmagic card
    } else m_configSdl.kcfg_external_display->setEnabled(false);

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

void KdenliveSettingsDialog::slotUpdateHDMIModes()
{
    QStringList modes = m_configCapture.kcfg_hdmi_capturedevice->itemData(m_configCapture.kcfg_hdmi_capturedevice->currentIndex()).toStringList();
    m_configCapture.kcfg_hdmi_capturemode->clear();
    m_configCapture.kcfg_hdmi_capturemode->insertItems(0, modes);
}

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
                    m_configCapture.kcfg_rmd_alsa_device->addItem(line.section(':', 1, 1), "plughw:" + QString::number(deviceId.section('-', 0, 0).toInt()) + ',' + QString::number(deviceId.section('-', 1, 1).toInt()));
                }
            }
            file.close();
        }
    }
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
}


void KdenliveSettingsDialog::slotReadAudioDevices()
{
    QString result = QString(m_readProcess.readAllStandardOutput());
    kDebug() << "// / / / / / READING APLAY: ";
    kDebug() << result;
    QStringList lines = result.split('\n');
    foreach(const QString & data, lines) {
        kDebug() << "// READING LINE: " << data;
        if (data.simplified().startsWith("card")) {
            QString card = data.section(':', 0, 0).section(' ', -1);
            QString device = data.section(':', 1, 1).section(' ', -1);
            m_configSdl.kcfg_audio_device->addItem(data.section(':', -1), "plughw:" + card + ',' + device);
            m_configCapture.kcfg_rmd_alsa_device->addItem(data.section(':', -1), "plughw:" + card + ',' + device);
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

#ifndef NO_JOGSHUTTLE
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

#endif /* NO_JOGSHUTTLE */

void KdenliveSettingsDialog::rebuildVideo4Commands()
{
    QString captureCommand;
    if (!m_configCapture.kcfg_video4adevice->text().isEmpty()) captureCommand = "-f " + m_configCapture.kcfg_video4aformat->text() + " -i " + m_configCapture.kcfg_video4adevice->text() + " -acodec " + m_configCapture.kcfg_video4acodec->text();

    captureCommand +=  " -f " + m_configCapture.kcfg_video4vformat->text() + " -s " + m_configCapture.kcfg_video4size->text() + " -r " + QString::number(m_configCapture.kcfg_video4rate->value()) + " -i " + m_configCapture.kcfg_video4vdevice->text() + " -vcodec " + m_configCapture.kcfg_video4vcodec->text();
    m_configCapture.kcfg_video4capture->setText(captureCommand);
}
void KdenliveSettingsDialog::updateWidgets()
{
    // Revert widgets to last saved state (for example when user pressed "Cancel")
    // kDebug() << "// // // KCONFIG Revert called";
#ifndef NO_JOGSHUTTLE
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
#endif
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

    if (updateCapturePath) emit updateCaptureFolder();

    QString value = m_configCapture.kcfg_rmd_alsa_device->itemData(m_configCapture.kcfg_rmd_alsa_device->currentIndex()).toString();
    if (value != KdenliveSettings::rmd_alsadevicename()) {
        KdenliveSettings::setRmd_alsadevicename(value);
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

#ifndef NO_JOGSHUTTLE
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

#if KDE_IS_VERSION(4,3,0)
    KConfigDialog::settingsChangedSlot();
#endif

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
    QMap< QString, QString > profiles = transConfig.entryMap();
    QMapIterator<QString, QString> i(profiles);
    while (i.hasNext()) {
        i.next();
        QTreeWidgetItem *item = new QTreeWidgetItem(m_configTranscode.profiles_list, QStringList() << i.key() << i.value());
        item->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled | Qt::ItemIsEditable);
    }
    m_configTranscode.profiles_list->blockSignals(false);
}

void KdenliveSettingsDialog::saveTranscodeProfiles()
{
    KSharedConfigPtr config = KSharedConfig::openConfig("kdenlivetranscodingrc");
    //KSharedConfigPtr config = KGlobal::config();
    KConfigGroup transConfig(config, "Transcoding");
    // read the entries
    transConfig.deleteGroup();
    int max = m_configTranscode.profiles_list->topLevelItemCount();
    for (int i = 0; i < max; i++) {
        QTreeWidgetItem *item = m_configTranscode.profiles_list->topLevelItem(i);
        transConfig.writeEntry(item->text(0), item->text(1));
    }
    config->sync();
}

void KdenliveSettingsDialog::slotAddTranscode()
{
    QTreeWidgetItem *item = new QTreeWidgetItem(m_configTranscode.profiles_list, QStringList() << i18n("Name") << i18n("Parameters"));
    item->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled | Qt::ItemIsEditable);
    m_configTranscode.profiles_list->setCurrentItem(item);
    m_configTranscode.profiles_list->editItem(item);
    slotDialogModified();
}

void KdenliveSettingsDialog::slotDeleteTranscode()
{
    QTreeWidgetItem *item = m_configTranscode.profiles_list->currentItem();
    if (item == NULL) return;
    delete item;
    slotDialogModified();
}

void KdenliveSettingsDialog::slotShuttleModified()
{
#ifndef NO_JOGSHUTTLE
    QStringList actions;
    actions << "monitor_pause";  // the Job rest position action.
    foreach (KComboBox* button, m_shuttle_buttons) {
        actions << m_mappable_actions[button->currentText()];
    }
    QString maps = JogShuttleConfig::actionMap(actions);
    m_shuttleModified = KdenliveSettings::shuttlebuttons() != maps;
#endif
#if KDE_IS_VERSION(4,3,0)
    KConfigDialog::updateButtons();
#endif
}

void KdenliveSettingsDialog::slotDialogModified()
{
    m_modified = true;
#if KDE_IS_VERSION(4,3,0)
    KConfigDialog::updateButtons();
#endif
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
    QString size = m_configCapture.kcfg_detectedv4ldevices->itemData(m_configCapture.kcfg_detectedv4ldevices->currentIndex(), Qt::UserRole + 1).toString();
    if (!size.isEmpty()) m_configCapture.kcfg_video4size->setText(size);
    rebuildVideo4Commands();
}


#include "kdenlivesettingsdialog.moc"


