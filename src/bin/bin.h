/*
SPDX-FileCopyrightText: 2012 Till Theato <root@ttill.de>
SPDX-FileCopyrightText: 2014 Jean-Baptiste Mardelle <jb@kdenlive.org>
This file is part of Kdenlive. See www.kdenlive.org.

SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include "abstractprojectitem.h"
#include "project/transcodeseek.h"
#include "utils/timecode.h"

#include <KMessageWidget>
#include <kddockwidgets/DockWidget.h>

#include <QActionGroup>
#include <QDir>
#include <QDomElement>
#include <QFuture>
#include <QLineEdit>
#include <QListView>
#include <QListWidget>
#include <QMutex>
#include <QPushButton>
#include <QTimer>
#include <QTreeView>
#include <QUrl>
#include <QWidget>

#include <KRecentDirs>

class AbstractProjectItem;
class BinItemDelegate;
class BinListItemDelegate;
class ClipController;
class EffectStackModel;
class InvalidDialog;
class TranscodeSeek;
class KdenliveDoc;
class KSelectAction;
class KeyframeModelList;
class TagWidget;
class Monitor;
class ProjectClip;
class ProjectFolder;
class ProjectItemModel;
class ProjectSortProxyModel;
class QDockWidget;
class QMenu;
class QScrollArea;
class QTimeLine;
class QToolBar;
class QToolButton;
class QUndoCommand;
class QVBoxLayout;
class SmallJobLabel;
class MediaBrowser;
class MarkerListModel;

namespace Mlt {
class Producer;
}

/** @class MyListView
    @brief \@todo Describe class MyListView
    @todo Describe class MyListView
 */
class MyListView : public QListView
{
    Q_OBJECT
public:
    explicit MyListView(QWidget *parent = nullptr);

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void focusInEvent(QFocusEvent *event) override;
    void enterEvent(QEnterEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void dropEvent(QDropEvent *event) override;
    void paintEvent(QPaintEvent *event) override;

Q_SIGNALS:
    void focusView();
    void updateDragMode(PlaylistState::ClipState type);
    void displayBinFrame(QModelIndex ix, int frame, bool storeFrame = false);
    void performDrag(const QModelIndexList indexes);

private:
    QPoint m_startPos;
    PlaylistState::ClipState m_dragType;
    QModelIndex m_lastHoveredItem;
    QModelIndexList m_clickedIndexes;
};

/** @class MyTreeView
    @brief \@todo Describe class MyTreeView
    @todo Describe class MyTreeView
 */
class MyTreeView : public QTreeView
{
    Q_OBJECT
    Q_PROPERTY(bool editing READ isEditing WRITE setEditing NOTIFY editingChanged)
public:
    explicit MyTreeView(QWidget *parent = nullptr);
    void setEditing(bool edit);

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void focusInEvent(QFocusEvent *event) override;
    void enterEvent(QEnterEvent *) override;
    void leaveEvent(QEvent *event) override;
    void dropEvent(QDropEvent *event) override;
    void paintEvent(QPaintEvent *event) override;

protected Q_SLOTS:
    void closeEditor(QWidget *editor, QAbstractItemDelegate::EndEditHint hint) override;
    void editorDestroyed(QObject *editor) override;

private:
    QPoint m_startPos;
    PlaylistState::ClipState m_dragType;
    QModelIndex m_lastHoveredItem;
    QModelIndexList m_clickedIndexes;
    bool m_editing;
    bool isEditing() const;

Q_SIGNALS:
    void focusView();
    void updateDragMode(PlaylistState::ClipState type);
    void displayBinFrame(QModelIndex ix, int frame, bool storeFrame = false);
    void processDragEnd();
    void selectCurrent();
    void editingChanged();
    void performDrag(const QModelIndexList indexes);
};

/** @class SmallJobLabel
    @brief \@todo Describe class SmallJobLabel
    @todo Describe class SmallJobLabel
 */
class SmallJobLabel : public QPushButton
{
    Q_OBJECT
public:
    explicit SmallJobLabel(QWidget *parent = nullptr);
    static const QString getStyleSheet(const QPalette &p);
    void setAction(QAction *action);

private:
    enum ItemRole { NameRole = Qt::UserRole, DurationRole, UsageRole };

