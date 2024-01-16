/*
    SPDX-FileCopyrightText: 2017 Nicolas Carion
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include "moveableItem.hpp"
#include "undohelper.hpp"
#include <memory>

namespace Mlt {
class Producer;
}
class EffectStackModel;
class MarkerListModel;
class TimelineModel;
class TrackModel;
class KeyframeModel;
class ClipSnapModel;

/** @brief This class represents a Clip object, as viewed by the backend.
   In general, the Gui associated with it will send modification queries (such as resize or move), and this class authorize them or not depending on the
   validity of the modifications
*/
class ClipModel : public MoveableItem<Mlt::Producer>
{
    ClipModel() = delete;

protected:
    /** @brief This constructor is not meant to be called, call the static construct instead */
    ClipModel(const std::shared_ptr<TimelineModel> &parent, std::shared_ptr<Mlt::Producer> prod, const QString &binClipId, int id,
              PlaylistState::ClipState state, double speed = 1.);

public:
    ~ClipModel() override;
    /** @brief Creates a clip, which references itself to the parent timeline
       Returns the (unique) id of the created clip
       @param parent is a pointer to the timeline
       @param binClip is the id of the bin clip associated
       @param id Requested id of the clip. Automatic if -1
    */
    static int construct(const std::shared_ptr<TimelineModel> &parent, const QString &binClipId, int id, PlaylistState::ClipState state, int audioStream = -1,
                         double speed = 1., bool warp_pitch = false);

    /** @brief Creates a clip, which references itself to the parent timeline
       Returns the (unique) id of the created clip
    This variants assumes a producer is already known, which should typically happen only at loading time.
    Note that there is no guarantee that this producer is actually going to be used. It might be discarded.
    */
    static int construct(const std::shared_ptr<TimelineModel> &parent, const QString &binClipId, const std::shared_ptr<Mlt::Producer> &producer,
                         PlaylistState::ClipState state, int tid, const QString &originalDecimalPoint, int playlist = 0);

    /** @brief returns a property of the clip, or from it's parent if it's a cut
     */
    const QString getProperty(const QString &name) const override;
    int getIntProperty(const QString &name) const;
    /** @brief returns the bin clip name
     */
    const QString clipName() const;
    const QString clipTag() const;
    QSize getFrameSize() const;
    bool showKeyframes() const;
    void setShowKeyframes(bool show);

    /** @brief Returns true if the clip can be converted to a video clip */
    bool canBeVideo() const;
    /** @brief Returns true if the clip can be converted to an audio clip */
    bool canBeAudio() const;

    /** @brief Returns true if the producer is embedded in a chain (for use with timeremap) */
    bool isChain() const;
    bool hasTimeRemap() const;
    /** @brief Returns the duration of the input map */
    int getRemapInputDuration() const;
    /** @brief Get the time remap effect parameters */
    QMap<QString, QString> getRemapValues() const;
    void setRemapValue(const QString &name, const QString &value);

    /** @brief Returns a comma separated list of effect names */
    const QString effectNames() const;

    /** @brief Returns a list of external files (e.g. LUTs) used by the effects of the clip */
    const QStringList externalFiles() const;

    /** @brief Returns the timeline clip status (video / audio only) */
    PlaylistState::ClipState clipState() const;
    /** @brief Returns the bin clip type (image, color, AV, ...) */
    ClipType::ProducerType clipType() const;
    /** @brief Sets the timeline clip status (video / audio only) */
    bool setClipState(PlaylistState::ClipState state, Fun &undo, Fun &redo);
    /** @brief The fake track is used in insert/overwrite mode.
     *  in this case, dragging a clip is always accepted, but the change is not applied to the model.
     *  so we use a 'fake' track id to pass to the qml view
     */
    int getFakeTrackId() const;
    void setFakeTrackId(int fid);
    void setFakePosition(int fpos);
    int getFakePosition() const;
    void setMixDuration(int mix, int offset);
    void setMixDuration(int mix);
    int getMixDuration() const;
    int getMixCutPosition() const;
    void setGrab(bool grab) override;
    void setSelected(bool sel) override;

