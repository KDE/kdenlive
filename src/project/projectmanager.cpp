/*
SPDX-FileCopyrightText: 2014 Till Theato <root@ttill.de>
SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "projectmanager.h"
#include "bin/bin.h"
#include "bin/projectclip.h"
#include "bin/projectitemmodel.h"
#include "core.h"
#include "doc/docundostack.hpp"
#include "doc/kdenlivedoc.h"
#include "jobs/cliploadtask.h"
#include "kdenlivesettings.h"
#include "mainwindow.h"
#include "monitor/monitormanager.h"
#include "monitor/monitorproxy.h"
#include "profiles/profilemodel.hpp"
#include "project/dialogs/archivewidget.h"
#include "project/dialogs/backupwidget.h"
#include "project/dialogs/noteswidget.h"
#include "project/dialogs/projectsettings.h"
#include "timeline2/model/timelinefunctions.hpp"
#include "utils/qstringutils.h"
#include "utils/thumbnailcache.hpp"
#include "xml/xml.hpp"
#include <audiomixer/mixermanager.hpp>
#include <bin/clipcreator.hpp>
#include <lib/localeHandling.h>

// Temporary for testing
#include "bin/model/markerlistmodel.hpp"

#include "profiles/profilerepository.hpp"
#include "project/notesplugin.h"
#include "timeline2/model/builders/meltBuilder.hpp"
#include "timeline2/view/timelinecontroller.h"
#include "timeline2/view/timelinewidget.h"

#include "utils/KMessageBox_KdenliveCompat.h"
#include <KActionCollection>
#include <KConfigGroup>
#include <KJob>
#include <KJobWidgets>
#include <KLocalizedString>
#include <KMessageBox>
#include <KNotification>
#include <KRecentDirs>
#include <kcoreaddons_version.h>

#include "kdenlive_debug.h"
#include <QAction>
#include <QCryptographicHash>
#include <QFileDialog>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocale>
#include <QMimeDatabase>
#include <QMimeType>
#include <QProgressDialog>
#include <QSaveFile>
#include <QTimeZone>
#include <QUndoGroup>

static QString getProjectNameFilters(bool ark = true)
{
    QString filter = i18n("Kdenlive Project") + QStringLiteral(" (*.kdenlive)");
    if (ark) {
        filter.append(";;" + i18n("Archived Project") + QStringLiteral(" (*.tar.gz *.zip)"));
    }
    return filter;
}

ProjectManager::ProjectManager(QObject *parent)
    : QObject(parent)
    , m_activeTimelineModel(nullptr)
    , m_notesPlugin(nullptr)
{
    // Ensure the default data folder exist
    QDir dir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation));
    dir.mkpath(QStringLiteral(".backup"));
    dir.mkdir(QStringLiteral("titles"));
}

ProjectManager::~ProjectManager() = default;

void ProjectManager::slotLoadOnOpen()
{
    if (m_startUrl.isValid()) {
        openFile();
    } else if (KdenliveSettings::openlastproject()) {
        openLastFile();
    } else {
        newFile(false);
    }
    if (!m_loadClipsOnOpen.isEmpty() && (m_project != nullptr)) {
        const QStringList list = m_loadClipsOnOpen.split(QLatin1Char(','));
        QList<QUrl> urls;
        urls.reserve(list.count());
        for (const QString &path : list) {
            // qCDebug(KDENLIVE_LOG) << QDir::current().absoluteFilePath(path);
            urls << QUrl::fromLocalFile(QDir::current().absoluteFilePath(path));
        }
        pCore->bin()->droppedUrls(urls);
    }
    m_loadClipsOnOpen.clear();
    Q_EMIT pCore->closeSplash();
    // Release startup crash lock file
    QFile lockFile(QDir::temp().absoluteFilePath(QStringLiteral("kdenlivelock")));
    lockFile.remove();
    // For some reason Qt seems to be doing some stuff that modifies the tabs text after window is shown, so use a timer
    QTimer::singleShot(1000, this, []() {
        QList<QTabBar *> tabbars = pCore->window()->findChildren<QTabBar *>();
        for (QTabBar *tab : qAsConst(tabbars)) {
            // Fix tabbar tooltip containing ampersand
            for (int i = 0; i < tab->count(); i++) {
                tab->setTabToolTip(i, tab->tabText(i).replace('&', ""));
            }
        }
        pCore->window()->checkMaxCacheSize();
    });
}

void ProjectManager::slotLoadHeadless(const QUrl &projectUrl)
{
    if (projectUrl.isValid()) {
        doOpenFileHeadless(projectUrl);
    }
    // Release startup crash lock file
    QFile lockFile(QDir::temp().absoluteFilePath(QStringLiteral("kdenlivelock")));
    lockFile.remove();
}

void ProjectManager::init(const QUrl &projectUrl, const QString &clipList)
{
    m_startUrl = projectUrl;
    m_loadClipsOnOpen = clipList;
    m_fileRevert = KStandardAction::revert(this, SLOT(slotRevert()), pCore->window()->actionCollection());
    m_fileRevert->setIcon(QIcon::fromTheme(QStringLiteral("document-revert")));
    m_fileRevert->setEnabled(false);

    QAction *a = KStandardAction::open(this, SLOT(openFile()), pCore->window()->actionCollection());
    a->setIcon(QIcon::fromTheme(QStringLiteral("document-open")));
    a = KStandardAction::saveAs(this, SLOT(saveFileAs()), pCore->window()->actionCollection());
    a->setIcon(QIcon::fromTheme(QStringLiteral("document-save-as")));
    a = KStandardAction::openNew(this, SLOT(newFile()), pCore->window()->actionCollection());
    a->setIcon(QIcon::fromTheme(QStringLiteral("document-new")));
    m_recentFilesAction = KStandardAction::openRecent(this, SLOT(openFile(QUrl)), pCore->window()->actionCollection());

    QAction *saveCopyAction = new QAction(QIcon::fromTheme(QStringLiteral("document-save-as")), i18n("Save Copy…"), this);
    pCore->window()->addAction(QStringLiteral("file_save_copy"), saveCopyAction);
    connect(saveCopyAction, &QAction::triggered, this, [this] { saveFileAs(true); });

    QAction *backupAction = new QAction(QIcon::fromTheme(QStringLiteral("edit-undo")), i18n("Open Backup File…"), this);
    pCore->window()->addAction(QStringLiteral("open_backup"), backupAction);
    connect(backupAction, SIGNAL(triggered(bool)), SLOT(slotOpenBackup()));
    m_notesPlugin = new NotesPlugin(this);

    m_autoSaveTimer.setSingleShot(true);
    m_autoSaveTimer.setInterval(3000);
    connect(&m_autoSaveTimer, &QTimer::timeout, this, &ProjectManager::slotAutoSave);
}

void ProjectManager::newFile(bool showProjectSettings)
{
    QString profileName = KdenliveSettings::default_profile();
    if (profileName.isEmpty()) {
        profileName = pCore->getCurrentProfile()->path();
    }
    newFile(profileName, showProjectSettings);
}

void ProjectManager::newFile(QString profileName, bool showProjectSettings)
{
    QUrl startFile = QUrl::fromLocalFile(KdenliveSettings::defaultprojectfolder() + QStringLiteral("/_untitled.kdenlive"));
    if (checkForBackupFile(startFile, true)) {
        return;
    }
    m_fileRevert->setEnabled(false);
    QString projectFolder;
    QMap<QString, QString> documentProperties;
    QMap<QString, QString> documentMetadata;
    std::pair<int, int> projectTracks{KdenliveSettings::videotracks(), KdenliveSettings::audiotracks()};
    int audioChannels = 2;
    if (KdenliveSettings::audio_channels() == 1) {
        audioChannels = 4;
    } else if (KdenliveSettings::audio_channels() == 2) {
        audioChannels = 6;
    }
    pCore->monitorManager()->resetDisplay();
    QString documentId = QString::number(QDateTime::currentMSecsSinceEpoch());
    documentProperties.insert(QStringLiteral("documentid"), documentId);
    bool sameProjectFolder = KdenliveSettings::sameprojectfolder();
    if (!showProjectSettings) {
        if (!closeCurrentDocument()) {
            return;
        }
        if (KdenliveSettings::customprojectfolder()) {
            projectFolder = KdenliveSettings::defaultprojectfolder();
            QDir folder(projectFolder);
            if (!projectFolder.endsWith(QLatin1Char('/'))) {
                projectFolder.append(QLatin1Char('/'));
            }
            documentProperties.insert(QStringLiteral("storagefolder"), folder.absoluteFilePath(documentId));
        }
    } else {
        QPointer<ProjectSettings> w = new ProjectSettings(nullptr, QMap<QString, QString>(), QStringList(), projectTracks.first, projectTracks.second,
                                                          audioChannels, KdenliveSettings::defaultprojectfolder(), false, true, pCore->window());
        connect(w.data(), &ProjectSettings::refreshProfiles, pCore->window(), &MainWindow::slotRefreshProfiles);
        if (w->exec() != QDialog::Accepted) {
            delete w;
            return;
        }
        if (!closeCurrentDocument()) {
            delete w;
            return;
        }
        if (KdenliveSettings::videothumbnails() != w->enableVideoThumbs()) {
            pCore->window()->slotSwitchVideoThumbs();
        }
        if (KdenliveSettings::audiothumbnails() != w->enableAudioThumbs()) {
            pCore->window()->slotSwitchAudioThumbs();
        }
        profileName = w->selectedProfile();
        projectFolder = w->storageFolder();
        projectTracks = w->tracks();
        audioChannels = w->audioChannels();
        documentProperties.insert(QStringLiteral("enableproxy"), QString::number(int(w->useProxy())));
        documentProperties.insert(QStringLiteral("generateproxy"), QString::number(int(w->generateProxy())));
        documentProperties.insert(QStringLiteral("proxyminsize"), QString::number(w->proxyMinSize()));
        documentProperties.insert(QStringLiteral("proxyparams"), w->proxyParams());
        documentProperties.insert(QStringLiteral("proxyextension"), w->proxyExtension());
        documentProperties.insert(QStringLiteral("proxyresize"), QString::number(w->proxyResize()));
        documentProperties.insert(QStringLiteral("audioChannels"), QString::number(w->audioChannels()));
        documentProperties.insert(QStringLiteral("generateimageproxy"), QString::number(int(w->generateImageProxy())));
        QString preview = w->selectedPreview();
        if (!preview.isEmpty()) {
            documentProperties.insert(QStringLiteral("previewparameters"), preview.section(QLatin1Char(';'), 0, 0));
            documentProperties.insert(QStringLiteral("previewextension"), preview.section(QLatin1Char(';'), 1, 1));
        }
        documentProperties.insert(QStringLiteral("proxyimageminsize"), QString::number(w->proxyImageMinSize()));
        if (!projectFolder.isEmpty()) {
            if (!projectFolder.endsWith(QLatin1Char('/'))) {
                projectFolder.append(QLatin1Char('/'));
            }
            documentProperties.insert(QStringLiteral("storagefolder"), projectFolder + documentId);
        }
        if (w->useExternalProxy()) {
            documentProperties.insert(QStringLiteral("enableexternalproxy"), QStringLiteral("1"));
            documentProperties.insert(QStringLiteral("externalproxyparams"), w->externalProxyParams());
        }
        sameProjectFolder = w->docFolderAsStorageFolder();
        // Metadata
        documentMetadata = w->metadata();
        delete w;
    }
    m_notesPlugin->clear();
    KdenliveDoc *doc = new KdenliveDoc(projectFolder, pCore->window()->m_commandStack, profileName, documentProperties, documentMetadata, projectTracks, audioChannels, pCore->window());
    doc->m_autosave = new KAutoSaveFile(startFile, doc);
    doc->m_sameProjectFolder = sameProjectFolder;
    ThumbnailCache::get()->clearCache();
    pCore->bin()->setDocument(doc);
    m_project = doc;
    initSequenceProperties(m_project->uuid(), {KdenliveSettings::audiotracks(), KdenliveSettings::videotracks()});
    updateTimeline(true, QString(), QString(), QDateTime(), 0);
    pCore->window()->connectDocument();
    bool disabled = m_project->getDocumentProperty(QStringLiteral("disabletimelineeffects")) == QLatin1String("1");
    QAction *disableEffects = pCore->window()->actionCollection()->action(QStringLiteral("disable_timeline_effects"));
    if (disableEffects) {
        if (disabled != disableEffects->isChecked()) {
            disableEffects->blockSignals(true);
            disableEffects->setChecked(disabled);
            disableEffects->blockSignals(false);
        }
    }
    activateDocument(m_project->activeUuid);
    std::shared_ptr<ProjectClip> mainClip = pCore->projectItemModel()->getSequenceClip(m_project->uuid());
    if (mainClip) {
        mainClip->reloadTimeline(m_activeTimelineModel->getMasterEffectStackModel());
    }
    Q_EMIT docOpened(m_project);
    Q_EMIT pCore->gotMissingClipsCount(0, 0);
    m_project->loading = false;
    m_lastSave.start();
    if (pCore->monitorManager()) {
        Q_EMIT pCore->monitorManager()->updatePreviewScaling();
        pCore->monitorManager()->projectMonitor()->slotActivateMonitor();
        pCore->monitorManager()->projectMonitor()->setProducer(m_activeTimelineModel->producer(), 0);
        const QUuid uuid = m_project->activeUuid;
        pCore->monitorManager()->projectMonitor()->adjustRulerSize(m_activeTimelineModel->duration() - 1, m_project->getFilteredGuideModel(uuid));
    }
}

void ProjectManager::setActiveTimeline(const QUuid &uuid)
{
    m_activeTimelineModel = m_project->getTimeline(uuid);
    m_project->activeUuid = uuid;
}

void ProjectManager::activateDocument(const QUuid &uuid)
{
    qDebug() << "===== ACTIVATING DOCUMENT: " << uuid << "\n::::::::::::::::::::::";
    /*if (m_project && (m_project->uuid() == uuid)) {
        auto match = m_timelineModels.find(uuid.toString());
        if (match == m_timelineModels.end()) {
            qDebug()<<"=== ERROR";
            return;
        }
        m_mainTimelineModel = match->second;
        pCore->window()->raiseTimeline(uuid);
        qDebug()<<"=== ERROR 2";
        return;
    }*/
    // Q_ASSERT(m_openedDocuments.contains(uuid));
    /*m_project = m_openedDocuments.value(uuid);
    m_fileRevert->setEnabled(m_project->isModified());
    m_notesPlugin->clear();
    Q_EMIT docOpened(m_project);*/

    m_activeTimelineModel = m_project->getTimeline(uuid);
    m_project->activeUuid = uuid;

    /*pCore->bin()->setDocument(m_project);
    pCore->window()->connectDocument();*/
    pCore->window()->raiseTimeline(uuid);
    pCore->window()->slotSwitchTimelineZone(m_project->getDocumentProperty(QStringLiteral("enableTimelineZone")).toInt() == 1);
    pCore->window()->slotSetZoom(m_project->zoom(uuid).x());
    // Q_EMIT pCore->monitorManager()->updatePreviewScaling();
    // pCore->monitorManager()->projectMonitor()->slotActivateMonitor();
}