    QTimeLine *m_timeLine{nullptr};
    QAction *m_action{nullptr};
    QMutex m_locker;

public Q_SLOTS:
    void slotSetJobCount(int jobCount);

private Q_SLOTS:
    void slotTimeLineChanged(qreal value);
    void slotTimeLineFinished();
};

/** @class LineEventEater
    @brief \@todo Describe class LineEventEater
    @todo Describe class LineEventEater
 */
class LineEventEater : public QObject
{
    Q_OBJECT
public:
    explicit LineEventEater(QObject *parent = nullptr);

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;

Q_SIGNALS:
    void clearSearchLine();
    void showClearButton(bool);
};

/**
 * @class Bin
 * @brief The bin widget takes care of both item model and view upon project opening.
 */
class Bin : public QWidget
{
    Q_OBJECT

    /** @brief Defines the view types (icon view, tree view,...)  */
    enum BinViewType { BinTreeView, BinIconView, BinUnknownView };

public:
    explicit Bin(std::shared_ptr<ProjectItemModel> model, QWidget *parent = nullptr, bool isMainBin = true, BinViewType type = BinUnknownView);
    ~Bin() override;

    struct ClipZoneInfo
    {
        QString clipId;
        QPoint zone;
        int seekFrame;
    };

    bool isLoading;
    /** @brief Sets the document for the bin and initialize some stuff  */
    const QString setDocument(KdenliveDoc *project, const QString &id = QString());
    /** @brief Delete all project related data, to be called before setDocument  */
    void cleanDocument();

    /** @brief Create a clip item from its xml description  */
    void createClip(const QDomElement &xml);

    /** @brief Used to notify the Model View that an item was updated */
    void emitItemUpdated(std::shared_ptr<AbstractProjectItem> item);

    /** @brief Open a producer in the clip monitor */
    void openProducer(std::shared_ptr<ProjectClip> controller, const QUuid &sequenceUuid = QUuid());
    void openProducer(std::shared_ptr<ProjectClip> controller, int in, int out);

    /** @brief Get a clip from it's id */
    std::shared_ptr<ProjectClip> getBinClip(const QString &id);
    /** @brief Get a clip's name from it's id */
    const QString getBinClipName(const QString &id) const;
    /** @brief Returns the duration of a given clip. */
    size_t getClipDuration(int itemId) const;
    /** @brief Returns the frame size of a given clip. */
    QSize getFrameSize(int itemId) const;
    /** @brief Returns the state of a given clip: AudioOnly, VideoOnly, Disabled (Disabled means it has audio and video capabilities */
    std::pair<PlaylistState::ClipState, ClipType::ProducerType> getClipState(int itemId) const;

    /** @brief Add markers on clip \@param binId at \@param markersData with @comments text if given */
    void addClipMarker(const QString &binId, const QMap<int, QString> &markersData);

    /** @brief Get the count of all markers in all clips using this category */
    int getAllClipMarkers(int category) const;

    /** @brief Remove all clip markers using a category */
    void removeMarkerCategories(QList<int> toRemove, const QMap<int, int> remapCategories);

    /** @brief Returns a list of selected clip ids.
     *  @param allowSubClips: if true, will include subclip ids in the form: "master clip id/in/out"
     */
    std::vector<QString> selectedClipsIds(bool allowSubClips = false, bool allowFolders = false);

    // Returns the selected clips
    QList<std::shared_ptr<ProjectClip>> selectedClips();

    /** @brief Current producer has changed, refresh monitor and timeline*/
    void refreshClip(const QString &id);

    void setupMenu();

    /** @brief The source file was modified, we will reload it soon, disable item in the meantime */
    void setWaitingStatus(const QString &id);

    const QString getDocumentProperty(const QString &key);

    /** @brief Ask MLT to reload this clip's producer  */
    void reloadClip(const QString &id);

