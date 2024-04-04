/*
    SPDX-FileCopyrightText: 2017 Nicolas Carion
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include "definitions.h"
#include "undohelper.hpp"
#include <QReadWriteLock>
#include <QSharedPointer>
#include <memory>
#include <mlt++/MltPlaylist.h>
#include <mlt++/MltProfile.h>
#include <mlt++/MltTractor.h>
#include <unordered_map>
#include <unordered_set>

class TimelineModel;
class ClipModel;
class CompositionModel;
class EffectStackModel;
class AssetParameterModel;

class MixInfo
{
public:
    int firstClipId = -1;
    int secondClipId = -1;
    /** @brief in and out of the first clip in the mix */
    std::pair<int, int> firstClipInOut = {-1, -1};
    /** @brief in and out of the second clip in the mix */
    std::pair<int, int> secondClipInOut = {-1, -1};
    /** @brief Distance between first clip out and cut pos */
    int mixOffset = 0;
};

/** @brief This class represents a Track object, as viewed by the backend.
   To allow same track transitions, a Track object corresponds to two Mlt::Playlist, between which we can switch when required by the transitions.
   In general, the Gui associated with it will send modification queries (such as resize or move), and this class authorize them or not depending on the
   validity of the modifications
*/
class TrackModel
{

public:
    TrackModel() = delete;
    ~TrackModel();

    friend class ClipModel;
    friend class CompositionModel;
    friend class TimelineController;
    friend struct TimelineFunctions;
    friend class TimelineItemModel;
    friend class TimelineModel;

private:
    /** This constructor is private, call the static construct instead */
    TrackModel(const std::weak_ptr<TimelineModel> &parent, int id = -1, const QString &trackName = QString(), bool audioTrack = false);
    TrackModel(const std::weak_ptr<TimelineModel> &parent, Mlt::Tractor mltTrack, int id = -1);

public:
    /** @brief Creates a track, which references itself to the parent
       Returns the (unique) id of the created track
       @param id Requested id of the track. Automatic if id = -1
       @param pos is the optional position of the track. If left to -1, it will be added at the end
     */
    static int construct(const std::weak_ptr<TimelineModel> &parent, int id = -1, int pos = -1, const QString &trackName = QString(), bool audioTrack = false,
                         bool singleOperation = true);

    /** @brief returns the number of clips */
    int getClipsCount();

    /** @brief returns the number of compositions */
    int getCompositionsCount() const;

    /** @brief Perform a split at the requested position */
    bool splitClip(QSharedPointer<ClipModel> caller, int position);

    /** @brief Implicit conversion operator to access the underlying producer
     */
    operator Mlt::Producer &() { return *m_track.get(); }

    /** @brief Returns true if track is in locked state
     */
    bool isLocked() const;
    /** @brief Returns true if track is active in timeline, ie.
     * will receive insert/lift/overwrite/extract operations
     */
    bool isTimelineActive() const;
    /** @brief Returns true if track is active and not locked
     */
    bool shouldReceiveTimelineOp() const;
    /** @brief Returns true if track is an audio track
     */
    bool isAudioTrack() const;
    Mlt::Tractor *getTrackService();
    /** @brief Returns the track type (audio / video)
     */
    PlaylistState::ClipState trackType() const;
    /** @brief Returns true if track is disabled
     */
    bool isHidden() const;
    /** @brief Returns true if track is disabled
     */
    bool isMute() const;

