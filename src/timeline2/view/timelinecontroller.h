/*
    SPDX-FileCopyrightText: 2017 Jean-Baptiste Mardelle
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include "definitions.h"
#include "lib/audio/audioCorrelation.h"
#include "timeline2/model/timelineitemmodel.hpp"

#include <KActionCollection>
#include <QApplication>
#include <QDir>

class QAction;
class QQuickItem;

// see https://bugreports.qt.io/browse/QTBUG-57714, don't expose a QWidget as a context property
class TimelineController : public QObject
{
    Q_OBJECT
    /** @brief holds a list of currently selected clips (list of clipId's)
     */
    Q_PROPERTY(QList<int> selection READ selection NOTIFY selectionChanged)
    Q_PROPERTY(int selectedMix READ selectedMix NOTIFY selectedMixChanged)
    /** @brief holds the timeline zoom factor
     */
    Q_PROPERTY(double scaleFactor READ scaleFactor WRITE setScaleFactor NOTIFY scaleFactorChanged)
    /** @brief holds the current project duration
     */
    Q_PROPERTY(int duration READ duration NOTIFY durationChanged)
    Q_PROPERTY(int fullDuration READ fullDuration NOTIFY durationChanged)
    Q_PROPERTY(bool audioThumbFormat READ audioThumbFormat NOTIFY audioThumbFormatChanged)
    Q_PROPERTY(bool audioThumbNormalize READ audioThumbNormalize NOTIFY audioThumbNormalizeChanged)
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
    Q_PROPERTY(QVariantList masterEffectZones MEMBER m_masterEffectZones NOTIFY masterZonesChanged)
    Q_PROPERTY(int workingPreview READ workingPreview NOTIFY workingPreviewChanged)
    Q_PROPERTY(bool useRuler READ useRuler NOTIFY useRulerChanged)
    Q_PROPERTY(bool scrollVertically READ scrollVertically NOTIFY scrollVerticallyChanged)
    Q_PROPERTY(int activeTrack READ activeTrack WRITE setActiveTrack NOTIFY activeTrackChanged)
    Q_PROPERTY(QVariantList subtitlesList READ subtitlesList NOTIFY subtitlesListChanged)
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
    Q_PROPERTY(bool subtitlesWarning READ subtitlesWarning NOTIFY subtitlesWarningChanged)
    Q_PROPERTY(bool subtitlesDisabled READ subtitlesDisabled NOTIFY subtitlesDisabledChanged)
    Q_PROPERTY(bool subtitlesLocked READ subtitlesLocked NOTIFY subtitlesLockedChanged)
    Q_PROPERTY(int activeSubPosition MEMBER m_activeSubPosition NOTIFY activeSubtitlePositionChanged)
    Q_PROPERTY(bool guidesLocked READ guidesLocked NOTIFY guidesLockedChanged)
    Q_PROPERTY(bool autotrackHeight MEMBER m_autotrackHeight NOTIFY autotrackHeightChanged)
    Q_PROPERTY(QPoint effectZone MEMBER m_effectZone NOTIFY effectZoneChanged)
    Q_PROPERTY(int trimmingMainClip READ trimmingMainClip NOTIFY trimmingMainClipChanged)
    Q_PROPERTY(int multicamIn MEMBER multicamIn NOTIFY multicamInChanged)

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
    /** @brief Edit a title clip with a title widget
     */
    Q_INVOKABLE void editTitleClip(int itemId = -1);
    /** @brief Open a sequence timeline
     */
    Q_INVOKABLE void focusTimelineSequence(int id);
    /** @brief Edit an animation with Glaxnimate
     */
    Q_INVOKABLE void editAnimationClip(int itemId = -1);

    /** @brief Returns the topmost track containing a selected item (-1 if selection is embty) */
    Q_INVOKABLE int selectedTrack() const;

    /** @brief Select the clip in active track under cursor
        @param type is the type of the object (clip or composition)
        @param select: true if the object should be selected and false if it should be deselected
        @param addToCurrent: if true, the object will be added to the new selection
        @param showErrorMsg inform the user that no item was selected
        @return false if no item was found under timeline cursor in active track
    */
    bool selectCurrentItem(KdenliveObjectType type, bool select, bool addToCurrent = false, bool showErrorMsg = true);

    /** @brief Select all timeline items
     */
    void selectAll();
    /** @brief Select all items in one track
     */
    void selectCurrentTrack();
    /** @brief Select multiple objects on the timeline
        @param tracks List of ids of tracks from which to select
        @param start/endFrame Interval from which to select the items
        @param addToSelect if true, the old selection is retained
    */
    Q_INVOKABLE void selectItems(const QVariantList &tracks, int startFrame, int endFrame, bool addToSelect, bool selectBottomCompositions, bool selectSubTitles);

    /** @brief request a selection with a list of ids*/
    Q_INVOKABLE void selectItems(const QList<int> &ids);

    /** @brief Returns true is item is selected as well as other items */
    Q_INVOKABLE bool isInSelection(int itemId);

    /** @brief Show/hide audio record controls on a track
     */
    Q_INVOKABLE void switchRecording(int trackId, bool record);
    /** @brief Add recorded file to timeline
     */
    void finishRecording(const QString &recordedFile);
    /** @brief Add given file to bin, and then insert it at current point in timeline
        @param highlightClip If true, highlights the newly created clip in the bin as well
     */
    void addAndInsertFile(const QString &recordedFile, const bool isAudioClip, const bool highlightClip);
    /** @brief Open Kdenlive's config diablog on a defined page and tab
     */
    Q_INVOKABLE void showConfig(int page, int tab);

    /** @brief Returns true if we have at least one active track
     */
    Q_INVOKABLE bool hasActiveTracks() const;

    /** @brief returns current timeline's zoom factor
     */
    Q_INVOKABLE double scaleFactor() const;
    /** @brief set current timeline's zoom factor
     */
    void setScaleFactorOnMouse(double scale, bool zoomOnMouse);
    void setScaleFactor(double scale);
    /** @brief Returns the project's duration (tractor)
     */
    Q_INVOKABLE int duration() const;
    Q_INVOKABLE int fullDuration() const;
    /** @brief Returns the current cursor position (frame currently displayed by MLT)
     */
    /** @brief Returns the seek request position (-1 = no seek pending)
     */
    Q_INVOKABLE QVariantList audioTarget() const;
    Q_INVOKABLE QVariantList lastAudioTarget() const;
    Q_INVOKABLE const QString audioTargetName(int tid) const;
    Q_INVOKABLE int videoTarget() const;
    Q_INVOKABLE int hasAudioTarget() const;
    Q_INVOKABLE int clipTargets() const;
    Q_INVOKABLE bool hasVideoTarget() const;
    bool autoScroll() const;
    Q_INVOKABLE int activeTrack() const { return m_activeTrack; }
    Q_INVOKABLE int trimmingMainClip() const { return m_trimmingMainClip; }
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
    Q_INVOKABLE int doubleClickInterval() const { return QApplication::doubleClickInterval(); }
    Q_INVOKABLE void showToolTip(const QString &info = QString()) const;
    Q_INVOKABLE void showKeyBinding(const QString &info = QString()) const;
    Q_INVOKABLE void showTimelineToolInfo(bool show) const;
    /** @brief The model list for this timeline's subtitles */
    Q_INVOKABLE QVariantList subtitlesList() const;
    /** @brief Returns true if the avfilter.subtiles filter is not found */
    bool subtitlesWarning() const;
    Q_INVOKABLE void subtitlesWarningDetails();
    void switchSubtitleDisable();
    bool subtitlesDisabled() const;
    void switchSubtitleLock();
    bool subtitlesLocked() const;
    bool guidesLocked() const;
    int zoneIn() const { return m_zone.x(); }
    int zoneOut() const { return m_zone.y(); }
    void setZoneIn(int inPoint);
    void setZoneOut(int outPoint);
    void setZone(const QPoint &zone, bool withUndo = true);
    /** @brief Adjust timeline zone to current selection */
    void setZoneToSelection();
    /** @brief Request a seek operation
       @param position is the desired new timeline position
     */
    Q_INVOKABLE void setPosition(int position);
    Q_INVOKABLE bool snap();
    Q_INVOKABLE bool ripple();
    Q_INVOKABLE bool scrub();
    Q_INVOKABLE QString timecode(int frames) const;
    QString framesToClock(int frames) const;
    Q_INVOKABLE QString simplifiedTC(int frames) const;
    /** @brief Request inserting a new clip in timeline (dragged from bin or monitor)
       @param tid is the destination track
       @param position is the timeline position
       @param xml is the data describing the dropped clip
       @param logUndo if set to false, no undo object is stored
       @return the id of the inserted clip
     */
    Q_INVOKABLE int insertClip(int tid, int position, const QString &xml, bool logUndo, bool refreshView, bool useTargets);
    /** @brief Request inserting multiple clips into the timeline (dragged from bin or monitor)
     * @param tid is the destination track
     * @param position is the timeline position
     * @param binIds the IDs of the bins being dropped
     * @param logUndo if set to false, no undo object is stored
     * @return the ids of the inserted clips
     */
    Q_INVOKABLE QList<int> insertClips(int tid, int position, const QStringList &binIds, bool logUndo, bool refreshView);
    Q_INVOKABLE int copyItem();
    std::pair<int, QString> getCopyItemData();
    Q_INVOKABLE bool pasteItem(int position = -1, int tid = -1);
    /** @brief Request inserting a new composition in timeline (dragged from compositions list)
       @param tid is the destination track
       @param position is the timeline position
       @param transitionId is the data describing the dropped composition
       @param logUndo if set to false, no undo object is stored
       @return the id of the inserted composition
    */
    Q_INVOKABLE int insertComposition(int tid, int position, const QString &transitionId, bool logUndo);
    /** @brief Request inserting a new mix in timeline (dragged from compositions list)
       @param tid is the destination track
       @param position is the timeline position (clip start of the second clip)
       @param transitionId is the data describing the dropped composition
    */
    Q_INVOKABLE void insertNewMix(int tid, int position, const QString &transitionId);
    /** @brief Returns the cut position if the composition is over a cut between 2 clips, -1 otherwise
    */
    Q_INVOKABLE int isOnCut(int cid) const;
    /** @brief Request inserting a new composition in timeline (dragged from compositions list)
       this function will check if there is a clip at insert point and
       adjust the composition length accordingly
       @param tid is the destination track
       @param position is the timeline position
       @param transitionId is the data describing the dropped composition
       @param logUndo if set to false, no undo object is stored
       @return the id of the inserted composition
    */
    Q_INVOKABLE int insertNewCompositionAtPos(int tid, int position, const QString &transitionId);
    Q_INVOKABLE int insertNewComposition(int tid, int clipId, int offset, const QString &transitionId, bool logUndo);

    /** @brief Request deletion of the currently selected clips
     */
    Q_INVOKABLE void deleteSelectedClips();

    Q_INVOKABLE void triggerAction(const QString &name);
    Q_INVOKABLE const QString actionText(const QString &name);

    /** @brief Returns id of the timeline selected clip if there is only 1 clip selected
     * or an AVSplit group. If allowComposition is true, returns composition id if
     * only 1 is selected, otherwise returns -1. If restrictToCurrentPos is true, it will
     * only return the id if timeline cursor is inside item
     */
    int getMainSelectedItem(bool restrictToCurrentPos = true, bool allowComposition = false);
    int getMainSelectedClip();
    /** @brief Return the {position, track id} of current selection. Operates only on video items (or audio if audioPart is true)
     */
    std::pair<int, int> selectionPosition(int *aTracks, int *vTracks);

    /** @brief Do we want to display video thumbnails
     */
    bool showThumbnails() const;
    bool showAudioThumbnails() const;
    bool showMarkers() const;
    bool audioThumbFormat() const;
    bool audioThumbNormalize() const;
    /** @brief Do we want to display audio thumbnails
     */
    Q_INVOKABLE bool showWaveforms() const;
    /** @brief Invoke the GUI to add new timeline tracks
     */
    Q_INVOKABLE void beginAddTrack(int tid);
    /** @brief Remove multiple(or single) timeline tracks
     */
    Q_INVOKABLE void deleteMultipleTracks(int tid);
    /** @brief Show / hide audio rec controls in active track
     */
    void switchTrackRecord(int tid = -1, bool monitor = false);
    /** @brief Group selected items in timeline
     */
    Q_INVOKABLE void groupSelection();
    /** @brief Ungroup selected items in timeline
     */
    Q_INVOKABLE void unGroupSelection(int cid = -1);
    /** @brief Ask for edit marker dialog
     */
    Q_INVOKABLE void editMarker(int cid = -1, int position = -1);
    /** @brief Ask for marker add dialog
     */
    Q_INVOKABLE void addMarker(int cid = -1, int position = -1);
    /** @brief Ask for quick marker add (without dialog)
     */
    Q_INVOKABLE void addQuickMarker(int cid = -1, int position = -1);
    /** @brief Ask for marker delete
     */
    Q_INVOKABLE void deleteMarker(int cid = -1, int position = -1);
    /** @brief Ask for all markers delete
     */
    Q_INVOKABLE void deleteAllMarkers(int cid = -1);
    /** @brief Ask for edit timeline guide dialog
     */
    Q_INVOKABLE void editGuide(int frame = -1);
    Q_INVOKABLE void moveGuideById(int id, int newFrame);
    Q_INVOKABLE int moveGuideWithoutUndo(int mid, int newFrame);
    /** @brief Move all guides in the given range
     * @param start the start point of the range in frames
     * @param end the end point of the range in frames
     * @param offset how many frames the guides are moved
     */
    Q_INVOKABLE bool moveGuidesInRange(int start, int end, int offset);
    /** @brief Move all guides in the given range (same as above but with undo/redo)
     * @param start the start point of the range in frames
     * @param end the end point of the range in frames
     * @param offset how many frames the guides are moved
     * @param undo
     * @param redo
     */
    Q_INVOKABLE bool moveGuidesInRange(int start, int end, int offset, Fun &undo, Fun &redo);

    /** @brief Add a timeline guide
     */
    Q_INVOKABLE void switchGuide(int frame = -1, bool deleteOnly = false, bool showGui = false);
    /** @brief Request monitor refresh
     */
    Q_INVOKABLE void requestRefresh();

    /** @brief Show the asset of the given item in the AssetPanel
       If the id corresponds to a clip, we show the corresponding effect stack
       If the id corresponds to a composition, we show its properties
    */
    Q_INVOKABLE void showAsset(int id);
    Q_INVOKABLE void showTrackAsset(int trackId);
    /** @brief Adjust height of all similar (audio or video) tracks
    */
    Q_INVOKABLE void adjustTrackHeight(int trackId, int height);
    Q_INVOKABLE void adjustAllTrackHeight(int trackId, int height);
    Q_INVOKABLE void collapseAllTrackHeight(int trackId, bool collapse, int collapsedHeight);

    /** @brief Reset track \@trackId height to default track height. Adjusts all tracks if \@trackId == -1
    */
    Q_INVOKABLE void defaultTrackHeight(int trackId);

    Q_INVOKABLE bool exists(int itemId);

    Q_INVOKABLE int headerWidth() const;
    Q_INVOKABLE void setHeaderWidth(int width);
    /** @brief Hide / show a timeline track
     */
    Q_INVOKABLE void hideTrack(int trackId, bool hide);

    /** @brief Seek to next snap point
     */
    void gotoNextSnap();
    /** @brief Seek to previous snap point
     */
    void gotoPreviousSnap();
    /** @brief Seek to previous guide
     */
    void gotoPreviousGuide();
    /** @brief Seek to next guide
     */
    void gotoNextGuide();

    /** @brief Set current item's start point to cursor position
     */
    void setInPoint(bool ripple = false);
    /** @brief Set current item's end point to cursor position
     */
    void setOutPoint(bool ripple = false);
    /** @brief Return the project's tractor
     */
    Mlt::Tractor *tractor();
    /** @brief Return a track's producer
     */
    Mlt::Producer trackProducer(int tid);
    /** @brief Get the list of currently selected clip id's
     */
    QList<int> selection() const;
    /** @brief Returns the id of the currently selected mix's clip, -1 if no mix selected
     */
    int selectedMix() const;

    /** @brief Add an asset (effect, composition)
     */
    void addAsset(const QVariantMap &data);

    /** @brief Cuts the clip on current track at timeline position
     */
    Q_INVOKABLE void cutClipUnderCursor(int position = -1, int track = -1);
    /** @brief Cuts all clips at timeline position
     */
    Q_INVOKABLE void cutAllClipsUnderCursor(int position = -1);
    /** @brief Request a spacer operation
     */
    Q_INVOKABLE int requestSpacerStartOperation(int trackId, int position);
    /** @brief Returns the minimum available position for a spacer operation
     */
    Q_INVOKABLE int spacerMinPos() const;
    /** @brief Get a list of guides Id after a given frame
     */
    Q_INVOKABLE QVector<int> spacerSelection(int startFrame);
    /** @brief Move a list of guides from a given offset
     */
    Q_INVOKABLE void spacerMoveGuides(const QVector<int> &ids, int offset);
    /** @brief Get the position of the first marker in the list
     */
    Q_INVOKABLE int getGuidePosition(int ids);
    /** @brief Request a spacer operation
     */
    Q_INVOKABLE bool requestSpacerEndOperation(int clipId, int startPosition, int endPosition, int affectedTrack, const QVector<int> &selectedGuides = QVector<int>(), int guideStart = -1);
    /** @brief Request a Fade in effect for clip
     */
    Q_INVOKABLE void adjustFade(int cid, const QString &effectId, int duration, int initialDuration);

    Q_INVOKABLE const QString getTrackNameFromMltIndex(int trackPos);
    /** @brief Request inserting space in a track
     */
    Q_INVOKABLE void insertSpace(int trackId = -1, int frame = -1);
    Q_INVOKABLE void removeSpace(int trackId = -1, int frame = -1, bool affectAllTracks = false);
    /** @brief Remove all spaces in a @trackId track after @frame position
     */
    void removeTrackSpaces(int trackId, int frame);
    /** @brief Remove all clips in a @trackId track after @frame position
     */
    void removeTrackClips(int trackId, int frame);
    /** @brief If clip is enabled, disable, otherwise enable
     */
    Q_INVOKABLE void switchEnableState(std::unordered_set<int> selection = {});
    Q_INVOKABLE void addCompositionToClip(const QString &assetId, int clipId = -1, int offset = -1);
    Q_INVOKABLE void addEffectToClip(const QString &assetId, int clipId = -1);
    Q_INVOKABLE void setEffectsEnabled(int clipId, bool enabled);

    Q_INVOKABLE void requestClipCut(int clipId, int position);

    /** @brief Extract (delete + remove space) current clip
     */
    void extract(int clipId = -1, bool singleSelectionMode = false);
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

    /** @brief Seeks to selected clip start / end
     */
    Q_INVOKABLE void pasteEffects(int targetId = -1);
    Q_INVOKABLE void deleteEffects(int targetId = -1);
    Q_INVOKABLE double fps() const;
    Q_INVOKABLE void addEffectKeyframe(int cid, int frame, double val);
    Q_INVOKABLE void removeEffectKeyframe(int cid, int frame);
    Q_INVOKABLE void updateEffectKeyframe(int cid, int oldFrame, int newFrame, const QVariant &normalizedValue = QVariant());
    Q_INVOKABLE bool hasKeyframeAt(int cid, int frame);

    /** @brief Make current timeline track active/inactive*/
    Q_INVOKABLE void switchTrackActive(int trackId = -1);
    /** @brief Toggle the active/inactive state of all tracks*/
    void switchAllTrackActive();
    /** @brief Make all tracks active or inactive */
    void makeAllTrackActive();
    void switchTrackDisabled();
    void switchTrackLock(bool applyToAll = false);
    void switchTargetTrack();

    const QString getTrackNameFromIndex(int trackIndex);
    /** @brief Seeks to selected clip start / end or, if none is selected, to the start / end of the clip under the cursor
     */
    void seekCurrentClip(bool seekToEnd = false);
    /** @brief Seeks to a clip start (or end) based on it's clip id
     */
    void seekToClip(int cid, bool seekToEnd);
    /** @brief Returns true if timeline cursor is inside the item
     */
    bool positionIsInItem(int id);
    /** @brief Returns the number of tracks (audioTrakcs, videoTracks)
     */
    QPair<int, int> getAvTracksCount() const;
    /** @brief Request monitor refresh if item (clip or composition) is under timeline cursor
     */
    void refreshItem(int id);
    /** @brief Seek timeline to mouse position
     */
    void seekToMouse();

    /** @brief Set a property on the active track
     */
    void setActiveTrackProperty(const QString &name, const QString &value);
    /** @brief Get a property on the active track
     */
    const QVariant getActiveTrackProperty(const QString &name) const;
    /** @brief Is the active track audio
     */
    bool isActiveTrackAudio() const;

    /** @brief Returns a list of all luma files used in the project
     */
    QStringList extractCompositionLumas() const;
    /** @brief Returns a list of all external files used by effects in the timeline
     */
    QStringList extractExternalEffectFiles() const;
    /** @brief Get the x,y position of the mouse in the timeline widget
     */
    Q_INVOKABLE const QPoint getMousePosInTimeline() const;
    /** @brief Get the frame where mouse is positioned
     */
    Q_INVOKABLE int getMousePos();
    /** @brief Get the frame where mouse is positioned
     */
    int getMouseTrack();
    /** @brief Returns a map of track ids/track names
     */
    QMap<int, QString> getTrackNames(bool videoOnly);
    /** @brief Returns the transition a track index for a composition (MLT index / Track id)
     */
    QPair<int, int> getCompositionATrack(int cid) const;
    void setCompositionATrack(int cid, int aTrack);
    /** @brief Return true if composition's a_track is automatic (no forced track)
     */
    bool compositionAutoTrack(int cid) const;
    const QString getClipBinId(int clipId) const;
    void focusItem(int itemId);
    /** @brief Create and display a split clip view to compare effect
     */
    bool createSplitOverlay(int clipId, std::shared_ptr<Mlt::Filter> filter);
    /** @brief Delete the split clip view to compare effect
     */
    void removeSplitOverlay();

    /** @brief Limit the given offset to the max/min possible offset of the main trimming clip
     *  @returns The bounded offset
     */
    int trimmingBoundOffset(int offset);
    /** @brief @todo TODO */
    bool slipProcessSelection(int mainClipId, bool addToSelection);
    Q_INVOKABLE bool requestStartTrimmingMode(int clipId = -1, bool addToSelection = false, bool right = false);
    Q_INVOKABLE void requestEndTrimmingMode();
    Q_INVOKABLE void slipPosChanged(int offset);
    Q_INVOKABLE void ripplePosChanged(int pos, bool right);

    /** @brief @todo TODO */
    Q_INVOKABLE int requestItemRippleResize(int itemId, int size, bool right, bool logUndo = true, int snapDistance = -1, bool allowSingleResize = false);

    /** @brief Add current timeline zone to preview rendering
     */
    void addPreviewRange(bool add);
    /** @brief Clear current timeline zone from preview rendering
     */
    void clearPreviewRange(bool resetZones);
    void startPreviewRender();
    void stopPreviewRender();
    QVariantList dirtyChunks() const;
    QVariantList renderedChunks() const;
    /** @brief returns the frame currently processed by timeline preview, -1 if none
     */
    int workingPreview() const;

    /** @brief Return true if we want to use timeline ruler zone for editing */
    bool useRuler() const;

    /** @brief Return true if the scroll wheel should scroll vertically (Shift key for horizontal); false if it should scroll horizontally (Shift for vertical) */
    bool scrollVertically() const;

    /** @brief Load timeline preview from saved doc
     */
    void loadPreview(const QString &chunks, const QString &dirty, bool enable, Mlt::Playlist &playlist);

    /** @brief Change track compsiting mode */
    void switchCompositing(bool enable);

    /** @brief Change a clip item's speed in timeline */
    Q_INVOKABLE void changeItemSpeed(int clipId, double speed);
    /** @brief Activate time remap on the clip */
    void remapItemTime(int clipId);
    /** @brief Delete selected zone and fill gap by moving following clips
     *  @param lift if true, the zone will simply be deleted but clips won't be moved
     */
    void extractZone(QPoint zone, bool liftOnly = false);
    /** @brief Insert clip monitor into timeline
     *  @returns the zone end position or -1 on fail
     */
    Q_INVOKABLE bool insertClipZone(const QString &binId, int tid, int pos);
    int insertZone(const QString &binId, QPoint zone, bool overwrite, Fun &undo, Fun &redo);
    void updateClip(int clipId, const QVector<int> &roles);
    void showClipKeyframes(int clipId, bool value);
    void showCompositionKeyframes(int clipId, bool value);
    /** @brief Adjust all timeline tracks height */
    void resetTrackHeight();
    /** @brief timeline preview params changed, reset */
    void resetPreview();
    /** @brief Set target tracks (video, audio) */
    void setTargetTracks(bool hasVideo, const QMap <int, QString> &audioTargets);
    /** @brief Restore Bin Clip original target tracks (video, audio) */
    void restoreTargetTracks();
    /** @brief Return asset's display name from it's id (effect or composition) */
    Q_INVOKABLE const QString getAssetName(const QString &assetId, bool isTransition);
    /** @brief Set keyboard grabbing on current selection */
    Q_INVOKABLE void grabCurrent();
    /** @brief Returns true if an item is currently grabbed (has keyboard focus) */
    bool grabIsActive() const;
    /** @brief Returns keys for all used thumbnails */
    const std::unordered_map<QString, std::vector<int>> getThumbKeys();
    /** @brief Returns true if a drag operation is currently running in timeline */
    bool dragOperationRunning();
    /** @brief Returns true if the timeline is in trimming mode (slip, slide, ripple, rolle) */
    bool trimmingActive();
    /** @brief Disconnect some stuff before closing project */
    void prepareClose();
    /** @brief Check that we don't keep a deleted track id */
    void checkTrackDeletion(int selectedTrackIx);
    /** @brief Return true if an overlay track is used */
    bool hasPreviewTrack() const;
    /** @brief Display project master effects */
    Q_INVOKABLE void showMasterEffects();
    /** @brief Return true if an instance of this bin clip is currently under timeline cursor */
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
    /** @brief Create a mix transition with currently selected clip. If delta = -1, mix with previous clip, +1 with next clip and 0 will check cursor position*/
    Q_INVOKABLE void mixClip(int cid = -1, int delta = 0);
    /** @brief Temporarily un/plug a list of clips in timeline. */
    void temporaryUnplug(const QList<int> &clipIds, bool hide);
    /** @brief Import a subtitle file*/
    void importSubtitle(const QString &path = QString());
    /** @brief Export a subtitle file*/
    void exportSubtitle();
    /** @brief Launch speech recognition on timeline zone*/
    void subtitleSpeechRecognition();
    /** @brief Show active effect zone for current effect*/
    void showRulerEffectZone(QPair <int, int>inOut, bool checked);
    /** @brief Set the list of master effect zones */
    void updateMasterZones(const QVariantList &zones);
    /** @brief get Maximum duration of a clip */
    int clipMaxDuration(int cid);
    /** @brief Get Mix cut pos (the duration of the mix on the right clip) */
    int getMixCutPos(int cid) const;
    /** @brief Get align info for a mix. */
    MixAlignment getMixAlign(int cid) const;
    /** @brief Process a lift operation for multitrack operation. */
    void processMultitrackOperation(int tid, int in);
    /** @brief Save all sequence properties (timeline position, guides, groups, ..) to the timeline tractor. */
    void getSequenceProperties(QMap<QString, QString> &seqProps);
    /** @brief Trigger refresh of subtitles combo menu. */
    void refreshSubtitlesComboIndex();

