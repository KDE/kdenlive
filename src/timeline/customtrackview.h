/***************************************************************************
 *   Copyright (C) 2007 by Jean-Baptiste Mardelle (jb@kdenlive.org)        *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA          *
 ***************************************************************************/

#ifndef CUSTOMTRACKVIEW_H
#define CUSTOMTRACKVIEW_H

#include <KColorScheme>

#include <QGraphicsView>
#include <QGraphicsItemAnimation>
#include <QTimeLine>
#include <QMenu>
#include <QMutex>
#include <QWaitCondition>

#include "doc/kdenlivedoc.h"
#include "timeline/guide.h"
#include "effectslist/effectslist.h"
#include "timeline/customtrackscene.h"
#include "timeline/managers/abstracttoolmanager.h"

class Timeline;
class ClipController;
class ClipItem;
class AbstractClipItem;
class AbstractGroupItem;
class Transition;
class AudioCorrelation;
class KSelectAction;

class CustomTrackView : public QGraphicsView
{
    Q_OBJECT

public:
    CustomTrackView(KdenliveDoc *doc, Timeline *timeline, CustomTrackScene *projectscene, QWidget *parent = nullptr);
    virtual ~ CustomTrackView();

    void mousePressEvent(QMouseEvent *event) Q_DECL_OVERRIDE;
    void mouseReleaseEvent(QMouseEvent *event) Q_DECL_OVERRIDE;
    void mouseMoveEvent(QMouseEvent *event) Q_DECL_OVERRIDE;
    void mouseDoubleClickEvent(QMouseEvent *event) Q_DECL_OVERRIDE;
    void addTrack(const TrackInfo &type, int ix = -1);
    void removeTrack(int ix);
    /** @brief Makes the document use new track info (name, type, ...). */
    void configTracks(const QList<TrackInfo> &trackInfos);
    int cursorPos() const;
    void checkAutoScroll();
    /**
      Move the clip at \c start to \c end.

      If \c out_actualEnd is not nullptr, it will be set to the position the clip really ended up at.
      For example, attempting to move a clip to t = -1 s will actually move it to t = 0 s.
      */
    bool moveClip(const ItemInfo &start, const ItemInfo &end, bool refresh, bool alreadyMoved, ItemInfo *out_actualEnd = nullptr);
    void moveGroup(QList<ItemInfo> startClip, QList<ItemInfo> startTransition, const GenTime &offset, const int trackOffset, bool alreadyMoved, bool reverseMove);
    /** move transition, startPos = (old start, old end), endPos = (new start, new end) */
    void moveTransition(const ItemInfo &start, const ItemInfo &end, bool refresh);
    void resizeClip(const ItemInfo &start, const ItemInfo &end, bool dontWorry = false);
    void addClip(const QString &clipId, const ItemInfo &info, const EffectsList &list, PlaylistState::ClipState state, bool refresh = true);
    void deleteClip(const ItemInfo &info, bool refresh = true);
    void addMarker(const QString &id, const CommentedTime &marker);
    void addData(const QString &id, const QString &key, const QString &data);
    void setScale(double scaleFactor, double verticalScale, bool zoomOnMouse = false);
    void deleteClip(const QString &clipId, QUndoCommand *deleteCommand);
    /** @brief An effect was dropped on @param clip */
    void slotDropEffect(ClipItem *clip, const QDomElement &effect, GenTime pos, int track);
    /** @brief A transition was dropped on @param clip */
    void slotDropTransition(ClipItem *clip, const QDomElement &transition, QPointF scenePos);
    /** @brief Add effect to current clip */
    void slotAddEffectToCurrentItem(const QDomElement &effect);
    /** @brief Add effect to a clip or selection */
    void slotAddEffect(const QDomElement &effect, const GenTime &pos, int track);
    void slotAddGroupEffect(const QDomElement &effect, AbstractGroupItem *group, AbstractClipItem *dropTarget = nullptr);
    void addEffect(int track, GenTime pos, const QDomElement &effect);
    void deleteEffect(int track, const GenTime &pos, const QDomElement &effect);
    void updateEffect(int track, GenTime pos, const QDomElement &insertedEffect, bool refreshEffectStack = false, bool replaceEffect = false, bool refreshMonitor = true);
    /** @brief Enable / disable a list of effects */
    void updateEffectState(int track, GenTime pos, const QList<int> &effectIndexes, bool disable, bool updateEffectStack);
    void moveEffect(int track, const GenTime &pos, const QList<int> &oldPos, const QList<int> &newPos);
    void addTransition(const ItemInfo &transitionInfo, int endTrack, const QDomElement &params, bool refresh);
    void deleteTransition(const ItemInfo &transitionInfo, int endTrack, const QDomElement &params, bool refresh);
    void updateTransition(int track, const GenTime &pos,  const QDomElement &oldTransition, const QDomElement &transition, bool updateTransitionWidget);
    void activateMonitor();
    int duration() const;
    void deleteSelectedClips();
    /** @brief Cuts all clips that are selected at the timeline cursor position. */
    void cutSelectedClips(QList<QGraphicsItem *> itemList = QList<QGraphicsItem *>(), GenTime currentPos = GenTime());
    void setContextMenu(QMenu *timeline, QMenu *clip, QMenu *transition, QActionGroup *clipTypeGroup, QMenu *markermenu);
    bool checkTrackHeight(bool force = false);
    void updateSceneFrameWidth(double fpsChanged = 1.0);
    void setTool(ProjectTool tool);
    void cutClip(const ItemInfo &info, const GenTime &cutTime, bool cut, const EffectsList &oldStack = EffectsList(), bool execute = true);
    Transition *cutTransition(const ItemInfo &info, const GenTime &cutTime, bool cut, const QDomElement &oldStack = QDomElement(), bool execute = true);
    void slotSeekToPreviousSnap();
    void slotSeekToNextSnap();
    double getSnapPointForPos(double pos);
    bool findString(const QString &text);
    void selectFound(const QString &track, const QString &pos);
    bool findNextString(const QString &text);
    void initSearchStrings();
    void clearSearchStrings();
    QList<ItemInfo> findId(const QString &clipId);
    void clipStart();
    void clipEnd();
    void doChangeClipSpeed(const ItemInfo &info, const ItemInfo &speedIndependantInfo, PlaylistState::ClipState state, const double speed, int strobe, const QString &id, bool removeEffect = false);
    /** @brief Every command added to the undo stack automatically triggers a document change event.
     *  This function should only be called when changing a document setting or another function that
     *  is not integrated in the undo / redo system */
    void setDocumentModified();
    void setInPoint();
    void setOutPoint();

