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

#include <QWidget>
#include <QApplication>
#include <QStyledItemDelegate>
#include <QPainter>
#include <QDomElement>
#include <QPushButton>
#include <QUrl>
#include <QListView>
#include <QFuture>
#include <QMutex>
#include <QLineEdit>
#include <QDir>

class KdenliveDoc;
class QVBoxLayout;
class QScrollArea;
class ClipController;
class QDockWidget;
class QTimeLine;
class QToolBar;
class QMenu;
class QToolButton;
class QUndoCommand;
class ProjectItemModel;
class ProjectClip;
class ProjectFolder;
class AbstractProjectItem;
class Monitor;
class ProjectSortProxyModel;
class JobManager;
class ProjectFolderUp;
class InvalidDialog;
class BinItemDelegate;
class BinMessageWidget;
class SmallJobLabel;

namespace Mlt
{
class Producer;
}

class MyListView: public QListView
{
    Q_OBJECT
public:
    explicit MyListView(QWidget *parent = nullptr);

protected:
    void focusInEvent(QFocusEvent *event) Q_DECL_OVERRIDE;
signals:
    void focusView();
};

class MyTreeView: public QTreeView
{
    Q_OBJECT
    Q_PROPERTY(bool editing READ isEditing WRITE setEditing)
public:
    explicit MyTreeView(QWidget *parent = nullptr);
    void setEditing(bool edit);
protected:
    void mousePressEvent(QMouseEvent *event) Q_DECL_OVERRIDE;
    void mouseMoveEvent(QMouseEvent *event) Q_DECL_OVERRIDE;
    void focusInEvent(QFocusEvent *event) Q_DECL_OVERRIDE;
    void keyPressEvent(QKeyEvent *event) Q_DECL_OVERRIDE;

protected slots:
    void closeEditor(QWidget *editor, QAbstractItemDelegate::EndEditHint hint) Q_DECL_OVERRIDE;
    void editorDestroyed(QObject *editor) Q_DECL_OVERRIDE;

private:
    QPoint m_startPos;
    bool m_editing;
    bool performDrag();
    bool isEditing() const;

signals:
    void focusView();
};

class BinMessageWidget: public KMessageWidget
{
    Q_OBJECT
public:
    explicit BinMessageWidget(QWidget *parent = nullptr);
    BinMessageWidget(const QString &text, QWidget *parent = nullptr);

protected:
    bool event(QEvent *ev) Q_DECL_OVERRIDE;

signals:
    void messageClosing();
};

class SmallJobLabel: public QPushButton
{
    Q_OBJECT
public:
    explicit SmallJobLabel(QWidget *parent = nullptr);
    static const QString getStyleSheet(const QPalette &p);
    void setAction(QAction *action);
private:
    enum ItemRole {
        NameRole = Qt::UserRole,
        DurationRole,
        UsageRole
    };

    QTimeLine *m_timeLine;
    QAction *m_action;

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
    bool eventFilter(QObject *obj, QEvent *event) Q_DECL_OVERRIDE;

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
    enum BinViewType {BinTreeView, BinIconView };

public:
    explicit Bin(QWidget *parent = nullptr);
    ~Bin();

    bool isLoading;

    /** @brief Sets the document for the bin and initialize some stuff  */
    void setDocument(KdenliveDoc *project);

    /** @brief Returns the root folder, which is the parent for all items in the view */
    ProjectFolder *rootFolder();

    /** @brief Create a clip item from its xml description  */
    void createClip(const QDomElement &xml);

    /** @brief Used to notify the Model View that an item was updated */
    void emitItemUpdated(AbstractProjectItem *item);

    /** @brief Set monitor associated with this bin (clipmonitor) */
    void setMonitor(Monitor *monitor);

    /** @brief Returns the clip monitor */
    Monitor *monitor();

    /** @brief Open a producer in the clip monitor */
    void openProducer(ClipController *controller);
    void openProducer(ClipController *controller, int in, int out);

    /** @brief Trigger deletion of an item */
    void deleteClip(const QString &id);

    /** @brief Get a clip from it's id */
    ProjectClip *getBinClip(const QString &id);

    /** @brief Returns a list of selected clips  */
    QList<ProjectClip *> selectedClips();

    /** @brief Start a job of selected type for a clip  */
    void startJob(const QString &id, AbstractClipJob::JOBTYPE type);

