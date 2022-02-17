/*
SPDX-FileCopyrightText: 2014 Till Theato <root@ttill.de>
SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "projectmanager.h"
#include "bin/bin.h"
#include "bin/projectitemmodel.h"
#include "bin/projectclip.h"
#include "core.h"
#include "doc/kdenlivedoc.h"
#include "doc/documentobjectmodel.h"
#include "kdenlivesettings.h"
#include "mainwindow.h"
#include "monitor/monitormanager.h"
#include "profiles/profilemodel.hpp"
#include "project/dialogs/archivewidget.h"
#include "project/dialogs/backupwidget.h"
#include "project/dialogs/noteswidget.h"
#include "project/dialogs/projectsettings.h"
#include "utils/thumbnailcache.hpp"
#include "xml/xml.hpp"

// Temporary for testing
#include "bin/model/markerlistmodel.hpp"

#include "profiles/profilerepository.hpp"
#include "project/notesplugin.h"
#include "timeline2/model/builders/meltBuilder.hpp"
#include "timeline2/view/timelinecontroller.h"
#include "timeline2/view/timelinewidget.h"

#include <KActionCollection>
#include <KJob>
#include <KMessageBox>
#include <KRecentDirs>
#include <kcoreaddons_version.h>
#include <klocalizedstring.h>

#include "kdenlive_debug.h"
#include <KConfigGroup>
#include <QAction>
#include <QCryptographicHash>
#include <QFileDialog>
#include <QLocale>
#include <QMimeDatabase>
#include <QMimeType>
#include <QProgressDialog>
#include <QTimeZone>
#include <audiomixer/mixermanager.hpp>
#include <lib/localeHandling.h>
#include <bin/clipcreator.hpp>

static QString getProjectNameFilters(bool ark=true) {
    auto filter = i18n("Kdenlive Project (*.kdenlive)");
    if (ark) {
        filter.append(";;" + i18n("Archived Project (*.tar.gz *.zip)"));
    }
    return filter;
}


ProjectManager::ProjectManager(QObject *parent)
    : QObject(parent)
    , m_mainTimelineModel(nullptr)
    , m_project(nullptr)
{
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
    connect(saveCopyAction,  &QAction::triggered, this, [this]{ saveFileAs(true); });

    QAction *backupAction = new QAction(QIcon::fromTheme(QStringLiteral("edit-undo")), i18n("Open Backup File…"), this);
    pCore->window()->addAction(QStringLiteral("open_backup"), backupAction);
    connect(backupAction, SIGNAL(triggered(bool)), SLOT(slotOpenBackup()));

    m_notesPlugin = new NotesPlugin(this);

    m_autoSaveTimer.setSingleShot(true);
    connect(&m_autoSaveTimer, &QTimer::timeout, this, &ProjectManager::slotAutoSave);

    // Ensure the default data folder exist
    QDir dir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation));
    dir.mkpath(QStringLiteral(".backup"));
    dir.mkdir(QStringLiteral("titles"));
}

ProjectManager::~ProjectManager() = default;

void ProjectManager::slotLoadOnOpen()
{
    m_loading = true;
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
    m_loading = false;
    emit pCore->closeSplash();
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
    });
}

void ProjectManager::init(const QUrl &projectUrl, const QString &clipList)
{
    m_startUrl = projectUrl;
    m_loadClipsOnOpen = clipList;
}

void ProjectManager::newFile(bool showProjectSettings, bool closeCurrent)
{
    QString profileName = KdenliveSettings::default_profile();
    if (profileName.isEmpty()) {
        profileName = pCore->getCurrentProfile()->path();
    }
    newFile(profileName, showProjectSettings, closeCurrent);
}

void ProjectManager::activateDocument(const QUuid &uuid)
{
    qDebug()<<"===== ACTIVATING DOCUMENT: "<<uuid<<"\n::::::::::::::::::::::";
    /*if (m_project && (m_project->uuid == uuid || m_openedDocuments.value(uuid) == m_project)) {
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
    m_project = m_openedDocuments.value(uuid);
    m_fileRevert->setEnabled(m_project->isModified());
    m_notesPlugin->clear();
    emit docOpened(m_project);

    auto match = m_timelineModels.find(uuid.toString());
    if (match == m_timelineModels.end()) {
        qDebug()<<"=== ERROR";
        return;
    }
    m_mainTimelineModel = match->second;
    if (m_project->documentProfile() != pCore->getCurrentProfilePath()) {
        qDebug()<<"=== CHANGING PROJECT PROFILE TO: "<<m_project->documentProfile();
        pCore->setCurrentProfile(m_project->documentProfile());
    }

    pCore->setProjectItemModel(uuid);
    pCore->bin()->setProjectModel(pCore->projectItemModel());
    pCore->bin()->setDocument(m_project);
    pCore->window()->connectDocument();
    pCore->window()->raiseTimeline(uuid);
    pCore->window()->slotSwitchTimelineZone(m_project->getDocumentProperty(QStringLiteral("enableTimelineZone")).toInt() == 1);
    emit pCore->monitorManager()->updatePreviewScaling();
    //pCore->monitorManager()->projectMonitor()->slotActivateMonitor();
}

void ProjectManager::newFile(QString profileName, bool showProjectSettings, bool closeCurrent)
{
    if (closeCurrent && !closeCurrentDocument()) {
        return;
    }
    QUrl startFile = QUrl::fromLocalFile(KdenliveSettings::defaultprojectfolder() + QStringLiteral("/_untitled.kdenlive"));
    if (checkForBackupFile(startFile, true)) {
        return;
    }
    QString projectFolder;
    QMap<QString, QString> documentProperties;
    QMap<QString, QString> documentMetadata;
    QPair<int, int> projectTracks{KdenliveSettings::videotracks(), KdenliveSettings::audiotracks()};
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
        if (KdenliveSettings::customprojectfolder()) {
            projectFolder = KdenliveSettings::defaultprojectfolder();
            if (!projectFolder.endsWith(QLatin1Char('/'))) {
                projectFolder.append(QLatin1Char('/'));
            }
            documentProperties.insert(QStringLiteral("storagefolder"), projectFolder + documentId);
        }
    } else {
        QPointer<ProjectSettings> w = new ProjectSettings(nullptr, QMap<QString, QString>(), QStringList(), projectTracks.first, projectTracks.second, audioChannels, KdenliveSettings::defaultprojectfolder(), false, true, pCore->window());
        connect(w.data(), &ProjectSettings::refreshProfiles, pCore->window(), &MainWindow::slotRefreshProfiles);
        if (w->exec() != QDialog::Accepted) {
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
        sameProjectFolder = w->docFolderAsStorageFolder();
        documentMetadata = w->metadata();
        delete w;
    }
    bool openBackup;
    if (closeCurrent) {
        pCore->bin()->cleanDocument();
        ThumbnailCache::get()->clearCache();
    }
    KdenliveDoc *doc = new KdenliveDoc(QUrl(), projectFolder, profileName, documentProperties, documentMetadata, projectTracks, audioChannels, &openBackup, pCore->window());
    doc->m_autosave = new KAutoSaveFile(startFile, doc);
    m_openedDocuments.insert(doc->uuid, doc);
    doc->m_sameProjectFolder = sameProjectFolder;
    ThumbnailCache::get()->clearCache();
    pCore->bin()->setDocument(doc);
    m_project = doc;
    pCore->monitorManager()->activateMonitor(Kdenlive::ProjectMonitor);
    updateTimeline(doc, 0, true, QString(), QString(), QDateTime(), 0);
    pCore->window()->connectDocument();
    //TODO-MULTITL doc is not yet activated so main model not available
    //pCore->mixer()->setModel(m_mainTimelineModel);
    bool disabled = m_project->getDocumentProperty(QStringLiteral("disabletimelineeffects")) == QLatin1String("1");
    QAction *disableEffects = pCore->window()->actionCollection()->action(QStringLiteral("disable_timeline_effects"));
    if (disableEffects) {
        if (disabled != disableEffects->isChecked()) {
            disableEffects->blockSignals(true);
            disableEffects->setChecked(disabled);
            disableEffects->blockSignals(false);
        }
    }
    m_lastSave.start();
}

bool ProjectManager::closeDocument(const QUuid &uuid)
{
    KdenliveDoc *doc = m_openedDocuments.value(uuid);
    if (doc && doc->isModified()) {
        QString message;
        if (doc->url().fileName().isEmpty()) {
            message = i18n("Save changes to document?");
        } else {
            message = i18n("The project <b>\"%1\"</b> has been changed.\nDo you want to save your changes?", doc->url().fileName());
        }

        switch (KMessageBox::warningYesNoCancel(pCore->window(), message)) {
        case KMessageBox::Yes:
            // save document here. If saving fails, return false;
            // TODO: save document with uuid
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
    doc->objectModel()->timeline()->unsetModel();
    m_openedDocuments.remove(uuid);
    pCore->deleteProjectModel(uuid);
    delete doc;
    // TODO: handle subtitles
    //pCore->window()->resetSubtitles();
    //pCore->bin()->cleanDocument();
    return true;
}

void ProjectManager::testSetActiveDocument(KdenliveDoc *doc, std::shared_ptr<TimelineItemModel> timeline)
{
    m_project = doc;
    m_mainTimelineModel = timeline;
}

bool ProjectManager::testSaveFileAs(const QString &outputFileName)
{
    QString saveFolder = QFileInfo(outputFileName).absolutePath();
    QMap<QString,QString>docProperties;
    docProperties.insert(QStringLiteral("version"), QString::number(m_project->getDocumentVersion()));
    docProperties.insert(QStringLiteral("timelineHash"), m_mainTimelineModel->timelineHash().toHex());
    pCore->projectItemModel()->saveDocumentProperties(docProperties, QMap<QString,QString>(),
                                                      m_project->getGuideModel(pCore->window()->getCurrentTimeline()->uuid));
    QString scene = m_mainTimelineModel->sceneList(saveFolder);
    
    QSaveFile file(outputFileName);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qDebug()<<"//////  ERROR writing to file: " << outputFileName;
        return false;
    }

    file.write(scene.toUtf8());
    if (!file.commit()) {
        qDebug()<<"Cannot write to file %1";
        return false;
    }
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

        switch (KMessageBox::warningYesNoCancel(pCore->window(), message)) {
        case KMessageBox::Yes:
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
    if (m_project) {
        ::mlt_pool_purge();
        pCore->cleanup();
        disconnect(pCore->window()->getCurrentTimeline()->controller(), &TimelineController::durationChanged, this, &ProjectManager::adjustProjectDuration);
        pCore->window()->getCurrentTimeline()->controller()->clipActions.clear();
        if (!quit && !qApp->isSavingSession()) {
            pCore->bin()->abortOperations();
        }
        
        //pCore->window()->getCurrentTimeline()->unsetModel();
        //pCore->window()->resetSubtitles();
        //pCore->window()->closeTimelines();
        if (m_mainTimelineModel) {
            m_mainTimelineModel->prepareClose();
        }
    }
    if (m_project) {
        pCore->window()->closeDocument(m_project->uuid);
        pCore->bin()->cleanDocument();
    }
    if (!quit && !qApp->isSavingSession() && m_project) {
        emit pCore->window()->clearAssetPanel();
        pCore->monitorManager()->clipMonitor()->slotOpenClip(nullptr);
        pCore->monitorManager()->projectMonitor()->setProducer(nullptr);
        delete m_project;
        m_project = nullptr;
    }
    pCore->mixer()->unsetModel();
    // Release model shared pointers
    m_mainTimelineModel.reset();
    return true;
}

bool ProjectManager::saveFileAs(const QString &outputFileName, bool saveACopy)
{
    pCore->monitorManager()->pauseActiveMonitor();
    QString oldProjectFolder = m_project->url().isEmpty()
            ? QString()
            : QFileInfo(m_project->url().toLocalFile()).absolutePath() + QStringLiteral("/cachefiles");
            // this was the old project folder in case the "save in project file location" setting was active

    // Sync document properties
    bool playlistSave = pCore->activeUuid() != m_project->uuid && m_uuidMap.value(pCore->activeUuid()) != m_project->uuid;
    if (!playlistSave && !saveACopy && outputFileName != m_project->url().toLocalFile()) {
        // Project filename changed
        pCore->window()->updateProjectPath(outputFileName);
    }
    if (!playlistSave) {
        prepareSave();
        m_project->updateSubtitle(outputFileName);
    }
    QString saveFolder = QFileInfo(outputFileName).absolutePath();
    QString scene = projectSceneList(saveFolder);
    if (playlistSave) {
        // This is a secondary timeline
        if (outputFileName.isEmpty()) {
            KMessageBox::error(QApplication::activeWindow(), i18n("Cannot find write path for timeline: %1", pCore->activeUuid().toString()));
            return false;
        }
        QSaveFile file(outputFileName);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            KMessageBox::error(QApplication::activeWindow(), i18n("Cannot write to file %1", outputFileName));
            return false;
        }

        const QByteArray sceneData = scene.toUtf8();
        file.write(sceneData);
        if (!file.commit()) {
            KMessageBox::error(QApplication::activeWindow(), i18n("Cannot write to file %1", outputFileName));
            return false;
        }
        pCore->displayBinMessage(i18n("Saved playlist %1", QFileInfo(outputFileName).fileName()), KMessageWidget::Information);
        return true;
    }
    if (!m_replacementPattern.isEmpty()) {
        QMapIterator<QString, QString> i(m_replacementPattern);
        while (i.hasNext()) {
            i.next();
            scene.replace(i.key(), i.value());
        }
    }
    if (!m_project->saveSceneList(outputFileName, scene)) {
        return false;
    }
    QUrl url = QUrl::fromLocalFile(outputFileName);
    // Save timeline thumbnails
    QStringList thumbKeys = pCore->window()->getCurrentTimeline()->controller()->getThumbKeys();
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
    m_recentFilesAction->addUrl(url);
    // remember folder for next project opening
    KRecentDirs::add(QStringLiteral(":KdenliveProjectsFolder"), saveFolder);
    saveRecentFiles();
    if (!saveACopy) {
        m_fileRevert->setEnabled(true);
        pCore->window()->m_undoView->stack()->setClean();
        QString newProjectFolder(saveFolder + QStringLiteral("/cachefiles"));
        if(((oldProjectFolder.isEmpty() && m_project->m_sameProjectFolder) || m_project->projectTempFolder() == oldProjectFolder) && newProjectFolder != m_project->projectTempFolder()) {
            KMessageBox::ButtonCode answer = KMessageBox::warningContinueCancel(
                pCore->window(), i18n("The location of the project file changed. You selected to use the location of the project file to save temporary files. "
                           "This will move all temporary files from <b>%1</b> to <b>%2</b>, the project file will then be reloaded",
                           m_project->projectTempFolder(), newProjectFolder));

            if (answer == KMessageBox::Continue) {
                // Proceed with move
                QString documentId = QDir::cleanPath(m_project->getDocumentProperty(QStringLiteral("documentid")));
                bool ok;
                documentId.toLongLong(&ok, 10);
                if (!ok || documentId.isEmpty()) {
                    KMessageBox::sorry(pCore->window(), i18n("Cannot perform operation, invalid document id: %1", documentId));
                } else {
                    QDir newDir(newProjectFolder);
                    QDir oldDir(m_project->projectTempFolder());
                    if (newDir.exists(documentId)) {
                        KMessageBox::sorry(pCore->window(), i18n("Cannot perform operation, target directory already exists: %1", newDir.absoluteFilePath(documentId)));
                    } else {
                        // Proceed with the move
                        pCore->projectManager()->moveProjectData(oldDir.absoluteFilePath(documentId), newDir.absolutePath());
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
    fd.setDirectory(m_project->url().isValid() ? m_project->url().adjusted(QUrl::RemoveFilename).toLocalFile() : KdenliveSettings::defaultprojectfolder());
    fd.setNameFilter(getProjectNameFilters(false));
    fd.setAcceptMode(QFileDialog::AcceptSave);
    fd.setFileMode(QFileDialog::AnyFile);
    fd.setDefaultSuffix(QStringLiteral("kdenlive"));
    if (fd.exec() != QDialog::Accepted || fd.selectedFiles().isEmpty()) {
        return false;
    }
    
    QString outputFile = fd.selectedFiles().constFirst();

    bool ok = false;
    QDir cacheDir = m_project->getCacheDir(CacheBase, &ok);
    if (ok) {
        QFile file(cacheDir.absoluteFilePath(QString::fromLatin1(QUrl::toPercentEncoding(QStringLiteral(".") + outputFile))));
        file.open(QIODevice::ReadWrite | QIODevice::Text);
        file.close();
    }
    return saveFileAs(outputFile, saveACopy);
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
        if (KMessageBox::questionYesNo(nullptr, i18n("Auto-saved file exist. Do you want to recover now?"), i18n("File Recovery"),
                                       KGuiItem(i18n("Recover")), KGuiItem(i18n("Do not recover"))) == KMessageBox::Yes) {
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
        KMessageBox::sorry(pCore->window(), i18n("File %1 is not a Kdenlive project file", url.toLocalFile()));
        if (m_startUrl.isValid()) {
            // we tried to open an invalid file from command line, init new project
            newFile(false);
        }
        return;
    }*/

    if ((m_project != nullptr) && m_project->url() == url) {
        return;
    }

    /*if (!closeCurrentDocument()) {
        return;
    }*/
    if (checkForBackupFile(url)) {
        return;
    }
    pCore->displayMessage(i18n("Opening file %1", url.toLocalFile()), OperationCompletedMessage, 100);
    doOpenFile(url, nullptr);
}