    /** @brief Prepares inserting space.
    *
    * Shows a dialog to configure length and track. */
    void slotInsertSpace();
    /** @brief Prepares removing space. */
    void slotRemoveSpace(bool multiTrack = false);
    void insertSpace(const QList<ItemInfo> &clipsToMove, const QList<ItemInfo> &transToMove, int track, const GenTime &duration, const GenTime &offset);
    ClipItem *getActiveClipUnderCursor(bool allowOutsideCursor = false) const;
    void deleteTimelineTrack(int ix, const TrackInfo &trackinfo);
    void saveThumbnails();
    void autoTransition();
    void initCursorPos(int pos);

    /** @brief Locks or unlocks a track.
    * @param ix number of track
    * @param lock whether to lock or unlock
    * @param requestUpdate (default = true) Whether to request an update of the icon in the track header
    *
    * Makes sure no clip on track to lock is selected. */
    void lockTrack(int ix, bool lock, bool requestUpdate = true);
    void groupClips(bool group = true, QList<QGraphicsItem *> itemList = QList<QGraphicsItem *>(), bool forceLock = false, QUndoCommand *command = nullptr, bool doIt = true);
    void doGroupClips(const QList<ItemInfo> &clipInfos, const QList<ItemInfo> &transitionInfos, bool group);
    void loadGroups(const QDomNodeList &groups);

    /** @brief Creates SplitAudioCommands for selected clips. */
    void splitAudio(bool warn = true, const ItemInfo &info = ItemInfo(), int destTrack = -1, QUndoCommand *masterCommand = nullptr);

    /// Define which clip to take as reference for automatic audio alignment
    void setAudioAlignReference();

    /// Automatically align the currently selected clips to synchronize their audio with the reference's audio
    void alignAudio();

