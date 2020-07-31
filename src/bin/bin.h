/*
Copyright (C) 2012  Till Theato <root@ttill.de>
Copyright (C) 2014  Jean-Baptiste Mardelle <jb@kdenlive.org>
This file is part of Kdenlive. See www.kdenlive.org.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of
the License or (at your option) version 3 or any later version
accepted by the membership of KDE e.V. (or its successor approved
by the membership of KDE e.V.), which shall act as a proxy
defined in Section 14 of version 3 of the license.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef KDENLIVE_BIN_H
#define KDENLIVE_BIN_H

#include "abstractprojectitem.h"
#include "timecode.h"

#include <KMessageWidget>

#include <QDir>
#include <QDomElement>
#include <QFuture>
#include <QLineEdit>
#include <QListView>
#include <QMutex>
#include <QPushButton>
#include <QTreeView>
#include <QListWidget>
#include <QUrl>
#include <QWidget>
#include <QActionGroup>

class AbstractProjectItem;
class BinItemDelegate;
class BinListItemDelegate;
class ClipController;
class EffectStackModel;
class InvalidDialog;
class KdenliveDoc;
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

namespace Mlt {
class Producer;
}

class MyListView : public QListView
{
    Q_OBJECT
public:
    explicit MyListView(QWidget *parent = nullptr);

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void focusInEvent(QFocusEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
signals:
    void focusView();
    void updateDragMode(PlaylistState::ClipState type);
    void displayBinFrame(QModelIndex ix, int frame);
    void processDragEnd();
private:
    QPoint m_startPos;
    PlaylistState::ClipState m_dragType;
};

class MyTreeView : public QTreeView
{
    Q_OBJECT
    Q_PROPERTY(bool editing READ isEditing WRITE setEditing NOTIFY editingChanged)
public:
    explicit MyTreeView(QWidget *parent = nullptr);
    void setEditing(bool edit);

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void focusInEvent(QFocusEvent *event) override;

protected slots:
    void closeEditor(QWidget *editor, QAbstractItemDelegate::EndEditHint hint) override;
    void editorDestroyed(QObject *editor) override;

private:
    QPoint m_startPos;
    PlaylistState::ClipState m_dragType;
    bool m_editing;
    bool performDrag();
    bool isEditing() const;

signals:
    void focusView();
    void updateDragMode(PlaylistState::ClipState type);
    void displayBinFrame(QModelIndex ix, int frame);
    void processDragEnd();
    void selectCurrent();
    void editingChanged();
};

class SmallJobLabel : public QPushButton
{
    Q_OBJECT
public:
    explicit SmallJobLabel(QWidget *parent = nullptr);
    static const QString getStyleSheet(const QPalette &p);
    void setAction(QAction *action);

private:
    enum ItemRole { NameRole = Qt::UserRole, DurationRole, UsageRole };

    QTimeLine *m_timeLine;
    QAction *m_action{nullptr};
    QMutex m_locker;

public slots:
    void slotSetJobCount(int jobCount);

private slots:
    void slotTimeLineChanged(qreal value);
    void slotTimeLineFinished();
};

class LineEventEater : public QObject
{
    Q_OBJECT
public:
    explicit LineEventEater(QObject *parent = nullptr);

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;

signals:
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
    enum BinViewType { BinTreeView, BinIconView };

public:
    explicit Bin(std::shared_ptr<ProjectItemModel> model, QWidget *parent = nullptr);
    ~Bin() override;

    bool isLoading;

    /** @brief Sets the document for the bin and initialize some stuff  */
    void setDocument(KdenliveDoc *project);

    /** @brief Create a clip item from its xml description  */
    void createClip(const QDomElement &xml);

    /** @brief Used to notify the Model View that an item was updated */
    void emitItemUpdated(std::shared_ptr<AbstractProjectItem> item);

    /** @brief Set monitor associated with this bin (clipmonitor) */
    void setMonitor(Monitor *monitor);

    /** @brief Returns the clip monitor */
    Monitor *monitor();

    /** @brief Open a producer in the clip monitor */
    void openProducer(std::shared_ptr<ProjectClip> controller);
    void openProducer(std::shared_ptr<ProjectClip> controller, int in, int out);

    /** @brief Get a clip from it's id */
    std::shared_ptr<ProjectClip> getBinClip(const QString &id);
    /** @brief Get a clip's name from it's id */
    const QString getBinClipName(const QString &id) const;
    /** @brief Returns the duration of a given clip. */
    size_t getClipDuration(int itemId) const;
    /** @brief Returns the state of a given clip: AudioOnly, VideoOnly, Disabled (Disabled means it has audio and video capabilities */
    PlaylistState::ClipState getClipState(int itemId) const;

    /** @brief Add markers on clip @param binId at @param positions */ 
    void addClipMarker(const QString binId, QList<int> positions);

    /** @brief Returns a list of selected clip ids.
     *  @param allowSubClips: if true, will include subclip ids in the form: "master clip id/in/out"
     */
    std::vector<QString> selectedClipsIds(bool allowSubClips = false);

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
    void updateTargets(const QString &id);

    void doMoveClip(const QString &id, const QString &newParentId);
    void doMoveFolder(const QString &id, const QString &newParentId);
    void setupGeneratorMenu();

    /** @brief Set focus to the Bin view. */
    void focusBinView() const;
    /** @brief Get a string list of all clip ids that are inside a folder defined by id. */
    QStringList getBinFolderClipIds(const QString &id) const;
    /** @brief Build a rename subclip command. */
    void renameSubClipCommand(const QString &id, const QString &newName, const QString &oldName, int in, int out);
    /** @brief Rename a clip zone (subclip). */
    void renameSubClip(const QString &id, const QString &newName, int in, int out);
    /** @brief Returns current project's timecode. */
    Timecode projectTimecode() const;
    /** @brief Trigger timecode format refresh where needed. */
    void updateTimecodeFormat();
    /** @brief Edit an effect settings to a bin clip. */
    void editMasterEffect(const std::shared_ptr<AbstractProjectItem> &clip);
    /** @brief An effect setting was changed, update stack if displayed. */
    void updateMasterEffect(ClipController *ctl);
    void rebuildMenu();
    void refreshIcons();

    /** @brief This function change the global enabled state of the bin effects
     */
    void setBinEffectsEnabled(bool enabled);

    void requestAudioThumbs(const QString &id, long duration);
    /** @brief Proxy status for the project changed, update. */
    void refreshProxySettings();
    /** @brief A clip is ready, update its info panel if displayed. */
    void emitRefreshPanel(const QString &id);
    /** @brief Returns true if there is no clip. */
    bool isEmpty() const;
    /** @brief Trigger reload of all clips. */
    void reloadAllProducers(bool reloadThumbs = true);
    /** @brief Ensure all audio thumbs have been created */
    void checkAudioThumbs();
    /** @brief Get usage stats for project bin. */
    void getBinStats(uint *used, uint *unused, qint64 *usedSize, qint64 *unusedSize);
    /** @brief Returns the clip properties dockwidget. */
    QDockWidget *clipPropertiesDock();
    /** @brief Returns a document's cache dir. ok is set to false if folder does not exist */
    QDir getCacheDir(CacheType type, bool *ok) const;
    void rebuildProxies();
    /** @brief Return a list of all clips hashes used in this project */
    QStringList getProxyHashList();
    /** @brief Get binId of the current selected folder */
    QString getCurrentFolder();
    /** @brief Save a clip zone as MLT playlist */
    void saveZone(const QStringList &info, const QDir &dir);
    /** @brief A bin clip changed (its effects), invalidate preview */
    void invalidateClip(const QString &binId);
    /** @brief Recreate missing proxies on document opening */
    void checkMissingProxies();
    /** @brief Save folder state (expanded or not) */
    void saveFolderState();
    /** @brief Load folder state (expanded or not) */
    void loadFolderState(QStringList foldersToExpand);

    // TODO refac: remove this and call directly the function in ProjectItemModel
    void cleanup();
    void selectAll();

