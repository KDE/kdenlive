/*
SPDX-FileCopyrightText: 2014 Till Theato <root@ttill.de>
SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "core.h"
#include "assets/keyframes/model/keyframemodel.hpp"
#include "audiomixer/mixermanager.hpp"
#include "bin/bin.h"
#include "bin/mediabrowser.h"
#include "bin/projectitemmodel.h"
#include "capture/mediacapture.h"
#include "dialogs/proxytest.h"
#include "dialogs/subtitleedit.h"
#include "dialogs/textbasededit.h"
#include "dialogs/timeremap.h"
#include "doc/docundostack.hpp"
#include "doc/kdenlivedoc.h"
#include "kdenlive_debug.h"
#include "kdenlivesettings.h"
#include "library/librarywidget.h"
#include "mainwindow.h"
#include "mltconnection.h"
#include "mltcontroller/clipcontroller.h"
#include "monitor/monitormanager.h"
#include "profiles/profilemodel.hpp"
#include "profiles/profilerepository.hpp"
#include "project/dialogs/guideslist.h"
#include "project/projectmanager.h"
#include "timeline2/model/timelineitemmodel.hpp"
#include "timeline2/view/timelinecontroller.h"
#include "timeline2/view/timelinewidget.h"
#include <mlt++/MltRepository.h>

#include <KIO/OpenFileManagerWindowJob>
#include <KMessageBox>
#include <kddockwidgets/Config.h>
#include <kddockwidgets/core/Draggable_p.h>

#include <QCoreApplication>
#include <QDesktopServices>
#include <QDir>
#include <QImageReader>
#include <QInputDialog>
#include <QQuickStyle>
#include <locale>
#ifdef Q_OS_MAC
#include <xlocale.h>
#endif

static bool m_inhibitHideBarTimer{false};

std::unique_ptr<Core> Core::m_self;
Core::Core(LinuxPackageType packageType)
    : audioThumbCache(QStringLiteral("audioCache"), 2000000)
    , taskManager(this)
    , m_packageType(packageType)
    , m_capture(new MediaCapture(this))
    , sessionId(QUuid::createUuid().toString())
{
    m_hideTimer.setInterval(5000);
    m_hideTimer.setSingleShot(true);
    connect(&m_hideTimer, &QTimer::timeout, this, [&]() { Q_EMIT hideBars(!KdenliveSettings::showtitlebars()); });
}

void Core::startHideBarsTimer()
{
    if (!m_inhibitHideBarTimer) {
        m_hideTimer.start();
    }
}

void Core::updateHideBarsTimer(bool inhibit)
{
    if (inhibit) {
        if (m_hideTimer.isActive()) {
            m_hideTimer.stop();
            m_inhibitHideBarTimer = true;
        }
    } else {
        if (m_inhibitHideBarTimer && !KdenliveSettings::showtitlebars()) {
            m_hideTimer.start();
        }
        m_inhibitHideBarTimer = false;
    }
}

void Core::prepareShutdown()
{
    m_guiConstructed = false;
    // m_mainWindow->getCurrentTimeline()->controller()->prepareClose();
    projectItemModel()->blockSignals(true);
    QThreadPool::globalInstance()->clear();
}

void Core::finishShutdown()
{
    if (m_monitorManager) {
        delete m_monitorManager;
    }
    if (m_projectManager) {
        delete m_projectManager;
    }
    ClipController::mediaUnavailable.reset();
}

Core::~Core() {}

bool Core::build(LinuxPackageType packageType, bool testMode)
{
    if (m_self) {
        return true;
    }
    m_self.reset(new Core(packageType));
    m_self->initLocale();

    qRegisterMetaType<audioShortVector>("audioShortVector");
    qRegisterMetaType<QVector<double>>("QVector<double>");
    qRegisterMetaType<QList<QAction *>>("QList<QAction*>");
    qRegisterMetaType<MessageType>("MessageType");
    qRegisterMetaType<stringMap>("stringMap");
    qRegisterMetaType<sequenceMap>("sequenceMap");
    qRegisterMetaType<audioByteArray>("audioByteArray");
    qRegisterMetaType<QList<ItemInfo>>("QList<ItemInfo>");
    qRegisterMetaType<std::shared_ptr<Mlt::Producer>>("std::shared_ptr<Mlt::Producer>");
    qRegisterMetaType<QVector<int>>();
    qRegisterMetaType<QDomElement>("QDomElement");
    qRegisterMetaType<requestClipInfo>("requestClipInfo");
    qRegisterMetaType<QVector<QPair<QString, QVariant>>>("paramVector");
    qRegisterMetaType<ProfileParam *>("ProfileParam*");
    qRegisterMetaType<ObjectId>("ObjectId");
    KeyframeModel::initKeyframeTypes();

    // Increase memory limit allowed per image
    QImageReader::setAllocationLimit(1024);

    if (!testMode) {
        // Check if we had a crash
        QFile lockFile(QDir::temp().absoluteFilePath(QStringLiteral("kdenlivelock")));
        if (lockFile.exists()) {
            // a previous instance crashed, propose some actions
            if (KdenliveSettings::gpu_accel()) {
                // Propose to disable movit
                if (KMessageBox::questionTwoActions(QApplication::activeWindow(),
                                                    i18n("Kdenlive crashed on last startup.\nDo you want to disable experimental GPU processing (Movit) ?"), {},
                                                    KGuiItem(i18n("Disable GPU processing")), KStandardGuiItem::cont()) == KMessageBox::PrimaryAction) {
                    KdenliveSettings::setGpu_accel(false);
                }
            } else {
                // propose to delete config files
                if (KMessageBox::questionTwoActions(QApplication::activeWindow(),
                                                    i18n("Kdenlive crashed on last startup.\nDo you want to reset the configuration files ?"), {},
                                                    KStandardGuiItem::reset(), KStandardGuiItem::cont()) == KMessageBox::PrimaryAction) {
                    // Release startup crash lock file
                    QFile lockFile(QDir::temp().absoluteFilePath(QStringLiteral("kdenlivelock")));
                    lockFile.remove();
                    return false;
                }
            }
        } else {
            // Create lock file
            lockFile.open(QFile::WriteOnly);
            lockFile.write(QByteArray());
            lockFile.close();
        }
    }

    m_self->m_projectItemModel = ProjectItemModel::construct();
    m_self->m_projectManager = new ProjectManager(m_self.get());
    return true;
}

void Core::initHeadless(const QUrl &url)
{
    MltConnection::construct(QString());
    m_projectManager = new ProjectManager(this);
    QMetaObject::invokeMethod(pCore->projectManager(), "slotLoadHeadless", Qt::QueuedConnection, Q_ARG(QUrl, url));
    connect(this, &Core::displayBinMessage, this,
            [](QString text, int, QList<QAction *>, bool, BinMessage::BinCategory) { qInfo() << QStringLiteral("Bin message: ") << text; });
    connect(this, &Core::displayBinLogMessage, this, [](QString text, int, QString) { qInfo() << QStringLiteral("Bin message: ") << text; });
}

void Core::initGUI(const QString &MltPath, const QUrl &Url, const QString &clipsToLoad)
{
    m_mainWindow = new MainWindow();
    KDDockWidgets::Config::self().setDragAboutToStartFunc([](KDDockWidgets::Core::Draggable *) -> bool {
        if (!KdenliveSettings::showtitlebars()) {
            pCore->updateHideBarsTimer(true);
        }
        return true;
    });

    KDDockWidgets::Config::self().setDragEndedFunc([]() {
        // cleanup
        pCore->updateHideBarsTimer(false);
    });

    // The MLT Factory will be initiated there, all MLT classes will be usable only after this
    bool inSandbox = m_packageType == LinuxPackageType::AppImage || m_packageType == LinuxPackageType::Flatpak || m_packageType == LinuxPackageType::Snap;
    if (inSandbox) {
        // In a sandbox environment we need to search some paths recursively
        QString appPath = qApp->applicationDirPath();
        KdenliveSettings::setFfmpegpath(QDir::cleanPath(appPath + QStringLiteral("/ffmpeg")));
        KdenliveSettings::setFfplaypath(QDir::cleanPath(appPath + QStringLiteral("/ffplay")));
        KdenliveSettings::setFfprobepath(QDir::cleanPath(appPath + QStringLiteral("/ffprobe")));
        KdenliveSettings::setMeltpath(QDir::cleanPath(appPath + QStringLiteral("/melt")));
        MltConnection::construct(QDir::cleanPath(appPath + QStringLiteral("/../share/mlt/profiles")));
    } else {
        // Open connection with Mlt
        MltConnection::construct(MltPath);
    }

    // TODO Qt6 see: https://doc.qt.io/qt-6/qtquickcontrols-changes-qt6.html#custom-styles-are-now-proper-qml-modules

    connect(this, &Core::showConfigDialog, m_mainWindow, &MainWindow::slotShowPreferencePage);

    Bin *bin = new Bin(m_projectItemModel, m_mainWindow);
    connect(bin, &Bin::requestShowClipProperties, bin, &Bin::showClipProperties, Qt::QueuedConnection);
    m_mainWindow->addBin(bin, QString(), false);

    connect(m_projectItemModel.get(), &ProjectItemModel::refreshPanel, m_mainWindow->activeBin(), &Bin::refreshPanel);
    connect(m_projectItemModel.get(), &ProjectItemModel::refreshClip, m_mainWindow->activeBin(), &Bin::refreshClip);
    connect(m_projectItemModel.get(), &ProjectItemModel::itemDropped, m_mainWindow->activeBin(), &Bin::slotItemDropped, Qt::QueuedConnection);
    connect(m_projectItemModel.get(), &ProjectItemModel::urlsDropped, this, [this](const QList<QUrl> urls, const QModelIndex parent) {
        QMetaObject::invokeMethod(m_mainWindow->activeBin(), "slotUrlsDropped", Qt::QueuedConnection, Q_ARG(QList<QUrl>, urls), Q_ARG(QModelIndex, parent));
    });

    connect(m_projectItemModel.get(), &ProjectItemModel::effectDropped, m_mainWindow->activeBin(), &Bin::slotEffectDropped);
    connect(m_projectItemModel.get(), &ProjectItemModel::addTag, m_mainWindow->activeBin(), &Bin::slotTagDropped);
    connect(m_projectItemModel.get(), &QAbstractItemModel::dataChanged, m_mainWindow->activeBin(), &Bin::slotItemEdited);

    m_monitorManager = new MonitorManager(this);
    projectManager()->init(Url, clipsToLoad);
    m_mainWindow->init();

    m_guiConstructed = true;
    m_projectItemModel->buildPlaylist(QUuid());
    // load the profiles from disk
    ProfileRepository::get()->refresh();
    // load default profile
    m_profile = KdenliveSettings::default_profile();
    // load default profile and ask user to select one if not found.
    if (m_profile.isEmpty()) {
        m_profile = ProjectManager::getDefaultProjectFormat();
        KdenliveSettings::setDefault_profile(m_profile);
    }
    setCurrentProfile(m_profile);
    profileChanged();
    resetThumbProfile();

    if (!ProfileRepository::get()->profileExists(m_profile)) {
        KMessageBox::error(m_mainWindow, i18n("The default profile of Kdenlive is not set or invalid, press OK to set it to a correct value."));

        // TODO this simple widget should be improved and probably use profileWidget
        // we get the list of profiles
        QVector<QPair<QString, QString>> all_profiles = ProfileRepository::get()->getAllProfiles();
        QStringList all_descriptions;
        for (const auto &profile : std::as_const(all_profiles)) {
            all_descriptions << profile.first;
        }

        // ask the user
        bool ok;
        QString item = QInputDialog::getItem(m_mainWindow, i18nc("@title:window", "Select Default Profile"), i18n("Profile:"), all_descriptions, 0, false, &ok);
        if (ok) {
            ok = false;
            for (const auto &profile : std::as_const(all_profiles)) {
                if (profile.first == item) {
                    m_profile = profile.second;
                    ok = true;
                }
            }
        }
        if (!ok) {
            KMessageBox::error(
                m_mainWindow,
                i18n("The given profile is invalid. We default to the profile \"dv_pal\", but you can change this from Kdenlive's settings panel"));
            m_profile = QStringLiteral("dv_pal");
        }
        KdenliveSettings::setDefault_profile(m_profile);
        profileChanged();
    }
    // Init producer shown for unavailable media
    // TODO make it a more proper image, it currently causes a crash on exit
    ClipController::mediaUnavailable = std::make_shared<Mlt::Producer>(ProfileRepository::get()->getProfile(m_self->m_profile)->profile(), "color:blue");
    ClipController::mediaUnavailable->set("length", 99999999);
    if (qApp->isSessionRestored()) {
        // NOTE: we are restoring only one window, because Kdenlive only uses one MainWindow
        m_mainWindow->restore(1, false);
    }

    if (!Url.isEmpty()) {
        Q_EMIT loadingMessageNewStage(i18n("Loading project…"));
    }
    connect(this, &Core::displayBinMessage, this, &Core::displayBinMessagePrivate);
    connect(this, &Core::displayBinLogMessage, this, &Core::displayBinLogMessagePrivate);

    QMetaObject::invokeMethod(pCore->projectManager(), "slotLoadOnOpen", Qt::QueuedConnection);
}

void Core::restoreLayout()
{
    if (KdenliveSettings::kdockLayout().isEmpty() || !KdenliveSettings::kdockLayout().contains(QStringLiteral("KdenliveKDDock"))) {
        // No existing layout, probably first run
        m_mainWindow->show();
    } else {
        KDDockWidgets::LayoutSaver dockLayout(KDDockWidgets::RestoreOption_AbsoluteFloatingDockWindows);
        dockLayout.restoreLayout(KdenliveSettings::kdockLayout().toLatin1());
    }
    if (!KdenliveSettings::showtitlebars()) {
        Q_EMIT hideBars(!KdenliveSettings::showtitlebars());
    }
}

void Core::buildDocks()
{
    // Mixer
    m_mixerWidget = new MixerManager(m_mainWindow);
    connect(m_capture.get(), &MediaCapture::recordStateChanged, m_mixerWidget, &MixerManager::recordStateChanged);
    connect(m_mixerWidget, &MixerManager::updateRecVolume, m_capture.get(), &MediaCapture::setAudioVolume);
    connect(m_monitorManager, &MonitorManager::cleanMixer, m_mixerWidget, &MixerManager::clearMixers);
    m_mixerWidget->checkAudioLevelVersion();
    connect(m_mixerWidget, &MixerManager::showEffectStack, m_projectManager, &ProjectManager::showTrackEffectStack);

    // Media Browser
    m_mediaBrowser = new MediaBrowser(m_mainWindow);

    // Library
    m_library = new LibraryWidget(m_projectManager, m_mainWindow);
    connect(m_library, SIGNAL(addProjectClips(QList<QUrl>)), m_mainWindow->getBin(), SLOT(droppedUrls(QList<QUrl>)));
    connect(this, &Core::updateLibraryPath, m_library, &LibraryWidget::slotUpdateLibraryPath);
    m_library->setupActions();

    // Subtitles
    m_subtitleWidget = new SubtitleEdit(m_mainWindow);
    connect(m_subtitleWidget, &SubtitleEdit::addSubtitle, m_mainWindow, &MainWindow::slotAddSubtitle);
    connect(m_subtitleWidget, &SubtitleEdit::cutSubtitle, this, [this](int id, int cursorPos) {
        if (m_guiConstructed && m_mainWindow->getCurrentTimeline()->controller()) {
            if (cursorPos <= 0) {
                m_mainWindow->getCurrentTimeline()->controller()->requestClipCut(id, -1);
            } else {
                m_mainWindow->getCurrentTimeline()->model()->getSubtitleModel()->doCutSubtitle(id, cursorPos);
            }
        }
    });

    // Text edit speech
    m_textEditWidget = new TextBasedEdit(m_mainWindow);

    // Time remap
    m_timeRemapWidget = new TimeRemap(m_mainWindow);

    // Guides
    m_guidesList = new GuidesList(m_mainWindow);
}

void Core::buildLumaThumbs(const QStringList &values)
{
    for (auto &entry : values) {
        if (MainWindow::m_lumacache.contains(entry)) {
            continue;
        }
        QImage pix(entry);
        if (!pix.isNull()) {
            MainWindow::m_lumacache.insert(entry, pix.scaled(50, 30, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        }
    }
}

QString Core::openExternalApp(QString appPath, QStringList args)
{
    QProcess process;
    if (QFileInfo(appPath).isRelative()) {
        QString updatedPath = QStandardPaths::findExecutable(appPath);
        if (updatedPath.isEmpty()) {
            return i18n("Cannot open file %1", appPath);
        }
        appPath = updatedPath;
    }
#if defined(Q_OS_MACOS)
    args.prepend(QStringLiteral("--args"));
    args.prepend(appPath);
    args.prepend(QStringLiteral("-a"));
    process.setProgram("open");
#else
    process.setProgram(appPath);
#endif
    process.setArguments(args);
    if (pCore->packageType() == LinuxPackageType::AppImage) {
        // Strip appimage custom LD_LIBRARY_PATH...
        QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
        QStringList libPath = env.value(QStringLiteral("LD_LIBRARY_PATH")).split(QLatin1Char(':'), Qt::SkipEmptyParts);
        QStringList updatedLDPath;
        for (auto &s : libPath) {
            if (!s.startsWith(QStringLiteral("/tmp/.mount_"))) {
                updatedLDPath << s;
            }
        }
        if (updatedLDPath.isEmpty()) {
            env.remove(QStringLiteral("LD_LIBRARY_PATH"));
        } else {
            env.insert(QStringLiteral("LD_LIBRARY_PATH"), updatedLDPath.join(QLatin1Char(':')));
        }
        libPath = env.value(QStringLiteral("PATH")).split(QLatin1Char(':'), Qt::SkipEmptyParts);
        updatedLDPath.clear();
        for (auto &s : libPath) {
            if (!s.startsWith(QStringLiteral("/tmp/.mount_"))) {
                updatedLDPath << s;
            }
        }
        if (updatedLDPath.isEmpty()) {
            env.remove(QStringLiteral("PATH"));
        } else {
            env.insert(QStringLiteral("PATH"), updatedLDPath.join(QLatin1Char(':')));
        }
        process.setProcessEnvironment(env);
    }
    qCInfo(KDENLIVE_LOG) << "Starting external app" << appPath << "with arguments" << args;
    if (!process.startDetached()) {
        return process.errorString();
    }
    return QString();
}

const QString Core::nameForLumaFile(const QString &filename)
{
    static QMap<QString, QString> names;
    names.insert("square2-bars.pgm", i18nc("Luma transition name", "Square 2 Bars"));
    names.insert("checkerboard_small.pgm", i18nc("Luma transition name", "Checkerboard Small"));
    names.insert("horizontal_blinds.pgm", i18nc("Luma transition name", "Horizontal Blinds"));
    names.insert("radial.pgm", i18nc("Luma transition name", "Radial"));
    names.insert("linear_x.pgm", i18nc("Luma transition name", "Linear X"));
    names.insert("bi-linear_x.pgm", i18nc("Luma transition name", "Bi-Linear X"));
    names.insert("linear_y.pgm", i18nc("Luma transition name", "Linear Y"));
    names.insert("bi-linear_y.pgm", i18nc("Luma transition name", "Bi-Linear Y"));
    names.insert("square.pgm", i18nc("Luma transition name", "Square"));
    names.insert("square2.pgm", i18nc("Luma transition name", "Square 2"));
    names.insert("cloud.pgm", i18nc("Luma transition name", "Cloud"));
    names.insert("symmetric_clock.pgm", i18nc("Luma transition name", "Symmetric Clock"));
    names.insert("radial-bars.pgm", i18nc("Luma transition name", "Radial Bars"));
    names.insert("spiral.pgm", i18nc("Luma transition name", "Spiral"));
    names.insert("spiral2.pgm", i18nc("Luma transition name", "Spiral 2"));
    names.insert("curtain.pgm", i18nc("Luma transition name", "Curtain"));
    names.insert("burst.pgm", i18nc("Luma transition name", "Burst"));
    names.insert("clock.pgm", i18nc("Luma transition name", "Clock"));

    names.insert("luma01.pgm", i18nc("Luma transition name", "Bar Horizontal"));
    names.insert("luma02.pgm", i18nc("Luma transition name", "Bar Vertical"));
    names.insert("luma03.pgm", i18nc("Luma transition name", "Barn Door Horizontal"));
    names.insert("luma04.pgm", i18nc("Luma transition name", "Barn Door Vertical"));
    names.insert("luma05.pgm", i18nc("Luma transition name", "Barn Door Diagonal SW-NE"));
    names.insert("luma06.pgm", i18nc("Luma transition name", "Barn Door Diagonal NW-SE"));
    names.insert("luma07.pgm", i18nc("Luma transition name", "Diagonal Top Left"));
    names.insert("luma08.pgm", i18nc("Luma transition name", "Diagonal Top Right"));
    names.insert("luma09.pgm", i18nc("Luma transition name", "Matrix Waterfall Horizontal"));
    names.insert("luma10.pgm", i18nc("Luma transition name", "Matrix Waterfall Vertical"));
    names.insert("luma11.pgm", i18nc("Luma transition name", "Matrix Snake Horizontal"));
    names.insert("luma12.pgm", i18nc("Luma transition name", "Matrix Snake Parallel Horizontal"));
    names.insert("luma13.pgm", i18nc("Luma transition name", "Matrix Snake Vertical"));
    names.insert("luma14.pgm", i18nc("Luma transition name", "Matrix Snake Parallel Vertical"));
    names.insert("luma15.pgm", i18nc("Luma transition name", "Barn V Up"));
    names.insert("luma16.pgm", i18nc("Luma transition name", "Iris Circle"));
    names.insert("luma17.pgm", i18nc("Luma transition name", "Double Iris"));
    names.insert("luma18.pgm", i18nc("Luma transition name", "Iris Box"));
    names.insert("luma19.pgm", i18nc("Luma transition name", "Box Bottom Right"));
    names.insert("luma20.pgm", i18nc("Luma transition name", "Box Bottom Left"));
    names.insert("luma21.pgm", i18nc("Luma transition name", "Box Right Center"));
    names.insert("luma22.pgm", i18nc("Luma transition name", "Clock Top"));

    return names.contains(filename) ? names.constFind(filename).value() : filename;
}

std::unique_ptr<Core> &Core::self()
{
    if (!m_self) {
        qWarning() << "Core has not been created";
    }
    return m_self;
}

MainWindow *Core::window()
{
    return m_mainWindow;
}

ProjectManager *Core::projectManager()
{
    return m_projectManager;
}

MonitorManager *Core::monitorManager()
{
    return m_monitorManager;
}

Monitor *Core::getMonitor(int id)
{
    if (id == Kdenlive::ClipMonitor) {
        return m_monitorManager->clipMonitor();
    }
    return m_monitorManager->projectMonitor();
}

void Core::seekMonitor(int id, int position)
{
    if (!m_guiConstructed) {
        return;
    }
    if (id == Kdenlive::ProjectMonitor) {
        m_monitorManager->projectMonitor()->requestSeek(position);
    } else {
        m_monitorManager->clipMonitor()->requestSeek(position);
    }
}

MediaBrowser *Core::mediaBrowser()
{
    if (!m_mainWindow) {
        return nullptr;
    }
    return m_mediaBrowser;
}

Bin *Core::bin()
{
    if (!m_mainWindow) {
        return nullptr;
    }
    return m_mainWindow->getBin();
}

Bin *Core::activeBin()
{
    return m_mainWindow->activeBin();
}

void Core::selectBinClip(const QString &clipId, bool activateMonitor, int frame, const QPoint &zone)
{
    m_mainWindow->activeBin()->selectClipById(clipId, frame, zone, activateMonitor);
}

void Core::selectTimelineItem(int id)
{
    if (m_guiConstructed && m_mainWindow->getCurrentTimeline() && m_mainWindow->getCurrentTimeline()->model()) {
        m_mainWindow->getCurrentTimeline()->model()->requestAddToSelection(id, true);
    }
}

LibraryWidget *Core::library()
{
    return m_library;
}

GuidesList *Core::guidesList()
{
    return m_guidesList;
}

TextBasedEdit *Core::textEditWidget()
{
    return m_textEditWidget;
}

TimeRemap *Core::timeRemapWidget()
{
    return m_timeRemapWidget;
}

bool Core::currentRemap(const QString &clipId)
{
    return m_timeRemapWidget == nullptr ? false : m_timeRemapWidget->currentClip() == clipId;
}

SubtitleEdit *Core::subtitleWidget()
{
    return m_subtitleWidget;
}

MixerManager *Core::mixer()
{
    return m_mixerWidget;
}

void Core::initLocale()
{
    QLocale systemLocale = QLocale(); // For disabling group separator by default
    systemLocale.setNumberOptions(QLocale::OmitGroupSeparator);
    QLocale::setDefault(systemLocale);
}

ToolType::ProjectTool Core::activeTool()
{
    return m_mainWindow->getCurrentTimeline()->activeTool();
}

const QUuid Core::currentTimelineId() const
{
    if (m_projectManager->getTimeline()) {
        return m_projectManager->getTimeline()->uuid();
    }
    return QUuid();
}

std::unique_ptr<Mlt::Repository> &Core::getMltRepository()
{
    return MltConnection::self()->getMltRepository();
}

std::unique_ptr<ProfileModel> &Core::getCurrentProfile() const
{
    return ProfileRepository::get()->getProfile(m_currentProfile);
}

Mlt::Profile &Core::getMonitorProfile()
{
    return m_monitorProfile;
}

Mlt::Profile &Core::getProjectProfile()
{
    return m_projectProfile;
}

std::pair<int, int> Core::getProjectFpsInfo() const
{
    return {m_projectProfile.frame_rate_num(), m_projectProfile.frame_rate_den()};
}

void Core::updateMonitorProfile()
{
    m_monitorProfile.set_colorspace(m_projectProfile.colorspace());
    m_monitorProfile.set_frame_rate(m_projectProfile.frame_rate_num(), m_projectProfile.frame_rate_den());
    m_monitorProfile.set_width(m_projectProfile.width());
    m_monitorProfile.set_height(m_projectProfile.height());
    m_monitorProfile.set_progressive(m_projectProfile.progressive());
    m_monitorProfile.set_sample_aspect(m_projectProfile.sample_aspect_num(), m_projectProfile.sample_aspect_den());
    m_monitorProfile.set_display_aspect(m_projectProfile.display_aspect_num(), m_projectProfile.display_aspect_den());
    m_monitorProfile.set_explicit(true);
    Q_EMIT monitorProfileUpdated();
}

const QString &Core::getCurrentProfilePath() const
{
    return m_currentProfile;
}

bool Core::setCurrentProfile(const QString profilePath)
{
    if (m_currentProfile == profilePath) {
        // no change required, ensure timecode has correct fps
        m_timecode.setFormat(getCurrentProfile()->fps());
        Q_EMIT updateProjectTimecode();
        return true;
    }
    if (ProfileRepository::get()->profileExists(profilePath)) {
        // Ensure all running tasks are stopped before attempting a global profile change
        taskManager.slotCancelJobs();
        m_currentProfile = profilePath;
        std::unique_ptr<ProfileModel> &currentProfile = getCurrentProfile();
        m_projectProfile.set_colorspace(currentProfile->colorspace());
        m_projectProfile.set_frame_rate(currentProfile->frame_rate_num(), currentProfile->frame_rate_den());
        m_projectProfile.set_height(currentProfile->height());
        m_projectProfile.set_progressive(currentProfile->progressive());
        m_projectProfile.set_sample_aspect(currentProfile->sample_aspect_num(), currentProfile->sample_aspect_den());
        m_projectProfile.set_display_aspect(currentProfile->display_aspect_num(), currentProfile->display_aspect_den());
        m_projectProfile.set_width(currentProfile->width());
        free(m_projectProfile.get_profile()->description);
        m_projectProfile.get_profile()->description = strdup(currentProfile->description().toUtf8().constData());
        m_projectProfile.set_explicit(true);
        updateMonitorProfile();
        // Regenerate thumbs profile
        resetThumbProfile();
        // inform render widget
        m_timecode.setFormat(currentProfile->fps());
        profileChanged();
        if (m_guiConstructed) {
            m_monitorManager->resetProfiles();
            Q_EMIT m_monitorManager->updatePreviewScaling();
            if (m_mainWindow->hasTimeline() && m_mainWindow->getCurrentTimeline() && m_mainWindow->getCurrentTimeline()->model()) {
                Q_EMIT m_mainWindow->updateRenderWidgetProfile();
                // m_mainWindow->getCurrentTimeline()->model()->updateProfile(getProjectProfile());
                m_mainWindow->getCurrentTimeline()->model()->updateFieldOrderFilter(currentProfile);
                checkProfileValidity();
                Q_EMIT m_mainWindow->getCurrentTimeline()->controller()->frameFormatChanged();
            }
            Q_EMIT updateProjectTimecode();
        }
        return true;
    }
    return false;
}

void Core::checkProfileValidity()
{
    int offset = (getProjectProfile().width() % 2) + (getProjectProfile().height() % 2);
    if (offset > 0) {
        // Profile is broken, warn user
        Q_EMIT displayBinMessage(i18n("Your project profile is invalid, rendering might fail."), KMessageWidget::Warning);
    }
}

double Core::getCurrentSar() const
{
    return getCurrentProfile()->sar();
}

double Core::getCurrentDar() const
{
    return getCurrentProfile()->dar();
}

double Core::getCurrentFps() const
{
    return getCurrentProfile()->fps();
}

const QSize Core::getCurrentFrameDisplaySize() const
{
    return {qRound(getCurrentProfile()->height() * getCurrentDar()), getCurrentProfile()->height()};
}

const QSize Core::getCurrentFrameSize() const
{
    return {getCurrentProfile()->width(), getCurrentProfile()->height()};
}

void Core::refreshProjectMonitorOnce(bool slowUpdate)
{
    if (!m_guiConstructed || currentDoc()->isBusy()) return;
    m_monitorManager->refreshProjectMonitor(false, slowUpdate);
}

void Core::refreshProjectRange(QPair<int, int> range)
{
    if (!m_guiConstructed || currentDoc()->isBusy()) return;
    m_monitorManager->refreshProjectRange(range, true);
}

const QSize Core::getCompositionSizeOnTrack(const ObjectId &id)
{
    return m_mainWindow->getCurrentTimeline()->model()->getCompositionSizeOnTrack(id);
}

QPair<int, QString> Core::currentTrackInfo() const
{
    if (m_mainWindow->getCurrentTimeline()->controller()) {
        int tid = m_mainWindow->getCurrentTimeline()->controller()->activeTrack();
        if (tid >= 0) {
            return {m_mainWindow->getCurrentTimeline()->model()->getTrackMltIndex(tid), m_mainWindow->getCurrentTimeline()->model()->getTrackTagById(tid)};
        }
        if (m_mainWindow->getCurrentTimeline()->model()->isSubtitleTrack(tid)) {
            return {tid, i18n("Subtitles")};
        }
    }
    return {-1, QString()};
}

int Core::getItemPosition(const ObjectId &id)
{
    switch (id.type) {
    case KdenliveObjectType::TimelineClip:
        if (currentDoc()->getTimeline(id.uuid)->isClip(id.itemId)) {
            return currentDoc()->getTimeline(id.uuid)->getClipPosition(id.itemId);
        } else {
            qWarning() << "querying non clip properties";
        }
        break;
    case KdenliveObjectType::TimelineComposition:
        if (currentDoc()->getTimeline(id.uuid)->isComposition(id.itemId)) {
            return currentDoc()->getTimeline(id.uuid)->getCompositionPosition(id.itemId);
        }
        break;
    case KdenliveObjectType::TimelineMix:
        if (currentDoc()->getTimeline(id.uuid)->isClip(id.itemId)) {
            return currentDoc()->getTimeline(id.uuid)->getMixInOut(id.itemId).first;
        } else {
            qWarning() << "querying non clip properties";
        }
        break;
    case KdenliveObjectType::BinClip:
    case KdenliveObjectType::TimelineTrack:
    case KdenliveObjectType::Master:
        return 0;
    default:
        qWarning() << "unhandled object type";
    }
    return 0;
}

int Core::getItemIn(const ObjectId &id)
{
    if (!m_guiConstructed) {
        qWarning() << "GUI not build";
        return 0;
    }
    switch (id.type) {
    case KdenliveObjectType::TimelineClip: {
        auto timeline = currentDoc()->getTimeline(id.uuid);
        if (timeline && timeline->isClip(id.itemId)) {
            return currentDoc()->getTimeline(id.uuid)->getClipIn(id.itemId);
        } else {
            qWarning() << "querying non clip properties";
        }
        break;
    }
    case KdenliveObjectType::TimelineMix:
    case KdenliveObjectType::TimelineComposition:
    case KdenliveObjectType::BinClip:
    case KdenliveObjectType::TimelineTrack:
    case KdenliveObjectType::Master:
        return 0;
    default:
        qWarning() << "unhandled object type";
    }
    return 0;
}

std::pair<PlaylistState::ClipState, ClipType::ProducerType> Core::getItemState(const ObjectId &id)
{
    switch (id.type) {
    case KdenliveObjectType::TimelineClip:
        if (currentDoc()->getTimeline(id.uuid)->isClip(id.itemId)) {
            return currentDoc()->getTimeline(id.uuid)->getClipState(id.itemId);
        } else {
            qWarning() << "querying non clip properties";
        }
        break;
    case KdenliveObjectType::TimelineComposition:
        return {PlaylistState::VideoOnly, ClipType::Unknown};
    case KdenliveObjectType::BinClip:
        if (!m_guiConstructed) {
            return {PlaylistState::Disabled, ClipType::Unknown};
        }
        return m_mainWindow->getBin()->getClipState(id.itemId);
    case KdenliveObjectType::TimelineTrack:
        return {currentDoc()->getTimeline(id.uuid)->isAudioTrack(id.itemId) ? PlaylistState::AudioOnly : PlaylistState::VideoOnly, ClipType::Unknown};
    case KdenliveObjectType::Master:
        return {PlaylistState::Disabled, ClipType::Unknown};
    default:
        qWarning() << "unhandled object type";
        break;
    }
    return {PlaylistState::Disabled, ClipType::Unknown};
}

int Core::getItemDuration(const ObjectId &id)
{
    switch (id.type) {
    case KdenliveObjectType::TimelineClip:
        if (currentDoc()->getTimeline(id.uuid)->isClip(id.itemId)) {
            return currentDoc()->getTimeline(id.uuid)->getClipPlaytime(id.itemId);
        } else {
            qWarning() << "querying non clip properties";
        }
        break;
    case KdenliveObjectType::TimelineComposition:
        if (currentDoc()->getTimeline(id.uuid)->isComposition(id.itemId)) {
            return currentDoc()->getTimeline(id.uuid)->getCompositionPlaytime(id.itemId);
        }
        break;
    case KdenliveObjectType::BinClip:
        if (!m_guiConstructed) return 0;
        return int(m_mainWindow->getBin()->getClipDuration(id.itemId));
    case KdenliveObjectType::TimelineTrack:
    case KdenliveObjectType::Master:
        return currentDoc()->getTimeline(id.uuid)->duration();
    case KdenliveObjectType::TimelineMix:
        if (currentDoc()->getTimeline(id.uuid)->isClip(id.itemId)) {
            return currentDoc()->getTimeline(id.uuid)->getMixDuration(id.itemId);
        } else {
            qWarning() << "querying non clip properties";
        }
        break;
    default:
        qWarning() << "unhandled object type: " << (int)id.type;
    }
    return 0;
}

QSize Core::getItemFrameSize(const ObjectId &id)
{
    switch (id.type) {
    case KdenliveObjectType::TimelineClip:
        if (currentDoc()->getTimeline(id.uuid)->isClip(id.itemId)) {
            return currentDoc()->getTimeline(id.uuid)->getClipFrameSize(id.itemId);
        } else {
            qWarning() << "querying non clip properties";
        }
        break;
    case KdenliveObjectType::BinClip:
        if (!m_guiConstructed) return QSize();
        return m_mainWindow->getBin()->getFrameSize(id.itemId);
    case KdenliveObjectType::TimelineTrack:
    case KdenliveObjectType::Master:
    case KdenliveObjectType::TimelineComposition:
    case KdenliveObjectType::TimelineMix:
        return pCore->getCurrentFrameSize();
    default:
        qWarning() << "unhandled object type frame size";
    }
    return pCore->getCurrentFrameSize();
}

int Core::getItemTrack(const ObjectId &id)
{
    switch (id.type) {
    case KdenliveObjectType::TimelineClip:
    case KdenliveObjectType::TimelineComposition:
    case KdenliveObjectType::TimelineMix:
        return currentDoc()->getTimeline(id.uuid)->getItemTrackId(id.itemId);
    default:
        qWarning() << "unhandled object type";
    }
    return 0;
}

bool Core::itemContainsPos(const ObjectId &id, int pos)
{
    if (!m_guiConstructed || (!id.uuid.isNull() && !m_mainWindow->getTimeline(id.uuid))) return false;
    switch (id.type) {
    case KdenliveObjectType::TimelineClip:
    case KdenliveObjectType::TimelineComposition:
    case KdenliveObjectType::TimelineMix: {
        int itemPos = getItemPosition(id);
        if (pos < itemPos) {
            return false;
        }
        if (pos >= itemPos + getItemDuration(id)) {
            return false;
        }
        return true;
    }
    default:
        return true;
    }
}

void Core::refreshProjectItem(const ObjectId &id)
{
    if (!m_guiConstructed || (!id.uuid.isNull() && !m_mainWindow->getTimeline(id.uuid))) return;
    switch (id.type) {
    case KdenliveObjectType::TimelineClip:
    case KdenliveObjectType::TimelineMix:
        if (currentDoc()->getTimeline(id.uuid)->isClip(id.itemId)) {
            m_mainWindow->getTimeline(id.uuid)->controller()->refreshItem(id.itemId);
        }
        break;
    case KdenliveObjectType::TimelineComposition:
        if (currentDoc()->getTimeline(id.uuid)->isComposition(id.itemId)) {
            m_mainWindow->getTimeline(id.uuid)->controller()->refreshItem(id.itemId);
        }
        break;
    case KdenliveObjectType::TimelineTrack:
        if (m_mainWindow->getTimeline(id.uuid)->model()->isTrack(id.itemId)) {
            refreshProjectMonitorOnce();
        }
        break;
    case KdenliveObjectType::BinClip:
        if (m_monitorManager->clipMonitorVisible()) {
            m_monitorManager->activateMonitor(Kdenlive::ClipMonitor);
            m_monitorManager->refreshClipMonitor(true);
        }
        if (m_monitorManager->projectMonitorVisible() && m_mainWindow->getCurrentTimeline() &&
            m_mainWindow->getCurrentTimeline()->controller()->refreshIfVisible(id.itemId)) {
            m_monitorManager->refreshTimer.start();
        }
        break;
    case KdenliveObjectType::Master:
        if (m_monitorManager->isActive(Kdenlive::ProjectMonitor)) {
            if (m_monitorManager->projectMonitorVisible()) {
                m_monitorManager->activateMonitor(Kdenlive::ProjectMonitor, false);
                m_monitorManager->refreshProjectMonitor(true);
            }
            m_monitorManager->markMonitorDirty(Kdenlive::ClipMonitor, id.uuid);
        } else {
            m_monitorManager->markMonitorDirty(Kdenlive::ProjectMonitor, id.uuid);
            if (m_monitorManager->clipMonitorVisible()) {
                m_monitorManager->activateMonitor(Kdenlive::ClipMonitor, false);
                m_monitorManager->refreshClipMonitor(true);
            }
        }
        break;
    default:
        qWarning() << "unhandled object type";
    }
}

bool Core::hasTimelinePreview() const
{
    if (!m_guiConstructed) {
        return false;
    }
    return m_mainWindow->getCurrentTimeline()->controller()->renderedChunks().size() > 0;
}

KdenliveDoc *Core::currentDoc()
{
    return m_projectManager->current();
}

Timecode Core::timecode() const
{
    return m_timecode;
}

void Core::setDocumentModified()
{
    m_projectManager->current()->setModified();
}

int Core::projectDuration() const
{
    std::shared_ptr<TimelineItemModel> activeModel = m_projectManager->getTimeline();
    if (activeModel) {
        return activeModel->duration();
    }
    return 0;
}

void Core::profileChanged()
{
    GenTime::setFps(getCurrentFps());
}

void Core::pushUndo(const Fun &undo, const Fun &redo, const QString &text)
{
    undoStack()->push(new FunctionalUndoCommand(undo, redo, text));
}

void Core::pushUndo(QUndoCommand *command)
{
    undoStack()->push(command);
}

int Core::undoIndex() const
{
    return m_projectManager->undoStack()->index();
}

void Core::displaySelectionMessage(const QString &message)
{
    if (m_mainWindow) {
        Q_EMIT m_mainWindow->displaySelectionMessage(message);
    }
}

void Core::displayMessage(const QString &message, MessageType type, int timeout)
{
    if (m_mainWindow) {
        if (type == ProcessingJobMessage || type == OperationCompletedMessage) {
            Q_EMIT m_mainWindow->displayProgressMessage(message, type, timeout);
        } else {
            Q_EMIT m_mainWindow->displayMessage(message, type, timeout);
        }
    } else {
        qDebug() << message;
    }
}

void Core::loadingClips(int count, bool allowInterrupt)
{
    Q_EMIT m_mainWindow->displayProgressMessage(i18n("Loading clips"), MessageType::ProcessingJobMessage, count, allowInterrupt);
}

void Core::displayBinMessagePrivate(const QString &text, int type, const QList<QAction *> &actions, bool showClose, BinMessage::BinCategory messageCategory)
{
    if (m_mainWindow) {
        activeBin()->doDisplayMessage(text, KMessageWidget::MessageType(type), actions, showClose, messageCategory);
    }
}

void Core::displayBinLogMessagePrivate(const QString &text, int type, const QString logInfo)
{
    if (m_mainWindow) {
        activeBin()->doDisplayMessage(text, KMessageWidget::MessageType(type), logInfo);
    }
}

void Core::clearAssetPanel(int itemId)
{
    if (m_guiConstructed) Q_EMIT m_mainWindow->clearAssetPanel(itemId);
}

std::shared_ptr<EffectStackModel> Core::getItemEffectStack(const QUuid &uuid, int itemType, int itemId)
{
    if (!m_guiConstructed) return nullptr;
    switch (itemType) {
    case int(KdenliveObjectType::TimelineClip):
        return currentDoc()->getTimeline(uuid)->getClipEffectStack(itemId);
    case int(KdenliveObjectType::TimelineTrack):
        return currentDoc()->getTimeline(uuid)->getTrackEffectStackModel(itemId);
    case int(KdenliveObjectType::BinClip):
        return m_projectItemModel->getClipEffectStack(itemId);
    case int(KdenliveObjectType::Master):
        return currentDoc()->getTimeline(uuid)->getMasterEffectStackModel();
    default:
        return nullptr;
    }
}

std::shared_ptr<DocUndoStack> Core::undoStack()
{
    return projectManager()->undoStack();
}

QMap<int, QString> Core::getTrackNames(bool videoOnly)
{
    if (!m_guiConstructed) return QMap<int, QString>();
    return m_mainWindow->getCurrentTimeline()->controller()->getTrackNames(videoOnly);
}

QPair<int, int> Core::getCompositionATrack(int cid) const
{
    if (!m_guiConstructed) return {};
    return m_mainWindow->getCurrentTimeline()->controller()->getCompositionATrack(cid);
}

bool Core::compositionAutoTrack(int cid) const
{
    return m_mainWindow->getCurrentTimeline()->controller()->compositionAutoTrack(cid);
}

void Core::setCompositionATrack(int cid, int aTrack)
{
    if (!m_guiConstructed) return;
    m_mainWindow->getCurrentTimeline()->controller()->setCompositionATrack(cid, aTrack);
}

std::shared_ptr<ProjectItemModel> Core::projectItemModel()
{
    return m_projectItemModel;
}

void Core::invalidateRange(QPair<int, int> range)
{
    if (!m_guiConstructed || currentDoc()->isBusy() || !m_mainWindow->getCurrentTimeline() || m_mainWindow->getCurrentTimeline()->loading) return;
    Q_EMIT m_mainWindow->getCurrentTimeline()->model()->invalidateZone(range.first, range.second);
}

void Core::invalidateItem(ObjectId itemId)
{
    if (!m_guiConstructed || !m_mainWindow->getCurrentTimeline() || m_mainWindow->getCurrentTimeline()->loading) return;
    auto tl = m_mainWindow->getTimeline(itemId.uuid);
    switch (itemId.type) {
    case KdenliveObjectType::TimelineClip:
    case KdenliveObjectType::TimelineComposition:
        if (tl) {
            tl->controller()->invalidateItem(itemId.itemId);
        }
        break;
    case KdenliveObjectType::TimelineTrack:
        if (tl) {
            tl->controller()->invalidateTrack(itemId.itemId);
        }
        break;
    case KdenliveObjectType::BinClip:
        m_mainWindow->getBin()->invalidateClip(QString::number(itemId.itemId));
        break;
    case KdenliveObjectType::Master:
        if (tl) {
            Q_EMIT tl->model()->invalidateZone(0, -1);
        }
        break;
    default:
        // compositions should not have effects
        break;
    }
}

double Core::getClipSpeed(ObjectId id) const
{
    auto tl = m_mainWindow->getTimeline(id.uuid);
    if (tl) {
        return tl->model()->getClipSpeed(id.itemId);
    }
    return 1.;
}

void Core::updateItemKeyframes(ObjectId id)
{
    if (id.type == KdenliveObjectType::TimelineClip && m_guiConstructed) {
        auto tl = m_mainWindow->getTimeline(id.uuid);
        if (tl) {
            tl->controller()->updateClip(id.itemId, {TimelineModel::KeyframesRole});
        }
    }
}

void Core::updateItemModel(ObjectId id, const QString &service, const QString &updatedParam)
{
    if (!m_guiConstructed) {
        return;
    }

    if (id.type != KdenliveObjectType::TimelineClip || m_mainWindow->getCurrentTimeline()->loading) {
        return;
    }

    if (!service.startsWith(QLatin1String("fade"))) {
        return;
    }

    auto tl = m_mainWindow->getTimeline(id.uuid);
    if (!tl) {
        return;
    }

    bool startFade = service.startsWith(QLatin1String("fadein")) || service.startsWith(QLatin1String("fade_from_"));
    QVector<int> roles;
    if (startFade) {
        roles = {TimelineModel::FadeInRole, TimelineModel::FadeInMethodRole};
        if (updatedParam == QLatin1String("alpha") || updatedParam == QLatin1String("level")) {
            roles << TimelineModel::FadeInMethodRole;
        }
    } else { // endFade
        roles = {TimelineModel::FadeOutRole, TimelineModel::FadeOutMethodRole};
        if (updatedParam == QLatin1String("alpha") || updatedParam == QLatin1String("level")) {
            roles << TimelineModel::FadeOutMethodRole;
        }
    }
    tl->controller()->updateClip(id.itemId, roles);
}

void Core::showClipKeyframes(ObjectId id, bool enable)
{
    auto tl = m_mainWindow->getTimeline(id.uuid);
    if (!tl) {
        return;
    }
    if (id.type == KdenliveObjectType::TimelineClip) {
        tl->controller()->showClipKeyframes(id.itemId, enable);
    } else if (id.type == KdenliveObjectType::TimelineComposition) {
        tl->controller()->showCompositionKeyframes(id.itemId, enable);
    }
}

void Core::resetThumbProfile()
{
    m_thumbProfile.set_colorspace(m_projectProfile.colorspace());
    m_thumbProfile.set_frame_rate(m_projectProfile.frame_rate_num(), m_projectProfile.frame_rate_den());
    double factor = 144. / m_projectProfile.height();
    m_thumbProfile.set_height(144);
    int width = qRound(m_projectProfile.width() * factor);
    if (width % 2 > 0) {
        width++;
    }
    m_thumbProfile.set_width(width);
    m_thumbProfile.set_progressive(m_projectProfile.progressive());
    m_thumbProfile.set_sample_aspect(m_projectProfile.sample_aspect_num(), m_projectProfile.sample_aspect_den());
    m_thumbProfile.set_display_aspect(m_projectProfile.display_aspect_num(), m_projectProfile.display_aspect_den());
    m_thumbProfile.set_explicit(true);
}

Mlt::Profile &Core::thumbProfile()
{
    return m_thumbProfile;
}

int Core::getMonitorPosition(Kdenlive::MonitorId id) const
{
    if (m_guiConstructed) {
        switch (id) {
        case Kdenlive::ClipMonitor:
            return m_monitorManager->clipMonitor()->position();
        default:
            return m_monitorManager->projectMonitor()->position();
        }
    }
    return 0;
}

void Core::triggerAction(const QString &name)
{
    QAction *action = m_mainWindow->actionCollection()->action(name);
    if (action) {
        action->trigger();
    }
}

const QString Core::actionText(const QString &name)
{
    QAction *action = m_mainWindow->actionCollection()->action(name);
    if (action) {
        return action->toolTip();
    }
    return QString();
}

void Core::addActionToCollection(const QString &name, QAction *action)
{
    m_mainWindow->actionCollection()->addAction(name, action);
}

void Core::clean()
{
    m_self.reset();
}

void Core::startMediaCapture(const QUuid &uuid, int tid, bool checkAudio, bool checkVideo)
{
    Q_UNUSED(checkVideo)
    // TODO: fix video capture
    /*if (checkAudio && checkVideo) {
        m_capture->recordVideo(tid, true);
    } else*/
    if (checkAudio) {
        m_capture->recordAudio(uuid, tid, true);
    }
    m_mediaCaptureFile = m_capture->getCaptureOutputLocation();
}