public Q_SLOTS:
    void updateClipActions();
    void resetView();
    void setAudioTarget(const QMap<int, int> &tracks);
    Q_INVOKABLE void switchAudioTarget(int trackId);
    Q_INVOKABLE void setVideoTarget(int track);
    Q_INVOKABLE void setActiveTrack(int track);
    void addEffectToCurrentClip(const QStringList &effectData);
    /** @brief Dis / enable timeline preview. */
    void disablePreview(bool disable);
    void invalidateItem(int cid);
    void invalidateTrack(int tid);
    void checkDuration();
    /** @brief Dis / enable multi track view. */
    void slotMultitrackView(bool enable = true, bool refresh = true);
    /** @brief Activate a video track by its position (0 = topmost). */
    void activateTrackAndSelect(int trackPosition, bool notesMode = false);
    /** @brief Save timeline selected clips to target folder. */
    void saveTimelineSelection(const QDir &targetDir);
    /** @brief Restore timeline scroll pos on open. */
    void setScrollPos(int pos);
    /** @brief Request resizing currently selected mix. */
    void resizeMix(int cid, int duration, MixAlignment align, int leftFrames = -1);
    /** @brief change zone info with undo. */
    Q_INVOKABLE void updateZone(const QPoint oldZone, const QPoint newZone, bool withUndo = true);
    Q_INVOKABLE void updateEffectZone(const QPoint oldZone, const QPoint newZone, bool withUndo = true);
    void updateTrimmingMode();
    /** @brief When a clip or composition is moved, inform asset panel to update cursor position in keyframe views. */
    void checkClipPosition(const QModelIndex &topLeft, const QModelIndex &, const QVector<int> &roles);
    /** @brief Adjust all tracks height to fit in view. */
    Q_INVOKABLE void autofitTrackHeight(int timelineHeight, int collapsedHeight);
    Q_INVOKABLE void subtitlesMenuActivatedAsync(int ix);
    /** @brief Switch the active subtitle in the list. */
    void subtitlesMenuActivated(int ix);

