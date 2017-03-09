/*
Copyright (C) 2014  Till Theato <root@ttill.de>
This file is part of kdenlive. See www.kdenlive.org.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
*/

#include "projectmanager.h"
#include "core.h"
#include "bin/bin.h"
#include "mltcontroller/bincontroller.h"
#include "mltcontroller/producerqueue.h"
#include "mainwindow.h"
#include "kdenlivesettings.h"
#include "monitor/monitormanager.h"
#include "doc/kdenlivedoc.h"
#include "timeline/timeline.h"
#include "project/dialogs/projectsettings.h"
#include "timeline/customtrackview.h"
#include "transitionsettings.h"
#include "project/dialogs/archivewidget.h"
#include "effectstack/effectstackview2.h"
#include "project/dialogs/backupwidget.h"
#include "project/notesplugin.h"
#include "utils/KoIconUtils.h"

#include <KActionCollection>
#include <KRecentDirs>
#include <KJob>
#include <KMessageBox>
#include <klocalizedstring.h>

#include <QProgressDialog>
#include <QCryptographicHash>
#include <QFileDialog>
#include <QAction>
#include "kdenlive_debug.h"
#include <QMimeDatabase>
#include <QMimeType>
#include <QTimeZone>
#include <QLocale>
#include <KConfigGroup>

