/*
    SPDX-FileCopyrightText: 2007 Jean-Baptiste Mardelle <jb@kdenlive.org>

SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

/** @class KdenliveDoc
 *  @brief Represents a kdenlive project file
 *
 *  Instances of KdenliveDoc classes are created by void MainWindow::newFile(bool showProjectSettings, bool force)
*/

#pragma once

#include <QAction>
#include <QDir>
#include <QList>
#include <QMap>
#include <QObject>
#include <QUuid>
#include <memory>
#include <qdom.h>

#include <kautosavefile.h>
#include "../bin/model/subtitlemodel.hpp"

#include "definitions.h"
#include "utils/gentime.h"
#include "utils/timecode.h"

class MainWindow;
class TrackInfo;
class ProjectClip;
class MarkerListModel;
class Render;
class ProfileParam;
class SubtitleModel;
class MarkerSortModel;

class QUndoGroup;
class QUndoCommand;
class DocUndoStack;

namespace Mlt {
class Profile;
}

/** Object returned by KdenliveDoc::Open(), containing a pointer to a KdenliveDoc
 * (if successful) and also additional information about whether the doc was
 * modified or upgraded, and any error message. If the doc is nullptr, then
 * errorMessage() will return an error string that can be shown to the user.
 */
class DocOpenResult {
public:
    bool isSuccessful() const { return m_succeeded; }
    bool isAborted() const { return m_aborted; }
    /** @returns a unique_ptr to the KdenliveDoc, or an empty unique_ptr */
    std::unique_ptr<KdenliveDoc> getDocument() { return std::move(m_doc); }
    /** @returns an error message if the doc could not be opened. */
    QString getError() const { return m_errorMessage; }
    /** @return true if the doc was upgraded from an older version */
    bool wasUpgraded() const { return m_upgraded; }
    /** @return true if the doc was modified by the validator */
    bool wasModified() const { return m_modified; }
    void setDocument(std::unique_ptr<KdenliveDoc> doc) {
        m_doc = std::move(doc);
        m_succeeded = true;
    }
    void setError(const QString &error) { m_errorMessage = error; }
    void setAborted() { m_aborted = true; }
    void setUpgraded(bool upgraded) { m_upgraded = upgraded; }
    void setModified(bool modified) { m_modified = modified; }

private:
    std::unique_ptr<KdenliveDoc> m_doc;
    QString m_errorMessage = QString();
    QString m_notification = QString();
    bool m_upgraded = false;
    bool m_modified = false;
    bool m_succeeded = false;
    bool m_aborted = false;
};

class KdenliveDoc : public QObject
{
    Q_OBJECT

public:
    // static members
    /** @brief Replace proxy clips with originals for rendering. */
    static void useOriginals(QDomDocument &doc);
    static void processProxyNodes(QDomNodeList producers, const QString &root, const QMap<QString, QString> &proxies);

    /** @brief Create a new empty Kdenlive project with the specified profile and requested number of tracks.
     *
     * @param tracks The number of <video, audio> tracks to create in the project.
    */
    KdenliveDoc(QString projectFolder, QUndoGroup *undoGroup, const QString &profileName, const QMap<QString, QString> &properties,
                const QMap<QString, QString> &metadata, const QPair<int, int> &tracks, int audioChannels, MainWindow *parent = nullptr);
    /** @brief Open an existing Kdenlive project, returning nothing if the project cannot be opened. */
    static DocOpenResult Open(const QUrl &url, const QString &projectFolder, QUndoGroup *undoGroup,
                bool recoverCorruption, MainWindow *parent = nullptr);
    /** @brief Create a dummy project, used for testing. */
    KdenliveDoc(std::shared_ptr<DocUndoStack> undoStack, std::pair<int, int> tracks = {2, 2}, MainWindow *parent = nullptr);
    ~KdenliveDoc() override;
    friend class LoadJob;
    QUuid activeUuid;
    /** @brief True if we are currently closing the project. */
    bool closing;
    /** @brief Get current document's producer. */
    const QByteArray getAndClearProjectXml();
    double fps() const;
    int width() const;
    int height() const;
    QUrl url() const;
    KAutoSaveFile *m_autosave;
    /** @brief Whether the project folder should be in the same folder as the project file (var is only used for new projects)*/
    bool m_sameProjectFolder;
    Timecode timecode() const;
    std::shared_ptr<DocUndoStack> commandStack();

    int getFramePos(const QString &duration);
    /** @brief Get a list of all clip ids that are inside a folder. */
    QStringList getBinFolderClipIds(const QString &folderId) const;

