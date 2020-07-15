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

#ifndef KDENLIVESETTINGSDIALOG_H
#define KDENLIVESETTINGSDIALOG_H

#include <KConfigDialog>
#include <KProcess>
#include <QMap>

#include "ui_configcapture_ui.h"
#include "ui_configenv_ui.h"
#include "ui_configjogshuttle_ui.h"
#include "ui_configmisc_ui.h"
#include "ui_configproject_ui.h"
#include "ui_configproxy_ui.h"
#include "ui_configsdl_ui.h"
#include "ui_configtimeline_ui.h"
#include "ui_configtranscode_ui.h"
#include "ui_configcolors_ui.h"

class ProfileWidget;

class KdenliveSettingsDialog : public KConfigDialog
{
    Q_OBJECT

public:
    KdenliveSettingsDialog(QMap<QString, QString> mappable_actions, bool gpuAllowed, QWidget *parent = nullptr);
    ~KdenliveSettingsDialog() override;
    void showPage(int page, int option);
    void checkProfile();

protected slots:
    void updateSettings() override;
    void updateWidgets() override;
    bool hasChanged() override;
    void accept() override;

private slots:
    void slotCheckShuttle(int state = 0);
    void slotUpdateShuttleDevice(int ix = 0);
    void slotEditImageApplication();
    void slotEditAudioApplication();
    void slotReadAudioDevices();
    void slotUpdateGrabRegionStatus();
    void slotCheckAlsaDriver();
    void slotCheckAudioBackend();
    void slotAddTranscode();
    void slotDeleteTranscode();
    /** @brief Update current transcoding profile. */
    void slotUpdateTranscodingProfile();
    /** @brief Enable / disable the update profile button. */
    void slotEnableTranscodeUpdate();
    /** @brief Update display of current transcoding profile parameters. */
    void slotSetTranscodeProfile();
    void slotShuttleModified();
    void slotDialogModified();
    void slotEnableCaptureFolder();
    void slotEnableLibraryFolder();
    void slotUpdatev4lDevice();
    void slotUpdatev4lCaptureProfile();
    void slotManageEncodingProfile();
    void slotUpdateDecklinkProfile(int ix = 0);
    void slotUpdateProxyProfile(int ix = 0);
    void slotUpdatePreviewProfile(int ix = 0);
    void slotUpdateV4lProfile(int ix = 0);
    void slotUpdateGrabProfile(int ix = 0);
    void slotEditVideo4LinuxProfile();
    void slotReloadBlackMagic();
    void slotReloadShuttleDevices();
    void loadExternalProxyProfiles();
    void slotUpdateAudioCaptureChannels(int index);
    void slotUpdateAudioCaptureSampleRate(int index);

private:
    KPageWidgetItem *m_page1;
    KPageWidgetItem *m_page2;
    KPageWidgetItem *m_page3;
    KPageWidgetItem *m_page4;
    KPageWidgetItem *m_page5;
    KPageWidgetItem *m_page6;
    KPageWidgetItem *m_page7;
    KPageWidgetItem *m_page8;
    KPageWidgetItem *m_page10;
    Ui::ConfigEnv_UI m_configEnv;
    Ui::ConfigMisc_UI m_configMisc;
    Ui::ConfigColors_UI m_configColors;
    Ui::ConfigTimeline_UI m_configTimeline;
    Ui::ConfigCapture_UI m_configCapture;
    Ui::ConfigJogShuttle_UI m_configShuttle;
    Ui::ConfigSdl_UI m_configSdl;
    Ui::ConfigTranscode_UI m_configTranscode;
    Ui::ConfigProject_UI m_configProject;
    Ui::ConfigProxy_UI m_configProxy;
    ProfileWidget *m_pw;
    KProcess m_readProcess;
    bool m_modified;
    bool m_shuttleModified;
    QMap<QString, QString> m_mappable_actions;
    QVector<QComboBox *> m_shuttle_buttons;
    void initDevices();
    void loadTranscodeProfiles();
    void saveTranscodeProfiles();
    void loadCurrentV4lProfileInfo();
    void saveCurrentV4lProfile();
    void loadEncodingProfiles();
    void setupJogshuttleBtns(const QString &device);
    /** @brief Fill a combobox with the found blackmagic devices */
    static bool getBlackMagicDeviceList(QComboBox *devicelist, bool force = false);
    static bool getBlackMagicOutputDeviceList(QComboBox *devicelist, bool force = false);
    /** @brief Init QtMultimedia audio record settings */
    bool initAudioRecDevice();
signals:
    void customChanged();
    void doResetConsumer(bool fullReset);
    void updateCaptureFolder();
    void updateLibraryFolder();
    // Screengrab method changed between fullsceen and region, update rec monitor
    void updateFullScreenGrab();
    /** @brief A settings changed that requires a Kdenlive restart, trigger it */
    void restartKdenlive(bool resetConfig = false);
    void checkTabPosition();
    /** @brief Switch between merged / separate channels for audio thumbs */
    void audioThumbFormatChanged();
    /** @brief An important timeline property changed, prepare for a reset */
    void resetView();
    /** @brief Monitor background color changed, update monitors */
    void updateMonitorBg();
};

#endif