ProjectManager::ProjectManager(QObject *parent) :
    QObject(parent),
    m_project(nullptr),
    m_trackView(nullptr),
    m_progressDialog(nullptr)
{
    m_fileRevert = KStandardAction::revert(this, SLOT(slotRevert()), pCore->window()->actionCollection());
    m_fileRevert->setIcon(KoIconUtils::themedIcon(QStringLiteral("document-revert")));
    m_fileRevert->setEnabled(false);

    QAction *a = KStandardAction::open(this,                   SLOT(openFile()),               pCore->window()->actionCollection());
    a->setIcon(KoIconUtils::themedIcon(QStringLiteral("document-open")));
    a = KStandardAction::saveAs(this,                 SLOT(saveFileAs()),             pCore->window()->actionCollection());
    a->setIcon(KoIconUtils::themedIcon(QStringLiteral("document-save-as")));
    a = KStandardAction::openNew(this,                SLOT(newFile()),                pCore->window()->actionCollection());
    a->setIcon(KoIconUtils::themedIcon(QStringLiteral("document-new")));
    m_recentFilesAction = KStandardAction::openRecent(this, SLOT(openFile(QUrl)), pCore->window()->actionCollection());

    QAction *backupAction = new QAction(KoIconUtils::themedIcon(QStringLiteral("edit-undo")), i18n("Open Backup File"), this);
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

ProjectManager::~ProjectManager()
{
    delete m_notesPlugin;

    delete m_trackView;
    delete m_project;
}

void ProjectManager::slotLoadOnOpen()
{
    if (m_startUrl.isValid()) {
        openFile();
    } else if (KdenliveSettings::openlastproject()) {
        openLastFile();
    } else {
        newFile(false);
    }

    if (!m_loadClipsOnOpen.isEmpty() && m_project) {
        const QStringList list = m_loadClipsOnOpen.split(QLatin1Char(','));
        QList<QUrl> urls;
        urls.reserve(list.count());
        for (const QString &path : list) {
            //qCDebug(KDENLIVE_LOG) << QDir::current().absoluteFilePath(path);
            urls << QUrl::fromLocalFile(QDir::current().absoluteFilePath(path));
        }
        pCore->bin()->droppedUrls(urls);
    }
    m_loadClipsOnOpen.clear();
}

void ProjectManager::init(const QUrl &projectUrl, const QString &clipList)
{
    m_startUrl = projectUrl;
    m_loadClipsOnOpen = clipList;
}

void ProjectManager::newFile(bool showProjectSettings, bool force)
{
    if (!pCore->window()->m_timelineArea->isEnabled() && !force) {
        return;
    }
    // fix mantis#3160
    QUrl startFile = QUrl::fromLocalFile(KdenliveSettings::defaultprojectfolder() + QStringLiteral("/_untitled.kdenlive"));
    if (checkForBackupFile(startFile)) {
        return;
    }
    m_fileRevert->setEnabled(false);
    QString profileName = KdenliveSettings::default_profile();
    if (profileName.isEmpty()) {
        profileName = KdenliveSettings::current_profile();
    }
    QString projectFolder;
    QMap<QString, QString> documentProperties;
    QMap<QString, QString> documentMetadata;
    QPoint projectTracks(KdenliveSettings::videotracks(), KdenliveSettings::audiotracks());
    pCore->monitorManager()->resetDisplay();
    QString documentId = QString::number(QDateTime::currentMSecsSinceEpoch());
    documentProperties.insert(QStringLiteral("decimalPoint"), QLocale().decimalPoint());
    documentProperties.insert(QStringLiteral("documentid"), documentId);
    if (!showProjectSettings) {
        if (!closeCurrentDocument()) {
            return;
        }
        if (KdenliveSettings::customprojectfolder()) {
            projectFolder = KdenliveSettings::defaultprojectfolder();
            if (!projectFolder.endsWith(QLatin1Char('/'))) {
                projectFolder.append(QLatin1Char('/'));
            }
            documentProperties.insert(QStringLiteral("storagefolder"), projectFolder + documentId);
        }
    } else {
        QPointer<ProjectSettings> w = new ProjectSettings(nullptr, QMap<QString, QString> (), QStringList(), projectTracks.x(), projectTracks.y(), KdenliveSettings::defaultprojectfolder(), false, true, pCore->window());
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
        documentProperties.insert(QStringLiteral("enableproxy"), QString::number((int) w->useProxy()));
        documentProperties.insert(QStringLiteral("generateproxy"), QString::number((int) w->generateProxy()));
        documentProperties.insert(QStringLiteral("proxyminsize"), QString::number(w->proxyMinSize()));
        documentProperties.insert(QStringLiteral("proxyparams"), w->proxyParams());
        documentProperties.insert(QStringLiteral("proxyextension"), w->proxyExtension());
        documentProperties.insert(QStringLiteral("generateimageproxy"), QString::number((int) w->generateImageProxy()));
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
            documentProperties.insert(QStringLiteral("storagefolder"), projectFolder +documentId);
        }
        documentMetadata = w->metadata();
        delete w;
    }
    pCore->window()->m_timelineArea->setEnabled(true);
    bool openBackup;
    m_notesPlugin->clear();
    KdenliveDoc *doc = new KdenliveDoc(QUrl(), projectFolder, pCore->window()->m_commandStack, profileName, documentProperties, documentMetadata, projectTracks, pCore->monitorManager()->projectMonitor()->render, m_notesPlugin, &openBackup, pCore->window());
    doc->m_autosave = new KAutoSaveFile(startFile, doc);
    bool ok;
    pCore->bin()->setDocument(doc);
    QList<QAction *> rulerActions;
    rulerActions << pCore->window()->actionCollection()->action(QStringLiteral("set_render_timeline_zone"));
    rulerActions << pCore->window()->actionCollection()->action(QStringLiteral("unset_render_timeline_zone"));
    m_trackView = new Timeline(doc, pCore->window()->kdenliveCategoryMap.value(QStringLiteral("timeline"))->actions(), rulerActions, &ok, pCore->window());
    // Set default target tracks to upper audio / lower video tracks
    m_trackView->audioTarget = projectTracks.y() > 0 ? projectTracks.y() : -1;
    m_trackView->videoTarget = projectTracks.x() > 0 ? projectTracks.y() + 1 : -1;
    connect(m_trackView->projectView(), SIGNAL(importPlaylistClips(ItemInfo, QString, QUndoCommand *)), pCore->bin(), SLOT(slotExpandUrl(ItemInfo, QString, QUndoCommand *)), Qt::DirectConnection);

    m_trackView->loadTimeline();
    pCore->window()->m_timelineArea->addTab(m_trackView, QIcon::fromTheme(QStringLiteral("kdenlive")), doc->description());
    m_project = doc;
    if (!ok) {
        // MLT is broken
        //pCore->window()->m_timelineArea->setEnabled(false);
        //pCore->window()->m_projectList->setEnabled(false);
        pCore->window()->slotPreferences(6);
        return;
    }
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
    emit docOpened(m_project);
    //pCore->monitorManager()->activateMonitor(Kdenlive::ClipMonitor);
    m_trackView->projectView()->setFocus();
    m_lastSave.start();
}