    /** @brief Separates the audio of a clip to a audio track.
    * @param pos Position of the clip to split
    * @param track Track of the clip
    * @param split Split or unsplit
    * @return true if the split was successful  */
    bool doSplitAudio(const GenTime &pos, int track, int destTrack, bool split);
    /** @brief Sets the clip type (av, video only, audio only) of the current selection. */
    void setClipType(PlaylistState::ClipState state);
    void doChangeClipType(const ItemInfo &info, PlaylistState::ClipState state);
    /** @brief Check if there is a guide at position.
     * @param pos Position to check
     * @param framePos If set to true, pos is an exact frame number, otherwise it's a mouse event pos
     */
    int hasGuide(double pos, bool framePos);
    void reloadTransitionLumas();
    void updateProjectFps();
    double fps() const;
    int selectedTrack() const;
    QStringList selectedClips() const;
    /** @brief Checks whether an item can be inserted (make sure it does not overlap another item) */
    bool canBePastedTo(const ItemInfo &info, int type, const QList<AbstractClipItem *> &excluded = QList<AbstractClipItem *>()) const;

    /** @brief Selects a clip.
    * @param add Whether to select or deselect
    * @param group (optional) Whether to add the clip to a group
    * @param track (optional) The track of the clip (has to be combined with @param pos)
    * @param pos (optional) The position of the clip (has to be combined with @param track) */
    void selectClip(bool add, bool group = false, int track = -1, int pos = -1);
    void selectTransition(bool add, bool group = false);
    QStringList extractTransitionsLumas();
    void setEditMode(TimelineMode::EditMode mode);

    /** @brief Inserts @param clip.
    * @param clip The clip to insert
    * @param in The inpoint of the clip (crop from start)
    * @param out The outpoint of the clip (crop from end)
    *
    * Inserts at the position of timeline cursor and selected track. */
    void insertClipCut(const QString &id, int in, int out);
    void clearSelection(bool emitInfo = true);
    void editItemDuration();
    void buildGuidesMenu(QMenu *goMenu) const;
    /** update the timeline objects when palette changes */
    void updatePalette();
    /** @brief Returns true if a track has audio data on it.
    * @param track The track number
    *
    * Check whether given track has a clip with audio in it. */
    bool hasAudio(int track) const;

    int getFrameWidth() const;
    /** @brief Returns last requested seeking pos, or current cursor position. */
    int seekPosition() const;

    void monitorRefresh(bool invalidateRange = false);
    /** @brief Trigger a monitor refresh if timeline cursor is inside range. */
    void monitorRefresh(const ItemInfo &range, bool invalidateRange = false);

