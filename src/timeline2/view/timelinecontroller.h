/***************************************************************************
 *   Copyright (C) 2017 by Jean-Baptiste Mardelle                          *
 *   This file is part of Kdenlive. See www.kdenlive.org.                  *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) version 3 or any later version accepted by the       *
 *   membership of KDE e.V. (or its successor approved  by the membership  *
 *   of KDE e.V.), which shall act as a proxy defined in Section 14 of     *
 *   version 3 of the license.                                             *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>. *
 ***************************************************************************/

#ifndef TIMELINECONTROLLER_H
#define TIMELINECONTROLLER_H

#include "definitions.h"
#include "lib/audio/audioCorrelation.h"
#include "timeline2/model/timelineitemmodel.hpp"

#include <KActionCollection>
#include <QDir>

class PreviewManager;
class QAction;
class QQuickItem;

// see https://bugreports.qt.io/browse/QTBUG-57714, don't expose a QWidget as a context property
class TimelineController : public QObject
{
    Q_OBJECT
    /* @brief holds a list of currently selected clips (list of clipId's)
     */
    Q_PROPERTY(QList<int> selection READ selection NOTIFY selectionChanged)
    /* @brief holds the timeline zoom factor
     */
    Q_PROPERTY(double scaleFactor READ scaleFactor WRITE setScaleFactor NOTIFY scaleFactorChanged)
    /* @brief holds the current project duration
     */
    Q_PROPERTY(int duration READ duration NOTIFY durationChanged)
    Q_PROPERTY(int fullDuration READ fullDuration NOTIFY durationChanged)
    Q_PROPERTY(bool audioThumbFormat READ audioThumbFormat NOTIFY audioThumbFormatChanged)
    Q_PROPERTY(int zoneIn READ zoneIn WRITE setZoneIn NOTIFY zoneChanged)
    Q_PROPERTY(int zoneOut READ zoneOut WRITE setZoneOut NOTIFY zoneChanged)
    Q_PROPERTY(bool ripple READ ripple NOTIFY rippleChanged)
    Q_PROPERTY(bool scrub READ scrub NOTIFY scrubChanged)
    Q_PROPERTY(bool snap READ snap NOTIFY snapChanged)
    Q_PROPERTY(bool showThumbnails READ showThumbnails NOTIFY showThumbnailsChanged)
    Q_PROPERTY(bool showMarkers READ showMarkers NOTIFY showMarkersChanged)
    Q_PROPERTY(bool showAudioThumbnails READ showAudioThumbnails NOTIFY showAudioThumbnailsChanged)
    Q_PROPERTY(QVariantList dirtyChunks READ dirtyChunks NOTIFY dirtyChunksChanged)
    Q_PROPERTY(QVariantList renderedChunks READ renderedChunks NOTIFY renderedChunksChanged)
    Q_PROPERTY(int workingPreview READ workingPreview NOTIFY workingPreviewChanged)
    Q_PROPERTY(bool useRuler READ useRuler NOTIFY useRulerChanged)
    Q_PROPERTY(int activeTrack READ activeTrack WRITE setActiveTrack NOTIFY activeTrackChanged)
    Q_PROPERTY(QVariantList audioTarget READ audioTarget NOTIFY audioTargetChanged)
    Q_PROPERTY(int videoTarget READ videoTarget WRITE setVideoTarget NOTIFY videoTargetChanged)

    Q_PROPERTY(QVariantList lastAudioTarget READ lastAudioTarget  NOTIFY lastAudioTargetChanged)
    Q_PROPERTY(int lastVideoTarget MEMBER m_lastVideoTarget NOTIFY lastVideoTargetChanged)

