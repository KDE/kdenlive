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

#include <kdeversion.h>
#include <KColorScheme>

#include <QGraphicsView>
#include <QGraphicsItemAnimation>
#include <QTimeLine>
#include <QMenu>
#include <QUndoStack>
#include <QMutex>
#include <QWaitCondition>

#include "kdenlivedoc.h"
#include "docclipbase.h"
#include "guide.h"
#include "effectslist.h"
#include "customtrackscene.h"

class ClipItem;
class AbstractClipItem;
class AbstractGroupItem;
class Transition;
class AudioCorrelation;

class CustomTrackView : public QGraphicsView
{
    Q_OBJECT

public:
    CustomTrackView(KdenliveDoc *doc, CustomTrackScene* projectscene, QWidget *parent = 0);
    virtual ~ CustomTrackView();
    virtual void mousePressEvent(QMouseEvent * event);
    virtual void mouseReleaseEvent(QMouseEvent * event);
    virtual void mouseMoveEvent(QMouseEvent * event);
    virtual void mouseDoubleClickEvent(QMouseEvent *event);
    void addTrack(TrackInfo type, int ix = -1);
    void removeTrack(int ix);
    /** @brief Makes the document use new track infos (name, type, ...). */
    void configTracks(QList <TrackInfo> trackInfos);
    int cursorPos();
    void checkAutoScroll();
    /**
      Move the clip at \c start to \c end.

      If \c out_actualEnd is not NULL, it will be set to the position the clip really ended up at.
      For example, attempting to move a clip to t = -1 s will actually move it to t = 0 s.
      */
    bool moveClip(const ItemInfo &start, const ItemInfo &end, bool refresh, ItemInfo *out_actualEnd = NULL);
    void moveGroup(QList <ItemInfo> startClip, QList <ItemInfo> startTransition, const GenTime &offset, const int trackOffset, bool reverseMove = false);
    /** move transition, startPos = (old start, old end), endPos = (new start, new end) */
    void moveTransition(const ItemInfo &start, const ItemInfo &end, bool refresh);
    void resizeClip(const ItemInfo &start, const ItemInfo &end, bool dontWorry = false);
    void addClip(QDomElement xml, const QString &clipId, ItemInfo info, EffectsList list = EffectsList(), bool overwrite = false, bool push = false, bool refresh = true);
    void deleteClip(ItemInfo info, bool refresh = true);
    void slotDeleteClipMarker(const QString &comment, const QString &id, const GenTime &position);
    void slotDeleteAllClipMarkers(const QString &id);
    void addMarker(const QString &id, const CommentedTime marker);
    void addData(const QString &id, const QString &key, const QString &data);
    void setScale(double scaleFactor, double verticalScale);
    void deleteClip(const QString &clipId);
    /** @brief Add effect to current clip */
    void slotAddEffect(QDomElement effect, GenTime pos, int track);
    void slotAddGroupEffect(QDomElement effect, AbstractGroupItem *group, AbstractClipItem *dropTarget = NULL);
    void addEffect(int track, GenTime pos, QDomElement effect);
    void deleteEffect(int track, GenTime pos, QDomElement effect);
    void updateEffect(int track, GenTime pos, QDomElement insertedEffect, bool refreshEffectStack = false);
    /** @brief Enable / disable a list of effects */
    void updateEffectState(int track, GenTime pos, QList <int> effectIndexes, bool disable, bool updateEffectStack);
    void moveEffect(int track, GenTime pos, QList <int> oldPos, QList <int> newPos);
    void addTransition(ItemInfo transitionInfo, int endTrack, QDomElement params, bool refresh);
    void deleteTransition(ItemInfo transitionInfo, int endTrack, QDomElement params, bool refresh);
    void updateTransition(int track, GenTime pos,  QDomElement oldTransition, QDomElement transition, bool updateTransitionWidget);
    void moveTransition(GenTime oldpos, GenTime newpos);
    void activateMonitor();
    int duration() const;
    void deleteSelectedClips();
    /** @brief Cuts all clips that are selected at the timeline cursor position. */
    void cutSelectedClips();
    void setContextMenu(QMenu *timeline, QMenu *clip, QMenu *transition, QActionGroup *clipTypeGroup, QMenu *markermenu);
    bool checkTrackHeight();
    void updateSceneFrameWidth();
    //QList <TrackInfo> tracksList() const;
    void setTool(PROJECTTOOL tool);
    ClipItem *cutClip(ItemInfo info, GenTime cutTime, bool cut, bool execute = true);
    void slotSeekToPreviousSnap();
    void slotSeekToNextSnap();
    double getSnapPointForPos(double pos);
    void editKeyFrame(const GenTime &pos, const int track, const int index, const QString &keyframes);
    bool findString(const QString &text);
    void selectFound(QString track, QString pos);
    bool findNextString(const QString &text);
    void initSearchStrings();
    void clearSearchStrings();
    QList<ItemInfo> findId(const QString &clipId);
    void clipStart();
    void clipEnd();
    void doChangeClipSpeed(ItemInfo info, ItemInfo speedIndependantInfo, const double speed, const double oldspeed, int strobe, const QString &id);
    /** @brief Sets the document as modified. */
    void setDocumentModified();
    void setInPoint();
    void setOutPoint();

