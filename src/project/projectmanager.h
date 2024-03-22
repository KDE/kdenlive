/*
SPDX-FileCopyrightText: 2014 Till Theato <root@ttill.de>
SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

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
class TimelineWidget;

/** @class ProjectManager
    @brief Takes care of interaction with projects.
 */
class /*KDENLIVECORE_EXPORT*/ ProjectManager : public QObject
{
    Q_OBJECT

public:
    friend class RenderRequest;
    /** @brief Sets up actions to interact for project interaction (undo, redo, open, save, ...) and creates an empty project. */
    explicit ProjectManager(QObject *parent = nullptr);
    ~ProjectManager() override;

    /** @brief Returns a pointer to the currently opened project. A project should always be open.
        The method is virtual to allow mocking
     */
    virtual KdenliveDoc *current();

    /** @brief Store command line args for later opening. */
    void init(const QUrl &projectUrl, const QString &clipList);

    void doOpenFile(const QUrl &url, KAutoSaveFile *stale, bool isBackup = false);
    void doOpenFileHeadless(const QUrl &url);
    KRecentFilesAction *recentFilesAction();
    void prepareSave();
    /** @brief Disable all bin effects in current project
     *  @param disable if true, all project bin effects will be disabled
     *  @param refreshMonitor if false, monitors will not be refreshed
     */
    void disableBinEffects(bool disable, bool refreshMonitor = true);
    /** @brief Returns current project's xml scene */
    QString projectSceneList(const QString &outputFolder, const QString &overlayData = QString());
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

    virtual const QDir cacheDir(bool audio, bool *ok) const;

    /** @brief This will create a backup file with fps appended to project name,
     *  and save the project with an updated profile info, then reopen it.
     */
    void saveWithUpdatedProfile(const QString &updatedProfile);

    /** @brief Get the number of tracks in this project (video, audio).
     */
    QPair<int, int> avTracksCount();

    /** @brief Add requested audio tracks number to project.
     */
    void addAudioTracks(int tracksCount);
    /** @brief This method is only there for tests, do not use in real app.
     */
    void testSetActiveDocument(KdenliveDoc *doc, std::shared_ptr<TimelineItemModel> timeline = nullptr);
    /** @brief This method is only there for tests, do not use in real app.
     */
    bool testSaveFileAs(const QString &outputFileName);
    /** @brief Retrieve the current timeline (mostly used for testing.
     */
    std::shared_ptr<TimelineItemModel> getTimeline();
    /** @brief Initialize a new timeline's default properties.
     */
    void initSequenceProperties(const QUuid &uuid, std::pair<int, int> tracks);
    /** @brief Open a timeline clip in a tab.
     *  @returns true if the timeline was not previously opened
     */
    bool openTimeline(const QString &id, const QUuid &uuid, int position = -1, bool duplicate = false,
                      std::shared_ptr<TimelineItemModel> existingModel = nullptr);
    /** @brief Set a property on timeline uuid
     */
    void setTimelinePropery(QUuid uuid, const QString &prop, const QString &val);
    /** @brief Get the count of timelines in this project
     */
    int getTimelinesCount() const;

    void activateDocument(const QUuid &uuid);
    /** @brief Close a timeline tab through its uuid
     */
    bool closeTimeline(const QUuid &uuid, bool onDeletion = false, bool clearUndo = true);
    /** @brief Update a timeline sequence before saving or extracting xml
     */
    void syncTimeline(const QUuid &uuid, bool refresh = false);
    void doSyncTimeline(std::shared_ptr<TimelineItemModel> model, bool refresh);
    void setActiveTimeline(const QUuid &uuid);
    /** @brief Replace all instance of a @param sourceId clip with another @param replacementId in the active timline sequence
     * @param replaceAudio if true, only the audio clips will be replaced. if false, only the video parts.
     */
    void replaceTimelineInstances(const QString &sourceId, const QString &replacementId, bool replaceAudio, bool replaceVideo);

public Q_SLOTS:
    void newFile(QString profileName, bool showProjectSettings = true);
    void newFile(bool showProjectSettings = true);
    /** @brief Shows file open dialog. */
    void openFile();
    void openLastFile();
    /** @brief Load files / clips passed on the command line. */
    void slotLoadOnOpen();