    Q_PROPERTY(int hasAudioTarget READ hasAudioTarget NOTIFY hasAudioTargetChanged)
    Q_PROPERTY(bool hasVideoTarget READ hasVideoTarget NOTIFY hasVideoTargetChanged)
    Q_PROPERTY(int clipTargets READ clipTargets NOTIFY hasAudioTargetChanged)
    Q_PROPERTY(bool autoScroll READ autoScroll NOTIFY autoScrollChanged)
    Q_PROPERTY(QColor videoColor READ videoColor NOTIFY colorsChanged)
    Q_PROPERTY(QColor audioColor READ audioColor NOTIFY colorsChanged)
    Q_PROPERTY(QColor titleColor READ titleColor NOTIFY colorsChanged)
    Q_PROPERTY(QColor imageColor READ imageColor NOTIFY colorsChanged)
    Q_PROPERTY(QColor thumbColor1 READ thumbColor1 NOTIFY colorsChanged)
    Q_PROPERTY(QColor thumbColor2 READ thumbColor2 NOTIFY colorsChanged)
    Q_PROPERTY(QColor slideshowColor READ slideshowColor NOTIFY colorsChanged)
    Q_PROPERTY(QColor targetColor READ targetColor NOTIFY colorsChanged)
    Q_PROPERTY(QColor targetTextColor READ targetTextColor NOTIFY colorsChanged)
    Q_PROPERTY(QColor lockedColor READ lockedColor NOTIFY colorsChanged)
    Q_PROPERTY(QColor selectionColor READ selectionColor NOTIFY colorsChanged)
    Q_PROPERTY(QColor groupColor READ groupColor NOTIFY colorsChanged)

public:
    TimelineController(QObject *parent);
    ~TimelineController() override;
    /** @brief Sets the model that this widgets displays */
    void setModel(std::shared_ptr<TimelineItemModel> model);
    std::shared_ptr<TimelineItemModel> getModel() const;
    void setRoot(QQuickItem *root);
    /** @brief Edit an item's in/out points with a dialog
     */
    Q_INVOKABLE void editItemDuration(int itemId = -1);

    /** @brief Returns the topmost track containing a selected item (-1 if selection is embty) */
    Q_INVOKABLE int selectedTrack() const;

    /** @brief Select the clip in active track under cursor
        @param type is the type of the object (clip or composition)
        @param select: true if the object should be selected and false if it should be deselected
        @param addToCurrent: if true, the object will be added to the new selection
    */
    void selectCurrentItem(ObjectType type, bool select, bool addToCurrent = false);

    /** @brief Select all timeline items
     */
    void selectAll();
    /* @brief Select all items in one track
     */
    void selectCurrentTrack();
    /** @brief Select multiple objects on the timeline
        @param tracks List of ids of tracks from which to select
        @param start/endFrame Interval from which to select the items
        @param addToSelect if true, the old selection is retained
    */
    Q_INVOKABLE void selectItems(const QVariantList &tracks, int startFrame, int endFrame, bool addToSelect, bool selectBottomCompositions);

    /** @brief request a selection with a list of ids*/
    Q_INVOKABLE void selectItems(const QList<int> &ids);

    /* @brief Returns true is item is selected as well as other items */
    Q_INVOKABLE bool isInSelection(int itemId);

    /* @brief Show/hide audio record controls on a track
     */
    Q_INVOKABLE void switchRecording(int trackId);
    /* @brief Add recorded file to timeline
     */
    void finishRecording(const QString &recordedFile);
    /* @brief Open Kdenlive's config diablog on a defined page and tab
     */
    Q_INVOKABLE void showConfig(int page, int tab);

    /* @brief Returns true if we have at least one active track
     */
    Q_INVOKABLE bool hasActiveTracks() const;