    /** @brief Returns an XML representation of the clip with its effects */
    QDomElement toXml(QDomDocument &document);

    /** @brief Retrieve a list of all snaps for this clip */
    void allSnaps(std::vector<int> &snaps, int offset = 0) const;

    /** @brief Replace the bin producer with another bin clip */
    void switchBinReference(const QString newId, const QUuid &uuid);

protected:
    /** @brief helper functions that creates the lambda */
    Fun setClipState_lambda(PlaylistState::ClipState state);
    /** @brief Returns a clip hash, useful for regression testing */
    QString clipHash() const;

public:
    /** @brief returns the length of the item on the timeline
     */
    int getPlaytime() const override;

    /** @brief Returns the bin clip's id */
    const QString &binId() const;

    void registerClipToBin(std::shared_ptr<Mlt::Producer> service, bool registerProducer);
    void deregisterClipToBin(const QUuid &uuid);

    bool addEffect(const QString &effectId);
    bool copyEffect(const QUuid &uuid, const std::shared_ptr<EffectStackModel> &stackModel, int rowId);
    /** @brief Import effects from a different stackModel */
    bool importEffects(std::shared_ptr<EffectStackModel> stackModel);
    /** @brief Import effects from a service that contains some (another clip?) */
    bool importEffects(std::weak_ptr<Mlt::Service> service);

    bool removeFade(bool fromStart);
    /** @brief Adjust effects duration. Should be called after each resize / cut operation */
    bool adjustEffectLength(bool adjustFromEnd, int oldIn, int newIn, int oldDuration, int duration, int offset, Fun &undo, Fun &redo, bool logUndo);
    bool adjustEffectLength(const QString &effectName, int duration, int originalDuration, Fun &undo, Fun &redo);
    void passTimelineProperties(const std::shared_ptr<ClipModel> &other);
    KeyframeModel *getKeyframeModel();

    int fadeIn() const;
    int fadeOut() const;

    /** @brief Tracks have two sub playlists to enable same track transitions. This returns the index of the sub-playlist containing this clip */
    int getSubPlaylistIndex() const;
    void setSubPlaylistIndex(int index, int trackId);
    const QString clipThumbPath();

    friend class TrackModel;
    friend class TimelineModel;
    friend class TimelineItemModel;
    friend class TimelineController;
    friend struct TimelineFunctions;

protected:
    Mlt::Producer *service() const override;

    /** @brief Performs a resize of the given clip.
     *  This method is protected because it shouldn't be called directly. Call the function in the timeline instead.
     *  If a snap point is within reach, the operation will be coerced to use it.
     *  @param size is the new size of the clip
     *  @param right is true if we change the right side of the clip, false otherwise
     *  @param undo Lambda function containing the current undo stack. Will be updated with current operation
     *  @param redo Lambda function containing the current redo queue. Will be updated with current operation
     *  @return Returns true if the operation succeeded, and otherwise nothing is modified
     */
    bool requestResize(int size, bool right, Fun &undo, Fun &redo, bool logUndo = true, bool hasMix = false) override;

    /** @brief Performs a slip of the given clip
     *  This moves the in and out point of the clip without changing its size or position.
     *  @param offset How many frames in and out point should be moved
     *  @param undo Lambda function containing the current undo stack. Will be updated with current operation
     *  @param redo Lambda function containing the current redo queue. Will be updated with current operation
     *  @return Returns true if the operation succeeded, and otherwise nothing is modified
     */
    bool requestSlip(int offset, Fun &undo, Fun &redo, bool logUndo = true);

    void setCurrentTrackId(int tid, bool finalMove = true) override;
    void setPosition(int pos) override;
    void setInOut(int in, int out) override;

    /** @brief This function change the global (timeline-wise) enabled state of the effects
     */
    void setTimelineEffectsEnabled(bool enabled);