bool ProjectManager::closeCurrentDocument(bool saveChanges, bool quit)
{
    if (m_project && m_project->isModified() && saveChanges) {
        QString message;
        if (m_project->url().fileName().isEmpty()) {
            message = i18n("Save changes to document?");
        } else {
            message = i18n("The project <b>\"%1\"</b> has been changed.\nDo you want to save your changes?", m_project->url().fileName());
        }

        switch (KMessageBox::warningYesNoCancel(pCore->window(), message)) {
        case KMessageBox::Yes :
            // save document here. If saving fails, return false;
            if (saveFile() == false) {
                return false;
            }
            break;
        case KMessageBox::Cancel :
            return false;
            break;
        default:
            break;
        }
    }
    if (!quit && !qApp->isSavingSession()) {
        m_autoSaveTimer.stop();
        if (m_project) {
            pCore->producerQueue()->abortOperations();
            pCore->bin()->abortOperations();
            pCore->window()->slotTimelineClipSelected(nullptr, false);
            pCore->monitorManager()->clipMonitor()->slotOpenClip(nullptr);
            pCore->window()->m_effectStack->clear();
            pCore->window()->m_effectStack->transitionConfig()->slotTransitionItemSelected(nullptr, 0, QPoint(), false);
            delete m_trackView;
            m_trackView = nullptr;
            delete m_project;
            m_project = nullptr;
        }
        pCore->monitorManager()->setDocument(m_project);
    }
    return true;
}

bool ProjectManager::saveFileAs(const QString &outputFileName)
{
    pCore->monitorManager()->pauseActiveMonitor();
    // Sync document properties
    prepareSave();
    QString saveFolder = QFileInfo(outputFileName).absolutePath();
    QString scene = projectSceneList(saveFolder);
    if (!m_replacementPattern.isEmpty()) {
        QMapIterator<QString, QString> i(m_replacementPattern);
        while (i.hasNext()) {
            i.next();
            scene.replace(i.key(), i.value());
        }
    }
    if (m_project->saveSceneList(outputFileName, scene) == false) {
        return false;
    }
    QUrl url = QUrl::fromLocalFile(outputFileName);
    // Save timeline thumbnails
    m_trackView->projectView()->saveThumbnails();
    m_project->setUrl(url);
    // setting up autosave file in ~/.kde/data/stalefiles/kdenlive/
    // saved under file name
    // actual saving by KdenliveDoc::slotAutoSave() called by a timer 3 seconds after the document has been edited
    // This timer is set by KdenliveDoc::setModified()
    if (m_project->m_autosave == nullptr) {
        // The temporary file is not opened or created until actually needed.
        // The file filename does not have to exist for KAutoSaveFile to be constructed (if it exists, it will not be touched).
        m_project->m_autosave = new KAutoSaveFile(url, this);
    } else {
        m_project->m_autosave->setManagedFile(url);
    }

    pCore->window()->setWindowTitle(m_project->description());
    m_project->setModified(false);
    m_recentFilesAction->addUrl(url);
    // remember folder for next project opening
    KRecentDirs::add(QStringLiteral(":KdenliveProjectsFolder"), saveFolder);
    saveRecentFiles();
    m_fileRevert->setEnabled(true);
    pCore->window()->m_undoView->stack()->setClean();

    return true;
}

void ProjectManager::saveRecentFiles()
{
    KSharedConfigPtr config = KSharedConfig::openConfig();
    m_recentFilesAction->saveEntries(KConfigGroup(config, "Recent Files"));
    config->sync();
}