private slots:
    void slotAddClip();
    /** @brief Reload clip from disk */
    void slotReloadClip();
    /** @brief Replace clip with another file */
    void slotReplaceClip();
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

    /** @brief Update status for clip jobs  */
    void slotUpdateJobStatus(const QString &, int, int, const QString &label = QString(), const QString &actionName = QString(),
                             const QString &details = QString());
    void slotSetIconSize(int size);
    void selectProxyModel(const QModelIndex &id);
    void slotSaveHeaders();
    void slotItemDropped(const QStringList &ids, const QModelIndex &parent);
    void slotItemDropped(const QList<QUrl> &urls, const QModelIndex &parent);
    void slotEffectDropped(const QStringList &effectData, const QModelIndex &parent);
    void slotTagDropped(const QString &tag, const QModelIndex &parent);
    void slotItemEdited(const QModelIndex &, const QModelIndex &, const QVector<int> &);

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
    void showBinFrame(QModelIndex ix, int frame);
    /** @brief Switch a tag on/off on current selection
     */
    void switchTag(const QString &tag, bool add);
    /** @brief Update project tags
     */
    void updateTags(QMap <QString, QString> tags);
    void rebuildFilters(QMap <QString, QString> tags);
    /** @brief Switch a tag on  a clip list
     */
    void editTags(QList <QString> allClips, const QString &tag, bool add);

