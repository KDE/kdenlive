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
#include "effects/effectstack/model/effectstackmodel.hpp"
#include "filewatcher.hpp"
#include "timecode.h"

#include <KMessageWidget>

#include <QApplication>
#include <QDir>
#include <QDomElement>
#include <QFuture>
#include <QLineEdit>
#include <QListView>
#include <QMutex>
#include <QPainter>
#include <QPushButton>
#include <QStyledItemDelegate>
#include <QUrl>
#include <QWidget>

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
class ProjectFolderUp;
class InvalidDialog;
class BinItemDelegate;
class BinMessageWidget;
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
    void focusInEvent(QFocusEvent *event) override;
signals:
    void focusView();
};

class MyTreeView : public QTreeView
{
    Q_OBJECT
    Q_PROPERTY(bool editing READ isEditing WRITE setEditing)
public:
    explicit MyTreeView(QWidget *parent = nullptr);
    void setEditing(bool edit);

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void focusInEvent(QFocusEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;

protected slots:
    void closeEditor(QWidget *editor, QAbstractItemDelegate::EndEditHint hint) override;
    void editorDestroyed(QObject *editor) override;

private:
    QPoint m_startPos;
    bool m_editing;
    bool performDrag();
    bool isEditing() const;

signals:
    void focusView();
};

class BinMessageWidget : public KMessageWidget
{
    Q_OBJECT
public:
    explicit BinMessageWidget(QWidget *parent = nullptr);
    BinMessageWidget(const QString &text, QWidget *parent = nullptr);

protected:
    bool event(QEvent *ev) override;

signals:
    void messageClosing();
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
    explicit Bin(const std::shared_ptr<ProjectItemModel> &model, QWidget *parent = nullptr);
    ~Bin();

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

    /** @brief Returns a list of selected clip ids
     @param excludeFolders: if true, ids of folders are not returned
     */
    std::vector<QString> selectedClipsIds(bool excludeFolders = true);

    // Returns the selected clips
    QList<std::shared_ptr<ProjectClip>> selectedClips();

    /** @brief Current producer has changed, refresh monitor and timeline*/
    void refreshClip(const QString &id);

    void setupMenu(QMenu *addMenu, QAction *defaultAction, const QHash<QString, QAction *> &actions);

    /** @brief The source file was modified, we will reload it soon, disable item in the meantime */
    void setWaitingStatus(const QString &id);

    const QString getDocumentProperty(const QString &key);

    /** @brief A proxy clip was just created, pass it to the responsible item  */
    void gotProxy(const QString &id, const QString &path);

    /** @brief Give a number available for a clip id, used when adding a new clip to the project. Id must be unique */
    int getFreeClipId();

    /** @brief Give a number available for a folder id, used when adding a new folder to the project. Id must be unique */
    int getFreeFolderId();

    /** @brief Returns the id of the last inserted clip */
    int lastClipId() const;

    /** @brief Ask MLT to reload this clip's producer  */
    void reloadClip(const QString &id);

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
    void renameSubClip(const QString &id, const QString &newName, const QString &oldName, int in, int out);
    /** @brief Returns current project's timecode. */
    Timecode projectTimecode() const;
    /** @brief Trigger timecode format refresh where needed. */
    void updateTimecodeFormat();
    /** @brief Edit an effect settings to a bin clip. */
    void editMasterEffect(std::shared_ptr<AbstractProjectItem> clip);
    /** @brief An effect setting was changed, update stack if displayed. */
    void updateMasterEffect(ClipController *ctl);
    /** @brief Display a message about an operation in status bar. */
    void emitMessage(const QString &, int, MessageType);
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
    void reloadAllProducers();
    /** @brief Get usage stats for project bin. */
    void getBinStats(uint *used, uint *unused, qint64 *usedSize, qint64 *unusedSize);
    /** @brief Returns the clip properties dockwidget. */
    QDockWidget *clipPropertiesDock();
    /** @brief Returns a document's cache dir. ok is set to false if folder does not exist */
    QDir getCacheDir(CacheType type, bool *ok) const;
    /** @brief Command adding a bin clip */
    bool addClip(QDomElement elem, const QString &clipId);
    void rebuildProxies();
    /** @brief Return a list of all clips hashes used in this project */
    QStringList getProxyHashList();
    /** @brief Get info (id, name) of a folder (or the currently selected one)  */
    const QStringList getFolderInfo(const QModelIndex &selectedIx = QModelIndex());
    /** @brief Get binId of the current selected folder */
    QString getCurrentFolder();
    /** @brief Save a clip zone as MLT playlist */
    void saveZone(const QStringList &info, const QDir &dir);
    void addWatchFile(const QString &binId, const QString &url);
    void removeWatchFile(const QString &binId, const QString &url);