    // TODO make protected
    QVariant getProperty(const QString &name) const;
    void setProperty(const QString &name, const QString &value);
    /** @brief Remove a composition between 2 same track clips */
    bool requestRemoveMix(std::pair<int, int> clipIds, Fun &undo, Fun &redo);
    /** @brief Create a composition between 2 same track clips */
    bool requestClipMix(const QString &mixId, std::pair<int, int> clipIds, std::pair<int, int> mixDurations, bool updateView, bool finalMove, Fun &undo,
                        Fun &redo, bool groupMove);
    /** @brief Get clip ids and in/out position for mixes in this clip */
    std::pair<MixInfo, MixInfo> getMixInfo(int cid) const;
    /** @brief Delete a mix composition */
    bool deleteMix(int clipId, bool final, bool notify = true);
    /** @brief Create a mix composition using clip ids */
    bool createMix(std::pair<int, int> clipIds, std::pair<int, int> mixData);
    bool createMix(MixInfo info, std::pair<QString, QVector<QPair<QString, QVariant>>> params, std::pair<int, int> tracks, bool finalMove);
    /** @brief Create a mix composition using mix info */
    bool createMix(MixInfo info, bool isAudio);
    /** @brief Change id of first clip in a mix (in case of clip cut) */
    bool reAssignEndMix(int currentId, int newId);
    /** @brief Get all necessary infos to clone a mix */
    std::pair<QString, QVector<QPair<QString, QVariant>>> getMixParams(int cid);
    /** @brief Get the mix tracks */
    std::pair<int, int> getMixTracks(int cid) const;
    void switchMix(int cid, const QString &composition, Fun &undo, Fun &redo);
    /** @brief Ensure we don't have unsynced mixes in the playlist (mixes without owner clip) */
    void syncronizeMixes(bool finalMove);
    /** @brief Remove a mix in the track (if its clip was removed) */
    void removeMix(const MixInfo &info);
    /** @brief Switch a clip from one playlist to the other */
    bool switchPlaylist(int clipId, int position, int sourcePlaylist, int destPlaylist);
    /** @brief Load a same track transition from project */
    bool loadMix(Mlt::Transition *t);
    /** @brief Set mix duration and mix cut pos on a clip */
    void setMixDuration(int cid, int mixDuration, int mixCut);
    int getMixDuration(int cid) const;
    /** @brief Get the assetparameter model for a mix */
    const std::shared_ptr<AssetParameterModel> mixModel(int cid);
    /** @brief Get a list of current effect stack zones */
    QVariantList stackZones() const;
    /** @brief Return true if a clip starts at pos in one of the trak playlists */
    bool hasClipStart(int pos);
    /** @brief Calculate a hash based on all clips an d mixes positions/playtime */
    QByteArray trackHash();

protected:
    /** @brief This will lock the track: it will no longer allow insertion/deletion/resize of items
       This functions are dangerous to call directly since locking the track will potentially
       mess up with the undo/redo system (a track lock may make an undo impossible).
       Prefer calling TimelineModel::setTrackLockedState */
    void lock();
    void unlock();

    /** @brief Returns a lambda that performs a resize of the given clip.
       The lambda returns true if the operation succeeded, and otherwise nothing is modified
       This method is protected because it shouldn't be called directly. Call the function in the timeline instead.
       @param clipId is the id of the clip
       @param in is the new starting on the clip
       @param out is the new ending on the clip
       @param right is true if we change the right side of the clip, false otherwise
       @param hasMix is true if we change the right side of the clip, false otherwise
    */
    Fun requestClipResize_lambda(int clipId, int in, int out, bool right, bool hasMix = false, bool finalMove = false);

    /** @brief Performs an insertion of the given clip.
       Returns true if the operation succeeded, and otherwise, the track is not modified.
       This method is protected because it shouldn't be called directly. Call the function in the timeline instead.
       @param clip is the id of the clip
       @param position is the position where to insert the clip
       @param updateView whether we send update to the view
       @param finalMove if the move is finished (not while dragging), so we invalidate timeline preview / check project duration
       @param undo Lambda function containing the current undo stack. Will be updated with current operation
       @param redo Lambda function containing the current redo queue. Will be updated with current operation
    */
    bool requestClipInsertion(int clipId, int position, bool updateView, bool finalMove, Fun &undo, Fun &redo, bool groupMove = false, bool newInsertion = true,
                              const QList<int> &allowedClipMixes = {});
    /** @brief This function returns a lambda that performs the requested operation */
    Fun requestClipInsertion_lambda(int clipId, int position, bool updateView, bool finalMove, bool groupMove = false, const QList<int> &allowedClipMixes = {});

    /** @brief Performs an deletion of the given clip.
       Returns true if the operation succeeded, and otherwise, the track is not modified.
       This method is protected because it shouldn't be called directly. Call the function in the timeline instead.
       @param clipId is the id of the clip
       @param updateView whether we send update to the view
       @param finalMove if the move is finished (not while dragging), so we invalidate timeline preview / check project duration
       @param undo Lambda function containing the current undo stack. Will be updated with current operation
       @param redo Lambda function containing the current redo queue. Will be updated with current operation
       @param groupMove If true, this is part of a larger operation and some operations like checking track duration will not be performed and have to be
       performed separately
       @param finalDeletion If true, the clip will be deselected (should be false if this is a clip move doing delete/insert)
    */
    bool requestClipDeletion(int clipId, bool updateView, bool finalMove, Fun &undo, Fun &redo, bool groupMove, bool finalDeletion,
                             const QList<int> &allowedClipMixes = {});
    /** @brief This function returns a lambda that performs the requested operation */
    Fun requestClipDeletion_lambda(int clipId, bool updateView, bool finalMove, bool groupMove, bool finalDeletion);

