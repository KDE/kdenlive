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
#include "timeline2/model/timelineitemmodel.hpp"
#include "timelinewidget.h"

class PreviewManager;
class QAction;

// see https://bugreports.qt.io/browse/QTBUG-57714, don't expose a QWidget as a context property
class TimelineController : public QObject
{
    Q_OBJECT
    /* @brief holds a list of currently selected clips (list of clipId's)
     */
    Q_PROPERTY(QList<int> selection READ selection WRITE setSelection NOTIFY selectionChanged)
    /* @brief holds the timeline zoom factor
     */
    Q_PROPERTY(double scaleFactor READ scaleFactor WRITE setScaleFactor NOTIFY scaleFactorChanged)
    /* @brief holds the current project duration
     */
    Q_PROPERTY(int duration READ duration NOTIFY durationChanged)
    Q_PROPERTY(bool audioThumbFormat READ audioThumbFormat NOTIFY audioThumbFormatChanged)
    /* @brief holds the current timeline position
     */
    Q_PROPERTY(int position READ position WRITE setPosition NOTIFY positionChanged)
    Q_PROPERTY(int zoneIn READ zoneIn WRITE setZoneIn NOTIFY zoneChanged)
    Q_PROPERTY(int zoneOut READ zoneOut WRITE setZoneOut NOTIFY zoneChanged)
    Q_PROPERTY(int seekPosition READ seekPosition WRITE setSeekPosition NOTIFY seekPositionChanged)
    Q_PROPERTY(bool ripple READ ripple NOTIFY rippleChanged)
    Q_PROPERTY(bool scrub READ scrub NOTIFY scrubChanged)
    Q_PROPERTY(bool showThumbnails READ showThumbnails NOTIFY showThumbnailsChanged)
    Q_PROPERTY(bool showMarkers READ showMarkers NOTIFY showMarkersChanged)
    Q_PROPERTY(bool showAudioThumbnails READ showAudioThumbnails NOTIFY showAudioThumbnailsChanged)
    Q_PROPERTY(QVariantList dirtyChunks READ dirtyChunks NOTIFY dirtyChunksChanged)
    Q_PROPERTY(QVariantList renderedChunks READ renderedChunks NOTIFY renderedChunksChanged)
    Q_PROPERTY(int workingPreview READ workingPreview NOTIFY workingPreviewChanged)
    Q_PROPERTY(bool useRuler READ useRuler NOTIFY useRulerChanged)
    Q_PROPERTY(int activeTrack READ activeTrack WRITE setActiveTrack NOTIFY activeTrackChanged)
    Q_PROPERTY(int audioTarget READ audioTarget WRITE setAudioTarget NOTIFY audioTargetChanged)
    Q_PROPERTY(int videoTarget READ videoTarget WRITE setVideoTarget NOTIFY videoTargetChanged)

public:
    TimelineController(KActionCollection *actionCollection, QObject *parent);
    virtual ~TimelineController();
    /* @brief Sets the model that this widgets displays */
    void setModel(std::shared_ptr<TimelineItemModel> model);
    std::shared_ptr<TimelineItemModel> getModel() const;
    void setRoot(QQuickItem *root);