    /* @brief returns current timeline's zoom factor
     */
    Q_INVOKABLE double scaleFactor() const;
    /* @brief set current timeline's zoom factor
     */
    void setScaleFactorOnMouse(double scale, bool zoomOnMouse);
    void setScaleFactor(double scale);
    /* @brief Returns the project's duration (tractor)
     */
    Q_INVOKABLE int duration() const;
    Q_INVOKABLE int fullDuration() const;
    /* @brief Returns the current cursor position (frame currently displayed by MLT)
     */
    /* @brief Returns the seek request position (-1 = no seek pending)
     */
    Q_INVOKABLE QVariantList audioTarget() const;
    Q_INVOKABLE QVariantList lastAudioTarget() const;
    Q_INVOKABLE const QString audioTargetName(int tid) const;
    Q_INVOKABLE int videoTarget() const;
    Q_INVOKABLE int hasAudioTarget() const;
    Q_INVOKABLE int clipTargets() const;
    Q_INVOKABLE bool hasVideoTarget() const;
    Q_INVOKABLE bool autoScroll() const;
    Q_INVOKABLE int activeTrack() const { return m_activeTrack; }
    Q_INVOKABLE QColor videoColor() const;
    Q_INVOKABLE QColor audioColor() const;
    Q_INVOKABLE QColor titleColor() const;
    Q_INVOKABLE QColor imageColor() const;
    Q_INVOKABLE QColor thumbColor1() const;
    Q_INVOKABLE QColor thumbColor2() const;
    Q_INVOKABLE QColor slideshowColor() const;
    Q_INVOKABLE QColor targetColor() const;
    Q_INVOKABLE QColor targetTextColor() const;
    Q_INVOKABLE QColor lockedColor() const;
    Q_INVOKABLE QColor selectionColor() const;
    Q_INVOKABLE QColor groupColor() const;
    /* @brief Request a seek operation
       @param position is the desired new timeline position
     */
    int zoneIn() const { return m_zone.x(); }
    int zoneOut() const { return m_zone.y(); }
    void setZoneIn(int inPoint);
    void setZoneOut(int outPoint);
    void setZone(const QPoint &zone, bool withUndo = true);
    /* @brief Request a seek operation
       @param position is the desired new timeline position
     */
    Q_INVOKABLE void setPosition(int position);
    Q_INVOKABLE bool snap();
    Q_INVOKABLE bool ripple();
    Q_INVOKABLE bool scrub();
    Q_INVOKABLE QString timecode(int frames) const;
    QString framesToClock(int frames) const;
    Q_INVOKABLE QString simplifiedTC(int frames) const;
    /* @brief Request inserting a new clip in timeline (dragged from bin or monitor)
       @param tid is the destination track
       @param position is the timeline position
       @param xml is the data describing the dropped clip
       @param logUndo if set to false, no undo object is stored
       @return the id of the inserted clip
     */
    Q_INVOKABLE int insertClip(int tid, int position, const QString &xml, bool logUndo, bool refreshView, bool useTargets);
    /* @brief Request inserting multiple clips into the timeline (dragged from bin or monitor)
     * @param tid is the destination track
     * @param position is the timeline position
     * @param binIds the IDs of the bins being dropped
     * @param logUndo if set to false, no undo object is stored
     * @return the ids of the inserted clips
     */
    Q_INVOKABLE QList<int> insertClips(int tid, int position, const QStringList &binIds, bool logUndo, bool refreshView);
    Q_INVOKABLE void copyItem();
    Q_INVOKABLE bool pasteItem(int position = -1, int tid = -1);
    /* @brief Request inserting a new composition in timeline (dragged from compositions list)
       @param tid is the destination track
       @param position is the timeline position
       @param transitionId is the data describing the dropped composition
       @param logUndo if set to false, no undo object is stored
       @return the id of the inserted composition
    */
    Q_INVOKABLE int insertComposition(int tid, int position, const QString &transitionId, bool logUndo);
    /* @brief Request inserting a new composition in timeline (dragged from compositions list)
       this function will check if there is a clip at insert point and
       adjust the composition length accordingly
       @param tid is the destination track
       @param position is the timeline position
       @param transitionId is the data describing the dropped composition
       @param logUndo if set to false, no undo object is stored
       @return the id of the inserted composition
    */
    Q_INVOKABLE int insertNewComposition(int tid, int position, const QString &transitionId, bool logUndo);
    Q_INVOKABLE int insertNewComposition(int tid, int clipId, int offset, const QString &transitionId, bool logUndo);

    /* @brief Request deletion of the currently selected clips
     */
    Q_INVOKABLE void deleteSelectedClips();

    Q_INVOKABLE void triggerAction(const QString &name);
    Q_INVOKABLE const QString actionText(const QString &name);

    /* @brief Returns id of the timeline selcted clip if there is only 1 clip selected
     * or an AVSplit group. If allowComposition is true, returns composition id if 
     * only 1 is selected, otherwise returns -1. If restrictToCurrentPos is true, it will
     * only return the id if timeline cursor is inside item
     */
    int getMainSelectedItem(bool restrictToCurrentPos = true, bool allowComposition = false);
    int getMainSelectedClip() const;