    /** @brief Returns frame number of current mouse position. */
    int getMousePos() const;
    /** @brief Insert space in timeline after clips were moved. if fromstart is true, we assume clips have not yet been moved manually. */
    void completeSpaceOperation(int track, GenTime &timeOffset, bool fromStart = false);
    void spaceToolMoveToSnapPos(double snappedPos);
    void createRectangleSelection(Qt::KeyboardModifiers modifiers);
    int spaceToolSelectTrackOnly(int track, QList<QGraphicsItem *> &selection, GenTime pos = GenTime(-1));
    QList<QGraphicsItem *> selectAllItemsToTheRight(int x);
    GenTime createGroupForSelectedItems(QList<QGraphicsItem *> &selection);
    void resetSelectionGroup(bool selectItems = true);
    /** @brief Returns all infos necessary to save guides. */
    QMap<double, QString> guidesData() const;
    /** @brief Reset scroll bar to 0. */
    void scrollToStart();
    /** @brief Returns a track index (in MLT values) from an y position in timeline. */
    int getTrackFromPos(double y) const;
    /** @brief Returns customtrackview y position from an MLT track number. */
    int getPositionFromTrack(int track) const;
    /** @brief Expand current timeline clip (recover clips and tracks from an MLT playlist) */
    void expandActiveClip();
    /** @brief Import amultitrack MLT playlist in timeline */
    void importPlaylist(const ItemInfo &info, const QMap<QString, QString> &idMap, const QDomDocument &doc, QUndoCommand *command);
    /** @brief Returns true if there is a selected item in timeline */
    bool hasSelection() const;
    /** @brief Get the index of the video track that is just above current track */
    int getNextVideoTrack(int track);
    /** @brief returns id of clip under cursor and set pos to cursor position in clip,
     *  zone gets in/out points */
    const QString getClipUnderCursor(int *pos, QPoint *zone = nullptr) const;
    /** @brief returns displayable timecode info */
    QString getDisplayTimecode(const GenTime &time) const;
    QString getDisplayTimecodeFromFrames(int frames) const;
    /** @brief Call the default QGraphicsView mouse event */
    void graphicsViewMouseEvent(QMouseEvent *event);
    /** @brief Creates an overlay track with filtered clip */
    bool createSplitOverlay(Mlt::Filter *filter);
    void removeSplitOverlay();
    /** @brief Geometry keyframes dropped on a transition, start import */
    void dropTransitionGeometry(Transition *trans, const QString &geometry);
    /** @brief Geometry keyframes dropped on a clip, start import */
    void dropClipGeometry(ClipItem *trans, const QString &geometry);
    /** @brief Switch current track lock state */
    void switchTrackLock();
    void switchAllTrackLock();
    /** @brief Insert space in timeline. track = -1 means all tracks */
    void insertTimelineSpace(GenTime startPos, GenTime duration, int track = -1, const QList<ItemInfo> &excludeList = QList<ItemInfo>());
    void trimMode(bool enable, int ripplePos = -1);
    /** @brief Returns a clip from timeline
    *  @param pos the end time position
    *  @param track the track where the clip is in MLT coordinates */
    ClipItem *getClipItemAtEnd(GenTime pos, int track);
    /** @brief Returns a clip from timeline
     *  @param pos the time position
     *  @param track the track where the clip is in MLT coordinates
     *  @param end the end position of the clip in case of overlapping clips (overwrite mode) */
    ClipItem *getClipItemAtStart(GenTime pos, int track, GenTime end = GenTime());
    /** @brief Takes care of updating effects and attached transitions during a resize from start.
    * @param item Item to resize
    * @param oldInfo The item's info before resizement (set to item->info() is @param check true)
    * @param pos New startPos
    * @param check (optional, default = false) Whether to check for collisions
    * @param command (optional) Will be used as parent command (for undo history) */
    void prepareResizeClipStart(AbstractClipItem *item, const ItemInfo &oldInfo, int pos, bool check = false, QUndoCommand *command = nullptr);