void Core::stopMediaCapture(int tid, bool checkAudio, bool checkVideo)
{
    Q_UNUSED(checkVideo)
    // TODO: fix video capture
    /*if (checkAudio && checkVideo) {
        m_capture->recordVideo(tid, false);
    } else*/
    if (checkAudio) {
        m_capture->recordAudio(QUuid(), tid, false);
    }
}

void Core::monitorAudio(int tid, bool monitor)
{
    m_mainWindow->getCurrentTimeline()->controller()->switchTrackRecord(tid, monitor);
    if (monitor && pCore->monitorManager()->projectMonitor()->isPlaying()) {
        pCore->monitorManager()->projectMonitor()->stop();
    }
}

void Core::startRecording(bool showCountdown)
{
    int trackId = m_capture->startCapture(showCountdown);
    if (trackId == -1) {
        return;
    }
    m_mainWindow->getCurrentTimeline()->startAudioRecord(trackId);
    pCore->monitorManager()->slotPlay();
}

QStringList Core::getAudioCaptureDevices()
{
    return m_capture->getAudioCaptureDevices();
}

int Core::getMediaCaptureState()
{
    return m_capture->getState();
}

bool Core::isMediaMonitoring() const
{
    return m_capture && m_capture->recordStatus() == MediaCapture::RecordMonitoring;
}