    Q_INVOKABLE bool isMultitrackSelected() const { return m_selection.isMultitrackSelected; }
    Q_INVOKABLE int selectedTrack() const { return m_selection.selectedTrack; }
    /* @brief Add a clip id to current selection
     */
    Q_INVOKABLE void addSelection(int newSelection);
    /* @brief returns current timeline's zoom factor
     */
    Q_INVOKABLE double scaleFactor() const;
    /* @brief set current timeline's zoom factor
     */
    void setScaleFactorOnMouse(double scale, bool zoomOnMouse);
    Q_INVOKABLE void setScaleFactor(double scale);
    /* @brief Returns the project's duration (tractor)
     */
    Q_INVOKABLE int duration() const;
    /* @brief Returns the current cursor position (frame currently displayed by MLT)
     */
    Q_INVOKABLE int position() const { return m_position; }
    /* @brief Returns the seek request position (-1 = no seek pending)
     */
    Q_INVOKABLE int seekPosition() const { return m_seekPosition; }
    Q_INVOKABLE int audioTarget() const;
    Q_INVOKABLE int videoTarget() const;
    Q_INVOKABLE int activeTrack() const { return m_activeTrack; }
    /* @brief Request a seek operation
       @param position is the desired new timeline position
     */
    Q_INVOKABLE int zoneIn() const { return m_zone.x(); }
    Q_INVOKABLE int zoneOut() const { return m_zone.y(); }
    Q_INVOKABLE void setZoneIn(int inPoint);
    Q_INVOKABLE void setZoneOut(int outPoint);
    void setZone(const QPoint &zone);
    /* @brief Request a seek operation
       @param position is the desired new timeline position
     */
    Q_INVOKABLE void setPosition(int position);
    Q_INVOKABLE bool snap();
    Q_INVOKABLE bool ripple();
    Q_INVOKABLE bool scrub();
    Q_INVOKABLE QString timecode(int frames);
    /* @brief Request inserting a new clip in timeline (dragged from bin or monitor)
       @param tid is the destination track
       @param position is the timeline position
       @param xml is the data describing the dropped clip
       @param logUndo if set to false, no undo object is stored
       @return the id of the inserted clip
     */
    Q_INVOKABLE int insertClip(int tid, int position, const QString &xml, bool logUndo, bool refreshView);
    /* @brief Request inserting multiple clips into the timeline (dragged from bin or monitor)
     * @param tid is the destination track
     * @param position is the timeline position
     * @param binIds the IDs of the bins being dropped
     * @param logUndo if set to false, no undo object is stored
     * @return the ids of the inserted clips
     */
    Q_INVOKABLE QList<int> insertClips(int tid, int position, const QStringList &binIds, bool logUndo, bool refreshView);
    /* @brief Request the grouping of the given clips
     * @param clipIds the ids to be grouped
     * @return the group id or -1 in case of faiure
     */
    Q_INVOKABLE int groupClips(const QList<int> &clipIds);
    /* @brief Request the ungrouping of clips
     * @param clipId the id of a clip belonging to the group
     * @return true in case of success, false otherwise
     */
    Q_INVOKABLE bool ungroupClips(int clipId);
    Q_INVOKABLE void copyItem();
    Q_INVOKABLE bool pasteItem(int clipId = -1, int tid = -1, int position = -1);
    /* @brief Request inserting a new composition in timeline (dragged from compositions list)
       @param tid is the destination track
       @param position is the timeline position
       @param transitionId is the data describing the dropped composition
       @param logUndo if set to false, no undo object is stored
       @return the id of the inserted composition
    */
    Q_INVOKABLE int insertComposition(int tid, int position, const QString &transitionId, bool logUndo);

    /* @brief Request deletion of the currently selected clips
     */
    Q_INVOKABLE void deleteSelectedClips();

    Q_INVOKABLE void triggerAction(const QString &name);

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
    /* @brief Group selected items in timeline
     */
    Q_INVOKABLE void groupSelection();
    /* @brief Ungroup selected items in timeline
     */
    Q_INVOKABLE void unGroupSelection(int cid = -1);
    /* @brief Ask for edit marker dialog
     */
    Q_INVOKABLE void editMarker(const QString &cid, int frame);
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

    Q_INVOKABLE void selectItems(QVariantList arg, int startFrame, int endFrame, bool addToSelect);

    Q_INVOKABLE int headerWidth() const;
    Q_INVOKABLE void setHeaderWidth(int width);

    /* @brief Seek to next snap point
     */
    void gotoNextSnap();
    /* @brief Seek to previous snap point
     */
    void gotoPreviousSnap();
    /* @brief Set current item's start point to cursor position
     */
    void setInPoint();
    /* @brief Set current item's end point to cursor position
     */
    void setOutPoint();
    /* @brief Return the project's tractor
     */
    Mlt::Tractor *tractor();
    /* @brief Sets the list of currently selected clips
       @param selection is the list of id's
       @param trackIndex is current clip's track
       @param isMultitrack is true if we want to select the whole tractor (currently unused)
     */
    void setSelection(const QList<int> &selection = QList<int>(), int trackIndex = -1, bool isMultitrack = false);
    /* @brief Get the list of currenly selected clip id's
     */
    QList<int> selection() const;

    /* @brief Add an asset (effect, composition)
     */
    void addAsset(const QVariantMap data);

    /* @brief Cuts the clip on current track at timeline position
     */
    Q_INVOKABLE void cutClipUnderCursor(int position = -1, int track = -1);
    /* @brief Request a spacer operation
     */
    Q_INVOKABLE int requestSpacerStartOperation(int trackId, int position);
    /* @brief Request a spacer operation
     */
    Q_INVOKABLE bool requestSpacerEndOperation(int clipId, int startPosition, int endPosition);
    /* @brief Request a Fade in effect for clip
     */
    Q_INVOKABLE void adjustFade(int cid, const QString &effectId, int duration, int initialDuration);

    Q_INVOKABLE const QString getTrackNameFromMltIndex(int trackPos);
    /* @brief Request inserting space in a track
     */
    Q_INVOKABLE void insertSpace(int trackId = -1, int frame = -1);
    Q_INVOKABLE void removeSpace(int trackId = -1, int frame = -1, bool affectAllTracks = false);
    /* @brief Change a clip status (normal / audio only / video only)
     */
    Q_INVOKABLE void setClipStatus(int clipId, int status);

    Q_INVOKABLE void requestClipCut(int clipId, int position);

    Q_INVOKABLE void extract(int clipId);