void ProjectManager::slotSaveSelection(const QString &path)
{
    m_trackView->projectView()->exportTimelineSelection(path);
}

bool ProjectManager::hasSelection() const
{
    return m_trackView->projectView()->hasSelection();
}

bool ProjectManager::saveFileAs()
{
    QFileDialog fd(pCore->window());
    fd.setDirectory(m_project->url().isValid() ? m_project->url().adjusted(QUrl::RemoveFilename).toLocalFile() : KdenliveSettings::defaultprojectfolder());
    fd.setMimeTypeFilters(QStringList() << QStringLiteral("application/x-kdenlive"));
    fd.setAcceptMode(QFileDialog::AcceptSave);
    fd.setFileMode(QFileDialog::AnyFile);
    fd.setDefaultSuffix(QStringLiteral("kdenlive"));
    if (fd.exec() != QDialog::Accepted || fd.selectedFiles().isEmpty()) {
        return false;
    }
    QString outputFile = fd.selectedFiles().first();

#if KXMLGUI_VERSION_MINOR < 23 && KXMLGUI_VERSION_MAJOR == 5
    // Since Plasma 5.7 (release at same time as KF 5.23,
    // the file dialog manages the overwrite check
    if (QFile::exists(outputFile)) {
        // Show the file dialog again if the user does not want to overwrite the file
        if (KMessageBox::questionYesNo(pCore->window(), i18n("File %1 already exists.\nDo you want to overwrite it?", outputFile)) == KMessageBox::No) {
            return saveFileAs();
        }
    }
#endif

    bool ok = false;
    QDir cacheDir = m_project->getCacheDir(CacheBase, &ok);
    if (ok) {
        QFile file(cacheDir.absoluteFilePath(QString::fromLatin1(QUrl::toPercentEncoding(QStringLiteral(".") + outputFile))));
        file.open(QIODevice::ReadWrite | QIODevice::Text);
        file.close();
    }
    return saveFileAs(outputFile);
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
    } else {
        bool result = saveFileAs(m_project->url().toLocalFile());
        m_project->m_autosave->resize(0);
        return result;
    }
}