void ProjectManager::testSetActiveDocument(KdenliveDoc *doc, std::shared_ptr<TimelineItemModel> timeline)
{
    m_project = doc;
    if (timeline == nullptr) {
        // New nested document format, build timeline model now
        const QUuid uuid = m_project->uuid();
        timeline = TimelineItemModel::construct(uuid, m_project->commandStack());
        std::shared_ptr<Mlt::Tractor> tc = pCore->projectItemModel()->getExtraTimeline(uuid.toString());
        if (!constructTimelineFromTractor(timeline, nullptr, *tc.get(), m_project->modifiedDecimalPoint(), QString(), QString())) {
            qDebug() << "===== LOADING PROJECT INTERNAL ERROR";
        }
    }
    const QUuid uuid = timeline->uuid();
    m_project->addTimeline(uuid, timeline);
    timeline->isClosed = false;
    m_activeTimelineModel = timeline;
    m_project->activeUuid = uuid;
    std::shared_ptr<ProjectClip> mainClip = pCore->projectItemModel()->getClipByBinID(pCore->projectItemModel()->getSequenceId(uuid));
    if (mainClip) {
        if (timeline->getGuideModel() == nullptr) {
            timeline->setMarkerModel(mainClip->markerModel());
        }
        m_project->loadSequenceGroupsAndGuides(uuid);
    }
    // Open all other timelines
    QMap<QUuid, QString> allSequences = pCore->projectItemModel()->getAllSequenceClips();
    QMapIterator<QUuid, QString> i(allSequences);
    while (i.hasNext()) {
        i.next();
        if (m_project->getTimeline(i.key(), true) == nullptr) {
            const QUuid uid = i.key();
            std::shared_ptr<Mlt::Tractor> tc = pCore->projectItemModel()->getExtraTimeline(uid.toString());
            if (tc) {
                std::shared_ptr<TimelineItemModel> timelineModel = TimelineItemModel::construct(uid, m_project->commandStack());
                const QString chunks = m_project->getSequenceProperty(uid, QStringLiteral("previewchunks"));
                const QString dirty = m_project->getSequenceProperty(uid, QStringLiteral("dirtypreviewchunks"));
                if (constructTimelineFromTractor(timelineModel, nullptr, *tc.get(), m_project->modifiedDecimalPoint(), chunks, dirty)) {
                    m_project->addTimeline(uid, timelineModel, false);
                    pCore->projectItemModel()->setExtraTimelineSaved(uid.toString());
                    std::shared_ptr<Mlt::Producer> prod = std::make_shared<Mlt::Producer>(timelineModel->tractor());
                    passSequenceProperties(uid, prod, *tc.get(), timelineModel, nullptr);
                    std::shared_ptr<ProjectClip> clip = pCore->projectItemModel()->getClipByBinID(i.value());
                    prod->parent().set("kdenlive:clipname", clip->clipName().toUtf8().constData());
                    prod->set("kdenlive:description", clip->description().toUtf8().constData());
                    // Store sequence properties for later re-use
                    if (timelineModel->getGuideModel() == nullptr) {
                        timelineModel->setMarkerModel(clip->markerModel());
                    }
                    m_project->loadSequenceGroupsAndGuides(uid);
                    clip->setProducer(prod, false, false);
                    clip->reloadTimeline(timelineModel->getMasterEffectStackModel());
                }
            }
        }
    }
}

std::shared_ptr<TimelineItemModel> ProjectManager::getTimeline()
{
    return m_activeTimelineModel;
}

bool ProjectManager::testSaveFileAs(const QString &outputFileName)
{
    QString saveFolder = QFileInfo(outputFileName).absolutePath();
    m_project->setDocumentProperty(QStringLiteral("opensequences"), m_project->uuid().toString());
    m_project->setDocumentProperty(QStringLiteral("activetimeline"), m_project->uuid().toString());

    QMap<QString, QString> docProperties = m_project->documentProperties(true);
    pCore->projectItemModel()->saveDocumentProperties(docProperties, QMap<QString, QString>());
    // QString scene = m_activeTimelineModel->sceneList(saveFolder);
    int duration = m_activeTimelineModel->duration();
    QString scene = pCore->projectItemModel()->sceneList(saveFolder, QString(), QString(), m_activeTimelineModel->tractor(), duration);
    if (scene.isEmpty()) {
        qDebug() << "//////  ERROR writing EMPTY scene list to file: " << outputFileName;
        return false;
    }
    QSaveFile file(outputFileName);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qDebug() << "//////  ERROR writing to file: " << outputFileName;
        return false;
    }

    file.write(scene.toUtf8());
    if (!file.commit()) {
        qDebug() << "Cannot write to file %1";
        return false;
    }
    qDebug() << "------------\nSAVED FILE AS: " << outputFileName << "\n==============";
    return true;
}

bool ProjectManager::closeCurrentDocument(bool saveChanges, bool quit)
{
    // Disable autosave
    m_autoSaveTimer.stop();
    if ((m_project != nullptr) && m_project->isModified() && saveChanges) {
        QString message;
        if (m_project->url().fileName().isEmpty()) {
            message = i18n("Save changes to document?");
        } else {
            message = i18n("The project <b>\"%1\"</b> has been changed.\nDo you want to save your changes?", m_project->url().fileName());
        }

        switch (KMessageBox::warningTwoActionsCancel(pCore->window(), message, {}, KStandardGuiItem::save(), KStandardGuiItem::dontSave())) {
        case KMessageBox::PrimaryAction:
            // save document here. If saving fails, return false;
            if (!saveFile()) {
                return false;
            }
            break;
        case KMessageBox::Cancel:
            return false;
            break;
        default:
            break;
        }
    }

    // Abort clip loading if any
    Q_EMIT pCore->stopProgressTask();
    qApp->processEvents();
    bool guiConstructed = pCore->window() != nullptr;
    if (guiConstructed) {
        pCore->window()->disableMulticam();
        Q_EMIT pCore->window()->clearAssetPanel();
        pCore->mixer()->unsetModel();
        pCore->monitorManager()->clipMonitor()->slotOpenClip(nullptr);
        pCore->monitorManager()->projectMonitor()->setProducer(nullptr);
    }
    if (m_project) {
        pCore->taskManager.slotCancelJobs(true);
        m_project->closing = true;
        if (m_activeTimelineModel) {
            m_activeTimelineModel->m_closing = true;
        }
        if (guiConstructed && !quit && !qApp->isSavingSession()) {
            pCore->bin()->abortOperations();
        }
        m_project->commandStack()->clear();
        pCore->cleanup();
        if (guiConstructed) {
            pCore->monitorManager()->clipMonitor()->getControllerProxy()->documentClosed();
            const QList<QUuid> uuids = m_project->getTimelinesUuids();
            for (auto &uid : uuids) {
                pCore->window()->closeTimelineTab(uid);
                pCore->window()->resetSubtitles(uid);
                m_project->closeTimeline(uid, true);
            }
        } else {
            // Close all timelines
            const QList<QUuid> uuids = m_project->getTimelinesUuids();
            for (auto &uid : uuids) {
                m_project->closeTimeline(uid, true);
            }
        }
    }
    // Ensure we don't have stuck references to timelinemodel
    // qDebug() << "TIMELINEMODEL COUNTS: " << m_activeTimelineModel.use_count();
    // Q_ASSERT(m_activeTimelineModel.use_count() <= 1);
    m_activeTimelineModel.reset();
    // Release model shared pointers
    if (guiConstructed) {
        pCore->bin()->cleanDocument();
        delete m_project;
        m_project = nullptr;
    } else {
        pCore->projectItemModel()->clean();
        m_project = nullptr;
    }
    mlt_service_cache_set_size(nullptr, "producer_avformat", 0);
    ::mlt_pool_purge();
    return true;
}