    /** @brief Performs an insertion of the given composition.
       Returns true if the operation succeeded, and otherwise, the track is not modified.
       This method is protected because it shouldn't be called directly. Call the function in the timeline instead.
       Note that in Mlt, the composition insertion logic is not really at the track level, but we use that level to do collision checking
       @param compoId is the id of the composition
       @param position is the position where to insert the composition
       @param updateView whether we send update to the view
       @param undo Lambda function containing the current undo stack. Will be updated with current operation
       @param redo Lambda function containing the current redo queue. Will be updated with current operation
    */
    bool requestCompositionInsertion(int compoId, int position, bool updateView, bool finalMove, Fun &undo, Fun &redo);
    /** @brief This function returns a lambda that performs the requested operation */
    Fun requestCompositionInsertion_lambda(int compoId, int position, bool updateView, bool finalMove = false);

    bool requestCompositionDeletion(int compoId, bool updateView, bool finalMove, Fun &undo, Fun &redo, bool finalDeletion);
    Fun requestCompositionDeletion_lambda(int compoId, bool updateView, bool finalMove = false);
    Fun requestCompositionResize_lambda(int compoId, int in, int out = -1, bool logUndo = false);

    /** @brief Returns the size of the blank before or after the given clip
       @param clipId is the id of the clip
       @param after is true if we query the blank after, false otherwise
    */
    int getBlankSizeNearClip(int clipId, bool after);
    int getBlankSizeNearComposition(int compoId, bool after);
    int getBlankStart(int position);
    int getPreviousBlankEnd(int position);
    int getNextBlankStart(int position, bool allowCurrentPos = true);
    /** @brief Returns the start of the blank on a specific playlist */
    int getBlankStart(int position, int track);
    int getBlankSizeAtPos(int frame);
    /** @brief Returns the start of the clip on a specific playlist */
    int getClipStart(int position, int track);
    int getClipEnd(int position, int track);
    /** @brief Returns true if clip at position is the last on playlist
     * @param position the position in playlist
     */
    bool isLastClip(int position);

    /** @brief Returns the best composition duration depending on clips on the track */
    int suggestCompositionLength(int position);
    /** @brief Returns the best composition duration depending on compositions on the track */
    QPair<int, int> validateCompositionLength(int pos, int offset, int duration, int endPos);

    /** @brief Returns the (unique) construction id of the track*/
    int getId() const;

    /** @brief This function is used only by the QAbstractItemModel
      Given a row in the model, retrieves the corresponding clip id. If it does not exist, returns -1
    */
    int getClipByRow(int row) const;

    /** @brief This function is used only by the QAbstractItemModel
      Given a row in the model, retrieves the corresponding composition id. If it does not exist, returns -1
    */
    int getCompositionByRow(int row) const;
    /** @brief This function is used only by the QAbstractItemModel
      Given a clip ID, returns the row of the clip.
    */
    int getRowfromClip(int clipId) const;

    /** @brief This function is used only by the QAbstractItemModel
      Given a composition ID, returns the row of the composition.
    */
    int getRowfromComposition(int compoId) const;

    /** @brief This is an helper function that test frame level consistency with the MLT structures */
    bool checkConsistency();

    /** @brief Returns true if we have a composition intersecting with the range [in,out]*/
    bool hasIntersectingComposition(int in, int out) const;

    /** @brief This is an helper function that returns the sub-playlist in which the clip is inserted, along with its index in the playlist
     @param position the position of the target clip*/
    std::pair<int, int> getClipIndexAt(int position, int playlist = -1);
    QSharedPointer<Mlt::Producer> getClipProducer(int clipId);

    /** @brief This is an helper function that checks in all playlists if the given position is a blank */
    bool isBlankAt(int position, int playlist = -1);

    /** @brief This is an helper function that returns the end of the blank that covers given position */
    int getBlankEnd(int position);
    /** @brief Same, but we restrict to a specific track*/
    int getBlankEnd(int position, int track);

