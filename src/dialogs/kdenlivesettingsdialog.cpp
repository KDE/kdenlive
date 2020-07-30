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
#include "clipcreationdialog.h"
#include "core.h"
#include "dialogs/profilesdialog.h"
#include "encodingprofilesdialog.h"
#include "kdenlivesettings.h"
#include "mainwindow.h"
#include "timeline2/view/timelinewidget.h"
#include "timeline2/view/timelinecontroller.h"
#include "profiles/profilemodel.hpp"
#include "profiles/profilerepository.hpp"
#include "profilesdialog.h"
#include "project/dialogs/profilewidget.h"
#include "wizard.h"

#ifdef USE_V4L
#include "capture/v4lcapture.h"
#endif

#include "kdenlive_debug.h"
#include "klocalizedstring.h"
#include <KIO/DesktopExecParser>
#include <KLineEdit>
#include <KMessageBox>
#include <KOpenWithDialog>
#include <KService>
#include <QAction>
#include <QDir>
#include <QGuiApplication>
#include <QScreen>
#include <QSize>
#include <QThread>
#include <QTimer>
#include <cstdio>
#include <cstdlib>
#include <fcntl.h>
#include <unistd.h>

#ifdef USE_JOGSHUTTLE
#include "jogshuttle/jogaction.h"
#include "jogshuttle/jogshuttleconfig.h"
#include <QStandardPaths>
#include <linux/input.h>
#endif

KdenliveSettingsDialog::KdenliveSettingsDialog(QMap<QString, QString> mappable_actions, bool gpuAllowed, QWidget *parent)
    : KConfigDialog(parent, QStringLiteral("settings"), KdenliveSettings::self())
    , m_modified(false)
    , m_shuttleModified(false)
    , m_mappable_actions(std::move(mappable_actions))
{
    KdenliveSettings::setV4l_format(0);
    QWidget *p1 = new QWidget;
    m_configMisc.setupUi(p1);
    m_page1 = addPage(p1, i18n("Misc"));
    m_page1->setIcon(QIcon::fromTheme(QStringLiteral("configure")));

    m_configMisc.kcfg_use_exiftool->setEnabled(!QStandardPaths::findExecutable(QStringLiteral("exiftool")).isEmpty());

    QWidget *p8 = new QWidget;
    m_configProject.setupUi(p8);
    m_page8 = addPage(p8, i18n("Project Defaults"));
    auto *vbox = new QVBoxLayout;
    m_pw = new ProfileWidget(this);
    vbox->addWidget(m_pw);
    m_configProject.profile_box->setLayout(vbox);
    m_configProject.profile_box->setTitle(i18n("Select the default profile (preset)"));
    // Select profile
    m_pw->loadProfile(KdenliveSettings::default_profile().isEmpty() ? pCore->getCurrentProfile()->path() : KdenliveSettings::default_profile());
    connect(m_pw, &ProfileWidget::profileChanged, this, &KdenliveSettingsDialog::slotDialogModified);
    m_page8->setIcon(QIcon::fromTheme(QStringLiteral("project-defaults")));
    m_configProject.projecturl->setMode(KFile::Directory);
    m_configProject.projecturl->setUrl(QUrl::fromLocalFile(KdenliveSettings::defaultprojectfolder()));
    connect(m_configProject.kcfg_videotracks, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), this, [this]() {
        if (m_configProject.kcfg_videotracks->value() + m_configProject.kcfg_audiotracks->value() <= 0) {
            m_configProject.kcfg_videotracks->setValue(1);
        }
    });
    connect(m_configProject.kcfg_audiotracks, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), this, [this] () {
        if (m_configProject.kcfg_videotracks->value() + m_configProject.kcfg_audiotracks->value() <= 0) {
            m_configProject.kcfg_audiotracks->setValue(1);
        }
    });

    QWidget *p9 = new QWidget;
    m_configProxy.setupUi(p9);
    KPageWidgetItem *page9 = addPage(p9, i18n("Proxy Clips"));
    page9->setIcon(QIcon::fromTheme(QStringLiteral("zoom-out")));
    connect(m_configProxy.kcfg_generateproxy, &QAbstractButton::toggled, m_configProxy.kcfg_proxyminsize, &QWidget::setEnabled);
    m_configProxy.kcfg_proxyminsize->setEnabled(KdenliveSettings::generateproxy());
    connect(m_configProxy.kcfg_generateimageproxy, &QAbstractButton::toggled, m_configProxy.kcfg_proxyimageminsize, &QWidget::setEnabled);
    m_configProxy.kcfg_proxyimageminsize->setEnabled(KdenliveSettings::generateimageproxy());
    loadExternalProxyProfiles();

    QWidget *p3 = new QWidget;
    m_configTimeline.setupUi(p3);
    m_page3 = addPage(p3, i18n("Timeline"));
    m_page3->setIcon(QIcon::fromTheme(QStringLiteral("video-display")));

    QWidget *p2 = new QWidget;
    m_configEnv.setupUi(p2);
    m_configEnv.mltpathurl->setMode(KFile::Directory);
    m_configEnv.mltpathurl->lineEdit()->setObjectName(QStringLiteral("kcfg_mltpath"));
    m_configEnv.rendererpathurl->lineEdit()->setObjectName(QStringLiteral("kcfg_rendererpath"));
    m_configEnv.ffmpegurl->lineEdit()->setObjectName(QStringLiteral("kcfg_ffmpegpath"));
    m_configEnv.ffplayurl->lineEdit()->setObjectName(QStringLiteral("kcfg_ffplaypath"));
    m_configEnv.ffprobeurl->lineEdit()->setObjectName(QStringLiteral("kcfg_ffprobepath"));
    m_configEnv.tmppathurl->setMode(KFile::Directory);
    m_configEnv.tmppathurl->lineEdit()->setObjectName(QStringLiteral("kcfg_currenttmpfolder"));
    m_configEnv.capturefolderurl->setMode(KFile::Directory);
    m_configEnv.capturefolderurl->lineEdit()->setObjectName(QStringLiteral("kcfg_capturefolder"));
    m_configEnv.capturefolderurl->setEnabled(!KdenliveSettings::capturetoprojectfolder());
    connect(m_configEnv.kcfg_capturetoprojectfolder, &QAbstractButton::clicked, this, &KdenliveSettingsDialog::slotEnableCaptureFolder);
    // Library folder
    m_configEnv.libraryfolderurl->setMode(KFile::Directory);
    m_configEnv.libraryfolderurl->lineEdit()->setObjectName(QStringLiteral("kcfg_libraryfolder"));
    m_configEnv.libraryfolderurl->setEnabled(!KdenliveSettings::librarytodefaultfolder());
    m_configEnv.kcfg_librarytodefaultfolder->setToolTip(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + QStringLiteral("/library"));
    connect(m_configEnv.kcfg_librarytodefaultfolder, &QAbstractButton::clicked, this, &KdenliveSettingsDialog::slotEnableLibraryFolder);

    // Mime types
    QStringList mimes = ClipCreationDialog::getExtensions();
    std::sort(mimes.begin(), mimes.end());
    m_configEnv.supportedmimes->setPlainText(mimes.join(QLatin1Char(' ')));

    m_page2 = addPage(p2, i18n("Environment"));
    m_page2->setIcon(QIcon::fromTheme(QStringLiteral("application-x-executable-script")));

    QWidget *p10 = new QWidget;
    m_configColors.setupUi(p10);
    m_page10 = addPage(p10, i18n("Colors"));
    m_page10->setIcon(QIcon::fromTheme(QStringLiteral("color-management")));
    
    QWidget *p4 = new QWidget;
    m_configCapture.setupUi(p4);
    // Remove ffmpeg tab, unused
    m_configCapture.tabWidget->removeTab(0);
    m_configCapture.label->setVisible(false);
    m_configCapture.kcfg_defaultcapture->setVisible(false);
    //m_configCapture.tabWidget->removeTab(2);
#ifdef USE_V4L

    // Video 4 Linux device detection
    for (int i = 0; i < 10; ++i) {
        QString path = QStringLiteral("/dev/video") + QString::number(i);
        if (QFile::exists(path)) {
            QStringList deviceInfo = V4lCaptureHandler::getDeviceName(path);
            if (!deviceInfo.isEmpty()) {
                m_configCapture.kcfg_detectedv4ldevices->addItem(deviceInfo.at(0), path);
                m_configCapture.kcfg_detectedv4ldevices->setItemData(m_configCapture.kcfg_detectedv4ldevices->count() - 1, deviceInfo.at(1), Qt::UserRole + 1);
            }
        }
    }
    connect(m_configCapture.kcfg_detectedv4ldevices, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this,
            &KdenliveSettingsDialog::slotUpdatev4lDevice);
    connect(m_configCapture.kcfg_v4l_format, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this,
            &KdenliveSettingsDialog::slotUpdatev4lCaptureProfile);
    connect(m_configCapture.config_v4l, &QAbstractButton::clicked, this, &KdenliveSettingsDialog::slotEditVideo4LinuxProfile);

    slotUpdatev4lDevice();
#endif

    m_page4 = addPage(p4, i18n("Capture"));
    m_page4->setIcon(QIcon::fromTheme(QStringLiteral("media-record")));
    m_configCapture.tabWidget->setCurrentIndex(KdenliveSettings::defaultcapture());
#ifdef Q_WS_MAC
    m_configCapture.tabWidget->setEnabled(false);
    m_configCapture.kcfg_defaultcapture->setEnabled(false);
    m_configCapture.label->setText(i18n("Capture is not yet available on Mac OS X."));
#endif

    QWidget *p5 = new QWidget;
    m_configShuttle.setupUi(p5);