bool ProjectManager::saveFileAs(const QString &outputFileName, bool saveOverExistingFile, bool saveACopy)
{
    pCore->monitorManager()->pauseActiveMonitor();
    QString oldProjectFolder =
        m_project->url().isEmpty() ? QString() : QFileInfo(m_project->url().toLocalFile()).absolutePath() + QStringLiteral("/cachefiles");
    // this was the old project folder in case the "save in project file location" setting was active

    // Sync document properties
    if (!saveACopy && outputFileName != m_project->url().toLocalFile()) {
        // Project filename changed
        pCore->window()->updateProjectPath(outputFileName);
    }
    // If current sequence was modified and is used in another sequence, ensure to sync it
    const QUuid activeUuid = m_activeTimelineModel->uuid();
    if (m_project->sequenceThumbRequiresRefresh(activeUuid)) {
        pCore->bin()->updateSequenceClip(activeUuid, m_activeTimelineModel->duration(), -1);
    }
    prepareSave();
    QString saveFolder = QFileInfo(outputFileName).absolutePath();
    m_project->updateWorkFilesBeforeSave(outputFileName);
    checkProjectIntegrity();
    QString scene = projectSceneList(saveFolder);
    if (!m_replacementPattern.isEmpty()) {
        QMapIterator<QString, QString> i(m_replacementPattern);
        while (i.hasNext()) {
            i.next();
            scene.replace(i.key(), i.value());
        }
    }
    m_project->updateWorkFilesAfterSave();
    if (!m_project->saveSceneList(outputFileName, scene, saveOverExistingFile)) {
        KNotification::event(QStringLiteral("ErrorMessage"), i18n("Saving project file <br><b>%1</B> failed", outputFileName), QPixmap());
        return false;
    }
    QUrl url = QUrl::fromLocalFile(outputFileName);
    // Save timeline thumbnails
    std::unordered_map<QString, std::vector<int>> thumbKeys = pCore->window()->getCurrentTimeline()->controller()->getThumbKeys();
    pCore->projectItemModel()->updateCacheThumbnail(thumbKeys);
    // Remove duplicates
    for (auto p : thumbKeys) {
        std::sort(p.second.begin(), p.second.end());
        auto last = std::unique(p.second.begin(), p.second.end());
        p.second.erase(last, p.second.end());
    }
    ThumbnailCache::get()->saveCachedThumbs(thumbKeys);
    if (!saveACopy) {
        m_project->setUrl(url);
        // setting up autosave file in ~/.kde/data/stalefiles/kdenlive/
        // saved under file name
        // actual saving by KdenliveDoc::slotAutoSave() called by a timer 3 seconds after the document has been edited
        // This timer is set by KdenliveDoc::setModified()
        const QString projectId = QCryptographicHash::hash(url.fileName().toUtf8(), QCryptographicHash::Md5).toHex();
        QUrl autosaveUrl = QUrl::fromLocalFile(QFileInfo(outputFileName).absoluteDir().absoluteFilePath(projectId + QStringLiteral(".kdenlive")));
        if (m_project->m_autosave == nullptr) {
            // The temporary file is not opened or created until actually needed.
            // The file filename does not have to exist for KAutoSaveFile to be constructed (if it exists, it will not be touched).
            m_project->m_autosave = new KAutoSaveFile(autosaveUrl, m_project);
        } else {
            m_project->m_autosave->setManagedFile(autosaveUrl);
        }

        pCore->window()->setWindowTitle(m_project->description());
        m_project->setModified(false);
    }
    KNotification::event(QStringLiteral("SaveSuccess"), i18n("Saving successful"), QPixmap());

    m_recentFilesAction->addUrl(url);
    // remember folder for next project opening
    KRecentDirs::add(QStringLiteral(":KdenliveProjectsFolder"), saveFolder);
    saveRecentFiles();
    if (!saveACopy) {
        m_fileRevert->setEnabled(true);
        pCore->window()->m_undoView->stack()->setClean();
        QString newProjectFolder(saveFolder + QStringLiteral("/cachefiles"));
        if (((oldProjectFolder.isEmpty() && m_project->m_sameProjectFolder) || m_project->projectTempFolder() == oldProjectFolder) &&
            newProjectFolder != m_project->projectTempFolder()) {
            KMessageBox::ButtonCode answer = KMessageBox::warningContinueCancel(
                pCore->window(), i18n("The location of the project file changed. You selected to use the location of the project file to save temporary files. "
                                      "This will move all temporary files from <b>%1</b> to <b>%2</b>, the project file will then be reloaded",
                                      m_project->projectTempFolder(), newProjectFolder));

            if (answer == KMessageBox::Continue) {
                // Discard running jobs, for example proxy clips since data will be moved
                pCore->taskManager.slotCancelJobs();
                // Proceed with move
                QString documentId = QDir::cleanPath(m_project->getDocumentProperty(QStringLiteral("documentid")));
                bool ok;
                documentId.toLongLong(&ok, 10);
                if (!ok || documentId.isEmpty()) {
                    KMessageBox::error(pCore->window(), i18n("Cannot perform operation, invalid document id: %1", documentId));
                } else {
                    QDir newDir(newProjectFolder);
                    QDir oldDir(m_project->projectTempFolder());
                    if (newDir.exists(documentId)) {
                        KMessageBox::error(pCore->window(),
                                           i18n("Cannot perform operation, target directory already exists: %1", newDir.absoluteFilePath(documentId)));
                    } else {
                        // Proceed with the move
                        moveProjectData(oldDir.absoluteFilePath(documentId), newDir.absolutePath());
                    }
                }
            }
        }
    }
    return true;
}

void ProjectManager::saveRecentFiles()
{
    KSharedConfigPtr config = KSharedConfig::openConfig();
    m_recentFilesAction->saveEntries(KConfigGroup(config, "Recent Files"));
    config->sync();
}

bool ProjectManager::saveFileAs(bool saveACopy)
{
    QFileDialog fd(pCore->window());
    if (saveACopy) {
        fd.setWindowTitle(i18nc("@title:window", "Save Copy"));
    }
    if (m_project->url().isValid()) {
        fd.selectUrl(m_project->url());
    } else {
        fd.setDirectory(KdenliveSettings::defaultprojectfolder());
    }
    fd.setNameFilter(getProjectNameFilters(false));
    fd.setAcceptMode(QFileDialog::AcceptSave);
    fd.setFileMode(QFileDialog::AnyFile);
    fd.setDefaultSuffix(QStringLiteral("kdenlive"));
    if (fd.exec() != QDialog::Accepted || fd.selectedFiles().isEmpty()) {
        return false;
    }

    QString outputFile = fd.selectedFiles().constFirst();

    bool ok;
    QDir cacheDir = m_project->getCacheDir(CacheBase, &ok);
    if (ok) {
        QFile file(cacheDir.absoluteFilePath(QString::fromLatin1(QUrl::toPercentEncoding(QStringLiteral(".") + outputFile))));
        file.open(QIODevice::ReadWrite | QIODevice::Text);
        file.close();
    }
    return saveFileAs(outputFile, false, saveACopy);
}

bool ProjectManager::saveFile()
{
    if (!m_project) {
        // Calling saveFile before a project was created, something is wrong
        qCDebug(KDENLIVE_LOG) << "SaveFile called without project";
        return false;
    }
    if (m_project->url().isEmpty()) {
        return saveFileAs();
    }
    bool result = saveFileAs(m_project->url().toLocalFile());
    m_project->m_autosave->resize(0);
    return result;
}

void ProjectManager::openFile()
{
    if (m_startUrl.isValid()) {
        openFile(m_startUrl);
        m_startUrl.clear();
        return;
    }
    QUrl url = QFileDialog::getOpenFileUrl(pCore->window(), QString(), QUrl::fromLocalFile(KRecentDirs::dir(QStringLiteral(":KdenliveProjectsFolder"))),
                                           getProjectNameFilters());
    if (!url.isValid()) {
        return;
    }
    KRecentDirs::add(QStringLiteral(":KdenliveProjectsFolder"), url.adjusted(QUrl::RemoveFilename).toLocalFile());
    m_recentFilesAction->addUrl(url);
    saveRecentFiles();
    openFile(url);
}

void ProjectManager::openLastFile()
{
    if (m_recentFilesAction->selectableActionGroup()->actions().isEmpty()) {
        // No files in history
        newFile(false);
        return;
    }

    QAction *firstUrlAction = m_recentFilesAction->selectableActionGroup()->actions().last();
    if (firstUrlAction) {
        firstUrlAction->trigger();
    } else {
        newFile(false);
    }
}

// fix mantis#3160 separate check from openFile() so we can call it from newFile()
// to find autosaved files (in ~/.local/share/stalefiles/kdenlive) and recover it
bool ProjectManager::checkForBackupFile(const QUrl &url, bool newFile)
{
    // Check for autosave file that belong to the url we passed in.
    const QString projectId = QCryptographicHash::hash(url.fileName().toUtf8(), QCryptographicHash::Md5).toHex();
    QUrl autosaveUrl = newFile ? url : QUrl::fromLocalFile(QFileInfo(url.path()).absoluteDir().absoluteFilePath(projectId + QStringLiteral(".kdenlive")));
    QList<KAutoSaveFile *> staleFiles = KAutoSaveFile::staleFiles(autosaveUrl);
    QFileInfo sourceInfo(url.toLocalFile());
    QDateTime sourceTime;
    if (sourceInfo.exists()) {
        sourceTime = QFileInfo(url.toLocalFile()).lastModified();
    }
    KAutoSaveFile *orphanedFile = nullptr;
    // Check if we can have a lock on one of the file,
    // meaning it is not handled by any Kdenlive instance
    if (!staleFiles.isEmpty()) {
        for (KAutoSaveFile *stale : qAsConst(staleFiles)) {
            if (stale->open(QIODevice::QIODevice::ReadWrite)) {
                // Found orphaned autosave file
                if (!sourceTime.isValid() || QFileInfo(stale->fileName()).lastModified() > sourceTime) {
                    orphanedFile = stale;
                    break;
                }
            }
        }
    }

    if (orphanedFile) {
        if (KMessageBox::questionTwoActions(nullptr, i18n("Auto-saved file exist. Do you want to recover now?"), i18n("File Recovery"),
                                            KGuiItem(i18n("Recover")), KGuiItem(i18n("Do not recover"))) == KMessageBox::PrimaryAction) {
            doOpenFile(url, orphanedFile);
            return true;
        }
    }
    // remove the stale files
    for (KAutoSaveFile *stale : qAsConst(staleFiles)) {
        stale->open(QIODevice::ReadWrite);
        delete stale;
    }
    return false;
}

void ProjectManager::openFile(const QUrl &url)
{
    QMimeDatabase db;
    // Make sure the url is a Kdenlive project file
    QMimeType mime = db.mimeTypeForUrl(url);
    if (mime.inherits(QStringLiteral("application/x-compressed-tar")) || mime.inherits(QStringLiteral("application/zip"))) {
        // Opening a compressed project file, we need to process it
        // qCDebug(KDENLIVE_LOG)<<"Opening archive, processing";
        QPointer<ArchiveWidget> ar = new ArchiveWidget(url);
        if (ar->exec() == QDialog::Accepted) {
            openFile(QUrl::fromLocalFile(ar->extractedProjectFile()));
        } else if (m_startUrl.isValid()) {
            // we tried to open an invalid file from command line, init new project
            newFile(false);
        }
        delete ar;
        return;
    }

    /*if (!url.fileName().endsWith(".kdenlive")) {
        // This is not a Kdenlive project file, abort loading
        KMessageBox::error(pCore->window(), i18n("File %1 is not a Kdenlive project file", url.toLocalFile()));
        if (m_startUrl.isValid()) {
            // we tried to open an invalid file from command line, init new project
            newFile(false);
        }
        return;
    }*/

    if ((m_project != nullptr) && m_project->url() == url) {
        return;
    }

    if (!closeCurrentDocument()) {
        return;
    }
    if (checkForBackupFile(url)) {
        return;
    }
    pCore->displayMessage(i18n("Opening file %1", url.toLocalFile()), OperationCompletedMessage, 100);
    doOpenFile(url, nullptr);
}

void ProjectManager::abortLoading()
{
    KMessageBox::error(pCore->window(), i18n("Could not recover corrupted file."));
    Q_EMIT pCore->loadingMessageHide();
    // Don't propose to save corrupted doc
    m_project->setModified(false);
    // Open default blank document
    newFile(false);
}