    // TODO refac: remove this and call directly the function in ProjectItemModel
    void cleanup();
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
    void slotUpdateJobStatus(const QString &, int, int, const QString &label = QString(), const QString &actionName = QString(),
                             const QString &details = QString());
    void slotSetIconSize(int size);
    void selectProxyModel(const QModelIndex &id);
    void slotSaveHeaders();
    void slotItemDropped(const QStringList &ids, const QModelIndex &parent);
    void slotItemDropped(const QList<QUrl> &urls, const QModelIndex &parent);
    void slotEffectDropped(const QStringList &effectData, const QModelIndex &parent);
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
    void slotRemoveInvalidClip(const QString &id, bool replace, const QString &errorMessage);
    /** @brief Reload clip thumbnail - when frame for thumbnail changed */
    void slotRefreshClipThumbnail(const QString &id);
    void slotDeleteClip();
    void slotItemDoubleClicked(const QModelIndex &ix, const QPoint pos);
    void slotSwitchClipProperties(std::shared_ptr<ProjectClip> clip);
    void slotSwitchClipProperties();
    /** @brief Creates a new folder with optional name, and returns new folder's id */
    QString slotAddFolder(const QString &folderName = QString());
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
    /** @brief Pass some important properties to timeline track producers. */
    void updateTimelineProducers(const QString &id, const QMap<QString, QString> &passProperties);
    /** @brief Add effect to active Bin clip (used when double clicking an effect in list). */
    void slotAddEffect(QString id, const QStringList &effectData);
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
    void droppedUrls(const QList<QUrl> &urls, const QStringList &folderInfo = QStringList());
    /** @brief Returns the effectstack of a given clip. */
    std::shared_ptr<EffectStackModel> getClipEffectStack(int itemId);
    int getClipDuration(int itemId) const;

protected:
    /* This function is called whenever an item is selected to propagate signals
       (for ex request to show the clip in the monitor)
    */
    void setCurrent(std::shared_ptr<AbstractProjectItem> item);
    void selectClip(const std::shared_ptr<ProjectClip> &clip);
    void contextMenuEvent(QContextMenuEvent *event) override;
    bool eventFilter(QObject *obj, QEvent *event) override;

private:
    std::shared_ptr<ProjectItemModel> m_itemModel;
    QAbstractItemView *m_itemView;
    /** @brief An "Up" item that is inserted in bin when using icon view so that user can navigate up */
    std::shared_ptr<ProjectFolderUp> m_folderUp;
    BinItemDelegate *m_binTreeViewDelegate;
    ProjectSortProxyModel *m_proxyModel;
    QToolBar *m_toolbar;
    KdenliveDoc *m_doc;
    FileWatcher m_fileWatcher;
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
    void showClipProperties(std::shared_ptr<ProjectClip> clip, bool forceRefresh = false);
    /** @brief Get the QModelIndex value for an item in the Bin. */
    QModelIndex getIndexForId(const QString &id, bool folderWanted) const;
    std::shared_ptr<ProjectClip> getFirstSelectedClip();
    void showTitleWidget(std::shared_ptr<ProjectClip> clip);
    void showSlideshowWidget(std::shared_ptr<ProjectClip> clip);
    void processAudioThumbs();

signals:
    void itemUpdated(std::shared_ptr<AbstractProjectItem>);
    void producerReady(const QString &id);
    /** @brief Save folder info into MLT. */
    void storeFolder(const QString &folderId, const QString &parentId, const QString &oldParentId, const QString &folderName);
    void gotFilterJobResults(const QString &, int, int, stringMap, stringMap);
    /** @brief Trigger timecode format refresh where needed. */
    void refreshTimeCode();
    /** @brief Request display of effect stack for a Bin clip. */
    void requestShowEffectStack(const QString &clipName, std::shared_ptr<EffectStackModel>, QPair<int, int> range, QSize frameSize, bool showKeyframes);
    /** @brief Request that the given clip is displayed in the clip monitor */
    void requestClipShow(std::shared_ptr<ProjectClip>);
    void displayBinMessage(const QString &, KMessageWidget::MessageType);
    void displayMessage(const QString &, int, MessageType);
    void requesteInvalidRemoval(const QString &, const QString &, const QString &);
    /** @brief Analysis data changed, refresh panel. */
    void updateAnalysisData(const QString &);
    void openClip(std::shared_ptr<ProjectClip> c, int in = -1, int out = -1);
    /** @brief Fill context menu with occurrences of this clip in timeline. */
    void findInTimeline(const QString &, QList<int> ids = QList<int>());
    void clipNameChanged(const QString &);
    /** @brief A clip was updated, request panel update. */
    void refreshPanel(const QString &id);
};

#endif