    /** @brief refresh monitor (if clip changed)  */
    void reloadMonitorIfActive(const QString &id);
    /** @brief refresh monitor stream selector  */
    void reloadMonitorStreamIfActive(const QString &id);
    /** @brief Update timeline targets according to selected audio streams */
    void updateTargets(QString id = QStringLiteral("-1"));
    /** @brief Update some timeline related stuff, like used clips bound, audio targets,... */
    void sequenceActivated();

    /** @brief Move clips inside the bin (from one folder to another)
        @param ids a map of ids: {item id, {new parent, old parent}}
        @param redo if true, change to new parent, otherwise to old parent
    */
    void doMoveClips(QMap<QString, std::pair<QString, QString>> ids, bool redo, bool dropFromSameSource);
    /** @brief Block/unblock signals from the bin selection model, useful for operations affecting many clips */
    void blockBin(bool block);
    void doMoveFolder(const QString &id, const QString &newParentId);
    void setupGeneratorMenu();
    QMenu *addClipMenu() const;
    void setReadyCallBack(const std::function<void(const QString &)> &cb);
    void setSuggestedDuration(int duration);
    void clearMonitor();

    /** @brief Set focus to the Bin view. */
    void focusBinView();
    /** @brief Get a string list of all clip ids that are inside a folder defined by id. */
    QStringList getBinFolderClipIds(const QString &id) const;
    /** @brief Build a rename subclip command. */
    void renameSubClipCommand(const QString &id, const QString &newName, const QString &oldName, int in, int out);
    /** @brief Rename a clip zone (subclip). */
    void renameSubClip(const QString &id, const QString &newName, int in, int out);

    /** @brief Edit an effect settings to a bin clip. */
    void editMasterEffect(const std::shared_ptr<AbstractProjectItem> &clip);
    /** @brief An effect setting was changed, update stack if displayed. */
    void updateMasterEffect(ClipController *ctl);
    void rebuildMenu();
    void refreshIcons();

    /** @brief This function change the global enabled state of the bin effects
     */
    void setBinEffectsEnabled(bool enabled, bool refreshMonitor = true);

    void requestAudioThumbs(const QString &id, long duration);
    /** @brief Proxy status for the project changed, update. */
    void refreshProxySettings();
    /** @brief A clip is ready, update its info panel if displayed. */
    void emitRefreshPanel(const QString &id);
    /** @brief Returns true if there is a clip in bin. Timeline clips are ignored. */
    bool hasUserClip() const;
    /** @brief Trigger reload of all clips. */
    void reloadAllProducers(bool reloadThumbs = true);
    /** @brief Ensure all audio thumbs have been created */
    void checkAudioThumbs();
    /** @brief Get usage stats for project bin. */
    void getBinStats(uint *used, uint *unused, qint64 *usedSize, qint64 *unusedSize);
    /** @brief Returns the clip properties dockwidget. */
    KDDockWidgets::QtWidgets::DockWidget *clipPropertiesDock();
    void rebuildProxies();
    /** @brief Return a list of all clips hashes used in this project */
    QStringList getProxyHashList();
    /** @brief Get binId of the current selected folder */
    const QString getCurrentFolder();
    /** @brief Save a clip zone as MLT playlist */
    void saveZone(const QStringList &info, const QDir &dir);
    /** @brief A bin clip changed (its effects), invalidate preview */
    void invalidateClip(const QString &binId);
    /** @brief A bin clip changed (its effects), invalidate audio preview */
    void invalidateClipAudio(const QString &binId);
    /** @brief Update sequence audio thumb if necessary */
    void rebuildAudioThumb(const QString &binId);
    /** @brief Recreate missing proxies on document opening */
    void checkMissingProxies();
    /** @brief Save folder state (expanded or not) */
    void saveFolderState();
    /** @brief Load folder state (expanded or not), zoom level and possible other project stored Bin settings */
    void loadBinProperties(const QStringList &foldersToExpand, int zoomLevel = -1);
    /** @brief gets a QList of all clips used in timeline */
    QList<int> getUsedClipIds();
    /** @brief Update a new timeline clip when it has been changed
     * @param uuid the uuid of the timeline clip that was changed
     * @param id the updated duration of the timeline clip
     * * @param current the uuid of the currently active timeline
     */
    void updateSequenceClip(const QUuid &uuid, std::pair<int, int> durations, int pos, bool forceUpdate = false);
    /** @brief Update a sequence AV info (has audio/video) */
    void updateSequenceAVType(const QUuid &uuid, int tracksCount);