    /** @brief Returns the clip id on this track at position requested, or -1 if no clip */
    int getClipByPosition(int position, int playlist = -1);
    /** @brief Returns the clip id on this track that starts at position requested, or -1 if no clip */
    int getClipByStartPosition(int position) const;

    /** @brief Returns the composition id on this track starting position requested, or -1 if not found */
    int getCompositionByPosition(int position);
    /** @brief Add a track effect */
    bool addEffect(const QString &effectId);

    /** @brief Returns a comma separated list of effect names */
    const QString effectNames() const;

    /** @brief Returns true if effect stack is enabled */
    bool stackEnabled() const;

    /** @brief Enable / disable the track's effect stack */
    void setEffectStackEnabled(bool enable);

    /** @brief This function removes the clip from the mlt object, and then insert it back in the same spot again.
     * This is used when some properties of the clip have changed, and we need this to refresh it */
    void replugClip(int clipId);
    void temporaryReplugClip(int cid);
    void temporaryUnplugClip(int clipId);

    int trackDuration() const;

    /** @brief Returns the list of the ids of the clips that intersect the given range */
    std::unordered_set<int> getClipsInRange(int position, int end = -1);
    /** @brief Returns the list of the ids of the compositions that intersect the given range */
    std::unordered_set<int> getCompositionsInRange(int position, int end);

    /** @brief Import effects from a service that contains some (another track) */
    bool importEffects(std::weak_ptr<Mlt::Service> service);
    /** @brief Copy effects from another effect stack */
    bool copyEffect(const std::shared_ptr<EffectStackModel> &stackModel, int rowId);
    /** @brief Returns true if we have a blank at position for duration */
    bool isAvailable(int position, int duration, int playlist);
    /** @brief Returns true if we have a blank at position for duration, with the exception of clip ids exception */
    bool isAvailableWithExceptions(int position, int duration, const QVector<int> &exceptions);
    /** @brief Returns the number of same track transitions (mix) in this track */
    int mixCount() const;
    /** @brief Returns true if the track has a same track transition for this clip (cid) */
    bool hasMix(int cid) const;
    /** @brief Returns true if this clip has a mix at start */
    bool hasStartMix(int cid) const;
    /** @brief Returns true if this clip has a mix at end */
    bool hasEndMix(int cid) const;
    /** @brief Returns the cid of the second partner or -1 if the given clip has no end mix */
    int getSecondMixPartner(int cid) const;
    /** @brief Returns the cut position if the composition is over a cut between 2 clips, -1 otherwise
     */
    int isOnCut(int cid);
    /** @brief Returns all mix info as xml */
    QDomElement mixXml(QDomDocument &document, int cid) const;
    /** @brief Check if a mix is reversed (moslty used in tests) */
    bool mixIsReversed(int cid) const;
    /** @brief Adjust effect stack length to current track duration */
    void adjustStackLength(int duration, int newDuration, Fun &undo, Fun &redo);

public Q_SLOTS:
    /** Delete the current track and all its associated clips */
    void slotDelete();

private:
    std::weak_ptr<TimelineModel> m_parent;
    /// this is the creation id of the track, used for book-keeping
    int m_id;

    // We fake two playlists to allow same track transitions.
    std::shared_ptr<Mlt::Tractor> m_track;
    Mlt::Playlist m_playlists[2];
    /// A list of clips having a same track transition, in the form: {first_clip_id, second_clip_id} where first_clip is placed before second_clip
    QMap<int, int> m_mixList;

    /** This is important to keep an ordered structure to store the clips, since we use their ids order as row order*/
    std::map<int, std::shared_ptr<ClipModel>> m_allClips;
    /** This is important to keep an ordered structure to store the compositions, since we use their ids order as row order*/
    std::map<int, std::shared_ptr<CompositionModel>> m_allCompositions;

    /** We store the positions of the compositions. In Melt, the compositions are not inserted at the track level, but we keep
     *  those positions here to check for moves and resize
     */
    std::map<int, int> m_compoPos;

    /// This is a lock that ensures safety in case of concurrent access
    mutable QReadWriteLock m_lock;
    void reverseCompositionXml(const QString &composition, QDomElement xml);
    void updateCompositionDirection(Mlt::Transition &transition, bool reverse);

protected:
    bool m_softDelete;
    std::shared_ptr<EffectStackModel> m_effectStack;
    /// A list of same track transitions for this track, in the form: {second_clip_id, transition}
    std::unordered_map<int, std::shared_ptr<AssetParameterModel>> m_sameCompositions;
};