    /** @brief Discard jobs from a chosen type, use NOJOBTYPE to discard all jobs for this clip */
    void discardJobs(const QString &id, AbstractClipJob::JOBTYPE type = AbstractClipJob::NOJOBTYPE);

    /** @brief Check if there is a job waiting / running for this clip  */
    bool hasPendingJob(const QString &id, AbstractClipJob::JOBTYPE type);

    /** @brief Reload / replace a producer */
    void reloadProducer(const QString &id, const QDomElement &xml);

    /** @brief Current producer has changed, refresh monitor and timeline*/
    void refreshClip(const QString &id);

    /** @brief Some stuff used to notify the Item Model */
    void emitAboutToAddItem(AbstractProjectItem *item);
    void emitItemAdded(AbstractProjectItem *item);
    void emitAboutToRemoveItem(AbstractProjectItem *item);
    void emitItemRemoved(AbstractProjectItem *item);
    void setupMenu(QMenu *addMenu, QAction *defaultAction, const QHash<QString, QAction *> &actions);

    /** @brief The source file was modified, we will reload it soon, disable item in the meantime */
    void setWaitingStatus(const QString &id);

    const QString getDocumentProperty(const QString &key);

    /** @brief A proxy clip was just created, pass it to the responsible item  */
    void gotProxy(const QString &id, const QString &path);

    /** @brief Get the document's renderer frame size  */
    const QSize getRenderSize();

    /** @brief Give a number available for a clip id, used when adding a new clip to the project. Id must be unique */
    int getFreeClipId();

    /** @brief Give a number available for a folder id, used when adding a new folder to the project. Id must be unique */
    int getFreeFolderId();

    /** @brief Returns the id of the last inserted clip */
    int lastClipId() const;

    /** @brief Ask MLT to reload this clip's producer  */
    void reloadClip(const QString &id);

    /** @brief Delete a folder  */
    void doRemoveFolder(const QString &id);
    /** @brief Add a folder  */
    void doAddFolder(const QString &id, const QString &name, const QString &parentId);
    void removeFolder(const QString &id, QUndoCommand *deleteCommand);
    void removeSubClip(const QString &id, QUndoCommand *deleteCommand);
    void doMoveClip(const QString &id, const QString &newParentId);
    void doMoveFolder(const QString &id, const QString &newParentId);
    void setupGeneratorMenu();
    void startClipJob(const QStringList &params);

    void addClipCut(const QString &id, int in, int out);
    void removeClipCut(const QString &id, int in, int out);

    /** @brief Create the subclips defined in the parent clip. */
    void loadSubClips(const QString &id, const QMap<QString, QString> &data);

    /** @brief Set focus to the Bin view. */
    void focusBinView() const;
    /** @brief Get a string list of all clip ids that are inside a folder defined by id. */
    QStringList getBinFolderClipIds(const QString &id) const;
    /** @brief Build a rename folder command. */
    void renameFolderCommand(const QString &id, const QString &newName, const QString &oldName);
    /** @brief Rename a folder and store new name in MLT. */
    void renameFolder(const QString &id, const QString &name);
    /** @brief Build a rename subclip command. */
    void renameSubClipCommand(const QString &id, const QString &newName, const QString &oldName, int in, int out);
    /** @brief Rename a clip zone (subclip). */
    void renameSubClip(const QString &id, const QString &newName, const QString &oldName, int in, int out);
    /** @brief Returns current project's timecode. */
    Timecode projectTimecode() const;
    /** @brief Trigger timecode format refresh where needed. */
    void updateTimecodeFormat();
    /** @brief If clip monitor is displaying clip with id @param id, refresh markers. */
    void refreshClipMarkers(const QString &id);
    /** @brief Delete a clip marker. */
    void deleteClipMarker(const QString &comment, const QString &id, const GenTime &position);
    /** @brief Delete all markers from @param id clip. */
    void deleteAllClipMarkers(const QString &id);
    /** @brief Remove an effect from a bin clip. */
    void removeEffect(const QString &id, const QDomElement &effect);
    void moveEffect(const QString &id, const QList<int> &oldPos, const QList<int> &newPos);
    /** @brief Add an effect to a bin clip. */
    void addEffect(const QString &id, QDomElement &effect);
    /** @brief Update a bin clip effect. */
    void updateEffect(const QString &id, QDomElement &effect, int ix, bool refreshStackWidget);
    void changeEffectState(const QString &id, const QList<int> &indexes, bool disable, bool refreshStack);
    /** @brief Edit an effect settings to a bin clip. */
    void editMasterEffect(ClipController *ctl);
    /** @brief An effect setting was changed, update stack if displayed. */
    void updateMasterEffect(ClipController *ctl);
    /** @brief Display a message about an operation in status bar. */
    void emitMessage(const QString &, int, MessageType);
    void rebuildMenu();
    void refreshIcons();
    /** @brief Update status of disable effects action (when loading a document). */
    void setBinEffectsDisabledStatus(bool disabled);