bool Core::isMediaCapturing() const
{
    return m_capture && m_capture->recordStatus() == MediaCapture::RecordRecording;
}

bool Core::captureShowsCountDown() const
{
    return m_capture && m_capture->recordStatus() == MediaCapture::RecordShowingCountDown;
}

void Core::switchCapture()
{
    Q_EMIT recordAudio(-1, !isMediaCapturing());
}

std::shared_ptr<MediaCapture> Core::getAudioDevice()
{
    return m_capture;
}

void Core::resetAudioMonitoring()
{
    if (isMediaMonitoring()) {
        m_capture->switchMonitorState(false);
        m_capture->switchMonitorState(true);
    }
}

void Core::setAudioMonitoring(bool enable)
{
    m_capture->switchMonitorState(enable);
}

QString Core::getProjectCaptureFolderName()
{
    if (currentDoc()) {
        return currentDoc()->projectCaptureFolder() + QDir::separator();
    }
    return QString();
}

const QString Core::getTimelineClipBinId(ObjectId id)
{
    if (!m_guiConstructed) {
        return QString();
    }
    auto tl = m_mainWindow->getTimeline(id.uuid);
    if (tl && tl->model()->isClip(id.itemId)) {
        return tl->model()->getClipBinId(id.itemId);
    }
    return QString();
}
std::unordered_set<QString> Core::getAllTimelineTracksId()
{
    std::unordered_set<int> timelineClipIds = m_mainWindow->getCurrentTimeline()->model()->getItemsInRange(-1, 0);
    std::unordered_set<QString> tClipBinIds;
    for (int id : timelineClipIds) {
        auto idString = m_mainWindow->getCurrentTimeline()->model()->getClipBinId(id);
        tClipBinIds.insert(idString);
    }
    return tClipBinIds;
}

