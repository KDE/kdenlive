/*
    SPDX-FileCopyrightText: 2021 Jean-Baptiste Mardelle <jb@kdenlive.org>

SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "kdenlivesettingsdialog.h"
#include "core.h"
#include "dialogs/customcamcorderdialog.h"
#include "dialogs/profilesdialog.h"
#include "dialogs/wizard.h"
#include "doc/kdenlivedoc.h"
#include "filefilter.h"
#include "kdenlivesettings.h"
#include "mainwindow.h"
#include "monitor/monitor.h"
#include "monitor/monitorproxy.h"
#include "pluginssettings.h"
#include "profiles/profilemodel.hpp"
#include "profiles/profilerepository.hpp"
#include "project/dialogs/guidecategories.h"
#include "project/dialogs/profilewidget.h"
#include "project/projectmanager.h"
#include "timeline2/view/timelinecontroller.h"
#include "timeline2/view/timelinewidget.h"

#include "KLocalizedString"
#include "kdenlive_debug.h"
#include <KIO/DesktopExecParser>
#include <KIO/FileCopyJob>
#include <KIO/JobUiDelegateFactory>
#include <KIO/OpenFileManagerWindowJob>
#include <KIO/OpenUrlJob>
#include <KLineEdit>
#include <KMessageBox>
#include <KOpenWithDialog>
#include <KService>
#include <KUrlRequesterDialog>
#include <kddockwidgets/Config.h>

#include <QAction>
#include <QAudioDevice>
#include <QButtonGroup>
#include <QDir>
#include <QGuiApplication>
#include <QInputDialog>
#include <QMediaDevices>
#include <QRegularExpression>
#include <QScreen>
#include <QSize>
#include <QThread>
#include <QTimer>
#include <QtConcurrent/QtConcurrentRun>

#include <cstdio>
#include <cstdlib>
#include <fcntl.h>

#ifdef USE_JOGSHUTTLE
#include "jogshuttle/jogshuttleconfig.h"
#include <QStandardPaths>
#include <linux/input.h>
#include <memory>
#endif

KdenliveSettingsDialog::KdenliveSettingsDialog(QMap<QString, QString> mappable_actions, bool gpuAllowed, QWidget *parent)
    : KConfigDialog(parent, QStringLiteral("settings"), KdenliveSettings::self())
    , m_modified(false)
    , m_shuttleModified(false)
    , m_voskUpdated(false)
    , m_mappable_actions(std::move(mappable_actions))
{
    setWindowModality(Qt::ApplicationModal);

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
    m_pageColors = addPage(p11, i18n("Colors and Markers"), QStringLiteral("color-management"));
    m_guidesCategories = new GuideCategories(nullptr, this);
    QVBoxLayout *guidesLayout = new QVBoxLayout(m_configColors.guides_box);
    guidesLayout->addWidget(m_guidesCategories);

    m_pluginsPage = new PluginsSettings(this);
    connect(m_pluginsPage, &PluginsSettings::openBrowserUrl, this, &KdenliveSettingsDialog::openBrowserUrl);
    m_pageSpeech = addPage(m_pluginsPage, i18n("Plugins"), QStringLiteral("plugins"));

    QWidget *p7 = new QWidget;
    m_configSdl.setupUi(p7);
    m_pagePlay = addPage(p7, i18n("Playback"), QStringLiteral("media-playback-start"));

    QWidget *p5 = new QWidget;
    m_configCapture.setupUi(p5);
    m_decklinkProfiles = new EncodingProfilesChooser(this, EncodingProfilesManager::DecklinkCapture, false, QStringLiteral("decklink_profile"));
    m_configCapture.decklink_profile_box->addWidget(m_decklinkProfiles);
    m_grabProfiles = new EncodingProfilesChooser(this, EncodingProfilesManager::ScreenCapture, false, QStringLiteral("grab_profile"));
    m_configCapture.screen_grab_profile_box->addWidget(m_grabProfiles);
    m_pageCapture = addPage(p5, i18n("Capture"), QStringLiteral("media-record"));

    initDevices();
    initCapturePage();
    initJogShuttlePage();
    initTranscodePage();

    initSdlPage(gpuAllowed);

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

    m_configCapture.defaultaudiocapture->addItems(audioDevices);
    QString selectedDevice = KdenliveSettings::defaultaudiocapture();
    int selectedIndex = m_configCapture.defaultaudiocapture->findText(selectedDevice);
    if (!selectedDevice.isEmpty() && selectedIndex > -1) {
        m_configCapture.defaultaudiocapture->setCurrentIndex(selectedIndex);
    }
    connect(m_configCapture.defaultaudiocapture, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this, [&]() {
        updateRecordProperties();
        updateButtons();
    });
    return true;
}

void KdenliveSettingsDialog::initMiscPage()
{
    QWidget *p1 = new QWidget;
    m_configMisc.setupUi(p1);
    m_pageMisc = addPage(p1, i18n("Misc"), QStringLiteral("configure"));

    m_configMisc.kcfg_use_exiftool->setEnabled(!QStandardPaths::findExecutable(QStringLiteral("exiftool")).isEmpty());

    static const QRegularExpression reg(R"((\+|-)?\d{2}:\d{2}:\d{2}(:||,)\d{2,3})");
    QValidator *validator = new QRegularExpressionValidator(reg, this);
    const QString inputMask = pCore->timecode().mask();
    QSignalBlocker bk1(m_configMisc.kcfg_color_duration);
    QSignalBlocker bk2(m_configMisc.kcfg_title_duration);
    QSignalBlocker bk3(m_configMisc.kcfg_transition_duration);
    QSignalBlocker bk4(m_configMisc.kcfg_mix_duration);
    QSignalBlocker bk5(m_configMisc.kcfg_image_duration);
    QSignalBlocker bk6(m_configMisc.kcfg_sequence_duration);
    QSignalBlocker bk7(m_configMisc.kcfg_fade_duration);
    QSignalBlocker bk8(m_configMisc.kcfg_subtitle_duration);
    m_configMisc.kcfg_color_duration->setInputMask(inputMask);
    m_configMisc.kcfg_color_duration->setValidator(validator);
    m_configMisc.kcfg_title_duration->setInputMask(inputMask);
    m_configMisc.kcfg_title_duration->setValidator(validator);
    m_configMisc.kcfg_transition_duration->setInputMask(inputMask);
    m_configMisc.kcfg_transition_duration->setValidator(validator);
    m_configMisc.kcfg_mix_duration->setInputMask(inputMask);
    m_configMisc.kcfg_mix_duration->setValidator(validator);
    m_configMisc.kcfg_image_duration->setInputMask(inputMask);
    m_configMisc.kcfg_image_duration->setValidator(validator);
    m_configMisc.kcfg_sequence_duration->setInputMask(inputMask);
    m_configMisc.kcfg_sequence_duration->setValidator(validator);
    m_configMisc.kcfg_fade_duration->setInputMask(inputMask);
    m_configMisc.kcfg_fade_duration->setValidator(validator);
    m_configMisc.kcfg_subtitle_duration->setInputMask(inputMask);
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
    m_configMisc.kcfg_enableBuiltInEffects->setWhatsThis(xi18nc(
        "@info:whatsthis", "When enabled, this will add basic effects (Flip, Transform, Volume) to all clips for convenience. These effects are disabled "
                           "and will only be applied if you change a parameter."));
    connect(m_configMisc.preferredcomposite, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this, [&]() {
        if (m_configMisc.preferredcomposite->currentText() != KdenliveSettings::preferredcomposite()) {
            KdenliveSettings::setPreferredcomposite(m_configMisc.preferredcomposite->currentText());
            int mode = pCore->currentDoc()->getSequenceProperty(pCore->window()->getCurrentTimeline()->getUuid(), QStringLiteral("compositing")).toInt();
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
    QString currentPreviewData = KdenliveSettings::previewparams().isEmpty()
                                     ? QString()
                                     : QStringLiteral("%1;%2").arg(KdenliveSettings::previewparams(), KdenliveSettings::previewextension());
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
    m_alphaProxyProfiles = new EncodingProfilesChooser(p10, EncodingProfilesManager::ProxyAlphaClips, true, QStringLiteral("alpha_profile"));
    m_configProxy.alpha_profile_box->addWidget(m_alphaProxyProfiles);
    addPage(p10, i18n("Proxy Clips"), QStringLiteral("transform-scale"));
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
    m_configEnv.videofolderurl->setEnabled(KdenliveSettings::videotodefaultfolder() == KdenliveDoc::SaveToCustomFolder);
    m_configEnv.videofolderurl->setPlaceholderText(QStandardPaths::writableLocation(QStandardPaths::MoviesLocation));
    m_configEnv.kcfg_videotodefaultfolder->setItemText(KdenliveDoc::SaveToVideoFolder,
                                                       i18n("Use default folder: %1", QStandardPaths::writableLocation(QStandardPaths::MoviesLocation)));
    if (KdenliveSettings::customprojectfolder()) {
        m_configEnv.kcfg_videotodefaultfolder->setItemText(KdenliveDoc::SaveToProjectFolder,
                                                           i18n("Always use project folder: %1", KdenliveSettings::defaultprojectfolder()));
    } else {
        m_configEnv.kcfg_videotodefaultfolder->setItemText(KdenliveDoc::SaveToProjectFolder, i18n("Always use active project folder"));
    }
    connect(m_configEnv.kcfg_videotodefaultfolder, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &KdenliveSettingsDialog::slotEnableVideoFolder);

    // Window drag behavior on non KDE desktops
    if (qgetenv("XDG_CURRENT_DESKTOP") == QLatin1String("KDE")) {
        m_configEnv.dragFromTitlebarOnly->setVisible(false);
    } else {
        auto cfg = KSharedConfig::openConfig(QStringLiteral("breezerc"), KConfig::SimpleConfig, QStandardPaths::GenericConfigLocation);
        KConfigGroup group(cfg, "Style");
        if (group.readEntry(QStringLiteral("WindowDragMode")) == QLatin1String("WD_NONE")) {
            m_configEnv.dragFromTitlebarOnly->setChecked(true);
            m_breezeDragFromTitlebarOnly = true;
        }
        KSharedConfigPtr config = KSharedConfig::openConfig();
        KConfigGroup settingsGroup(config, "KDE");
        const QString currentStyle = settingsGroup.readEntry(QStringLiteral("widgetStyle")).toLower();
        if (!currentStyle.isEmpty() && currentStyle != QLatin1String("breeze")) {
            m_configEnv.dragFromTitlebarOnly->setEnabled(false);
        }
    }

    // Mime types
    QStringList mimes = FileFilter::getExtensions();
    std::sort(mimes.begin(), mimes.end());
    m_configEnv.supportedmimes->setPlainText(mimes.join(QLatin1Char(' ')));

    connect(m_configEnv.kp_image, &QAbstractButton::clicked, this, &KdenliveSettingsDialog::slotEditImageApplication);
    connect(m_configEnv.kp_audio, &QAbstractButton::clicked, this, &KdenliveSettingsDialog::slotEditAudioApplication);
    connect(m_configEnv.kp_anim, &QAbstractButton::clicked, this, &KdenliveSettingsDialog::slotEditGlaxnimateApplication);
    connect(m_configEnv.kp_video, &QAbstractButton::clicked, this, &KdenliveSettingsDialog::slotEditVideoApplication);

    m_pageEnv = addPage(p2, i18n("Environment"), QStringLiteral("application-x-executable-script"));
#if defined Q_OS_MAC
    // Power management not implemented on Mac
    m_configEnv.kcfg_usePowerManagement->setEnabled(false);
#endif
}

void KdenliveSettingsDialog::slotUpdateBreezeDrag(bool dragOnTitleBar)
{
    auto cfg = KSharedConfig::openConfig(QStringLiteral("breezerc"), KConfig::SimpleConfig, QStandardPaths::GenericConfigLocation);
    KConfigGroup group(cfg, "Style");
    if (!dragOnTitleBar) {
        // Revert to default behavior
        group.deleteEntry(QStringLiteral("WindowDragMode"));
    } else {
        // Drag through title bar only
        group.writeEntry(QStringLiteral("WindowDragMode"), QStringLiteral("WD_NONE"));
    }
    cfg->sync();
    // Ensure Breeze style directly updates its configuration
    QEvent event(QEvent::ApplicationPaletteChange);
    qApp->sendEvent(qApp, &event);
}

void KdenliveSettingsDialog::initCapturePage()
{
    m_configCapture.label->setVisible(false);
    m_configCapture.kcfg_defaultcapture->setVisible(false);
    m_configCapture.tabWidget->setCurrentIndex(KdenliveSettings::defaultcapture());
#ifdef Q_WS_MAC
    m_configCapture.tabWidget->setEnabled(false);
    m_configCapture.kcfg_defaultcapture->setEnabled(false);
    m_configCapture.label->setText(i18n("Capture is not yet available on Mac OS X."));
#endif

    connect(m_configCapture.kcfg_grab_capture_type, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this,
            &KdenliveSettingsDialog::slotUpdateGrabRegionStatus);

    slotUpdateGrabRegionStatus();

    // Init audio capture device
    initAudioRecDevice();

    // Fill channels and sample rate with supported values
    updateRecordProperties();

    // Ensure changes to channels / sample rate trigger a changed signal
    connect(m_configCapture.audiocapturechannels, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this, [&]() { updateButtons(); });
    connect(m_configCapture.audiocapturesamplerate, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this, [&]() { updateButtons(); });
    connect(m_configCapture.audiocapturesampleformat, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this, [&]() { updateButtons(); });

    m_configCapture.labelNoAudioDevices->setVisible(false);

    connect(m_configCapture.audiocapturedefaults, &QPushButton::clicked, this, &KdenliveSettingsDialog::useRecordDefault);

    getBlackMagicDeviceList(m_configCapture.kcfg_decklink_capturedevice);
}

void KdenliveSettingsDialog::useRecordDefault()
{
    const QString device = m_configCapture.defaultaudiocapture->currentText();
    QAudioDevice deviceInfo = QMediaDevices::defaultAudioInput();
    const auto deviceInfos = QMediaDevices::audioInputs();
    for (const QAudioDevice &devInfo : deviceInfos) {
        qDebug() << "Found device : " << devInfo.description();
        if (devInfo.description() == device) {
            deviceInfo = devInfo;
            qDebug() << "Monitoring using device : " << devInfo.description();
            break;
        }
    }
    if (deviceInfo.isNull()) {
        return;
    }
    QAudioFormat defaultFormat = deviceInfo.preferredFormat();
    int ix = m_configCapture.audiocapturechannels->findData(defaultFormat.channelCount());
    if (ix > -1) {
        m_configCapture.audiocapturechannels->setCurrentIndex(ix);
    }
    ix = m_configCapture.audiocapturesamplerate->findData(defaultFormat.sampleRate());
    if (ix > -1) {
        m_configCapture.audiocapturesamplerate->setCurrentIndex(ix);
    }
    ix = m_configCapture.audiocapturesampleformat->findData(defaultFormat.sampleFormat());
    if (ix > -1) {
        m_configCapture.audiocapturesampleformat->setCurrentIndex(ix);
    }
}

void KdenliveSettingsDialog::updateRecordProperties()
{
    // Find active device
    const QString device = m_configCapture.defaultaudiocapture->currentText();
    QAudioDevice deviceInfo = QMediaDevices::defaultAudioInput();
    const auto deviceInfos = QMediaDevices::audioInputs();
    for (const QAudioDevice &devInfo : deviceInfos) {
        qDebug() << "Found device : " << devInfo.description();
        if (devInfo.description() == device) {
            deviceInfo = devInfo;
            qDebug() << "Monitoring using device : " << devInfo.description();
            break;
        }
    }
    if (deviceInfo.isNull()) {
        m_configCapture.audiocapturechannels->setEnabled(false);
        m_configCapture.audiocapturesamplerate->setEnabled(false);
        m_configCapture.audiocapturesampleformat->setEnabled(false);
        m_configCapture.audiocapturedefaults->setEnabled(false);
    }
    m_configCapture.audiocapturechannels->setEnabled(true);
    m_configCapture.audiocapturesamplerate->setEnabled(true);
    m_configCapture.audiocapturesampleformat->setEnabled(true);
    m_configCapture.audiocapturedefaults->setEnabled(true);

    // Supported audio channels
    m_configCapture.audiocapturechannels->clear();
    int min = deviceInfo.minimumChannelCount();
    // Max channel count seems to frequently go up to 32, which is probably incorrect, limit to 6
    int max = qMin(6, deviceInfo.maximumChannelCount());
    if (min < 2) {
        m_configCapture.audiocapturechannels->addItem(i18n("Mono (1 channel)"), 1);
    }
    if (max > 1) {
        m_configCapture.audiocapturechannels->addItem(i18n("Stereo (2 channels)"), 2);
    }
    for (int i = 3; i <= max; i++) {
        m_configCapture.audiocapturechannels->addItem(i18n("%1 channels", i), i);
    }
    int channelsIndex = m_configCapture.audiocapturechannels->findData(KdenliveSettings::audiocapturechannels());
    m_configCapture.audiocapturechannels->setCurrentIndex(qMax(channelsIndex, 0));

    // Default audio capture sample rate
    m_configCapture.audiocapturesamplerate->clear();
    min = deviceInfo.minimumSampleRate();
    max = deviceInfo.maximumSampleRate();

    if (max < 44100) {
        m_configCapture.audiocapturesamplerate->addItem(i18n("%1 Hz", max), max);
    } else {
        if (min <= 44100) {
            m_configCapture.audiocapturesamplerate->addItem(i18n("44100 Hz"), 44100);
        }
        if (max >= 48000) {
            m_configCapture.audiocapturesamplerate->addItem(i18n("48000 Hz"), 48000);
        }
        if (max >= 96000) {
            m_configCapture.audiocapturesamplerate->addItem(i18n("96000 Hz"), 96000);
        }
    }

    int sampleRateIndex = m_configCapture.audiocapturesamplerate->findData(KdenliveSettings::audiocapturesamplerate());
    m_configCapture.audiocapturesamplerate->setCurrentIndex(qMax(sampleRateIndex, 0));

    // Default audio capture sample formats
    m_configCapture.audiocapturesampleformat->clear();
    QList<QAudioFormat::SampleFormat> sampleFormats = deviceInfo.supportedSampleFormats();
    for (auto &fm : sampleFormats) {
        switch (fm) {
        case QAudioFormat::UInt8:
            m_configCapture.audiocapturesampleformat->addItem(i18n("Unsigned 8 bit"), QAudioFormat::UInt8);
            break;
        case QAudioFormat::Int16:
            m_configCapture.audiocapturesampleformat->addItem(i18n("Signed 16 bit"), QAudioFormat::Int16);
            break;
        case QAudioFormat::Int32:
            m_configCapture.audiocapturesampleformat->addItem(i18n("Signed 32 bit"), QAudioFormat::Int32);
            break;
        case QAudioFormat::Float:
            m_configCapture.audiocapturesampleformat->addItem(i18n("Float"), QAudioFormat::Float);
            break;
        default:
            break;
        }
    }
    int sampleFormatIndex = m_configCapture.audiocapturesampleformat->findData(KdenliveSettings::audiocapturesampleformat());
    m_configCapture.audiocapturesampleformat->setCurrentIndex(qMax(sampleFormatIndex, 0));
}

void KdenliveSettingsDialog::initJogShuttlePage()
{
    QWidget *p6 = new QWidget;
    m_configShuttle.setupUi(p6);
#ifdef USE_JOGSHUTTLE
#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
    connect(m_configShuttle.kcfg_enableshuttle, &QCheckBox::checkStateChanged, this, &KdenliveSettingsDialog::slotCheckShuttle);
#else
    connect(m_configShuttle.kcfg_enableshuttle, &QCheckBox::stateChanged, this, &KdenliveSettingsDialog::slotCheckShuttle);
#endif
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
#if defined(Q_OS_WIN)
    m_configShuttle.shuttledisabled->setText(i18n("For device configuration see <a "
                                                  "href=\"https://docs.kdenlive.org/getting_started/configure_kdenlive/configuration_jogshuttle.html"
                                                  "#windows?mtm_campaign=kdenlive_inapp&mtm_kwd=jogshuttle_settings\">our "
                                                  "documentation</a>."));
    connect(m_configShuttle.shuttledisabled, &QLabel::linkActivated, this, &KdenliveSettingsDialog::openBrowserUrl);
#endif
}

void KdenliveSettingsDialog::openBrowserUrl(const QString &url)
{
    qDebug() << "=== LINK CLICKED: " << url;
    auto *job = new KIO::OpenUrlJob(QUrl(url));
    job->setUiDelegate(KIO::createDefaultJobUiDelegate(KJobUiDelegate::AutoHandlingEnabled, this));
    // methods like setRunExecutables, setSuggestedFilename, setEnableExternalBrowser, setFollowRedirections
    // exist in both classes
    job->start();
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
#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
    connect(m_configTranscode.profile_audioonly, &QCheckBox::checkStateChanged, this, &KdenliveSettingsDialog::slotEnableTranscodeUpdate);
#else
    connect(m_configTranscode.profile_audioonly, &QCheckBox::stateChanged, this, &KdenliveSettingsDialog::slotEnableTranscodeUpdate);
#endif

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

    std::sort(action_names.begin(), action_names.end());

    // Here we need to compute the action_id -> index-in-action_names. We iterate over the
    // action_names, as the sorting may depend on the user-language.
    QStringList actions_map = JogShuttleConfig::actionMap(KdenliveSettings::shuttlebuttons());
    QMap<QString, int> action_pos;
    for (const QString &action_id : std::as_const(actions_map)) {
        // This loop find out at what index is the string that would map to the action_id.
        for (int i = 0; i < action_names.size(); ++i) {
            if (mappable_actions[action_names.at(i)] == action_id) {
                action_pos[action_id] = i;
                break;
            }
        }
    }

    int i = 0;
    for (QComboBox *button : std::as_const(m_shuttle_buttons)) {
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
    m_configEnv.videofolderurl->setEnabled(ix == KdenliveDoc::SaveToCustomFolder);
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
                line = stream.readLine();
            }
            file.close();
        } else {
            qCDebug(KDENLIVE_LOG) << " / / / /CANNOT READ PCM";
        }
    }

    if (!KdenliveSettings::audiodevicename().isEmpty()) {
        // Select correct alsa device
        int ix = m_configSdl.kcfg_audio_device->findData(KdenliveSettings::audiodevicename());
        m_configSdl.kcfg_audio_device->setCurrentIndex(ix);
        KdenliveSettings::setAudio_device(ix);
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
}

void KdenliveSettingsDialog::fillMonitorData()
{
    QSignalBlocker bk(m_configSdl.fullscreen_monitor);
    m_configSdl.fullscreen_monitor->clear();
    m_configSdl.fullscreen_monitor->addItem(i18n("auto"));
    int ix = 0;
    const QList<QScreen *> screens = qApp->screens();
    for (const QScreen *screen : screens) {
        QString screenName = screen->name().isEmpty() ? i18n("Monitor %1", ix + 1) : QStringLiteral("%1: %2").arg(ix + 1).arg(screen->name());
        if (!screen->model().isEmpty()) {
            screenName.append(QStringLiteral(" - %1").arg(screen->model()));
        }
        if (!screen->manufacturer().isEmpty()) {
            screenName.append(QStringLiteral(" (%1)").arg(screen->manufacturer()));
        }
        m_configSdl.fullscreen_monitor->addItem(screenName, QStringLiteral("%1:%2").arg(QString::number(ix), screen->serialNumber()));
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
    const QStringList lines = result.split(QLatin1Char('\n'));
    for (const QString &devicestr : lines) {
        if (!devicestr.startsWith(QLatin1Char(' ')) && devicestr.count(QLatin1Char(':')) > 1) {
            QString card = devicestr.section(QLatin1Char(':'), 0, 0).section(QLatin1Char(' '), -1);
            QString device = devicestr.section(QLatin1Char(':'), 1, 1).section(QLatin1Char(' '), -1);
            m_configSdl.kcfg_audio_device->addItem(devicestr.section(QLatin1Char(':'), -1).simplified(), QStringLiteral("plughw:%1,%2").arg(card, device));
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
    case Kdenlive::PageSpeech: {
        if (option == 0) {
            m_pluginsPage->checkSpeechDependencies();
        }
        setCurrentPage(m_pageSpeech);
        m_pluginsPage->setActiveTab(option);
        break;
    }
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

void KdenliveSettingsDialog::slotEditVideoApplication()
{
    QUrl url = KUrlRequesterDialog::getUrl(QUrl::fromLocalFile(KdenliveSettings::defaultvideoapp()), this, i18n("Enter path to the video editing application"));
    if (!url.isEmpty()) {
        m_configEnv.kcfg_defaultvideoapp->setText(url.toLocalFile());
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
    for (const QString &action_id : std::as_const(actions_map)) {
        // This loop find out at what index is the string that would map to the action_id.
        for (int i = 0; i < action_names.size(); ++i) {
            if (m_mappable_actions[action_names[i]] == action_id) {
                action_pos[action_id] = i;
                break;
            }
        }
    }
    int i = 0;
    for (QComboBox *button : std::as_const(m_shuttle_buttons)) {
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
    Kdenlive::ConfigPage page = Kdenlive::NoPage;
    auto *current = currentPage();
    if (current == m_pageMisc) {
        page = Kdenlive::PageMisc;
    } else if (current == m_pageEnv) {
        page = Kdenlive::PageEnv;
    } else if (current == m_pageTimeline) {
        page = Kdenlive::PageTimeline;
    } else if (current == m_pageTools) {
        page = Kdenlive::PageTools;
    } else if (current == m_pageCapture) {
        page = Kdenlive::PageMisc;
    } else if (current == m_pageCapture) {
        page = Kdenlive::PageCapture;
    } else if (current == m_pageJog) {
        page = Kdenlive::PageJogShuttle;
    } else if (current == m_pagePlay) {
        page = Kdenlive::PagePlayback;
    } else if (current == m_pageTranscode) {
        page = Kdenlive::PageTranscode;
    } else if (current == m_pageProject) {
        page = Kdenlive::PageProjectDefaults;
    } else if (current == m_pageColors) {
        page = Kdenlive::PageColorsGuides;
    } else if (current == m_pageSpeech) {
        page = Kdenlive::PageSpeech;
    }

    pCore->window()->m_lastConfigPage = page;
    KConfigDialog::accept();
}

void KdenliveSettingsDialog::updateExternalApps()
{
    m_configEnv.kcfg_defaultimageapp->setText(KdenliveSettings::defaultimageapp());
    m_configEnv.kcfg_defaultaudioapp->setText(KdenliveSettings::defaultaudioapp());
    m_configEnv.kcfg_glaxnimatePath->setText(KdenliveSettings::glaxnimatePath());
    m_configEnv.kcfg_defaultvideoapp->setText(KdenliveSettings::defaultvideoapp());
}

void KdenliveSettingsDialog::updateSettings()
{
    // Save changes to settings (for example when user pressed "Apply" or "Ok")
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
            m_configEnv.kcfg_videotodefaultfolder->setItemText(KdenliveDoc::SaveToProjectFolder,
                                                               i18n("Always use project folder: %1", KdenliveSettings::defaultprojectfolder()));
            m_configEnv.kcfg_capturetoprojectfolder->setItemText(1, i18n("Always use project folder: %1", KdenliveSettings::defaultprojectfolder()));
        }
    }

    if (m_configMisc.kcfg_enableBuiltInEffects->isChecked() != KdenliveSettings::enableBuiltInEffects()) {
        KdenliveSettings::setEnableBuiltInEffects(m_configMisc.kcfg_enableBuiltInEffects->isChecked());
        pCore->window()->reloadAssetPanel();
    }

    if (m_configProject.kcfg_customprojectfolder->isChecked() != KdenliveSettings::customprojectfolder()) {
        if (KdenliveSettings::customprojectfolder()) {
            m_configEnv.kcfg_videotodefaultfolder->setItemText(KdenliveDoc::SaveToProjectFolder, i18n("Always use active project folder"));
            m_configEnv.kcfg_capturetoprojectfolder->setItemText(1, i18n("Always use active project folder"));
        } else {
            m_configEnv.kcfg_videotodefaultfolder->setItemText(KdenliveDoc::SaveToProjectFolder,
                                                               i18n("Always use project folder: %1", KdenliveSettings::defaultprojectfolder()));
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

    // Check if screengrab is fullscreen
    if (m_configCapture.kcfg_grab_capture_type->currentIndex() != KdenliveSettings::grab_capture_type()) {
        KdenliveSettings::setGrab_capture_type(m_configCapture.kcfg_grab_capture_type->currentIndex());
    }

    bool audioRecDeviceChanged = false;
    if (m_configCapture.defaultaudiocapture->currentText() != KdenliveSettings::defaultaudiocapture()) {
        KdenliveSettings::setDefaultaudiocapture(m_configCapture.defaultaudiocapture->currentText());
        audioRecDeviceChanged = true;
    }

    // Check audio capture changes
    if (KdenliveSettings::audiocapturechannels() != m_configCapture.audiocapturechannels->currentData().toInt() ||
        KdenliveSettings::audiocapturevolume() != m_configCapture.kcfg_audiocapturevolume->value() ||
        KdenliveSettings::audiocapturesamplerate() != m_configCapture.audiocapturesamplerate->currentData().toInt() ||
        KdenliveSettings::audiocapturesampleformat() != m_configCapture.audiocapturesampleformat->currentData().toInt()) {
        KdenliveSettings::setAudiocapturechannels(m_configCapture.audiocapturechannels->currentData().toInt());
        KdenliveSettings::setAudiocapturevolume(m_configCapture.kcfg_audiocapturevolume->value());
        KdenliveSettings::setAudiocapturesamplerate(m_configCapture.audiocapturesamplerate->currentData().toInt());
        KdenliveSettings::setAudiocapturesampleformat(m_configCapture.audiocapturesampleformat->currentData().toInt());
        audioRecDeviceChanged = true;
    }

    if (audioRecDeviceChanged) {
        pCore->resetAudioMonitoring();
    }

    // screengrab
    QString string = m_grabProfiles->currentParams();
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

    // proxies with alpha
    string = m_alphaProxyProfiles->currentParams();
    if (string != KdenliveSettings::proxyalphaparams()) {
        KdenliveSettings::setProxyalphaparams(string);
    }
    string = m_alphaProxyProfiles->currentExtension();
    if (string != KdenliveSettings::proxyalphaextension()) {
        KdenliveSettings::setProxyalphaextension(string);
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

    // Breeze drag
    if (m_configEnv.dragFromTitlebarOnly->isChecked() != m_breezeDragFromTitlebarOnly) {
        slotUpdateBreezeDrag(m_configEnv.dragFromTitlebarOnly->isChecked());
    }

    // Autosave
    if (m_configMisc.kcfg_autosave_time->value() != KdenliveSettings::autosave_time()) {
        KdenliveSettings::setAutosave_time(m_configMisc.kcfg_autosave_time->value());
        pCore->projectManager()->updateAutoSaveTimer();
    }

    if (updateLibrary) {
        Q_EMIT updateLibraryFolder();
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

    QString value = m_configSdl.kcfg_audio_driver->currentData().toString();
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

    if (m_configColors.kcfg_monitorGridH->value() != KdenliveSettings::monitorGridH()) {
        KdenliveSettings::setMonitorGridH(m_configColors.kcfg_monitorGridH->value());
    }
    if (m_configColors.kcfg_monitorGridV->value() != KdenliveSettings::monitorGridV()) {
        KdenliveSettings::setMonitorGridV(m_configColors.kcfg_monitorGridV->value());
    }

    if (m_configColors.kcfg_window_background->color() != KdenliveSettings::window_background()) {
        KdenliveSettings::setWindow_background(m_configColors.kcfg_window_background->color());
    }

    if (m_configColors.kcfg_thumbColor1->color() != KdenliveSettings::thumbColor1() ||
        m_configColors.kcfg_thumbColor2->color() != KdenliveSettings::thumbColor2()) {
        KdenliveSettings::setThumbColor1(m_configColors.kcfg_thumbColor1->color());
        KdenliveSettings::setThumbColor2(m_configColors.kcfg_thumbColor2->color());
    }

    if (m_configColors.kcfg_overlayColor->color() != KdenliveSettings::overlayColor()) {
        KdenliveSettings::setOverlayColor(m_configColors.kcfg_overlayColor->color());
    }

    if (m_configMisc.kcfg_tabposition->currentIndex() != KdenliveSettings::tabposition()) {
        KdenliveSettings::setTabposition(m_configMisc.kcfg_tabposition->currentIndex());
        // Reload layout to make it happen
        KDDockWidgets::LayoutSaver dockLayout(KDDockWidgets::RestoreOption_AbsoluteFloatingDockWindows);
        const QByteArray layoutData = dockLayout.serializeLayout();
        if (KdenliveSettings::tabposition() == 1) {
            KDDockWidgets::Config::self().setTabsAtBottom(true);
        } else {
            KDDockWidgets::Config::self().setTabsAtBottom(false);
        }
        dockLayout.restoreLayout(layoutData);
    }

    if (m_configTimeline.kcfg_displayallchannels->isChecked() != KdenliveSettings::displayallchannels()) {
        KdenliveSettings::setDisplayallchannels(m_configTimeline.kcfg_displayallchannels->isChecked());
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
    for (QComboBox *button : std::as_const(m_shuttle_buttons)) {
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

    if (m_configTimeline.kcfg_centeredplayhead->isChecked() != KdenliveSettings::centeredplayhead()) {
        KdenliveSettings::setCenteredplayhead(m_configTimeline.kcfg_centeredplayhead->isChecked());
        Q_EMIT pCore->centeredPlayheadChanged();
    }

    if (m_configTimeline.kcfg_pauseonseek->isChecked() != KdenliveSettings::pauseonseek()) {
        KdenliveSettings::setPauseonseek(m_configTimeline.kcfg_pauseonseek->isChecked());
    }

    if (m_configTimeline.kcfg_seekonaddeffect->isChecked() != KdenliveSettings::seekonaddeffect()) {
        KdenliveSettings::setSeekonaddeffect(m_configTimeline.kcfg_seekonaddeffect->isChecked());
    }

    if (m_configTimeline.kcfg_scrollvertically->isChecked() != KdenliveSettings::scrollvertically()) {
        KdenliveSettings::setScrollvertically(m_configTimeline.kcfg_scrollvertically->isChecked());
    }

    m_pluginsPage->applySettings();

    // Mimes
    if (m_configEnv.kcfg_addedExtensions->text() != KdenliveSettings::addedExtensions()) {
        // Update list
        KdenliveSettings::setAddedExtensions(m_configEnv.kcfg_addedExtensions->text());
        QStringList mimes = FileFilter::getExtensions();
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
    for (QComboBox *button : std::as_const(m_shuttle_buttons)) {
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

    if (KdenliveSettings::defaultaudiocapture() != m_configCapture.defaultaudiocapture->currentText()) {
        return true;
    }
    if (KdenliveSettings::audiocapturechannels() != m_configCapture.audiocapturechannels->currentData().toInt()) {
        return true;
    }
    if (KdenliveSettings::audiocapturesamplerate() != m_configCapture.audiocapturesamplerate->currentData().toInt()) {
        return true;
    }
    if (KdenliveSettings::audiocapturesampleformat() != m_configCapture.audiocapturesampleformat->currentData().toInt()) {
        return true;
    }
    return KConfigDialog::hasChanged();
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

void KdenliveSettingsDialog::showHelp()
{
    pCore->window()->appHelpActivated();
}