    /* @brief Do we want to display video thumbnails
     */
    bool showThumbnails() const;
    bool showAudioThumbnails() const;
    bool showMarkers() const;
    bool audioThumbFormat() const;
    /* @brief Do we want to display audio thumbnails
     */
    Q_INVOKABLE bool showWaveforms() const;
    /* @brief Insert a timeline track
     */
    Q_INVOKABLE void addTrack(int tid);
    /* @brief Remove a timeline track
     */
    Q_INVOKABLE void deleteTrack(int tid);
    /* @brief Show / hide audio rec controls in active track
     */
    void switchTrackRecord(int tid = -1);
    /* @brief Group selected items in timeline
     */
    Q_INVOKABLE void groupSelection();
    /* @brief Ungroup selected items in timeline
     */
    Q_INVOKABLE void unGroupSelection(int cid = -1);
    /* @brief Ask for edit marker dialog
     */
    Q_INVOKABLE void editMarker(int cid = -1, int position = -1);
    /* @brief Ask for marker add dialog
     */
    Q_INVOKABLE void addMarker(int cid = -1, int position = -1);
    /* @brief Ask for quick marker add (without dialog)
     */
    Q_INVOKABLE void addQuickMarker(int cid = -1, int position = -1);
    /* @brief Ask for marker delete
     */
    Q_INVOKABLE void deleteMarker(int cid = -1, int position = -1);
    /* @brief Ask for all markers delete
     */
    Q_INVOKABLE void deleteAllMarkers(int cid = -1);
    /* @brief Ask for edit timeline guide dialog
     */
    Q_INVOKABLE void editGuide(int frame = -1);
    Q_INVOKABLE void moveGuide(int frame, int newFrame);
    /* @brief Add a timeline guide
     */
    Q_INVOKABLE void switchGuide(int frame = -1, bool deleteOnly = false);
    /* @brief Request monitor refresh
     */
    Q_INVOKABLE void requestRefresh();

    /* @brief Show the asset of the given item in the AssetPanel
       If the id corresponds to a clip, we show the corresponding effect stack
       If the id corresponds to a composition, we show its properties
    */
    Q_INVOKABLE void showAsset(int id);
    Q_INVOKABLE void showTrackAsset(int trackId);
    /* @brief Adjust height of all similar (audio or video) tracks
    */
    Q_INVOKABLE void adjustAllTrackHeight(int trackId, int height);
    Q_INVOKABLE void collapseAllTrackHeight(int trackId, bool collapse, int collapsedHeight);

    /** @brief Reset track @trackId height to default track height. Adjusts all tracks if @trackId == -1
    */
    Q_INVOKABLE void defaultTrackHeight(int trackId);

    Q_INVOKABLE bool exists(int itemId);

    Q_INVOKABLE int headerWidth() const;
    Q_INVOKABLE void setHeaderWidth(int width);

    /* @brief Seek to next snap point
     */
    void gotoNextSnap();
    /* @brief Seek to previous snap point
     */
    void gotoPreviousSnap();
    /* @brief Seek to previous guide
     */
    void gotoPreviousGuide();
    /* @brief Seek to next guide
     */
    void gotoNextGuide();

    /* @brief Set current item's start point to cursor position
     */
    void setInPoint();
    /* @brief Set current item's end point to cursor position
     */
    void setOutPoint();
    /* @brief Return the project's tractor
     */
    Mlt::Tractor *tractor();
    /* @brief Return a track's producer
     */
    Mlt::Producer trackProducer(int tid);
    /* @brief Get the list of currently selected clip id's
     */
    QList<int> selection() const;

    /* @brief Add an asset (effect, composition)
     */
    void addAsset(const QVariantMap &data);

    /* @brief Cuts the clip on current track at timeline position
     */
    Q_INVOKABLE void cutClipUnderCursor(int position = -1, int track = -1);
    /* @brief Cuts all clips at timeline position
     */
    Q_INVOKABLE void cutAllClipsUnderCursor(int position = -1);
    /* @brief Request a spacer operation
     */
    Q_INVOKABLE int requestSpacerStartOperation(int trackId, int position);
    /* @brief Request a spacer operation
     */
    Q_INVOKABLE bool requestSpacerEndOperation(int clipId, int startPosition, int endPosition, int affectedTrack);
    /* @brief Request a Fade in effect for clip
     */
    Q_INVOKABLE void adjustFade(int cid, const QString &effectId, int duration, int initialDuration);