    void selectAll();
    /** @brief Save an mlt playlist from a bin id and a list of cuts
     * @param binId the id of the source clip for zones
     * @param savePath the path for the resulting playlist
     * @param zones the source cli pzones that will be put in the result playlist
     * @param properties some extra properties that will be set on the producer
     * @param createNew if true, the playlist will be added as a new clip in project binId */
    void savePlaylist(const QString &binId, const QString &savePath, const QVector<QPoint> &zones, const QMap<QString, QString> &properties, bool createNew);
    /** @brief Do some checks on the profile */
    static void checkProfile(const std::shared_ptr<Mlt::Producer> &producer);
    /** @brief Should we process a profile check for added clips */
    std::atomic<bool> shouldCheckProfile;
    /** @brief Set the message for key binding info. */
    void updateKeyBinding(const QString &bindingMessage = QString());
    /** @brief Returns true if a clip with id cid is visible in this bin. */
    bool containsId(const QString &cid) const;
    void replaceSingleClip(const QString clipId, const QString &newUrl);
    /** @brief List all clips referenced in a timeline sequence. */
    QStringList sequenceReferencedClips(const QUuid &uuid) const;
    /** @brief Define a thumbnail for a sequence clip. */
    void setSequenceThumbnail(const QUuid &uuid, int frame);
    /** @brief When saving or rendering, copy timewarp temporary playlists to the correct folder. */
    void moveTimeWarpToFolder(const QDir sequenceFolder, bool copy);
    /** @brief Create new sequence clip
     * @param aTracks the audio tracks count, use default if -1
     * @param vTracks the video tracks count, use default if -1 */
    void buildSequenceClip(int aTracks = -1, int vTracks = -1);
    const QString buildSequenceClipWithUndo(Fun &undo, Fun &redo, int aTracks = -1, int vTracks = -1, QString suggestedName = QString());
    /** @brief Returns true if the project uses a clip with variable framerate. */
    bool usesVariableFpsClip();
    void applyClipAssetGroupCommand(int cid, const QString &assetId, const QModelIndex &index, const QString &previousValue, QString value,
                                    QUndoCommand *command);
    void applyClipAssetGroupKeyframeCommand(int cid, const QString &assetId, const QModelIndex &index, GenTime pos, const QVariant &previousValue,
                                            const QVariant &value, int ix, QUndoCommand *command);
    void applyClipAssetGroupMultiKeyframeCommand(int cid, const QString &assetId, const QList<QModelIndex> &indexes, GenTime pos,
                                                 const QStringList &sourceValues, const QStringList &values, QUndoCommand *command);
    int clipAssetGroupInstances(int cid, const QString &assetId);
    /** @brief Return all similar keyframe models from the selection */
    QList<std::shared_ptr<KeyframeModelList>> getGroupKeyframeModels(int bid, const QString &assetId);
    void removeEffectFromGroup(int bid, const QString &assetId, int eid);
    void disableEffectFromGroup(int cid, const QString &assetId, bool disable, Fun &undo, Fun &redo);
    /** @brief The root id of the folder that this bin displays */
    const QString rootFolderId() const;
    /** @brief Export some basic info (root folder, view type) to a string to save it */
    const QString binInfoToString() const;
    /** @brief Load info (root folder, view type) from a string */
    const QString loadInfo(const QStringList binInfo, const QStringList existingNames);
    /** @brief Display a clip effect stack */
    void showItemEffectStack(ObjectId owner);
    /** @brief Return the bin search widget */
    QLineEdit *searchLine();
    /** @brief Get marker models for all clips */
    const QList<std::shared_ptr<MarkerListModel>> getAllClipsMarkers();
    /** @brief Get the first selected clip*/
    std::shared_ptr<ProjectClip> getFirstSelectedClip();
    /** @brief Expand / collapse current item */
    void expandCurrent();
    /** @brief Expand / collapse all items */
    void expandAll();
    bool isMainBin() const;
    void buildPropertiesDock(KDDockWidgets::QtWidgets::DockWidget *parentDock);
    const QString slotAddClipToProject(const QUrl &url);
    /** @brief Save all sequence audio thumbnails to disk */
    void saveSequenceAudioThumb();
    /** @brief Generate or load all sequence audio thumbnails from disk */
    void loadSequenceAudioThumb();

public Q_SLOTS:
    const QString slotUrlsDropped(const QList<QUrl> urls, const QModelIndex parent);

private Q_SLOTS:
    void slotAddClip();
    /** @brief Reload clip from disk */
    void slotReloadClip();
    /** @brief Replace clip with another file */
    void slotReplaceClip();
    /** @brief Replace audio or video component in timeline with another file */
    void slotReplaceClipInTimeline();
    /** @brief Set sorting column */
    void slotSetSorting();
    /** @brief Show/hide date column */
    void slotShowColumn(bool show);
    /** @brief Ensure current item is selected */
    void ensureCurrent();
    /** @brief Go to parent folder */
    void slotBack();
    /** @brief Setup the bin view type (icon view, tree view, ...).
     * @param action The action whose data defines the view type or nullptr to keep default view */
    void slotInitView(QAction *action);
    void slotSetIconSize(int size);
    void selectProxyModel(const QModelIndex &id);
    void slotSaveHeaders();

