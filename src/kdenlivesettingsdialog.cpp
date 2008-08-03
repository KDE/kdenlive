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

#include <QDir>
#include <QTimer>

#include <KStandardDirs>
#include <KDebug>
#include <kopenwithdialog.h>

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <linux/input.h>

#include "profilesdialog.h"
#include "kdenlivesettings.h"
#include "kdenlivesettingsdialog.h"

KdenliveSettingsDialog::KdenliveSettingsDialog(QWidget * parent): KConfigDialog(parent, "settings", KdenliveSettings::self()) {

    QWidget *p1 = new QWidget;
    m_configMisc.setupUi(p1);
    page1 = addPage(p1, i18n("Misc"), "configure");

    QWidget *p3 = new QWidget;
    m_configDisplay.setupUi(p3);
    page3 = addPage(p3, i18n("Display"), "display");

    QWidget *p2 = new QWidget;
    m_configEnv.setupUi(p2);
    m_configEnv.mltpathurl->setMode(KFile::Directory);
    m_configEnv.mltpathurl->lineEdit()->setObjectName("kcfg_mltpath");
    m_configEnv.rendererpathurl->lineEdit()->setObjectName("kcfg_rendererpath");
    m_configEnv.tmppathurl->setMode(KFile::Directory);
    m_configEnv.tmppathurl->lineEdit()->setObjectName("kcfg_currenttmpfolder");
    m_configEnv.capturefolderurl->setMode(KFile::Directory);
    m_configEnv.capturefolderurl->lineEdit()->setObjectName("kcfg_capturefolder");
    page2 = addPage(p2, i18n("Environment"), "terminal");

    QWidget *p4 = new QWidget;
    m_configCapture.setupUi(p4);
    page4 = addPage(p4, i18n("Capture"), "audio-card");
    m_configCapture.tabWidget->setCurrentIndex(KdenliveSettings::defaultcapture());

    QWidget *p5 = new QWidget;
    m_configShuttle.setupUi(p5);
    connect(m_configShuttle.kcfg_enableshuttle, SIGNAL(stateChanged(int)), this, SLOT(slotCheckShuttle(int)));
    connect(m_configShuttle.shuttledevicelist, SIGNAL(activated(int)), this, SLOT(slotUpdateShuttleDevice(int)));
    slotCheckShuttle(KdenliveSettings::enableshuttle());
    page5 = addPage(p5, i18n("JogShuttle"), "input-mouse");

    QWidget *p6 = new QWidget;
    m_configSdl.setupUi(p6);
    page6 = addPage(p6, i18n("Playback"), "audio-card");

    QStringList actions;
    actions << i18n("Do nothing");
    actions << i18n("Play / Pause");
    actions << i18n("Cut");
    m_configShuttle.kcfg_shuttle1->addItems(actions);
    m_configShuttle.kcfg_shuttle2->addItems(actions);
    m_configShuttle.kcfg_shuttle3->addItems(actions);
    m_configShuttle.kcfg_shuttle4->addItems(actions);
    m_configShuttle.kcfg_shuttle5->addItems(actions);

    connect(m_configCapture.kcfg_video4vdevice, SIGNAL(editingFinished()), this, SLOT(rebuildVideo4Commands()));
    connect(m_configCapture.kcfg_video4adevice, SIGNAL(editingFinished()), this, SLOT(rebuildVideo4Commands()));
    connect(m_configCapture.kcfg_video4vformat, SIGNAL(editingFinished()), this, SLOT(rebuildVideo4Commands()));
    connect(m_configCapture.kcfg_video4aformat, SIGNAL(editingFinished()), this, SLOT(rebuildVideo4Commands()));
    connect(m_configCapture.kcfg_video4vencoding, SIGNAL(editingFinished()), this, SLOT(rebuildVideo4Commands()));
    connect(m_configCapture.kcfg_video4aencoding, SIGNAL(editingFinished()), this, SLOT(rebuildVideo4Commands()));
    connect(m_configCapture.kcfg_video4size, SIGNAL(editingFinished()), this, SLOT(rebuildVideo4Commands()));
    connect(m_configCapture.kcfg_video4rate, SIGNAL(editingFinished()), this, SLOT(rebuildVideo4Commands()));

    connect(m_configEnv.kp_image, SIGNAL(clicked()), this, SLOT(slotEditImageApplication()));
    connect(m_configEnv.kp_audio, SIGNAL(clicked()), this, SLOT(slotEditAudioApplication()));
    connect(m_configEnv.kp_player, SIGNAL(clicked()), this, SLOT(slotEditVideoApplication()));

    QStringList profilesNames = ProfilesDialog::getProfileNames();
    m_configMisc.kcfg_profiles_list->addItems(profilesNames);
    m_defaultProfile = ProfilesDialog::getSettingsFromFile(KdenliveSettings::default_profile()).value("description");
    if (profilesNames.contains(m_defaultProfile)) {
        m_configMisc.kcfg_profiles_list->setCurrentItem(m_defaultProfile);
        KdenliveSettings::setProfiles_list(profilesNames.indexOf(m_defaultProfile));
    }

    slotUpdateDisplay();
    m_audioDevice = KdenliveSettings::audio_device();
    initDevices();
    connect(m_configMisc.kcfg_profiles_list, SIGNAL(currentIndexChanged(int)), this, SLOT(slotUpdateDisplay()));
}