    /** @brief Prepares inserting space.
    *
    * Shows a dialog to configure length and track. */
    void slotInsertSpace();
    /** @brief Prepares removing space. */
    void slotRemoveSpace();
    void insertSpace(QList<ItemInfo> clipsToMove, QList<ItemInfo> transToMove, int track, const GenTime &duration, const GenTime &offset);
    ClipItem *getActiveClipUnderCursor(bool allowOutsideCursor = false) const;
    void deleteTimelineTrack(int ix, TrackInfo trackinfo);
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
    void groupClips(bool group = true);
    void doGroupClips(QList <ItemInfo> clipInfos, QList <ItemInfo> transitionInfos, bool group);
    void loadGroups(const QDomNodeList &groups);

    /** @brief Creates SplitAudioCommands for selected clips. */
    void splitAudio();

    /// Define which clip to take as reference for automatic audio alignment
    void setAudioAlignReference();

    /// Automatically align the currently selected clips to synchronize their audio with the reference's audio
    void alignAudio();

    /** @brief Seperates the audio of a clip to a audio track.
    * @param pos Position of the clip to split
    * @param track Track of the clip
    * @param split Split or unsplit */
    void doSplitAudio(const GenTime &pos, int track, EffectsList effects, bool split);
    void setVideoOnly();
    void setAudioOnly();
    void setAudioAndVideo();
    void doChangeClipType(const GenTime &pos, int track, bool videoOnly, bool audioOnly);
    int hasGuide(int pos, int offset);
    void reloadTransitionLumas();
    void updateProjectFps();
    double fps() const;
    int selectedTrack() const;
    QStringList selectedClips() const;
    QList<ClipItem *> selectedClipItems() const;
    /** @brief Checks wheter an item can be inserted (make sure it does not overlap another item) */
    bool canBePastedTo(ItemInfo info, int type) const;

    /** @brief Selects a clip.
    * @param add Whether to select or deselect
    * @param group (optional) Whether to add the clip to a group
    * @param track (optional) The track of the clip (has to be combined with @param pos)
    * @param pos (optional) The position of the clip (has to be combined with @param track) */
    void selectClip(bool add, bool group = false, int track = -1, int pos = -1);
    void selectTransition(bool add, bool group = false);
    QStringList extractTransitionsLumas();
    void setEditMode(EDITMODE mode);

    /** @brief Inserts @param clip.
    * @param clip The clip to insert
    * @param in The inpoint of the clip (crop from start)
    * @param out The outpoint of the clip (crop from end)
    *
    * Inserts at the position of timeline cursor and selected track. */
    void insertClipCut(DocClipBase *clip, int in, int out);
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

    int getFrameWidth();
    /** @brief Returns last requested seeking pos (or SEEK_INACTIVE if no seek). */
    int seekPosition() const;