    Q_INVOKABLE void splitAudio(int clipId);
    void switchTrackLock(bool applyToAll = false);
    void switchTargetTrack();

    const QString getTrackNameFromIndex(int trackIndex);
    /* @brief Seeks to selected clip start / end
     */
    void seekCurrentClip(bool seekToEnd = false);
    /* @brief Seeks to a clip start (or end) based on it's clip id
     */
    void seekToClip(int cid, bool seekToEnd);
    /* @brief Returns the number of tracks (audioTrakcs, videoTracks)
     */
    QPoint getTracksCount() const;
    /* @brief Request monitor refresh if item (clip or composition) is under timeline cursor
     */
    void refreshItem(int id);
    /* @brief Seek timeline to mouse position
     */
    void seekToMouse();

    /* @brief User enabled / disabled snapping, update timeline behavior
     */
    void snapChanged(bool snap);

    /* @brief Returns a list of all luma files used in the project
     */
    QStringList extractCompositionLumas() const;
    /* @brief Get the frame where mouse is positionned
     */
    int getMousePos();
    /* @brief Get the frame where mouse is positionned
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
    bool createSplitOverlay(Mlt::Filter *filter);
    /* @brief Delete the split clip view to compare effect
     */
    void removeSplitOverlay();
    /* @brief Add current timeline zone to preview rendering
     */
    void addPreviewRange(bool add);
    /* @brief Clear current timeline zone from preview rendering
     */
    void clearPreviewRange();
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
    void loadPreview(QString chunks, QString dirty, const QDateTime &documentDate, int enable);
    /* @brief Return document properties with added settings from timeline
     */
    QMap<QString, QString> documentProperties();

    /** @brief Change track compsiting mode */
    void switchCompositing(int mode);

    /** @brief Change a clip item's speed in timeline */
    Q_INVOKABLE void changeItemSpeed(int clipId, int speed);
    /** @brief Delete selected zone and fill gap by moving following clips*/
    void extractZone(QPoint zone);
    /** @brief Delete selected zone */
    void liftZone(QPoint zone);
    bool insertZone(const QString &binId, QPoint zone, bool overwrite);
    void updateClip(int clipId, QVector<int> roles);
    void showClipKeyframes(int clipId, bool value);
    void showCompositionKeyframes(int clipId, bool value);
    /** @brief Returns last usable timeline position (seek request or current pos) */
    int timelinePosition() const;
    /** @brief Adjust all timeline tracks height */
    void resetTrackHeight();
    /** @brief timeline preview params changed, reset */
    void resetPreview();
    /** @brief Select the clip in active track under cursor */
    void selectCurrentItem(ObjectType type, bool select, bool addToCurrent = false);

public slots:
    void selectMultitrack();
    Q_INVOKABLE void setSeekPosition(int position);
    Q_INVOKABLE void setAudioTarget(int track);
    Q_INVOKABLE void setVideoTarget(int track);
    Q_INVOKABLE void setActiveTrack(int track);
    void onSeeked(int position);
    void addEffectToCurrentClip(const QStringList &effectData);
    /** @brief Dis / enable timeline preview. */
    void disablePreview(bool disable);
    void invalidateClip(int cid);
    void invalidateZone(int in, int out);
    void checkDuration();

private:
    QQuickItem *m_root;
    KActionCollection *m_actionCollection;
    std::shared_ptr<TimelineItemModel> m_model;
    bool m_usePreview;
    struct Selection
    {
        QList<int> selectedItems;
        int selectedTrack;
        bool isMultitrackSelected;
    };
    int m_position;
    int m_seekPosition;
    int m_audioTarget;
    int m_videoTarget;
    int m_activeTrack;
    QPoint m_zone;
    double m_scale;
    static int m_duration;
    Selection m_selection;
    Selection m_savedSelection;
    PreviewManager *m_timelinePreview;
    QAction *m_disablePreview;
    void emitSelectedFromSelection();
    int getCurrentItem();
    void initializePreview();
    // Get a list of currently selected items, including clips grouped with selection
    std::unordered_set<int> getCurrentSelectionIds() const;

signals:
    void selected(Mlt::Producer *producer);
    void selectionChanged();
    void frameFormatChanged();
    void trackHeightChanged();
    void scaleFactorChanged();
    void audioThumbFormatChanged();
    void durationChanged();
    void positionChanged();
    void seekPositionChanged();
    void audioTargetChanged();
    void videoTargetChanged();
    void activeTrackChanged();
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
    void showItemEffectStack(const QString &clipName, std::shared_ptr<EffectStackModel>, QPair<int, int> range, QSize frameSize, bool showKeyframes);
    /* @brief notify of chunks change
     */
    void dirtyChunksChanged();
    void renderedChunksChanged();
    void workingPreviewChanged();
    void useRulerChanged();
};

#endif