KdenliveSettingsDialog::~KdenliveSettingsDialog() {}

void KdenliveSettingsDialog::initDevices() {
    // Fill audio drivers
    m_configSdl.kcfg_audio_driver->addItem(i18n("Automatic"), QString());
    m_configSdl.kcfg_audio_driver->addItem(i18n("OSS"), "dsp");
    m_configSdl.kcfg_audio_driver->addItem(i18n("ALSA"), "alsa");
    m_configSdl.kcfg_audio_driver->addItem(i18n("OSS with DMA access"), "dma");
    m_configSdl.kcfg_audio_driver->addItem(i18n("Esound daemon"), "esd");
    m_configSdl.kcfg_audio_driver->addItem(i18n("ARTS daemon"), "artsc");

    /*if (!KdenliveSettings::audiodriver().isEmpty())
    for (uint i = 1;i < m_configDisplay.kcfg_audio_driver->count(); i++) {
     if (m_configDisplay.kcfg_audio_driver->itemData(i).toString() == KdenliveSettings::audiodriver())
    m_configDisplay.kcfg_audio_driver->setCurrentIndex(i);
    }*/

    // Fill video drivers
    m_configSdl.kcfg_video_driver->addItem(i18n("Automatic"), QString());
    m_configSdl.kcfg_video_driver->addItem(i18n("X11"), "x11");
    m_configSdl.kcfg_video_driver->addItem(i18n("XFREE86 DGA 2.0"), "dga");
    m_configSdl.kcfg_video_driver->addItem(i18n("Nano X"), "nanox");
    m_configSdl.kcfg_video_driver->addItem(i18n("Framebuffer console"), "fbcon");
    m_configSdl.kcfg_video_driver->addItem(i18n("Direct FB"), "directfb");
    m_configSdl.kcfg_video_driver->addItem(i18n("SVGAlib"), "svgalib");
    m_configSdl.kcfg_video_driver->addItem(i18n("General graphics interface"), "ggi");
    m_configSdl.kcfg_video_driver->addItem(i18n("Ascii art library"), "aalib");

    // Fill the list of audio playback devices
    m_configSdl.kcfg_audio_device->addItem(i18n("Default"), QString());
    if (KStandardDirs::findExe("aplay") != QString::null) {
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
            while (!stream.atEnd()) {
                line = stream.readLine();
                if (line.contains("playback")) {
                    QString deviceId = line.section(":", 0, 0);
                    m_configSdl.kcfg_audio_device->addItem(line.section(":", 1, 1), "plughw:" + QString::number(deviceId.section("-", 0, 0).toInt()) + "," + QString::number(deviceId.section("-", 1, 1).toInt()));
                }
            }
            file.close();
        }
    }
}


void KdenliveSettingsDialog::slotReadAudioDevices() {
    QString result = QString(m_readProcess.readAllStandardOutput());
    kDebug() << "// / / / / / READING APLAY: ";
    kDebug() << result;
    QStringList lines = result.split('\n');
    foreach(QString data, lines) {
        kDebug() << "// READING LINE: " << data;
        if (data.simplified().startsWith("card")) {
            QString card = data.section(":", 0, 0).section(" ", -1);
            QString device = data.section(":", 1, 1).section(" ", -1);
            m_configSdl.kcfg_audio_device->addItem(data.section(":", -1), "plughw:" + card + "," + device);
        }
    }
}

void KdenliveSettingsDialog::showPage(int page, int option) {
    switch (page) {
    case 1:
        setCurrentPage(page1);
        break;
    case 2:
        setCurrentPage(page2);
        break;
    case 3:
        setCurrentPage(page3);
        break;
    case 4:
        setCurrentPage(page4);
        m_configCapture.tabWidget->setCurrentIndex(option);
        break;
    case 5:
        setCurrentPage(page5);
        break;

    }
}

void KdenliveSettingsDialog::slotEditVideoApplication() {
    KService::Ptr service;
    KOpenWithDialog dlg(KUrl::List(), i18n("Select default video player"), m_configEnv.kcfg_defaultplayerapp->text(), this);
    if (dlg.exec() != QDialog::Accepted)
        return;

    service = dlg.service();
    m_configEnv.kcfg_defaultplayerapp->setText(service->exec());
}

void KdenliveSettingsDialog::slotEditAudioApplication() {
    KService::Ptr service;
    KOpenWithDialog dlg(KUrl::List(), i18n("Select default audio editor"), m_configEnv.kcfg_defaultaudioapp->text(), this);
    if (dlg.exec() != QDialog::Accepted)
        return;

    service = dlg.service();
    m_configEnv.kcfg_defaultaudioapp->setText(service->exec());
}