    void requestAudioThumbs(const QString &id, long duration);
    /** @brief Proxy status for the project changed, update. */
    void refreshProxySettings();
    /** @brief A clip is ready, update its info panel if displayed. */
    void emitRefreshPanel(const QString &id);
    /** @brief Audio thumbs just finished creating, update on monitor display. */
    void emitRefreshAudioThumbs(const QString &id);
    /** @brief Returns true if there is no clip. */
    bool isEmpty() const;
    /** @brief Trigger reload of all clips. */
    void reloadAllProducers();
    /** @brief Remove all unused clip from project bin. */
    void cleanup();
    /** @brief Get usage stats for project bin. */
    void getBinStats(uint *used, uint *unused, qint64 *usedSize, qint64 *unusedSize);
    /** @brief Returns the clip properties dockwidget. */
    QDockWidget *clipPropertiesDock();
    /** @brief Returns a cached thumbnail. */
    QImage findCachedPixmap(const QString &path);
    void cachePixmap(const QString &path, const QImage &img);
    /** @brief Returns a document's cache dir. ok is set to false if folder does not exist */
    QDir getCacheDir(CacheType type, bool *ok) const;
    /** @brief Command adding a bin clip */
    bool addClip(QDomElement elem, const QString &clipId);
    void rebuildProxies();
    /** @brief Return a list of all clips hashes used in this project */
    QStringList getProxyHashList();
    /** @brief Get info (id, name) of a folder (or the currently selected one)  */
    const QStringList getFolderInfo(const QModelIndex &selectedIx = QModelIndex());
    /** @brief Save a clip zone as MLT playlist */
    void saveZone(const QStringList &info, const QDir &dir);

private slots:
    void slotAddClip();
    void slotReloadClip();
    /** @brief Set sorting column */
    void slotSetSorting();
    /** @brief Show/hide date column */
    void slotShowDateColumn(bool show);
    void slotShowDescColumn(bool show);

    /** @brief Setup the bin view type (icon view, tree view, ...).
    * @param action The action whose data defines the view type or nullptr to keep default view */
    void slotInitView(QAction *action);

    /** @brief Update status for clip jobs  */
    void slotUpdateJobStatus(const QString &, int, int, const QString &label = QString(), const QString &actionName = QString(), const QString &details = QString());
    void slotSetIconSize(int size);
    void rowsInserted(const QModelIndex &parent, int start, int end);
    void rowsRemoved(const QModelIndex &parent, int start, int end);
    void selectProxyModel(const QModelIndex &id);
    void autoSelect();
    void slotSaveHeaders();
    void slotItemDropped(const QStringList &ids, const QModelIndex &parent);
    void slotItemDropped(const QList<QUrl> &urls, const QModelIndex &parent);
    void slotEffectDropped(const QString &effect, const QModelIndex &parent);
    void slotUpdateEffect(QString id, QDomElement oldEffect, QDomElement newEffect, int ix, bool refreshStack = false);
    void slotChangeEffectState(QString id, const QList<int> &indexes, bool disable);
    void slotItemEdited(const QModelIndex &, const QModelIndex &, const QVector<int> &);
    void slotAddUrl(const QString &url, int folderId, const QMap<QString, QString> &data = QMap<QString, QString>());
    void slotAddUrl(const QString &url, const QMap<QString, QString> &data = QMap<QString, QString>());