#ifdef USE_JOGSHUTTLE
    m_configShuttle.toolBtnReload->setIcon(QIcon::fromTheme(QStringLiteral("view-refresh")));
    connect(m_configShuttle.kcfg_enableshuttle, &QCheckBox::stateChanged, this, &KdenliveSettingsDialog::slotCheckShuttle);
    connect(m_configShuttle.shuttledevicelist, SIGNAL(activated(int)), this, SLOT(slotUpdateShuttleDevice(int)));
    connect(m_configShuttle.toolBtnReload, &QAbstractButton::clicked, this, &KdenliveSettingsDialog::slotReloadShuttleDevices);

    slotCheckShuttle(static_cast<int>(KdenliveSettings::enableshuttle()));
    m_configShuttle.shuttledisabled->hide();

    // Store the button pointers into an array for easier handling them in the other functions.
    // TODO: impl enumerator or live with cut and paste :-)))
    setupJogshuttleBtns(KdenliveSettings::shuttledevice());
#if 0
    m_shuttle_buttons.push_back(m_configShuttle.shuttle1);
    m_shuttle_buttons.push_back(m_configShuttle.shuttle2);
    m_shuttle_buttons.push_back(m_configShuttle.shuttle3);
    m_shuttle_buttons.push_back(m_configShuttle.shuttle4);
    m_shuttle_buttons.push_back(m_configShuttle.shuttle5);
    m_shuttle_buttons.push_back(m_configShuttle.shuttle6);
    m_shuttle_buttons.push_back(m_configShuttle.shuttle7);
    m_shuttle_buttons.push_back(m_configShuttle.shuttle8);
    m_shuttle_buttons.push_back(m_configShuttle.shuttle9);
    m_shuttle_buttons.push_back(m_configShuttle.shuttle10);
    m_shuttle_buttons.push_back(m_configShuttle.shuttle11);
    m_shuttle_buttons.push_back(m_configShuttle.shuttle12);
    m_shuttle_buttons.push_back(m_configShuttle.shuttle13);
    m_shuttle_buttons.push_back(m_configShuttle.shuttle14);
    m_shuttle_buttons.push_back(m_configShuttle.shuttle15);
#endif

#else  /* ! USE_JOGSHUTTLE */
    m_configShuttle.kcfg_enableshuttle->hide();
    m_configShuttle.kcfg_enableshuttle->setDisabled(true);
#endif /* USE_JOGSHUTTLE */
    m_page5 = addPage(p5, i18n("JogShuttle"));
    m_page5->setIcon(QIcon::fromTheme(QStringLiteral("dialog-input-devices")));

    QWidget *p6 = new QWidget;
    m_configSdl.setupUi(p6);
    m_configSdl.reload_blackmagic->setIcon(QIcon::fromTheme(QStringLiteral("view-refresh")));
    connect(m_configSdl.reload_blackmagic, &QAbstractButton::clicked, this, &KdenliveSettingsDialog::slotReloadBlackMagic);

    // m_configSdl.kcfg_openglmonitors->setHidden(true);

    m_page6 = addPage(p6, i18n("Playback"));
    m_page6->setIcon(QIcon::fromTheme(QStringLiteral("media-playback-start")));

    QWidget *p7 = new QWidget;
    m_configTranscode.setupUi(p7);
    m_page7 = addPage(p7, i18n("Transcode"));
    m_page7->setIcon(QIcon::fromTheme(QStringLiteral("edit-copy")));

    connect(m_configTranscode.button_add, &QAbstractButton::clicked, this, &KdenliveSettingsDialog::slotAddTranscode);
    connect(m_configTranscode.button_delete, &QAbstractButton::clicked, this, &KdenliveSettingsDialog::slotDeleteTranscode);
    connect(m_configTranscode.profiles_list, &QListWidget::itemChanged, this, &KdenliveSettingsDialog::slotDialogModified);
    connect(m_configTranscode.profiles_list, &QListWidget::currentRowChanged, this, &KdenliveSettingsDialog::slotSetTranscodeProfile);

    connect(m_configTranscode.profile_name, &QLineEdit::textChanged, this, &KdenliveSettingsDialog::slotEnableTranscodeUpdate);
    connect(m_configTranscode.profile_description, &QLineEdit::textChanged, this, &KdenliveSettingsDialog::slotEnableTranscodeUpdate);
    connect(m_configTranscode.profile_extension, &QLineEdit::textChanged, this, &KdenliveSettingsDialog::slotEnableTranscodeUpdate);
    connect(m_configTranscode.profile_parameters, &QPlainTextEdit::textChanged, this, &KdenliveSettingsDialog::slotEnableTranscodeUpdate);
    connect(m_configTranscode.profile_audioonly, &QCheckBox::stateChanged, this, &KdenliveSettingsDialog::slotEnableTranscodeUpdate);

    connect(m_configTranscode.button_update, &QAbstractButton::pressed, this, &KdenliveSettingsDialog::slotUpdateTranscodingProfile);

    m_configTranscode.profile_parameters->setMaximumHeight(QFontMetrics(font()).lineSpacing() * 5);

    connect(m_configEnv.kp_image, &QAbstractButton::clicked, this, &KdenliveSettingsDialog::slotEditImageApplication);
    connect(m_configEnv.kp_audio, &QAbstractButton::clicked, this, &KdenliveSettingsDialog::slotEditAudioApplication);

    loadEncodingProfiles();

    connect(m_configSdl.kcfg_audio_driver, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this,
            &KdenliveSettingsDialog::slotCheckAlsaDriver);
    connect(m_configSdl.kcfg_audio_backend, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this,
            &KdenliveSettingsDialog::slotCheckAudioBackend);
    initDevices();
    connect(m_configCapture.kcfg_grab_capture_type, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this,
            &KdenliveSettingsDialog::slotUpdateGrabRegionStatus);

    slotUpdateGrabRegionStatus();
    loadTranscodeProfiles();

    // decklink profile
    QAction *act = new QAction(QIcon::fromTheme(QStringLiteral("configure")), i18n("Configure profiles"), this);
    act->setData(4);
    connect(act, &QAction::triggered, this, &KdenliveSettingsDialog::slotManageEncodingProfile);
    m_configCapture.decklink_manageprofile->setDefaultAction(act);
    m_configCapture.decklink_showprofileinfo->setIcon(QIcon::fromTheme(QStringLiteral("help-about")));
    m_configCapture.decklink_parameters->setVisible(false);
    m_configCapture.decklink_parameters->setMaximumHeight(QFontMetrics(font()).lineSpacing() * 4);
    m_configCapture.decklink_parameters->setPlainText(KdenliveSettings::decklink_parameters());
    connect(m_configCapture.kcfg_decklink_profile, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this,
            &KdenliveSettingsDialog::slotUpdateDecklinkProfile);
    connect(m_configCapture.decklink_showprofileinfo, &QAbstractButton::clicked, m_configCapture.decklink_parameters, &QWidget::setVisible);

    // ffmpeg profile
    m_configCapture.v4l_showprofileinfo->setIcon(QIcon::fromTheme(QStringLiteral("help-about")));
    m_configCapture.v4l_parameters->setVisible(false);
    m_configCapture.v4l_parameters->setMaximumHeight(QFontMetrics(font()).lineSpacing() * 4);
    m_configCapture.v4l_parameters->setPlainText(KdenliveSettings::v4l_parameters());

    act = new QAction(QIcon::fromTheme(QStringLiteral("configure")), i18n("Configure profiles"), this);
    act->setData(2);
    connect(act, &QAction::triggered, this, &KdenliveSettingsDialog::slotManageEncodingProfile);
    m_configCapture.v4l_manageprofile->setDefaultAction(act);
    connect(m_configCapture.kcfg_v4l_profile, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this,
            &KdenliveSettingsDialog::slotUpdateV4lProfile);
    connect(m_configCapture.v4l_showprofileinfo, &QAbstractButton::clicked, m_configCapture.v4l_parameters, &QWidget::setVisible);

    // screen grab profile
    m_configCapture.grab_showprofileinfo->setIcon(QIcon::fromTheme(QStringLiteral("help-about")));
    m_configCapture.grab_parameters->setVisible(false);
    m_configCapture.grab_parameters->setMaximumHeight(QFontMetrics(font()).lineSpacing() * 4);
    m_configCapture.grab_parameters->setPlainText(KdenliveSettings::grab_parameters());
    act = new QAction(QIcon::fromTheme(QStringLiteral("configure")), i18n("Configure profiles"), this);
    act->setData(3);
    connect(act, &QAction::triggered, this, &KdenliveSettingsDialog::slotManageEncodingProfile);
    m_configCapture.grab_manageprofile->setDefaultAction(act);
    connect(m_configCapture.kcfg_grab_profile, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this,
            &KdenliveSettingsDialog::slotUpdateGrabProfile);
    connect(m_configCapture.grab_showprofileinfo, &QAbstractButton::clicked, m_configCapture.grab_parameters, &QWidget::setVisible);

    // audio capture channels
    m_configCapture.audiocapturechannels->clear();
    m_configCapture.audiocapturechannels->addItem(i18n("Mono (1 channel)"), 1);
    m_configCapture.audiocapturechannels->addItem(i18n("Stereo (2 channels)"), 2);

    int channelsIndex = m_configCapture.audiocapturechannels->findData(KdenliveSettings::audiocapturechannels());
    m_configCapture.audiocapturechannels->setCurrentIndex(qMax(channelsIndex, 0));
    connect(m_configCapture.audiocapturechannels, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this,
            &KdenliveSettingsDialog::slotUpdateAudioCaptureChannels);

    // audio capture sample rate
    m_configCapture.audiocapturesamplerate->clear();
    m_configCapture.audiocapturesamplerate->addItem(i18n("44100 Hz"), 44100);
    m_configCapture.audiocapturesamplerate->addItem(i18n("48000 Hz"), 48000);

    int sampleRateIndex = m_configCapture.audiocapturesamplerate->findData(KdenliveSettings::audiocapturesamplerate());
    m_configCapture.audiocapturesamplerate->setCurrentIndex(qMax(sampleRateIndex, 0));
    connect(m_configCapture.audiocapturesamplerate, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this,
            &KdenliveSettingsDialog::slotUpdateAudioCaptureSampleRate);

    m_configCapture.labelNoAudioDevices->setVisible(false);

    // Timeline preview
    act = new QAction(QIcon::fromTheme(QStringLiteral("configure")), i18n("Configure profiles"), this);
    act->setData(1);
    connect(act, &QAction::triggered, this, &KdenliveSettingsDialog::slotManageEncodingProfile);
    m_configProject.preview_manageprofile->setDefaultAction(act);
    connect(m_configProject.kcfg_preview_profile, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this,
            &KdenliveSettingsDialog::slotUpdatePreviewProfile);
    connect(m_configProject.preview_showprofileinfo, &QAbstractButton::clicked, m_configProject.previewparams, &QWidget::setVisible);
    m_configProject.previewparams->setVisible(false);
    m_configProject.previewparams->setMaximumHeight(QFontMetrics(font()).lineSpacing() * 3);
    m_configProject.previewparams->setPlainText(KdenliveSettings::previewparams());
    m_configProject.preview_showprofileinfo->setIcon(QIcon::fromTheme(QStringLiteral("help-about")));
    m_configProject.preview_showprofileinfo->setToolTip(i18n("Show default timeline preview parameters"));
    m_configProject.preview_manageprofile->setIcon(QIcon::fromTheme(QStringLiteral("configure")));
    m_configProject.preview_manageprofile->setToolTip(i18n("Manage timeline preview profiles"));
    m_configProject.kcfg_preview_profile->setToolTip(i18n("Select default timeline preview profile"));

    // proxy profile stuff
    m_configProxy.proxy_showprofileinfo->setIcon(QIcon::fromTheme(QStringLiteral("help-about")));
    m_configProxy.proxy_showprofileinfo->setToolTip(i18n("Show default profile parameters"));
    m_configProxy.proxy_manageprofile->setIcon(QIcon::fromTheme(QStringLiteral("configure")));
    m_configProxy.proxy_manageprofile->setToolTip(i18n("Manage proxy profiles"));
    m_configProxy.kcfg_proxy_profile->setToolTip(i18n("Select default proxy profile"));
    m_configProxy.proxyparams->setVisible(false);
    m_configProxy.proxyparams->setMaximumHeight(QFontMetrics(font()).lineSpacing() * 3);
    m_configProxy.proxyparams->setPlainText(KdenliveSettings::proxyparams());

    act = new QAction(QIcon::fromTheme(QStringLiteral("configure")), i18n("Configure profiles"), this);
    act->setData(0);
    connect(act, &QAction::triggered, this, &KdenliveSettingsDialog::slotManageEncodingProfile);
    m_configProxy.proxy_manageprofile->setDefaultAction(act);

    connect(m_configProxy.proxy_showprofileinfo, &QAbstractButton::clicked, m_configProxy.proxyparams, &QWidget::setVisible);
    connect(m_configProxy.kcfg_proxy_profile, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this,
            &KdenliveSettingsDialog::slotUpdateProxyProfile);

    slotUpdateProxyProfile(-1);
    slotUpdateV4lProfile(-1);
    slotUpdateGrabProfile(-1);
    slotUpdateDecklinkProfile(-1);

    // enable GPU accel only if Movit is found
    m_configSdl.kcfg_gpu_accel->setEnabled(gpuAllowed);
    m_configSdl.kcfg_gpu_accel->setToolTip(i18n("GPU processing needs MLT compiled with Movit and Rtaudio modules"));

    getBlackMagicDeviceList(m_configCapture.kcfg_decklink_capturedevice);
    if (!getBlackMagicOutputDeviceList(m_configSdl.kcfg_blackmagic_output_device)) {
        // No blackmagic card found
        m_configSdl.kcfg_external_display->setEnabled(false);
    }
    
    initAudioRecDevice();

    // Config dialog size
    KSharedConfigPtr config = KSharedConfig::openConfig();
    KConfigGroup settingsGroup(config, "settings");
    QSize optimalSize;

    if (!settingsGroup.exists() || !settingsGroup.hasKey("dialogSize")) {
        const QSize screenSize = (QGuiApplication::primaryScreen()->availableSize() * 0.9);
        const QSize targetSize = QSize(1024, 700);
        optimalSize = targetSize.boundedTo(screenSize);
    } else {
        optimalSize = settingsGroup.readEntry("dialogSize", QVariant(size())).toSize();
    }
    resize(optimalSize);
}

