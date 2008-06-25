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
    page2 = addPage(p2, i18n("Environnement"), "terminal");

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


    QStringList profilesNames = ProfilesDialog::getProfileNames();
    m_configMisc.kcfg_profiles_list->addItems(profilesNames);
    m_defaultProfile = ProfilesDialog::getSettingsFromFile(KdenliveSettings::default_profile()).value("description");
    if (profilesNames.contains(m_defaultProfile)) {
        m_configMisc.kcfg_profiles_list->setCurrentItem(m_defaultProfile);
        KdenliveSettings::setProfiles_list(profilesNames.indexOf(m_defaultProfile));
    }

    slotUpdateDisplay();

    connect(m_configMisc.kcfg_profiles_list, SIGNAL(currentIndexChanged(int)), this, SLOT(slotUpdateDisplay()));
}

KdenliveSettingsDialog::~KdenliveSettingsDialog() {}


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

bool KdenliveSettingsDialog::hasChanged() {
    kDebug() << "// // // KCONFIG hasChanged called: " << m_configMisc.kcfg_profiles_list->currentText() << ", " << m_defaultProfile;
    if (m_configMisc.kcfg_profiles_list->currentText() != m_defaultProfile) return true;
    return KConfigDialog::hasChanged();
}

void KdenliveSettingsDialog::updateSettings() {
    kDebug() << "// // // KCONFIG UPDATE called";
    m_defaultProfile = m_configMisc.kcfg_profiles_list->currentText();
    KdenliveSettings::setDefault_profile(m_defaultPath);
    KConfigDialog::updateSettings();
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