int Core::getDurationFromString(const QString &time)
{
    return m_timecode.getFrameCount(time);
}

void Core::processInvalidFilter(const QString &service, const QString &message, const QString &log)
{
    if (m_guiConstructed) Q_EMIT m_mainWindow->assetPanelWarning(service, message, log);
}

void Core::updateProjectTags(int previousCount, const QMap<int, QStringList> &tags)
{
    if (previousCount > tags.size()) {
        // Clear previous tags
        for (int i = 1; i <= previousCount; i++) {
            QString current = currentDoc()->getDocumentProperty(QStringLiteral("tag%1").arg(i));
            if (!current.isEmpty()) {
                currentDoc()->setDocumentProperty(QStringLiteral("tag%1").arg(i), QString());
            }
        }
    }
    QMapIterator<int, QStringList> j(tags);
    int i = 1;
    while (j.hasNext()) {
        j.next();
        currentDoc()->setDocumentProperty(QStringLiteral("tag%1").arg(i), QStringLiteral("%1:%2").arg(j.value().at(1), j.value().at(2)));
        i++;
    }
}

std::unique_ptr<Mlt::Producer> Core::getMasterProducerInstance()
{
    if (m_guiConstructed && m_mainWindow->getCurrentTimeline()) {
        const QString scene = m_projectManager->projectSceneList(QString(), true).first;
        std::unique_ptr<Mlt::Producer> producer(new Mlt::Producer(pCore->getProjectProfile(), "xml-string", scene.toUtf8().constData()));
        return producer;
    }
    return nullptr;
}

