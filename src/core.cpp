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
#include "doc/docundostack.hpp"
#include "doc/kdenlivedoc.h"
#include "jobs/jobmanager.h"
#include "kdenlive_debug.h"
#include "kdenlivesettings.h"
#include "library/librarywidget.h"
#include "mainwindow.h"
#include "mltconnection.h"
#include "mltcontroller/bincontroller.h"
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

#include <mlt++/MltRepository.h>

#include <locale>
#ifdef Q_OS_MAC
#include <xlocale.h>
#endif

std::unique_ptr<Core> Core::m_self;
Core::Core()
    : m_mainWindow(nullptr)
    , m_projectManager(nullptr)
    , m_monitorManager(nullptr)
    , m_binWidget(nullptr)
    , m_library(nullptr)
    , m_thumbProfile(nullptr)
{
}

void Core::prepareShutdown()
{
    m_guiConstructed = false;
}

Core::~Core()
{
    if (m_monitorManager) {
        delete m_monitorManager;
    }
    m_binController->destroyBin();
    // delete m_binWidget;
    delete m_projectManager;
}

void Core::build(const QString &MltPath)
{
    if (m_self) {
        qDebug() << "DEBUG: Warning : trying to create a second Core";
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

    // Open connection with Mlt
    MltConnection::construct(MltPath);

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
    // Job manger must be created before bin to correctly connect
    m_self->m_jobManager.reset(new JobManager(m_self.get()));
}

void Core::initGUI(const QUrl &Url)
{
    m_guiConstructed = true;
    m_profile = KdenliveSettings::default_profile();
    m_currentProfile = m_profile;
    profileChanged();
    m_mainWindow = new MainWindow();

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
        for (const auto &profile : all_profiles) {
            all_descriptions << profile.first;
        }

        // ask the user
        bool ok;
        QString item = QInputDialog::getItem(m_mainWindow, i18n("Select Default Profile"), i18n("Profile:"), all_descriptions, 0, false, &ok);
        if (ok) {
            ok = false;
            for (const auto &profile : all_profiles) {
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
    m_binWidget = new Bin(m_projectItemModel);
    m_binController = std::make_shared<BinController>();
    m_library = new LibraryWidget(m_projectManager);
    connect(m_library, SIGNAL(addProjectClips(QList<QUrl>)), m_binWidget, SLOT(droppedUrls(QList<QUrl>)));
    connect(this, &Core::updateLibraryPath, m_library, &LibraryWidget::slotUpdateLibraryPath);
    connect(m_binWidget, &Bin::storeFolder, m_binController.get(),
            &BinController::slotStoreFolder);
    // connect(m_binController.get(), &BinController::slotProducerReady, m_binWidget, &Bin::slotProducerReady, Qt::DirectConnection);
    // connect(m_binController.get(), &BinController::prepareTimelineReplacement, m_binWidget, &Bin::prepareTimelineReplacement, Qt::DirectConnection);

    // connect(m_binController.get(), &BinController::requestAudioThumb, m_binWidget, &Bin::slotCreateAudioThumb);
    connect(m_binController.get(), &BinController::abortAudioThumbs, m_binWidget, &Bin::abortAudioThumbs);
    connect(m_binController.get(), &BinController::setDocumentNotes, m_projectManager, &ProjectManager::setDocumentNotes);
    m_monitorManager = new MonitorManager(this);
    // Producer queue, creating MLT::Producers on request
    /*
    m_producerQueue = new ProducerQueue(m_binController);
    connect(m_producerQueue, &ProducerQueue::gotFileProperties, m_binWidget, &Bin::slotProducerReady);
    connect(m_producerQueue, &ProducerQueue::replyGetImage, m_binWidget, &Bin::slotThumbnailReady);
    connect(m_producerQueue, &ProducerQueue::removeInvalidClip, m_binWidget, &Bin::slotRemoveInvalidClip, Qt::DirectConnection);
    connect(m_producerQueue, SIGNAL(addClip(QString, QMap<QString, QString>)), m_binWidget, SLOT(slotAddUrl(QString, QMap<QString, QString>)));
    connect(m_binController.get(), SIGNAL(createThumb(QDomElement, QString, int)), m_producerQueue, SLOT(getFileProperties(QDomElement, QString, int)));
    connect(m_binWidget, &Bin::producerReady, m_producerQueue, &ProducerQueue::slotProcessingDone, Qt::DirectConnection);
    // TODO
    connect(m_producerQueue, SIGNAL(removeInvalidProxy(QString,bool)), m_binWidget, SLOT(slotRemoveInvalidProxy(QString,bool)));*/

    m_mainWindow->init();
    projectManager()->init(Url, QString());
    QTimer::singleShot(0, pCore->projectManager(), &ProjectManager::slotLoadOnOpen);
    if (qApp->isSessionRestored()) {
        // NOTE: we are restoring only one window, because Kdenlive only uses one MainWindow
        m_mainWindow->restore(1, false);
    }
    m_mainWindow->show();
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

std::shared_ptr<BinController> Core::binController()
{
    return m_binController;
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

void Core::initLocale()
{
    QLocale systemLocale = QLocale();
#ifndef Q_OS_MAC
    setlocale(LC_NUMERIC, nullptr);
#else
    setlocale(LC_NUMERIC_MASK, nullptr);
#endif

// localeconv()->decimal_point does not give reliable results on Windows
#ifndef Q_OS_WIN
    char *separator = localeconv()->decimal_point;
    if (QString::fromUtf8(separator) != QChar(systemLocale.decimalPoint())) {
        // qCDebug(KDENLIVE_LOG)<<"------\n!!! system locale is not similar to Qt's locale... be prepared for bugs!!!\n------";
        // HACK: There is a locale conflict, so set locale to C
        // Make sure to override exported values or it won't work
        qputenv("LANG", "C");
#ifndef Q_OS_MAC
        setlocale(LC_NUMERIC, "C");
#else
        setlocale(LC_NUMERIC_MASK, "C");
#endif
        systemLocale = QLocale::c();
    }
#endif

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

const QString &Core::getCurrentProfilePath() const
{
    return m_currentProfile;
}

bool Core::setCurrentProfile(const QString &profilePath)
{
    if (m_currentProfile == profilePath) {
        // no change required
        return true;
    }
    if (ProfileRepository::get()->profileExists(profilePath)) {
        m_currentProfile = profilePath;
        m_thumbProfile.reset();
        // inform render widget
        m_mainWindow->updateRenderWidgetProfile();
        return true;
    }
    return false;
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
    return QSize((int)(getCurrentProfile()->height() * getCurrentDar() + 0.5), getCurrentProfile()->height());
}

QSize Core::getCurrentFrameSize() const
{
    return QSize(getCurrentProfile()->width(), getCurrentProfile()->height());
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
        return 0;
        break;
    default:
        qDebug() << "ERROR: unhandled object type";
    }
    return 0;
}

int Core::getItemIn(const ObjectId &id)
{
    if (!m_guiConstructed) return 0;
    switch (id.first) {
    case ObjectType::TimelineClip:
        if (m_mainWindow->getCurrentTimeline()->controller()->getModel()->isClip(id.second)) {
            return m_mainWindow->getCurrentTimeline()->controller()->getModel()->getClipIn(id.second);
        }
        break;
    case ObjectType::TimelineComposition:
        return 0;
        break;
    case ObjectType::BinClip:
        return 0;
        break;
    default:
        qDebug() << "ERROR: unhandled object type";
    }
    return 0;
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
        return m_binWidget->getClipDuration(id.second);
        break;
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
        m_monitorManager->refreshClipMonitor();
        break;
    default:
        qDebug() << "ERROR: unhandled object type";
    }
}

KdenliveDoc *Core::currentDoc()
{
    return m_projectManager->current();
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

void Core::displayMessage(const QString &message, MessageType type, int timeout)
{
    m_mainWindow->displayMessage(message, type, timeout);
}

void Core::clearAssetPanel(int itemId)
{
    if (m_guiConstructed)
        m_mainWindow->clearAssetPanel(itemId);
}

void Core::adjustAssetRange(int itemId, int in, int out)
{
    if (m_guiConstructed)
        m_mainWindow->adjustAssetPanelRange(itemId, in, out);
}

std::shared_ptr<EffectStackModel> Core::getItemEffectStack(int itemType, int itemId)
{
    if (!m_guiConstructed) return nullptr;
    switch (itemType) {
    case (int)ObjectType::TimelineClip:
        return m_mainWindow->getCurrentTimeline()->controller()->getModel()->getClipEffectStack(itemId);
    case (int)ObjectType::TimelineTrack:
        // TODO
        return nullptr;
        break;
    case (int)ObjectType::BinClip:
        return m_binWidget->getClipEffectStack(itemId);
    default:
        return nullptr;
    }
}

std::shared_ptr<DocUndoStack> Core::undoStack()
{
    return projectManager()->undoStack();
}

QMap<int, QString> Core::getVideoTrackNames()
{
    if (!m_guiConstructed) return QMap<int, QString>();
    return m_mainWindow->getCurrentTimeline()->controller()->getTrackNames(true);
}

QPair<int, int> Core::getCompositionATrack(int cid) const
{
    if (!m_guiConstructed) return QPair<int, int>();
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

void Core::invalidateItem(ObjectId itemId)
{
    if (!m_mainWindow || m_mainWindow->getCurrentTimeline()->loading) return;
    switch (itemId.first) {
    case ObjectType::TimelineClip:
        m_mainWindow->getCurrentTimeline()->controller()->invalidateClip(itemId.second);
        break;
    case ObjectType::TimelineTrack:
        // TODO: invalidate all clips in track
        break;
    default:
        // bin clip should automatically be reloaded, compositions should not have effects
        break;
    }
}

double Core::getClipSpeed(int id) const
{
    return m_mainWindow->getCurrentTimeline()->controller()->getModel()->getClipSpeed(id);
}

void Core::updateItemKeyframes(ObjectId id)
{
    if (id.first == ObjectType::TimelineClip && m_mainWindow) {
        m_mainWindow->getCurrentTimeline()->controller()->updateClip(id.second, {TimelineModel::KeyframesRole});
    }
}

void Core::updateItemModel(ObjectId id, const QString &service)
{
    if (!m_mainWindow->getCurrentTimeline()->loading && service.startsWith(QLatin1String("fade")) && id.first == ObjectType::TimelineClip) {
        bool startFade = service == QLatin1String("fadein") || service == QLatin1String("fade_from_black");
        m_mainWindow->getCurrentTimeline()->controller()->updateClip(id.second, {startFade ?TimelineModel::FadeInRole : TimelineModel::FadeOutRole});
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
    if (!m_thumbProfile) {
        m_thumbProfile = std::unique_ptr<Mlt::Profile>(new Mlt::Profile(m_currentProfile.toStdString().c_str()));
        m_thumbProfile->set_height(200);
        int width = 200 * m_thumbProfile->dar();
        width += width % 8;
        m_thumbProfile->set_width(width);
    }
    return m_thumbProfile.get();
}