    /** @brief Takes care of updating effects and attached transitions during a resize from end.
    * @param item Item to resize
    * @param oldInfo The item's info before resizement (set to item->info() is @param check true)
    * @param pos New endPos
    * @param check (optional, default = false) Whether to check for collisions
    * @param command (optional) Will be used as parent command (for undo history) */
    void prepareResizeClipEnd(AbstractClipItem *item, const ItemInfo &oldInfo, int pos, bool check = false, QUndoCommand *command = nullptr);
    AbstractClipItem *dragItem();
    /** @brief Cut clips in all non locked tracks. */
    void cutTimeline(int cutPos, const QList<ItemInfo> &excludedClips, const QList<ItemInfo> &excludedTransitions, QUndoCommand *masterCommand, int track = -1);
    void updateClipTypeActions(ClipItem *clip);
    void setOperationMode(OperationType mode);
    OperationType operationMode() const;
    OperationType prepareMode() const;
    TimelineMode::EditMode sceneEditMode();
    bool isLastClip(const ItemInfo &info);
    TrackInfo getTrackInfo(int ix);
    Transition *getTransitionItemAtStart(GenTime pos, int track);
    Transition *getTransitionItemAtEnd(GenTime pos, int track);
    void adjustTimelineTransitions(TimelineMode::EditMode mode, Transition *item, QUndoCommand *command);
    /** @brief Get the index of the video track that is just below current track */
    int getPreviousVideoTrack(int track);
    /** @brief Updates the duration stored in a track's TrackInfo.
     * @param track Number of track as used in ItemInfo (not the numbering used in KdenliveDoc) (negative for all tracks)
     * @param command If effects need to be updated the commands to do this will be attached to this undo command
     *
     * In addition to update the duration in TrackInfo it updates effects with keyframes on the track. */
    void updateTrackDuration(int track, QUndoCommand *command);
    /** @brief Send updtaed info to transition widget. */
    void updateTransitionWidget(Transition *tr, const ItemInfo &info);
    AbstractGroupItem *selectionGroup();
    Timecode timecode();
    /** @brief Collects information about the group's children to pass it on to RazorGroupCommand.
    * @param group The group to cut
    * @param cutPos The absolute position of the cut */
    void razorGroup(AbstractGroupItem *group, GenTime cutPos);
    void reloadTrack(const ItemInfo &info, bool includeLastFrame);
    GenTime groupSelectedItems(QList<QGraphicsItem *> selection = QList<QGraphicsItem *>(), bool createNewGroup = false, bool selectNewGroup = false);
    void sortGuides();
    void initTools();
    AbstractToolManager *toolManager(AbstractToolManager::ToolManagerType trimType);
    /** @brief Perform a ripple move on timeline clips */
    bool rippleClip(ClipItem *clip, int diff);
    void finishRipple(ClipItem *clip, const ItemInfo &startInfo, int diff, bool fromStart);

public slots:
    /** @brief Send seek request to MLT. */
    void seekCursorPos(int pos);
    /** @brief Move timeline cursor to new position. */
    void setCursorPos(int pos);
    void moveCursorPos(int delta);
    void slotDeleteEffectGroup(ClipItem *clip, int track, const QDomDocument &doc, bool affectGroup = true);
    void slotDeleteEffect(ClipItem *clip, int track, const QDomElement &effect, bool affectGroup = true, QUndoCommand *parentCommand = nullptr);
    void slotChangeEffectState(ClipItem *clip, int track, QList<int> effectIndexes, bool disable);
    void slotChangeEffectPosition(ClipItem *clip, int track, const QList<int> &currentPos, int newPos);
    void slotUpdateClipEffect(ClipItem *clip, int track, const QDomElement &oldeffect, const QDomElement &effect, int ix, bool refreshEffectStack = true);
    void slotUpdateClipRegion(ClipItem *clip, int ix, const QString &region);
    void slotRefreshEffects(ClipItem *clip);
    void setDuration(int duration);
    void slotAddTransition(ClipItem *clip, const ItemInfo &transitionInfo, int endTrack, const QDomElement &transition = QDomElement());
    void slotAddTransitionToSelectedClips(const QDomElement &transition, QList<QGraphicsItem *> itemList = QList<QGraphicsItem *>());
    void slotTransitionUpdated(Transition *, const QDomElement &);
    void slotSwitchTrackLock(int ix, bool enable, bool applyToAll = false);
    void slotUpdateClip(const QString &clipId, bool reload = true);

    bool addGuide(const GenTime &pos, const QString &comment, bool loadingProject = false);

    /** @brief Shows a dialog for adding a guide.
     * @param dialog (default = true) false = do not show the dialog but use current position as position and comment */
    void slotAddGuide(bool dialog = true);
    void slotEditGuide(const CommentedTime &guide);
    void slotEditGuide(int guidePos = -1, const QString &newText = QString());
    void slotDeleteGuide(int guidePos = -1);
    void slotDeleteAllGuides();
    void editGuide(const GenTime &oldPos, const GenTime &pos, const QString &comment);
    void copyClip();
    void pasteClip();
    void pasteClipEffects();
    void slotUpdateAllThumbs();
    void slotCheckPositionScrolling();
    void slotInsertTrack(int ix);
    /** @brief Trigger a monitor refresh. */
    void monitorRefresh(const QList<ItemInfo> &range, bool invalidateRange = false);

    /** @brief Shows a dialog for selecting a track to delete.
    * @param ix Number of the track, which should be pre-selected in the dialog */
    void slotDeleteTrack(int ix);
    /** @brief Shows the configure tracks dialog. */
    void slotConfigTracks(int ix);
    void clipNameChanged(const QString &id);
    void slotSelectTrack(int ix, bool switchTarget = false);
    int insertZone(TimelineMode::EditMode sceneMode, const QString &clipId, QPoint binZone);