// static
bool KdenliveSettingsDialog::getBlackMagicDeviceList(QComboBox *devicelist, bool force)
{
    if (!force && !KdenliveSettings::decklink_device_found()) {
        return false;
    }
    Mlt::Profile profile;
    Mlt::Producer bm(profile, "decklink");
    int found_devices = 0;
    if (bm.is_valid()) {
        bm.set("list_devices", 1);
        found_devices = bm.get_int("devices");
    } else {
        KdenliveSettings::setDecklink_device_found(false);
    }
    if (found_devices <= 0) {
        devicelist->setEnabled(false);
        return false;
    }
    KdenliveSettings::setDecklink_device_found(true);
    for (int i = 0; i < found_devices; ++i) {
        char *tmp = qstrdup(QStringLiteral("device.%1").arg(i).toUtf8().constData());
        devicelist->addItem(bm.get(tmp));
        delete[] tmp;
    }
    return true;
}

// static
bool KdenliveSettingsDialog::initAudioRecDevice()
{
    QStringList audioDevices = pCore->getAudioCaptureDevices();

    //show a hint to the user to know what to check for in case the device list are empty (common issue)
    m_configCapture.labelNoAudioDevices->setVisible(audioDevices.empty());

    m_configCapture.kcfg_defaultaudiocapture->addItems(audioDevices);
    connect(m_configCapture.kcfg_defaultaudiocapture, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this, [&]() {
        QString currentDevice = m_configCapture.kcfg_defaultaudiocapture->currentText();
        KdenliveSettings::setDefaultaudiocapture(currentDevice);
    });
    QString selectedDevice = KdenliveSettings::defaultaudiocapture();
    int selectedIndex = m_configCapture.kcfg_defaultaudiocapture->findText(selectedDevice);
    if (!selectedDevice.isEmpty() && selectedIndex > -1) {
        m_configCapture.kcfg_defaultaudiocapture->setCurrentIndex(selectedIndex);
    }
    return true;
}

bool KdenliveSettingsDialog::getBlackMagicOutputDeviceList(QComboBox *devicelist, bool force)
{
    if (!force && !KdenliveSettings::decklink_device_found()) {
        return false;
    }
    Mlt::Profile profile;
    Mlt::Consumer bm(profile, "decklink");
    int found_devices = 0;
    if (bm.is_valid()) {
        bm.set("list_devices", 1);
        found_devices = bm.get_int("devices");
    } else {
        KdenliveSettings::setDecklink_device_found(false);
    }
    if (found_devices <= 0) {
        devicelist->setEnabled(false);
        return false;
    }
    KdenliveSettings::setDecklink_device_found(true);
    for (int i = 0; i < found_devices; ++i) {
        char *tmp = qstrdup(QStringLiteral("device.%1").arg(i).toUtf8().constData());
        devicelist->addItem(bm.get(tmp));
        delete[] tmp;
    }
    devicelist->addItem(QStringLiteral("test"));
    return true;
}

void KdenliveSettingsDialog::setupJogshuttleBtns(const QString &device)
{
    QList<QComboBox *> list;
    QList<QLabel *> list1;

    list << m_configShuttle.shuttle1;
    list << m_configShuttle.shuttle2;
    list << m_configShuttle.shuttle3;
    list << m_configShuttle.shuttle4;
    list << m_configShuttle.shuttle5;
    list << m_configShuttle.shuttle6;
    list << m_configShuttle.shuttle7;
    list << m_configShuttle.shuttle8;
    list << m_configShuttle.shuttle9;
    list << m_configShuttle.shuttle10;
    list << m_configShuttle.shuttle11;
    list << m_configShuttle.shuttle12;
    list << m_configShuttle.shuttle13;
    list << m_configShuttle.shuttle14;
    list << m_configShuttle.shuttle15;

    list1 << m_configShuttle.label_2;  // #1
    list1 << m_configShuttle.label_4;  // #2
    list1 << m_configShuttle.label_3;  // #3
    list1 << m_configShuttle.label_7;  // #4
    list1 << m_configShuttle.label_5;  // #5
    list1 << m_configShuttle.label_6;  // #6
    list1 << m_configShuttle.label_8;  // #7
    list1 << m_configShuttle.label_9;  // #8
    list1 << m_configShuttle.label_10; // #9
    list1 << m_configShuttle.label_11; // #10
    list1 << m_configShuttle.label_12; // #11
    list1 << m_configShuttle.label_13; // #12
    list1 << m_configShuttle.label_14; // #13
    list1 << m_configShuttle.label_15; // #14
    list1 << m_configShuttle.label_16; // #15

    for (int i = 0; i < list.count(); ++i) {
        list[i]->hide();
        list1[i]->hide();
    }
#ifdef USE_JOGSHUTTLE
    if (!m_configShuttle.kcfg_enableshuttle->isChecked()) {
        return;
    }
    int keysCount = JogShuttle::keysCount(device);

    for (int i = 0; i < keysCount; ++i) {
        m_shuttle_buttons.push_back(list[i]);
        list[i]->show();
        list1[i]->show();
    }

    // populate the buttons with the current configuration. The items are sorted
    // according to the user-selected language, so they do not appear in random order.
    QMap<QString, QString> mappable_actions(m_mappable_actions);
    QList<QString> action_names = mappable_actions.keys();
    QList<QString>::Iterator iter = action_names.begin();
    // qCDebug(KDENLIVE_LOG) << "::::::::::::::::";
    while (iter != action_names.end()) {
        // qCDebug(KDENLIVE_LOG) << *iter;
        ++iter;
    }

    // qCDebug(KDENLIVE_LOG) << "::::::::::::::::";

    std::sort(action_names.begin(), action_names.end());
    iter = action_names.begin();
    while (iter != action_names.end()) {
        // qCDebug(KDENLIVE_LOG) << *iter;
        ++iter;
    }
    // qCDebug(KDENLIVE_LOG) << "::::::::::::::::";

    // Here we need to compute the action_id -> index-in-action_names. We iterate over the
    // action_names, as the sorting may depend on the user-language.
    QStringList actions_map = JogShuttleConfig::actionMap(KdenliveSettings::shuttlebuttons());
    QMap<QString, int> action_pos;
    for (const QString &action_id : qAsConst(actions_map)) {
        // This loop find out at what index is the string that would map to the action_id.
        for (int i = 0; i < action_names.size(); ++i) {
            if (mappable_actions[action_names.at(i)] == action_id) {
                action_pos[action_id] = i;
                break;
            }
        }
    }

    int i = 0;
    for (QComboBox *button : qAsConst(m_shuttle_buttons)) {
        button->addItems(action_names);
        connect(button, SIGNAL(activated(int)), this, SLOT(slotShuttleModified()));
        ++i;
        if (i < actions_map.size()) {
            button->setCurrentIndex(action_pos[actions_map[i]]);
        }
    }
#else
    Q_UNUSED(device)
#endif
}