    Q_INVOKABLE const QString getTrackNameFromMltIndex(int trackPos);
    /* @brief Request inserting space in a track
     */
    Q_INVOKABLE void insertSpace(int trackId = -1, int frame = -1);
    Q_INVOKABLE void removeSpace(int trackId = -1, int frame = -1, bool affectAllTracks = false);
    /* @brief If clip is enabled, disable, otherwise enable
     */
    Q_INVOKABLE void switchEnableState(std::unordered_set<int> selection = {});
    Q_INVOKABLE void addCompositionToClip(const QString &assetId, int clipId = -1, int offset = -1);
    Q_INVOKABLE void addEffectToClip(const QString &assetId, int clipId = -1);

    Q_INVOKABLE void requestClipCut(int clipId, int position);

    /** @brief Extract (delete + remove space) current clip
     */
    void extract(int clipId = -1);
    /** @brief Save current clip cut as bin subclip
     */
    void saveZone(int clipId = -1);

    Q_INVOKABLE void splitAudio(int clipId);
    Q_INVOKABLE void splitVideo(int clipId);
    Q_INVOKABLE void setAudioRef(int clipId = -1);
    Q_INVOKABLE void alignAudio(int clipId = -1);
    Q_INVOKABLE void urlDropped(QStringList droppedFile, int frame, int tid);

    Q_INVOKABLE bool endFakeMove(int clipId, int position, bool updateView, bool logUndo, bool invalidateTimeline);
    Q_INVOKABLE int getItemMovingTrack(int itemId) const;
    bool endFakeGroupMove(int clipId, int groupId, int delta_track, int delta_pos, bool updateView, bool logUndo);
    bool endFakeGroupMove(int clipId, int groupId, int delta_track, int delta_pos, bool updateView, bool finalMove, Fun &undo, Fun &redo);

    bool splitAV();

    /* @brief Seeks to selected clip start / end
     */
    Q_INVOKABLE void pasteEffects(int targetId = -1);
    Q_INVOKABLE double fps() const;
    Q_INVOKABLE void addEffectKeyframe(int cid, int frame, double val);
    Q_INVOKABLE void removeEffectKeyframe(int cid, int frame);
    Q_INVOKABLE void updateEffectKeyframe(int cid, int oldFrame, int newFrame, const QVariant &normalizedValue = QVariant());
    Q_INVOKABLE bool hasKeyframeAt(int cid, int frame);

    /** @brief Make current timeline track active/inactive*/
    Q_INVOKABLE void switchTrackActive(int trackId = -1);
    /** @brief Toogle the active/inactive state of all tracks*/
    void switchAllTrackActive();
    /** @brief Make all tracks active or inactive */
    void makeAllTrackActive();
    void switchTrackLock(bool applyToAll = false);
    void switchTargetTrack();

    const QString getTrackNameFromIndex(int trackIndex);
    /* @brief Seeks to selected clip start / end
     */
    void seekCurrentClip(bool seekToEnd = false);
    /* @brief Seeks to a clip start (or end) based on it's clip id
     */
    void seekToClip(int cid, bool seekToEnd);
    /* @brief Returns true if timeline cursor is inside the item
     */
    bool positionIsInItem(int id);
    /* @brief Returns the number of tracks (audioTrakcs, videoTracks)
     */
    QPair<int, int>getTracksCount() const;
    /* @brief Request monitor refresh if item (clip or composition) is under timeline cursor
     */
    void refreshItem(int id);
    /* @brief Seek timeline to mouse position
     */
    void seekToMouse();

    /* @brief Set a property on the active track
     */
    void setActiveTrackProperty(const QString &name, const QString &value);
    /* @brief Get a property on the active track
     */
    const QVariant getActiveTrackProperty(const QString &name) const;
    /* @brief Is the active track audio
     */
    bool isActiveTrackAudio() const;