void ProjectManager::doOpenFile(const QUrl &url, KAutoSaveFile *stale, bool isBackup)
{
    Q_ASSERT(m_project == nullptr);
    m_fileRevert->setEnabled(true);

    ThumbnailCache::get()->clearCache();
    pCore->monitorManager()->resetDisplay();
    pCore->monitorManager()->activateMonitor(Kdenlive::ProjectMonitor);
    Q_EMIT pCore->loadingMessageNewStage(i18n("Loading project…"));
    qApp->processEvents();
    m_notesPlugin->clear();

    DocOpenResult openResult = KdenliveDoc::Open(stale ? QUrl::fromLocalFile(stale->fileName()) : url,
        QString(), pCore->window()->m_commandStack, false, pCore->window());

    KdenliveDoc *doc = nullptr;
    if (!openResult.isSuccessful() && !openResult.isAborted()) {
        if (!isBackup) {
            int answer = KMessageBox::warningTwoActionsCancel(
                pCore->window(), i18n("Cannot open the project file. Error:\n%1\nDo you want to open a backup file?", openResult.getError()),
                i18n("Error opening file"), KGuiItem(i18n("Open Backup")), KGuiItem(i18n("Recover")));
            if (answer == KMessageBox::PrimaryAction) { // Open Backup
                slotOpenBackup(url);
                return;
            } else if (answer == KMessageBox::SecondaryAction) { // Recover
                // if file was broken by Kdenlive 0.9.4, we can try recovering it. If successful, continue through rest of this function.
                openResult = KdenliveDoc::Open(stale ? QUrl::fromLocalFile(stale->fileName()) : url,
                    QString(), pCore->window()->m_commandStack, true, pCore->window());
                if (openResult.isSuccessful()) {
                    doc = openResult.getDocument().release();
                    doc->requestBackup();
                } else {
                    KMessageBox::error(pCore->window(), i18n("Could not recover corrupted file."));
                }
            }
        } else {
            KMessageBox::detailedError(pCore->window(), i18n("Could not open the backup project file."), openResult.getError());
        }
    } else {
        doc = openResult.getDocument().release();
    }

    // if we could not open the file, and could not recover (or user declined), stop now
    if (!openResult.isSuccessful() || !doc) {
        Q_EMIT pCore->loadingMessageHide();
        // Open default blank document
        newFile(false);
        return;
    }

    if (openResult.wasUpgraded()) {
        pCore->displayMessage(i18n("Your project was upgraded, a backup will be created on next save"),
            ErrorMessage);
    } else if (openResult.wasModified()) {
        pCore->displayMessage(i18n("Your project was modified on opening, a backup will be created on next save"),
            ErrorMessage);
    }
    pCore->displayMessage(QString(), OperationCompletedMessage);

    if (stale == nullptr) {
        const QString projectId = QCryptographicHash::hash(url.fileName().toUtf8(), QCryptographicHash::Md5).toHex();
        QUrl autosaveUrl = QUrl::fromLocalFile(QFileInfo(url.path()).absoluteDir().absoluteFilePath(projectId + QStringLiteral(".kdenlive")));
        stale = new KAutoSaveFile(autosaveUrl, doc);
        doc->m_autosave = stale;
    } else {
        doc->m_autosave = stale;
        stale->setParent(doc);
        // if loading from an autosave of unnamed file, or restore failed then keep unnamed
        bool loadingFailed = doc->url().isEmpty();
        if (url.fileName().contains(QStringLiteral("_untitled.kdenlive"))) {
            doc->setUrl(QUrl());
            doc->setModified(true);
        } else if (!loadingFailed) {
            doc->setUrl(url);
        }
        doc->setModified(!loadingFailed);
        stale->setParent(doc);
    }
    pCore->bin()->setDocument(doc);

    // Set default target tracks to upper audio / lower video tracks
    m_project = doc;
    m_mltWarnings.clear();
    connect(pCore.get(), &Core::mltWarning, this, &ProjectManager::handleLog, Qt::QueuedConnection);
    pCore->monitorManager()->projectMonitor()->locked = true;
    QDateTime documentDate = QFileInfo(m_project->url().toLocalFile()).lastModified();
    Q_EMIT pCore->loadingMessageNewStage(i18n("Loading timeline…"), 0);
    qApp->processEvents();
    bool timelineResult = updateTimeline(true, m_project->getDocumentProperty(QStringLiteral("previewchunks")),
                                         m_project->getDocumentProperty(QStringLiteral("dirtypreviewchunks")), documentDate,
                                         m_project->getDocumentProperty(QStringLiteral("disablepreview")).toInt());
    disconnect(pCore.get(), &Core::mltWarning, this, &ProjectManager::handleLog);
    if (!timelineResult) {
        Q_EMIT pCore->loadingMessageHide();
        // Don't propose to save corrupted doc
        abortProjectLoad(url);
        return;
    }
    m_mltWarnings.clear();

    // Re-open active timelines
    QStringList openedTimelines = m_project->getDocumentProperty(QStringLiteral("opensequences")).split(QLatin1Char(';'), Qt::SkipEmptyParts);
    auto sequences = pCore->projectItemModel()->getAllSequenceClips();
    const int taskCount = openedTimelines.count() + sequences.count();
    Q_EMIT pCore->loadingMessageNewStage(i18n("Building sequences…"), taskCount);
    qApp->processEvents();

    QList<QUuid> openedUuids;
    for (auto &uid : openedTimelines) {
        const QUuid uuid(uid);
        openedUuids << uuid;
        const QString binId = pCore->projectItemModel()->getSequenceId(uuid);
        if (!binId.isEmpty()) {
            if (!openTimeline(binId, uuid)) {
                abortProjectLoad(url);
                return;
            }
        }
        Q_EMIT pCore->loadingMessageIncrease();
        qApp->processEvents();
    }

    // Now that sequence clips are fully built, fetch thumbnails
    QList<QUuid> uuids = sequences.keys();
    // Load all sequence models into memory
    for (auto &uid : uuids) {
        if (pCore->currentDoc()->getTimeline(uid, true) == nullptr) {
            std::shared_ptr<Mlt::Tractor> tc = pCore->projectItemModel()->getExtraTimeline(uid.toString());
            if (tc) {
                std::shared_ptr<TimelineItemModel> timelineModel = TimelineItemModel::construct(uid, m_project->commandStack());
                const QString chunks = m_project->getSequenceProperty(uid, QStringLiteral("previewchunks"));
                const QString dirty = m_project->getSequenceProperty(uid, QStringLiteral("dirtypreviewchunks"));
                const QString binId = pCore->projectItemModel()->getSequenceId(uid);
                m_project->addTimeline(uid, timelineModel, false);
                if (constructTimelineFromTractor(timelineModel, nullptr, *tc.get(), m_project->modifiedDecimalPoint(), chunks, dirty)) {
                    pCore->projectItemModel()->setExtraTimelineSaved(uid.toString());
                    std::shared_ptr<Mlt::Producer> prod = std::make_shared<Mlt::Producer>(timelineModel->tractor());
                    passSequenceProperties(uid, prod, *tc.get(), timelineModel, nullptr);
                    std::shared_ptr<ProjectClip> clip = pCore->projectItemModel()->getClipByBinID(binId);
                    prod->parent().set("kdenlive:clipname", clip->clipName().toUtf8().constData());
                    prod->set("kdenlive:description", clip->description().toUtf8().constData());
                    if (timelineModel->getGuideModel() == nullptr) {
                        timelineModel->setMarkerModel(clip->markerModel());
                    }
                    // This sequence is not active, ensure it has a transparent background
                    timelineModel->makeTransparentBg(true);
                    m_project->loadSequenceGroupsAndGuides(uid);
                    clip->setProducer(prod, false, false);
                    clip->reloadTimeline(timelineModel->getMasterEffectStackModel());
                } else {
                    qWarning() << "XXXXXXXXX\nLOADING TIMELINE " << uid.toString() << " FAILED\n";
                    m_project->closeTimeline(uid, true);
                }
            }
        }
        Q_EMIT pCore->loadingMessageIncrease();
    }
    const QStringList sequenceIds = sequences.values();
    for (auto &id : sequenceIds) {
        ClipLoadTask::start(ObjectId(KdenliveObjectType::BinClip, id.toInt(), QUuid()), QDomElement(), true, -1, -1, this);
    }
    // Raise last active timeline
    QUuid activeUuid(m_project->getDocumentProperty(QStringLiteral("activetimeline")));
    if (activeUuid.isNull()) {
        activeUuid = m_project->uuid();
    }
    if (!activeUuid.isNull()) {
        const QString binId = pCore->projectItemModel()->getSequenceId(activeUuid);
        if (binId.isEmpty()) {
            if (pCore->projectItemModel()->sequenceCount() == 0) {
                // Something is broken here, abort
                abortLoading();
                return;
            }
        } else {
            if (!openTimeline(binId, activeUuid)) {
                abortProjectLoad(url);
                return;
            }
        }
    }
    pCore->window()->connectDocument();
    // Now load active sequence in project monitor
    pCore->monitorManager()->projectMonitor()->locked = false;
    int position = m_project->getSequenceProperty(activeUuid, QStringLiteral("position"), QString::number(0)).toInt();
    pCore->monitorManager()->projectMonitor()->setProducer(m_activeTimelineModel->producer(), position);

    Q_EMIT docOpened(m_project);
    pCore->displayMessage(QString(), OperationCompletedMessage, 100);
    m_lastSave.start();
    m_project->loading = false;
    if (pCore->monitorManager()) {
        Q_EMIT pCore->monitorManager()->updatePreviewScaling();
        pCore->monitorManager()->projectMonitor()->slotActivateMonitor();
        pCore->monitorManager()->projectMonitor()->setProducer(m_activeTimelineModel->producer(), 0);
        const QUuid uuid = m_project->activeUuid;
        pCore->monitorManager()->projectMonitor()->adjustRulerSize(m_activeTimelineModel->duration() - 1, m_project->getFilteredGuideModel(uuid));
    }
    checkProjectWarnings();
    pCore->projectItemModel()->missingClipTimer.start();
    Q_EMIT pCore->loadingMessageHide();
}

void ProjectManager::abortProjectLoad(const QUrl &url)
{
    m_project->setModified(false);
    Q_EMIT pCore->loadingMessageHide();
    pCore->monitorManager()->projectMonitor()->locked = false;
    int answer = KMessageBox::warningContinueCancelList(pCore->window(), i18n("Error opening file"), m_mltWarnings, i18n("Error opening file"),
                                                        KGuiItem(i18n("Open Backup")), KStandardGuiItem::cancel(), QString(), KMessageBox::Notify);
    if (answer == KMessageBox::Continue) {
        // Open Backup
        m_mltWarnings.clear();
        delete m_project;
        m_project = nullptr;
        if (slotOpenBackup(url)) {
            return;
        }
    }
    m_mltWarnings.clear();
    // Open default blank document
    newFile(false);
}

void ProjectManager::doOpenFileHeadless(const QUrl &url)
{
    Q_ASSERT(m_project == nullptr);
    QUndoGroup *undoGroup = new QUndoGroup();
    std::shared_ptr<DocUndoStack> undoStack = std::make_shared<DocUndoStack>(nullptr);
    undoGroup->addStack(undoStack.get());

    DocOpenResult openResult = KdenliveDoc::Open(url, QString() /*QDir::temp().path()*/, undoGroup, false, nullptr);

    KdenliveDoc *doc = nullptr;
    if (!openResult.isSuccessful() && !openResult.isAborted()) {
        qCritical() << i18n("Cannot open the project file. Error:\n%1\n", openResult.getError());
    } else {
        doc = openResult.getDocument().release();
    }

    // if we could not open the file, and could not recover (or user declined), stop now
    if (!openResult.isSuccessful() || !doc) {
        return;
    }

    // Set default target tracks to upper audio / lower video tracks
    m_project = doc;

    if (!updateTimeline(false, QString(), QString(), QDateTime(), 0)) {
        return;
    }

    // Re-open active timelines
    QStringList openedTimelines = m_project->getDocumentProperty(QStringLiteral("opensequences")).split(QLatin1Char(';'), Qt::SkipEmptyParts);
    for (auto &uid : openedTimelines) {
        const QUuid uuid(uid);
        const QString binId = pCore->projectItemModel()->getSequenceId(uuid);
        if (!binId.isEmpty()) {
            openTimeline(binId, uuid);
        }
    }
    // Raise last active timeline
    QUuid activeUuid(m_project->getDocumentProperty(QStringLiteral("activetimeline")));
    if (activeUuid.isNull()) {
        activeUuid = m_project->uuid();
    }

    auto timeline = m_project->getTimeline(activeUuid);
    testSetActiveDocument(m_project, timeline);
}

void ProjectManager::slotRevert()
{
    if (m_project->isModified() &&
        KMessageBox::warningContinueCancel(pCore->window(),
                                           i18n("This will delete all changes made since you last saved your project. Are you sure you want to continue?"),
                                           i18n("Revert to last saved version")) == KMessageBox::Cancel) {
        return;
    }
    QUrl url = m_project->url();
    if (closeCurrentDocument(false)) {
        doOpenFile(url, nullptr);
    }
}

KdenliveDoc *ProjectManager::current()
{
    return m_project;
}

bool ProjectManager::slotOpenBackup(const QUrl &url)
{
    QUrl projectFile;
    QUrl projectFolder;
    QString projectId;
    if (url.isValid()) {
        // we could not open the project file, guess where the backups are
        projectFolder = QUrl::fromLocalFile(KdenliveSettings::defaultprojectfolder());
        projectFile = url;
    } else {
        projectFolder = QUrl::fromLocalFile(m_project ? m_project->projectTempFolder() : QString());
        projectFile = m_project->url();
        projectId = m_project->getDocumentProperty(QStringLiteral("documentid"));
    }
    bool result = false;
    QPointer<BackupWidget> dia = new BackupWidget(projectFile, projectFolder, projectId, pCore->window());
    if (dia->exec() == QDialog::Accepted) {
        QString requestedBackup = dia->selectedFile();
        if (m_project) {
            m_project->backupLastSavedVersion(projectFile.toLocalFile());
            closeCurrentDocument(false);
        }
        doOpenFile(QUrl::fromLocalFile(requestedBackup), nullptr, true);
        if (m_project) {
            if (!m_project->url().isEmpty()) {
                // Only update if restore succeeded
                pCore->window()->slotEditSubtitle();
                m_project->setUrl(projectFile);
                m_project->setModified(true);
            }
            pCore->window()->setWindowTitle(m_project->description());
            result = true;
        }
    }
    delete dia;
    return result;
}

KRecentFilesAction *ProjectManager::recentFilesAction()
{
    return m_recentFilesAction;
}

void ProjectManager::slotStartAutoSave()
{
    if (m_lastSave.elapsed() > 300000) {
        // If the project was not saved in the last 5 minute, force save
        m_autoSaveTimer.stop();
        slotAutoSave();
    } else {
        m_autoSaveTimer.start(); // will trigger slotAutoSave() in 3 seconds
    }
}