void ProjectManager::doOpenFile(const QUrl &url, KAutoSaveFile *stale)
{
    //Q_ASSERT(m_project == nullptr);
    m_fileRevert->setEnabled(true);

    delete m_progressDialog;
    m_progressDialog = nullptr;
    if (m_project && !closeCurrentDocument()) {
        return;
    }
    ThumbnailCache::get()->clearCache();
    pCore->monitorManager()->resetDisplay();
    pCore->monitorManager()->activateMonitor(Kdenlive::ProjectMonitor);
    if (!m_loading) {
        m_progressDialog = new QProgressDialog(pCore->window());
        m_progressDialog->setWindowTitle(i18nc("@title:window", "Loading Project"));
        m_progressDialog->setCancelButton(nullptr);
        m_progressDialog->setLabelText(i18n("Loading project"));
        m_progressDialog->setMaximum(0);
        m_progressDialog->show();
    }
    bool openBackup;
    m_notesPlugin->clear();
    int audioChannels = 2;
    if (KdenliveSettings::audio_channels() == 1) {
        audioChannels = 4;
    } else if (KdenliveSettings::audio_channels() == 2) {
        audioChannels = 6;
    }

    KdenliveDoc *doc = new KdenliveDoc(stale ? QUrl::fromLocalFile(stale->fileName()) : url, QString(),
                                       KdenliveSettings::default_profile().isEmpty() ? pCore->getCurrentProfile()->path() : KdenliveSettings::default_profile(),
                                       QMap<QString, QString>(), QMap<QString, QString>(),
                                       {KdenliveSettings::videotracks(), KdenliveSettings::audiotracks()}, audioChannels, &openBackup, pCore->window());
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
    if (m_progressDialog) {
        m_progressDialog->setLabelText(i18n("Loading clips"));
        m_progressDialog->setMaximum(doc->clipsCount());
    } else {
        emit pCore->loadingMessageUpdated(QString(), 0, doc->clipsCount());
    }

    m_openedDocuments.insert(doc->uuid, doc);
    
    // TODO-MULTITL refac delete this
    //pCore->bin()->setDocument(doc);
    /*QList<QAction *> rulerActions;
    rulerActions << pCore->window()->actionCollection()->action(QStringLiteral("set_render_timeline_zone"));
    rulerActions << pCore->window()->actionCollection()->action(QStringLiteral("unset_render_timeline_zone"));
    rulerActions << pCore->window()->actionCollection()->action(QStringLiteral("clear_render_timeline_zone"));*/
    

    // Set default target tracks to upper audio / lower video tracks
    m_project = doc;
    m_project->loadDocumentGuides();
    QDateTime documentDate = QFileInfo(m_project->url().toLocalFile()).lastModified();
    if (!updateTimeline(m_project, m_project->getDocumentProperty(QStringLiteral("position")).toInt(), true, m_project->getDocumentProperty(QStringLiteral("previewchunks")), m_project->getDocumentProperty(QStringLiteral("dirtypreviewchunks")), documentDate, m_project->getDocumentProperty(QStringLiteral("disablepreview")).toInt())) {
        delete m_progressDialog;
        m_progressDialog = nullptr;
        return;
    }
    emit docOpened(m_project);
    pCore->displayMessage(QString(), OperationCompletedMessage, 100);
    if (openBackup) {
        slotOpenBackup(url);
    }
    m_lastSave.start();
    delete m_progressDialog;
    m_progressDialog = nullptr;
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
        projectFolder = QUrl::fromLocalFile(m_project->projectTempFolder());
        projectFile = m_project->url();
        projectId = m_project->getDocumentProperty(QStringLiteral("documentid"));
    }
    bool result = false;
    QPointer<BackupWidget> dia = new BackupWidget(projectFile, projectFolder, projectId, pCore->window());
    if (dia->exec() == QDialog::Accepted) {
        QString requestedBackup = dia->selectedFile();
        m_project->backupLastSavedVersion(projectFile.toLocalFile());
        closeCurrentDocument(false);
        doOpenFile(QUrl::fromLocalFile(requestedBackup), nullptr);
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
        m_autoSaveTimer.start(3000); // will trigger slotAutoSave() in 3 seconds
    }
}