private Q_SLOTS:
    void updateVideoTarget();
    void updateAudioTarget();
    /** @brief Dis / enable multi track view. */
    void updateMultiTrack();
    /** @brief An operation was attempted on a locked track, animate lock icon to make user aware */
    void slotFlashLock(int trackId);
    void initializePreview();
    /** @brief Display the active subtitle mode in subtitle track combobox. */
    void loadSubtitleIndex();

public:
    /** @brief a list of actions that have to be enabled/disabled depending on the timeline selection */
    QList<QAction *> clipActions;
    /** @brief The in point for a multicam operation */
    int multicamIn;
    /** @brief Set the in point for a multicam operation and trigger necessary signals */
    void setMulticamIn(int pos);

private:
    int m_duration;
    QQuickItem *m_root;
    KActionCollection *m_actionCollection;
    std::shared_ptr<TimelineItemModel> m_model;
    bool m_usePreview;
    int m_audioTarget;
    int m_videoTarget;
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
    int m_activeTrack;
    double m_scale;
    QAction *m_disablePreview;
    std::shared_ptr<AudioCorrelation> m_audioCorrelator;
    QMutex m_metaMutex;
    bool m_ready;
    std::vector<int> m_activeSnaps;
    int m_snapStackIndex;
    QMetaObject::Connection m_connection;
    QMetaObject::Connection m_deleteConnection;
    QPoint m_effectZone;
    bool m_autotrackHeight;
    QVariantList m_masterEffectZones;
    /** @brief The clip that is displayed in the preview monitor during a trimming operation*/
    int m_trimmingMainClip;
    /** @brief The position of the active subtitle in the menu list*/
    int m_activeSubPosition{-1};

    int getMenuOrTimelinePos() const;
    /** @brief Prepare the preview manager */
    void connectPreviewManager();