std::unique_ptr<Mlt::Producer> Core::getTrackProducerInstance(int tid)
{
    if (m_guiConstructed && m_mainWindow->getCurrentTimeline()) {
        std::unique_ptr<Mlt::Producer> producer(new Mlt::Producer(m_mainWindow->getCurrentTimeline()->controller()->trackProducer(tid)));
        return producer;
    }
    return nullptr;
}

bool Core::enableMultiTrack(bool enable)
{
    if (!m_guiConstructed || !m_mainWindow->getCurrentTimeline()) {
        return false;
    }
    bool isMultiTrack = pCore->monitorManager()->isMultiTrack();
    if (isMultiTrack || enable) {
        pCore->window()->getCurrentTimeline()->controller()->slotMultitrackView(enable, true);
        return true;
    }
    return false;
}

int Core::audioChannels()
{
    if (m_projectManager && m_projectManager->current()) {
        return m_projectManager->current()->audioChannels();
    }
    return 2;
}

void Core::addGuides(const QMap<QUuid, QMap<int, QString>> &guides)
{
    QMapIterator<QUuid, QMap<int, QString>> i(guides);
    while (i.hasNext()) {
        i.next();
        QMap<GenTime, QString> markers;
        QMap<int, QString> values = i.value();
        int ix = 1;
        for (auto j = values.cbegin(), end = values.cend(); j != end; ++j) {
            GenTime p(j.key(), pCore->getCurrentFps());
            markers.insert(p, j.value().isEmpty() ? i18n("Marker %1", ix) : j.value());
            ix++;
        }
        auto timeline = m_mainWindow->getTimeline(i.key());
        if (timeline == nullptr) {
            // Timeline not found, default to active one
            timeline = m_mainWindow->getCurrentTimeline();
        }
        timeline->controller()->getModel()->getGuideModel()->addMarkers(markers);
    }
}

