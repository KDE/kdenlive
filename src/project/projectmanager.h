/*
Copyright (C) 2014  Till Theato <root@ttill.de>
This file is part of kdenlive. See www.kdenlive.org.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
*/

#ifndef PROJECTMANAGER_H
#define PROJECTMANAGER_H

#include "kdenlivecore_export.h"
#include <KRecentFilesAction>
#include <QDir>
#include <QObject>
#include <QTime>
#include <QTimer>
#include <QUrl>
#include <QElapsedTimer>

#include "timeline2/model/timelineitemmodel.hpp"

#include <memory>
#include <unordered_map>
#include <unordered_set>

class KAutoSaveFile;
class KJob;
class KdenliveDoc;
class MarkerListModel;
class NotesPlugin;
class Project;
class QAction;
class QProgressDialog;
class QUrl;
class DocUndoStack;

/**
 * @class ProjectManager
 * @brief Takes care of interaction with projects.
 */

class /*KDENLIVECORE_EXPORT*/ ProjectManager : public QObject
{
    Q_OBJECT

public:
    /** @brief Sets up actions to interact for project interaction (undo, redo, open, save, ...) and creates an empty project. */
    explicit ProjectManager(QObject *parent = nullptr);
    ~ProjectManager() override;

    /** @brief Returns a pointer to the currently opened project. A project should always be open.
        The method is virtual to allow mocking
     */
    virtual KdenliveDoc *current();

    /** @brief Store command line args for later opening. */
    void init(const QUrl &projectUrl, const QString &clipList);

    void doOpenFile(const QUrl &url, KAutoSaveFile *stale);
    KRecentFilesAction *recentFilesAction();
    void prepareSave();
    /** @brief Disable all bin effects in current project */
    void disableBinEffects(bool disable);
    /** @brief Returns current project's xml scene */
    QString projectSceneList(const QString &outputFolder);
    /** @brief returns a default hd profile depending on timezone*/
    static QString getDefaultProjectFormat();
    void saveZone(const QStringList &info, const QDir &dir);
    /** @brief Move project data files to new url */
    void moveProjectData(const QString &src, const QString &dest);
    /** @brief Retrieve current project's notes */
    QString documentNotes() const;

    /** @brief Retrieve the current Guide Model
        The method is virtual to allow mocking
     */
    virtual std::shared_ptr<MarkerListModel> getGuideModel();

    /** @brief Return the current undo stack
        The method is virtual to allow mocking
    */
    virtual std::shared_ptr<DocUndoStack> undoStack();

    /** @brief This will create a backup file with fps appended to project name,
     *  and save the project with an updated profile info, then reopen it.
     */
    void saveWithUpdatedProfile(const QString &updatedProfile);

    /** @brief Get the number of tracks in this projec (video, audio).
     */
    QPair<int, int> tracksCount();

    /** @brief Add requested audio tracks number to projet.
     */
    void addAudioTracks(int tracksCount);

public slots:
    void newFile(QString profileName, bool showProjectSettings = true);
    void newFile(bool showProjectSettings = true);
    /** @brief Shows file open dialog. */
    void openFile();
    void openLastFile();
    /** @brief Load files / clips passed on the command line. */
    void slotLoadOnOpen();

    /** @brief Checks whether a URL is available to save to.
     * @return Whether the file was saved. */
    bool saveFile();

    /** @brief Shows a save file dialog for saving the project.
     * @param saveACopy Default is false. If true, the file title of the dialog is set to "Save Copy…"
     * @return Whether the file was saved. */
    bool saveFileAs(bool saveACopy = false);

    /** @brief Set properties to match outputFileName and save the document.
     * Creates an autosave version of the output file too (only if not in copymode), at
     * ~/.kde/data/stalefiles/kdenlive/ \n
     * that will be actually written in KdenliveDoc::slotAutoSave()
     * @param outputFileName The URL to save to / The document's URL.
     * @param saveACopy Default is false. If true, the file will be saved but isn’t opened afterwards. Besides no autosave version will be created
     * @return Whether we had success. */
    bool saveFileAs(const QString &outputFileName, bool saveACopy = false);

    /** @brief Close currently opened document. Returns false if something went wrong (cannot save modifications, ...). */
    bool closeCurrentDocument(bool saveChanges = true, bool quit = false);

    /** @brief Prepares opening @param url.
     *
     * Checks if already open and whether backup exists */
    void openFile(const QUrl &url);

    /** @brief Start autosave timer */
    void slotStartAutoSave();

    /** @brief Update project and monitors profiles */
    void slotResetProfiles(bool reloadThumbs);

    /** @brief Rebuild consumers after a property change */
    void slotResetConsumers(bool fullReset);

    /** @brief Dis/enable all timeline effects */
    void slotDisableTimelineEffects(bool disable);

    /** @brief Un/Lock current timeline track */
    void slotSwitchTrackLock();
    void slotSwitchAllTrackLock();

    /** @brief Make current timeline track active/inactive*/
    void slotSwitchTrackActive();
    /** @brief Toogle the active/inactive state of all tracks*/
    void slotSwitchAllTrackActive();
    /** @brief Make all tracks active or inactive */
    void slotMakeAllTrackActive();
    /** @brief Restore current clip target tracks */
    void slotRestoreTargetTracks();

    /** @brief Un/Set current track as target */
    void slotSwitchTrackTarget();

    /** @brief Set the text for current project's notes */
    void setDocumentNotes(const QString &notes);

    /** @brief Project's duration changed, adjust monitor, etc. */
    void adjustProjectDuration();
    /** @brief Add an asset in timeline (effect, transition). */
    void activateAsset(const QVariantMap &effectData);
    /** @brief insert current timeline timecode in notes widget and focus widget to allow entering quick note */
    void slotAddProjectNote();

private slots:
    void slotRevert();
    /** @brief Open the project's backupdialog. */
    bool slotOpenBackup(const QUrl &url = QUrl());
    /** @brief Start autosaving the document. */
    void slotAutoSave();
    /** @brief Report progress of folder move operation. */
    void slotMoveProgress(KJob *, unsigned long progress);
    void slotMoveFinished(KJob *job);

signals:
    void docOpened(KdenliveDoc *document);

protected:
    /** @brief Update the timeline according to the MLT XML */
    bool updateTimeline(int pos = -1, int scrollPos = -1);

private:
    /** @brief checks if autoback files exists, recovers from it if user says yes, returns true if files were recovered. */
    bool checkForBackupFile(const QUrl &url, bool newFile = false);

    KdenliveDoc *m_project{nullptr};
    std::shared_ptr<TimelineItemModel> m_mainTimelineModel;
    QElapsedTimer m_lastSave;
    QTimer m_autoSaveTimer;
    QUrl m_startUrl;
    QString m_loadClipsOnOpen;
    QMap<QString, QString> m_replacementPattern;

    QAction *m_fileRevert;
    KRecentFilesAction *m_recentFilesAction;
    NotesPlugin *m_notesPlugin;
    QProgressDialog *m_progressDialog{nullptr};
    /** @brief If true, means we are still opening Kdenlive, send messages to splash screen */
    bool m_loading;
    void saveRecentFiles();
};

#endif