    /** @brief Trigger a monitor refresh. */
    void monitorRefresh();
    
public slots:
    /** @brief Send seek request to MLT. */
    void seekCursorPos(int pos);
    /** @brief Move timeline cursor to new position. */
    void setCursorPos(int pos);
    void moveCursorPos(int delta);
    void updateCursorPos();
    void slotDeleteEffect(ClipItem *clip, int track, QDomElement effect, bool affectGroup = true);
    void slotChangeEffectState(ClipItem *clip, int track, QList <int> effectIndexes, bool disable);
    void slotChangeEffectPosition(ClipItem *clip, int track, QList <int> currentPos, int newPos);
    void slotUpdateClipEffect(ClipItem *clip, int track, QDomElement oldeffect, QDomElement effect, int ix, bool refreshEffectStack = true);
    void slotUpdateClipRegion(ClipItem *clip, int ix, QString region);
    void slotRefreshEffects(ClipItem *clip);
    void setDuration(int duration);
    void slotAddTransition(ClipItem* clip, ItemInfo transitionInfo, int endTrack, QDomElement transition = QDomElement());
    void slotAddTransitionToSelectedClips(QDomElement transition);
    void slotTransitionUpdated(Transition *, QDomElement);
    void slotSwitchTrackAudio(int ix);
    void slotSwitchTrackVideo(int ix);
    void slotSwitchTrackLock(int ix);
    void slotUpdateClip(const QString &clipId, bool reload = true);
    
    /** @brief Add extra data to a clip. */
    void slotAddClipExtraData(const QString &id, const QString &key, const QString &data = QString(), QUndoCommand *groupCommand = 0);
    /** @brief Creates a AddClipCommand to add, edit or delete a marker.
     * @param id Id of the marker's clip
     * @param t Position of the marker
     * @param c Comment of the marker */
    void slotAddClipMarker(const QString &id, QList <CommentedTime> newMarker, QUndoCommand *groupCommand = 0);
    void slotLoadClipMarkers(const QString &id);
    void slotSaveClipMarkers(const QString &id);
    bool addGuide(const GenTime &pos, const QString &comment);

    /** @brief Shows a dialog for adding a guide.
     * @param dialog (default = true) false = do not show the dialog but use current position as position and comment */
    void slotAddGuide(bool dialog = true);
    void slotEditGuide(CommentedTime guide);
    void slotEditGuide(int guidePos = -1);
    void slotDeleteGuide(int guidePos = -1);
    void slotDeleteAllGuides();
    void editGuide(const GenTime &oldPos, const GenTime &pos, const QString &comment);
    void copyClip();
    void pasteClip();
    void pasteClipEffects();
    void slotUpdateAllThumbs();
    void slotCheckPositionScrolling();
    void slotInsertTrack(int ix);

    /** @brief Shows a dialog for selecting a track to delete.
    * @param ix Number of the track, which should be pre-selected in the dialog */
    void slotDeleteTrack(int ix);
    /** @brief Shows the configure tracks dialog. */
    void slotConfigTracks(int ix);
    void clipNameChanged(const QString &id, const QString &name);
    void slotTrackUp();
    void slotTrackDown();
    void slotSelectTrack(int ix);
    void insertZoneOverwrite(QStringList data, int in);

    /** @brief Rebuilds a group to fit again after children changed.
    * @param childTrack the track of one of the groups children
    * @param childPos The position of the same child */
    void rebuildGroup(int childTrack, GenTime childPos);
    /** @brief Rebuilds a group to fit again after children changed.
    * @param group The group to rebuild */
    void rebuildGroup(AbstractGroupItem *group);