KdenliveSettingsDialog::~KdenliveSettingsDialog() = default;

void KdenliveSettingsDialog::slotUpdateGrabRegionStatus()
{
    m_configCapture.region_group->setHidden(m_configCapture.kcfg_grab_capture_type->currentIndex() != 1);
}

void KdenliveSettingsDialog::slotEnableCaptureFolder()
{
    m_configEnv.capturefolderurl->setEnabled(!m_configEnv.kcfg_capturetoprojectfolder->isChecked());
}

void KdenliveSettingsDialog::slotEnableLibraryFolder()
{
    m_configEnv.libraryfolderurl->setEnabled(!m_configEnv.kcfg_librarytodefaultfolder->isChecked());
}

void KdenliveSettingsDialog::initDevices()
{
    // Fill audio drivers
    m_configSdl.kcfg_audio_driver->addItem(i18n("Automatic"), QString());
#if defined(Q_OS_WIN)
    //TODO: i18n
    m_configSdl.kcfg_audio_driver->addItem("DirectSound", "directsound");
    m_configSdl.kcfg_audio_driver->addItem("WinMM", "winmm");
    m_configSdl.kcfg_audio_driver->addItem("Wasapi", "wasapi");
#elif !defined(Q_WS_MAC)
    m_configSdl.kcfg_audio_driver->addItem(i18n("ALSA"), "alsa");
    m_configSdl.kcfg_audio_driver->addItem(i18n("PulseAudio"), "pulseaudio");
    m_configSdl.kcfg_audio_driver->addItem(i18n("OSS"), "dsp");
    //m_configSdl.kcfg_audio_driver->addItem(i18n("OSS with DMA access"), "dma");
    m_configSdl.kcfg_audio_driver->addItem(i18n("Esound daemon"), "esd");
    m_configSdl.kcfg_audio_driver->addItem(i18n("ARTS daemon"), "artsc");
#endif

    if (!KdenliveSettings::audiodrivername().isEmpty())
        for (int i = 1; i < m_configSdl.kcfg_audio_driver->count(); ++i) {
            if (m_configSdl.kcfg_audio_driver->itemData(i).toString() == KdenliveSettings::audiodrivername()) {
                m_configSdl.kcfg_audio_driver->setCurrentIndex(i);
                KdenliveSettings::setAudio_driver((uint)i);
            }
        }

    // Fill the list of audio playback / recording devices
    m_configSdl.kcfg_audio_device->addItem(i18n("Default"), QString());
    m_configCapture.kcfg_v4l_alsadevice->addItem(i18n("Default"), "default");
    if (!QStandardPaths::findExecutable(QStringLiteral("aplay")).isEmpty()) {
        m_readProcess.setOutputChannelMode(KProcess::OnlyStdoutChannel);
        m_readProcess.setProgram(QStringLiteral("aplay"), QStringList() << QStringLiteral("-l"));
        connect(&m_readProcess, &KProcess::readyReadStandardOutput, this, &KdenliveSettingsDialog::slotReadAudioDevices);
        m_readProcess.execute(5000);
    } else {
        // If aplay is not installed on the system, parse the /proc/asound/pcm file
        QFile file(QStringLiteral("/proc/asound/pcm"));
        if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QTextStream stream(&file);
            QString line = stream.readLine();
            QString deviceId;
            while (!line.isNull()) {
                if (line.contains(QStringLiteral("playback"))) {
                    deviceId = line.section(QLatin1Char(':'), 0, 0);
                    m_configSdl.kcfg_audio_device->addItem(line.section(QLatin1Char(':'), 1, 1), QStringLiteral("plughw:%1,%2")
                                                                                                     .arg(deviceId.section(QLatin1Char('-'), 0, 0).toInt())
                                                                                                     .arg(deviceId.section(QLatin1Char('-'), 1, 1).toInt()));
                }
                if (line.contains(QStringLiteral("capture"))) {
                    deviceId = line.section(QLatin1Char(':'), 0, 0);
                    m_configCapture.kcfg_v4l_alsadevice->addItem(
                        line.section(QLatin1Char(':'), 1, 1).simplified(),
                        QStringLiteral("hw:%1,%2").arg(deviceId.section(QLatin1Char('-'), 0, 0).toInt()).arg(deviceId.section(QLatin1Char('-'), 1, 1).toInt()));
                }
                line = stream.readLine();
            }
            file.close();
        } else {
            qCDebug(KDENLIVE_LOG) << " / / / /CANNOT READ PCM";
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

    if (!KdenliveSettings::v4l_alsadevicename().isEmpty()) {
        // Select correct alsa device
        int ix = m_configCapture.kcfg_v4l_alsadevice->findData(KdenliveSettings::v4l_alsadevicename());
        m_configCapture.kcfg_v4l_alsadevice->setCurrentIndex(ix);
        KdenliveSettings::setV4l_alsadevice(ix);
    }

    m_configSdl.kcfg_audio_backend->addItem(i18n("SDL"), KdenliveSettings::sdlAudioBackend());
    m_configSdl.kcfg_audio_backend->addItem(i18n("RtAudio"), "rtaudio");

    if (!KdenliveSettings::audiobackend().isEmpty()) {
        int ix = m_configSdl.kcfg_audio_backend->findData(KdenliveSettings::audiobackend());
        m_configSdl.kcfg_audio_backend->setCurrentIndex(ix);
        KdenliveSettings::setAudio_backend(ix);
    }
    m_configSdl.group_sdl->setEnabled(KdenliveSettings::audiobackend().startsWith(QLatin1String("sdl")));

    loadCurrentV4lProfileInfo();
}

void KdenliveSettingsDialog::slotReadAudioDevices()
{
    QString result = QString(m_readProcess.readAllStandardOutput());
    // qCDebug(KDENLIVE_LOG) << "// / / / / / READING APLAY: ";
    // qCDebug(KDENLIVE_LOG) << result;
    const QStringList lines = result.split(QLatin1Char('\n'));
    for (const QString &devicestr : lines) {
        ////qCDebug(KDENLIVE_LOG) << "// READING LINE: " << data;
        if (!devicestr.startsWith(QLatin1Char(' ')) && devicestr.count(QLatin1Char(':')) > 1) {
            QString card = devicestr.section(QLatin1Char(':'), 0, 0).section(QLatin1Char(' '), -1);
            QString device = devicestr.section(QLatin1Char(':'), 1, 1).section(QLatin1Char(' '), -1);
            m_configSdl.kcfg_audio_device->addItem(devicestr.section(QLatin1Char(':'), -1).simplified(), QStringLiteral("plughw:%1,%2").arg(card, device));
            m_configCapture.kcfg_v4l_alsadevice->addItem(devicestr.section(QLatin1Char(':'), -1).simplified(),
                                                         QStringLiteral("hw:%1,%2").arg(card, device));
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

void KdenliveSettingsDialog::slotEditAudioApplication()
{
    KService::Ptr service;
    QPointer<KOpenWithDialog> dlg = new KOpenWithDialog(QList<QUrl>(), i18n("Select default audio editor"), m_configEnv.kcfg_defaultaudioapp->text(), this);
    if (dlg->exec() == QDialog::Accepted) {
        service = dlg->service();
        m_configEnv.kcfg_defaultaudioapp->setText(KIO::DesktopExecParser::executablePath(service->exec()));
    }

    delete dlg;
}

void KdenliveSettingsDialog::slotEditImageApplication()
{
    QPointer<KOpenWithDialog> dlg = new KOpenWithDialog(QList<QUrl>(), i18n("Select default image editor"), m_configEnv.kcfg_defaultimageapp->text(), this);
    if (dlg->exec() == QDialog::Accepted) {
        KService::Ptr service = dlg->service();
        m_configEnv.kcfg_defaultimageapp->setText(KIO::DesktopExecParser::executablePath(service->exec()));
    }
    delete dlg;
}

void KdenliveSettingsDialog::slotCheckShuttle(int state)
{
#ifdef USE_JOGSHUTTLE
    m_configShuttle.config_group->setEnabled(state != 0);
    m_configShuttle.shuttledevicelist->clear();

    QStringList devNames = KdenliveSettings::shuttledevicenames();
    QStringList devPaths = KdenliveSettings::shuttledevicepaths();

    if (devNames.count() != devPaths.count()) {
        return;
    }
    for (int i = 0; i < devNames.count(); ++i) {
        m_configShuttle.shuttledevicelist->addItem(devNames.at(i), devPaths.at(i));
    }
    if (state != 0) {
        setupJogshuttleBtns(m_configShuttle.shuttledevicelist->itemData(m_configShuttle.shuttledevicelist->currentIndex()).toString());
    }
#else
    Q_UNUSED(state)
#endif /* USE_JOGSHUTTLE */
}

void KdenliveSettingsDialog::slotUpdateShuttleDevice(int ix)
{
#ifdef USE_JOGSHUTTLE
    QString device = m_configShuttle.shuttledevicelist->itemData(ix).toString();
    // KdenliveSettings::setShuttledevice(device);
    setupJogshuttleBtns(device);
    m_configShuttle.kcfg_shuttledevice->setText(device);
#else
    Q_UNUSED(ix)
#endif /* USE_JOGSHUTTLE */
}

void KdenliveSettingsDialog::updateWidgets()
{
// Revert widgets to last saved state (for example when user pressed "Cancel")
// //qCDebug(KDENLIVE_LOG) << "// // // KCONFIG Revert called";
#ifdef USE_JOGSHUTTLE
    // revert jog shuttle device
    if (m_configShuttle.shuttledevicelist->count() > 0) {
        for (int i = 0; i < m_configShuttle.shuttledevicelist->count(); ++i) {
            if (m_configShuttle.shuttledevicelist->itemData(i) == KdenliveSettings::shuttledevice()) {
                m_configShuttle.shuttledevicelist->setCurrentIndex(i);
                break;
            }
        }
    }

    // Revert jog shuttle buttons
    QList<QString> action_names = m_mappable_actions.keys();
    std::sort(action_names.begin(), action_names.end());
    QStringList actions_map = JogShuttleConfig::actionMap(KdenliveSettings::shuttlebuttons());
    QMap<QString, int> action_pos;
    for (const QString &action_id : qAsConst(actions_map)) {
        // This loop find out at what index is the string that would map to the action_id.
        for (int i = 0; i < action_names.size(); ++i) {
            if (m_mappable_actions[action_names[i]] == action_id) {
                action_pos[action_id] = i;
                break;
            }
        }
    }
    int i = 0;
    for (QComboBox *button : qAsConst(m_shuttle_buttons)) {
        ++i;
        if (i < actions_map.size()) {
            button->setCurrentIndex(action_pos[actions_map[i]]);
        }
    }
#endif /* USE_JOGSHUTTLE */
}

void KdenliveSettingsDialog::accept()
{
    if (m_pw->selectedProfile().isEmpty()) {
        KMessageBox::error(this, i18n("Please select a video profile"));
        return;
    }
    KConfigDialog::accept();
}

void KdenliveSettingsDialog::updateSettings()
{
    // Save changes to settings (for example when user pressed "Apply" or "Ok")
    // //qCDebug(KDENLIVE_LOG) << "// // // KCONFIG UPDATE called";
    if (m_pw->selectedProfile().isEmpty()) {
        KMessageBox::error(this, i18n("Please select a video profile"));
        return;
    }
    KdenliveSettings::setDefault_profile(m_pw->selectedProfile());

    if (m_configEnv.ffmpegurl->text().isEmpty()) {
        QString infos;
        QString warnings;
        Wizard::slotCheckPrograms(infos, warnings);
        m_configEnv.ffmpegurl->setText(KdenliveSettings::ffmpegpath());
        m_configEnv.ffplayurl->setText(KdenliveSettings::ffplaypath());
        m_configEnv.ffprobeurl->setText(KdenliveSettings::ffprobepath());
    }
    
    if (m_configTimeline.kcfg_trackheight->value() == 0) {
        QFont ft = QFontDatabase::systemFont(QFontDatabase::SmallestReadableFont);
        // Default unit for timeline.qml objects size
        int baseUnit = qMax(28, (int) (QFontInfo(ft).pixelSize() * 1.8 + 0.5));
        int trackHeight = qMax(50, (int) (2.2 * baseUnit + 6));
        m_configTimeline.kcfg_trackheight->setValue(trackHeight);
    } else if (m_configTimeline.kcfg_trackheight->value() != KdenliveSettings::trackheight()) {
        QFont ft = QFontDatabase::systemFont(QFontDatabase::SmallestReadableFont);
        // Default unit for timeline.qml objects size
        int baseUnit = qMax(28, (int) (QFontInfo(ft).pixelSize() * 1.8 + 0.5));
        if (m_configTimeline.kcfg_trackheight->value() < baseUnit) {
            m_configTimeline.kcfg_trackheight->setValue(baseUnit);
        }
    }
    
    bool resetConsumer = false;
    bool fullReset = false;
    bool updateCapturePath = false;
    bool updateLibrary = false;

    /*if (m_configShuttle.shuttledevicelist->count() > 0) {
    QString device = m_configShuttle.shuttledevicelist->itemData(m_configShuttle.shuttledevicelist->currentIndex()).toString();
    if (device != KdenliveSettings::shuttledevice()) KdenliveSettings::setShuttledevice(device);
    }*/

    // Capture default folder
    if (m_configEnv.kcfg_capturetoprojectfolder->isChecked() != KdenliveSettings::capturetoprojectfolder()) {
        KdenliveSettings::setCapturetoprojectfolder(m_configEnv.kcfg_capturetoprojectfolder->isChecked());
        updateCapturePath = true;
    }

    if (m_configProject.projecturl->url().toLocalFile() != KdenliveSettings::defaultprojectfolder()) {
        KdenliveSettings::setDefaultprojectfolder(m_configProject.projecturl->url().toLocalFile());
    }

    if (m_configEnv.capturefolderurl->url().toLocalFile() != KdenliveSettings::capturefolder()) {
        KdenliveSettings::setCapturefolder(m_configEnv.capturefolderurl->url().toLocalFile());
        updateCapturePath = true;
    }

    // Library default folder
    if (m_configEnv.kcfg_librarytodefaultfolder->isChecked() != KdenliveSettings::librarytodefaultfolder()) {
        KdenliveSettings::setLibrarytodefaultfolder(m_configEnv.kcfg_librarytodefaultfolder->isChecked());
        updateLibrary = true;
    }

    if (m_configEnv.libraryfolderurl->url().toLocalFile() != KdenliveSettings::libraryfolder()) {
        KdenliveSettings::setLibraryfolder(m_configEnv.libraryfolderurl->url().toLocalFile());
        if (!KdenliveSettings::librarytodefaultfolder()) {
            updateLibrary = true;
        }
    }

    if (m_configCapture.kcfg_v4l_format->currentIndex() != (int)KdenliveSettings::v4l_format()) {
        saveCurrentV4lProfile();
        KdenliveSettings::setV4l_format(0);
    }

    // Check if screengrab is fullscreen
    if (m_configCapture.kcfg_grab_capture_type->currentIndex() != KdenliveSettings::grab_capture_type()) {
        KdenliveSettings::setGrab_capture_type(m_configCapture.kcfg_grab_capture_type->currentIndex());
        emit updateFullScreenGrab();
    }

    // Check encoding profiles
    // FFmpeg
    QString profilestr = m_configCapture.kcfg_v4l_profile->currentData().toString();
    if (!profilestr.isEmpty() && (profilestr.section(QLatin1Char(';'), 0, 0) != KdenliveSettings::v4l_parameters() ||
                                  profilestr.section(QLatin1Char(';'), 1, 1) != KdenliveSettings::v4l_extension())) {
        KdenliveSettings::setV4l_parameters(profilestr.section(QLatin1Char(';'), 0, 0));
        KdenliveSettings::setV4l_extension(profilestr.section(QLatin1Char(';'), 1, 1));
    }
    // screengrab
    profilestr = m_configCapture.kcfg_grab_profile->currentData().toString();
    if (!profilestr.isEmpty() && (profilestr.section(QLatin1Char(';'), 0, 0) != KdenliveSettings::grab_parameters() ||
                                  profilestr.section(QLatin1Char(';'), 1, 1) != KdenliveSettings::grab_extension())) {
        KdenliveSettings::setGrab_parameters(profilestr.section(QLatin1Char(';'), 0, 0));
        KdenliveSettings::setGrab_extension(profilestr.section(QLatin1Char(';'), 1, 1));
    }

    // decklink
    profilestr = m_configCapture.kcfg_decklink_profile->currentData().toString();
    if (!profilestr.isEmpty() && (profilestr.section(QLatin1Char(';'), 0, 0) != KdenliveSettings::decklink_parameters() ||
                                  profilestr.section(QLatin1Char(';'), 1, 1) != KdenliveSettings::decklink_extension())) {
        KdenliveSettings::setDecklink_parameters(profilestr.section(QLatin1Char(';'), 0, 0));
        KdenliveSettings::setDecklink_extension(profilestr.section(QLatin1Char(';'), 1, 1));
    }
    // proxies
    profilestr = m_configProxy.kcfg_proxy_profile->currentData().toString();
    if (!profilestr.isEmpty() && (profilestr.section(QLatin1Char(';'), 0, 0) != KdenliveSettings::proxyparams() ||
                                  profilestr.section(QLatin1Char(';'), 1, 1) != KdenliveSettings::proxyextension())) {
        KdenliveSettings::setProxyparams(profilestr.section(QLatin1Char(';'), 0, 0));
        KdenliveSettings::setProxyextension(profilestr.section(QLatin1Char(';'), 1, 1));
    }

    // external proxies
    profilestr = m_configProxy.kcfg_external_proxy_profile->currentData().toString();
    if (!profilestr.isEmpty() && (profilestr != KdenliveSettings::externalProxyProfile())) {
        KdenliveSettings::setExternalProxyProfile(profilestr);
    }

    // timeline preview
    profilestr = m_configProject.kcfg_preview_profile->currentData().toString();
    if (!profilestr.isEmpty() && (profilestr.section(QLatin1Char(';'), 0, 0) != KdenliveSettings::previewparams() ||
                                  profilestr.section(QLatin1Char(';'), 1, 1) != KdenliveSettings::previewextension())) {
        KdenliveSettings::setPreviewparams(profilestr.section(QLatin1Char(';'), 0, 0));
        KdenliveSettings::setPreviewextension(profilestr.section(QLatin1Char(';'), 1, 1));
    }

    if (updateCapturePath) {
        emit updateCaptureFolder();
    }
    if (updateLibrary) {
        emit updateLibraryFolder();
    }

    QString value = m_configCapture.kcfg_v4l_alsadevice->currentData().toString();
    if (value != KdenliveSettings::v4l_alsadevicename()) {
        KdenliveSettings::setV4l_alsadevicename(value);
    }

    if (m_configSdl.kcfg_external_display->isChecked() != KdenliveSettings::external_display()) {
        KdenliveSettings::setExternal_display(m_configSdl.kcfg_external_display->isChecked());
        resetConsumer = true;
        fullReset = true;
    } else if (KdenliveSettings::external_display() &&
               KdenliveSettings::blackmagic_output_device() != m_configSdl.kcfg_blackmagic_output_device->currentIndex()) {
        resetConsumer = true;
        fullReset = true;
    }

    value = m_configSdl.kcfg_audio_driver->currentData().toString();
    if (value != KdenliveSettings::audiodrivername()) {
        KdenliveSettings::setAudiodrivername(value);
        resetConsumer = true;
    }

    if (value == QLatin1String("alsa")) {
        // Audio device setting is only valid for alsa driver
        value = m_configSdl.kcfg_audio_device->currentData().toString();
        if (value != KdenliveSettings::audiodevicename()) {
            KdenliveSettings::setAudiodevicename(value);
            resetConsumer = true;
        }
    } else if (!KdenliveSettings::audiodevicename().isEmpty()) {
        KdenliveSettings::setAudiodevicename(QString());
        resetConsumer = true;
    }

    value = m_configSdl.kcfg_audio_backend->currentData().toString();
    if (value != KdenliveSettings::audiobackend()) {
        KdenliveSettings::setAudiobackend(value);
        resetConsumer = true;
        fullReset = true;
    }

    if (m_configSdl.kcfg_window_background->color() != KdenliveSettings::window_background()) {
        KdenliveSettings::setWindow_background(m_configSdl.kcfg_window_background->color());
        emit updateMonitorBg();
    }
    
    if (m_configColors.kcfg_thumbColor1->color() != KdenliveSettings::thumbColor1() || m_configColors.kcfg_thumbColor2->color() != KdenliveSettings::thumbColor2()) {
        KdenliveSettings::setThumbColor1(m_configColors.kcfg_thumbColor1->color());
        KdenliveSettings::setThumbColor2(m_configColors.kcfg_thumbColor2->color());
        emit pCore->window()->getMainTimeline()->controller()->colorsChanged();
        emit pCore->getMonitor(Kdenlive::ClipMonitor)->refreshAudioThumbs();
    }

    if (m_configSdl.kcfg_volume->value() != KdenliveSettings::volume()) {
        KdenliveSettings::setVolume(m_configSdl.kcfg_volume->value());
        resetConsumer = true;
    }

    if (m_configMisc.kcfg_tabposition->currentIndex() != KdenliveSettings::tabposition()) {
        KdenliveSettings::setTabposition(m_configMisc.kcfg_tabposition->currentIndex());
    }

    if (m_configTimeline.kcfg_displayallchannels->isChecked() != KdenliveSettings::displayallchannels()) {
        KdenliveSettings::setDisplayallchannels(m_configTimeline.kcfg_displayallchannels->isChecked());
        emit audioThumbFormatChanged();
        emit pCore->getMonitor(Kdenlive::ClipMonitor)->refreshAudioThumbs();
    }

    if (m_modified) {
        // The transcoding profiles were modified, save.
        m_modified = false;
        saveTranscodeProfiles();
    }

#ifdef USE_JOGSHUTTLE
    m_shuttleModified = false;

    QStringList actions;
    actions << QStringLiteral("monitor_pause"); // the Job rest position action.
    for (QComboBox *button : qAsConst(m_shuttle_buttons)) {
        actions << m_mappable_actions[button->currentText()];
    }
    QString maps = JogShuttleConfig::actionMap(actions);
    // fprintf(stderr, "Shuttle config: %s\n", JogShuttleConfig::actionMap(actions).toLatin1().constData());
    if (KdenliveSettings::shuttlebuttons() != maps) {
        KdenliveSettings::setShuttlebuttons(maps);
    }
#endif

    bool restart = false;
    if (m_configSdl.kcfg_gpu_accel->isChecked() != KdenliveSettings::gpu_accel()) {
        // GPU setting was changed, we need to restart Kdenlive or everything will be corrupted
        if (KMessageBox::warningContinueCancel(this, i18n("Kdenlive must be restarted to change this setting")) == KMessageBox::Continue) {
            restart = true;
        } else {
            m_configSdl.kcfg_gpu_accel->setChecked(KdenliveSettings::gpu_accel());
        }
    }

    if (m_configTimeline.kcfg_trackheight->value() != KdenliveSettings::trackheight()) {
        KdenliveSettings::setTrackheight(m_configTimeline.kcfg_trackheight->value());
        emit resetView();
    }

    if (m_configTimeline.kcfg_autoscroll->isChecked() != KdenliveSettings::autoscroll()) {
        KdenliveSettings::setAutoscroll(m_configTimeline.kcfg_autoscroll->isChecked());
        emit pCore->autoScrollChanged();
    }

    // Mimes
    if (m_configEnv.kcfg_addedExtensions->text() != KdenliveSettings::addedExtensions()) {
        // Update list
        KdenliveSettings::setAddedExtensions(m_configEnv.kcfg_addedExtensions->text());
        QStringList mimes = ClipCreationDialog::getExtensions();
        std::sort(mimes.begin(), mimes.end());
        m_configEnv.supportedmimes->setPlainText(mimes.join(QLatin1Char(' ')));
    }

    KConfigDialog::settingsChangedSlot();
    // KConfigDialog::updateSettings();
    if (resetConsumer) {
        emit doResetConsumer(fullReset);
    }
    if (restart) {
        emit restartKdenlive();
    }
    emit checkTabPosition();

    // remembering Config dialog size
    KSharedConfigPtr config = KSharedConfig::openConfig();
    KConfigGroup settingsGroup(config, "settings");
    settingsGroup.writeEntry("dialogSize", QVariant(size()));
}

void KdenliveSettingsDialog::slotCheckAlsaDriver()
{
    QString value = m_configSdl.kcfg_audio_driver->itemData(m_configSdl.kcfg_audio_driver->currentIndex()).toString();
    m_configSdl.kcfg_audio_device->setEnabled(value == QLatin1String("alsa"));
}

void KdenliveSettingsDialog::slotCheckAudioBackend()
{
    QString value = m_configSdl.kcfg_audio_backend->itemData(m_configSdl.kcfg_audio_backend->currentIndex()).toString();
    m_configSdl.group_sdl->setEnabled(value.startsWith(QLatin1String("sdl")));
}

void KdenliveSettingsDialog::loadTranscodeProfiles()
{
    KSharedConfigPtr config =
        KSharedConfig::openConfig(QStandardPaths::locate(QStandardPaths::AppDataLocation, QStringLiteral("kdenlivetranscodingrc")), KConfig::CascadeConfig);
    KConfigGroup transConfig(config, "Transcoding");
    // read the entries
    m_configTranscode.profiles_list->blockSignals(true);
    m_configTranscode.profiles_list->clear();
    QMap<QString, QString> profiles = transConfig.entryMap();
    QMapIterator<QString, QString> i(profiles);
    while (i.hasNext()) {
        i.next();
        auto *item = new QListWidgetItem(i.key());
        QString profilestr = i.value();
        if (profilestr.contains(QLatin1Char(';'))) {
            item->setToolTip(profilestr.section(QLatin1Char(';'), 1, 1));
        }
        item->setData(Qt::UserRole, profilestr);
        m_configTranscode.profiles_list->addItem(item);
        // item->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled | Qt::ItemIsEditable);
    }
    m_configTranscode.profiles_list->blockSignals(false);
    m_configTranscode.profiles_list->setCurrentRow(0);
}

void KdenliveSettingsDialog::saveTranscodeProfiles()
{
    QString transcodeFile = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + QStringLiteral("/kdenlivetranscodingrc");
    KSharedConfigPtr config = KSharedConfig::openConfig(transcodeFile);
    KConfigGroup transConfig(config, "Transcoding");
    // read the entries
    transConfig.deleteGroup();
    int max = m_configTranscode.profiles_list->count();
    for (int i = 0; i < max; ++i) {
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
    QString profilestr = m_configTranscode.profile_parameters->toPlainText();
    profilestr.append(" %1." + m_configTranscode.profile_extension->text());
    profilestr.append(';');
    if (!m_configTranscode.profile_description->text().isEmpty()) {
        profilestr.append(m_configTranscode.profile_description->text());
    }
    if (m_configTranscode.profile_audioonly->isChecked()) {
        profilestr.append(";audio");
    }
    item->setData(Qt::UserRole, profilestr);
    m_configTranscode.profiles_list->addItem(item);
    m_configTranscode.profiles_list->setCurrentItem(item);
    slotDialogModified();
}

void KdenliveSettingsDialog::slotUpdateTranscodingProfile()
{
    QListWidgetItem *item = m_configTranscode.profiles_list->currentItem();
    if (!item) {
        return;
    }
    m_configTranscode.button_update->setEnabled(false);
    item->setText(m_configTranscode.profile_name->text());
    QString profilestr = m_configTranscode.profile_parameters->toPlainText();
    profilestr.append(" %1." + m_configTranscode.profile_extension->text());
    profilestr.append(';');
    if (!m_configTranscode.profile_description->text().isEmpty()) {
        profilestr.append(m_configTranscode.profile_description->text());
    }
    if (m_configTranscode.profile_audioonly->isChecked()) {
        profilestr.append(QStringLiteral(";audio"));
    }
    item->setData(Qt::UserRole, profilestr);
    slotDialogModified();
}

void KdenliveSettingsDialog::slotDeleteTranscode()
{
    QListWidgetItem *item = m_configTranscode.profiles_list->currentItem();
    if (item == nullptr) {
        return;
    }
    delete item;
    slotDialogModified();
}

void KdenliveSettingsDialog::slotEnableTranscodeUpdate()
{
    if (!m_configTranscode.profile_box->isEnabled()) {
        return;
    }
    bool allow = true;
    if (m_configTranscode.profile_name->text().isEmpty() || m_configTranscode.profile_extension->text().isEmpty()) {
        allow = false;
    }
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
    QString profilestr = item->data(Qt::UserRole).toString();
    if (profilestr.contains(QLatin1Char(';'))) {
        m_configTranscode.profile_description->setText(profilestr.section(QLatin1Char(';'), 1, 1));
        if (profilestr.section(QLatin1Char(';'), 2, 2) == QLatin1String("audio")) {
            m_configTranscode.profile_audioonly->setChecked(true);
        }
        profilestr = profilestr.section(QLatin1Char(';'), 0, 0).simplified();
    }
    m_configTranscode.profile_extension->setText(profilestr.section(QLatin1Char('.'), -1));
    m_configTranscode.profile_parameters->setPlainText(profilestr.section(QLatin1Char(' '), 0, -2));
    m_configTranscode.profile_box->setEnabled(true);
}

void KdenliveSettingsDialog::slotShuttleModified()
{
#ifdef USE_JOGSHUTTLE
    QStringList actions;
    actions << QStringLiteral("monitor_pause"); // the Job rest position action.
    for (QComboBox *button : qAsConst(m_shuttle_buttons)) {
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

// virtual
bool KdenliveSettingsDialog::hasChanged()
{
    if (m_modified || m_shuttleModified) {
        return true;
    }
    return KConfigDialog::hasChanged();
}

void KdenliveSettingsDialog::slotUpdatev4lDevice()
{
    QString device = m_configCapture.kcfg_detectedv4ldevices->itemData(m_configCapture.kcfg_detectedv4ldevices->currentIndex()).toString();
    if (!device.isEmpty()) {
        m_configCapture.kcfg_video4vdevice->setText(device);
    }
    QString info = m_configCapture.kcfg_detectedv4ldevices->itemData(m_configCapture.kcfg_detectedv4ldevices->currentIndex(), Qt::UserRole + 1).toString();

    m_configCapture.kcfg_v4l_format->blockSignals(true);
    m_configCapture.kcfg_v4l_format->clear();

    QString vl4ProfilePath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + QStringLiteral("/profiles/video4linux");
    if (QFile::exists(vl4ProfilePath)) {
        m_configCapture.kcfg_v4l_format->addItem(i18n("Current settings"));
    }

#if QT_VERSION < QT_VERSION_CHECK(5, 15, 0)
    QStringList pixelformats = info.split('>', QString::SkipEmptyParts);
#else
    QStringList pixelformats = info.split('>', Qt::SkipEmptyParts);
#endif
    QString itemSize;
    QString pixelFormat;
    QStringList itemRates;
    for (int i = 0; i < pixelformats.count(); ++i) {
        QString format = pixelformats.at(i).section(QLatin1Char(':'), 0, 0);
#if QT_VERSION < QT_VERSION_CHECK(5, 15, 0)
        QStringList sizes = pixelformats.at(i).split(':', QString::SkipEmptyParts);
#else
        QStringList sizes = pixelformats.at(i).split(':', Qt::SkipEmptyParts);
#endif
        pixelFormat = sizes.takeFirst();
        for (int j = 0; j < sizes.count(); ++j) {
            itemSize = sizes.at(j).section(QLatin1Char('='), 0, 0);
#if QT_VERSION < QT_VERSION_CHECK(5, 15, 0)
            itemRates = sizes.at(j).section(QLatin1Char('='), 1, 1).split(QLatin1Char(','), QString::SkipEmptyParts);
#else
            itemRates = sizes.at(j).section(QLatin1Char('='), 1, 1).split(QLatin1Char(','), Qt::SkipEmptyParts);
#endif
            for (int k = 0; k < itemRates.count(); ++k) {
                m_configCapture.kcfg_v4l_format->addItem(
                    QLatin1Char('[') + format + QStringLiteral("] ") + itemSize + QStringLiteral(" (") + itemRates.at(k) + QLatin1Char(')'),
                    QStringList() << format << itemSize.section('x', 0, 0) << itemSize.section('x', 1, 1) << itemRates.at(k).section(QLatin1Char('/'), 0, 0)
                                  << itemRates.at(k).section(QLatin1Char('/'), 1, 1));
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
    m_configCapture.p_size->setText(info.at(1) + QLatin1Char('x') + info.at(2));
    m_configCapture.p_fps->setText(info.at(3) + QLatin1Char('/') + info.at(4));
    m_configCapture.p_aspect->setText(QStringLiteral("1/1"));
    m_configCapture.p_display->setText(info.at(1) + QLatin1Char('/') + info.at(2));
    m_configCapture.p_colorspace->setText(ProfileRepository::getColorspaceDescription(601));
    m_configCapture.p_progressive->setText(i18n("Progressive"));

    QDir dir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + QStringLiteral("/profiles/"));
    if (!dir.exists() || !dir.exists(QStringLiteral("video4linux"))) {
        saveCurrentV4lProfile();
    }
}

void KdenliveSettingsDialog::loadCurrentV4lProfileInfo()
{
    QDir dir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + QStringLiteral("/profiles/"));
    if (!dir.exists()) {
        dir.mkpath(QStringLiteral("."));
    }
    if (!ProfileRepository::get()->profileExists(dir.absoluteFilePath(QStringLiteral("video4linux")))) {
        // No default formats found, build one
        std::unique_ptr<ProfileParam> prof(new ProfileParam(pCore->getCurrentProfile().get()));
        prof->m_width = 320;
        prof->m_height = 200;
        prof->m_frame_rate_num = 15;
        prof->m_frame_rate_den = 1;
        prof->m_display_aspect_num = 4;
        prof->m_display_aspect_den = 3;
        prof->m_sample_aspect_num = 1;
        prof->m_sample_aspect_den = 1;
        prof->m_progressive = true;
        prof->m_colorspace = 601;
        ProfileRepository::get()->saveProfile(prof.get(), dir.absoluteFilePath(QStringLiteral("video4linux")));
    }
    auto &prof = ProfileRepository::get()->getProfile(dir.absoluteFilePath(QStringLiteral("video4linux")));
    m_configCapture.p_size->setText(QString::number(prof->width()) + QLatin1Char('x') + QString::number(prof->height()));
    m_configCapture.p_fps->setText(QString::number(prof->frame_rate_num()) + QLatin1Char('/') + QString::number(prof->frame_rate_den()));
    m_configCapture.p_aspect->setText(QString::number(prof->sample_aspect_num()) + QLatin1Char('/') + QString::number(prof->sample_aspect_den()));
    m_configCapture.p_display->setText(QString::number(prof->display_aspect_num()) + QLatin1Char('/') + QString::number(prof->display_aspect_den()));
    m_configCapture.p_colorspace->setText(ProfileRepository::getColorspaceDescription(prof->colorspace()));
    if (prof->progressive()) {
        m_configCapture.p_progressive->setText(i18n("Progressive"));
    }
}

void KdenliveSettingsDialog::saveCurrentV4lProfile()
{
    std::unique_ptr<ProfileParam> profile(new ProfileParam(pCore->getCurrentProfile().get()));
    profile->m_description = QStringLiteral("Video4Linux capture");
    profile->m_colorspace = ProfileRepository::getColorspaceFromDescription(m_configCapture.p_colorspace->text());
    profile->m_width = m_configCapture.p_size->text().section('x', 0, 0).toInt();
    profile->m_height = m_configCapture.p_size->text().section('x', 1, 1).toInt();
    profile->m_sample_aspect_num = m_configCapture.p_aspect->text().section(QLatin1Char('/'), 0, 0).toInt();
    profile->m_sample_aspect_den = m_configCapture.p_aspect->text().section(QLatin1Char('/'), 1, 1).toInt();
    profile->m_display_aspect_num = m_configCapture.p_display->text().section(QLatin1Char('/'), 0, 0).toInt();
    profile->m_display_aspect_den = m_configCapture.p_display->text().section(QLatin1Char('/'), 1, 1).toInt();
    profile->m_frame_rate_num = m_configCapture.p_fps->text().section(QLatin1Char('/'), 0, 0).toInt();
    profile->m_frame_rate_den = m_configCapture.p_fps->text().section(QLatin1Char('/'), 1, 1).toInt();
    profile->m_progressive = m_configCapture.p_progressive->text() == i18n("Progressive");
    QDir dir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + QStringLiteral("/profiles/"));
    if (!dir.exists()) {
        dir.mkpath(QStringLiteral("."));
    }
    ProfileRepository::get()->saveProfile(profile.get(), dir.absoluteFilePath(QStringLiteral("video4linux")));
}

void KdenliveSettingsDialog::slotManageEncodingProfile()
{
    auto *act = qobject_cast<QAction *>(sender());
    int type = 0;
    if (act) {
        type = act->data().toInt();
    }
    QPointer<EncodingProfilesDialog> dia = new EncodingProfilesDialog(type);
    dia->exec();
    delete dia;
    loadEncodingProfiles();
}

void KdenliveSettingsDialog::loadExternalProxyProfiles()
{
    // load proxy profiles
    KConfig conf(QStringLiteral("externalproxies.rc"), KConfig::CascadeConfig, QStandardPaths::AppDataLocation);
    KConfigGroup group(&conf, "proxy");
    QMap<QString, QString> values = group.entryMap();
    QMapIterator<QString, QString> k(values);
    QString currentItem = KdenliveSettings::externalProxyProfile();
    m_configProxy.kcfg_external_proxy_profile->blockSignals(true);
    m_configProxy.kcfg_external_proxy_profile->clear();
    while (k.hasNext()) {
        k.next();
        if (!k.key().isEmpty()) {
            if (k.value().contains(QLatin1Char(';'))) {
                m_configProxy.kcfg_external_proxy_profile->addItem(k.key(), k.value());
            }
        }
    }
    if (!currentItem.isEmpty()) {
        m_configProxy.kcfg_external_proxy_profile->setCurrentIndex(m_configProxy.kcfg_external_proxy_profile->findText(currentItem));
    }
    m_configProxy.kcfg_external_proxy_profile->blockSignals(false);
}

void KdenliveSettingsDialog::loadEncodingProfiles()
{
    KConfig conf(QStringLiteral("encodingprofiles.rc"), KConfig::CascadeConfig, QStandardPaths::AppDataLocation);

    // Load v4l profiles
    m_configCapture.kcfg_v4l_profile->blockSignals(true);
    QString currentItem = m_configCapture.kcfg_v4l_profile->currentText();
    m_configCapture.kcfg_v4l_profile->clear();
    KConfigGroup group(&conf, "video4linux");
    QMap<QString, QString> values = group.entryMap();
    QMapIterator<QString, QString> i(values);
    while (i.hasNext()) {
        i.next();
        if (!i.key().isEmpty()) {
            m_configCapture.kcfg_v4l_profile->addItem(i.key(), i.value());
        }
    }
    m_configCapture.kcfg_v4l_profile->blockSignals(false);
    if (!currentItem.isEmpty()) {
        m_configCapture.kcfg_v4l_profile->setCurrentIndex(m_configCapture.kcfg_v4l_profile->findText(currentItem));
    }

    // Load Screen Grab profiles
    m_configCapture.kcfg_grab_profile->blockSignals(true);
    currentItem = m_configCapture.kcfg_grab_profile->currentText();
    m_configCapture.kcfg_grab_profile->clear();
    KConfigGroup group2(&conf, "screengrab");
    values = group2.entryMap();
    QMapIterator<QString, QString> j(values);
    while (j.hasNext()) {
        j.next();
        if (!j.key().isEmpty()) {
            m_configCapture.kcfg_grab_profile->addItem(j.key(), j.value());
        }
    }
    m_configCapture.kcfg_grab_profile->blockSignals(false);
    if (!currentItem.isEmpty()) {
        m_configCapture.kcfg_grab_profile->setCurrentIndex(m_configCapture.kcfg_grab_profile->findText(currentItem));
    }

    // Load Decklink profiles
    m_configCapture.kcfg_decklink_profile->blockSignals(true);
    currentItem = m_configCapture.kcfg_decklink_profile->currentText();
    m_configCapture.kcfg_decklink_profile->clear();
    KConfigGroup group3(&conf, "decklink");
    values = group3.entryMap();
    QMapIterator<QString, QString> k(values);
    while (k.hasNext()) {
        k.next();
        if (!k.key().isEmpty()) {
            m_configCapture.kcfg_decklink_profile->addItem(k.key(), k.value());
        }
    }
    m_configCapture.kcfg_decklink_profile->blockSignals(false);
    if (!currentItem.isEmpty()) {
        m_configCapture.kcfg_decklink_profile->setCurrentIndex(m_configCapture.kcfg_decklink_profile->findText(currentItem));
    }

    // Load Timeline Preview profiles
    m_configProject.kcfg_preview_profile->blockSignals(true);
    currentItem = m_configProject.kcfg_preview_profile->currentText();
    m_configProject.kcfg_preview_profile->clear();
    KConfigGroup group5(&conf, "timelinepreview");
    values = group5.entryMap();
    m_configProject.kcfg_preview_profile->addItem(i18n("Automatic"));
    QMapIterator<QString, QString> l(values);
    while (l.hasNext()) {
        l.next();
        if (!l.key().isEmpty()) {
            m_configProject.kcfg_preview_profile->addItem(l.key(), l.value());
        }
    }
    if (!currentItem.isEmpty()) {
        m_configProject.kcfg_preview_profile->setCurrentIndex(m_configProject.kcfg_preview_profile->findText(currentItem));
    }
    m_configProject.kcfg_preview_profile->blockSignals(false);
    QString profilestr = m_configProject.kcfg_preview_profile->itemData(m_configProject.kcfg_preview_profile->currentIndex()).toString();
    if (profilestr.isEmpty()) {
        m_configProject.previewparams->clear();
    } else {
        m_configProject.previewparams->setPlainText(profilestr.section(QLatin1Char(';'), 0, 0));
    }

    // Load Proxy profiles
    m_configProxy.kcfg_proxy_profile->blockSignals(true);
    currentItem = m_configProxy.kcfg_proxy_profile->currentText();
    m_configProxy.kcfg_proxy_profile->clear();
    KConfigGroup group4(&conf, "proxy");
    values = group4.entryMap();
    m_configProxy.kcfg_proxy_profile->addItem(i18n("Automatic"));
    QMapIterator<QString, QString> m(values);
    while (m.hasNext()) {
        m.next();
        if (!m.key().isEmpty()) {
            m_configProxy.kcfg_proxy_profile->addItem(m.key(), m.value());
        }
    }
    if (!currentItem.isEmpty()) {
        m_configProxy.kcfg_proxy_profile->setCurrentIndex(m_configProxy.kcfg_proxy_profile->findText(currentItem));
    }
    m_configProxy.kcfg_proxy_profile->blockSignals(false);
    profilestr = m_configProxy.kcfg_proxy_profile->itemData(m_configProxy.kcfg_proxy_profile->currentIndex()).toString();
    if (profilestr.isEmpty()) {
        m_configProxy.proxyparams->clear();
    } else {
        m_configProxy.proxyparams->setPlainText(profilestr.section(QLatin1Char(';'), 0, 0));
    }
}

void KdenliveSettingsDialog::slotUpdateDecklinkProfile(int ix)
{
    if (ix == -1) {
        ix = KdenliveSettings::decklink_profile();
    } else {
        ix = m_configCapture.kcfg_decklink_profile->currentIndex();
    }
    QString profilestr = m_configCapture.kcfg_decklink_profile->itemData(ix).toString();
    if (profilestr.isEmpty()) {
        return;
    }
    m_configCapture.decklink_parameters->setPlainText(profilestr.section(QLatin1Char(';'), 0, 0));
    //
}

void KdenliveSettingsDialog::slotUpdateV4lProfile(int ix)
{
    if (ix == -1) {
        ix = KdenliveSettings::v4l_profile();
    } else {
        ix = m_configCapture.kcfg_v4l_profile->currentIndex();
    }
    QString profilestr = m_configCapture.kcfg_v4l_profile->itemData(ix).toString();
    if (profilestr.isEmpty()) {
        return;
    }
    m_configCapture.v4l_parameters->setPlainText(profilestr.section(QLatin1Char(';'), 0, 0));
    //
}

void KdenliveSettingsDialog::slotUpdateGrabProfile(int ix)
{
    if (ix == -1) {
        ix = KdenliveSettings::grab_profile();
    } else {
        ix = m_configCapture.kcfg_grab_profile->currentIndex();
    }
    QString profilestr = m_configCapture.kcfg_grab_profile->itemData(ix).toString();
    if (profilestr.isEmpty()) {
        return;
    }
    m_configCapture.grab_parameters->setPlainText(profilestr.section(QLatin1Char(';'), 0, 0));
    //
}

void KdenliveSettingsDialog::slotUpdateProxyProfile(int ix)
{
    if (ix == -1) {
        ix = KdenliveSettings::proxy_profile();
    } else {
        ix = m_configProxy.kcfg_proxy_profile->currentIndex();
    }
    QString profilestr = m_configProxy.kcfg_proxy_profile->itemData(ix).toString();
    if (profilestr.isEmpty()) {
        return;
    }
    m_configProxy.proxyparams->setPlainText(profilestr.section(QLatin1Char(';'), 0, 0));
}

void KdenliveSettingsDialog::slotUpdatePreviewProfile(int ix)
{
    if (ix == -1) {
        ix = KdenliveSettings::preview_profile();
    } else {
        ix = m_configProject.kcfg_preview_profile->currentIndex();
    }
    QString profilestr = m_configProject.kcfg_preview_profile->itemData(ix).toString();
    if (profilestr.isEmpty()) {
        return;
    }
    m_configProject.previewparams->setPlainText(profilestr.section(QLatin1Char(';'), 0, 0));
}

void KdenliveSettingsDialog::slotEditVideo4LinuxProfile()
{
    QString vl4ProfilePath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + QStringLiteral("/profiles/video4linux");
    QPointer<ProfilesDialog> w = new ProfilesDialog(vl4ProfilePath, true);
    if (w->exec() == QDialog::Accepted) {
        // save and update profile
        loadCurrentV4lProfileInfo();
    }
    delete w;
}

void KdenliveSettingsDialog::slotReloadBlackMagic()
{
    getBlackMagicDeviceList(m_configCapture.kcfg_decklink_capturedevice, true);
    if (!getBlackMagicOutputDeviceList(m_configSdl.kcfg_blackmagic_output_device, true)) {
        // No blackmagic card found
        m_configSdl.kcfg_external_display->setEnabled(false);
    }
    m_configSdl.kcfg_external_display->setEnabled(KdenliveSettings::decklink_device_found());
}

void KdenliveSettingsDialog::checkProfile()
{
    m_pw->loadProfile(KdenliveSettings::default_profile().isEmpty() ? pCore->getCurrentProfile()->path() : KdenliveSettings::default_profile());
}

void KdenliveSettingsDialog::slotReloadShuttleDevices()
{
#ifdef USE_JOGSHUTTLE
    QString devDirStr = QStringLiteral("/dev/input/by-id");
    QDir devDir(devDirStr);
    if (!devDir.exists()) {
        devDirStr = QStringLiteral("/dev/input");
    }

    QStringList devNamesList;
    QStringList devPathList;
    m_configShuttle.shuttledevicelist->clear();

    DeviceMap devMap = JogShuttle::enumerateDevices(devDirStr);
    DeviceMapIter iter = devMap.begin();
    while (iter != devMap.end()) {
        m_configShuttle.shuttledevicelist->addItem(iter.key(), iter.value());
        devNamesList << iter.key();
        devPathList << iter.value();
        ++iter;
    }
    KdenliveSettings::setShuttledevicenames(devNamesList);
    KdenliveSettings::setShuttledevicepaths(devPathList);
    QTimer::singleShot(200, this, SLOT(slotUpdateShuttleDevice()));
#endif // USE_JOGSHUTTLE
}

void KdenliveSettingsDialog::slotUpdateAudioCaptureChannels(int index)
{
    KdenliveSettings::setAudiocapturechannels(m_configCapture.audiocapturechannels->itemData(index).toInt());
}

void KdenliveSettingsDialog::slotUpdateAudioCaptureSampleRate(int index)
{
    KdenliveSettings::setAudiocapturesamplerate(m_configCapture.audiocapturesamplerate->itemData(index).toInt());
}