    void slotLoadHeadless(const QUrl &projectUrl);

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
    bool saveFileAs(const QString &outputFileName, bool saveOverExistingFile = true, bool saveACopy = false);

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

    /** @brief Mute/Unmute or Hide/Show current timeline track */
    void slotSwitchTrackDisabled();
    /** @brief Un/Lock current timeline track */
    void slotSwitchTrackLock();
    void slotSwitchAllTrackLock();

    /** @brief Make current timeline track active/inactive*/
    void slotSwitchTrackActive();
    /** @brief Toggle the active/inactive state of all tracks*/
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
    void adjustProjectDuration(int duration);
    /** @brief Add an asset in timeline (effect, transition). */
    void activateAsset(const QVariantMap &effectData);
    /** @brief insert current timeline timecode in notes widget and focus widget to allow entering quick note */
    void slotAddProjectNote();
    /** @brief insert license text in notes widget and focus widget to allow entering quick note. Virtual to allow Mocking */
    virtual void slotAddTextNote(const QString &text);
    /** @brief Open a timeline with a reference to a track / position. */
    void seekTimeline(const QString &frameAndTrack);
    /** @brief Create a sequence clip from timeline selection. */
    void slotCreateSequenceFromSelection();

private Q_SLOTS:
    void slotRevert();
    /** @brief A timeline sequence duration changed, update our properties. */
    void updateSequenceDuration(const QUuid &uuid);
    /** @brief Open the project's backupdialog. */
    bool slotOpenBackup(const QUrl &url = QUrl());
    /** @brief Start autosaving the document. */
    void slotAutoSave();
    /** @brief Report progress of folder move operation. */
    void slotMoveProgress(KJob *, unsigned long progress);
    void slotMoveFinished(KJob *job);
    /** @brief Collect MLT's warning logs. */
    void handleLog(const QString &message);

Q_SIGNALS:
    void docOpened(KdenliveDoc *document);

protected:
    /** @brief Update the timeline according to the MLT XML */
    bool updateTimeline(bool createNewTab, const QString &chunks, const QString &dirty, const QDateTime &documentDate, bool enablePreview);
    KdenliveDoc *m_project{nullptr};

private:
    /** @brief checks if autoback files exists, recovers from it if user says yes, returns true if files were recovered. */
    bool checkForBackupFile(const QUrl &url, bool newFile = false);
    /** @brief Update the sequence producer stored in the project model. */
    void updateSequenceProducer(const QUuid &uuid, std::shared_ptr<Mlt::Producer> prod);

    std::shared_ptr<TimelineItemModel> m_activeTimelineModel;
    QElapsedTimer m_lastSave;
    QTimer m_autoSaveTimer;
    QUrl m_startUrl;
    QString m_loadClipsOnOpen;
    QMap<QString, QString> m_replacementPattern;

    QAction *m_fileRevert;
    KRecentFilesAction *m_recentFilesAction;
    NotesPlugin *m_notesPlugin;
    QStringList m_mltWarnings;
    void saveRecentFiles();
    /** @brief Something went wrong, stop loading file */
    void abortLoading();
    /** @brief Project loading failed, ask user if he wants to open a backup */
    void requestBackup(const QString &errorMessage);
    /** @brief WHen building a sequence producer, ensure we pass along all properties */
    void passSequenceProperties(const QUuid &uuid, std::shared_ptr<Mlt::Producer> prod, Mlt::Tractor tractor, std::shared_ptr<TimelineItemModel> timelineModel,
                                TimelineWidget *timelineWidget);
    /** @brief Ensure sequences are correctly stored in our project model */
    void checkProjectIntegrity();
    /** @brief Opening a project file failed, propose to open a backup */
    void abortProjectLoad(const QUrl &url);
};