void Core::temporaryUnplug(const QList<int> &clipIds, bool hide)
{
    window()->getCurrentTimeline()->controller()->temporaryUnplug(clipIds, hide);
}

void Core::transcodeFile(const QString &url)
{
    qDebug() << "=== TRANSCODING: " << url;
    window()->slotTranscode({url});
}

void Core::transcodeFriendlyFile(const QString &binId, bool checkProfile)
{
    window()->slotFriendlyTranscode(binId, checkProfile);
}

void Core::setWidgetKeyBinding(const QString &mess)
{
    window()->setWidgetKeyBinding(mess);
}

void Core::showEffectZone(ObjectId id, QPair<int, int> inOut, bool checked)
{
    if (m_guiConstructed && id.type != KdenliveObjectType::BinClip) {
        auto tl = m_mainWindow->getTimeline(id.uuid);
        if (tl) {
            if (id.type == KdenliveObjectType::TimelineClip) {
                int offset = getItemPosition(id);
                inOut.first += offset;
                inOut.second += offset;
            }
            tl->controller()->showRulerEffectZone(inOut, checked);
        }
    }
}

void Core::updateMasterZones()
{
    if (m_guiConstructed && m_mainWindow->getCurrentTimeline() && m_mainWindow->getCurrentTimeline()->controller()) {
        m_mainWindow->getCurrentTimeline()->controller()->updateMasterZones(m_mainWindow->getCurrentTimeline()->model()->getMasterEffectZones());
    }
}

