/*
    SPDX-FileCopyrightText: 2021 Jean-Baptiste Mardelle <jb@kdenlive.org>

SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "kdenlivesettingsdialog.h"
#include "clipcreationdialog.h"
#include "core.h"
#include "dialogs/customcamcorderdialog.h"
#include "dialogs/profilesdialog.h"
#include "doc/kdenlivedoc.h"
#include "kdenlivesettings.h"
#include "mainwindow.h"
#include "monitor/monitor.h"
#include "monitor/monitorproxy.h"
#include "profiles/profilemodel.hpp"
#include "profiles/profilerepository.hpp"
#include "profilesdialog.h"
#include "project/dialogs/guidecategories.h"
#include "project/dialogs/profilewidget.h"
#include "timeline2/view/timelinecontroller.h"
#include "timeline2/view/timelinewidget.h"
#include "wizard.h"

#ifdef USE_V4L
#include "capture/v4lcapture.h"
#endif

#include "KLocalizedString"
#include "kdenlive_debug.h"
#include <KArchive>
#include <KArchiveDirectory>
#include <KIO/DesktopExecParser>
#include <KIO/FileCopyJob>
#include <KIO/OpenFileManagerWindowJob>
#include <kio/directorysizejob.h>
#include <kio_version.h>
#if KIO_VERSION >= QT_VERSION_CHECK(5, 98, 0)
#include <KIO/JobUiDelegateFactory>
#else
#include <KIO/JobUiDelegate>
#endif
#include "utils/KMessageBox_KdenliveCompat.h"
#include <KIO/OpenUrlJob>
#include <KLineEdit>
#include <KMessageBox>
#include <KOpenWithDialog>
#include <KService>
#include <KTar>
#include <KUrlRequesterDialog>
#include <KZip>

#include <QAction>
#include <QButtonGroup>
#include <QDir>
#include <QGuiApplication>
#include <QInputDialog>
#include <QRegularExpression>
#include <QScreen>
#include <QSize>
#include <QThread>
#include <QTimer>
#include <QtConcurrent>

#include <cstdio>
#include <cstdlib>
#include <fcntl.h>
#include <unistd.h>

#ifdef USE_JOGSHUTTLE
#include "jogshuttle/jogaction.h"
#include "jogshuttle/jogshuttleconfig.h"
#include <QStandardPaths>
#include <linux/input.h>
#include <memory>
#endif

SpeechList::SpeechList(QWidget *parent)
    : QListWidget(parent)
{
    setAlternatingRowColors(true);
    setAcceptDrops(true);
    setDropIndicatorShown(true);
    viewport()->setAcceptDrops(true);
}

QStringList SpeechList::mimeTypes() const
{
    return QStringList() << QStringLiteral("text/uri-list");
}

void SpeechList::dropEvent(QDropEvent *event)
{
    const QMimeData *qMimeData = event->mimeData();
    if (qMimeData->hasUrls()) {
        QList<QUrl> urls = qMimeData->urls();
        if (!urls.isEmpty()) {
            Q_EMIT getDictionary(urls.takeFirst());
        }
    }
}

KdenliveSettingsDialog::KdenliveSettingsDialog(QMap<QString, QString> mappable_actions, bool gpuAllowed, QWidget *parent)
    : KConfigDialog(parent, QStringLiteral("settings"), KdenliveSettings::self())
    , m_modified(false)
    , m_shuttleModified(false)
    , m_voskUpdated(false)
    , m_mappable_actions(std::move(mappable_actions))
{
    KdenliveSettings::setV4l_format(0);

    initMiscPage();
    initProjectPage();
    initProxyPage();

    QWidget *p3 = new QWidget;
    m_configTimeline.setupUi(p3);
    m_pageTimeline = addPage(p3, i18n("Timeline"), QStringLiteral("video-display"));

    QWidget *p4 = new QWidget;
    m_configTools.setupUi(p4);
    m_pageTools = addPage(p4, i18n("Tools"), QStringLiteral("tools"));

    initEnviromentPage();

    QWidget *p11 = new QWidget;
    m_configColors.setupUi(p11);
    m_pageColors = addPage(p11, i18n("Colors and Guides"), QStringLiteral("color-management"));
    m_guidesCategories = new GuideCategories(nullptr, this);
    QVBoxLayout *guidesLayout = new QVBoxLayout(m_configColors.guides_box);
    guidesLayout->addWidget(m_guidesCategories);

    QWidget *p12 = new QWidget;
    m_configSpeech.setupUi(p12);
    m_pageSpeech = addPage(p12, i18n("Speech To Text"), QStringLiteral("text-speak"));

    QWidget *p7 = new QWidget;
    m_configSdl.setupUi(p7);
    m_pagePlay = addPage(p7, i18n("Playback"), QStringLiteral("media-playback-start"));

    QWidget *p5 = new QWidget;
    m_configCapture.setupUi(p5);
    m_decklinkProfiles = new EncodingProfilesChooser(this, EncodingProfilesManager::DecklinkCapture, false, QStringLiteral("decklink_profile"));
    m_configCapture.decklink_profile_box->addWidget(m_decklinkProfiles);
    m_v4lProfiles = new EncodingProfilesChooser(this, EncodingProfilesManager::V4LCapture, false, QStringLiteral("v4l_profile"));
    m_configCapture.v4l_profile_box->addWidget(m_v4lProfiles);
    m_grabProfiles = new EncodingProfilesChooser(this, EncodingProfilesManager::ScreenCapture, false, QStringLiteral("grab_profile"));
    m_configCapture.screen_grab_profile_box->addWidget(m_grabProfiles);
    m_pageCapture = addPage(p5, i18n("Capture"), QStringLiteral("media-record"));

    initDevices();
    initCapturePage();
    initJogShuttlePage();
    initTranscodePage();

    initSdlPage(gpuAllowed);
    initSpeechPage();

    // Config dialog size
    KSharedConfigPtr config = KSharedConfig::openConfig();
    KConfigGroup settingsGroup(config, "settings");
    QSize optimalSize;

    if (!settingsGroup.exists() || !settingsGroup.hasKey("dialogSize")) {
        const QSize screenSize = (QGuiApplication::primaryScreen()->availableSize() * 0.9);
        const QSize targetSize = QSize(1024, 708);
        optimalSize = targetSize.boundedTo(screenSize);
    } else {
        optimalSize = settingsGroup.readEntry("dialogSize", QVariant(size())).toSize();
    }
    resize(optimalSize);
    // Use timers so that the Settings window opens before starting the scripts
    checkSpeechDependencies();
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

    // show a hint to the user to know what to check for in case the device list are empty (common issue)
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

void KdenliveSettingsDialog::initMiscPage()
{
    QWidget *p1 = new QWidget;
    m_configMisc.setupUi(p1);
    m_pageMisc = addPage(p1, i18n("Misc"), QStringLiteral("configure"));

    m_configMisc.kcfg_use_exiftool->setEnabled(!QStandardPaths::findExecutable(QStringLiteral("exiftool")).isEmpty());

    static const QRegularExpression reg(R"((\+|-)?\d{2}:\d{2}:\d{2}(:||,)\d{2})");
    QValidator *validator = new QRegularExpressionValidator(reg, this);
    m_configMisc.kcfg_color_duration->setInputMask(pCore->timecode().mask());
    m_configMisc.kcfg_color_duration->setValidator(validator);
    m_configMisc.kcfg_title_duration->setInputMask(pCore->timecode().mask());
    m_configMisc.kcfg_title_duration->setValidator(validator);
    m_configMisc.kcfg_transition_duration->setInputMask(pCore->timecode().mask());
    m_configMisc.kcfg_transition_duration->setValidator(validator);
    m_configMisc.kcfg_mix_duration->setInputMask(pCore->timecode().mask());
    m_configMisc.kcfg_mix_duration->setValidator(validator);
    m_configMisc.kcfg_image_duration->setInputMask(pCore->timecode().mask());
    m_configMisc.kcfg_image_duration->setValidator(validator);
    m_configMisc.kcfg_sequence_duration->setInputMask(pCore->timecode().mask());
    m_configMisc.kcfg_sequence_duration->setValidator(validator);
    m_configMisc.kcfg_fade_duration->setInputMask(pCore->timecode().mask());
    m_configMisc.kcfg_fade_duration->setValidator(validator);
    m_configMisc.kcfg_subtitle_duration->setInputMask(pCore->timecode().mask());
    m_configMisc.kcfg_subtitle_duration->setValidator(validator);

    m_configMisc.preferredcomposite->clear();
    m_configMisc.preferredcomposite->addItem(i18n("auto"));
    m_configMisc.preferredcomposite->addItems(KdenliveSettings::compositingList());

    if (!KdenliveSettings::preferredcomposite().isEmpty()) {
        int ix = m_configMisc.preferredcomposite->findText(KdenliveSettings::preferredcomposite());
        if (ix > -1) {
            m_configMisc.preferredcomposite->setCurrentIndex(ix);
        }
    }
    connect(m_configMisc.preferredcomposite, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this, [&]() {
        if (m_configMisc.preferredcomposite->currentText() != KdenliveSettings::preferredcomposite()) {
            KdenliveSettings::setPreferredcomposite(m_configMisc.preferredcomposite->currentText());
            int mode = pCore->currentDoc()->getDocumentProperty(QStringLiteral("compositing")).toInt();
            pCore->window()->getCurrentTimeline()->controller()->switchCompositing(mode);
            pCore->currentDoc()->setModified();
        }
    });
}

void KdenliveSettingsDialog::initProjectPage()
{
    QWidget *p9 = new QWidget;
    m_configProject.setupUi(p9);
    // Timeline preview
    QString currentPreviewData =
        KdenliveSettings::previewparams().isEmpty() ? QString() : QString("%1;%2").arg(KdenliveSettings::previewparams(), KdenliveSettings::previewextension());
    m_tlPreviewProfiles = new EncodingTimelinePreviewProfilesChooser(p9, true, currentPreviewData, false);
    m_configProject.preview_profile_box->addWidget(m_tlPreviewProfiles);
    auto *vbox = new QVBoxLayout;
    m_pw = new ProfileWidget(this);
    vbox->addWidget(m_pw);
    connect(m_pw, &ProfileWidget::profileChanged, this, [this]() { m_tlPreviewProfiles->filterPreviewProfiles(m_pw->selectedProfile()); });
    m_configProject.profile_box->setLayout(vbox);
    m_configProject.profile_box->setTitle(i18n("Select the Default Profile (preset)"));
    // Select profile
    m_pw->loadProfile(KdenliveSettings::default_profile().isEmpty() ? pCore->getCurrentProfile()->path() : KdenliveSettings::default_profile());
    m_tlPreviewProfiles->filterPreviewProfiles(m_pw->selectedProfile());
    connect(m_tlPreviewProfiles, &EncodingTimelinePreviewProfilesChooser::currentIndexChanged, this, &KdenliveSettingsDialog::slotDialogModified);
    connect(m_pw, &ProfileWidget::profileChanged, this, &KdenliveSettingsDialog::slotDialogModified);
    m_configProject.projecturl->setMode(KFile::Directory);
    m_configProject.projecturl->setUrl(QUrl::fromLocalFile(KdenliveSettings::defaultprojectfolder()));
    connect(m_configProject.projecturl, &KUrlRequester::textChanged, this, &KdenliveSettingsDialog::slotDialogModified);
    connect(m_configProject.kcfg_customprojectfolder, &QCheckBox::toggled, m_configProject.projecturl, &KUrlRequester::setEnabled);
    connect(m_configProject.kcfg_videotracks, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), this, [this]() {
        if (m_configProject.kcfg_videotracks->value() + m_configProject.kcfg_audiotracks->value() <= 0) {
            m_configProject.kcfg_videotracks->setValue(1);
        }
    });
    connect(m_configProject.kcfg_audiotracks, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), this, [this]() {
        if (m_configProject.kcfg_videotracks->value() + m_configProject.kcfg_audiotracks->value() <= 0) {
            m_configProject.kcfg_audiotracks->setValue(1);
        }
    });

    m_pageProject = addPage(p9, i18n("Project Defaults"), QStringLiteral("project-defaults"));
}

void KdenliveSettingsDialog::initProxyPage()
{
    QWidget *p10 = new QWidget;
    m_configProxy.setupUi(p10);
    m_proxyProfiles = new EncodingProfilesChooser(p10, EncodingProfilesManager::ProxyClips, true, QStringLiteral("proxy_profile"));
    m_configProxy.proxy_profile_box->addWidget(m_proxyProfiles);
    addPage(p10, i18n("Proxy Clips"), QStringLiteral("zoom-out"));
    connect(m_configProxy.kcfg_generateproxy, &QAbstractButton::toggled, m_configProxy.kcfg_proxyminsize, &QWidget::setEnabled);
    m_configProxy.kcfg_proxyminsize->setEnabled(KdenliveSettings::generateproxy());
    connect(m_configProxy.kcfg_generateimageproxy, &QAbstractButton::toggled, m_configProxy.kcfg_proxyimageminsize, &QWidget::setEnabled);
    m_configProxy.kcfg_proxyimageminsize->setEnabled(KdenliveSettings::generateimageproxy());
    loadExternalProxyProfiles();
    connect(m_configProxy.button_external, &QToolButton::clicked, this, &KdenliveSettingsDialog::configureExternalProxies);
}

void KdenliveSettingsDialog::configureExternalProxies()
{
    // We want to edit the profiles
    CustomCamcorderDialog cd;
    if (cd.exec() == QDialog::Accepted) {
        // reload profiles
        loadExternalProxyProfiles();
    }
}

void KdenliveSettingsDialog::initEnviromentPage()
{
    QWidget *p2 = new QWidget;
    m_configEnv.setupUi(p2);
    m_configEnv.mltpathurl->setMode(KFile::Directory);
    m_configEnv.mltpathurl->lineEdit()->setObjectName(QStringLiteral("kcfg_mltpath"));
    m_configEnv.rendererpathurl->lineEdit()->setObjectName(QStringLiteral("kcfg_meltpath"));
    m_configEnv.ffmpegurl->lineEdit()->setObjectName(QStringLiteral("kcfg_ffmpegpath"));
    m_configEnv.ffplayurl->lineEdit()->setObjectName(QStringLiteral("kcfg_ffplaypath"));
    m_configEnv.ffprobeurl->lineEdit()->setObjectName(QStringLiteral("kcfg_ffprobepath"));
    m_configEnv.mediainfourl->lineEdit()->setObjectName(QStringLiteral("kcfg_mediainfopath"));
    m_configEnv.tmppathurl->setMode(KFile::Directory);
    m_configEnv.tmppathurl->lineEdit()->setObjectName(QStringLiteral("kcfg_currenttmpfolder"));
    m_configEnv.capturefolderurl->setMode(KFile::Directory);
    m_configEnv.capturefolderurl->lineEdit()->setObjectName(QStringLiteral("kcfg_capturefolder"));
    KdenliveSettingsDialog::slotRevealCaptureFolder(KdenliveSettings::capturetoprojectfolder());
    m_configEnv.kcfg_capturetoprojectfolder->setItemText(0, i18n("Use default folder: %1", QStandardPaths::writableLocation(QStandardPaths::MoviesLocation)));
    if (KdenliveSettings::customprojectfolder()) {
        m_configEnv.kcfg_capturetoprojectfolder->setItemText(1, i18n("Always use project folder: %1", KdenliveSettings::defaultprojectfolder()));
    } else {
        m_configEnv.kcfg_capturetoprojectfolder->setItemText(1, i18n("Always use active project folder"));
    }
    connect(m_configEnv.kcfg_capturetoprojectfolder, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            &KdenliveSettingsDialog::slotRevealCaptureFolder);

    // Library folder
    m_configEnv.libraryfolderurl->setMode(KFile::Directory);
    m_configEnv.libraryfolderurl->lineEdit()->setObjectName(QStringLiteral("kcfg_libraryfolder"));
    m_configEnv.libraryfolderurl->setEnabled(!KdenliveSettings::librarytodefaultfolder());
    m_configEnv.libraryfolderurl->setPlaceholderText(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + QStringLiteral("/library"));
    m_configEnv.kcfg_librarytodefaultfolder->setToolTip(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + QStringLiteral("/library"));
    connect(m_configEnv.kcfg_librarytodefaultfolder, &QAbstractButton::clicked, this, &KdenliveSettingsDialog::slotEnableLibraryFolder);

    m_configEnv.kcfg_proxythreads->setMaximum(qMax(1, QThread::idealThreadCount() - 1));

    // Script rendering files folder
    m_configEnv.videofolderurl->setMode(KFile::Directory);
    m_configEnv.videofolderurl->lineEdit()->setObjectName(QStringLiteral("kcfg_videofolder"));
    m_configEnv.videofolderurl->setEnabled(KdenliveSettings::videotodefaultfolder() == 2);
    m_configEnv.videofolderurl->setPlaceholderText(QStandardPaths::writableLocation(QStandardPaths::MoviesLocation));
    m_configEnv.kcfg_videotodefaultfolder->setItemText(0, i18n("Use default folder: %1", QStandardPaths::writableLocation(QStandardPaths::MoviesLocation)));
    if (KdenliveSettings::customprojectfolder()) {
        m_configEnv.kcfg_videotodefaultfolder->setItemText(1, i18n("Always use project folder: %1", KdenliveSettings::defaultprojectfolder()));
    } else {
        m_configEnv.kcfg_videotodefaultfolder->setItemText(1, i18n("Always use active project folder"));
    }
    connect(m_configEnv.kcfg_videotodefaultfolder, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &KdenliveSettingsDialog::slotEnableVideoFolder);

    // Mime types
    QStringList mimes = ClipCreationDialog::getExtensions();
    std::sort(mimes.begin(), mimes.end());
    m_configEnv.supportedmimes->setPlainText(mimes.join(QLatin1Char(' ')));

    connect(m_configEnv.kp_image, &QAbstractButton::clicked, this, &KdenliveSettingsDialog::slotEditImageApplication);
    connect(m_configEnv.kp_audio, &QAbstractButton::clicked, this, &KdenliveSettingsDialog::slotEditAudioApplication);
    connect(m_configEnv.kp_anim, &QAbstractButton::clicked, this, &KdenliveSettingsDialog::slotEditGlaxnimateApplication);

    m_pageEnv = addPage(p2, i18n("Environment"), QStringLiteral("application-x-executable-script"));
}

void KdenliveSettingsDialog::initCapturePage()
{
    // Remove ffmpeg tab, unused
    m_configCapture.tabWidget->removeTab(0);
    m_configCapture.label->setVisible(false);
    m_configCapture.kcfg_defaultcapture->setVisible(false);
    // m_configCapture.tabWidget->removeTab(2);
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

    m_configCapture.tabWidget->setCurrentIndex(KdenliveSettings::defaultcapture());
#ifdef Q_WS_MAC
    m_configCapture.tabWidget->setEnabled(false);
    m_configCapture.kcfg_defaultcapture->setEnabled(false);
    m_configCapture.label->setText(i18n("Capture is not yet available on Mac OS X."));
#endif

    connect(m_configCapture.kcfg_grab_capture_type, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this,
            &KdenliveSettingsDialog::slotUpdateGrabRegionStatus);

    slotUpdateGrabRegionStatus();

    // audio capture channels
    m_configCapture.audiocapturechannels->clear();
    m_configCapture.audiocapturechannels->addItem(i18n("Mono (1 channel)"), 1);
    m_configCapture.audiocapturechannels->addItem(i18n("Stereo (2 channels)"), 2);

    int channelsIndex = m_configCapture.audiocapturechannels->findData(KdenliveSettings::audiocapturechannels());
    m_configCapture.audiocapturechannels->setCurrentIndex(qMax(channelsIndex, 0));
    connect(m_configCapture.audiocapturechannels, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this, [&]() { updateButtons(); });
    // audio capture sample rate
    m_configCapture.audiocapturesamplerate->clear();
    m_configCapture.audiocapturesamplerate->addItem(i18n("44100 Hz"), 44100);
    m_configCapture.audiocapturesamplerate->addItem(i18n("48000 Hz"), 48000);

    int sampleRateIndex = m_configCapture.audiocapturesamplerate->findData(KdenliveSettings::audiocapturesamplerate());
    m_configCapture.audiocapturesamplerate->setCurrentIndex(qMax(sampleRateIndex, 0));
    connect(m_configCapture.audiocapturesamplerate, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this, [&]() { updateButtons(); });

    m_configCapture.labelNoAudioDevices->setVisible(false);

    initAudioRecDevice();
    getBlackMagicDeviceList(m_configCapture.kcfg_decklink_capturedevice);
}

void KdenliveSettingsDialog::initJogShuttlePage()
{
    QWidget *p6 = new QWidget;
    m_configShuttle.setupUi(p6);
#ifdef USE_JOGSHUTTLE
    connect(m_configShuttle.kcfg_enableshuttle, &QCheckBox::stateChanged, this, &KdenliveSettingsDialog::slotCheckShuttle);
    connect(m_configShuttle.shuttledevicelist, SIGNAL(activated(int)), this, SLOT(slotUpdateShuttleDevice(int)));
    connect(m_configShuttle.toolBtnReload, &QAbstractButton::clicked, this, &KdenliveSettingsDialog::slotReloadShuttleDevices);

    slotCheckShuttle(static_cast<int>(KdenliveSettings::enableshuttle()));
    m_configShuttle.shuttledisabled->hide();

    // Store the button pointers into an array for easier handling them in the other functions.
    // TODO: impl enumerator or live with cut and paste :-)))
    setupJogshuttleBtns(KdenliveSettings::shuttledevice());
#if false
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
    m_pageJog = addPage(p6, i18n("JogShuttle"), QStringLiteral("dialog-input-devices"));
}

void KdenliveSettingsDialog::initTranscodePage()
{
    QWidget *p8 = new QWidget;
    m_configTranscode.setupUi(p8);
    m_pageTranscode = addPage(p8, i18n("Transcode"), QStringLiteral("edit-copy"));

    connect(m_configTranscode.button_add, &QAbstractButton::clicked, this, &KdenliveSettingsDialog::slotAddTranscode);
    connect(m_configTranscode.button_delete, &QAbstractButton::clicked, this, &KdenliveSettingsDialog::slotDeleteTranscode);
    connect(m_configTranscode.profiles_list, &QListWidget::itemChanged, this, &KdenliveSettingsDialog::slotDialogModified);
    connect(m_configTranscode.profiles_list, &QListWidget::currentRowChanged, this, &KdenliveSettingsDialog::slotSetTranscodeProfile);
    connect(m_configTranscode.profile_description, &QLineEdit::textChanged, this, &KdenliveSettingsDialog::slotEnableTranscodeUpdate);
    connect(m_configTranscode.profile_extension, &QLineEdit::textChanged, this, &KdenliveSettingsDialog::slotEnableTranscodeUpdate);
    connect(m_configTranscode.profile_parameters, &QPlainTextEdit::textChanged, this, &KdenliveSettingsDialog::slotEnableTranscodeUpdate);
    connect(m_configTranscode.profile_audioonly, &QCheckBox::stateChanged, this, &KdenliveSettingsDialog::slotEnableTranscodeUpdate);

    connect(m_configTranscode.button_update, &QAbstractButton::pressed, this, &KdenliveSettingsDialog::slotUpdateTranscodingProfile);

    m_configTranscode.profile_parameters->setMaximumHeight(QFontMetrics(font()).lineSpacing() * 5);
    loadTranscodeProfiles();
}

void KdenliveSettingsDialog::initSdlPage(bool gpuAllowed)
{
    connect(m_configSdl.reload_blackmagic, &QAbstractButton::clicked, this, &KdenliveSettingsDialog::slotReloadBlackMagic);
    // m_configSdl.kcfg_openglmonitors->setHidden(true);
    connect(m_configSdl.fullscreen_monitor, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this,
            &KdenliveSettingsDialog::slotDialogModified);
    connect(m_configSdl.kcfg_audio_driver, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this,
            &KdenliveSettingsDialog::slotCheckAlsaDriver);
    connect(m_configSdl.kcfg_audio_backend, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this,
            &KdenliveSettingsDialog::slotCheckAudioBackend);

    // enable GPU accel only if Movit is found
    m_configSdl.kcfg_gpu_accel->setEnabled(gpuAllowed);
    m_configSdl.kcfg_gpu_accel->setToolTip(i18n("GPU processing needs MLT compiled with Movit and Rtaudio modules"));
    if (!getBlackMagicOutputDeviceList(m_configSdl.kcfg_blackmagic_output_device)) {
        // No blackmagic card found
        m_configSdl.kcfg_external_display->setEnabled(false);
    }
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

void KdenliveSettingsDialog::slotRevealCaptureFolder(int ix)
{
    m_configEnv.capturefolderurl->setVisible(ix == 2);
    m_configEnv.kcfg_captureprojectsubfolder->setVisible(ix == 3);
}

void KdenliveSettingsDialog::slotEnableLibraryFolder()
{
    m_configEnv.libraryfolderurl->setEnabled(!m_configEnv.kcfg_librarytodefaultfolder->isChecked());
}

void KdenliveSettingsDialog::slotEnableVideoFolder(int ix)
{
    m_configEnv.videofolderurl->setEnabled(ix == 2);
}

void KdenliveSettingsDialog::initDevices()
{
    // Fill audio drivers
    m_configSdl.kcfg_audio_driver->addItem(i18n("Automatic"), QString());
#if defined(Q_OS_WIN)
    // TODO: i18n
    m_configSdl.kcfg_audio_driver->addItem("DirectSound", "directsound");
    m_configSdl.kcfg_audio_driver->addItem("WinMM", "winmm");
    m_configSdl.kcfg_audio_driver->addItem("Wasapi", "wasapi");
#elif !defined(Q_WS_MAC)
    m_configSdl.kcfg_audio_driver->addItem(i18n("ALSA"), "alsa");
    m_configSdl.kcfg_audio_driver->addItem(i18n("PulseAudio"), "pulseaudio");
    m_configSdl.kcfg_audio_driver->addItem(i18n("PipeWire"), "pipewire");
    m_configSdl.kcfg_audio_driver->addItem(i18n("OSS"), "dsp");
    // m_configSdl.kcfg_audio_driver->addItem(i18n("OSS with DMA access"), "dma");
    m_configSdl.kcfg_audio_driver->addItem(i18n("Esound Daemon"), "esd");
    m_configSdl.kcfg_audio_driver->addItem(i18n("ARTS Daemon"), "artsc");
#endif

    if (!KdenliveSettings::audiodrivername().isEmpty())
        for (int i = 1; i < m_configSdl.kcfg_audio_driver->count(); ++i) {
            if (m_configSdl.kcfg_audio_driver->itemData(i).toString() == KdenliveSettings::audiodrivername()) {
                m_configSdl.kcfg_audio_driver->setCurrentIndex(i);
                KdenliveSettings::setAudio_driver(uint(i));
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
    if (KdenliveSettings::consumerslist().isEmpty()) {
        Mlt::Properties *consumers = pCore->getMltRepository()->consumers();
        QStringList consumersItemList;
        consumersItemList.reserve(consumers->count());
        for (int i = 0; i < consumers->count(); ++i) {
            consumersItemList << consumers->get_name(i);
        }
        delete consumers;
        KdenliveSettings::setConsumerslist(consumersItemList);
    }
    if (KdenliveSettings::consumerslist().contains("rtaudio")) {
        m_configSdl.kcfg_audio_backend->addItem(i18n("RtAudio"), "rtaudio");
    }

    if (!KdenliveSettings::audiobackend().isEmpty()) {
        int ix = m_configSdl.kcfg_audio_backend->findData(KdenliveSettings::audiobackend());
        m_configSdl.kcfg_audio_backend->setCurrentIndex(ix);
        KdenliveSettings::setAudio_backend(ix);
    }
    slotCheckAudioBackend();

    // Fill monitors data
    fillMonitorData();
    connect(qApp, &QApplication::screenAdded, this, &KdenliveSettingsDialog::fillMonitorData);
    connect(qApp, &QApplication::screenRemoved, this, &KdenliveSettingsDialog::fillMonitorData);
    loadCurrentV4lProfileInfo();
}

void KdenliveSettingsDialog::fillMonitorData()
{
    QSignalBlocker bk(m_configSdl.fullscreen_monitor);
    m_configSdl.fullscreen_monitor->clear();
    m_configSdl.fullscreen_monitor->addItem(i18n("auto"));
    int ix = 0;
    for (const QScreen *screen : qApp->screens()) {
        QString screenName = screen->name().isEmpty() ? i18n("Monitor %1", ix + 1) : QString("%1: %2").arg(ix + 1).arg(screen->name());
        if (!screen->model().isEmpty()) {
            screenName.append(QString(" - %1").arg(screen->model()));
        }
        if (!screen->manufacturer().isEmpty()) {
            screenName.append(QString(" (%1)").arg(screen->manufacturer()));
        }
        m_configSdl.fullscreen_monitor->addItem(screenName, QString("%1:%2").arg(QString::number(ix), screen->serialNumber()));
        ix++;
    }
    if (!KdenliveSettings::fullscreen_monitor().isEmpty()) {
        int ix = m_configSdl.fullscreen_monitor->findData(KdenliveSettings::fullscreen_monitor());
        if (ix > -1) {
            m_configSdl.fullscreen_monitor->setCurrentIndex(ix);
        } else {
            // Monitor not found, reset
            m_configSdl.fullscreen_monitor->setCurrentIndex(0);
            KdenliveSettings::setFullscreen_monitor(QString());
        }
    }
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
            m_configCapture.kcfg_v4l_alsadevice->addItem(devicestr.section(QLatin1Char(':'), -1).simplified(), QStringLiteral("hw:%1,%2").arg(card, device));
        }
    }
}

void KdenliveSettingsDialog::showPage(Kdenlive::ConfigPage page, int option)
{
    switch (page) {
    case Kdenlive::PageMisc:
        setCurrentPage(m_pageMisc);
        break;
    case Kdenlive::PageEnv:
        setCurrentPage(m_pageEnv);
        break;
    case Kdenlive::PageTimeline:
        setCurrentPage(m_pageTimeline);
        break;
    case Kdenlive::PageTools:
        setCurrentPage(m_pageTools);
        break;
    case Kdenlive::PageCapture:
        setCurrentPage(m_pageCapture);
        m_configCapture.tabWidget->setCurrentIndex(option);
        break;
    case Kdenlive::PageJogShuttle:
        setCurrentPage(m_pageJog);
        break;
    case Kdenlive::PagePlayback:
        setCurrentPage(m_pagePlay);
        break;
    case Kdenlive::PageTranscode:
        setCurrentPage(m_pageTranscode);
        break;
    case Kdenlive::PageProjectDefaults:
        setCurrentPage(m_pageProject);
        break;
    case Kdenlive::PageColorsGuides:
        setCurrentPage(m_pageColors);
        break;
    case Kdenlive::PageSpeech:
        checkSpeechDependencies();
        setCurrentPage(m_pageSpeech);
        break;
    default:
        setCurrentPage(m_pageMisc);
    }
}

void KdenliveSettingsDialog::slotEditAudioApplication()
{
    QUrl url = KUrlRequesterDialog::getUrl(QUrl::fromLocalFile(KdenliveSettings::defaultaudioapp()), this, i18n("Enter path to the audio editing application"));
    if (!url.isEmpty()) {
        m_configEnv.kcfg_defaultaudioapp->setText(url.toLocalFile());
    }
}

void KdenliveSettingsDialog::slotEditImageApplication()
{
    QUrl url = KUrlRequesterDialog::getUrl(QUrl::fromLocalFile(KdenliveSettings::defaultimageapp()), this, i18n("Enter path to the image editing application"));
    if (!url.isEmpty()) {
        m_configEnv.kcfg_defaultimageapp->setText(url.toLocalFile());
    }
}

void KdenliveSettingsDialog::slotEditGlaxnimateApplication()
{
    QUrl url = KUrlRequesterDialog::getUrl(QUrl::fromLocalFile(KdenliveSettings::glaxnimatePath()), this, i18n("Enter path to the Glaxnimate application"));
    if (!url.isEmpty()) {
        m_configEnv.kcfg_glaxnimatePath->setText(url.toLocalFile());
    }
}

void KdenliveSettingsDialog::slotCheckShuttle(int state)
{
#ifdef USE_JOGSHUTTLE
    m_configShuttle.config_group->setEnabled(state != Qt::Unchecked);
    m_configShuttle.shuttledevicelist->clear();

    QStringList devNames = KdenliveSettings::shuttledevicenames();
    QStringList devPaths = KdenliveSettings::shuttledevicepaths();

    if (devNames.count() != devPaths.count()) {
        return;
    }
    for (int i = 0; i < devNames.count(); ++i) {
        m_configShuttle.shuttledevicelist->addItem(devNames.at(i), devPaths.at(i));
    }
    if (state != Qt::Unchecked) {
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

void KdenliveSettingsDialog::updateExternalApps()
{
    m_configEnv.kcfg_defaultimageapp->setText(KdenliveSettings::defaultimageapp());
    m_configEnv.kcfg_defaultaudioapp->setText(KdenliveSettings::defaultaudioapp());
    m_configEnv.kcfg_glaxnimatePath->setText(KdenliveSettings::glaxnimatePath());
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
        QString errors;
        Wizard::slotCheckPrograms(infos, warnings, errors);
        m_configEnv.ffmpegurl->setText(KdenliveSettings::ffmpegpath());
        m_configEnv.ffplayurl->setText(KdenliveSettings::ffplaypath());
        m_configEnv.ffprobeurl->setText(KdenliveSettings::ffprobepath());
    }
    if (m_configEnv.mediainfourl->text().isEmpty()) {
        m_configEnv.mediainfourl->setText(KdenliveSettings::mediainfopath());
    }

    if (m_configTimeline.kcfg_trackheight->value() == 0) {
        QFont ft = QFontDatabase::systemFont(QFontDatabase::SmallestReadableFont);
        // Default unit for timeline.qml objects size
        int baseUnit = qMax(28, qRound(QFontInfo(ft).pixelSize() * 1.8));
        int trackHeight = qMax(50, int(2.2 * baseUnit + 6));
        m_configTimeline.kcfg_trackheight->setValue(trackHeight);
    } else if (m_configTimeline.kcfg_trackheight->value() != KdenliveSettings::trackheight()) {
        QFont ft = QFontDatabase::systemFont(QFontDatabase::SmallestReadableFont);
        // Default unit for timeline.qml objects size
        int baseUnit = qMax(28, qRound(QFontInfo(ft).pixelSize() * 1.8));
        if (m_configTimeline.kcfg_trackheight->value() < baseUnit) {
            m_configTimeline.kcfg_trackheight->setValue(baseUnit);
        }
    }

    bool resetConsumer = false;
    bool fullReset = false;
    bool updateLibrary = false;

    // Guide categories
    const QStringList updatedGuides = m_guidesCategories->updatedGuides();
    if (KdenliveSettings::guidesCategories() != updatedGuides) {
        KdenliveSettings::setGuidesCategories(updatedGuides);
    }

    /*if (m_configShuttle.shuttledevicelist->count() > 0) {
    QString device = m_configShuttle.shuttledevicelist->itemData(m_configShuttle.shuttledevicelist->currentIndex()).toString();
    if (device != KdenliveSettings::shuttledevice()) KdenliveSettings::setShuttledevice(device);
    }*/
    m_tlPreviewProfiles->hideMessage();
    if (m_configProject.projecturl->url().toLocalFile() != KdenliveSettings::defaultprojectfolder()) {
        KdenliveSettings::setDefaultprojectfolder(m_configProject.projecturl->url().toLocalFile());
        if (!KdenliveSettings::sameprojectfolder()) {
            m_configEnv.kcfg_videotodefaultfolder->setItemText(1, i18n("Always use project folder: %1", KdenliveSettings::defaultprojectfolder()));
            m_configEnv.kcfg_capturetoprojectfolder->setItemText(1, i18n("Always use project folder: %1", KdenliveSettings::defaultprojectfolder()));
        }
    }

    if (m_configProject.kcfg_customprojectfolder->isChecked() != KdenliveSettings::customprojectfolder()) {
        if (KdenliveSettings::customprojectfolder()) {
            m_configEnv.kcfg_videotodefaultfolder->setItemText(1, i18n("Always use active project folder"));
            m_configEnv.kcfg_capturetoprojectfolder->setItemText(1, i18n("Always use active project folder"));
        } else {
            m_configEnv.kcfg_videotodefaultfolder->setItemText(1, i18n("Always use project folder: %1", KdenliveSettings::defaultprojectfolder()));
            m_configEnv.kcfg_capturetoprojectfolder->setItemText(1, i18n("Always use project folder: %1", KdenliveSettings::defaultprojectfolder()));
        }
    }

    if (m_configSdl.fullscreen_monitor->currentData().toString() != KdenliveSettings::fullscreen_monitor()) {
        KdenliveSettings::setFullscreen_monitor(m_configSdl.fullscreen_monitor->currentData().toString());
    }

    if (m_configEnv.capturefolderurl->url().toLocalFile() != KdenliveSettings::capturefolder()) {
        KdenliveSettings::setCapturefolder(m_configEnv.capturefolderurl->url().toLocalFile());
    }

    if (m_configEnv.kcfg_captureprojectsubfolder->text() != KdenliveSettings::captureprojectsubfolder()) {
        KdenliveSettings::setCaptureprojectsubfolder(m_configEnv.kcfg_captureprojectsubfolder->text());
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

    if (m_configCapture.kcfg_v4l_format->currentIndex() != int(KdenliveSettings::v4l_format())) {
        saveCurrentV4lProfile();
        KdenliveSettings::setV4l_format(0);
    }

    // Check if screengrab is fullscreen
    if (m_configCapture.kcfg_grab_capture_type->currentIndex() != KdenliveSettings::grab_capture_type()) {
        KdenliveSettings::setGrab_capture_type(m_configCapture.kcfg_grab_capture_type->currentIndex());
        Q_EMIT updateFullScreenGrab();
    }

    // Check audio capture changes
    if (KdenliveSettings::audiocapturechannels() != m_configCapture.audiocapturechannels->currentData().toInt() ||
        KdenliveSettings::audiocapturevolume() != m_configCapture.kcfg_audiocapturevolume->value() ||
        KdenliveSettings::audiocapturesamplerate() != m_configCapture.audiocapturesamplerate->currentData().toInt()) {
        KdenliveSettings::setAudiocapturechannels(m_configCapture.audiocapturechannels->currentData().toInt());
        KdenliveSettings::setAudiocapturevolume(m_configCapture.kcfg_audiocapturevolume->value());
        KdenliveSettings::setAudiocapturesamplerate(m_configCapture.audiocapturesamplerate->currentData().toInt());
        Q_EMIT resetAudioMonitoring();
    }

    // Check encoding profiles
    // FFmpeg
    QString string = m_v4lProfiles->currentParams();
    if (string != KdenliveSettings::v4l_parameters()) {
        KdenliveSettings::setV4l_parameters(string);
    }
    string = m_v4lProfiles->currentExtension();
    if (string != KdenliveSettings::v4l_extension()) {
        KdenliveSettings::setV4l_extension(string);
    }

    // screengrab
    string = m_grabProfiles->currentParams();
    if (string != KdenliveSettings::grab_parameters()) {
        KdenliveSettings::setGrab_parameters(string);
    }
    string = m_grabProfiles->currentExtension();
    if (string != KdenliveSettings::grab_extension()) {
        KdenliveSettings::setGrab_extension(string);
    }

    // decklink
    string = m_decklinkProfiles->currentParams();
    if (string != KdenliveSettings::decklink_parameters()) {
        KdenliveSettings::setDecklink_parameters(string);
    }
    string = m_decklinkProfiles->currentExtension();
    if (string != KdenliveSettings::decklink_extension()) {
        KdenliveSettings::setDecklink_extension(string);
    }

    // proxies
    string = m_proxyProfiles->currentParams();
    if (string != KdenliveSettings::proxyparams()) {
        KdenliveSettings::setProxyparams(string);
    }
    string = m_proxyProfiles->currentExtension();
    if (string != KdenliveSettings::proxyextension()) {
        KdenliveSettings::setProxyextension(string);
    }

    // external proxies
    QString profilestr = m_configProxy.kcfg_external_proxy_profile->currentData().toString();
    if (!profilestr.isEmpty() && (profilestr != KdenliveSettings::externalProxyProfile())) {
        KdenliveSettings::setExternalProxyProfile(profilestr);
    }

    // timeline preview
    string = m_tlPreviewProfiles->currentParams();
    if (string != KdenliveSettings::previewparams()) {
        KdenliveSettings::setPreviewparams(string);
    }
    string = m_tlPreviewProfiles->currentExtension();
    if (string != KdenliveSettings::previewextension()) {
        KdenliveSettings::setPreviewextension(string);
    }

    if (updateLibrary) {
        Q_EMIT updateLibraryFolder();
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

    if (m_configColors.kcfg_window_background->color() != KdenliveSettings::window_background()) {
        KdenliveSettings::setWindow_background(m_configColors.kcfg_window_background->color());
        Q_EMIT updateMonitorBg();
    }

    if (m_configColors.kcfg_thumbColor1->color() != KdenliveSettings::thumbColor1() ||
        m_configColors.kcfg_thumbColor2->color() != KdenliveSettings::thumbColor2()) {
        KdenliveSettings::setThumbColor1(m_configColors.kcfg_thumbColor1->color());
        KdenliveSettings::setThumbColor2(m_configColors.kcfg_thumbColor2->color());
        Q_EMIT pCore->window()->getCurrentTimeline()->controller()->colorsChanged();
        pCore->getMonitor(Kdenlive::ClipMonitor)->refreshAudioThumbs();
    }

    if (m_configColors.kcfg_overlayColor->color() != KdenliveSettings::overlayColor()) {
        KdenliveSettings::setOverlayColor(m_configColors.kcfg_overlayColor->color());
        pCore->getMonitor(Kdenlive::ProjectMonitor)->getControllerProxy()->colorsChanged();
        pCore->getMonitor(Kdenlive::ClipMonitor)->getControllerProxy()->colorsChanged();
    }

    if (m_configMisc.kcfg_tabposition->currentIndex() != KdenliveSettings::tabposition()) {
        KdenliveSettings::setTabposition(m_configMisc.kcfg_tabposition->currentIndex());
    }

    if (m_configTimeline.kcfg_displayallchannels->isChecked() != KdenliveSettings::displayallchannels()) {
        KdenliveSettings::setDisplayallchannels(m_configTimeline.kcfg_displayallchannels->isChecked());
        Q_EMIT audioThumbFormatChanged();
        pCore->getMonitor(Kdenlive::ClipMonitor)->refreshAudioThumbs();
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
        Q_EMIT resetView();
    }

    if (m_configTimeline.kcfg_autoscroll->isChecked() != KdenliveSettings::autoscroll()) {
        KdenliveSettings::setAutoscroll(m_configTimeline.kcfg_autoscroll->isChecked());
        Q_EMIT pCore->autoScrollChanged();
    }

    if (m_configTimeline.kcfg_pauseonseek->isChecked() != KdenliveSettings::pauseonseek()) {
        KdenliveSettings::setPauseonseek(m_configTimeline.kcfg_pauseonseek->isChecked());
    }

    if (m_configTimeline.kcfg_seekonaddeffect->isChecked() != KdenliveSettings::seekonaddeffect()) {
        KdenliveSettings::setSeekonaddeffect(m_configTimeline.kcfg_seekonaddeffect->isChecked());
    }

    if (m_configTimeline.kcfg_scrollvertically->isChecked() != KdenliveSettings::scrollvertically()) {
        KdenliveSettings::setScrollvertically(m_configTimeline.kcfg_scrollvertically->isChecked());
        Q_EMIT pCore->window()->getCurrentTimeline()->controller()->scrollVerticallyChanged();
    }

    // Speech
    switch (m_configSpeech.speech_stack->currentIndex()) {
    case 1:
        if (KdenliveSettings::speechEngine() != QLatin1String("whisper")) {
            KdenliveSettings::setSpeechEngine(QStringLiteral("whisper"));
            Q_EMIT pCore->speechEngineChanged();
        }
        break;
    default:
        if (!KdenliveSettings::speechEngine().isEmpty()) {
            KdenliveSettings::setSpeechEngine(QString());
            Q_EMIT pCore->speechEngineChanged();
        }
        break;
    }

    if (m_configSpeech.combo_wr_lang->currentData().toString() != KdenliveSettings::whisperLanguage()) {
        KdenliveSettings::setWhisperLanguage(m_configSpeech.combo_wr_lang->currentData().toString());
    }
    if (m_configSpeech.combo_wr_model->currentData().toString() != KdenliveSettings::whisperModel()) {
        KdenliveSettings::setWhisperModel(m_configSpeech.combo_wr_model->currentData().toString());
    }
    if (m_configSpeech.combo_wr_device->currentData().toString() != KdenliveSettings::whisperDevice()) {
        KdenliveSettings::setWhisperDevice(m_configSpeech.combo_wr_device->currentData().toString());
    }

    // Mimes
    if (m_configEnv.kcfg_addedExtensions->text() != KdenliveSettings::addedExtensions()) {
        // Update list
        KdenliveSettings::setAddedExtensions(m_configEnv.kcfg_addedExtensions->text());
        QStringList mimes = ClipCreationDialog::getExtensions();
        std::sort(mimes.begin(), mimes.end());
        m_configEnv.supportedmimes->setPlainText(mimes.join(QLatin1Char(' ')));
    }

    // proxy/transcode max concurrent jobs
    if (m_configEnv.kcfg_proxythreads->value() != KdenliveSettings::proxythreads()) {
        KdenliveSettings::setProxythreads(m_configEnv.kcfg_proxythreads->value());
        pCore->taskManager.updateConcurrency();
    }

    KConfigDialog::settingsChangedSlot();
    // KConfigDialog::updateSettings();
    if (resetConsumer) {
        Q_EMIT doResetConsumer(fullReset);
    }
    if (restart) {
        Q_EMIT restartKdenlive();
    }
    Q_EMIT checkTabPosition();

    // remembering Config dialog size
    KSharedConfigPtr config = KSharedConfig::openConfig();
    KConfigGroup settingsGroup(config, "settings");
    settingsGroup.writeEntry("dialogSize", QVariant(size()));
}

void KdenliveSettingsDialog::slotCheckAlsaDriver()
{
    QString value = m_configSdl.kcfg_audio_driver->currentData().toString();
    m_configSdl.kcfg_audio_device->setEnabled(value == QLatin1String("alsa"));
}

void KdenliveSettingsDialog::slotCheckAudioBackend()
{
    bool isSdl = m_configSdl.kcfg_audio_backend->currentData().toString().startsWith(QLatin1String("sdl"));
    m_configSdl.label_audio_driver->setEnabled(isSdl);
    m_configSdl.kcfg_audio_driver->setEnabled(isSdl);
    m_configSdl.label_audio_device->setEnabled(isSdl);
    m_configSdl.kcfg_audio_device->setEnabled(isSdl);
}

void KdenliveSettingsDialog::loadTranscodeProfiles()
{
    KSharedConfigPtr config = KSharedConfig::openConfig(QStringLiteral("kdenlivetranscodingrc"), KConfig::CascadeConfig, QStandardPaths::AppDataLocation);
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
    bool ok;
    QString presetName =
        QInputDialog::getText(this, i18nc("@title:window", "Enter Preset Name"), i18n("Enter the name of this preset:"), QLineEdit::Normal, QString(), &ok);
    if (!ok) return;
    if (!m_configTranscode.profiles_list->findItems(presetName, Qt::MatchExactly).isEmpty()) {
        KMessageBox::error(this, i18n("A profile with that name already exists"));
        return;
    }
    QListWidgetItem *item = new QListWidgetItem(presetName);
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
    if (m_configTranscode.profile_extension->text().isEmpty()) {
        allow = false;
    }
    m_configTranscode.button_update->setEnabled(allow);
}

void KdenliveSettingsDialog::slotSetTranscodeProfile()
{
    m_configTranscode.profile_box->setEnabled(false);
    m_configTranscode.button_update->setEnabled(false);
    m_configTranscode.profile_description->clear();
    m_configTranscode.profile_extension->clear();
    m_configTranscode.profile_parameters->clear();
    m_configTranscode.profile_audioonly->setChecked(false);
    QListWidgetItem *item = m_configTranscode.profiles_list->currentItem();
    if (!item) {
        return;
    }
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
    if (KdenliveSettings::audiocapturechannels() != m_configCapture.audiocapturechannels->currentData().toInt()) {
        return true;
    }
    if (KdenliveSettings::audiocapturesamplerate() != m_configCapture.audiocapturesamplerate->currentData().toInt()) {
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

    QStringList pixelformats = info.split('>', Qt::SkipEmptyParts);
    QString itemSize;
    QStringList itemRates;
    for (int i = 0; i < pixelformats.count(); ++i) {
        QString format = pixelformats.at(i).section(QLatin1Char(':'), 0, 0);
        QStringList sizes = pixelformats.at(i).split(':', Qt::SkipEmptyParts);
        sizes.takeFirst();
        for (int j = 0; j < sizes.count(); ++j) {
            itemSize = sizes.at(j).section(QLatin1Char('='), 0, 0);
            itemRates = sizes.at(j).section(QLatin1Char('='), 1, 1).split(QLatin1Char(','), Qt::SkipEmptyParts);
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

void KdenliveSettingsDialog::loadExternalProxyProfiles()
{
    // load proxy profiles
    KConfigGroup group(KSharedConfig::openConfig(QStringLiteral("externalproxies.rc"), KConfig::CascadeConfig, QStandardPaths::AppDataLocation), "proxy");
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
        m_configProxy.kcfg_external_proxy_profile->setCurrentIndex(m_configProxy.kcfg_external_proxy_profile->findData(currentItem));
    }
    m_configProxy.kcfg_external_proxy_profile->blockSignals(false);
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
    QTimer::singleShot(200, this, [&]() { slotUpdateShuttleDevice(); });
#endif // USE_JOGSHUTTLE
}

void KdenliveSettingsDialog::initSpeechPage()
{
    m_stt = new SpeechToText(SpeechToText::EngineType::EngineVosk, this);
    m_sttWhisper = new SpeechToText(SpeechToText::EngineType::EngineWhisper, this);
    m_configSpeech.whisperInfo->setWordWrap(true);
    m_configSpeech.whisperInfo->setText(
        i18n("On first run, Whisper will <b>download the chosen model</b>. After that, processing will happen offline. Cpu processing is very slow."));
    // Python env info label
    PythonDependencyMessage *pythonEnvLabel = new PythonDependencyMessage(this, m_sttWhisper, true);
    m_configEnv.message_layout_2->addWidget(pythonEnvLabel);
    // Also show VOSK setup messages in the python env page
    connect(m_stt, &AbstractPythonInterface::setupMessage,
            [pythonEnvLabel](const QString message, int type) { pythonEnvLabel->doShowMessage(message, KMessageWidget::MessageType(type)); });
    connect(m_stt, &AbstractPythonInterface::venvSetupChanged, this, [this]() {
        QSignalBlocker bk(m_configEnv.kcfg_usePythonVenv);
        m_configEnv.kcfg_usePythonVenv->setChecked(KdenliveSettings::usePythonVenv());
    });
    connect(m_sttWhisper, &AbstractPythonInterface::venvSetupChanged, this, [this]() {
        QSignalBlocker bk(m_configEnv.kcfg_usePythonVenv);
        m_configEnv.kcfg_usePythonVenv->setChecked(KdenliveSettings::usePythonVenv());
    });
    m_sttWhisper->checkPython(KdenliveSettings::usePythonVenv(), true);
    connect(m_configSpeech.kcfg_enableSeamless, &QCheckBox::toggled, [this](bool toggled) {
        m_sttWhisper->buildWhisperDeps(toggled);
        m_sttWhisper->checkDependencies(true);
        if (toggled) {
            m_configSpeech.whisperInfo->setText(
                i18n("On first run, SeamlessM4T will <b>download 9Gb of model data</b>. After that, translations will happen offline."));
        } else {
            m_configSpeech.whisperInfo->setText(
                i18n("On first run, Whisper will <b>download the chosen model</b>. After that, processing will happen offline. Cpu processing is very slow."));
        }
    });

    // Basic info about model folders
    const QString whisperModelFolder = QStandardPaths::locate(QStandardPaths::GenericCacheLocation, QStringLiteral("whisper"), QStandardPaths::LocateDirectory);
    if (!whisperModelFolder.isEmpty()) {
        m_configSpeech.model_folder_label->setText(QStringLiteral("<a href=\"%1\">%2</a>").arg(whisperModelFolder, i18n("Models folder")));
        m_configSpeech.model_folder_label->setVisible(true);
        KIO::DirectorySizeJob *job = KIO::directorySize(QUrl::fromLocalFile(whisperModelFolder));
        connect(job, &KJob::result, this, [job, label = m_configSpeech.model_size]() {
            label->setText(KIO::convertSize(job->totalSize()));
            job->deleteLater();
        });
    } else {
        m_configSpeech.model_folder_label->setVisible(false);
    }
    QString voskModelFolder = KdenliveSettings::vosk_folder_path();
    if (voskModelFolder.isEmpty()) {
        voskModelFolder = QStandardPaths::locate(QStandardPaths::AppDataLocation, QStringLiteral("speechmodels"), QStandardPaths::LocateDirectory);
    }
    if (!voskModelFolder.isEmpty()) {
        m_configSpeech.modelV_folder_label->setText(QStringLiteral("<a href=\"%1\">%2</a>").arg(voskModelFolder, i18n("Models folder")));
        m_configSpeech.modelV_folder_label->setVisible(true);
        KIO::DirectorySizeJob *job = KIO::directorySize(QUrl::fromLocalFile(voskModelFolder));
        connect(job, &KJob::result, this, [job, label = m_configSpeech.modelV_size]() {
            label->setText(KIO::convertSize(job->totalSize()));
            job->deleteLater();
        });
    } else {
        m_configSpeech.modelV_folder_label->setVisible(false);
    }
    connect(m_configSpeech.modelV_folder_label, &QLabel::linkActivated, [](const QString &link) { KIO::highlightInFileManager({QUrl::fromLocalFile(link)}); });
    connect(m_configSpeech.model_folder_label, &QLabel::linkActivated, [](const QString &link) { KIO::highlightInFileManager({QUrl::fromLocalFile(link)}); });
    QButtonGroup *speechEngineSelection = new QButtonGroup(this);
    speechEngineSelection->addButton(m_configSpeech.engine_vosk);
    speechEngineSelection->addButton(m_configSpeech.engine_whisper);
    connect(speechEngineSelection, &QButtonGroup::buttonClicked, [this](QAbstractButton *button) {
        if (button == m_configSpeech.engine_vosk) {
            m_configSpeech.speech_stack->setCurrentIndex(0);
            m_stt->checkDependencies(false);
        } else if (button == m_configSpeech.engine_whisper) {
            m_configSpeech.speech_stack->setCurrentIndex(1);
            m_sttWhisper->checkDependencies(false);
        }
    });
    if (KdenliveSettings::speechEngine() == QLatin1String("whisper")) {
        m_configSpeech.engine_whisper->setChecked(true);
        m_configSpeech.speech_stack->setCurrentIndex(1);
    } else {
        m_configSpeech.engine_vosk->setChecked(true);
        m_configSpeech.speech_stack->setCurrentIndex(0);
    }
    // Python setup
    m_configEnv.pythonSetupMessage->hide();
    connect(m_sttWhisper, &SpeechToText::gotPythonSize, m_configEnv.label_python_size, &QLabel::setText);
    connect(m_configEnv.kcfg_usePythonVenv, &QCheckBox::stateChanged, this, [this](int state) {
        if (m_sttWhisper->installInProcess()) {
            return;
        }
        m_configEnv.pythonSetupMessage->setMessageType(KMessageWidget::Information);
        if (state == Qt::Checked) {
            m_configEnv.pythonSetupMessage->setText(i18n("Setting up virtual environment…"));
        } else {
            m_configEnv.pythonSetupMessage->setText(i18n("Disabling virtual environment"));
        }
        m_configEnv.pythonSetupMessage->show();
        qApp->processEvents();
        bool updatePython = m_sttWhisper->checkPython(state == Qt::Checked, true);
        if (!updatePython) {
            // Setting up venv failed
            m_configEnv.kcfg_usePythonVenv->setChecked(false);
        } else {
            // Venv setting changed, refresh all speech interfaces
            m_stt->checkDependencies(true);
            m_sttWhisper->checkDependencies(true);
        }
        m_configEnv.pythonSetupMessage->hide();
    });
    connect(m_configEnv.button_python_delete, &QPushButton::clicked, this, [this]() {
        m_configEnv.pythonSetupMessage->setMessageType(KMessageWidget::Information);
        m_configEnv.pythonSetupMessage->setText(i18nc("@label:textbox", "Removing the virtual environment folder."));
        m_configEnv.pythonSetupMessage->show();
        if (m_sttWhisper->removePythonVenv()) {
            m_configEnv.kcfg_usePythonVenv->setChecked(false);
            m_configEnv.pythonSetupMessage->hide();
            m_configEnv.label_python_size->setText(i18nc("@label:textbox", "No python venv found"));
        } else {
            m_configEnv.pythonSetupMessage->setMessageType(KMessageWidget::Warning);
            m_configEnv.pythonSetupMessage->setText(i18nc("@label:textbox", "Failed to remove the virtual environment folder."));
        }
    });

    // Whisper
    m_configSpeech.combo_wr_device->addItem(i18n("Probing..."));
    PythonDependencyMessage *msgWhisper = new PythonDependencyMessage(this, m_sttWhisper);
    m_configSpeech.message_layout_wr->addWidget(msgWhisper);
    QList<std::pair<QString, QString>> whisperModels = m_sttWhisper->whisperModels();
    for (auto &w : whisperModels) {
        m_configSpeech.combo_wr_model->addItem(w.first, w.second);
    }
    int ix = m_configSpeech.combo_wr_model->findData(KdenliveSettings::whisperModel());
    if (ix > -1) {
        m_configSpeech.combo_wr_model->setCurrentIndex(ix);
    }
    QMap<QString, QString> whisperLanguages = m_sttWhisper->whisperLanguages();
    QMapIterator<QString, QString> j(whisperLanguages);
    while (j.hasNext()) {
        j.next();
        m_configSpeech.combo_wr_lang->addItem(j.key(), j.value());
    }
    ix = m_configSpeech.combo_wr_lang->findData(KdenliveSettings::whisperLanguage());
    if (ix > -1) {
        m_configSpeech.combo_wr_lang->setCurrentIndex(ix);
    }
    m_configSpeech.script_log->hide();
    connect(m_sttWhisper, &SpeechToText::scriptStarted, [this]() { QMetaObject::invokeMethod(m_configSpeech.script_log, "clear"); });
    connect(m_sttWhisper, &SpeechToText::installFeedback, [this](const QString jobData) {
        QMetaObject::invokeMethod(m_configSpeech.script_log, "show", Qt::QueuedConnection);
        QMetaObject::invokeMethod(m_configSpeech.script_log, "appendPlainText", Q_ARG(QString, jobData));
    });
    connect(m_sttWhisper, &SpeechToText::scriptFinished, [msgWhisper]() { QMetaObject::invokeMethod(msgWhisper, "checkAfterInstall", Qt::QueuedConnection); });

    connect(m_sttWhisper, &SpeechToText::scriptFeedback, [this](const QStringList jobData) {
        m_configSpeech.combo_wr_device->clear();
        for (auto &s : jobData) {
            if (s.contains(QLatin1Char('#'))) {
                m_configSpeech.combo_wr_device->addItem(s.section(QLatin1Char('#'), 1).simplified(), s.section(QLatin1Char('#'), 0, 0).simplified());
            } else {
                m_configSpeech.combo_wr_device->addItem(s.simplified(), s.simplified());
            }
        }
    });
    connect(m_sttWhisper, &SpeechToText::scriptGpuCheckFinished, [this]() {
        int ix = m_configSpeech.combo_wr_device->findData(KdenliveSettings::whisperDevice());
        if (ix > -1) {
            m_configSpeech.combo_wr_device->setCurrentIndex(ix);
        }
    });
    connect(m_sttWhisper, &SpeechToText::dependenciesAvailable, this, [&]() { m_sttWhisper->runConcurrentScript(QStringLiteral("checkgpu.py"), {}); });

    // VOSK
    m_configSpeech.vosk_folder->setPlaceholderText(
        QStandardPaths::locate(QStandardPaths::AppDataLocation, QStringLiteral("speechmodels"), QStandardPaths::LocateDirectory));
    PythonDependencyMessage *msgVosk = new PythonDependencyMessage(this, m_stt);
    m_configSpeech.message_layout->addWidget(msgVosk);

    connect(m_stt, &SpeechToText::dependenciesAvailable, this, [&]() {
        if (m_speechListWidget->count() == 0) {
            doShowSpeechMessage(i18n("Please add a speech model."), KMessageWidget::Information);
        }
    });
    connect(m_stt, &SpeechToText::dependenciesMissing, this, [&](const QStringList &) { m_configSpeech.speech_info->animatedHide(); });
    connect(m_stt, &SpeechToText::scriptStarted, [this]() { QMetaObject::invokeMethod(m_configSpeech.script_log, "clear"); });
    connect(m_stt, &SpeechToText::installFeedback, [this](const QString jobData) {
        QMetaObject::invokeMethod(m_configSpeech.script_log, "show", Qt::QueuedConnection);
        QMetaObject::invokeMethod(m_configSpeech.script_log, "appendPlainText", Q_ARG(QString, jobData));
    });
    connect(m_stt, &SpeechToText::scriptFinished, [msgVosk]() { QMetaObject::invokeMethod(msgVosk, "checkAfterInstall", Qt::QueuedConnection); });

    m_speechListWidget = new SpeechList(this);
    connect(m_speechListWidget, &SpeechList::getDictionary, this, &KdenliveSettingsDialog::getDictionary);
    QVBoxLayout *l = new QVBoxLayout(m_configSpeech.list_frame);
    l->setContentsMargins(0, 0, 0, 0);
    l->addWidget(m_speechListWidget);
    m_configSpeech.speech_info->setWordWrap(true);
    connect(m_configSpeech.check_config, &QPushButton::clicked, this, &KdenliveSettingsDialog::slotCheckSttConfig);

    connect(m_configSpeech.custom_vosk_folder, &QCheckBox::stateChanged, this, [this](int state) {
        m_configSpeech.vosk_folder->setEnabled(state != Qt::Unchecked);
        if (state == Qt::Unchecked) {
            // Clear custom folder
            m_configSpeech.vosk_folder->clear();
            KdenliveSettings::setVosk_folder_path(QString());
            slotParseVoskDictionaries();
        }
    });
    connect(m_configSpeech.vosk_folder, &KUrlRequester::urlSelected, this, [this](const QUrl &url) {
        KdenliveSettings::setVosk_folder_path(url.toLocalFile());
        slotParseVoskDictionaries();
    });
    m_configSpeech.models_url->setText(
        i18n("Download speech models from: <a href=\"https://alphacephei.com/vosk/models\">https://alphacephei.com/vosk/models</a>"));
    connect(m_configSpeech.models_url, &QLabel::linkActivated, this, [&](const QString &contents) {
        qDebug() << "=== LINK CLICKED: " << contents;
        auto *job = new KIO::OpenUrlJob(QUrl(contents));
#if KIO_VERSION >= QT_VERSION_CHECK(5, 98, 0)
        job->setUiDelegate(KIO::createDefaultJobUiDelegate(KJobUiDelegate::AutoHandlingEnabled, this));
#else
        job->setUiDelegate(new KIO::JobUiDelegate(KJobUiDelegate::AutoHandlingEnabled, this));
#endif
        // methods like setRunExecutables, setSuggestedFilename, setEnableExternalBrowser, setFollowRedirections
        // exist in both classes
        job->start();
    });
    connect(m_configSpeech.button_add, &QToolButton::clicked, this, [this]() { this->getDictionary(); });
    connect(m_configSpeech.button_delete, &QToolButton::clicked, this, &KdenliveSettingsDialog::removeDictionary);
    connect(this, &KdenliveSettingsDialog::parseDictionaries, this, &KdenliveSettingsDialog::slotParseVoskDictionaries);
    slotParseVoskDictionaries();
}

void KdenliveSettingsDialog::slotCheckSttConfig()
{
    m_configSpeech.check_config->setEnabled(false);
    qApp->processEvents();
    if (m_configSpeech.engine_vosk->isChecked()) {
        m_stt->checkDependencies(true);
    } else {
        m_sttWhisper->checkDependencies(true);
    }
    // Leave button disabled for 3 seconds so that the user doesn't trigger it again while it is processing
    QTimer::singleShot(3000, this, [&]() { m_configSpeech.check_config->setEnabled(true); });
}

void KdenliveSettingsDialog::doShowSpeechMessage(const QString &message, int messageType)
{
    m_configSpeech.speech_info->setMessageType(static_cast<KMessageWidget::MessageType>(messageType));
    m_configSpeech.speech_info->setText(message);
    m_configSpeech.speech_info->animatedShow();
}

void KdenliveSettingsDialog::getDictionary(const QUrl &sourceUrl)
{
    QUrl url = KUrlRequesterDialog::getUrl(sourceUrl, this, i18n("Enter url for the new dictionary"));
    if (url.isEmpty()) {
        return;
    }
    if (!url.isLocalFile()) {
        KIO::FileCopyJob *copyjob = KIO::file_copy(url, QUrl::fromLocalFile(QDir::temp().absoluteFilePath(url.fileName())));
        doShowSpeechMessage(i18n("Downloading model…"), KMessageWidget::Information);
        connect(copyjob, &KIO::FileCopyJob::result, this, &KdenliveSettingsDialog::downloadModelFinished);
    } else {
        processArchive(url.toLocalFile());
    }
}

void KdenliveSettingsDialog::removeDictionary()
{
    if (!KdenliveSettings::vosk_folder_path().isEmpty()) {
        doShowSpeechMessage(i18n("We do not allow deleting custom folder models, please do it manually."), KMessageWidget::Warning);
        return;
    }
    QString modelDirectory = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir dir(modelDirectory);
    if (!dir.cd(QStringLiteral("speechmodels"))) {
        doShowSpeechMessage(i18n("Cannot access dictionary folder."), KMessageWidget::Warning);
        return;
    }

    if (!m_speechListWidget->currentItem()) {
        return;
    }
    QString currentModel = m_speechListWidget->currentItem()->text();
    if (!currentModel.isEmpty() && dir.cd(currentModel)) {
        if (KMessageBox::questionTwoActions(this, i18n("Delete folder:\n%1", dir.absolutePath()), {}, KStandardGuiItem::del(), KStandardGuiItem::cancel()) ==
            KMessageBox::PrimaryAction) {
            // Make sure we don't accidentally delete a folder that is not ours
            if (dir.absolutePath().contains(QLatin1String("speechmodels"))) {
                dir.removeRecursively();
                slotParseVoskDictionaries();
            }
        }
    }
}

void KdenliveSettingsDialog::downloadModelFinished(KJob *job)
{
    qDebug() << "=== DOWNLOAD FINISHED!!";
    if (job->error() == 0 || job->error() == 112) {
        qDebug() << "=== NO ERROR ON DWNLD!!";
        auto *jb = static_cast<KIO::FileCopyJob *>(job);
        if (jb) {
            qDebug() << "=== JOB FOUND!!";
            QString archiveFile = jb->destUrl().toLocalFile();
            processArchive(archiveFile);
        } else {
            qDebug() << "=== JOB NOT FOUND!!";
            m_configSpeech.speech_info->setMessageType(KMessageWidget::Warning);
            m_configSpeech.speech_info->setText(i18n("Download error"));
        }
    } else {
        qDebug() << "=== GOT JOB ERROR: " << job->error();
        m_configSpeech.speech_info->setMessageType(KMessageWidget::Warning);
        m_configSpeech.speech_info->setText(i18n("Download error %1", job->errorString()));
    }
}

void KdenliveSettingsDialog::processArchive(const QString &archiveFile)
{
    QMimeDatabase db;
    QMimeType type = db.mimeTypeForFile(archiveFile);
    std::unique_ptr<KArchive> archive;
    if (type.inherits(QStringLiteral("application/zip"))) {
        archive = std::make_unique<KZip>(archiveFile);
    } else {
        archive = std::make_unique<KTar>(archiveFile);
    }
    QString modelDirectory = KdenliveSettings::vosk_folder_path();
    QDir dir;
    if (modelDirectory.isEmpty()) {
        modelDirectory = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
        dir = QDir(modelDirectory);
        dir.mkdir(QStringLiteral("speechmodels"));
        if (!dir.cd(QStringLiteral("speechmodels"))) {
            doShowSpeechMessage(i18n("Cannot access dictionary folder."), KMessageWidget::Warning);
            return;
        }
    } else {
        dir = QDir(modelDirectory);
    }
    if (archive->open(QIODevice::ReadOnly)) {
        doShowSpeechMessage(i18n("Extracting archive…"), KMessageWidget::Information);
        const KArchiveDirectory *archiveDir = archive->directory();
        if (!archiveDir->copyTo(dir.absolutePath())) {
            qDebug() << "=== Error extracting archive!!";
        } else {
            QFile::remove(archiveFile);
            Q_EMIT parseDictionaries();
            doShowSpeechMessage(i18n("New dictionary installed."), KMessageWidget::Positive);
        }
    } else {
        // Test if it is a folder
        QDir testDir(archiveFile);
        if (testDir.exists()) {
        }
        qDebug() << "=== CANNOT OPEN ARCHIVE!!";
    }
}

void KdenliveSettingsDialog::slotParseVoskDictionaries()
{
    m_speechListWidget->clear();
    QStringList final = m_stt->parseVoskDictionaries();
    m_speechListWidget->addItems(final);
    QString voskModelFolder;
    if (!KdenliveSettings::vosk_folder_path().isEmpty()) {
        m_configSpeech.custom_vosk_folder->setChecked(true);
        m_configSpeech.vosk_folder->setUrl(QUrl::fromLocalFile(KdenliveSettings::vosk_folder_path()));
        voskModelFolder = KdenliveSettings::vosk_folder_path();
    } else {
        voskModelFolder = QStandardPaths::locate(QStandardPaths::AppDataLocation, QStringLiteral("speechmodels"), QStandardPaths::LocateDirectory);
    }
    if (!final.isEmpty() && m_stt->missingDependencies().isEmpty()) {
        m_configSpeech.speech_info->animatedHide();
    } else if (final.isEmpty()) {
        doShowSpeechMessage(i18n("Please add a speech model."), KMessageWidget::Information);
    }
    if (!voskModelFolder.isEmpty()) {
        m_configSpeech.modelV_folder_label->setText(QStringLiteral("<a href=\"%1\">%2</a>").arg(voskModelFolder, i18n("Models folder")));
        m_configSpeech.modelV_folder_label->setVisible(true);
        KIO::DirectorySizeJob *job = KIO::directorySize(QUrl::fromLocalFile(voskModelFolder));
        connect(job, &KJob::result, this, [job, label = m_configSpeech.modelV_size]() {
            label->setText(KIO::convertSize(job->totalSize()));
            job->deleteLater();
        });
    } else {
        m_configSpeech.modelV_folder_label->setVisible(false);
    }
}

void KdenliveSettingsDialog::showHelp()
{
    pCore->window()->appHelpActivated();
}

void KdenliveSettingsDialog::checkSpeechDependencies()
{
    m_sttWhisper->checkDependenciesConcurrently();
    m_stt->checkDependenciesConcurrently();
}