void ProjectManager::openFile()
{
    if (m_startUrl.isValid()) {
        openFile(m_startUrl);
        m_startUrl.clear();
        return;
    }
    QUrl url = QFileDialog::getOpenFileUrl(pCore->window(), QString(), QUrl::fromLocalFile(KRecentDirs::dir(QStringLiteral(":KdenliveProjectsFolder"))), getMimeType());
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
bool ProjectManager::checkForBackupFile(const QUrl &url)
{
    // Check for autosave file that belong to the url we passed in.
    QList<KAutoSaveFile *> staleFiles = KAutoSaveFile::staleFiles(url);
    KAutoSaveFile *orphanedFile = nullptr;
    // Check if we can have a lock on one of the file,
    // meaning it is not handled by any Kdenlive instancce
    if (!staleFiles.isEmpty()) {
        foreach (KAutoSaveFile *stale, staleFiles) {
            if (stale->open(QIODevice::QIODevice::ReadWrite)) {
                // Found orphaned autosave file
                orphanedFile = stale;
                break;
            } else {
                // Another Kdenlive instance is probably handling this autosave file
                staleFiles.removeAll(stale);
                delete stale;
                continue;
            }
        }
    }

    if (orphanedFile) {
        if (KMessageBox::questionYesNo(nullptr,
                                       i18n("Auto-saved files exist. Do you want to recover them now?"),
                                       i18n("File Recovery"),
                                       KGuiItem(i18n("Recover")), KGuiItem(i18n("Don't recover"))) == KMessageBox::Yes) {
            doOpenFile(url, orphanedFile);
            return true;
        } else {
            // remove the stale files
            foreach (KAutoSaveFile *stale, staleFiles) {
                stale->open(QIODevice::ReadWrite);
                delete stale;
            }
        }
        return false;
    }
    return false;
}

void ProjectManager::openFile(const QUrl &url)
{
    QMimeDatabase db;
    // Make sure the url is a Kdenlive project file
    QMimeType mime = db.mimeTypeForUrl(url);
    if (mime.inherits(QStringLiteral("application/x-compressed-tar"))) {
        // Opening a compressed project file, we need to process it
        //qCDebug(KDENLIVE_LOG)<<"Opening archive, processing";
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

    if (m_project && m_project->url() == url) {
        return;
    }

    if (!closeCurrentDocument()) {
        return;
    }

    if (checkForBackupFile(url)) {
        return;
    }
    pCore->window()->slotGotProgressInfo(i18n("Opening file %1", url.toLocalFile()), 100, InformationMessage);
    doOpenFile(url, nullptr);
}

void ProjectManager::doOpenFile(const QUrl &url, KAutoSaveFile *stale)
{
    Q_ASSERT(m_project == nullptr);
    if (!pCore->window()->m_timelineArea->isEnabled()) {
        return;
    }
    m_fileRevert->setEnabled(true);

    // Recreate stopmotion widget on document change
    if (pCore->window()->m_stopmotion) {
        delete pCore->window()->m_stopmotion;
        pCore->window()->m_stopmotion = nullptr;
    }
    delete m_progressDialog;
    pCore->monitorManager()->resetDisplay();
    m_progressDialog = new QProgressDialog(pCore->window());
    m_progressDialog->setWindowTitle(i18n("Loading project"));
    m_progressDialog->setCancelButton(nullptr);
    m_progressDialog->setLabelText(i18n("Loading project"));
    m_progressDialog->setMaximum(0);
    m_progressDialog->show();
    bool openBackup;
    m_notesPlugin->clear();
    KdenliveDoc *doc = new KdenliveDoc(stale ? QUrl::fromLocalFile(stale->fileName()) : url, QString(), pCore->window()->m_commandStack, KdenliveSettings::default_profile().isEmpty() ? KdenliveSettings::current_profile() : KdenliveSettings::default_profile(), QMap<QString, QString> (), QMap<QString, QString> (), QPoint(KdenliveSettings::videotracks(), KdenliveSettings::audiotracks()), pCore->monitorManager()->projectMonitor()->render, m_notesPlugin, &openBackup, pCore->window());
    if (stale == nullptr) {
        stale = new KAutoSaveFile(url, doc);
        doc->m_autosave = stale;
    } else {
        doc->m_autosave = stale;
        stale->setParent(doc);
        // if loading from an autosave of unnamed file then keep unnamed
        if (url.fileName().contains(QStringLiteral("_untitled.kdenlive"))) {
            doc->setUrl(QUrl());
        } else {
            doc->setUrl(url);
        }
        doc->setModified(true);
        stale->setParent(doc);
    }
    m_progressDialog->setLabelText(i18n("Loading clips"));
    pCore->bin()->setDocument(doc);

    QList<QAction *> rulerActions;
    rulerActions << pCore->window()->actionCollection()->action(QStringLiteral("set_render_timeline_zone"));
    rulerActions << pCore->window()->actionCollection()->action(QStringLiteral("unset_render_timeline_zone"));
    rulerActions << pCore->window()->actionCollection()->action(QStringLiteral("clear_render_timeline_zone"));
    bool ok;
    m_trackView = new Timeline(doc, pCore->window()->kdenliveCategoryMap.value(QStringLiteral("timeline"))->actions(), rulerActions, &ok, pCore->window());
    connect(m_trackView, &Timeline::startLoadingBin, m_progressDialog, &QProgressDialog::setMaximum, Qt::DirectConnection);
    connect(m_trackView, &Timeline::resetUsageCount, pCore->bin(), &Bin::resetUsageCount, Qt::DirectConnection);
    connect(m_trackView, &Timeline::loadingBin, m_progressDialog, &QProgressDialog::setValue, Qt::DirectConnection);

    // Set default target tracks to upper audio / lower video tracks
    m_project = doc;
    m_trackView->audioTarget = doc->getDocumentProperty(QStringLiteral("audiotargettrack"), QStringLiteral("-1")).toInt();
    m_trackView->videoTarget = doc->getDocumentProperty(QStringLiteral("videotargettrack"), QStringLiteral("-1")).toInt();
    m_trackView->loadTimeline();
    m_trackView->loadGuides(pCore->binController()->takeGuidesData());
    connect(m_trackView->projectView(), SIGNAL(importPlaylistClips(ItemInfo, QString, QUndoCommand *)), pCore->bin(), SLOT(slotExpandUrl(ItemInfo, QString, QUndoCommand *)), Qt::DirectConnection);
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
    emit docOpened(m_project);

    pCore->window()->m_timelineArea->setCurrentIndex(pCore->window()->m_timelineArea->addTab(m_trackView, QIcon::fromTheme(QStringLiteral("kdenlive")), m_project->description()));
    if (!ok) {
        pCore->window()->m_timelineArea->setEnabled(false);
        KMessageBox::sorry(pCore->window(), i18n("Cannot open file %1.\nProject is corrupted.", url.toLocalFile()));
        pCore->window()->slotGotProgressInfo(QString(), 100);
        newFile(false, true);
        return;
    }
    m_trackView->setDuration(m_trackView->duration());

    pCore->window()->slotGotProgressInfo(QString(), 100);
    pCore->monitorManager()->projectMonitor()->adjustRulerSize(m_trackView->duration() - 1);
    if (openBackup) {
        slotOpenBackup(url);
    }
    m_lastSave.start();
    delete m_progressDialog;
    m_progressDialog = nullptr;
}

void ProjectManager::slotRevert()
{
    if (m_project->isModified() && KMessageBox::warningContinueCancel(pCore->window(), i18n("This will delete all changes made since you last saved your project. Are you sure you want to continue?"), i18n("Revert to last saved version")) == KMessageBox::Cancel) {
        return;
    }
    QUrl url = m_project->url();
    if (closeCurrentDocument(false)) {
        doOpenFile(url, nullptr);
    }
}

QString ProjectManager::getMimeType(bool open)
{
    QString mimetype = i18n("Kdenlive project (*.kdenlive)");
    if (open) {
        mimetype.append(QStringLiteral(";;") + i18n("Archived project (*.tar.gz)"));
    }
    return mimetype;
}

KdenliveDoc *ProjectManager::current()
{
    return m_project;
}

void ProjectManager::slotOpenBackup(const QUrl &url)
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

    QPointer<BackupWidget> dia = new BackupWidget(projectFile, projectFolder, projectId, pCore->window());
    if (dia->exec() == QDialog::Accepted) {
        QString requestedBackup = dia->selectedFile();
        m_project->backupLastSavedVersion(projectFile.toLocalFile());
        closeCurrentDocument(false);
        doOpenFile(QUrl::fromLocalFile(requestedBackup), nullptr);
        if (m_project) {
            m_project->setUrl(projectFile);
            m_project->setModified(true);
            pCore->window()->setWindowTitle(m_project->description());
        }
    }
    delete dia;
}

Timeline *ProjectManager::currentTimeline()
{
    return m_trackView;
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
    prepareSave();
    bool multitrackEnabled = m_trackView->multitrackView;
    if (multitrackEnabled) {
        // Multitrack view was enabled, disable for auto save
        m_trackView->slotMultitrackView(false);
    }
    m_trackView->connectOverlayTrack(false);
    m_project->slotAutoSave();
    m_trackView->connectOverlayTrack(true);
    if (multitrackEnabled) {
        // Multitrack view was enabled, re-enable for auto save
        m_trackView->slotMultitrackView(true);
    }
    m_lastSave.start();
}

QString ProjectManager::projectSceneList(const QString &outputFolder)
{
    bool multitrackEnabled = m_trackView->multitrackView;
    if (multitrackEnabled) {
        // Multitrack view was enabled, disable for auto save
        m_trackView->slotMultitrackView(false);
    }
    m_trackView->connectOverlayTrack(false);
    QString scene = pCore->monitorManager()->projectMonitor()->sceneList(outputFolder);
    m_trackView->connectOverlayTrack(true);
    if (multitrackEnabled) {
        // Multitrack view was enabled, re-enable for auto save
        m_trackView->slotMultitrackView(true);
    }
    return scene;
}

void ProjectManager::prepareSave()
{
    pCore->binController()->saveDocumentProperties(m_trackView->documentProperties(), m_project->metadata(), m_trackView->projectView()->guidesData());
    pCore->binController()->saveProperty(QStringLiteral("kdenlive:documentnotes"), m_project->documentNotes());
    pCore->binController()->saveProperty(QStringLiteral("kdenlive:clipgroups"), m_project->groupsXml());
}

void ProjectManager::slotResetProfiles()
{
    m_project->resetProfile();
    pCore->monitorManager()->resetProfiles(m_project->mltProfile(), m_project->timecode());
    pCore->monitorManager()->updateScopeSource();
}

void ProjectManager::slotExpandClip()
{
    m_trackView->projectView()->expandActiveClip();
}

void ProjectManager::disableBinEffects(bool disable)
{
    if (m_project) {
        if (disable) {
            m_project->setDocumentProperty(QStringLiteral("disablebineffects"), QString::number((int) true));
        } else {
            m_project->setDocumentProperty(QStringLiteral("disablebineffects"), QString());
        }
    }
    pCore->window()->m_effectStack->disableBinEffects(disable);
}

void ProjectManager::slotDisableTimelineEffects(bool disable)
{
    if (disable) {
        m_project->setDocumentProperty(QStringLiteral("disabletimelineeffects"), QString::number((int) true));
    } else {
        m_project->setDocumentProperty(QStringLiteral("disabletimelineeffects"), QString());
    }
    m_trackView->disableTimelineEffects(disable);
    pCore->window()->m_effectStack->disableTimelineEffects(disable);
    pCore->monitorManager()->projectMonitor()->refreshMonitorIfActive();
}

void ProjectManager::slotSwitchTrackLock()
{
    m_trackView->projectView()->switchTrackLock();
}

void ProjectManager::slotSwitchAllTrackLock()
{
    m_trackView->projectView()->switchAllTrackLock();
}

void ProjectManager::slotSwitchTrackTarget()
{
    m_trackView->switchTrackTarget();
}

QString ProjectManager::getDefaultProjectFormat()
{
    // On first run, lets use an HD1080p profile with fps related to timezone country. Then, when the first video is added to a project, if it does not match our profile, propose a new default.
    QTimeZone zone;
    zone = QTimeZone::systemTimeZone();

    QList<int> ntscCountries;
    ntscCountries << QLocale::Canada << QLocale::Chile << QLocale::CostaRica << QLocale::Cuba << QLocale::DominicanRepublic << QLocale::Ecuador;
    ntscCountries  << QLocale::Japan << QLocale::Mexico << QLocale::Nicaragua << QLocale::Panama << QLocale::Peru << QLocale::Philippines;
    ntscCountries  << QLocale::PuertoRico << QLocale::SouthKorea << QLocale::Taiwan << QLocale::UnitedStates;
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
    KIO::CopyJob *copyJob = KIO::move(QUrl::fromLocalFile(src), QUrl::fromLocalFile(dest));
    connect(copyJob, &KJob::result, this, &ProjectManager::slotMoveFinished);
    connect(copyJob, SIGNAL(percent(KJob *, ulong)), this, SLOT(slotMoveProgress(KJob *, ulong)));
    m_project->moveProjectData(src, dest);
}

void ProjectManager::slotMoveProgress(KJob *, unsigned long progress)
{
    pCore->window()->slotGotProgressInfo(i18n("Moving project folder"), progress, ProcessingJobMessage);
}

void ProjectManager::slotMoveFinished(KJob *job)
{
    if (job->error() == 0) {
        pCore->window()->slotGotProgressInfo(QString(), 100, InformationMessage);
        KIO::CopyJob *copyJob = static_cast<KIO::CopyJob *>(job);
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