Q_SIGNALS:
    void selected(Mlt::Producer *producer);
    void selectionChanged();
    void selectedMixChanged();
    void frameFormatChanged();
    void trackHeightChanged();
    void scaleFactorChanged();
    void audioThumbFormatChanged();
    void audioThumbNormalizeChanged();
    void durationChanged(int duration);
    void audioTargetChanged();
    void videoTargetChanged();
    void hasAudioTargetChanged();
    void hasVideoTargetChanged();
    void lastAudioTargetChanged();
    void autoScrollChanged();
    void lastVideoTargetChanged();
    void activeTrackChanged();
    void trimmingMainClipChanged();
    void colorsChanged();
    void showThumbnailsChanged();
    void showAudioThumbnailsChanged();
    void showMarkersChanged();
    void rippleChanged();
    void scrubChanged();
    void subtitlesWarningChanged();
    void multicamInChanged();
    void autotrackHeightChanged();
    void seeked(int position);
    void zoneChanged();
    void subtitlesListChanged();
    void zoneMoved(const QPoint &zone);
    /** @brief Requests that a given parameter model is displayed in the asset panel */
    void showTransitionModel(int tid, std::shared_ptr<AssetParameterModel>);
    /** @brief Requests that a given mix is displayed in the asset panel */
    void showMixModel(int cid, const std::shared_ptr<AssetParameterModel> &asset, bool refreshOnly);
    void showItemEffectStack(const QString &clipName, std::shared_ptr<EffectStackModel>, QSize frameSize, bool showKeyframes);
    void showSubtitle(int id);
    /** @brief notify of chunks change
     */
    void dirtyChunksChanged();
    void renderedChunksChanged();
    void workingPreviewChanged();
    void subtitlesDisabledChanged();
    void subtitlesLockedChanged();
    void useRulerChanged();
    void scrollVerticallyChanged();
    void updateZoom(double);
    /** @brief emitted when timeline selection changes, true if a clip is selected
     */
    void timelineClipSelected(bool);
    /** @brief User enabled / disabled snapping, update timeline behavior
     */
    void snapChanged();
    /** @brief Center timeline view on current position
     */
    void centerView();
    void guidesLockedChanged();
    void effectZoneChanged();
    void masterZonesChanged();
    Q_INVOKABLE void ungrabHack();
    void regainFocus();
    void updateAssetPosition(int itemId, const QUuid uuid);
    void stopAudioRecord();
    void activeSubtitlePositionChanged();
};