    void slotPrepareJobsMenu();
    void slotShowJobLog();
    /** @brief process clip job result. */
    void slotGotFilterJobResults(const QString &, int, int, const stringMap &, const stringMap &);
    /** @brief Reset all text and log data from info message widget. */
    void slotResetInfoMessage();
    /** @brief Show dialog prompting for removal of invalid clips. */
    void slotQueryRemoval(const QString &id, const QString &url, const QString &errorMessage);
    /** @brief Request display of current clip in monitor. */
    void slotOpenCurrent();
    void slotZoomView(bool zoomIn);
    /** @brief Widget gained focus, make sure we display effects for master clip. */
    void slotGotFocus();
    /** @brief Dis/Enable all bin effects. */
    void slotDisableEffects(bool disable);
    /** @brief Rename a Bin Item. */
    void slotRenameItem();
    void slotCreateAudioThumbs();
    void doRefreshPanel(const QString &id);
    /** @brief Send audio thumb data to monitor for display. */
    void slotSendAudioThumb(const QString &id);
    void doRefreshAudioThumbs(const QString &id);
    /** @brief Enable item view and hide message */
    void slotMessageActionTriggered();
    /** @brief Request editing of title or slideshow clip */
    void slotEditClip();
    /** @brief Enable / disable clear button on search line
     *  this is a workaround foq Qt bug 54676
     */
    void showClearButton(bool show);

public slots:
    void slotThumbnailReady(const QString &id, const QImage &img, bool fromFile = false);
    /** @brief The producer for this clip is ready.
     *  @param id the clip id
     *  @param controller The Controller for this clip
     */
    void slotProducerReady(const requestClipInfo &info, ClipController *controller);
    void slotRemoveInvalidClip(const QString &id, bool replace, const QString &errorMessage);
    /** @brief Create a folder when opening a document */
    void slotLoadFolders(const QMap<QString, QString> &foldersData);
    /** @brief Reload clip thumbnail - when frame for thumbnail changed */
    void slotRefreshClipThumbnail(const QString &id);
    void slotDeleteClip();
    void slotItemDoubleClicked(const QModelIndex &ix, const QPoint pos);
    void slotSwitchClipProperties(ProjectClip *clip);
    void slotSwitchClipProperties();
    /** @brief Creates a new folder with optional name, and returns new folder's id */
    QString slotAddFolder(const QString &folderName = QString());
    void slotCreateProjectClip();
    /** @brief Start a Cut Clip job on this clip (extract selected zone using FFmpeg) */
    void slotStartCutJob(const QString &id);
    /** @brief Triggered by a clip job action, start the job */
    void slotStartClipJob(bool enable);
    void slotEditClipCommand(const QString &id, const QMap<QString, QString> &oldProps, const QMap<QString, QString> &newProps);
    void slotCancelRunningJob(const QString &id, const QMap<QString, QString> &newProps);
    /** @brief Start a filter job requested by a filter applied in timeline */
    void slotStartFilterJob(const ItemInfo &info, const QString &id, QMap<QString, QString> &filterParams, QMap<QString, QString> &consumerParams, QMap<QString, QString> &extraParams);
    /** @brief Add a sub clip */
    void slotAddClipCut(const QString &id, int in, int out);
    /** @brief Open current clip in an external editing application */
    void slotOpenClip();
    void slotAddClipMarker(const QString &id, const QList<CommentedTime> &newMarker, QUndoCommand *groupCommand = nullptr);
    void slotLoadClipMarkers(const QString &id);
    void slotSaveClipMarkers(const QString &id);
    void slotDuplicateClip();
    void slotLocateClip();
    void slotDeleteEffect(const QString &id, QDomElement effect);
    void slotMoveEffect(const QString &id, const QList<int> &currentPos, int newPos);
    /** @brief Request audio thumbnail for clip with id */
    void slotCreateAudioThumb(const QString &id);
    /** @brief Abort audio thumbnail for clip with id */
    void slotAbortAudioThumb(const QString &id, long duration);
    /** @brief Add extra data to a clip. */
    void slotAddClipExtraData(const QString &id, const QString &key, const QString &data = QString(), QUndoCommand *groupCommand = nullptr);
    void slotUpdateClipProperties(const QString &id, const QMap<QString, QString> &properties, bool refreshPropertiesPanel);
    /** @brief Pass some important properties to timeline track producers. */
    void updateTimelineProducers(const QString &id, const QMap<QString, QString> &passProperties);
    /** @brief Add effect to active Bin clip (used when double clicking an effect in list). */
    void slotEffectDropped(QString id, QDomElement);
    /** @brief Request current frame from project monitor. 
     *  @param clipId is the id of a clip we want to hide from screenshot
     *  @param request true to start capture process, false to end it. It is necessary to emit a false after image is received
    **/
    void slotGetCurrentProjectImage(const QString &clipId, bool request);
    void slotExpandUrl(const ItemInfo &info, const QString &url, QUndoCommand *command);
    void abortAudioThumbs();
    /** @brief Abort all ongoing operations to prepare close. */
    void abortOperations();
    void doDisplayMessage(const QString &text, KMessageWidget::MessageType type, const QList<QAction *> &actions = QList<QAction *>());
    /** @brief Reset all clip usage to 0 */
    void resetUsageCount();
    /** @brief Select a clip in the Bin from its id. */
    void selectClipById(const QString &id, int frame = -1, const QPoint &zone = QPoint());
    void slotAddClipToProject(const QUrl &url);
    void doUpdateThumbsProgress(long ms);
    void droppedUrls(const QList<QUrl> &urls, const QStringList &folderInfo = QStringList());

protected:
    void contextMenuEvent(QContextMenuEvent *event) Q_DECL_OVERRIDE;
    bool eventFilter(QObject *obj, QEvent *event) Q_DECL_OVERRIDE;

private:
    ProjectItemModel *m_itemModel;
    QAbstractItemView *m_itemView;
    ProjectFolder *m_rootFolder;
    /** @brief An "Up" item that is inserted in bin when using icon view so that user can navigate up */
    ProjectFolderUp *m_folderUp;
    BinItemDelegate *m_binTreeViewDelegate;
    ProjectSortProxyModel *m_proxyModel;
    JobManager *m_jobManager;
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
    /** @brief Holds an available unique id for a clip to be created */
    int m_clipCounter;
    /** @brief Holds an available unique id for a folder to be created */
    int m_folderCounter;
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
    QAction *m_duplicateAction;
    QAction *m_locateAction;
    QAction *m_proxyAction;
    QAction *m_deleteAction;
    QAction *m_renameAction;
    QMenu *m_jobsMenu;
    QAction *m_cancelJobs;
    QAction *m_discardCurrentClipJobs;
    QAction *m_discardPendingJobs;
    SmallJobLabel *m_infoLabel;
    /** @brief The info widget for failed jobs. */
    BinMessageWidget *m_infoMessage;
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
    void showClipProperties(ProjectClip *clip, bool forceRefresh = false);
    /** @brief Get the QModelIndex value for an item in the Bin. */
    QModelIndex getIndexForId(const QString &id, bool folderWanted) const;
    /** @brief Get a Clip item from its id. */
    AbstractProjectItem *getClipForId(const QString &id) const;
    ProjectClip *getFirstSelectedClip();
    void showTitleWidget(ProjectClip *clip);
    void showSlideshowWidget(ProjectClip *clip);
    void processAudioThumbs();

signals:
    void itemUpdated(AbstractProjectItem *);
    void producerReady(const QString &id);
    /** @brief Save folder info into MLT. */
    void storeFolder(const QString &folderId, const QString &parentId, const QString &oldParentId, const QString &folderName);
    void gotFilterJobResults(const QString &, int, int, stringMap, stringMap);
    /** @brief The clip was changed and thumbnail needs a refresh. */
    void clipNeedsReload(const QString &, bool);
    /** @brief Trigger timecode format refresh where needed. */
    void refreshTimeCode();
    /** @brief Request display of effect stack for a Bin clip. */
    void masterClipSelected(ClipController *, Monitor *);
    /** @brief Request updating of the effect stack if currently displayed. */
    void masterClipUpdated(ClipController *, Monitor *);
    void displayBinMessage(const QString &, KMessageWidget::MessageType);
    void displayMessage(const QString &, int, MessageType);
    void requesteInvalidRemoval(const QString &, const QString &, const QString &);
    /** @brief Markers changed, refresh panel. */
    void refreshPanelMarkers();
    /** @brief Analysis data changed, refresh panel. */
    void updateAnalysisData(const QString &);
    void openClip(ClipController *c, int in = -1, int out = -1);
    /** @brief Fill context menu with occurrences of this clip in timeline. */
    void findInTimeline(const QString &);
    void clipNameChanged(const QString &);
    /** @brief A clip was updated, request panel update. */
    void refreshPanel(const QString &id);
    /** @brief A clip audio data was updated, request refresh. */
    void refreshAudioThumbs(const QString &id);

};

#endif
