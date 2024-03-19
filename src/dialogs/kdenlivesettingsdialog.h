/*
    SPDX-FileCopyrightText: 2008 Jean-Baptiste Mardelle <jb@kdenlive.org>

SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include <KConfigDialog>
#include <KProcess>
#include <QMap>
#include <QListWidget>

#include "ui_configcapture_ui.h"
#include "ui_configcolors_ui.h"
#include "ui_configenv_ui.h"
#include "ui_configjogshuttle_ui.h"
#include "ui_configmisc_ui.h"
#include "ui_configproject_ui.h"
#include "ui_configproxy_ui.h"
#include "ui_configsdl_ui.h"
#include "ui_configspeech_ui.h"
#include "ui_configtimeline_ui.h"
#include "ui_configtools_ui.h"
#include "ui_configtranscode_ui.h"

#include "pythoninterfaces/speechtotext.h"
#include "encodingprofilesdialog.h"

class ProfileWidget;
class GuideCategories;
class KJob;

class SpeechList : public QListWidget
{
    Q_OBJECT

public:
    SpeechList(QWidget *parent = nullptr);

protected:
    QStringList mimeTypes() const override;
    void dropEvent(QDropEvent *event) override;

Q_SIGNALS:
    void getDictionary(const QUrl url);
};

class KdenliveSettingsDialog : public KConfigDialog
{
    Q_OBJECT

public:
    KdenliveSettingsDialog(QMap<QString, QString> mappable_actions, bool gpuAllowed, QWidget *parent = nullptr);
    ~KdenliveSettingsDialog() override;
    void showPage(Kdenlive::ConfigPage page, int option);
    void checkProfile();
    /** @brief update kdenlive settings for external app path if they were changed outside config dialog */
    void updateExternalApps();

protected:
    void showHelp() override;

protected Q_SLOTS:
    void updateSettings() override;
    void updateWidgets() override;
    bool hasChanged() override;
    void accept() override;

private Q_SLOTS:
    void slotCheckShuttle(int state = 0);
    void slotUpdateShuttleDevice(int ix = 0);
    void slotEditImageApplication();
    void slotEditAudioApplication();
    void slotEditGlaxnimateApplication();
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
    void slotRevealCaptureFolder(int ix);
    void slotEnableLibraryFolder();
    void slotEnableVideoFolder(int ix);
    void slotUpdatev4lDevice();
    void slotUpdatev4lCaptureProfile();
    void slotEditVideo4LinuxProfile();
    void slotReloadBlackMagic();
    void slotReloadShuttleDevices();
    void loadExternalProxyProfiles();
    void slotParseVoskDictionaries();
    void getDictionary(const QUrl &sourceUrl = QUrl());
    void removeDictionary();
    void downloadModelFinished(KJob* job);
    void processArchive(const QString &path);
    void doShowSpeechMessage(const QString &message, int messageType);
    /** @brief Check required python dependencies for speech engine */
    void slotCheckSttConfig();
    /** @brief fill list of connected monitors */
    void fillMonitorData();
    /** @brief Open external proxies config dialog */
    void configureExternalProxies();

private:
    KPageWidgetItem *m_pageMisc;
    KPageWidgetItem *m_pageEnv;
    KPageWidgetItem *m_pageTimeline;
    KPageWidgetItem *m_pageTools;
    KPageWidgetItem *m_pageCapture;
    KPageWidgetItem *m_pageJog;
    KPageWidgetItem *m_pagePlay;
    KPageWidgetItem *m_pageTranscode;
    KPageWidgetItem *m_pageProject;
    KPageWidgetItem *m_pageColors;
    KPageWidgetItem *m_pageSpeech;
    Ui::ConfigEnv_UI m_configEnv;
    Ui::ConfigMisc_UI m_configMisc;
    Ui::ConfigColors_UI m_configColors;
    Ui::ConfigTimeline_UI m_configTimeline;
    Ui::ConfigTools_UI m_configTools;
    Ui::ConfigCapture_UI m_configCapture;
    Ui::ConfigJogShuttle_UI m_configShuttle;
    Ui::ConfigSdl_UI m_configSdl;
    Ui::ConfigTranscode_UI m_configTranscode;
    Ui::ConfigProject_UI m_configProject;
    Ui::ConfigProxy_UI m_configProxy;
    Ui::ConfigSpeech_UI m_configSpeech;
    SpeechList *m_speechListWidget;
    GuideCategories *m_guidesCategories;
    ProfileWidget *m_pw;
    KProcess m_readProcess;
    bool m_modified;
    bool m_shuttleModified;
    bool m_voskUpdated;
    SpeechToText *m_stt;
    SpeechToText *m_sttWhisper;
    QMap<QString, QString> m_mappable_actions;
    QVector<QComboBox *> m_shuttle_buttons;
    EncodingTimelinePreviewProfilesChooser *m_tlPreviewProfiles;
    EncodingProfilesChooser *m_proxyProfiles;
    EncodingProfilesChooser *m_decklinkProfiles;
    EncodingProfilesChooser *m_v4lProfiles;
    EncodingProfilesChooser *m_grabProfiles;
    void initDevices();
    void loadTranscodeProfiles();
    void saveTranscodeProfiles();
    void loadCurrentV4lProfileInfo();
    void saveCurrentV4lProfile();
    void setupJogshuttleBtns(const QString &device);
    /** @brief Fill a combobox with the found blackmagic devices */
    static bool getBlackMagicDeviceList(QComboBox *devicelist, bool force = false);
    static bool getBlackMagicOutputDeviceList(QComboBox *devicelist, bool force = false);
    /** @brief Init QtMultimedia audio record settings */
    bool initAudioRecDevice();
    void initMiscPage();
    void initProjectPage();
    void initProxyPage();
    void initTimelinePage();
    void initEnviromentPage();
    void initColorsPage();
    /** @brief Init Speech to text settings */
    void initSpeechPage();
    void initCapturePage();
    void initJogShuttlePage();
    void initSdlPage(bool gpuAllowed);
    void initTranscodePage();
    /** @brief Launch pytonh scripts to check speech engine dependencies */
    void checkSpeechDependencies();

Q_SIGNALS:
    void customChanged();
    void doResetConsumer(bool fullReset);
    void updateCaptureFolder();
    void updateLibraryFolder();
    /** @brief Screengrab method changed between fullsceen and region, update rec monitor */
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
    /** @brief Trigger parsing of the speech models folder */
    void parseDictionaries();
    /** @brief audio volume or rec channels changed, update audio monitor view */
    void resetAudioMonitoring();
};