    const QString description(const QString suffix = QString()) const;
    void setUrl(const QUrl &url);
    /** @brief Update path of subtitle url and timewarp sequence playlists. */
    void updateWorkFilesBeforeSave(const QString &newUrl = QString(), bool onRender = false);
    /** @brief Restore tmp work path for subtitle filters after saving. */
    void updateWorkFilesAfterSave();

    void prepareRenderAssets(const QDir &destFolder);
    void restoreRenderAssets();

    /** @brief Defines whether the document needs to be saved. */
    bool isModified() const;
    /** @brief Adds a "modified" attribute to the document root so that a backup
     * will be created the next time the document is saved.
     */
    void requestBackup();
    /** @brief prepare timelinemodels for closing
     */
    void prepareClose();

    /** @brief Returns the project folder, used to store project temporary files. */
    QString projectTempFolder() const;
    /** @brief Returns the folder used to store project data files (titles, etc). */
    QString projectDataFolder(const QString &newPath = QString(), bool folderForAudio = false) const;
    void setZoom(const QUuid &uuid, int horizontal, int vertical = -1);
    QPoint zoom(const QUuid &uuid) const;
    double dar() const;
    /** @brief Returns the project file xml. */
    QDomDocument xmlSceneList(const QString &scene);
    /** @brief Saves the project file xml to a file. */
    bool saveSceneList(const QString &path, const QString &scene);
    void cacheImage(const QString &fileId, const QImage &img) const;
    void setProjectFolder(const QUrl &url);
    void setZone(const QUuid &uuid, int start, int end);
    QPoint zone(const QUuid &uuid) const;
    /** @brief Returns target tracks (video, audio). */
    QPair<int, int> targetTracks(const QUuid &uuid) const;
    /** @brief Load document guides from properties. */
    void loadDocumentGuides(const QUuid &uuid, std::shared_ptr<TimelineItemModel> model);
    /** @brief Load sequence properties from the MLT tractor. */
    void loadSequenceProperties(const QUuid &uuid, Mlt::Properties sequenceProps);
    /** @brief Set a document property. */
    void setDocumentProperty(const QString &name, const QString &value);
    virtual const QString getDocumentProperty(const QString &name, const QString &defaultValue = QString()) const;
    bool hasDocumentProperty(const QString &name) const;
    /** @brief Set a timeline sequence property. */
    void setSequenceProperty(const QUuid &uuid, const QString &name, const QString &value);
    void setSequenceProperty(const QUuid &uuid, const QString &name, int value);
    /** @brief Get a timeline sequence property. */
    const QString getSequenceProperty(const QUuid &uuid, const QString &name, const QString &defaultValue = QString()) const;
    /** @brief Returns true if a sequence property exists. */
    bool hasSequenceProperty(const QUuid &uuid, const QString &name) const;
    /** @brief Delete the sequence property after it has been used. */
    void clearSequenceProperty(const QUuid &uuid, const QString &name);
    const QMap<QString, QString> getSequenceProperties(const QUuid &uuid) const;
    /** @brief Move document properties into sequence properties (mostly useful to convert older KdenliveDoc formats . */
    void importSequenceProperties(const QUuid uuid, const QStringList properties);

    /** @brief Gets the list of renderer properties saved into the document. */
    QMap<QString, QString> getRenderProperties() const;
    /** @brief Read the display ratio from an xml project file. */
    static double getDisplayRatio(const QString &path);
    /** @brief Backup the project file */
    void backupLastSavedVersion(const QString &path);
    /** @brief Returns the document metadata (author, copyright, ...) */
    const QMap<QString, QString> metadata() const;
    /** @brief Set the document metadata (author, copyright, ...) */
    void setMetadata(const QMap<QString, QString> &meta);
    /** @brief Get all document properties that need to be saved */
    QMap<QString, QString> documentProperties();
    bool useProxy() const;
    bool useExternalProxy() const;
    /** @brief Returns true if a proxy clip should be automatically generated for this width.
     *  if width < 0, returns true if the document's automatic proxy generation is enabled
     */
    bool autoGenerateProxy(int width) const;
    /** @brief Returns true if a proxy clip should be automatically generated for this width.
     *  if width < 0, returns true if the document's automatic proxy generation is enabled
     */
    bool autoGenerateImageProxy(int width) const;
    /** @brief Saves effects embedded in project file. */
    void saveCustomEffects(const QDomNodeList &customeffects);
    void resetProfile(bool reloadThumbs);
    /** @brief Returns true if the profile file has changed. */
    bool profileChanged(const QString &profile) const;
    /** @brief Get an action from main actioncollection. */
    QAction *getAction(const QString &name);
    /** @brief Add an action to main actioncollection. */
    void doAddAction(const QString &name, QAction *a, const QKeySequence &shortcut);
    void invalidatePreviews(QList<int> chunks);
    void previewProgress(int p);
    /** @brief Select most appropriate rendering profile for timeline preview based on fps / size. */
    void selectPreviewProfile();
    void displayMessage(const QString &text, MessageType type = DefaultMessage, int timeOut = 0);
    /** @brief Get a cache directory for this project. */
    const QDir getCacheDir(CacheType type, bool *ok, const QUuid uuid = QUuid()) const;
    /** @brief Create standard cache dirs for the project */
    void initCacheDirs();
    /** @brief Get a list of all proxy hash used in this project */
    QStringList getProxyHashList();
    /** @brief Move project data files to new url */
    const QList<QUrl> getProjectData(const QString &dest, bool *ok);