    /* @brief Returns a list of all luma files used in the project
     */
    QStringList extractCompositionLumas() const;
    /* @brief Get the frame where mouse is positioned
     */
    int getMousePos();
    /* @brief Get the frame where mouse is positioned
     */
    int getMouseTrack();
    /* @brief Returns a map of track ids/track names
     */
    QMap<int, QString> getTrackNames(bool videoOnly);
    /* @brief Returns the transition a track index for a composition (MLT index / Track id)
     */
    QPair<int, int> getCompositionATrack(int cid) const;
    void setCompositionATrack(int cid, int aTrack);
    /* @brief Return true if composition's a_track is automatic (no forced track)
     */
    bool compositionAutoTrack(int cid) const;
    const QString getClipBinId(int clipId) const;
    void focusItem(int itemId);
    /* @brief Create and display a split clip view to compare effect
     */
    bool createSplitOverlay(int clipId, std::shared_ptr<Mlt::Filter> filter);
    /* @brief Delete the split clip view to compare effect
     */
    void removeSplitOverlay();
    /* @brief Add current timeline zone to preview rendering
     */
    void addPreviewRange(bool add);
    /* @brief Clear current timeline zone from preview rendering
     */
    void clearPreviewRange(bool resetZones);
    void startPreviewRender();
    void stopPreviewRender();
    QVariantList dirtyChunks() const;
    QVariantList renderedChunks() const;
    /* @brief returns the frame currently processed by timeline preview, -1 if none
     */
    int workingPreview() const;

    /** @brief Return true if we want to use timeline ruler zone for editing */
    bool useRuler() const;
    /* @brief Load timeline preview from saved doc
     */
    void loadPreview(const QString &chunks, const QString &dirty, const QDateTime &documentDate, int enable);
    /* @brief Return document properties with added settings from timeline
     */
    QMap<QString, QString> documentProperties();

    /** @brief Change track compsiting mode */
    void switchCompositing(int mode);

    /** @brief Change a clip item's speed in timeline */
    Q_INVOKABLE void changeItemSpeed(int clipId, double speed);
    /** @brief Delete selected zone and fill gap by moving following clips
     *  @param lift if true, the zone will simply be deleted but clips won't be moved
     */
    void extractZone(QPoint zone, bool liftOnly = false);
    /** @brief Insert clip monitor into timeline
     *  @returns the zone end position or -1 on fail
     */
    Q_INVOKABLE bool insertClipZone(const QString &binId, int tid, int pos);
    int insertZone(const QString &binId, QPoint zone, bool overwrite);
    void updateClip(int clipId, const QVector<int> &roles);
    void showClipKeyframes(int clipId, bool value);
    void showCompositionKeyframes(int clipId, bool value);
    /** @brief Adjust all timeline tracks height */
    void resetTrackHeight();
    /** @brief timeline preview params changed, reset */
    void resetPreview();
    /** @brief Set target tracks (video, audio) */
    void setTargetTracks(bool hasVideo, QMap <int, QString> audioTargets);
    /** @brief Restore Bin Clip original target tracks (video, audio) */
    void restoreTargetTracks();
    /** @brief Return asset's display name from it's id (effect or composition) */
    Q_INVOKABLE const QString getAssetName(const QString &assetId, bool isTransition);
    /** @brief Set keyboard grabbing on current selection */
    Q_INVOKABLE void grabCurrent();
    /** @brief Returns keys for all used thumbnails */
    QStringList getThumbKeys();
    /** @brief Returns true if a drag operation is currently running in timeline */
    bool dragOperationRunning();
    /** @brief Disconnect some stuff before closing project */
    void prepareClose();
    /** @brief Check that we don't keep a deleted track id */
    void checkTrackDeletion(int selectedTrackIx);
    /** @brief Return true if an overlay track is used */
    bool hasPreviewTrack() const;
    void updatePreviewConnection(bool enable);
    /** @brief Display project master effects */
    Q_INVOKABLE void showMasterEffects();
    /** @brief Return true if an instance of this bin clip is currently undet timeline cursor */
    bool refreshIfVisible(int cid);
    /** @brief Collapse / expand active track */
    void collapseActiveTrack();
    /** @brief Expand MLT playlist to its contained clips/compositions */
    void expandActiveClip();
    /** @brief Retrieve a list of possible audio stream targets */
    QMap <int, QString> getCurrentTargets(int trackId, int &activeTargetStream);
    /** @brief Define audio stream target for a track index */
    void assignAudioTarget(int trackId, int stream);
    /** @brief Define a stream target for current track from the stream index */
    void assignCurrentTarget(int index);
    /** @brief Get the first unassigned target audio stream. */
    int getFirstUnassignedStream() const;