void ProjectManager::slotAutoSave()
{
    if (m_project->loading || m_project->closing) {
        // Dont start autosave if the project is still loading
        return;
    }
    prepareSave();
    QString saveFolder = m_project->url().adjusted(QUrl::RemoveFilename | QUrl::StripTrailingSlash).toLocalFile();
    QString scene = projectSceneList(saveFolder);
    if (!m_replacementPattern.isEmpty()) {
        QMapIterator<QString, QString> i(m_replacementPattern);
        while (i.hasNext()) {
            i.next();
            scene.replace(i.key(), i.value());
        }
    }
    if (!scene.contains(QLatin1String("<track "))) {
        // In some unexplained cases, the MLT playlist is corrupted and all tracks are deleted. Don't save in that case.
        pCore->displayMessage(i18n("Project was corrupted, cannot backup. Please close and reopen your project file to recover last backup"), ErrorMessage);
        return;
    }
    m_project->slotAutoSave(scene);
    m_lastSave.start();
}

QString ProjectManager::projectSceneList(const QString &outputFolder, const QString &overlayData)
{
    // Disable multitrack view and overlay
    bool isMultiTrack = pCore->monitorManager() && pCore->monitorManager()->isMultiTrack();
    bool hasPreview = pCore->window() && pCore->window()->getCurrentTimeline()->controller()->hasPreviewTrack();
    bool isTrimming = pCore->monitorManager() && pCore->monitorManager()->isTrimming();
    if (isMultiTrack) {
        pCore->window()->getCurrentTimeline()->controller()->slotMultitrackView(false, false);
    }
    if (hasPreview) {
        pCore->window()->getCurrentTimeline()->model()->updatePreviewConnection(false);
    }
    if (isTrimming) {
        pCore->window()->getCurrentTimeline()->controller()->requestEndTrimmingMode();
    }
    if (pCore->mixer()) {
        pCore->mixer()->pauseMonitoring(true);
    }

    // We must save from the primary timeline model
    int duration = pCore->window() ? pCore->window()->getCurrentTimeline()->controller()->duration() : m_activeTimelineModel->duration();
    QString scene = pCore->projectItemModel()->sceneList(outputFolder, QString(), overlayData, m_activeTimelineModel->tractor(), duration);
    if (pCore->mixer()) {
        pCore->mixer()->pauseMonitoring(false);
    }
    if (isMultiTrack) {
        pCore->window()->getCurrentTimeline()->controller()->slotMultitrackView(true, false);
    }
    if (hasPreview) {
        pCore->window()->getCurrentTimeline()->model()->updatePreviewConnection(true);
    }
    if (isTrimming) {
        pCore->window()->getCurrentTimeline()->controller()->requestStartTrimmingMode();
    }
    return scene;
}

void ProjectManager::setDocumentNotes(const QString &notes)
{
    if (m_notesPlugin) {
        m_notesPlugin->widget()->setHtml(notes);
    }
}

QString ProjectManager::documentNotes() const
{
    QString text = m_notesPlugin->widget()->toPlainText().simplified();
    if (text.isEmpty()) {
        return QString();
    }
    return m_notesPlugin->widget()->toHtml();
}

void ProjectManager::slotAddProjectNote()
{
    m_notesPlugin->showDock();
    m_notesPlugin->widget()->setFocus();
    m_notesPlugin->widget()->addProjectNote();
}

void ProjectManager::slotAddTextNote(const QString &text)
{
    m_notesPlugin->showDock();
    m_notesPlugin->widget()->setFocus();
    m_notesPlugin->widget()->addTextNote(text);
}

void ProjectManager::prepareSave()
{
    pCore->projectItemModel()->saveDocumentProperties(pCore->currentDoc()->documentProperties(), m_project->metadata());
    pCore->bin()->saveFolderState();
    pCore->projectItemModel()->saveProperty(QStringLiteral("kdenlive:documentnotes"), documentNotes());
    pCore->projectItemModel()->saveProperty(QStringLiteral("kdenlive:docproperties.opensequences"), pCore->window()->openedSequences().join(QLatin1Char(';')));
    pCore->projectItemModel()->saveProperty(QStringLiteral("kdenlive:docproperties.activetimeline"), m_activeTimelineModel->uuid().toString());
}

void ProjectManager::slotResetProfiles(bool reloadThumbs)
{
    m_project->resetProfile(reloadThumbs);
    pCore->monitorManager()->updateScopeSource();
}

void ProjectManager::slotResetConsumers(bool fullReset)
{
    pCore->monitorManager()->resetConsumers(fullReset);
}

void ProjectManager::disableBinEffects(bool disable, bool refreshMonitor)
{
    if (m_project) {
        if (disable) {
            m_project->setDocumentProperty(QStringLiteral("disablebineffects"), QString::number(1));
        } else {
            m_project->setDocumentProperty(QStringLiteral("disablebineffects"), QString());
        }
    }
    if (refreshMonitor) {
        pCore->monitorManager()->refreshProjectMonitor();
        pCore->monitorManager()->refreshClipMonitor();
    }
}

void ProjectManager::slotDisableTimelineEffects(bool disable)
{
    if (disable) {
        m_project->setDocumentProperty(QStringLiteral("disabletimelineeffects"), QString::number(true));
    } else {
        m_project->setDocumentProperty(QStringLiteral("disabletimelineeffects"), QString());
    }
    m_activeTimelineModel->setTimelineEffectsEnabled(!disable);
    pCore->monitorManager()->refreshProjectMonitor();
}

void ProjectManager::slotSwitchTrackDisabled()
{
    pCore->window()->getCurrentTimeline()->controller()->switchTrackDisabled();
}

void ProjectManager::slotSwitchTrackLock()
{
    pCore->window()->getCurrentTimeline()->controller()->switchTrackLock();
}

void ProjectManager::slotSwitchTrackActive()
{
    pCore->window()->getCurrentTimeline()->controller()->switchTrackActive();
}

void ProjectManager::slotSwitchAllTrackActive()
{
    pCore->window()->getCurrentTimeline()->controller()->switchAllTrackActive();
}

void ProjectManager::slotMakeAllTrackActive()
{
    pCore->window()->getCurrentTimeline()->controller()->makeAllTrackActive();
}

void ProjectManager::slotRestoreTargetTracks()
{
    pCore->window()->getCurrentTimeline()->controller()->restoreTargetTracks();
}

void ProjectManager::slotSwitchAllTrackLock()
{
    pCore->window()->getCurrentTimeline()->controller()->switchTrackLock(true);
}

void ProjectManager::slotSwitchTrackTarget()
{
    pCore->window()->getCurrentTimeline()->controller()->switchTargetTrack();
}

QString ProjectManager::getDefaultProjectFormat()
{
    // On first run, lets use an HD1080p profile with fps related to timezone country. Then, when the first video is added to a project, if it does not match
    // our profile, propose a new default.
    QTimeZone zone;
    zone = QTimeZone::systemTimeZone();

    QList<int> ntscCountries;
    ntscCountries << QLocale::Canada << QLocale::Chile << QLocale::CostaRica << QLocale::Cuba << QLocale::DominicanRepublic << QLocale::Ecuador;
    ntscCountries << QLocale::Japan << QLocale::Mexico << QLocale::Nicaragua << QLocale::Panama << QLocale::Peru << QLocale::Philippines;
    ntscCountries << QLocale::PuertoRico << QLocale::SouthKorea << QLocale::Taiwan << QLocale::UnitedStates;
    bool ntscProject = ntscCountries.contains(zone.country());
    if (!ntscProject) {
        return QStringLiteral("atsc_1080p_25");
    }
    return QStringLiteral("atsc_1080p_2997");
}

void ProjectManager::saveZone(const QStringList &info, const QDir &dir)
{
    pCore->bin()->saveZone(info, dir);
}

void ProjectManager::moveProjectData(const QString &src, const QString &dest)
{
    // Move proxies
    bool ok;
    const QList<QUrl> proxyUrls = m_project->getProjectData(&ok);
    if (!ok) {
        // Could not move temporary data, abort
        KMessageBox::error(pCore->window(), i18n("Error moving project folder, cannot access cache folder"));
        return;
    }
    Fun copyTmp = [this, src, dest]() {
        // Move tmp folder (thumbnails, timeline preview)
        KIO::CopyJob *copyJob = KIO::move(QUrl::fromLocalFile(src), QUrl::fromLocalFile(dest), KIO::DefaultFlags);
        if (copyJob->uiDelegate()) {
            KJobWidgets::setWindow(copyJob, pCore->window());
        }
        connect(copyJob, &KJob::percentChanged, this, &ProjectManager::slotMoveProgress);
        connect(copyJob, &KJob::result, this, &ProjectManager::slotMoveFinished);
        return true;
    };
    if (!proxyUrls.isEmpty()) {
        QDir proxyDir(dest + QStringLiteral("/proxy/"));
        if (proxyDir.mkpath(QStringLiteral("."))) {
            KIO::CopyJob *job = KIO::move(proxyUrls, QUrl::fromLocalFile(proxyDir.absolutePath()));
            connect(job, &KJob::percentChanged, this, &ProjectManager::slotMoveProgress);
            connect(job, &KJob::result, this, [copyTmp](KJob *job) {
                if (job->error() == 0) {
                    copyTmp();
                } else {
                    KMessageBox::error(pCore->window(), i18n("Error moving project folder: %1", job->errorText()));
                }
            });
            if (job->uiDelegate()) {
                KJobWidgets::setWindow(job, pCore->window());
            }
        }
    } else {
        copyTmp();
    }
}

void ProjectManager::slotMoveProgress(KJob *, unsigned long progress)
{
    pCore->displayMessage(i18n("Moving project folder"), ProcessingJobMessage, static_cast<int>(progress));
}

void ProjectManager::slotMoveFinished(KJob *job)
{
    if (job->error() == 0) {
        pCore->displayMessage(QString(), OperationCompletedMessage, 100);
        auto *copyJob = static_cast<KIO::CopyJob *>(job);
        QString newFolder = copyJob->destUrl().toLocalFile();
        // Check if project folder is inside document folder, in which case, paths will be relative
        QDir projectDir(m_project->url().toString(QUrl::RemoveFilename | QUrl::RemoveScheme));
        QDir srcDir(m_project->projectTempFolder());
        if (srcDir.absolutePath().startsWith(projectDir.absolutePath())) {
            m_replacementPattern.insert(QStringLiteral(">proxy/"), QStringLiteral(">") + newFolder + QStringLiteral("/proxy/"));
        } else {
            m_replacementPattern.insert(m_project->projectTempFolder() + QStringLiteral("/proxy/"), newFolder + QStringLiteral("/proxy/"));
        }
        m_project->setProjectFolder(QUrl::fromLocalFile(newFolder));
        saveFile();
        m_replacementPattern.clear();
        slotRevert();
    } else {
        KMessageBox::error(pCore->window(), i18n("Error moving project folder: %1", job->errorText()));
    }
}

void ProjectManager::requestBackup(const QString &errorMessage)
{
    KMessageBox::ButtonCode res = KMessageBox::warningContinueCancel(qApp->activeWindow(), errorMessage);
    if (pCore->window()->getCurrentTimeline()) {
        pCore->window()->getCurrentTimeline()->loading = false;
    }
    m_project->setModified(false);
    if (res == KMessageBox::Continue) {
        // Try opening backup
        if (!slotOpenBackup(m_project->url())) {
            newFile(false);
        }
    } else {
        newFile(false);
    }
}