public slots:
    void slotRemoveInvalidClip(const QString &id, bool replace, const QString &errorMessage);
    /** @brief Reload clip thumbnail - when frame for thumbnail changed */
    void slotRefreshClipThumbnail(const QString &id);
    void slotDeleteClip();
    void slotItemDoubleClicked(const QModelIndex &ix, const QPoint &pos, uint modifiers);
    void slotSwitchClipProperties(const std::shared_ptr<ProjectClip> &clip);
    void slotSwitchClipProperties();
    /** @brief Creates a new folder with optional name, and returns new folder's id */
    void slotAddFolder();
    void slotCreateProjectClip();
    void slotEditClipCommand(const QString &id, const QMap<QString, QString> &oldProps, const QMap<QString, QString> &newProps);
    /** @brief Start a filter job requested by a filter applied in timeline */
    void slotStartFilterJob(const ItemInfo &info, const QString &id, QMap<QString, QString> &filterParams, QMap<QString, QString> &consumerParams,
                            QMap<QString, QString> &extraParams);
    /** @brief Open current clip in an external editing application */
    void slotOpenClip();
    void slotDuplicateClip();
    void slotLocateClip();
    /** @brief Add extra data to a clip. */
    void slotAddClipExtraData(const QString &id, const QString &key, const QString &data = QString(), QUndoCommand *groupCommand = nullptr);
    void slotUpdateClipProperties(const QString &id, const QMap<QString, QString> &properties, bool refreshPropertiesPanel);
    /** @brief Add effect to active Bin clip (used when double clicking an effect in list). */
    void slotAddEffect(QString id, const QStringList &effectData);
    void slotExpandUrl(const ItemInfo &info, const QString &url, QUndoCommand *command);
    /** @brief Abort all ongoing operations to prepare close. */
    void abortOperations();
    void doDisplayMessage(const QString &text, KMessageWidget::MessageType type, const QList<QAction *> &actions = QList<QAction *>(), bool showCloseButton = false, BinMessage::BinCategory messageCategory = BinMessage::BinCategory::NoMessage);
    void doDisplayMessage(const QString &text, KMessageWidget::MessageType type, const QString &logInfo);
    /** @brief Reset all clip usage to 0 */
    void resetUsageCount();
    /** @brief Select a clip in the Bin from its id. */
    void selectClipById(const QString &id, int frame = -1, const QPoint &zone = QPoint());
    void slotAddClipToProject(const QUrl &url);
    void droppedUrls(const QList<QUrl> &urls, const QString &folderInfo = QString());
    /** @brief Returns the effectstack of a given clip. */
    std::shared_ptr<EffectStackModel> getClipEffectStack(int itemId);
    /** @brief Adjust project profile to current clip. */
    void adjustProjectProfileToItem();
    /** @brief Check and propose auto adding audio tracks.
     * @param clipId The clip whose streams have to be checked
     * @param minimumTracksCount the number of active streams for this clip
     */
    void checkProjectAudioTracks(QString clipId, int minimumTracksCount);

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
    std::shared_ptr<ProjectItemModel> m_itemModel;
    QAbstractItemView *m_itemView;
    BinItemDelegate *m_binTreeViewDelegate;
    BinListItemDelegate *m_binListViewDelegate;
    std::unique_ptr<ProjectSortProxyModel> m_proxyModel;
    QToolBar *m_toolbar;
    KdenliveDoc *m_doc;
    QLineEdit *m_searchLine;
    QToolButton *m_addButton;
    QMenu *m_extractAudioAction;
    QMenu *m_transcodeAction;
    QMenu *m_clipsActionsMenu;
    QAction *m_inTimelineAction;
    QAction *m_showDate;
    QAction *m_showDesc;
    QAction *m_showRating;
    QAction *m_sortDescend;
    /** @brief Default view type (icon, tree, ...) */
    BinViewType m_listType;
    /** @brief Default icon size for the views. */
    QSize m_iconSize;
    /** @brief Keeps the column width info of the tree view. */
    QByteArray m_headerInfo;
    QVBoxLayout *m_layout;
    QDockWidget *m_propertiesDock;
    QScrollArea *m_propertiesPanel;
    QSlider *m_slider;
    Monitor *m_monitor;
    QIcon m_blankThumb;
    QMenu *m_menu;
    QAction *m_openAction;
    QAction *m_editAction;
    QAction *m_reloadAction;
    QAction *m_replaceAction;
    QAction *m_duplicateAction;
    QAction *m_locateAction;
    QAction *m_proxyAction;
    QAction *m_deleteAction;
    QAction *m_renameAction;
    QMenu *m_jobsMenu;
    QAction *m_cancelJobs;
    QAction *m_discardCurrentClipJobs;
    QAction *m_discardPendingJobs;
    QAction *m_upAction;
    QAction *m_tagAction;
    QActionGroup *m_sortGroup;
    SmallJobLabel *m_infoLabel;
    TagWidget *m_tagsWidget;
    QMenu *m_filterMenu;
    QActionGroup m_filterGroup;
    QActionGroup m_filterRateGroup;
    QActionGroup m_filterTypeGroup;
    QToolButton *m_filterButton;
    /** @brief The info widget for failed jobs. */
    KMessageWidget *m_infoMessage;
    BinMessage::BinCategory m_currentMessage;
    QStringList m_errorLog;
    InvalidDialog *m_invalidClipDialog;
    /** @brief Set to true if widget just gained focus (means we have to update effect stack . */
    bool m_gainedFocus;
    /** @brief List of Clip Ids that want an audio thumb. */
    QStringList m_audioThumbsList;
    QString m_processingAudioThumb;
    QMutex m_audioThumbMutex;
    /** @brief Total number of milliseconds to process for audio thumbnails */
    long m_audioDuration;
    /** @brief Total number of milliseconds already processed for audio thumbnails */
    long m_processedAudio;
    /** @brief Indicates whether audio thumbnail creation is running. */
    QFuture<void> m_audioThumbsThread;
    QAction *addAction(const QString &name, const QString &text, const QIcon &icon);
    void setupAddClipAction(QMenu *addClipMenu, ClipType::ProducerType type, const QString &name, const QString &text, const QIcon &icon);
    void showClipProperties(const std::shared_ptr<ProjectClip> &clip, bool forceRefresh = false);
    /** @brief Get the QModelIndex value for an item in the Bin. */
    QModelIndex getIndexForId(const QString &id, bool folderWanted) const;
    std::shared_ptr<ProjectClip> getFirstSelectedClip();
    void showTitleWidget(const std::shared_ptr<ProjectClip> &clip);
    void showSlideshowWidget(const std::shared_ptr<ProjectClip> &clip);
    void processAudioThumbs();
    void updateSortingAction(int ix);
    int wheelAccumulatedDelta;