    /** @brief Add tracks to project */
    void addTracks(int videoTracks, int audioTracks);
    /** @brief Get in/out of currently selected items */
    QPoint selectionInOut() const;

public slots:
    void resetView();
    void setAudioTarget(QMap<int, int> tracks);
    Q_INVOKABLE void switchAudioTarget(int trackId);
    Q_INVOKABLE void setVideoTarget(int track);
    Q_INVOKABLE void setActiveTrack(int track);
    void addEffectToCurrentClip(const QStringList &effectData);
    /** @brief Dis / enable timeline preview. */
    void disablePreview(bool disable);
    void invalidateItem(int cid);
    void invalidateTrack(int tid);
    void invalidateZone(int in, int out);
    void checkDuration();
    /** @brief Dis / enable multi track view. */
    void slotMultitrackView(bool enable = true, bool refresh = true);
    /** @brief Activate a video track by its position (0 = topmost). */
    void activateTrackAndSelect(int trackPosition);
    /** @brief Save timeline selected clips to target folder. */
    void saveTimelineSelection(const QDir &targetDir);
    /** @brief Restore timeline scroll pos on open. */
    void setScrollPos(int pos);
    /** @brief change zone info with undo. */
    Q_INVOKABLE void updateZone(const QPoint oldZone, const QPoint newZone, bool withUndo = true);

private slots:
    void updateClipActions();
    void updateVideoTarget();
    void updateAudioTarget();
    /** @brief Dis / enable multi track view. */
    void updateMultiTrack();

public:
    /** @brief a list of actions that have to be enabled/disabled depending on the timeline selection */
    QList<QAction *> clipActions;

private:
    QQuickItem *m_root;
    KActionCollection *m_actionCollection;
    std::shared_ptr<TimelineItemModel> m_model;
    bool m_usePreview;
    int m_audioTarget;
    int m_videoTarget;
    int m_activeTrack;
    int m_audioRef;
    int m_hasAudioTarget {0};
    bool m_hasVideoTarget {false};
    int m_lastVideoTarget {-1};
    /** @brief The last combination of audio targets in the form: {timeline track id, bin stream index} */
    QMap <int, int> m_lastAudioTarget;
    bool m_videoTargetActive {true};
    bool m_audioTargetActive {true};
    QPair<int, int> m_recordStart;
    int m_recordTrack;
    QPoint m_zone;
    double m_scale;
    static int m_duration;
    PreviewManager *m_timelinePreview;
    QAction *m_disablePreview;
    std::shared_ptr<AudioCorrelation> m_audioCorrelator;
    QMutex m_metaMutex;
    bool m_ready;
    std::vector<int> m_activeSnaps;
    int m_snapStackIndex;
    QMetaObject::Connection m_connection;
    QMetaObject::Connection m_deleteConnection;

    void initializePreview();
    bool darkBackground() const;
    int getMenuOrTimelinePos() const;

signals:
    void selected(Mlt::Producer *producer);
    void selectionChanged();
    void frameFormatChanged();
    void trackHeightChanged();
    void scaleFactorChanged();
    void audioThumbFormatChanged();
    void durationChanged();
    void audioTargetChanged();
    void videoTargetChanged();
    void hasAudioTargetChanged();
    void hasVideoTargetChanged();
    void lastAudioTargetChanged();
    void autoScrollChanged();
    void lastVideoTargetChanged();
    void activeTrackChanged();
    void colorsChanged();
    void showThumbnailsChanged();
    void showAudioThumbnailsChanged();
    void showMarkersChanged();
    void rippleChanged();
    void scrubChanged();
    void seeked(int position);
    void zoneChanged();
    void zoneMoved(const QPoint &zone);
    /* @brief Requests that a given parameter model is displayed in the asset panel */
    void showTransitionModel(int tid, std::shared_ptr<AssetParameterModel>);
    void showItemEffectStack(const QString &clipName, std::shared_ptr<EffectStackModel>, QSize frameSize, bool showKeyframes);
    /* @brief notify of chunks change
     */
    void dirtyChunksChanged();
    void renderedChunksChanged();
    void workingPreviewChanged();
    void useRulerChanged();
    void updateZoom(double);
    /* @brief emitted when timeline selection changes, true if a clip is selected
     */
    void timelineClipSelected(bool);
    /* @brief User enabled / disabled snapping, update timeline behavior
     */
    void snapChanged();
    Q_INVOKABLE void ungrabHack();
};

#endif
