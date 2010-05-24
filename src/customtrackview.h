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

#include <KPixmapCache>

#include <QGraphicsView>
#include <QGraphicsItemAnimation>
#include <QTimeLine>
#include <QMenu>
#include <QUndoStack>

#include "kdenlivedoc.h"
#include "docclipbase.h"
#include "guide.h"
#include "effectslist.h"
#include "customtrackscene.h"

class ClipItem;
class AbstractClipItem;
class AbstractGroupItem;
class Transition;

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
    void changeTrack(int ix, TrackInfo type);
    int cursorPos();
    void checkAutoScroll();
    void moveClip(const ItemInfo start, const ItemInfo end, bool refresh);
    void moveGroup(QList <ItemInfo> startClip, QList <ItemInfo> startTransition, const GenTime offset, const int trackOffset, bool reverseMove = false);
    /** move transition, startPos = (old start, old end), endPos = (new start, new end) */
    void moveTransition(const ItemInfo start, const ItemInfo end, bool m_refresh);
    void resizeClip(const ItemInfo start, const ItemInfo end, bool dontWorry = false);
    void addClip(QDomElement xml, const QString &clipId, ItemInfo info, EffectsList list = EffectsList(), bool overwrite = false, bool push = false, bool refresh = true);
    void deleteClip(ItemInfo info, bool refresh = true);
    void slotDeleteClipMarker(const QString &comment, const QString &id, const GenTime &position);
    void slotDeleteAllClipMarkers(const QString &id);
    void addMarker(const QString &id, const GenTime &pos, const QString comment);
    void setScale(double scaleFactor, double verticalScale);
    void deleteClip(const QString &clipId);
    void slotAddEffect(QDomElement effect, GenTime pos, int track);
    void slotAddGroupEffect(QDomElement effect, AbstractGroupItem *group);
    void addEffect(int track, GenTime pos, QDomElement effect);
    void deleteEffect(int track, GenTime pos, QDomElement effect);
    void updateEffect(int track, GenTime pos, QDomElement insertedEffect, int ix, bool triggeredByUser = true);
    void moveEffect(int track, GenTime pos, int oldPos, int newPos);
    void addTransition(ItemInfo transitionInfo, int endTrack, QDomElement params, bool refresh);
    void deleteTransition(ItemInfo transitionInfo, int endTrack, QDomElement params, bool refresh);
    void updateTransition(int track, GenTime pos,  QDomElement oldTransition, QDomElement transition, bool updateTransitionWidget);
    void moveTransition(GenTime oldpos, GenTime newpos);
    void activateMonitor();
    int duration() const;
    void deleteSelectedClips();
    void cutSelectedClips();
    void setContextMenu(QMenu *timeline, QMenu *clip, QMenu *transition, QActionGroup *clipTypeGroup, QMenu *markermenu);
    void checkTrackHeight();
    //QList <TrackInfo> tracksList() const;
    void setTool(PROJECTTOOL tool);
    ClipItem *cutClip(ItemInfo info, GenTime cutTime, bool cut, bool execute = true);
    void slotSeekToPreviousSnap();
    void slotSeekToNextSnap();
    double getSnapPointForPos(double pos);
    void editKeyFrame(const GenTime pos, const int track, const int index, const QString keyframes);
    bool findString(const QString &text);
    void selectFound(QString track, QString pos);
    bool findNextString(const QString &text);
    void initSearchStrings();
    void clearSearchStrings();
    QList<ItemInfo> findId(const QString &clipId);
    void clipStart();
    void clipEnd();
    void changeClipSpeed();
    void doChangeClipSpeed(ItemInfo info, ItemInfo speedIndependantInfo, const double speed, const double oldspeed, int strobe, const QString &id);
    void setDocumentModified();
    void setInPoint();
    void setOutPoint();
    /** @brief Prepares inserting space.
    *
    * Shows a dialog to configure length and track. */
    void slotInsertSpace();
    /** @brief Prepares removing space. */
    void slotRemoveSpace();
    void insertSpace(QList<ItemInfo> clipsToMove, QList<ItemInfo> transToMove, int track, const GenTime duration, const GenTime offset);
    ClipItem *getActiveClipUnderCursor(bool allowOutsideCursor = false) const;
    void deleteTimelineTrack(int ix, TrackInfo trackinfo);
    void changeTimelineTrack(int ix, TrackInfo trackinfo);
    void saveThumbnails();
    void autoTransition();
    QStringList getLadspaParams(QDomElement effect) const;
    void initCursorPos(int pos);
    /** @brief Locks or unlocks a track.
    * @param ix number of track
    * @param lock whether to lock or unlock
    *
    * Makes sure no clip on track to lock is selected. */
    void lockTrack(int ix, bool lock);
    void groupClips(bool group = true);
    void doGroupClips(QList <ItemInfo> clipInfos, QList <ItemInfo> transitionInfos, bool group);
    void loadGroups(const QDomNodeList groups);
    void splitAudio();
    void doSplitAudio(const GenTime &pos, int track, bool split);
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
    void selectClip(bool add, bool group = false);
    void selectTransition(bool add, bool group = false);
    QStringList extractTransitionsLumas();
    void setEditMode(EDITMODE mode);
    void insertClipCut(DocClipBase *clip, int in, int out);
    void clearSelection();
    void editItemDuration();
    void buildGuidesMenu(QMenu *goMenu) const;
    KPixmapCache* pixmapCache;