    /** @brief Rebuilds a group to fit again after children changed.
    * @param childTrack the track of one of the groups children
    * @param childPos The position of the same child */
    void rebuildGroup(int childTrack, const GenTime &childPos);
    /** @brief Rebuilds a group to fit again after children changed.
    * @param group The group to rebuild */
    void rebuildGroup(AbstractGroupItem *group);

    /** @brief Add en effect to a track.
    * @param effect The new effect xml
    * @param ix The track index */
    void slotAddTrackEffect(const QDomElement &effect, int ix);
    /** @brief Select all clips in selected track. */
    void slotSelectClipsInTrack();
    /** @brief Select all clips in timeline. */
    void slotSelectAllClips();

    /** @brief Update the list of snap points (sticky timeline hotspots).
    * @param selected The currently selected clip if any
    * @param offsetList The list of points that should also snap (for example when movin a clip, start and end points should snap
    * @param skipSelectedItems if true, the selected item start and end points will not be added to snap list */
    void updateSnapPoints(AbstractClipItem *selected, QList<GenTime> offsetList = QList<GenTime> (), bool skipSelectedItems = false);

    void slotAddEffect(ClipItem *clip, const QDomElement &effect, int track = -1);
    void slotImportClipKeyframes(GraphicsRectItem type, const ItemInfo &info, const QDomElement &xml, QMap<QString, QString> keyframes = QMap<QString, QString>());

    /** @brief Move playhead to mouse curser position if defined key is pressed */
    void slotAlignPlayheadToMousePos();

    void slotInfoProcessingFinished();
    void slotAlignClip(int, int, int);
    /** @brief Export part of the playlist in an xml file */
    void exportTimelineSelection(QString path = QString());
    /** Remove zone from current track */
    void extractZone(QPoint z, bool closeGap, const QList<ItemInfo> &excludedClips = QList<ItemInfo>(), QUndoCommand *masterCommand = nullptr, int track = -1);
    /** @brief Select an item in timeline. */
    void slotSelectItem(AbstractClipItem *item);
    /** @brief Cycle through timeline trim modes */
    void switchTrimMode(TrimMode mode = NormalTrim);
    void switchTrimMode(int mode);

protected:
    void drawBackground(QPainter *painter, const QRectF &rect) Q_DECL_OVERRIDE;
    //virtual void drawForeground ( QPainter * painter, const QRectF & rect );
    void dragEnterEvent(QDragEnterEvent *event) Q_DECL_OVERRIDE;
    void dragMoveEvent(QDragMoveEvent *event) Q_DECL_OVERRIDE;
    void dragLeaveEvent(QDragLeaveEvent *event) Q_DECL_OVERRIDE;
    bool event(QEvent *e) Q_DECL_OVERRIDE;
    /** @brief Something has been dropped onto the timeline */
    void dropEvent(QDropEvent *event) Q_DECL_OVERRIDE;
    void enterEvent(QEvent *event) Q_DECL_OVERRIDE;
    void leaveEvent(QEvent *event) Q_DECL_OVERRIDE;
    void wheelEvent(QWheelEvent *e) Q_DECL_OVERRIDE;
    void keyPressEvent(QKeyEvent *event) Q_DECL_OVERRIDE;
    virtual QStringList mimeTypes() const;
    virtual Qt::DropActions supportedDropActions() const;
    void contextMenuEvent(QContextMenuEvent *event) Q_DECL_OVERRIDE;

private:
    int m_ct;
    int m_tracksHeight;
    int m_projectDuration;
    int m_cursorPos;
    double m_cursorOffset;
    KdenliveDoc *m_document;
    Timeline *m_timeline;
    CustomTrackScene *m_scene;
    QGraphicsLineItem *m_cursorLine;
    ItemInfo m_dragItemInfo;
    ItemInfo m_selectionGroupInfo;
    /** @brief Possible timeline action */
    OperationType m_operationMode;
    /** @brief Currently running operation */
    OperationType m_moveOpMode;
    AbstractClipItem *m_dragItem;
    Guide *m_dragGuide;
    DocUndoStack *m_commandStack;
    QGraphicsItem *m_visualTip;
    QGraphicsItemAnimation *m_keyProperties;
    QTimeLine *m_keyPropertiesTimer;
    QColor m_tipColor;
    QPen m_tipPen;
    QPoint m_clickEvent;
    QList<CommentedTime> m_searchPoints;
    QList<Guide *> m_guides;
    QColor m_selectedTrackColor;
    QColor m_lockedTrackColor;
    QMap<AbstractToolManager::ToolManagerType, AbstractToolManager *> m_toolManagers;
    AbstractToolManager *m_currentToolManager;