void Core::testProxies()
{
    QScopedPointer<ProxyTest> dialog(new ProxyTest(QApplication::activeWindow()));
    dialog->exec();
}

void Core::resizeMix(const ObjectId id, int duration, MixAlignment align, int leftFrames)
{
    auto tl = m_mainWindow->getTimeline(id.uuid);
    if (tl) {
        tl->controller()->resizeMix(id.itemId, duration, align, leftFrames);
    }
}

MixAlignment Core::getMixAlign(const ObjectId &itemInfo) const
{
    return m_mainWindow->getTimeline(itemInfo.uuid)->controller()->getMixAlign(itemInfo.itemId);
}

int Core::getMixCutPos(const ObjectId &itemInfo) const
{
    return m_mainWindow->getTimeline(itemInfo.uuid)->controller()->getMixCutPos(itemInfo.itemId);
}

void Core::clearTimeRemap()
{
    if (timeRemapWidget()) {
        timeRemapWidget()->selectedClip(-1, QUuid());
    }
}

void Core::cleanup()
{
    audioThumbCache.clear();
    taskManager.slotCancelJobs();
    if (timeRemapWidget()) {
        timeRemapWidget()->selectedClip(-1, QUuid());
    }
    if (m_mainWindow && m_mainWindow->getCurrentTimeline()) {
        guidesList()->clear();
        disconnect(m_mainWindow->getCurrentTimeline()->controller(), &TimelineController::durationChanged, m_projectManager,
                   &ProjectManager::adjustProjectDuration);
        m_mainWindow->getCurrentTimeline()->controller()->clipActions.clear();
    }
}

void Core::addBin(const QString &id)
{
    Bin *bin = new Bin(m_projectItemModel, m_mainWindow, false);
    const QString folderName = bin->setDocument(pCore->currentDoc(), id);
    m_mainWindow->addBin(bin, folderName);
}

void Core::loadTimelinePreview(const QUuid uuid, const QString &chunks, const QString &dirty, bool enablePreview, Mlt::Playlist &playlist)
{
    std::shared_ptr<TimelineItemModel> tl = pCore->currentDoc()->getTimeline(uuid);
    if (tl) {
        tl->loadPreview(chunks, dirty, enablePreview, playlist);
    }
}

void Core::updateSequenceAVType(const QUuid &uuid, int tracksCount)
{
    if (m_mainWindow) {
        pCore->bin()->updateSequenceAVType(uuid, tracksCount);
    }
}

int Core::getAssetGroupedInstance(const ObjectId &id, const QString &assetId)
{
    switch (id.type) {
    case KdenliveObjectType::TimelineClip:
        return currentDoc()->getTimeline(id.uuid)->clipAssetGroupInstances(id.itemId, assetId);
    case KdenliveObjectType::BinClip:
        return bin()->clipAssetGroupInstances(id.itemId, assetId);
    default:
        return 0;
    }
}