signals:
    void itemUpdated(std::shared_ptr<AbstractProjectItem>);
    void producerReady(const QString &id);
    /** @brief Save folder info into MLT. */
    void storeFolder(const QString &folderId, const QString &parentId, const QString &oldParentId, const QString &folderName);
    void gotFilterJobResults(const QString &, int, int, stringMap, stringMap);
    /** @brief Trigger timecode format refresh where needed. */
    void refreshTimeCode();
    /** @brief Request display of effect stack for a Bin clip. */
    void requestShowEffectStack(const QString &clipName, std::shared_ptr<EffectStackModel>, QSize frameSize, bool showKeyframes);
    /** @brief Request that the given clip is displayed in the clip monitor */
    void requestClipShow(std::shared_ptr<ProjectClip>);
    void displayBinMessage(const QString &, KMessageWidget::MessageType);
    void requesteInvalidRemoval(const QString &, const QString &, const QString &);
    /** @brief Analysis data changed, refresh panel. */
    void updateAnalysisData(const QString &);
    void openClip(std::shared_ptr<ProjectClip> c, int in = -1, int out = -1);
    /** @brief Fill context menu with occurrences of this clip in timeline. */
    void findInTimeline(const QString &, QList<int> ids = QList<int>());
    void clipNameChanged(const QString &);
    /** @brief A clip was updated, request panel update. */
    void refreshPanel(const QString &id);
    /** @brief Upon selection, activate timeline target tracks. */
    void setupTargets(bool hasVideo, QMap <int, QString> audioStreams);
    /** @brief A drag event ended, inform timeline. */
    void processDragEnd();
    /** @brief Delete selected markers in clip properties dialog. */
    void deleteMarkers();
    /** @brief Selected all markers in clip properties dialog. */
    void selectMarkers();
};

#endif