bool ProjectManager::updateTimeline(bool createNewTab, const QString &chunks, const QString &dirty, const QDateTime &documentDate, bool enablePreview)
{
    pCore->taskManager.slotCancelJobs();
    const QUuid uuid = m_project->uuid();
    QMutexLocker lock(&pCore->xmlMutex);
    std::unique_ptr<Mlt::Producer> xmlProd(
        new Mlt::Producer(pCore->getProjectProfile().get_profile(), "xml-string", m_project->getAndClearProjectXml().constData()));
    lock.unlock();
    Mlt::Service s(*xmlProd.get());
    Mlt::Tractor tractor(s);
    if (xmlProd->property_exists("kdenlive:projectTractor")) {
        // This is the new multi-timeline document format
        // Get active sequence uuid
        std::shared_ptr<Mlt::Producer> tk(tractor.track(0));
        const QUuid activeUuid(tk->parent().get("kdenlive:uuid"));
        Q_ASSERT(!activeUuid.isNull());
        m_project->cleanupTimelinePreview(documentDate);
        pCore->projectItemModel()->buildPlaylist(uuid);
        // Load bin playlist
        return loadProjectBin(tractor, activeUuid);
    }
    if (tractor.count() == 0) {
        // Wow we have a project file with empty tractor, probably corrupted, propose to open a recovery file
        return false;
    }
    std::shared_ptr<TimelineItemModel> timelineModel = TimelineItemModel::construct(uuid, m_project->commandStack());

    if (m_project->hasDocumentProperty(QStringLiteral("groups"))) {
        // This is a pre-nesting project file, move all timeline properties to the timelineModel's tractor
        QStringList importedProperties({QStringLiteral("groups"), QStringLiteral("guides"), QStringLiteral("zonein"), QStringLiteral("zoneout"),
                                        QStringLiteral("audioTarget"), QStringLiteral("videoTarget"), QStringLiteral("activeTrack"), QStringLiteral("position"),
                                        QStringLiteral("scrollPos"), QStringLiteral("disablepreview"), QStringLiteral("previewchunks"),
                                        QStringLiteral("dirtypreviewchunks")});
        m_project->importSequenceProperties(uuid, importedProperties);
    } else {
        qDebug() << ":::: NOT FOUND DOCUMENT GUIDES !!!!!!!!!!!\n!!!!!!!!!!!!!!!!!!!!!";
    }
    m_project->addTimeline(uuid, timelineModel);
    timelineModel->isClosed = false;
    TimelineWidget *documentTimeline = nullptr;

    m_project->cleanupTimelinePreview(documentDate);
    if (pCore->window()) {
        if (!createNewTab) {
            documentTimeline = pCore->window()->getCurrentTimeline();
            documentTimeline->setModel(timelineModel, pCore->monitorManager()->projectMonitor()->getControllerProxy());
        } else {
            // Create a new timeline tab
            documentTimeline = pCore->window()->openTimeline(uuid, i18n("Sequence 1"), timelineModel);
        }
    }
    pCore->projectItemModel()->buildPlaylist(uuid);
    if (m_activeTimelineModel == nullptr) {
        m_activeTimelineModel = timelineModel;
        m_project->activeUuid = timelineModel->uuid();
    }
    if (!constructTimelineFromTractor(timelineModel, pCore->projectItemModel(), tractor, m_project->modifiedDecimalPoint(), chunks, dirty, enablePreview)) {
        // TODO: act on project load failure
        qDebug() << "// Project failed to load!!";
        requestBackup(i18n("Project file is corrupted - failed to load tracks. Try to find a backup file?"));
        return false;
    }
    // Free memory used by original playlist
    xmlProd->clear();
    xmlProd.reset(nullptr);
    // Build primary timeline sequence
    Fun undo = []() { return true; };
    Fun redo = []() { return true; };
    // Create the timelines folder to store timeline clips
    QString folderId = pCore->projectItemModel()->getFolderIdByName(i18n("Sequences"));
    if (folderId.isEmpty()) {
        pCore->projectItemModel()->requestAddFolder(folderId, i18n("Sequences"), QStringLiteral("-1"), undo, redo);
    }
    QString mainId;
    std::shared_ptr<Mlt::Producer> prod = std::make_shared<Mlt::Producer>(timelineModel->tractor()->cut());
    passSequenceProperties(uuid, prod, tractor, timelineModel, documentTimeline);
    pCore->projectItemModel()->requestAddBinClip(mainId, prod, folderId, undo, redo);
    pCore->projectItemModel()->setSequencesFolder(folderId.toInt());
    if (pCore->window()) {
        QObject::connect(timelineModel.get(), &TimelineModel::durationUpdated, this, &ProjectManager::updateSequenceDuration, Qt::UniqueConnection);
    }
    std::shared_ptr<ProjectClip> mainClip = pCore->projectItemModel()->getClipByBinID(mainId);
    timelineModel->setMarkerModel(mainClip->markerModel());
    m_project->loadSequenceGroupsAndGuides(uuid);
    if (documentTimeline) {
        documentTimeline->loadMarkerModel();
    }
    timelineModel->setUndoStack(m_project->commandStack());

    // Reset locale to C to ensure numbers are serialised correctly
    // QMutexLocker lock(&pCore->xmlMutex);
    // LocaleHandling::resetLocale();
    return true;
}

void ProjectManager::passSequenceProperties(const QUuid &uuid, std::shared_ptr<Mlt::Producer> prod, Mlt::Tractor tractor,
                                            std::shared_ptr<TimelineItemModel> timelineModel, TimelineWidget *timelineWidget)
{
    QPair<int, int> tracks = timelineModel->getAVtracksCount();
    prod->parent().set("id", uuid.toString().toUtf8().constData());
    prod->set("kdenlive:uuid", uuid.toString().toUtf8().constData());
    prod->set("kdenlive:clipname", i18n("Sequence 1").toUtf8().constData());
    prod->set("kdenlive:producer_type", ClipType::Timeline);
    prod->set("kdenlive:sequenceproperties.tracksCount", tracks.first + tracks.second);
    prod->parent().set("kdenlive:uuid", uuid.toString().toUtf8().constData());
    prod->parent().set("kdenlive:clipname", i18n("Sequence 1").toUtf8().constData());
    prod->parent().set("kdenlive:sequenceproperties.hasAudio", tracks.first > 0 ? 1 : 0);
    prod->parent().set("kdenlive:sequenceproperties.hasVideo", tracks.second > 0 ? 1 : 0);
    if (m_project->hasSequenceProperty(uuid, QStringLiteral("activeTrack"))) {
        int activeTrack = m_project->getSequenceProperty(uuid, QStringLiteral("activeTrack")).toInt();
        prod->parent().set("kdenlive:sequenceproperties.activeTrack", activeTrack);
    }
    prod->parent().set("kdenlive:sequenceproperties.tracksCount", tracks.first + tracks.second);
    prod->parent().set("kdenlive:sequenceproperties.documentuuid", m_project->uuid().toString().toUtf8().constData());
    if (tractor.property_exists("kdenlive:duration")) {
        const QString duration(tractor.get("kdenlive:duration"));
        const QString maxduration(tractor.get("kdenlive:maxduration"));
        prod->parent().set("kdenlive:duration", duration.toLatin1().constData());
        prod->parent().set("kdenlive:maxduration", maxduration.toLatin1().constData());
    } else {
        // Fetch duration from actual tractor
        int projectDuration = timelineModel->duration();
        if (timelineWidget) {
            timelineWidget->controller()->checkDuration();
        }
        prod->parent().set("kdenlive:duration", timelineModel->tractor()->frames_to_time(projectDuration + 1));
        prod->parent().set("kdenlive:maxduration", projectDuration + 1);
        prod->parent().set("length", projectDuration + 1);
        prod->parent().set("out", projectDuration);
    }
    if (tractor.property_exists("kdenlive:sequenceproperties.timelineHash")) {
        prod->parent().set("kdenlive:sequenceproperties.timelineHash", tractor.get("kdenlive:sequenceproperties.timelineHash"));
    }
    prod->parent().set("kdenlive:producer_type", ClipType::Timeline);
}

void ProjectManager::updateSequenceDuration(const QUuid &uuid)
{
    const QString binId = pCore->projectItemModel()->getSequenceId(uuid);
    std::shared_ptr<ProjectClip> mainClip = pCore->projectItemModel()->getClipByBinID(binId);
    std::shared_ptr<TimelineItemModel> model = m_project->getTimeline(uuid);
    qDebug() << "::: UPDATING MAIN TIMELINE DURATION: " << model->duration();
    if (mainClip && model) {
        QMap<QString, QString> properties;
        properties.insert(QStringLiteral("kdenlive:duration"), QString(model->tractor()->frames_to_time(model->duration())));
        properties.insert(QStringLiteral("kdenlive:maxduration"), QString::number(model->duration()));
        properties.insert(QStringLiteral("length"), QString::number(model->duration()));
        properties.insert(QStringLiteral("out"), QString::number(model->duration() - 1));
        mainClip->setProperties(properties, true);
    } else {
        qDebug() << ":::: MAIN CLIP PRODUCER NOT FOUND!!!";
    }
}

void ProjectManager::adjustProjectDuration(int duration)
{
    pCore->monitorManager()->projectMonitor()->adjustRulerSize(duration - 1, nullptr);
}

void ProjectManager::activateAsset(const QVariantMap &effectData)
{
    if (effectData.contains(QStringLiteral("kdenlive/effect"))) {
        pCore->window()->addEffect(effectData.value(QStringLiteral("kdenlive/effect")).toString());
    } else {
        pCore->window()->getCurrentTimeline()->controller()->addAsset(effectData);
    }
}

std::shared_ptr<MarkerListModel> ProjectManager::getGuideModel()
{
    return current()->getGuideModel(pCore->currentTimelineId());
}

std::shared_ptr<DocUndoStack> ProjectManager::undoStack()
{
    return current()->commandStack();
}

const QDir ProjectManager::cacheDir(bool audio, bool *ok) const
{
    if (m_project == nullptr) {
        *ok = false;
        return QDir();
    }
    return m_project->getCacheDir(audio ? CacheAudio : CacheThumbs, ok);
}