    /** @brief Returns a clip from timeline
     *  @param pos a time value that is inside the clip
     *  @param track the track where the clip is in MLT coordinates */
    ClipItem *getClipItemAtMiddlePoint(int pos, int track);
    /** @brief Returns the higher clip at pos on the timeline
     *  @param pos a time value that is inside the clip */
    ClipItem *getUpperClipItemAt(int pos);
    /** @brief Returns a moved clip from timeline (means that the item was moved but its ItemInfo coordinates have not been updated yet)
     * */
    ClipItem *getMovedClipItem(const ItemInfo &info, GenTime offset, int trackOffset);
    /** @brief Returns a transition from timeline
     *  @param pos a time value that is inside the clip
     *  @param track the track where the clip is in MLT coordinates */
    Transition *getTransitionItemAt(int pos, int track, bool alreadyMoved = false);
    Transition *getTransitionItemAt(GenTime pos, int track, bool alreadyMoved = false);
    void checkScrolling();
    /** Should we auto scroll while playing (keep in sync with KdenliveSettings::autoscroll() */
    bool m_autoScroll;
    void displayContextMenu(QPoint pos, AbstractClipItem *clip);
    void displayKeyframesMenu(QPoint pos, AbstractClipItem *clip);
    QMenu *m_timelineContextMenu;
    QMenu *m_timelineContextClipMenu;
    QMenu *m_timelineContextTransitionMenu;
    QMenu *m_timelineContextKeyframeMenu;
    KSelectAction *m_selectKeyframeType;
    QAction *m_attachKeyframeToEnd;
    QMenu *m_markerMenu;
    QAction *m_autoTransition;
    QAction *m_pasteEffectsAction;
    QAction *m_ungroupAction;
    QAction *m_disableClipAction;
    QAction *m_editGuide;
    QAction *m_deleteGuide;
    QList<QAction *> m_audioActions;
    QList<QAction *> m_avActions;
    QActionGroup *m_clipTypeGroup;
    bool m_clipDrag;

    int m_findIndex;
    ProjectTool m_tool;
    QCursor m_razorCursor;
    /** list containing items currently copied in the timeline */
    QList<AbstractClipItem *> m_copiedItems;
    /** Used to get the point in timeline where a context menu was opened */
    QPoint m_menuPosition;
    AbstractGroupItem *m_selectionGroup;
    int m_selectedTrack;

    QMutex m_selectionMutex;
    QMutex m_mutex;
    QWaitCondition m_producerNotReady;

    AudioCorrelation *m_audioCorrelator;
    ClipItem *m_audioAlignmentReference;

    void updatePositionEffects(ClipItem *item, const ItemInfo &info, bool standalone = true);
    bool insertDropClips(const QMimeData *mimeData, const QPoint &pos);
    bool canBePastedTo(const QList<ItemInfo> &infoList, int type) const;
    bool canBePasted(const QList<AbstractClipItem *> &items, GenTime offset, int trackOffset, QList<AbstractClipItem *>excluded = QList<AbstractClipItem *>()) const;
    ClipItem *getClipUnderCursor() const;
    AbstractClipItem *getMainActiveClip() const;
    /** Get available space for clip move (min and max free positions) */
    void getClipAvailableSpace(AbstractClipItem *item, GenTime &minimum, GenTime &maximum);
    /** Get available space for transition move (min and max free positions) */
    void getTransitionAvailableSpace(AbstractClipItem *item, GenTime &minimum, GenTime &maximum);
    /** Whether an item can be moved to a new position without colliding with similar items */
    bool itemCollision(AbstractClipItem *item, const ItemInfo &newPos);
    /** Selects all items in the scene rect, and sets ok to false if a group going over several tracks is found in it */
    QList<QGraphicsItem *> checkForGroups(const QRectF &rect, bool *ok);
    /** Adjust keyframes when pasted to another clip */
    void adjustKeyfames(GenTime oldstart, GenTime newstart, GenTime duration, QDomElement xml);