    /** @brief Cuts a group into two parts.
    * @param clips1 Clips before the cut
    * @param transitions1 Transitions before the cut
    * @param clipsCut Clips that need to be cut
    * @param transitionsCut Transitions that need to be cut
    * @param clips2 Clips behind the cut
    * @param transitions2 Transitions behind the cut
    * @param cutPos Absolute position of the cut
    * @param cut true = cut, false = "uncut" */
    void slotRazorGroup(QList <ItemInfo> clips1, QList <ItemInfo> transitions1, QList <ItemInfo> clipsCut, QList <ItemInfo> transitionsCut, QList <ItemInfo> clips2, QList <ItemInfo> transitions2, GenTime cutPos, bool cut);

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
    void updateSnapPoints(AbstractClipItem *selected, QList <GenTime> offsetList = QList <GenTime> (), bool skipSelectedItems = false);
    
    void slotAddEffect(ClipItem *clip, QDomElement effect);
    void slotImportClipKeyframes(GRAPHICSRECTITEM type);

    /** @brief Get effect parameters ready for MLT*/
    static void adjustEffectParameters(EffectsParameterList &parameters, QDomNodeList params, MltVideoProfile profile, const QString &prefix = QString());

protected:
    virtual void drawBackground(QPainter * painter, const QRectF & rect);
    //virtual void drawForeground ( QPainter * painter, const QRectF & rect );
    virtual void dragEnterEvent(QDragEnterEvent * event);
    virtual void dragMoveEvent(QDragMoveEvent * event);
    virtual void dragLeaveEvent(QDragLeaveEvent * event);
    /** @brief Something has been dropped onto the timeline */
    virtual void dropEvent(QDropEvent * event);
    virtual void wheelEvent(QWheelEvent * e);
    virtual void keyPressEvent(QKeyEvent * event);
    virtual QStringList mimeTypes() const;
    virtual Qt::DropActions supportedDropActions() const;

private:
    int m_ct;
    int m_tracksHeight;
    int m_projectDuration;
    int m_cursorPos;
    KdenliveDoc *m_document;
    CustomTrackScene *m_scene;
    QGraphicsLineItem *m_cursorLine;
    ItemInfo m_dragItemInfo;
    ItemInfo m_selectionGroupInfo;
    OPERATIONTYPE m_operationMode;
    OPERATIONTYPE m_moveOpMode;
    AbstractClipItem *m_dragItem;
    Guide *m_dragGuide;
    QUndoStack *m_commandStack;
    QGraphicsItem *m_visualTip;
    QGraphicsItemAnimation *m_animation;
    QTimeLine *m_animationTimer;
    QColor m_tipColor;
    QPen m_tipPen;
    QPoint m_clickPoint;
    QPoint m_clickEvent;
    QList <CommentedTime> m_searchPoints;
    QList <Guide *> m_guides;

    ClipItem *getClipItemAt(int pos, int track);
    ClipItem *getClipItemAt(GenTime pos, int track);
    ClipItem *getClipItemAtEnd(GenTime pos, int track);
    ClipItem *getClipItemAtStart(GenTime pos, int track);
    Transition *getTransitionItem(TransitionInfo info);
    Transition *getTransitionItemAt(int pos, int track);
    Transition *getTransitionItemAt(GenTime pos, int track);
    Transition *getTransitionItemAtEnd(GenTime pos, int track);
    Transition *getTransitionItemAtStart(GenTime pos, int track);
    void checkScrolling();
    /** Should we auto scroll while playing (keep in sync with KdenliveSettings::autoscroll() */
    bool m_autoScroll;
    void displayContextMenu(QPoint pos, AbstractClipItem *clip, AbstractGroupItem *group);
    QMenu *m_timelineContextMenu;
    QMenu *m_timelineContextClipMenu;
    QMenu *m_timelineContextTransitionMenu;
    QMenu *m_markerMenu;
    QAction *m_autoTransition;
    QAction *m_pasteEffectsAction;
    QAction *m_ungroupAction;
    QAction *m_editGuide;
    QAction *m_deleteGuide;
    QActionGroup *m_clipTypeGroup;
    QTimer m_scrollTimer;
    QTimer m_thumbsTimer;
    int m_scrollOffset;
    bool m_clipDrag;

