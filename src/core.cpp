/*
Copyright (C) 2014  Till Theato <root@ttill.de>
This file is part of kdenlive. See www.kdenlive.org.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
*/

#include "core.h"
#include "bin/bin.h"
#include "bin/projectitemmodel.h"
#include "capture/mediacapture.h"
#include "doc/docundostack.hpp"
#include "doc/kdenlivedoc.h"
#include "jobs/jobmanager.h"
#include "kdenlive_debug.h"
#include "kdenlivesettings.h"
#include "library/librarywidget.h"
#include "audiomixer/mixermanager.hpp"
#include "mainwindow.h"
#include "mltconnection.h"
#include "mltcontroller/clipcontroller.h"
#include "monitor/monitormanager.h"
#include "profiles/profilemodel.hpp"
#include "profiles/profilerepository.hpp"
#include "project/projectmanager.h"
#include "timeline2/model/timelineitemmodel.hpp"
#include "timeline2/view/timelinecontroller.h"
#include "timeline2/view/timelinewidget.h"

#include <mlt++/MltRepository.h>

#include <KMessageBox>
#include <QCoreApplication>
#include <QInputDialog>
#include <QDir>
#include <QQuickStyle>
#include <locale>
#ifdef Q_OS_MAC
#include <xlocale.h>
#endif

std::unique_ptr<Core> Core::m_self;
Core::Core()
    : audioThumbCache(QStringLiteral("audioCache"), 2000000)
    , m_thumbProfile(nullptr)
    , m_capture(new MediaCapture(this))
{
}

void Core::prepareShutdown()
{
    m_guiConstructed = false;
    m_mainWindow->getCurrentTimeline()->controller()->prepareClose();
    projectItemModel()->blockSignals(true);
    QThreadPool::globalInstance()->clear();
}

Core::~Core()
{
    qDebug() << "deleting core";
    if (m_monitorManager) {
        delete m_monitorManager;
    }
    // delete m_binWidget;
    if (m_projectManager) {
        delete m_projectManager;
    }
    ClipController::mediaUnavailable.reset();
}

void Core::build(bool isAppImage, const QString &MltPath)
{
    if (m_self) {
        return;
    }
    m_self.reset(new Core());
    m_self->initLocale();

    qRegisterMetaType<audioShortVector>("audioShortVector");
    qRegisterMetaType<QVector<double>>("QVector<double>");
    qRegisterMetaType<MessageType>("MessageType");
    qRegisterMetaType<stringMap>("stringMap");
    qRegisterMetaType<audioByteArray>("audioByteArray");
    qRegisterMetaType<QList<ItemInfo>>("QList<ItemInfo>");
    qRegisterMetaType<std::shared_ptr<Mlt::Producer>>("std::shared_ptr<Mlt::Producer>");
    qRegisterMetaType<QVector<int>>();
    qRegisterMetaType<QDomElement>("QDomElement");
    qRegisterMetaType<requestClipInfo>("requestClipInfo");

    if (isAppImage) {
        QString appPath = qApp->applicationDirPath();
        KdenliveSettings::setFfmpegpath(QDir::cleanPath(appPath + QStringLiteral("/ffmpeg")));
        KdenliveSettings::setFfplaypath(QDir::cleanPath(appPath + QStringLiteral("/ffplay")));
        KdenliveSettings::setFfprobepath(QDir::cleanPath(appPath + QStringLiteral("/ffprobe")));
        KdenliveSettings::setRendererpath(QDir::cleanPath(appPath + QStringLiteral("/melt")));
        MltConnection::construct(QDir::cleanPath(appPath + QStringLiteral("/../share/mlt/profiles")));
    } else {
        // Open connection with Mlt
        MltConnection::construct(MltPath);
    }

    // load the profile from disk
    ProfileRepository::get()->refresh();
    // load default profile
    m_self->m_profile = KdenliveSettings::default_profile();
    if (m_self->m_profile.isEmpty()) {
        m_self->m_profile = ProjectManager::getDefaultProjectFormat();
        KdenliveSettings::setDefault_profile(m_self->m_profile);
    }

    // Init producer shown for unavailable media
    // TODO make it a more proper image, it currently causes a crash on exit
    ClipController::mediaUnavailable = std::make_shared<Mlt::Producer>(ProfileRepository::get()->getProfile(m_self->m_profile)->profile(), "color:blue");
    ClipController::mediaUnavailable->set("length", 99999999);

    m_self->m_projectItemModel = ProjectItemModel::construct();
    // Job manager must be created before bin to correctly connect
    m_self->m_jobManager.reset(new JobManager(m_self.get()));
}