    /** @brief Returns a pointer to the guide model of timeline uuid */
    std::shared_ptr<MarkerListModel> getGuideModel(const QUuid uuid) const;
    std::shared_ptr<MarkerSortModel> getFilteredGuideModel(const QUuid uuid);

    // TODO REFAC: delete */
    Render *renderer();
    /** @brief Returns MLT's root (base path) */
    const QString documentRoot() const;
    /** @brief Returns true if timeline preview settings changed*/
    bool updatePreviewSettings(const QString &profile);
    /** @brief Returns the recommended proxy profile parameters */
    QString getAutoProxyProfile();
    /** @brief Returns the number of clips in this project (useful to show loading progress) */
    int clipsCount() const;
    int updateClipsCount();
    /** @brief Returns a list of project tags (color / description) */
    QMap <int, QStringList> getProjectTags() const;
    /** @brief Returns the number of audio channels for this project */
    int audioChannels() const;
    /** @brief Ensure we don't have leftover preview chunks (created after last save */
    void cleanupTimelinePreview(const QDateTime &documentDate);
    /** @brief Returns the guides categories for the project in format {name:index:#color} */
    const QStringList guidesCategories();
    /** @brief Set the guides categories for the project in format {name:index:#color} */
    void updateGuideCategories(const QStringList &categories, const QMap<int, int> remapCategories = {});

    /**
     * If the document used a decimal point different than “.”, it is stored in this property.
     * @return Original decimal point, or an empty string if it was “.” already
     */
    QString &modifiedDecimalPoint();
    void setModifiedDecimalPoint(const QString &decimalPoint) { m_modifiedDecimalPoint = decimalPoint; }
    /** @brief Get the list of secondary timelines uuid */
    const QStringList getSecondaryTimelines() const;

    /** @brief Returns a path for current document's subtitle file.
     *  uuid is appended to the path if this is not the primary timeline
     *  If final is true, this will be the project filename with ".srt" appended. Otherwise a file in /tmp */
    const QString subTitlePath(const QUuid &uuid, bool final);
    /** @brief Returns the list of all used subtitles paths. */
    QStringList getAllSubtitlesPath(bool final);
    /** @brief Creates a new project. */
    QDomDocument createEmptyDocument(int videotracks, int audiotracks, bool disableProfile = true);
    /** @brief Return the document version. */
    double getDocumentVersion() const;

    /** @brief Returns true if this project has subtitles. */
    bool hasSubtitles() const;
    /** @brief Generate a temporary subtitle file for a zone. */
    void generateRenderSubtitleFile(const QUuid &uuid, int in, int out, const QString &subtitleFile);
    /** @brief Returns the default definition  for guide categories.*/
    static const QStringList getDefaultGuideCategories();
    void addTimeline(const QUuid &uuid, std::shared_ptr<TimelineItemModel> model);
    /** @brief Load the guides into the model for a sequence.*/
    void loadSequenceGroupsAndGuides(const QUuid &uuid);
    /** @brief Get a timeline by its uuid.*/
    std::shared_ptr<TimelineItemModel> getTimeline(const QUuid &uuid);
    void closeTimeline(const QUuid &uuid);
    void checkUsage(const QUuid &uuid);
    QList<QUuid> getTimelinesUuids() const;
    /** @brief Returns the number of timelines in this project.*/
    int timelineCount() const;
    /** @brief Get the currently active project name.*/
    const QString projectName() const;
    /** @brief Returns the project's main uuid.*/
    const QUuid uuid() const;
    /** @brief Returns true if a sequence thumbnail needs an update.*/
    bool sequenceThumbRequiresRefresh(const QUuid &uuid) const;
    /** @brief Thumbnail for a sequence was updated, remove it from the update list.*/
    void sequenceThumbUpdated(const QUuid &uuid);

private:
    /** @brief Create a new KdenliveDoc using the provided QDomDocument (an
     * existing project file), used by the Open() named constructor. */
    KdenliveDoc(const QUrl &url, QDomDocument& newDom, QString projectFolder, QUndoGroup *undoGroup,
        MainWindow *parent = nullptr);
    /** @brief Set document default properties using hard-coded values and KdenliveSettings.
     *  @param newDocument true if we are creating a new document, false when opening an existing one
     */
    void initializeProperties(bool newDocument = true);
    QUuid m_uuid;
    QDomDocument m_document;
    int m_clipsCount;
    /** @brief MLT's root (base path) that is stripped from urls in saved xml */
    QString m_documentRoot;
    Timecode m_timecode;
    std::shared_ptr<DocUndoStack> m_commandStack;
    QString m_searchFolder;