void ProjectManager::slotAutoSave()
{
    if (pCore->activeUuid() != QUuid()) {
        // Disable autosave for secondary timelines
        m_lastSave.start();
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
    bool isMultiTrack = pCore->monitorManager()->isMultiTrack();
    TimelineWidget *timeline = pCore->window()->getCurrentTimeline();
    bool hasPreview = timeline->controller()->hasPreviewTrack();
    bool isTrimming = pCore->monitorManager()->isTrimming();
    if (isMultiTrack) {
        timeline->controller()->slotMultitrackView(false, false);
    }
    if (hasPreview) {
        timeline->controller()->updatePreviewConnection(false);
    }
    if (isTrimming) {
        timeline->controller()->requestEndTrimmingMode();
    }
    pCore->mixer()->pauseMonitoring(true);

    LocaleHandling::resetLocale();
    Mlt::Consumer xmlConsumer(pCore->getCurrentProfile()->profile(), "xml", "kdenlive_playlist");
    if (!outputFolder.isEmpty()) {
        xmlConsumer.set("root", outputFolder.toUtf8().constData());
    }
    if (!xmlConsumer.is_valid()) {
        return QString();
    }
    xmlConsumer.set("store", "kdenlive");
    xmlConsumer.set("time_format", "clock");
    // Disabling meta creates cleaner files, but then we don't have access to metadata on the fly (meta channels, etc)
    // And we must use "avformat" instead of "avformat-novalidate" on project loading which causes a big delay on project opening
    // xmlConsumer.set("no_meta", 1);
    Mlt::Service s(m_mainTimelineModel->tractor()->get_service());
    std::unique_ptr<Mlt::Filter> filter = nullptr;
    if (!overlayData.isEmpty()) {
        filter = std::make_unique<Mlt::Filter>(pCore->getCurrentProfile()->profile(), QString("dynamictext:%1").arg(overlayData).toUtf8().constData());
        filter->set("fgcolour", "#ffffff");
        filter->set("bgcolour", "#bb333333");
        s.attach(*filter.get());
    }
    xmlConsumer.connect(s);
    xmlConsumer.run();
    if (filter) {
        s.detach(*filter.get());
    }
    QString scene = QString::fromUtf8(xmlConsumer.get("kdenlive_playlist"));

    //QString scene = m_mainTimelineModel->sceneList(outputFolder, QString(), overlayData);
    pCore->mixer()->pauseMonitoring(false);
    if (isMultiTrack) {
        timeline->controller()->slotMultitrackView(true, false);
    }
    if (hasPreview) {
        timeline->controller()->updatePreviewConnection(true);
    }
    if (isTrimming) {
        timeline->controller()->requestStartTrimmingMode();
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
    pCore->projectItemModel()->saveDocumentProperties(pCore->window()->getCurrentTimeline()->controller()->documentProperties(), m_project->metadata(),
                                                      m_project->getGuideModel(pCore->window()->getCurrentTimeline()->uuid));
    pCore->bin()->saveFolderState();
    pCore->projectItemModel()->saveProperty(QStringLiteral("kdenlive:documentnotes"), documentNotes());
    pCore->projectItemModel()->saveProperty(QStringLiteral("kdenlive:docproperties.groups"), m_mainTimelineModel->groupsData());
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
    m_mainTimelineModel->setTimelineEffectsEnabled(!disable);
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
    // Move tmp folder (thumbnails, timeline preview)
    m_project->moveProjectData(src, dest);
    KIO::CopyJob *copyJob = KIO::move(QUrl::fromLocalFile(src), QUrl::fromLocalFile(dest));
    connect(copyJob, &KJob::result, this, &ProjectManager::slotMoveFinished);
#if KCOREADDONS_VERSION < QT_VERSION_CHECK(5,80,0)
    connect(copyJob, SIGNAL(percent(KJob*,ulong)), this, SLOT(slotMoveProgress(KJob*,ulong)));
#else
    connect(copyJob, &KJob::percentChanged, this, &ProjectManager::slotMoveProgress);
#endif
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
        KMessageBox::sorry(pCore->window(), i18n("Error moving project folder: %1", job->errorText()));
    }
}

bool ProjectManager::updateTimeline(KdenliveDoc *doc, int pos, bool createNewTab, const QString &chunks, const QString &dirty, const QDateTime &documentDate, int enablePreview)
{
    QUuid uuid = doc->uuid;
    TimelineWidget *documentTimeline = nullptr;
    pCore->taskManager.slotCancelJobs();
    QScopedPointer<Mlt::Producer> xmlProd(new Mlt::Producer(pCore->getCurrentProfile()->profile(), "xml-string",
                                                            doc->getAndClearProjectXml().constData()));

    Mlt::Service s(*xmlProd);
    Mlt::Tractor tractor(s);
    if (tractor.count() == 0) {
        // Wow we have a project file with empty tractor, probably corrupted, propose to open a recovery file
        KMessageBox::ButtonCode res = KMessageBox::warningContinueCancel(qApp->activeWindow(), i18n("Project file is corrupted (no tracks). Try to find a backup file?"));
        documentTimeline->loading = false;
        doc->setModified(false);
        if (res == KMessageBox::Continue) {
            // Try opening backup
            if (!slotOpenBackup(doc->url())) {
                newFile(false);
            }
        } else {
            newFile(false);
        }
        return false;
    }
    
    
    std::shared_ptr<TimelineItemModel> timelineModel = TimelineItemModel::construct(uuid, doc->getDocumentProfile(), doc->getGuideModel(uuid), doc->commandStack());
    // Add snap point at projec start
    timelineModel->addSnap(0);
    m_timelineModels.insert({uuid.toString(), timelineModel});
    if (!createNewTab) {
        pCore->taskManager.slotCancelJobs();
        documentTimeline = pCore->window()->getCurrentTimeline();
        doc->setModels(documentTimeline, pCore->getProjectItemModel(uuid));
        documentTimeline->setModel(timelineModel, pCore->monitorManager()->projectMonitor()->getControllerProxy());
    } else {
        // Create a new timeline tab
        pCore->buildProjectModel(uuid);
        documentTimeline = pCore->window()->openTimeline(uuid, doc->projectName(), timelineModel, pCore->monitorManager()->projectMonitor()->getControllerProxy());
        doc->setModels(documentTimeline, pCore->getProjectItemModel(uuid));
        //std::shared_ptr<MarkerListModel> guidesModel(new MarkerListModel(doc->commandStack(), this));
        //m_project->addGuidesModel(uuid, guidesModel);
        //std::shared_ptr<TimelineItemModel> timelineModel = TimelineItemModel::construct(uuid, pCore->getProjectProfile(), doc->getGuideModel(timeline->uuid), doc->commandStack());
        //m_secondaryTimelines.insert({timelineModel,uuid});
        //m_secondaryTimelineEntries.insert({clipId,timeline->uuid});
        //m_timelinePath.insert(timeline->uuid, clip->url());
        //timeline->setModel(timelineModel, pCore->monitorManager()->projectMonitor()->getControllerProxy());
    }
    qDebug()<<"&&&&&&&&& serring timeline model: "<<uuid;
    //documentTimeline->setModel(timelineModel, pCore->monitorManager()->projectMonitor()->getControllerProxy());
    //pCore->window()->slotSwitchTimelineZone(m_project->getDocumentProperty(QStringLiteral("enableTimelineZone")).toInt() == 1);
    documentTimeline->loading = true;
    bool projectErrors = false;
    if (!constructTimelineFromTractor(uuid, timelineModel, pCore->getProjectItemModel(uuid), tractor, m_progressDialog, doc->modifiedDecimalPoint(), doc->getSecondaryTimelines(), chunks, dirty, documentDate, enablePreview, &projectErrors)) {
        //TODO: act on project load failure
        qDebug()<<"// Project failed to load!!";
    }
    // Free memory used by original playlist
    xmlProd.reset(nullptr);
    const QString groupsData = m_project->getDocumentProperty(QStringLiteral("groups"));
    // update track compositing
    int compositing = pCore->currentDoc()->getDocumentProperty(QStringLiteral("compositing"), QStringLiteral("2")).toInt();
    if (compositing == 1) {
        // Composite transition is deprecated, switch to hq by default
        compositing = 2;
    }
    emit pCore->currentDoc()->updateCompositionMode(compositing);
    if (compositing < 2) {
        documentTimeline->controller()->switchCompositing(compositing);
    }
    if (!groupsData.isEmpty()) {
        timelineModel->loadGroups(groupsData);
    }
    connect(documentTimeline->controller(), &TimelineController::durationChanged, this, &ProjectManager::adjustProjectDuration);
    emit pCore->monitorManager()->updatePreviewScaling();
    pCore->monitorManager()->projectMonitor()->slotActivateMonitor();
    pCore->monitorManager()->projectMonitor()->setProducer(timelineModel->producer(), pos);
    pCore->monitorManager()->projectMonitor()->adjustRulerSize(timelineModel->duration() - 1, doc->getGuideModel(uuid));
    documentTimeline->controller()->setZone(doc->zone(), false);
    documentTimeline->controller()->setScrollPos(doc->getDocumentProperty(QStringLiteral("scrollPos")).toInt());
    int activeTrackPosition = doc->getDocumentProperty(QStringLiteral("activeTrack"), QString::number( - 1)).toInt();
    if (activeTrackPosition > -1 && activeTrackPosition < timelineModel->getTracksCount()) {
        documentTimeline->controller()->setActiveTrack(timelineModel->getTrackIndexFromPosition(activeTrackPosition));
    } else {
        // Subtitle model track was active
        documentTimeline->controller()->setActiveTrack(activeTrackPosition);
    }
    //timelineModel->setUndoStack(doc->commandStack());
    
    // Reset locale to C to ensure numbers are serialised correctly
    LocaleHandling::resetLocale();
    if (projectErrors) {
        m_notesPlugin->showDock();
        m_notesPlugin->widget()->raise();
        m_notesPlugin->widget()->setFocus();
    }
    return true;
}

void ProjectManager::adjustProjectDuration()
{
    pCore->monitorManager()->projectMonitor()->adjustRulerSize(m_mainTimelineModel->duration() - 1, nullptr);
}

void ProjectManager::activateAsset(const QVariantMap &effectData)
{
    if (effectData.contains(QStringLiteral("kdenlive/effect"))) {
        pCore->window()->addEffect(effectData.value(QStringLiteral("kdenlive/effect")).toString());
    } else {
        pCore->window()->getCurrentTimeline()->controller()->addAsset(effectData);
    }
}

std::shared_ptr<DocUndoStack> ProjectManager::undoStack(const QUuid &uuid)
{
    if (m_project && (m_project->uuid == uuid || uuid == QUuid())) {
        return m_project->commandStack();
    }
    KdenliveDoc *doc = getDocument(uuid);
    if (doc) {
        return doc->commandStack();
    }
    return nullptr;
}

QDir ProjectManager::cacheDir(bool audio, bool *ok)
{
    return current()->getCacheDir(audio ? CacheAudio : CacheThumbs, ok);
}

void ProjectManager::saveWithUpdatedProfile(const QString &updatedProfile)
{
    // First backup current project with fps appended
    bool saveInTempFile = false;
    if (m_project && m_project->isModified()) {
        switch (
            KMessageBox::warningYesNoCancel(pCore->window(), i18n("The project <b>\"%1\"</b> has been changed.\nDo you want to save your changes?",
                                                                  m_project->url().fileName().isEmpty() ? i18n("Untitled") : m_project->url().fileName()))) {
        case KMessageBox::Yes:
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
    QString convertedFile = currentFile.section(QLatin1Char('.'), 0, -2);
    double fpsRatio = newProfile->fps() / pCore->getCurrentFps();
    convertedFile.append(QString("-%1.kdenlive").arg(int(newProfile->fps() * 100)));
    QString saveFolder = m_project->url().adjusted(QUrl::RemoveFilename |   QUrl::StripTrailingSlash).toLocalFile();
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

    QFile f(currentFile);
    QDomDocument doc;
    doc.setContent(&f, false);
    f.close();
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
    if (QFile::exists(convertedFile)) {
        if (KMessageBox::warningYesNo(qApp->activeWindow(), i18n("Output file %1 already exists.\nDo you want to overwrite it?", convertedFile)) != KMessageBox::Yes) {
            return;
        }
    }
    QFile file(convertedFile);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return;
    }
    QTextStream out(&file);
    out.setCodec("UTF-8");
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

QPair<int, int> ProjectManager::tracksCount()
{
    return pCore->window()->getCurrentTimeline()->controller()->getTracksCount();
}

void ProjectManager::addAudioTracks(int tracksCount)
{
    pCore->window()->getCurrentTimeline()->controller()->addTracks(0, tracksCount);
}

void ProjectManager::openTimeline(const QString &id)
{
    std::shared_ptr<ProjectClip> clip = pCore->bin()->getBinClip(id);
    const QUuid uuid(clip->getProducerProperty(QStringLiteral("kdenlive:uuid")));
    if (m_openedDocuments.contains(uuid)) {
        pCore->window()->raiseTimeline(uuid);
        return;
    }

    std::unique_ptr<Mlt::Producer> xmlProd = nullptr;
    // Check if a tractor for this playlist already exists in the main timeline
    std::shared_ptr<Mlt::Tractor> tc = pCore->projectItemModel()->getExtraTimeline(uuid.toString());
    bool internalLoad = false;

    // Check if this is the first secondary timeline created. In this case, create a playlist item for the main timeline
    if (m_project->timelineCount() == 0) {
        //std::shared_ptr<Mlt::Producer>main = std::make_shared<Mlt::Producer>(new Mlt::Producer(m_mainTimelineModel->tractor()));
        qDebug()<<"=== MAIN TRACTIR TYPE: "<<m_mainTimelineModel->tractor()->producer()->type();
        //Mlt::Service s1(*m_mainTimelineModel->tractor()->producer());
        //Mlt::Tractor t1(s1);
        std::shared_ptr<Mlt::Producer>main(m_mainTimelineModel->tractor());
        //std::shared_ptr<Mlt::Producer>main(new Mlt::Producer(pCore->getCurrentProfile()->profile(), nullptr, "color:red"));
        QMap<QString, QString> mainProperties;
        mainProperties.insert(QStringLiteral("kdenlive:clipname"), i18n("Playlist"));
        int duration = m_mainTimelineModel->duration();
        mainProperties.insert("kdenlive:maxduration", QString::number(duration));
        mainProperties.insert(QStringLiteral("kdenlive:duration"), QString::number(duration - 1));
        mainProperties.insert(QStringLiteral("length"), QString::number(duration));
        //mainProperties.insert("out", QString::number(duration - 1));
        mainProperties.insert(QStringLiteral("kdenlive:uuid"), m_mainTimelineModel->uuid().toString().toUtf8().constData());
        QString mainId = ClipCreator::createPlaylistClip(i18n("Playlist"), QStringLiteral("-1"), std::move(pCore->projectItemModel()), main, m_mainTimelineModel->uuid(), mainProperties);
        pCore->bin()->registerPlaylist(m_mainTimelineModel->uuid(), mainId);
        std::shared_ptr<ProjectClip>mainClip = pCore->bin()->getBinClip(mainId);
        QObject::connect(m_mainTimelineModel.get(), &TimelineModel::durationUpdated, [model = m_mainTimelineModel, mainClip]() {
            QMap<QString, QString> properties;
            properties.insert(QStringLiteral("kdenlive:duration"), QString::number(model->duration()));
            properties.insert(QStringLiteral("kdenlive:maxduration"), QString::number(model->duration()));
            qDebug()<<"=== UPDATEING CLIP DURATION: "<<model->duration();
            mainClip->setProperties(properties, true);
        });
    }

    if (tc != nullptr && tc->is_valid()) {
        Mlt::Tractor s(*tc.get());
        qDebug()<<"=== LOADING EXTRA TIMELINE SERVICE: "<<s.type()<<"\n\n:::::::::::::::::";
        xmlProd.reset(new Mlt::Producer(s));
        internalLoad = true;
    } else {
        xmlProd.reset(clip->masterProducer());
    }
    if (xmlProd == nullptr || !xmlProd->is_valid()) {
        pCore->displayBinMessage(i18n("Cannot create a timeline from this clip:\n%1", clip->url()), KMessageWidget::Information);
        return;
    }
    m_uuidMap.insert(uuid, m_project->uuid);
    m_openedDocuments.insert(uuid, m_project);
    pCore->bin()->registerPlaylist(uuid, id);
    // Reference the new timeline's project model (same as main project)
    pCore->addProjectModel(uuid, pCore->projectItemModel());
    // Create guides model for the new timeline
    std::shared_ptr<MarkerListModel> guidesModel(new MarkerListModel(uuid, m_project->commandStack(), this));
    m_project->addTimeline(uuid, guidesModel);
    // Build timeline
    std::shared_ptr<TimelineItemModel> timelineModel = TimelineItemModel::construct(uuid, pCore->getProjectProfile(), guidesModel, m_project->commandStack());
    TimelineWidget *timeline = pCore->window()->openTimeline(uuid, clip->clipName(), timelineModel, pCore->monitorManager()->projectMonitor()->getControllerProxy());
    //pCore->buildProjectModel(timeline->uuid);
    //timeline->setModel(timelineModel, pCore->monitorManager()->projectMonitor()->getControllerProxy());
    //QDomDocument doc = m_project->createEmptyDocument(2, 2);
    if (internalLoad) {
        qDebug()<<"============= LOADING INTERNAL PLAYLIST!!!!";
        if (!constructTimelineFromTractor(timeline->uuid, timelineModel, nullptr, *tc.get(), m_progressDialog, m_project->modifiedDecimalPoint())) {
            qDebug()<<"===== LOADING PROJECT INTERNAL ERROR";
        }
        std::shared_ptr<Mlt::Producer>prod = std::make_shared<Mlt::Producer>(timeline->tractor());
        prod->set("kdenlive:duration", timelineModel->duration());
            prod->set("kdenlive:maxduration", timelineModel->duration());
            prod->set("length", timelineModel->duration());
            prod->set("out", timelineModel->duration() - 1);
            prod->set("kdenlive:clipname", clip->clipName().toUtf8().constData());
            prod->set("kdenlive:uuid", timelineModel->uuid().toString().toUtf8().constData());
            QObject::connect(timelineModel.get(), &TimelineModel::durationUpdated, [timelineModel, clip]() {
                QMap<QString, QString> properties;
                properties.insert(QStringLiteral("kdenlive:duration"), QString::number(timelineModel->duration()));
                properties.insert(QStringLiteral("kdenlive:maxduration"), QString::number(timelineModel->duration()));
                qDebug()<<"=== UPDATEING CLIP DURATION: "<<timelineModel->duration();
                clip->setProperties(properties, true);
            });
            clip->setProducer(prod);
    }
    else {
    Mlt::Service s(xmlProd->producer()->get_service());
    if (s.type() == mlt_service_multitrack_type) {
        Mlt::Multitrack multi(s);
        qDebug()<<"===== LOADING PROJECT FROM MULTITRACK: " <<multi.count()<<"\n============";
        if (!constructTimelineFromMelt(timeline->uuid, timelineModel, nullptr, multi, m_progressDialog, m_project->modifiedDecimalPoint())) {
            //TODO: act on project load failure
            qDebug()<<"// Project failed to load!!";
        }
    } else if (s.type() == mlt_service_tractor_type) {
        Mlt::Tractor tractor(s);
        qDebug()<<"==== GOT PROJECT CLIP LENGTH: "<<xmlProd->get_length()<<", TYPE:"<<s.type();
        qDebug()<<"==== GOT TRACKS: "<<tractor.count();
        if (!constructTimelineFromTractor(timeline->uuid, timelineModel, nullptr, tractor, m_progressDialog, m_project->modifiedDecimalPoint())) {
            //TODO: act on project load failure
            qDebug()<<"// Project failed to load!!";
        }
    } else {
        // Is it a Kdenlive project
            Mlt::Tractor tractor2(( mlt_tractor )xmlProd->get_producer());
            if (tractor2.count() == 0) {
                qDebug()<<"=== INVALID TRACTORE";
            }
            if (!constructTimelineFromTractor(timeline->uuid, timelineModel, nullptr, tractor2, m_progressDialog, m_project->modifiedDecimalPoint())) {
                qDebug()<<"// Project failed to load!!";
            }
            //std::shared_ptr<Mlt::Producer>prod(new Mlt::Producer(timeline->tractor()));
            std::shared_ptr<Mlt::Producer>prod = std::make_shared<Mlt::Producer>(timeline->tractor());
            prod->set("kdenlive:duration", timelineModel->duration());
            prod->set("kdenlive:maxduration", timelineModel->duration());
            prod->set("length", timelineModel->duration());
            prod->set("out", timelineModel->duration() - 1);
            prod->set("kdenlive:clipname", clip->clipName().toUtf8().constData());
            prod->set("kdenlive:uuid", timelineModel->uuid().toString().toUtf8().constData());
            QObject::connect(timelineModel.get(), &TimelineModel::durationUpdated, [timelineModel, clip]() {
                QMap<QString, QString> properties;
                properties.insert(QStringLiteral("kdenlive:duration"), QString::number(timelineModel->duration()));
                properties.insert(QStringLiteral("kdenlive:maxduration"), QString::number(timelineModel->duration()));
                qDebug()<<"=== UPDATEING CLIP DURATION: "<<timelineModel->duration();
                clip->setProperties(properties, true);
            });
            clip->setProducer(prod);
            QString retain = QStringLiteral("xml_retain %1").arg(timelineModel->uuid().toString());
            m_mainTimelineModel->tractor()->set(retain.toUtf8().constData(), timeline->tractor()->get_service(), 0);

        /*
        QFile f(clip->url());
        if (f.open(QFile::ReadOnly | QFile::Text)) {
            QTextStream in(&f);
            QString projectData = in.readAll();
            f.close();
            QScopedPointer<Mlt::Producer> xmlProd2(new Mlt::Producer(pCore->getCurrentProfile()->profile(), "xml-string",
                                                            projectData.toUtf8().constData()));
            Mlt::Service s2(*xmlProd2);
            Mlt::Tractor tractor2(s2);
            if (!constructTimelineFromTractor(timeline->uuid, timelineModel, nullptr, tractor2, m_progressDialog, m_project->modifiedDecimalPoint())) {
                qDebug()<<"// Project failed to load!!";
            }
            QString retain = QStringLiteral("xml_retain %1").arg(uuid.toString());
            std::shared_ptr<TimelineItemModel> mainModel = m_project->objectModel()->timeline()->model();
            mainModel->tractor()->set(retain.toUtf8().constData(), timeline->tractor()->get_service(), 0);
        }*/
    }
    }
    pCore->window()->raiseTimeline(timeline->uuid);
}

// Only used for tests, do not use inside the app
std::shared_ptr<MarkerListModel> ProjectManager::getGuideModel()
{
    return nullptr;
}

KdenliveDoc *ProjectManager::getDocument(const QUuid &uuid)
{
    return m_openedDocuments.value(uuid);
}