    int m_findIndex;
    PROJECTTOOL m_tool;
    QCursor m_razorCursor;
    QCursor m_spacerCursor;
    /** list containing items currently copied in the timeline */
    QList<AbstractClipItem *> m_copiedItems;
    /** Used to get the point in timeline where a context menu was opened */
    QPoint m_menuPosition;
    bool m_blockRefresh;
    AbstractGroupItem *m_selectionGroup;
    QList <ClipItem *> m_waitingThumbs;
    int m_selectedTrack;
    int m_spacerOffset;

    QMutex m_mutex;
    QWaitCondition m_producerNotReady;
    KStatefulBrush m_activeTrackBrush;

    AudioCorrelation *m_audioCorrelator;
    ClipItem *m_audioAlignmentReference;

    /** stores the state of the control modifier during mouse press.
     * Will then be used to identify whether we resize a group or only one item of it. */
    bool m_controlModifier;

    /** Get the index of the video track that is just below current track */
    int getPreviousVideoTrack(int track);
    void updatePositionEffects(ClipItem * item, ItemInfo info, bool standalone = true);
    bool insertDropClips(const QMimeData *data, const QPoint &pos);
    bool canBePastedTo(QList <ItemInfo> infoList, int type) const;
    bool canBePasted(QList<AbstractClipItem *> items, GenTime offset, int trackOffset) const;
    bool canBeMoved(QList<AbstractClipItem *> items, GenTime offset, int trackOffset) const;
    ClipItem *getClipUnderCursor() const;
    AbstractClipItem *getMainActiveClip() const;
    void resetSelectionGroup(bool selectItems = true);
    void groupSelectedItems(QList <QGraphicsItem *> selection = QList <QGraphicsItem *>(), bool force = false, bool createNewGroup = false, bool selectNewGroup = false);
    /** Get available space for clip move (min and max free positions) */
    void getClipAvailableSpace(AbstractClipItem *item, GenTime &minimum, GenTime &maximum);
    /** Get available space for transition move (min and max free positions) */
    void getTransitionAvailableSpace(AbstractClipItem *item, GenTime &minimum, GenTime &maximum);
    void updateClipTypeActions(ClipItem *clip);
    /** Whether an item can be moved to a new position without colliding with similar items */
    bool itemCollision(AbstractClipItem *item, ItemInfo newPos);
    /** Selects all items in the scene rect, and sets ok to false if a group going over several tracks is found in it */
    QList<QGraphicsItem *> checkForGroups(const QRectF &rect, bool *ok);
    /** Adjust clips under another one when working in overwrite mode */
    void adjustTimelineClips(EDITMODE mode, ClipItem *item, ItemInfo posinfo, QUndoCommand *command);
    void adjustTimelineTransitions(EDITMODE mode, Transition *item, QUndoCommand *command);
    /** Adjust keyframes when pasted to another clip */
    void adjustKeyfames(GenTime oldstart, GenTime newstart, GenTime duration, QDomElement xml);

    /** @brief Removes the tip and stops the animation timer. */
    void removeTipAnimation();
    /** @brief Creates a new tip animation.
    * @param clip clip to display the tip on
    * @param mode operation mode for which the tip should be displayed
    * @param size size of the tip */
    void setTipAnimation(AbstractClipItem *clip, OPERATIONTYPE mode, const double size);

    /** @brief Takes care of updating effects and attached transitions during a resize from start.
    * @param item Item to resize
    * @param oldInfo The item's info before resizement (set to item->info() is @param check true)
    * @param pos New startPos
    * @param check (optional, default = false) Whether to check for collisions
    * @param command (optional) Will be used as parent command (for undo history) */
    void prepareResizeClipStart(AbstractClipItem *item, ItemInfo oldInfo, int pos, bool check = false, QUndoCommand *command = NULL);

    /** @brief Takes care of updating effects and attached transitions during a resize from end.
    * @param item Item to resize
    * @param oldInfo The item's info before resizement (set to item->info() is @param check true)
    * @param pos New endPos
    * @param check (optional, default = false) Whether to check for collisions
    * @param command (optional) Will be used as parent command (for undo history) */
    void prepareResizeClipEnd(AbstractClipItem *item, ItemInfo oldInfo, int pos, bool check = false, QUndoCommand *command = NULL);