    /** @brief This functions should be called when the producer of the binClip changes, to allow refresh
     * @param state corresponds to the state of the clip we want (audio or video)
     * @param speed corresponds to the speed we need. Leave to 0 to keep current speed. Warning: this function doesn't notify the model. Unless you know what
     * you are doing, better use useTimewarProducer to change the speed
     */
    void refreshProducerFromBin(int trackId, PlaylistState::ClipState state, int stream, double speed, bool hasPitch, bool secondPlaylist = false,
                                bool timeremap = false);
    void refreshProducerFromBin(int trackId);

    /** @brief This functions replaces the current producer with a slowmotion one
       It also resizes the producer so that set of frames contained in the clip is the same
    */
    bool useTimewarpProducer(double speed, bool pitchCompensate, bool changeDuration, Fun &undo, Fun &redo);
    /** @brief Lambda that merely changes the speed (in and out are untouched) */
    Fun useTimewarpProducer_lambda(double speed, int stream, bool pitchCompensate);

    bool useTimeRemapProducer(bool enable, Fun &undo, Fun &redo);
    /** @brief Lambda that merely changes the speed (in and out are untouched) */
    Fun useTimeRemapProducer_lambda(bool enable, int audioStream, const QMap<QString, QString> &remapProperties);

    /** @brief Returns the marker model associated with this clip */
    std::shared_ptr<MarkerListModel> getMarkerModel() const;

    /** @brief Returns the number of audio channels for this clip */
    int audioChannels() const;
    /** @brief Returns the active audio stream for this clip (or -1 if we only have 1 stream */
    int audioStream() const;
    /** @brief Returns true if we have multiple audio streams in the master clip */
    bool audioMultiStream() const;
    /** @brief Returns the list of available audio stream indexes for the bin clip */
    int audioStreamIndex() const;

    bool audioEnabled() const;
    bool isAudioOnly() const;
    double getSpeed() const;

    /** @brief Returns the clip offset (calculated in the model between 2 clips from same bin clip */
    void setOffset(int offset);
    /** @brief Clears the clip offset (calculated in the model between 2 clips from same bin clip */
    void clearOffset();
    int getOffset() const;
    /** @brief Returns the producer's duration, or -1 if it can be resized without limit  */
    int getMaxDuration() const;

    /** @brief Returns the clip status (normal, proxied, missing, etc)  */
    FileStatus::ClipStatus clipStatus() const;

    /** @brief This is a debug function to ensure the clip is in a valid state */
    bool checkConsistency();

    /** @brief Resize remap keyframes */
    void requestRemapResize(int inPoint, int outPoint, int oldIn, int oldOut, Fun &undo, Fun &redo);

protected:
    std::shared_ptr<Mlt::Producer> m_producer;
    std::shared_ptr<Mlt::Producer> getProducer();

    std::shared_ptr<EffectStackModel> m_effectStack;
    std::shared_ptr<ClipSnapModel> m_clipMarkerModel;
    /** @brief This is the Id of the bin clip this clip corresponds to. */
    QString m_binClipId;

    /** @brief Whether this clip can be freely resized */
    bool m_endlessResize;

    /** @brief Used to trigger a forced thumb reload, when producer changes */
    bool forceThumbReload;

    PlaylistState::ClipState m_currentState;
    ClipType::ProducerType m_clipType;
    /** @brief Speed of the clip */
    double m_speed = -1;

    bool m_canBeVideo, m_canBeAudio;
    /** @brief Fake track id, used when dragging in insert/overwrite mode */
    int m_fakeTrack;
    int m_fakePosition;
    /** @brief Temporary val to store offset between two clips with same bin id. */
    int m_positionOffset;
    /** @brief Tracks have two sub playlists to enable same track transitions, we store in which one this clip is. */
    int m_subPlaylistIndex;

    /** @brief Remember last set track, so that we don't unnecessarily refresh the producer when deleting and re-adding a clip on same track */
    int m_lastTrackId = -1;

    /** @brief Duration of a same track mix. */
    int m_mixDuration;
    /** @brief Position of the original cut, relative to mix right side */
    int m_mixCutPos;
    /** @brief True if the clip has a timeremap effect */
    bool m_hasTimeRemap;
};