void Core::initGUI(const QUrl &Url, const QString &clipsToLoad)
{
    m_profile = KdenliveSettings::default_profile();
    m_currentProfile = m_profile;
    profileChanged();
    m_mainWindow = new MainWindow();
    m_guiConstructed = true;
    QStringList styles = QQuickStyle::availableStyles();
    if (styles.contains(QLatin1String("org.kde.desktop"))) {
        QQuickStyle::setStyle("org.kde.desktop");
    } else if (styles.contains(QLatin1String("Fusion"))) {
        QQuickStyle::setStyle("Fusion");
    }

    connect(this, &Core::showConfigDialog, m_mainWindow, &MainWindow::slotPreferences);

    // load default profile and ask user to select one if not found.
    if (m_profile.isEmpty()) {
        m_profile = ProjectManager::getDefaultProjectFormat();
        profileChanged();
        KdenliveSettings::setDefault_profile(m_profile);
    }

    if (!ProfileRepository::get()->profileExists(m_profile)) {
        KMessageBox::sorry(m_mainWindow, i18n("The default profile of Kdenlive is not set or invalid, press OK to set it to a correct value."));

        // TODO this simple widget should be improved and probably use profileWidget
        // we get the list of profiles
        QVector<QPair<QString, QString>> all_profiles = ProfileRepository::get()->getAllProfiles();
        QStringList all_descriptions;
        for (const auto &profile : qAsConst(all_profiles)) {
            all_descriptions << profile.first;
        }

        // ask the user
        bool ok;
        QString item = QInputDialog::getItem(m_mainWindow, i18n("Select Default Profile"), i18n("Profile:"), all_descriptions, 0, false, &ok);
        if (ok) {
            ok = false;
            for (const auto &profile : qAsConst(all_profiles)) {
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

    m_projectManager = new ProjectManager(this);
    m_binWidget = new Bin(m_projectItemModel, m_mainWindow);
    m_library = new LibraryWidget(m_projectManager, m_mainWindow);
    m_mixerWidget = new MixerManager(m_mainWindow);
    connect(m_library, SIGNAL(addProjectClips(QList<QUrl>)), m_binWidget, SLOT(droppedUrls(QList<QUrl>)));
    connect(this, &Core::updateLibraryPath, m_library, &LibraryWidget::slotUpdateLibraryPath);
    connect(m_capture.get(), &MediaCapture::recordStateChanged, m_mixerWidget, &MixerManager::recordStateChanged);
    connect(m_mixerWidget, &MixerManager::updateRecVolume, m_capture.get(), &MediaCapture::setAudioVolume);
    m_monitorManager = new MonitorManager(this);
    connect(m_monitorManager, &MonitorManager::cleanMixer, m_mixerWidget, &MixerManager::clearMixers);
    // Producer queue, creating MLT::Producers on request
    /*
    m_producerQueue = new ProducerQueue(m_binController);
    connect(m_producerQueue, &ProducerQueue::gotFileProperties, m_binWidget, &Bin::slotProducerReady);
    connect(m_producerQueue, &ProducerQueue::replyGetImage, m_binWidget, &Bin::slotThumbnailReady);
    connect(m_producerQueue, &ProducerQueue::requestProxy,
            [this](const QString &id){ m_binWidget->startJob(id, AbstractClipJob::PROXYJOB);});
    connect(m_producerQueue, &ProducerQueue::removeInvalidClip, m_binWidget, &Bin::slotRemoveInvalidClip, Qt::DirectConnection);
    connect(m_producerQueue, SIGNAL(addClip(QString, QMap<QString, QString>)), m_binWidget, SLOT(slotAddUrl(QString, QMap<QString, QString>)));
    connect(m_binController.get(), SIGNAL(createThumb(QDomElement, QString, int)), m_producerQueue, SLOT(getFileProperties(QDomElement, QString, int)));
    connect(m_binWidget, &Bin::producerReady, m_producerQueue, &ProducerQueue::slotProcessingDone, Qt::DirectConnection);
    // TODO
    connect(m_producerQueue, SIGNAL(removeInvalidProxy(QString,bool)), m_binWidget, SLOT(slotRemoveInvalidProxy(QString,bool)));*/

    m_mainWindow->init();
    if (!Url.isEmpty()) {
        emit loadingMessageUpdated(i18n("Loading project..."));
    }
    projectManager()->init(Url, clipsToLoad);
    if (qApp->isSessionRestored()) {
        // NOTE: we are restoring only one window, because Kdenlive only uses one MainWindow
        m_mainWindow->restore(1, false);
    }
    QMetaObject::invokeMethod(pCore->projectManager(), "slotLoadOnOpen", Qt::QueuedConnection);
    m_mainWindow->show();
    QThreadPool::globalInstance()->setMaxThreadCount(qMin(4, QThreadPool::globalInstance()->maxThreadCount()));
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

std::unique_ptr<Core> &Core::self()
{
    if (!m_self) {
        qDebug() << "Error : Core has not been created";
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

Bin *Core::bin()
{
    return m_binWidget;
}

void Core::selectBinClip(const QString &clipId, int frame, const QPoint &zone)
{
    m_binWidget->selectClipById(clipId, frame, zone);
}

std::shared_ptr<JobManager> Core::jobManager()
{
    return m_jobManager;
}

LibraryWidget *Core::library()
{
    return m_library;
}

MixerManager *Core::mixer()
{
    return m_mixerWidget;
}

void Core::initLocale()
{
    qDebug() << "Using modified system locale without group separator for numbers";
    QLocale systemLocale = QLocale(); // For disabling group separator by default → OK
    systemLocale.setNumberOptions(QLocale::OmitGroupSeparator);
    QLocale::setDefault(systemLocale);
}

std::unique_ptr<Mlt::Repository> &Core::getMltRepository()
{
    return MltConnection::self()->getMltRepository();
}

std::unique_ptr<ProfileModel> &Core::getCurrentProfile() const
{
    return ProfileRepository::get()->getProfile(m_currentProfile);
}

Mlt::Profile *Core::getProjectProfile()
{
    if (!m_projectProfile) {
        m_projectProfile = std::make_unique<Mlt::Profile>(m_currentProfile.toStdString().c_str());
        m_projectProfile->set_explicit(1);
    }
    return m_projectProfile.get();
}

const QString &Core::getCurrentProfilePath() const
{
    return m_currentProfile;
}

bool Core::setCurrentProfile(const QString &profilePath)
{
    if (m_currentProfile == profilePath) {
        // no change required, ensure timecode has correct fps
        m_timecode.setFormat(getCurrentProfile()->fps());
        return true;
    }
    if (ProfileRepository::get()->profileExists(profilePath)) {
        m_currentProfile = profilePath;
        m_thumbProfile.reset();
        if (m_projectProfile) {
            m_projectProfile->set_colorspace(getCurrentProfile()->colorspace());
            m_projectProfile->set_frame_rate(getCurrentProfile()->frame_rate_num(), getCurrentProfile()->frame_rate_den());
            m_projectProfile->set_height(getCurrentProfile()->height());
            m_projectProfile->set_progressive(getCurrentProfile()->progressive());
            m_projectProfile->set_sample_aspect(getCurrentProfile()->sample_aspect_num(), getCurrentProfile()->sample_aspect_den());
            m_projectProfile->set_display_aspect(getCurrentProfile()->display_aspect_num(), getCurrentProfile()->display_aspect_den());
            m_projectProfile->set_width(getCurrentProfile()->width());
            m_projectProfile->set_explicit(true);
        }
        // inform render widget
        m_timecode.setFormat(getCurrentProfile()->fps());
        profileChanged();
        emit m_mainWindow->updateRenderWidgetProfile();
        pCore->monitorManager()->resetProfiles();
        emit pCore->monitorManager()->updatePreviewScaling();
        if (m_guiConstructed && m_mainWindow->getCurrentTimeline()->controller()->getModel()) {
            m_mainWindow->getCurrentTimeline()->controller()->getModel()->updateProfile(getProjectProfile());
            checkProfileValidity();
        }
        return true;
    }
    return false;
}

void Core::checkProfileValidity()
{
    int offset = (getCurrentProfile()->profile().width() % 8) + (getCurrentProfile()->profile().height() % 2);
    if (offset > 0) {
        // Profile is broken, warn user
        if (m_binWidget) {
            emit m_binWidget->displayBinMessage(i18n("Your project profile is invalid, rendering might fail."), KMessageWidget::Warning);
        }
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


QSize Core::getCurrentFrameDisplaySize() const
{
    return {(int)(getCurrentProfile()->height() * getCurrentDar() + 0.5), getCurrentProfile()->height()};
}

QSize Core::getCurrentFrameSize() const
{
    return {getCurrentProfile()->width(), getCurrentProfile()->height()};
}

void Core::requestMonitorRefresh()
{
    if (!m_guiConstructed) return;
    m_monitorManager->refreshProjectMonitor();
}

void Core::refreshProjectRange(QSize range)
{
    if (!m_guiConstructed) return;
    m_monitorManager->refreshProjectRange(range);
}

int Core::getItemPosition(const ObjectId &id)
{
    if (!m_guiConstructed) return 0;
    switch (id.first) {
    case ObjectType::TimelineClip:
        if (m_mainWindow->getCurrentTimeline()->controller()->getModel()->isClip(id.second)) {
            return m_mainWindow->getCurrentTimeline()->controller()->getModel()->getClipPosition(id.second);
        }
        break;
    case ObjectType::TimelineComposition:
        if (m_mainWindow->getCurrentTimeline()->controller()->getModel()->isComposition(id.second)) {
            return m_mainWindow->getCurrentTimeline()->controller()->getModel()->getCompositionPosition(id.second);
        }
        break;
    case ObjectType::BinClip:
    case ObjectType::TimelineTrack:
    case ObjectType::Master:
        return 0;
        break;
    default:
        qDebug() << "ERROR: unhandled object type";
    }
    return 0;
}

int Core::getItemIn(const ObjectId &id)
{
    if (!m_guiConstructed || !m_mainWindow->getCurrentTimeline() || !m_mainWindow->getCurrentTimeline()->controller()->getModel()) {
        qDebug() << "/ / // QUERYING ITEM IN BUT GUI NOT BUILD!!";
        return 0;
    }
    switch (id.first) {
    case ObjectType::TimelineClip:
        if (m_mainWindow->getCurrentTimeline()->controller()->getModel()->isClip(id.second)) {
            return m_mainWindow->getCurrentTimeline()->controller()->getModel()->getClipIn(id.second);
        } else {
            qDebug()<<"// ERROR QUERYING NON CLIP PROPERTIES\n\n!!!!!!!!!!!!!!!!!!!!!!!!!!";
        }
        break;
    case ObjectType::TimelineComposition:
    case ObjectType::BinClip:
    case ObjectType::TimelineTrack:
    case ObjectType::Master:
        return 0;
        break;
    default:
        qDebug() << "ERROR: unhandled object type";
    }
    return 0;
}

PlaylistState::ClipState Core::getItemState(const ObjectId &id)
{
    if (!m_guiConstructed) return PlaylistState::Disabled;
    switch (id.first) {
    case ObjectType::TimelineClip:
        if (m_mainWindow->getCurrentTimeline()->controller()->getModel()->isClip(id.second)) {
            return m_mainWindow->getCurrentTimeline()->controller()->getModel()->getClipState(id.second);
        }
        break;
    case ObjectType::TimelineComposition:
        return PlaylistState::VideoOnly;
        break;
    case ObjectType::BinClip:
        return m_binWidget->getClipState(id.second);
        break;
    case ObjectType::TimelineTrack:
        return m_mainWindow->getCurrentTimeline()->controller()->getModel()->isAudioTrack(id.second) ? PlaylistState::AudioOnly : PlaylistState::VideoOnly;
    case ObjectType::Master:
        return PlaylistState::Disabled;
        break;
    default:
        qDebug() << "ERROR: unhandled object type";
        break;
    }
    return PlaylistState::Disabled;
}

int Core::getItemDuration(const ObjectId &id)
{
    if (!m_guiConstructed) return 0;
    switch (id.first) {
    case ObjectType::TimelineClip:
        if (m_mainWindow->getCurrentTimeline()->controller()->getModel()->isClip(id.second)) {
            return m_mainWindow->getCurrentTimeline()->controller()->getModel()->getClipPlaytime(id.second);
        }
        break;
    case ObjectType::TimelineComposition:
        if (m_mainWindow->getCurrentTimeline()->controller()->getModel()->isComposition(id.second)) {
            return m_mainWindow->getCurrentTimeline()->controller()->getModel()->getCompositionPlaytime(id.second);
        }
        break;
    case ObjectType::BinClip:
        return (int)m_binWidget->getClipDuration(id.second);
        break;
    case ObjectType::TimelineTrack:
    case ObjectType::Master:
        return m_mainWindow->getCurrentTimeline()->controller()->duration() - 1;
    default:
        qDebug() << "ERROR: unhandled object type";
    }
    return 0;
}

int Core::getItemTrack(const ObjectId &id)
{
    if (!m_guiConstructed) return 0;
    switch (id.first) {
    case ObjectType::TimelineClip:
    case ObjectType::TimelineComposition:
        return m_mainWindow->getCurrentTimeline()->controller()->getModel()->getItemTrackId(id.second);
        break;
    default:
        qDebug() << "ERROR: unhandled object type";
    }
    return 0;
}

void Core::refreshProjectItem(const ObjectId &id)
{
    if (!m_guiConstructed || m_mainWindow->getCurrentTimeline()->loading) return;
    switch (id.first) {
    case ObjectType::TimelineClip:
        if (m_mainWindow->getCurrentTimeline()->controller()->getModel()->isClip(id.second)) {
            m_mainWindow->getCurrentTimeline()->controller()->refreshItem(id.second);
        }
        break;
    case ObjectType::TimelineComposition:
        if (m_mainWindow->getCurrentTimeline()->controller()->getModel()->isComposition(id.second)) {
            m_mainWindow->getCurrentTimeline()->controller()->refreshItem(id.second);
        }
        break;
    case ObjectType::TimelineTrack:
        if (m_mainWindow->getCurrentTimeline()->controller()->getModel()->isTrack(id.second)) {
            requestMonitorRefresh();
        }
        break;
    case ObjectType::BinClip:
        m_monitorManager->activateMonitor(Kdenlive::ClipMonitor);
        m_monitorManager->refreshClipMonitor();
        if (m_monitorManager->projectMonitorVisible() && m_mainWindow->getCurrentTimeline()->controller()->refreshIfVisible(id.second)) {
            m_monitorManager->refreshTimer.start();
        }
        break;
    case ObjectType::Master:
        requestMonitorRefresh();
        break;
    default:
        qDebug() << "ERROR: unhandled object type";
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
    m_projectManager->current()->setModified();;
}

int Core::projectDuration() const
{
    if (!m_guiConstructed) {
        return 0;
    }
    return m_mainWindow->getCurrentTimeline()->controller()->duration();
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

void Core::displayMessage(const QString &message, MessageType type, int timeout)
{
    if (m_mainWindow) {
        if (type == ProcessingJobMessage || type == OperationCompletedMessage) {
            emit m_mainWindow->displayProgressMessage(message, type, timeout);
        } else {
            emit m_mainWindow->displayMessage(message, type, timeout);
        }
    } else {
        qDebug() << message;
    }
}

void Core::displayBinMessage(const QString &text, int type, const QList<QAction *> &actions, bool showClose, BinMessage::BinCategory messageCategory)
{
    m_binWidget->doDisplayMessage(text, (KMessageWidget::MessageType)type, actions, showClose, messageCategory);
}

void Core::displayBinLogMessage(const QString &text, int type, const QString &logInfo)
{
    m_binWidget->doDisplayMessage(text, (KMessageWidget::MessageType)type, logInfo);
}

void Core::clearAssetPanel(int itemId)
{
    if (m_guiConstructed) emit m_mainWindow->clearAssetPanel(itemId);
}

std::shared_ptr<EffectStackModel> Core::getItemEffectStack(int itemType, int itemId)
{
    if (!m_guiConstructed) return nullptr;
    switch (itemType) {
    case (int)ObjectType::TimelineClip:
        return m_mainWindow->getCurrentTimeline()->controller()->getModel()->getClipEffectStack(itemId);
    case (int)ObjectType::TimelineTrack:
        return m_mainWindow->getCurrentTimeline()->controller()->getModel()->getTrackEffectStackModel(itemId);
        break;
    case (int)ObjectType::BinClip:
        return m_binWidget->getClipEffectStack(itemId);
    case (int)ObjectType::Master:
        return m_mainWindow->getCurrentTimeline()->controller()->getModel()->getMasterEffectStackModel();
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

void Core::invalidateRange(QSize range)
{
    if (!m_guiConstructed || m_mainWindow->getCurrentTimeline()->loading) return;
    m_mainWindow->getCurrentTimeline()->controller()->invalidateZone(range.width(), range.height());
}

void Core::invalidateItem(ObjectId itemId)
{
    if (!m_guiConstructed || !m_mainWindow->getCurrentTimeline() || m_mainWindow->getCurrentTimeline()->loading) return;
    switch (itemId.first) {
    case ObjectType::TimelineClip:
    case ObjectType::TimelineComposition:
        m_mainWindow->getCurrentTimeline()->controller()->invalidateItem(itemId.second);
        break;
    case ObjectType::TimelineTrack:
        m_mainWindow->getCurrentTimeline()->controller()->invalidateTrack(itemId.second);
        break;
    case ObjectType::BinClip:
        m_binWidget->invalidateClip(QString::number(itemId.second));
        break;
    case ObjectType::Master:
        m_mainWindow->getCurrentTimeline()->controller()->invalidateZone(0, -1);
        break;
    default:
        // compositions should not have effects
        break;
    }
}

double Core::getClipSpeed(int id) const
{
    return m_mainWindow->getCurrentTimeline()->controller()->getModel()->getClipSpeed(id);
}

void Core::updateItemKeyframes(ObjectId id)
{
    if (id.first == ObjectType::TimelineClip && m_guiConstructed) {
        m_mainWindow->getCurrentTimeline()->controller()->updateClip(id.second, {TimelineModel::KeyframesRole});
    }
}

void Core::updateItemModel(ObjectId id, const QString &service)
{
    if (m_guiConstructed && id.first == ObjectType::TimelineClip && !m_mainWindow->getCurrentTimeline()->loading && service.startsWith(QLatin1String("fade"))) {
        bool startFade = service == QLatin1String("fadein") || service == QLatin1String("fade_from_black");
        m_mainWindow->getCurrentTimeline()->controller()->updateClip(id.second, {startFade ? TimelineModel::FadeInRole : TimelineModel::FadeOutRole});
    }
}

void Core::showClipKeyframes(ObjectId id, bool enable)
{
    if (id.first == ObjectType::TimelineClip) {
        m_mainWindow->getCurrentTimeline()->controller()->showClipKeyframes(id.second, enable);
    } else if (id.first == ObjectType::TimelineComposition) {
        m_mainWindow->getCurrentTimeline()->controller()->showCompositionKeyframes(id.second, enable);
    }
}

Mlt::Profile *Core::thumbProfile()
{
    QMutexLocker lck(&m_thumbProfileMutex);
    if (!m_thumbProfile) {
        m_thumbProfile = std::make_unique<Mlt::Profile>(m_currentProfile.toStdString().c_str());
        double factor = 144. / m_thumbProfile->height();
        m_thumbProfile->set_height(144);
        int width = m_thumbProfile->width() * factor + 0.5;
        if (width % 2 > 0) {
            width ++;
        }
        m_thumbProfile->set_width(width);
    }
    return m_thumbProfile.get();
}

int Core::getTimelinePosition() const
{
    if (m_guiConstructed) {
        return m_monitorManager->projectMonitor()->position();
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

void Core::clean()
{
    m_self.reset();
}

void Core::startMediaCapture(int tid, bool checkAudio, bool checkVideo)
{
    if (checkAudio && checkVideo) {
        m_capture->recordVideo(tid, true);
    } else if (checkAudio) {
        m_capture->recordAudio(tid, true);
    }
    m_mediaCaptureFile = m_capture->getCaptureOutputLocation();
}

void Core::stopMediaCapture(int tid, bool checkAudio, bool checkVideo)
{
    if (checkAudio && checkVideo) {
        m_capture->recordVideo(tid, false);
    } else if (checkAudio) {
        m_capture->recordAudio(tid, false);
    }
}

QStringList Core::getAudioCaptureDevices()
{
    return m_capture->getAudioCaptureDevices();
}

int Core::getMediaCaptureState()
{
    return m_capture->getState();
}

bool Core::isMediaCapturing()
{
    return m_capture->isRecording();
}

MediaCapture *Core::getAudioDevice()
{
    return m_capture.get();
}

QString Core::getProjectFolderName()
{
    if (currentDoc()) {
        return currentDoc()->projectDataFolder() + QDir::separator();
    }
    return QString();
}

QString Core::getTimelineClipBinId(int cid)
{
    if (!m_guiConstructed) {
        return QString();
    }
    if (m_mainWindow->getCurrentTimeline()->controller()->getModel()->isClip(cid)) {
        return m_mainWindow->getCurrentTimeline()->controller()->getModel()->getClipBinId(cid);
    }
    return QString();
}

int Core::getDurationFromString(const QString &time)
{
    if (!m_guiConstructed) {
        return 0;
    }
    const QString duration = currentDoc()->timecode().reformatSeparators(time);
    return currentDoc()->timecode().getFrameCount(duration);
}

void Core::processInvalidFilter(const QString service, const QString id, const QString message)
{
    if (m_guiConstructed) emit m_mainWindow->assetPanelWarning(service, id, message);
}

void Core::updateProjectTags(QMap <QString, QString> tags)
{
    // Clear previous tags
    for (int i = 1 ; i< 20; i++) {
        QString current = currentDoc()->getDocumentProperty(QString("tag%1").arg(i));
        if (current.isEmpty()) {
            break;
        } else {
            currentDoc()->setDocumentProperty(QString("tag%1").arg(i), QString());
        }
    }
    QMapIterator<QString, QString> j(tags);
    int i = 1;
    while (j.hasNext()) {
        j.next();
        currentDoc()->setDocumentProperty(QString("tag%1").arg(i), QString("%1:%2").arg(j.key(), j.value()));
        i++;
    }
}

std::unique_ptr<Mlt::Producer> Core::getMasterProducerInstance()
{
    if (m_guiConstructed && m_mainWindow->getCurrentTimeline()) {
        std::unique_ptr<Mlt::Producer> producer(m_mainWindow->getCurrentTimeline()->controller()->tractor()->cut(0, m_mainWindow->getCurrentTimeline()->controller()->duration() - 1));
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
        pCore->window()->getMainTimeline()->controller()->slotMultitrackView(enable, enable);
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

void Core::addGuides(QList <int> guides)
{
    QMap <GenTime, QString> markers;
    for (int pos : guides) {
        GenTime p(pos, pCore->getCurrentFps());
        markers.insert(p, pCore->currentDoc()->timecode().getDisplayTimecode(p, false));
    }
    pCore->currentDoc()->getGuideModel()->addMarkers(markers);
}