    /** @brief Collects information about the group's children to pass it on to RazorGroupCommand.
    * @param group The group to cut
    * @param cutPos The absolute position of the cut */
    void razorGroup(AbstractGroupItem *group, GenTime cutPos);

    /** @brief Gets the effect parameters that will be passed to Mlt. */
    EffectsParameterList getEffectArgs(const QDomElement &effect);

    /** @brief Update Tracknames to fit again after track was added/deleted.
     * @param track Number of track which was added/deleted
     * @param added true = track added, false = track deleted
     * 
     * The default track name consists of type + number. If we add/delete a track the number has to be adjusted
     * if the name is still the default one. */
    void updateTrackNames(int track, bool added);

    /** @brief Updates the duration stored in a track's TrackInfo.
     * @param track Number of track as used in ItemInfo (not the numbering used in KdenliveDoc) (negative for all tracks)
     * @param command If effects need to be updated the commands to do this will be attached to this undo command
     * 
     * In addition to update the duration in TrackInfo it updates effects with keyframes on the track. */
    void updateTrackDuration(int track, QUndoCommand *command);

    /** @brief Adjusts effects after a clip resize.
     * @param item The item that was resized
     * @param oldInfo pre resize info
     * @param fromStart false = resize from end
     * @param command Used as a parent for EditEffectCommand */
    void adjustEffects(ClipItem *item, ItemInfo oldInfo, QUndoCommand *command);
    
    /** @brief Prepare an add clip command for an effect */
    void processEffect(ClipItem *item, QDomElement effect, int offset, QUndoCommand *effectCommand);

private slots:
    void slotRefreshGuides();
    void slotEnableRefresh();
    void slotCheckMouseScrolling();
    void slotEditTimeLineGuide();
    void slotDeleteTimeLineGuide();
    void slotFetchNextThumbs();
    void checkTrackSequence(int track);
    void slotGoToMarker(QAction *action);
    void slotResetMenuPosition();
    void slotDoResetMenuPosition();
    /** @brief Re-create the clip thumbnails.
     *  @param id The clip's Id string.
     *  @param resetThumbs Should we recreate the timeline thumbnails. */
    void slotRefreshThumbs(const QString &id, bool resetThumbs);
    /** @brief A Filter job producer results. */
    void slotGotFilterJobResults(const QString &id, int startPos, int track, stringMap filterParams, stringMap extra);


signals:
    void cursorMoved(int, int);
    void zoomIn();
    void zoomOut();
    void mousePosition(int);
    /** @brief A clip was selected in timeline, update the effect stack
     *  @param clip The clip
     *  @param raise If true, the effect stack widget will be raised (come to front). */
    void clipItemSelected(ClipItem *clip, bool raise = true);
    void transitionItemSelected(Transition*, int track = 0, QPoint p = QPoint(), bool update = false);
    void activateDocumentMonitor();
    void trackHeightChanged();
    void tracksChanged();
    void displayMessage(const QString &, MessageType);
    void showClipFrame(DocClipBase *, QPoint, bool, const int);
    void doTrackLock(int, bool);
    void updateClipMarkers(DocClipBase *);
    void updateClipExtraData(DocClipBase *);
    void updateTrackHeaders();
    void playMonitor();
    /** @brief Monitor document changes (for example the presence of audio data in timeline for export widget.*/
    void documentModified();
    void forceClipProcessing(const QString &);
    void showTrackEffects(int, TrackInfo);
    /** @brief Update the track effect button that shows if a track has effects or not.*/
    void updateTrackEffectState(int);
    /** @brief Cursor position changed, repaint ruler.*/
    void updateRuler();
    /** @brief Send data from a clip to be imported as keyframes for effect / transition.*/
    void importKeyframes(GRAPHICSRECTITEM type, const QString&, int maximum);
};

#endif