void ProjectManager::saveWithUpdatedProfile(const QString &updatedProfile)
{
    // First backup current project with fps appended
    bool saveInTempFile = false;
    if (m_project && m_project->isModified()) {
        switch (KMessageBox::warningTwoActionsCancel(pCore->window(),
                                                     i18n("The project <b>\"%1\"</b> has been changed.\nDo you want to save your changes?",
                                                          m_project->url().fileName().isEmpty() ? i18n("Untitled") : m_project->url().fileName()),
                                                     {}, KStandardGuiItem::save(), KStandardGuiItem::dontSave())) {
        case KMessageBox::PrimaryAction:
            // save document here. If saving fails, return false;
            if (!saveFile()) {
                pCore->displayBinMessage(i18n("Project profile change aborted"), KMessageWidget::Information);
                return;
            }
            break;
        case KMessageBox::Cancel:
            pCore->displayBinMessage(i18n("Project profile change aborted"), KMessageWidget::Information);
            return;
            break;
        default:
            saveInTempFile = true;
            break;
        }
    }

    if (!m_project) {
        pCore->displayBinMessage(i18n("Project profile change aborted"), KMessageWidget::Information);
        return;
    }
    QString currentFile = m_project->url().toLocalFile();

    // Now update to new profile
    auto &newProfile = ProfileRepository::get()->getProfile(updatedProfile);
    QString convertedFile = QStringUtils::appendToFilename(currentFile, QString("-%1").arg(int(newProfile->fps() * 100)));
    QString saveFolder = m_project->url().adjusted(QUrl::RemoveFilename | QUrl::StripTrailingSlash).toLocalFile();
    QTemporaryFile tmpFile(saveFolder + "/kdenlive-XXXXXX.mlt");
    if (saveInTempFile) {
        // Save current playlist in tmp file
        if (!tmpFile.open()) {
            // Something went wrong
            pCore->displayBinMessage(i18n("Project profile change aborted"), KMessageWidget::Information);
            return;
        }
        prepareSave();
        QString scene = projectSceneList(saveFolder);
        if (!m_replacementPattern.isEmpty()) {
            QMapIterator<QString, QString> i(m_replacementPattern);
            while (i.hasNext()) {
                i.next();
                scene.replace(i.key(), i.value());
            }
        }
        tmpFile.write(scene.toUtf8());
        if (tmpFile.error() != QFile::NoError) {
            tmpFile.close();
            return;
        }
        tmpFile.close();
        currentFile = tmpFile.fileName();
        // Don't ask again to save
        m_project->setModified(false);
    }

    QDomDocument doc;
    if (!Xml::docContentFromFile(doc, currentFile, false)) {
        KMessageBox::error(qApp->activeWindow(), i18n("Cannot read file %1", currentFile));
        return;
    }

    QDomElement mltProfile = doc.documentElement().firstChildElement(QStringLiteral("profile"));
    if (!mltProfile.isNull()) {
        mltProfile.setAttribute(QStringLiteral("frame_rate_num"), newProfile->frame_rate_num());
        mltProfile.setAttribute(QStringLiteral("frame_rate_den"), newProfile->frame_rate_den());
        mltProfile.setAttribute(QStringLiteral("display_aspect_num"), newProfile->display_aspect_num());
        mltProfile.setAttribute(QStringLiteral("display_aspect_den"), newProfile->display_aspect_den());
        mltProfile.setAttribute(QStringLiteral("sample_aspect_num"), newProfile->sample_aspect_num());
        mltProfile.setAttribute(QStringLiteral("sample_aspect_den"), newProfile->sample_aspect_den());
        mltProfile.setAttribute(QStringLiteral("colorspace"), newProfile->colorspace());
        mltProfile.setAttribute(QStringLiteral("progressive"), newProfile->progressive());
        mltProfile.setAttribute(QStringLiteral("description"), newProfile->description());
        mltProfile.setAttribute(QStringLiteral("width"), newProfile->width());
        mltProfile.setAttribute(QStringLiteral("height"), newProfile->height());
    }
    QDomNodeList playlists = doc.documentElement().elementsByTagName(QStringLiteral("playlist"));
    double fpsRatio = newProfile->fps() / pCore->getCurrentFps();
    for (int i = 0; i < playlists.count(); ++i) {
        QDomElement e = playlists.at(i).toElement();
        if (e.attribute(QStringLiteral("id")) == QLatin1String("main_bin")) {
            Xml::setXmlProperty(e, QStringLiteral("kdenlive:docproperties.profile"), updatedProfile);
            // Update guides
            const QString &guidesData = Xml::getXmlProperty(e, QStringLiteral("kdenlive:docproperties.guides"));
            if (!guidesData.isEmpty()) {
                // Update guides position
                auto json = QJsonDocument::fromJson(guidesData.toUtf8());

                QJsonArray updatedList;
                if (json.isArray()) {
                    auto list = json.array();
                    for (const auto &entry : qAsConst(list)) {
                        if (!entry.isObject()) {
                            qDebug() << "Warning : Skipping invalid marker data";
                            continue;
                        }
                        auto entryObj = entry.toObject();
                        if (!entryObj.contains(QLatin1String("pos"))) {
                            qDebug() << "Warning : Skipping invalid marker data (does not contain position)";
                            continue;
                        }
                        int pos = qRound(double(entryObj[QLatin1String("pos")].toInt()) * fpsRatio);
                        QJsonObject currentMarker;
                        currentMarker.insert(QLatin1String("pos"), QJsonValue(pos));
                        currentMarker.insert(QLatin1String("comment"), entryObj[QLatin1String("comment")]);
                        currentMarker.insert(QLatin1String("type"), entryObj[QLatin1String("type")]);
                        updatedList.push_back(currentMarker);
                    }
                    QJsonDocument updatedJSon(updatedList);
                    Xml::setXmlProperty(e, QStringLiteral("kdenlive:docproperties.guides"), QString::fromUtf8(updatedJSon.toJson()));
                }
            }
            break;
        }
    }
    QDomNodeList producers = doc.documentElement().elementsByTagName(QStringLiteral("producer"));
    for (int i = 0; i < producers.count(); ++i) {
        QDomElement e = producers.at(i).toElement();
        bool ok;
        if (Xml::getXmlProperty(e, QStringLiteral("mlt_service")) == QLatin1String("qimage") && Xml::hasXmlProperty(e, QStringLiteral("ttl"))) {
            // Slideshow, duration is frame based, should be calculated again
            Xml::setXmlProperty(e, QStringLiteral("length"), QStringLiteral("0"));
            Xml::removeXmlProperty(e, QStringLiteral("kdenlive:duration"));
            e.setAttribute(QStringLiteral("out"), -1);
            continue;
        }
        int length = Xml::getXmlProperty(e, QStringLiteral("length")).toInt(&ok);
        if (ok && length > 0) {
            // calculate updated length
            Xml::setXmlProperty(e, QStringLiteral("length"), pCore->window()->getCurrentTimeline()->controller()->framesToClock(length));
        }
    }
    QDomNodeList chains = doc.documentElement().elementsByTagName(QStringLiteral("chain"));
    for (int i = 0; i < chains.count(); ++i) {
        QDomElement e = chains.at(i).toElement();
        bool ok;
        if (Xml::getXmlProperty(e, QStringLiteral("mlt_service")) == QLatin1String("qimage") && Xml::hasXmlProperty(e, QStringLiteral("ttl"))) {
            // Slideshow, duration is frame based, should be calculated again
            Xml::setXmlProperty(e, QStringLiteral("length"), QStringLiteral("0"));
            Xml::removeXmlProperty(e, QStringLiteral("kdenlive:duration"));
            e.setAttribute(QStringLiteral("out"), -1);
            continue;
        }
        int length = Xml::getXmlProperty(e, QStringLiteral("length")).toInt(&ok);
        if (ok && length > 0) {
            // calculate updated length
            Xml::setXmlProperty(e, QStringLiteral("length"), pCore->window()->getCurrentTimeline()->controller()->framesToClock(length));
        }
    }
    if (QFile::exists(convertedFile)) {
        if (KMessageBox::warningTwoActions(qApp->activeWindow(), i18n("Output file %1 already exists.\nDo you want to overwrite it?", convertedFile), {},
                                           KStandardGuiItem::overwrite(), KStandardGuiItem::cancel()) != KMessageBox::PrimaryAction) {
            return;
        }
    }
    QFile file(convertedFile);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return;
    }
    QTextStream out(&file);
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    out.setCodec("UTF-8");
#endif
    out << doc.toString();
    if (file.error() != QFile::NoError) {
        KMessageBox::error(qApp->activeWindow(), i18n("Cannot write to file %1", convertedFile));
        file.close();
        return;
    }
    file.close();
    // Copy subtitle file if any
    if (QFile::exists(currentFile + QStringLiteral(".srt"))) {
        QFile(currentFile + QStringLiteral(".srt")).copy(convertedFile + QStringLiteral(".srt"));
    }
    openFile(QUrl::fromLocalFile(convertedFile));
    pCore->displayBinMessage(i18n("Project profile changed"), KMessageWidget::Information);
}

QPair<int, int> ProjectManager::avTracksCount()
{
    return pCore->window()->getCurrentTimeline()->controller()->getAvTracksCount();
}

void ProjectManager::addAudioTracks(int tracksCount)
{
    pCore->window()->getCurrentTimeline()->controller()->addTracks(0, tracksCount);
}

void ProjectManager::initSequenceProperties(const QUuid &uuid, std::pair<int, int> tracks)
{
    // Initialize default timeline properties
    m_project->setSequenceProperty(uuid, QStringLiteral("documentuuid"), m_project->uuid().toString());
    m_project->setSequenceProperty(uuid, QStringLiteral("zoom"), 8);
    m_project->setSequenceProperty(uuid, QStringLiteral("verticalzoom"), 1);
    m_project->setSequenceProperty(uuid, QStringLiteral("zonein"), 0);
    m_project->setSequenceProperty(uuid, QStringLiteral("zoneout"), 75);
    m_project->setSequenceProperty(uuid, QStringLiteral("tracks"), tracks.first + tracks.second);
    m_project->setSequenceProperty(uuid, QStringLiteral("hasAudio"), tracks.first > 0 ? 1 : 0);
    m_project->setSequenceProperty(uuid, QStringLiteral("hasVideo"), tracks.second > 0 ? 1 : 0);
    const int activeTrack = tracks.second > 0 ? tracks.first : tracks.first - 1;
    m_project->setSequenceProperty(uuid, QStringLiteral("activeTrack"), activeTrack);
}

bool ProjectManager::openTimeline(const QString &id, const QUuid &uuid, int position, bool duplicate, std::shared_ptr<TimelineItemModel> existingModel)
{
    if (position > -1) {
        m_project->setSequenceProperty(uuid, QStringLiteral("position"), position);
    }
    if (pCore->window() && pCore->window()->raiseTimeline(uuid)) {
        return true;
    }
    if (!duplicate && existingModel == nullptr) {
        existingModel = m_project->getTimeline(uuid, true);
    }

    // Disable autosave while creating timelines
    m_autoSaveTimer.stop();
    std::shared_ptr<ProjectClip> clip = pCore->projectItemModel()->getClipByBinID(id);
    std::unique_ptr<Mlt::Producer> xmlProd = nullptr;
    // Check if a tractor for this playlist already exists in the main timeline
    std::shared_ptr<Mlt::Tractor> tc = pCore->projectItemModel()->getExtraTimeline(uuid.toString());
    bool internalLoad = false;
    if (tc != nullptr && tc->is_valid()) {
        internalLoad = true;
        if (tc->count() == 0) {
            // Corrupted timeline, abort and propose to open a backup
            return false;
        }
        if (duplicate) {
            pCore->projectItemModel()->setExtraTimelineSaved(uuid.toString());
        }
    } else {
        xmlProd.reset(new Mlt::Producer(clip->originalProducer().get()));
        if (xmlProd == nullptr || !xmlProd->is_valid()) {
            qDebug() << "::: LOADING EXTRA TIMELINE ERROR\n\nXXXXXXXXXXXXXXXXXXXXXXX";
            pCore->displayBinMessage(i18n("Cannot create a timeline from this clip:\n%1", clip->url()), KMessageWidget::Information);
            if (m_project->isModified()) {
                m_autoSaveTimer.start();
            }
            return false;
        }
    }

    // Build timeline
    std::shared_ptr<TimelineItemModel> timelineModel = existingModel != nullptr ? existingModel : TimelineItemModel::construct(uuid, m_project->commandStack());
    m_project->addTimeline(uuid, timelineModel);
    timelineModel->isClosed = false;
    TimelineWidget *timeline = nullptr;
    if (internalLoad) {
        qDebug() << "QQQQQQQQQQQQQQQQQQQQ\nINTERNAL SEQUENCE LOAD\n\nQQQQQQQQQQQQQQQQQQQQQQ";
        qDebug() << "============= LOADING INTERNAL PLAYLIST: " << uuid;
        const QString chunks = m_project->getSequenceProperty(uuid, QStringLiteral("previewchunks"));
        const QString dirty = m_project->getSequenceProperty(uuid, QStringLiteral("dirtypreviewchunks"));
        if (existingModel == nullptr && !constructTimelineFromTractor(timelineModel, nullptr, *tc.get(), m_project->modifiedDecimalPoint(), chunks, dirty)) {
            qDebug() << "===== LOADING PROJECT INTERNAL ERROR";
        }
        std::shared_ptr<Mlt::Producer> prod = std::make_shared<Mlt::Producer>(timelineModel->tractor());

        // Load stored sequence properties
        Mlt::Properties playlistProps(tc->get_properties());
        Mlt::Properties sequenceProperties;
        sequenceProperties.pass_values(playlistProps, "kdenlive:sequenceproperties.");
        for (int i = 0; i < sequenceProperties.count(); i++) {
            m_project->setSequenceProperty(uuid, qstrdup(sequenceProperties.get_name(i)), qstrdup(sequenceProperties.get(i)));
        }
        prod->set("kdenlive:duration", prod->frames_to_time(timelineModel->duration()));
        prod->set("kdenlive:maxduration", timelineModel->duration());
        prod->set("length", timelineModel->duration());
        prod->set("out", timelineModel->duration() - 1);
        prod->set("kdenlive:clipname", clip->clipName().toUtf8().constData());
        prod->set("kdenlive:description", clip->description().toUtf8().constData());
        prod->set("kdenlive:uuid", uuid.toString().toUtf8().constData());
        prod->set("kdenlive:producer_type", ClipType::Timeline);

        prod->parent().set("kdenlive:duration", prod->frames_to_time(timelineModel->duration()));
        prod->parent().set("kdenlive:maxduration", timelineModel->duration());
        prod->parent().set("length", timelineModel->duration());
        prod->parent().set("out", timelineModel->duration() - 1);
        prod->parent().set("kdenlive:clipname", clip->clipName().toUtf8().constData());
        prod->parent().set("kdenlive:description", clip->description().toUtf8().constData());
        prod->parent().set("kdenlive:uuid", uuid.toString().toUtf8().constData());
        prod->parent().set("kdenlive:producer_type", ClipType::Timeline);
        QObject::connect(timelineModel.get(), &TimelineModel::durationUpdated, this, &ProjectManager::updateSequenceDuration, Qt::UniqueConnection);
        timelineModel->setMarkerModel(clip->markerModel());
        m_project->loadSequenceGroupsAndGuides(uuid);
        clip->setProducer(prod, false, false);
        if (!duplicate) {
            clip->reloadTimeline(timelineModel->getMasterEffectStackModel());
        }
    } else {
        qDebug() << "GOT XML SERV: " << xmlProd->type() << " = " << xmlProd->parent().type();
        // Mlt::Service s(xmlProd->producer()->get_service());
        std::unique_ptr<Mlt::Tractor> tractor;
        if (xmlProd->type() == mlt_service_tractor_type) {
            tractor.reset(new Mlt::Tractor(*xmlProd.get()));
        } else if (xmlProd->type() == mlt_service_producer_type) {
            tractor.reset(new Mlt::Tractor((mlt_tractor)xmlProd->get_producer()));
            tractor->set("id", uuid.toString().toUtf8().constData());
        }
        // Load sequence properties from the xml producer
        Mlt::Properties playlistProps(xmlProd->get_properties());
        Mlt::Properties sequenceProperties;
        sequenceProperties.pass_values(playlistProps, "kdenlive:sequenceproperties.");
        for (int i = 0; i < sequenceProperties.count(); i++) {
            m_project->setSequenceProperty(uuid, qstrdup(sequenceProperties.get_name(i)), qstrdup(sequenceProperties.get(i)));
        }

        const QUuid sourceDocUuid(m_project->getSequenceProperty(uuid, QStringLiteral("documentuuid")));
        if (sourceDocUuid == m_project->uuid()) {
            qDebug() << "WWWWWWWWWWWWWWWWW\n\n\nIMPORTING FROM SAME PROJECT\n\nWWWWWWWWWWWWWWW";
        } else {
            qDebug() << "WWWWWWWWWWWWWWWWW\n\nImporting a sequence from another project: " << sourceDocUuid << " = " << m_project->uuid()
                     << "\n\nWWWWWWWWWWWWWWW";
            pCore->displayMessage(i18n("Importing a sequence clip, this is currently in experimental state"), ErrorMessage);
        }
        const QString chunks = m_project->getSequenceProperty(uuid, QStringLiteral("previewchunks"));
        const QString dirty = m_project->getSequenceProperty(uuid, QStringLiteral("dirtypreviewchunks"));
        if (!constructTimelineFromTractor(timelineModel, sourceDocUuid == m_project->uuid() ? nullptr : pCore->projectItemModel(), *tractor.get(),
                                          m_project->modifiedDecimalPoint(), chunks, dirty)) {
            // if (!constructTimelineFromMelt(timelineModel, *tractor.get(), m_progressDialog, m_project->modifiedDecimalPoint(), chunks, dirty)) {
            //  TODO: act on project load failure
            qDebug() << "// Project failed to load!!";
            if (m_project->isModified()) {
                m_autoSaveTimer.start();
            }
            return false;
        }
        qDebug() << "::: SEQUENCE LOADED WITH TRACKS: " << timelineModel->tractor()->count() << "\nZZZZZZZZZZZZ";
        std::shared_ptr<Mlt::Producer> prod = std::make_shared<Mlt::Producer>(timelineModel->tractor());
        prod->set("kdenlive:duration", timelineModel->tractor()->frames_to_time(timelineModel->duration()));
        prod->set("kdenlive:maxduration", timelineModel->duration());
        prod->set("length", timelineModel->duration());
        prod->set("kdenlive:producer_type", ClipType::Timeline);
        prod->set("out", timelineModel->duration() - 1);
        prod->set("kdenlive:clipname", clip->clipName().toUtf8().constData());
        prod->set("kdenlive:description", clip->description().toUtf8().constData());
        prod->set("kdenlive:uuid", uuid.toString().toUtf8().constData());

        prod->parent().set("kdenlive:duration", prod->frames_to_time(timelineModel->duration()));
        prod->parent().set("kdenlive:maxduration", timelineModel->duration());
        prod->parent().set("length", timelineModel->duration());
        prod->parent().set("out", timelineModel->duration() - 1);
        prod->parent().set("kdenlive:clipname", clip->clipName().toUtf8().constData());
        prod->parent().set("kdenlive:description", clip->description().toUtf8().constData());
        prod->parent().set("kdenlive:uuid", uuid.toString().toUtf8().constData());
        prod->parent().set("kdenlive:producer_type", ClipType::Timeline);
        timelineModel->setMarkerModel(clip->markerModel());
        if (pCore->bin()) {
            pCore->bin()->updateSequenceClip(uuid, timelineModel->duration(), -1);
        }
        updateSequenceProducer(uuid, prod);
        clip->setProducer(prod, false, false);
        m_project->loadSequenceGroupsAndGuides(uuid);
    }
    if (pCore->window()) {
        // Create tab widget
        timeline = pCore->window()->openTimeline(uuid, clip->clipName(), timelineModel);
    }

    int activeTrackPosition = m_project->getSequenceProperty(uuid, QStringLiteral("activeTrack"), QString::number(-1)).toInt();
    if (timeline == nullptr) {
        // We are in testing mode
        return true;
    }
    if (activeTrackPosition == -2) {
        // Subtitle model track always has ID == -2
        timeline->controller()->setActiveTrack(-2);
    } else if (activeTrackPosition > -1 && activeTrackPosition < timeline->model()->getTracksCount()) {
        // otherwise, convert the position to a track ID
        timeline->controller()->setActiveTrack(timeline->model()->getTrackIndexFromPosition(activeTrackPosition));
    } else {
        qWarning() << "[BUG] \"activeTrack\" property is" << activeTrackPosition << "but track count is only" << timeline->model()->getTracksCount();
        // set it to some valid track instead
        timeline->controller()->setActiveTrack(timeline->model()->getTrackIndexFromPosition(0));
    }
    /*if (m_renderWidget) {
        slotCheckRenderStatus();
        m_renderWidget->setGuides(m_project->getGuideModel());
        m_renderWidget->updateDocumentPath();
        m_renderWidget->setRenderProfile(m_project->getRenderProperties());
        m_renderWidget->updateMetadataToolTip();
    }*/
    pCore->window()->raiseTimeline(timeline->getUuid());
    pCore->bin()->updateTargets();
    if (m_project->isModified()) {
        m_autoSaveTimer.start();
    }
    return true;
}