    /** @brief Tells whether the current document has been changed after being saved. */
    bool m_modified;

    /** @brief The default recommended proxy extension */
    QString m_proxyExtension;

    /** @brief The default recommended proxy params */
    QString m_proxyParams;

    /** @brief Tells whether the current document was modified by Kdenlive on opening, and a backup should be created on save. */
    enum DOCSTATUS { CleanProject, ModifiedProject, UpgradedProject };
    DOCSTATUS m_documentOpenStatus;

    QUrl m_url;

    /** @brief The project folder, used to store project files (titles, effects...). */
    QString m_projectFolder;
    QList<int> m_undoChunks;
    QMap<QString, QString> m_documentProperties;
    QMap<QString, QString> m_documentMetadata;
    QMap<QUuid, QMap<QString, QString>> m_sequenceProperties;
    QUuid m_filteredTimelineUuid;
    QSet<QUuid> m_sequenceThumbsNeedsRefresh;

    QString m_modifiedDecimalPoint;
    /** @brief A list of guide models for this project (one for each timeline). */
    QMap<QUuid, std::shared_ptr<TimelineItemModel>> m_timelines;
    QString searchFileRecursively(const QDir &dir, const QString &matchSize, const QString &matchHash) const;

    /** @brief Creates a new project. */
    QDomDocument createEmptyDocument(const QList<TrackInfo> &tracks, bool disableProfile);

    /** @brief Updates the project folder location entry in the kdenlive file dialogs to point to the current project folder. */
    void updateProjectFolderPlacesEntry();
    /** @brief Only keep some backup files, delete some */
    void cleanupBackupFiles();
    /** @brief Load document properties from the xml file */
    void loadDocumentProperties();
    /** @brief update document properties to reflect a change in the current profile */
    void updateProjectProfile(bool reloadProducers = false, bool reloadThumbs = false);
    /** @brief initialize proxy settings based on hw status */
    void initProxySettings();

public Q_SLOTS:
    void slotCreateTextTemplateClip(const QString &group, const QString &groupId, QUrl path);

    /** @brief Sets the document as modified or up to date.
     *
     * If crash recovery is turned on, a timer calls KdenliveDoc::slotAutoSave() \n
     * Emits docModified connected to MainWindow::slotUpdateDocumentState \n
     *
     * @param mod (optional) true if the document has to be saved */
    void setModified(bool mod = true);
    QMap<QString, QString> proxyClipsById(const QStringList &ids, bool proxy, const QMap<QString, QString> &proxyPath = QMap<QString, QString>());
    void slotProxyCurrentItem(bool doProxy, QList<std::shared_ptr<ProjectClip>> clipList = QList<std::shared_ptr<ProjectClip>>(), bool force = false,
                              QUndoCommand *masterCommand = nullptr);
    /** @brief Saves the current project at the autosave location.
     *
     * The autosave files are in ~/.kde/data/stalefiles/kdenlive/ */
    void slotAutoSave(const QString &scene);
    void switchProfile(ProfileParam* pf, const QString &clipName);

private Q_SLOTS:
    void slotModified();
    void slotSwitchProfile(const QString &profile_path, bool reloadThumbs);
    /** @brief Check if we did a new action invalidating more recent undo items. */
    void checkPreviewStack(int ix);
    /** @brief Display error message on failed move. */
    void slotMoveFinished(KJob *job);
    /** @brief Save the project guide categories in the document properties. */
    void saveGuideCategories();

Q_SIGNALS:
    void resetProjectList();

    /** @brief Informs that the document status has been changed.
     *
     * If the document has been modified, it's called with true as an argument. */
    void docModified(bool);
    void selectLastAddedClip(const QString &);
    /** @brief When creating a backup file, also save a thumbnail of current timeline */
    void saveTimelinePreview(const QString &path);
    /** @brief Trigger the autosave timer start */
    void startAutoSave();
    /** @brief Current doc created effects, reload list */
    void reloadEffects(const QStringList &paths);
    /** @brief Fps was changed, update timeline (changed = 1 means no change) */
    void updateFps(double changed);
    /** @brief If a command is pushed when we are in the middle of undo stack, invalidate further undo history */
    void removeInvalidUndo(int ix);
    /** @brief Update compositing info */
    void updateCompositionMode(bool);
};