    /** @brief Reset all text and log data from info message widget. */
    void slotResetInfoMessage();
    /** @brief Show dialog prompting for removal of invalid clips. */
    void slotQueryRemoval(const QString &id, const QString &url, const QString &errorMessage);
    /** @brief Request display of current clip in monitor. */
    void slotOpenCurrent();
    void slotZoomView(bool zoomIn);
    /** @brief Widget gained focus, make sure we display effects for master clip. */
    void slotGotFocus();
    /** @brief Rename a Bin Item. */
    void slotRenameItem();
    void doRefreshPanel(const QString &id);
    /** @brief Enable item view and hide message */
    void slotMessageActionTriggered();
    /** @brief Request editing of title or slideshow clip */
    void slotEditClip();
    /** @brief Enable / disable clear button on search line
     *  this is a workaround foq Qt bug 54676
     */
    void showClearButton(bool show);
    /** @brief Display a defined frame in bin clip thumbnail
     */
    void showBinFrame(const QModelIndex &ix, int frame, bool storeFrame = false);
    /** @brief Switch a tag on/off on current selection
     */
    void switchTag(const QString &tag, bool add);
    /** @brief Update project tags
     */
    void updateTags(const QMap <int, QStringList> &previousTags, const QMap <int, QStringList> &tags);
    void rebuildFilters(int tagsCount);
    /** @brief Switch a tag on  a clip list
     */
    void editTags(const QList <QString> &allClips, const QString &tag, bool add);
    /** @brief Update the string description of the clips count, like: 123 clips (3 selected). */
    void updateClipsCount();
    /** @brief Update the menu entry listing the occurrences of a clip in timeline. */
    void updateTimelineOccurrences();
    /** @brief Set (or unset) the default folder for newly created sequence clips. */
    void setDefaultSequenceFolder(bool enable);
    /** @brief Set (or unset) the default folder for newly created audio captures. */
    void setDefaultAudioCaptureFolder(bool enable);
    /** @brief Fetch the filters from the UI and apply them to the proxy model */
    void slotApplyFilters(bool fromFilterButton = false);
    /** @brief Open a new Bin widget */
    void slotOpenNewBin();
    /** @brief Open clip in monitor */
    void openClipInMonitor(std::shared_ptr<ProjectClip> clip, int in = -1, int out = -1, const QUuid &uuid = QUuid(), int seekFrame = -1);
    /** @brief Perform the drag */
    bool performDrag(const QModelIndexList indexes);

public Q_SLOTS:

    void slotRemoveInvalidClip(const QString &id, bool replace, const QString &errorMessage);
    void slotDeleteClip();
    void slotItemDoubleClicked(const QModelIndex &ix, const QPoint &pos, uint modifiers);
    void slotSwitchClipProperties(const std::shared_ptr<ProjectClip> &clip);
    void slotSwitchClipProperties();
    /** @brief Creates a new folder with optional name, and returns new folder's id */
    void slotAddFolder();
    void slotCreateProjectClip();
    void slotEditClipCommand(const QString &id, const QMap<QString, QString> &oldProps, const QMap<QString, QString> &newProps);
    /** @brief Start a filter job requested by a filter applied in timeline */
    void slotStartFilterJob(/*const ItemInfo &info,*/ const QString &id, QMap<QString, QString> &filterParams, QMap<QString, QString> &consumerParams,
                            QMap<QString, QString> &extraParams);
    void slotItemDropped(const QStringList ids, const QModelIndex parent, bool dropFromSameSource);
    void slotEffectDropped(const QStringList &effectData, const QModelIndex &parent);
    void slotTagDropped(const QString &tag, const QModelIndex &parent);
    void slotItemEdited(const QModelIndex &, const QModelIndex &, const QVector<int> &);
    /** @brief Open current clip in an external editing application */
    void slotOpenClipExtern();
    void slotDuplicateClip();
    void slotLocateClip();
    void showClipProperties(const std::shared_ptr<ProjectClip> &clip, bool forceRefresh = false);
    /** @brief Add extra data to a clip. */
    void slotAddClipExtraData(const QString &id, const QString &key, const QString &data = QString());
    void slotUpdateClipProperties(const QString &id, const QMap<QString, QString> &properties, bool refreshPropertiesPanel);
    /** @brief Add effect to active Bin clip (used when double clicking an effect in list). */
    void slotAddEffect(std::vector<QString> ids, const QStringList &effectData);
    void slotExpandUrl(/*const ItemInfo &info,*/ const QString &url, QUndoCommand *command);
    /** @brief Abort all ongoing operations to prepare close. */
    void abortOperations();
    void doDisplaySimpleMessage(const QString &text, KMessageWidget::MessageType type);
    void doDisplayMessage(const QString &text, KMessageWidget::MessageType type, const QList<QAction *> &actions = QList<QAction *>(), bool showCloseButton = false, BinMessage::BinCategory messageCategory = BinMessage::BinCategory::NoMessage);
    void doDisplayMessage(const QString &text, KMessageWidget::MessageType type, const QString logInfo);
    /** @brief Select a clip in the Bin from its id. */
    void selectClipById(const QString &id, int frame = -1, const QPoint &zone = QPoint(), bool activateMonitor = true);
    void droppedUrls(const QList<QUrl> &urls, const QString &folderInfo = QString());
    /** @brief Adjust project profile to current clip. */
    void adjustProjectProfileToItem();
    /** @brief Check and propose auto adding audio tracks.
     * @param clipId The clip whose streams have to be checked
     * @param minimumTracksCount the number of active streams for this clip
     */
    void checkProjectAudioTracks(QString clipId, int minimumTracksCount);
    void showTitleWidget(const std::shared_ptr<ProjectClip> &clip);
    /** @brief Add a clip in a specially named folder */
    bool addProjectClipInFolder(const QString &path, const QString &sourceClipId, const QString &sourceFolder, const QString &jobId);
    /** @brief Add a filter with some presets to a clip */
    void addFilterToClip(const QString &sourceClipId, const QString &filterId, stringMap params);
    /** @brief Check if a clip profile matches project, propose switch otherwise */
    void slotCheckProfile(const QString &binId);
    /** @brief A non seekable clip was added to project, propose transcoding */
    void requestTranscoding(const QString &id, TranscodeSeek::TranscodeInfo info, bool checkProfile, const QString &suffix = QString(),
                            const QString &message = QString());
    /** @brief Display the transcode to edit friendly format for currently selected bin clips */
    void requestSelectionTranscoding(bool forceReplace = false);
    /** @brief Build the project bin audio/video icons according to color theme */
    void slotUpdatePalette();
    /** @brief Import multiple video streams in a clip */
    void processMultiStream(const QString &clipId, QList<int> videoStreams, QList<int> audioStreams);
    /** @brief Transcode all used clips with variable framerate to edit friendly format. */
    void transcodeUsedClips();

protected:
    /* This function is called whenever an item is selected to propagate signals
       (for ex request to show the clip in the monitor)
    */
    void setCurrent(const std::shared_ptr<AbstractProjectItem> &item);
    void selectClip(const std::shared_ptr<ProjectClip> &clip);
    void contextMenuEvent(QContextMenuEvent *event) override;
    bool eventFilter(QObject *obj, QEvent *event) override;
    QSize sizeHint() const override;

private:
    bool m_isMainBin;
    std::shared_ptr<ProjectItemModel> m_itemModel;
    QAbstractItemView *m_itemView;
    BinItemDelegate *m_binTreeViewDelegate;
    BinListItemDelegate *m_binListViewDelegate;
    std::unique_ptr<ProjectSortProxyModel> m_proxyModel;
    QToolBar *m_toolbar;
    KdenliveDoc *m_doc{nullptr};
    QLineEdit *m_searchLine;
    QToolButton *m_addButton;
    KSelectAction *m_listTypeAction{nullptr};
    QMenu *m_extractAudioAction{nullptr};
    QAction *m_transcodeAction{nullptr};
    QMenu *m_clipsActionsMenu{nullptr};
    QAction *m_inTimelineAction{nullptr};
    QAction *m_showDate{nullptr};
    QAction *m_showDesc{nullptr};
    QAction *m_showRating{nullptr};
    QAction *m_sortDescend{nullptr};
    /** @brief Default view type (icon, tree, ...) */
    BinViewType m_listType;
    /** @brief Default icon size for the views. */
    QSize m_baseIconSize;
    /** @brief Keeps the column width info of the tree view. */
    QByteArray m_headerInfo;
    QVBoxLayout *m_layout;
    KDDockWidgets::QtWidgets::DockWidget *m_propertiesDock;
    QScrollArea *m_propertiesPanel;
    QSlider *m_slider;
    QIcon m_blankThumb;
    QMenu *m_menu{nullptr};
    QAction *m_openAction{nullptr};
    QAction *m_editAction{nullptr};
    QAction *m_reloadAction{nullptr};
    QAction *m_replaceAction{nullptr};
    QAction *m_replaceInTimelineAction{nullptr};
    QAction *m_duplicateAction{nullptr};
    QAction *m_locateAction{nullptr};
    QAction *m_proxyAction{nullptr};
    QAction *m_deleteAction{nullptr};
    QAction *m_openInBin{nullptr};
    QAction *m_sequencesFolderAction{nullptr};
    QAction *m_audioCapturesFolderAction{nullptr};
    QAction *m_addClip{nullptr};
    QAction *m_createFolderAction{nullptr};
    QAction *m_renameAction{nullptr};
    QMenu *m_jobsMenu{nullptr};
    QAction *m_cancelJobs{nullptr};
    QAction *m_discardCurrentClipJobs{nullptr};
    QAction *m_discardPendingJobs{nullptr};
    QAction *m_upAction{nullptr};
    QAction *m_tagAction{nullptr};
    QActionGroup *m_sortGroup{nullptr};
    SmallJobLabel *m_infoLabel{nullptr};
    TagWidget *m_tagsWidget;
    QMenu *m_filterMenu{nullptr};
    QActionGroup m_filterTagGroup;
    QActionGroup m_filterRateGroup;
    QActionGroup m_filterUsageGroup;
    QActionGroup m_filterTypeGroup;
    QToolButton *m_filterButton;
    /** @brief The info widget for failed jobs. */
    KMessageWidget *m_infoMessage;
    QTimer m_messageTimer;
    BinMessage::BinCategory m_currentMessage;
    QStringList m_errorLog;
    /** @brief Dialog listing invalid clips on load. */
    InvalidDialog *m_invalidClipDialog;
    /** @brief Dialog listing non seekable clips on load. */
    TranscodeSeek *m_transcodingDialog;
    /** @brief Set to true if widget just gained focus (means we have to update effect stack . */
    bool m_gainedFocus;
    /** @brief List of Clip Ids that want an audio thumb. */
    QStringList m_audioThumbsList;
    QString m_processingAudioThumb;
    QMutex m_audioThumbMutex;
    /** @brief This is a lock that ensures safety in case of concurrent access */
    mutable QReadWriteLock m_lock;
    /** @brief Total number of milliseconds to process for audio thumbnails */
    long m_audioDuration;
    /** @brief Total number of milliseconds already processed for audio thumbnails */
    long m_processedAudio;
    /** @brief Indicates whether audio thumbnail creation is running. */
    QFuture<void> m_audioThumbsThread;
    std::function<void(const QString &)> m_readyCallBack;
    int m_suggestedDuration{-1};
    QAction *addBinAction(const QString &name, const QString &text, const QIcon &icon, const QString &category = {});
    void setupAddClipAction(QMenu *addClipMenu, ClipType::ProducerType type, const QString &name, const QString &text, const QIcon &icon);
    /** @brief Get the QModelIndex value for an item in the Bin. */
    QModelIndex getIndexForId(const QString &id, bool folderWanted) const;
    void showSlideshowWidget(const std::shared_ptr<ProjectClip> &clip);
    void processAudioThumbs();
    void updateSortingAction(int ix);
    int wheelAccumulatedDelta;
    QString m_keyBindingMessage;
    QString m_clipsCountMessage;
    ClipZoneInfo m_activateClipZoneInfo;
    /** @brief Show the clip count and key binfing info in status bar. */
    void showBinInfo();
    /** @brief Find all clip Ids that have a specific tag. */
    const QList<QString> getAllClipsWithTag(const QString &tag);
    /** @brief Paste effect on a list of clips. */
    bool doPasteEffect(std::vector<QString> ids, const QStringList &effectData);

Q_SIGNALS:
    void itemUpdated(std::shared_ptr<AbstractProjectItem>);
    void producerReady(const QString &id);
    /** @brief Save folder info into MLT. */
    void storeFolder(const QString &folderId, const QString &parentId, const QString &oldParentId, const QString &folderName);
    void gotFilterJobResults(const QString &, int, int, stringMap, stringMap);
    void requestShowClipProperties(const std::shared_ptr<ProjectClip> &clip, bool forceRefresh = false);
    /** @brief Request that the given clip is displayed in the clip monitor */
    void requestClipShow(std::shared_ptr<ProjectClip>);
    void displayBinMessage(const QString &, KMessageWidget::MessageType);
    void requesteInvalidRemoval(const QString &, const QString &, const QString &);
    /** @brief Analysis data changed, refresh panel. */
    void updateAnalysisData(const QString &);
    void openClip(std::shared_ptr<ProjectClip> c, int in = -1, int out = -1, const QUuid sequenceUuid = QUuid(), int seekFrame = -1);
    /** @brief Fill context menu with occurrences of this clip in timeline. */
    void findInTimeline(const QString &, QList<int> ids = QList<int>());
    void clipNameChanged(int, const QString);
    /** @brief A clip was updated, request panel update. */
    void refreshPanel(const QString &id);
    /** @brief Upon selection, activate timeline target tracks. */
    void setupTargets(bool hasVideo, QMap<int, QString> audioStreams);
    void requestBinClose();
    /** @brief Update a timeline tab name on clip rename. */
    void updateTabName(const QUuid &, const QString &);
    void requestAddClipReset();
    /** @brief Some timeline sequence producers have been updated, refresh their occurrences. */
    void requestUpdateSequences(QMap<QUuid, std::pair<int, int>> seqs);
};