QList<std::shared_ptr<KeyframeModelList>> Core::getGroupKeyframeModels(const ObjectId &id, const QString &assetId)
{
    if (KdenliveSettings::applyEffectParamsToGroup()) {
        switch (id.type) {
        case KdenliveObjectType::TimelineClip:
            if (auto tl = currentDoc()->getTimeline(id.uuid)) {
                return tl->getGroupKeyframeModels(id.itemId, assetId);
            }
            break;
        case KdenliveObjectType::BinClip:
            if (bin() != nullptr) {
                return bin()->getGroupKeyframeModels(id.itemId, assetId);
            }
            break;
        default:
            // Nothing to do
            break;
        }
    }
    return {};
}

void Core::groupAssetCommand(const ObjectId &id, const QString &assetId, const QModelIndex &index, const QString &previousValue, QString value,
                             QUndoCommand *command)
{
    if (KdenliveSettings::applyEffectParamsToGroup()) {
        switch (id.type) {
        case KdenliveObjectType::TimelineClip:
            if (auto tl = currentDoc()->getTimeline(id.uuid)) {
                tl->applyClipAssetGroupCommand(id.itemId, assetId, index, previousValue, value, command);
            }
            break;
        case KdenliveObjectType::BinClip:
            if (bin() != nullptr) {
                bin()->applyClipAssetGroupCommand(id.itemId, assetId, index, previousValue, value, command);
            }
            break;
        default:
            // Nothing to do
            return;
        }
    }
}

void Core::groupAssetKeyframeCommand(const ObjectId &id, const QString &assetId, const QModelIndex &index, GenTime pos, const QVariant &previousValue,
                                     const QVariant &value, int ix, QUndoCommand *command)
{
    if (KdenliveSettings::applyEffectParamsToGroup()) {
        switch (id.type) {
        case KdenliveObjectType::TimelineClip:
            if (auto tl = currentDoc()->getTimeline(id.uuid)) {
                pos -= GenTime(tl->getClipIn(id.itemId), getCurrentFps());
                tl->applyClipAssetGroupKeyframeCommand(id.itemId, assetId, index, pos, previousValue, value, ix, command);
            }
            break;
        case KdenliveObjectType::BinClip:
            if (bin() != nullptr) {
                bin()->applyClipAssetGroupKeyframeCommand(id.itemId, assetId, index, pos, previousValue, value, ix, command);
            }
            break;
        default:
            return;
        }
    }
}

void Core::groupAssetMultiKeyframeCommand(const ObjectId &id, const QString &assetId, const QList<QModelIndex> &indexes, GenTime pos,
                                          const QStringList &sourceValues, const QStringList &values, QUndoCommand *command)
{
    if (KdenliveSettings::applyEffectParamsToGroup()) {
        switch (id.type) {
        case KdenliveObjectType::TimelineClip:
            if (auto tl = currentDoc()->getTimeline(id.uuid)) {
                pos -= GenTime(tl->getClipIn(id.itemId), getCurrentFps());
                tl->applyClipAssetGroupMultiKeyframeCommand(id.itemId, assetId, indexes, pos, sourceValues, values, command);
            }
            break;
        case KdenliveObjectType::BinClip:
            if (bin() != nullptr) {
                bin()->applyClipAssetGroupMultiKeyframeCommand(id.itemId, assetId, indexes, pos, sourceValues, values, command);
            }
            break;
        default:
            return;
        }
    }
}

void Core::removeGroupEffect(const ObjectId &id, const QString &assetId, int originalId)
{
    switch (id.type) {
    case KdenliveObjectType::TimelineClip:
        if (auto tl = currentDoc()->getTimeline(id.uuid)) {
            tl->removeEffectFromGroup(id.itemId, assetId, originalId);
        }
        break;
    case KdenliveObjectType::BinClip:
        if (bin() != nullptr) {
            bin()->removeEffectFromGroup(id.itemId, assetId, originalId);
        }
        break;
    default:
        return;
    }
}

void Core::applyEffectDisableToGroup(const ObjectId &id, const QString &assetId, bool disable, Fun &undo, Fun &redo)
{
    switch (id.type) {
    case KdenliveObjectType::TimelineClip:
        if (auto tl = currentDoc()->getTimeline(id.uuid)) {
            tl->disableEffectFromGroup(id.itemId, assetId, disable, undo, redo);
        }
        break;
    case KdenliveObjectType::BinClip:
        if (bin() != nullptr) {
            bin()->disableEffectFromGroup(id.itemId, assetId, disable, undo, redo);
        }
        break;
    default:
        return;
    }
}

bool Core::guiReady() const
{
    return m_guiConstructed;
}

void Core::folderRenamed(const QString &binId, const QString &folderName)
{
    if (m_mainWindow) {
        m_mainWindow->folderRenamed(binId, folderName);
    }
}

void Core::highlightFileInExplorer(QList<QUrl> urls)
{
    if (urls.isEmpty()) {
        return;
    }
    if (m_packageType == LinuxPackageType::Flatpak) {
        QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(urls.first().toLocalFile()).absolutePath()));
    } else {
        KIO::highlightInFileManager(urls);
    }
}

void Core::updateHoverItem(const QUuid &uuid)
{
    if (m_guiConstructed && uuid == m_mainWindow->getCurrentTimeline()->getUuid()) {
        m_mainWindow->getCurrentTimeline()->regainFocus();
    }
}

std::pair<bool, bool> Core::assetHasAV(ObjectId id)
{
    switch (id.type) {
    case KdenliveObjectType::Master:
        return {true, true};
    case KdenliveObjectType::TimelineTrack: {
        auto timeline = m_mainWindow->getTimeline(id.uuid);
        if (timeline && timeline->model()->isAudioTrack(id.itemId)) {
            return {true, false};
        }
        return {false, true};
    }
    case KdenliveObjectType::TimelineClip: {
        auto timeline = m_mainWindow->getTimeline(id.uuid);
        if (timeline && timeline->model()->clipIsAudio(id.itemId)) {
            return {true, false};
        }
        return {false, true};
    }
    case KdenliveObjectType::BinClip: {
        PlaylistState::ClipState state = bin()->getClipState(id.itemId).first;
        if (state == PlaylistState::Disabled) {
            return {true, true};
        } else if (state == PlaylistState::AudioOnly) {
            return {true, false};
        } else {
            return {false, true};
        }
    }
    default:
        return {false, false};
    }
}

void Core::showEffectStackFromId(ObjectId owner)
{
    switch (owner.type) {
    case KdenliveObjectType::BinClip:
        activeBin()->showItemEffectStack(owner);
        break;
    case KdenliveObjectType::TimelineClip:
        if (m_guiConstructed && m_mainWindow->getCurrentTimeline()->controller()) {
            m_mainWindow->getCurrentTimeline()->controller()->showAsset(owner.itemId);
        }
        break;
    case KdenliveObjectType::TimelineTrack:
        if (m_guiConstructed && m_mainWindow->getCurrentTimeline()->controller()) {
            m_mainWindow->getCurrentTimeline()->controller()->showTrackAsset(owner.itemId);
        }
        break;
    case KdenliveObjectType::Master:
        if (m_guiConstructed && m_mainWindow->getCurrentTimeline()->controller()) {
            m_mainWindow->getCurrentTimeline()->controller()->showMasterEffects();
        }
        break;
    default:
        break;
    }
}

void Core::openDocumentationLink(const QUrl &link)
{
    if (KMessageBox::questionTwoActions(
            QApplication::activeWindow(),
            i18n("This will open a browser to display Kdenlive's online documentation at the following url:\n %1", link.toDisplayString()), {},
            KGuiItem(i18n("Open Browser")), KStandardGuiItem::cancel(), QStringLiteral("allow_browser_help")) == KMessageBox::SecondaryAction) {
        // Stop
        return;
    }
    QDesktopServices::openUrl(link);
}

std::pair<QString, int> Core::getSelectedClipAndOffset()
{
    if (!m_guiConstructed || !m_mainWindow->getCurrentTimeline()->controller()) {
        return {};
    }
    int cid = m_mainWindow->getCurrentTimeline()->controller()->getMainSelectedClip();
    if (cid == -1) {
        // No selected clip
        return {};
    }
    // Get position and in point
    ObjectId id(KdenliveObjectType::TimelineClip, cid, m_mainWindow->getCurrentTimeline()->getUuid());
    int offset = getItemPosition(id) - getItemIn(id);
    return {getTimelineClipBinId(id), offset};
}

int Core::currentTimelineOffset()
{
    return currentDoc()->getSequenceProperty(currentDoc()->activeUuid, QStringLiteral("kdenlive:sequenceproperties.timecodeOffset")).toInt();
}

void Core::updateHwDecoding()
{
    qputenv("MLT_AVFORMAT_HWACCEL", KdenliveSettings::hwDecoding().toUtf8());
}