public slots:
    void setCursorPos(int pos, bool seek = true);
    void moveCursorPos(int delta);
    void updateCursorPos();
    void slotDeleteEffect(ClipItem *clip, QDomElement effect, bool affectGroup = true);
    void slotChangeEffectState(ClipItem *clip, int effectPos, bool disable);
    void slotChangeEffectPosition(ClipItem *clip, int currentPos, int newPos);
    void slotUpdateClipEffect(ClipItem *clip, QDomElement oldeffect, QDomElement effect, int ix);
    void slotRefreshEffects(ClipItem *clip);
    void setDuration(int duration);
    void slotAddTransition(ClipItem* clip, ItemInfo transitionInfo, int endTrack, QDomElement transition = QDomElement());
    void slotAddTransitionToSelectedClips(QDomElement transition);
    void slotTransitionUpdated(Transition *, QDomElement);
    void slotSwitchTrackAudio(int ix);
    void slotSwitchTrackVideo(int ix);
    void slotSwitchTrackLock(int ix);
    void slotUpdateClip(const QString &clipId, bool reload = true);
    void slotAddClipMarker(const QString &id, GenTime t, QString c);
    bool addGuide(const GenTime pos, const QString &comment);
    void slotAddGuide();
    void slotEditGuide(CommentedTime guide);
    void slotEditGuide(int guidePos = -1);
    void slotDeleteGuide(int guidePos = -1);
    void slotDeleteAllGuides();
    void editGuide(const GenTime oldPos, const GenTime pos, const QString &comment);
    void copyClip();
    void pasteClip();
    void pasteClipEffects();
    void slotUpdateAllThumbs();
    void slotCheckPositionScrolling();
    void slotInsertTrack(int ix);
    void slotDeleteTrack(int ix);
    void slotChangeTrack(int ix);
    void clipNameChanged(const QString id, const QString name);
    void slotTrackUp();
    void slotTrackDown();
    void slotSelectTrack(int ix);
    void insertZoneOverwrite(QStringList data, int in);

protected:
    virtual void drawBackground(QPainter * painter, const QRectF & rect);
    //virtual void drawForeground ( QPainter * painter, const QRectF & rect );
    virtual void dragEnterEvent(QDragEnterEvent * event);
    virtual void dragMoveEvent(QDragMoveEvent * event);
    virtual void dragLeaveEvent(QDragLeaveEvent * event);
    virtual void dropEvent(QDropEvent * event);
    virtual void wheelEvent(QWheelEvent * e);
    virtual void keyPressEvent(QKeyEvent * event);
    virtual QStringList mimeTypes() const;
    virtual Qt::DropActions supportedDropActions() const;

private:
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
    void updateSnapPoints(AbstractClipItem *selected, QList <GenTime> offsetList = QList <GenTime> (), bool skipSelectedItems = false);
    ClipItem *getClipItemAt(int pos, int track);
    ClipItem *getClipItemAt(GenTime pos, int track);
    ClipItem *getClipItemAtEnd(GenTime pos, int track);
    ClipItem *getClipItemAtStart(GenTime pos, int track);
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

    /** Get the index of the video track that is just below current track */
    int getPreviousVideoTrack(int track);
    void updatePositionEffects(ClipItem * item, ItemInfo info);
    bool insertDropClips(const QMimeData *data, const QPoint pos);
    bool canBePastedTo(ItemInfo info, int type) const;
    bool canBePastedTo(QList <ItemInfo> infoList, int type) const;
    bool canBePasted(QList<AbstractClipItem *> items, GenTime offset, int trackOffset) const;
    bool canBeMoved(QList<AbstractClipItem *> items, GenTime offset, int trackOffset) const;
    ClipItem *getClipUnderCursor() const;
    AbstractClipItem *getMainActiveClip() const;
    void resetSelectionGroup(bool selectItems = true);
    void groupSelectedItems(bool force = false, bool createNewGroup = false);
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

signals:
    void cursorMoved(int, int);
    void zoomIn();
    void zoomOut();
    void mousePosition(int);
    void clipItemSelected(ClipItem*, int ix = -1);
    void transitionItemSelected(Transition*, int track = 0, QPoint p = QPoint(), bool update = false);
    void activateDocumentMonitor();
    void trackHeightChanged();
    void tracksChanged();
    void displayMessage(const QString, MessageType);
    void showClipFrame(DocClipBase *, QPoint, const int);
    void doTrackLock(int, bool);
    void updateClipMarkers(DocClipBase *);
    void updateTrackHeaders();
    void playMonitor();
};

#endif