    /** @brief Removes the tip and stops the animation timer. */
    void removeTipAnimation();

    /** @brief Adjusts effects after a clip resize.
     * @param item The item that was resized
     * @param oldInfo pre resize info
     * @param fromStart false = resize from end
     * @param command Used as a parent for EditEffectCommand */
    void adjustEffects(ClipItem *item, const ItemInfo &oldInfo, QUndoCommand *command);

    /** @brief Prepare an add clip command for an effect */
    void processEffect(ClipItem *item, const QDomElement &effect, int offset, QUndoCommand *effectCommand);
    /** @brief Reload all clips and transitions from MLT's playlist */
    void reloadTimeline();
    /** @brief Timeline selection changed, update effect stack. */
    void updateTimelineSelection();
    /** @brief Break groups containing an item in a locked track. */
    void breakLockedGroups(const QList<ItemInfo> &clipsToMove, const QList<ItemInfo> &transitionsToMove, QUndoCommand *masterCommand, bool doIt = true);
    void slotTrackUp();
    void slotTrackDown();

private slots:
    void slotRefreshGuides();
    void slotEditTimeLineGuide();
    void slotDeleteTimeLineGuide();
    void checkTrackSequence(int track);
    void slotGoToMarker(QAction *action);
    /** @brief Context menu is finished, prepare resetting las known menu pos. */
    void slotResetMenuPosition();
    /** @brief Context menu is finished, restore normal operation mode. */
    void slotContextMenuActivated();
    void slotDoResetMenuPosition();
    /** @brief A Filter job producer results. */
    void slotGotFilterJobResults(const QString &id, int startPos, int track, const stringMap &filterParams, const stringMap &extra);
    /** @brief Replace a producer in all tracks (for example when proxying a clip). */
    void slotReplaceTimelineProducer(const QString &id);
    void slotPrepareTimelineReplacement(const QString &id);
    /** @brief Update a producer in all tracks (for example when an effect changed). */
    void slotUpdateTimelineProducer(const QString &id);
    void slotEditKeyframeType(QAction *action);
    void slotAttachKeyframeToEnd(bool attach);
    void disableClip();
    void slotAcceptRipple(bool accept);
    void doRipple(bool accept);

signals:
    void cursorMoved(int, int);
    void zoomIn(bool zoomOnMouse);
    void zoomOut(bool zoomOnMouse);
    void mousePosition(int);
    /** @brief A clip was selected in timeline, update the effect stack
     *  @param clip The clip
     *  @param raise If true, the effect stack widget will be raised (come to front). */
    void clipItemSelected(ClipItem *clip, bool reloadStack = true);
    void transitionItemSelected(Transition *, int track = 0, QPoint p = QPoint(), bool update = false);
    void activateDocumentMonitor();
    void tracksChanged();
    void displayMessage(const QString &, MessageType);
    void doTrackLock(int, bool);
    void updateClipMarkers(ClipController *);
    void updateTrackHeaders();
    void playMonitor();
    void pauseMonitor();
    /** @brief Monitor document changes (for example the presence of audio data in timeline for export widget.*/
    void documentModified();
    void showTrackEffects(int, TrackInfo);
    /** @brief Update the track effect button that shows if a track has effects or not.*/
    void updateTrackEffectState(int);
    /** @brief Cursor position changed, repaint ruler.*/
    void updateRuler(int pos);
    /** @brief Send data from a clip to be imported as keyframes for effect / transition.*/
    void importKeyframes(GraphicsRectItem type, const QString &, const QString &);
    /** @brief Guides were changed, inform render widget*/
    void guidesUpdated();
    /** @brief Prepare importing and expand of a playlist clip */
    void importPlaylistClips(const ItemInfo &info, const QString &url, QUndoCommand *expandCommand);
    /** @brief Show a specific frame in clip monitor */
    void showClipFrame(const QString &id, int frame);
    /** @brief Select active keyframe in effect stack */
    void setActiveKeyframe(int);
    void loadMonitorScene(MonitorSceneType, bool);
    void updateTrimMode(const QString &mode = QString());
    void setQmlProperty(const QString &, const QVariant &);
};

#endif