void KdenliveSettingsDialog::slotEditImageApplication() {
    KService::Ptr service;
    KOpenWithDialog dlg(KUrl::List(), i18n("Select default image editor"), m_configEnv.kcfg_defaultimageapp->text(), this);
    if (dlg.exec() != QDialog::Accepted)
        return;

    service = dlg.service();
    m_configEnv.kcfg_defaultimageapp->setText(service->exec());
}

void KdenliveSettingsDialog::slotCheckShuttle(int state) {
    m_configShuttle.config_group->setEnabled(state);
    if (m_configShuttle.shuttledevicelist->count() == 0) {
        // parse devices
        QString baseName = "/dev/input/event";
        int fd;
        for (int i = 0; i < 30; i++) {
            QString filename = baseName + QString::number(i);
            kDebug() << "/// CHECKING OFR: " << filename;

            char name[256] = "unknown";
            fd = ::open((char *) filename.toUtf8().data(), O_RDONLY);
            if (fd >= 0 && ioctl(fd, EVIOCGNAME(sizeof(name)), name) >= 0) {
                m_configShuttle.shuttledevicelist->addItem(name, filename);
            }
            ::close(fd);
        }
        if (KdenliveSettings::shuttledevice().isEmpty()) QTimer::singleShot(1500, this, SLOT(slotUpdateShuttleDevice()));
    }
}

void KdenliveSettingsDialog::slotUpdateShuttleDevice(int ix) {
    QString device = m_configShuttle.shuttledevicelist->itemData(ix).toString();
    //KdenliveSettings::setShuttledevice(device);
    m_configShuttle.kcfg_shuttledevice->setText(device);
}

void KdenliveSettingsDialog::rebuildVideo4Commands() {
    QString captureCommand;
    if (!m_configCapture.kcfg_video4adevice->text().isEmpty()) captureCommand = "-f " + m_configCapture.kcfg_video4aformat->text() + " -i " + m_configCapture.kcfg_video4adevice->text();

    captureCommand +=  " -f " + m_configCapture.kcfg_video4vformat->text() + " -s " + m_configCapture.kcfg_video4size->text() + " -r " + QString::number(m_configCapture.kcfg_video4rate->value()) + " -i " + m_configCapture.kcfg_video4vdevice->text() + " -f " + m_configCapture.kcfg_video4vencoding->text();
    m_configCapture.kcfg_video4capture->setText(captureCommand);

    QString playbackCommand;
    playbackCommand =  "-f " + m_configCapture.kcfg_video4vencoding->text();
    m_configCapture.kcfg_video4playback->setText(playbackCommand);
}


// virtual protected
bool KdenliveSettingsDialog::isDefault() {
    return KConfigDialog::isDefault();
}

// virtual protected
bool KdenliveSettingsDialog::hasChanged() {
    kDebug() << "// // // KCONFIG hasChanged called: " << m_configMisc.kcfg_profiles_list->currentText() << ", " << m_defaultProfile;

    if (m_configMisc.kcfg_profiles_list->currentText() != m_defaultProfile) return true;
    return KConfigDialog::hasChanged();
}

void KdenliveSettingsDialog::updateSettings() {
    kDebug() << "// // // KCONFIG UPDATE called";
    m_defaultProfile = m_configMisc.kcfg_profiles_list->currentText();
    KdenliveSettings::setDefault_profile(m_defaultPath);

    bool resetProfile = false;
    QString value = m_configSdl.kcfg_audio_device->itemData(m_configSdl.kcfg_audio_device->currentIndex()).toString();
    if (value != KdenliveSettings::audiodevicename()) {
        KdenliveSettings::setAudiodevicename(value);
        resetProfile = true;
    }

    value = m_configSdl.kcfg_audio_driver->itemData(m_configSdl.kcfg_audio_driver->currentIndex()).toString();
    if (value != KdenliveSettings::audiodrivername()) {
        KdenliveSettings::setAudiodrivername(value);
        resetProfile = true;
    }

    value = m_configSdl.kcfg_video_driver->itemData(m_configSdl.kcfg_video_driver->currentIndex()).toString();
    if (value != KdenliveSettings::videodrivername()) {
        KdenliveSettings::setVideodrivername(value);
        resetProfile = true;
    }

    KConfigDialog::updateSettings();
    if (resetProfile) emit doResetProfile();
}

void KdenliveSettingsDialog::slotUpdateDisplay() {
    QString currentProfile = m_configMisc.kcfg_profiles_list->currentText();
    QMap< QString, QString > values = ProfilesDialog::getSettingsForProfile(currentProfile);
    m_configMisc.p_size->setText(values.value("width") + "x" + values.value("height"));
    m_configMisc.p_fps->setText(values.value("frame_rate_num") + "/" + values.value("frame_rate_den"));
    m_configMisc.p_aspect->setText(values.value("sample_aspect_num") + "/" + values.value("sample_aspect_den"));
    m_configMisc.p_display->setText(values.value("display_aspect_num") + "/" + values.value("display_aspect_den"));
    if (values.value("progressive").toInt() == 0) m_configMisc.p_progressive->setText(i18n("Interlaced"));
    else m_configMisc.p_progressive->setText(i18n("Progressive"));
    m_defaultPath = values.value("path");
}


#include "kdenlivesettingsdialog.moc"