void ProjectManager::setTimelinePropery(QUuid uuid, const QString &prop, const QString &val)
{
    std::shared_ptr<TimelineItemModel> model = m_project->getTimeline(uuid);
    if (model) {
        model->tractor()->set(prop.toUtf8().constData(), val.toUtf8().constData());
    }
}

int ProjectManager::getTimelinesCount() const
{
    return pCore->projectItemModel()->sequenceCount();
}

void ProjectManager::syncTimeline(const QUuid &uuid, bool refresh)
{
    std::shared_ptr<TimelineItemModel> model = m_project->getTimeline(uuid);
    doSyncTimeline(model, refresh);
}

void ProjectManager::doSyncTimeline(std::shared_ptr<TimelineItemModel> model, bool refresh)
{
    if (model) {
        std::shared_ptr<Mlt::Producer> prod = std::make_shared<Mlt::Producer>(model->tractor());
        int position = -1;
        if (model == m_activeTimelineModel) {
            position = pCore->getMonitorPosition();
        }
        const QUuid &uuid = model->uuid();
        if (refresh) {
            // Store sequence properties for later re-use
            Mlt::Properties sequenceProps;
            sequenceProps.pass_values(*model->tractor(), "kdenlive:sequenceproperties.");
            pCore->currentDoc()->loadSequenceProperties(uuid, sequenceProps);
        }
        updateSequenceProducer(uuid, prod);
        if (pCore->bin()) {
            pCore->bin()->updateSequenceClip(uuid, model->duration(), position);
        }
    }
}

bool ProjectManager::closeTimeline(const QUuid &uuid, bool onDeletion, bool clearUndo)
{
    std::shared_ptr<TimelineItemModel> model = m_project->getTimeline(uuid);
    if (model == nullptr) {
        qDebug() << "=== ERROR CANNOT FIND TIMELINE TO CLOSE: " << uuid << "\n\nHHHHHHHHHHHH";
        return false;
    }
    pCore->projectItemModel()->setExtraTimelineSaved(uuid.toString());
    if (onDeletion) {
        // triggered when deleting bin clip, also close timeline tab
        pCore->projectItemModel()->removeReferencedClips(uuid, true);
        if (pCore->window()) {
            pCore->window()->closeTimelineTab(uuid);
        }
    } else {
        if (!m_project->closing && !onDeletion) {
            if (m_project->isModified()) {
                syncTimeline(uuid);
            }
        }
    }
    m_project->closeTimeline(uuid, onDeletion);
    // The undo stack keeps references to guides model and will crash on undo if not cleared
    if (clearUndo) {
        qDebug() << ":::::::::::::: WARNING CLEARING NUDO STACK\n\n:::::::::::::::::";
        undoStack()->clear();
    }
    if (!m_project->closing) {
        m_project->setModified(true);
    }
    return true;
}

void ProjectManager::seekTimeline(const QString &frameAndTrack)
{
    int frame;
    if (frameAndTrack.contains(QLatin1Char('!'))) {
        QUuid uuid(frameAndTrack.section(QLatin1Char('!'), 0, 0));
        const QString binId = pCore->projectItemModel()->getSequenceId(uuid);
        openTimeline(binId, uuid);
        frame = frameAndTrack.section(QLatin1Char('!'), 1).section(QLatin1Char('?'), 0, 0).toInt();
    } else {
        frame = frameAndTrack.section(QLatin1Char('?'), 0, 0).toInt();
    }
    if (frameAndTrack.contains(QLatin1Char('?'))) {
        // Track and timecode info
        int track = frameAndTrack.section(QLatin1Char('?'), 1, 1).toInt();
        // Track uses MLT index, so remove 1 to discard black background track
        if (track > 0) {
            track--;
        }
        pCore->window()->getCurrentTimeline()->controller()->activateTrackAndSelect(track, true);
    } else {
        frame = frameAndTrack.toInt();
    }
    pCore->monitorManager()->projectMonitor()->requestSeek(frame);
}

void ProjectManager::slotCreateSequenceFromSelection()
{
    std::function<bool(void)> undo = []() { return true; };
    std::function<bool(void)> redo = []() { return true; };
    int aTracks = -1;
    int vTracks = -1;
    std::pair<int, QString> copiedData = pCore->window()->getCurrentTimeline()->controller()->getCopyItemData();
    if (copiedData.first == -1) {
        pCore->displayMessage(i18n("Select a clip to create sequence"), InformationMessage);
        return;
    }
    const QUuid sourceSequence = pCore->window()->getCurrentTimeline()->getUuid();
    std::pair<int, int> vPosition = pCore->window()->getCurrentTimeline()->controller()->selectionPosition(&aTracks, &vTracks);
    pCore->window()->getCurrentTimeline()->model()->requestItemDeletion(copiedData.first, undo, redo, true);
    const QString newSequenceId = pCore->bin()->buildSequenceClipWithUndo(undo, redo, aTracks, vTracks);
    if (newSequenceId.isEmpty()) {
        // Action canceled
        undo();
        return;
    }
    const QUuid destSequence = pCore->window()->getCurrentTimeline()->getUuid();
    int trackId = pCore->window()->getCurrentTimeline()->controller()->activeTrack();
    Fun local_redo1 = [destSequence, copiedData]() {
        pCore->window()->raiseTimeline(destSequence);
        return true;
    };
    local_redo1();
    bool result = TimelineFunctions::pasteClipsWithUndo(m_activeTimelineModel, copiedData.second, trackId, 0, undo, redo);
    if (!result) {
        undo();
        return;
    }
    PUSH_LAMBDA(local_redo1, redo);
    Fun local_redo = [sourceSequence]() {
        pCore->window()->raiseTimeline(sourceSequence);
        return true;
    };
    local_redo();
    PUSH_LAMBDA(local_redo, redo);
    int newId;
    result = m_activeTimelineModel->requestClipInsertion(newSequenceId, vPosition.second, vPosition.first, newId, false, true, false, undo, redo, {});
    if (!result) {
        undo();
        return;
    }
    m_activeTimelineModel->updateDuration();
    pCore->pushUndo(undo, redo, i18n("Create Sequence Clip"));
}

void ProjectManager::updateSequenceProducer(const QUuid &uuid, std::shared_ptr<Mlt::Producer> prod)
{
    // On timeline close, update the stored sequence producer
    std::shared_ptr<Mlt::Tractor> trac(new Mlt::Tractor(prod->parent()));
    qDebug() << "====== STORING SEQUENCE " << uuid << " WITH TKS: " << trac->count();
    pCore->projectItemModel()->storeSequence(uuid.toString(), trac);
}

void ProjectManager::replaceTimelineInstances(const QString &sourceId, const QString &replacementId, bool replaceAudio, bool replaceVideo)
{
    std::shared_ptr<ProjectClip> currentItem = pCore->projectItemModel()->getClipByBinID(sourceId);
    std::shared_ptr<ProjectClip> replacementItem = pCore->projectItemModel()->getClipByBinID(replacementId);
    if (!currentItem || !replacementItem || !m_activeTimelineModel) {
        qDebug() << " SOURCE CLI : " << sourceId << " NOT FOUND!!!";
        return;
    }
    int maxDuration = replacementItem->frameDuration();
    QList<int> instances = currentItem->timelineInstances();
    m_activeTimelineModel->processTimelineReplacement(instances, sourceId, replacementId, maxDuration, replaceAudio, replaceVideo);
}

void ProjectManager::checkProjectIntegrity()
{
    // Ensure the active timeline sequence is correctly inserted in the main_bin playlist
    const QString activeSequenceId(m_activeTimelineModel->tractor()->get("id"));
    pCore->projectItemModel()->checkSequenceIntegrity(activeSequenceId);
}

void ProjectManager::handleLog(const QString &message)
{
    m_mltWarnings << message;
}
