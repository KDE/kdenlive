/***************************************************************************
 *   Copyright (C) 2017 by Nicolas Carion                                  *
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

#ifndef CLIPMODEL_H
#define CLIPMODEL_H

#include "moveableItem.hpp"
#include "undohelper.hpp"
#include <QObject>
#include <memory>

namespace Mlt {
class Producer;
}
class EffectStackModel;
class MarkerListModel;
class TimelineModel;
class TrackModel;
class KeyframeModel;

/* @brief This class represents a Clip object, as viewed by the backend.
   In general, the Gui associated with it will send modification queries (such as resize or move), and this class authorize them or not depending on the
   validity of the modifications
*/
class ClipModel : public MoveableItem<Mlt::Producer>
{
    ClipModel() = delete;

protected:
    /* This constructor is not meant to be called, call the static construct instead */
    ClipModel(std::shared_ptr<TimelineModel> parent, std::shared_ptr<Mlt::Producer> prod, const QString &binClipId, int id, PlaylistState::ClipState state,
              double speed = 1.);

public:
    ~ClipModel();
    /* @brief Creates a clip, which references itself to the parent timeline
       Returns the (unique) id of the created clip
       @param parent is a pointer to the timeline
       @param binClip is the id of the bin clip associated
       @param id Requested id of the clip. Automatic if -1
    */
    static int construct(const std::shared_ptr<TimelineModel> &parent, const QString &binClipId, int id, PlaylistState::ClipState state, double speed = 1.);

    /* @brief Creates a clip, which references itself to the parent timeline
       Returns the (unique) id of the created clip
    This variants assumes a producer is already known, which should typically happen only at loading time.
    Note that there is no guarantee that this producer is actually going to be used. It might be discarded.
    */
    static int construct(const std::shared_ptr<TimelineModel> &parent, const QString &binClipId, std::shared_ptr<Mlt::Producer> producer,
                         PlaylistState::ClipState state);

    /* @brief returns a property of the clip, or from it's parent if it's a cut
     */
    const QString getProperty(const QString &name) const override;
    int getIntProperty(const QString &name) const;
    double getDoubleProperty(const QString &name) const;
    QSize getFrameSize() const;
    Q_INVOKABLE bool showKeyframes() const;
    Q_INVOKABLE void setShowKeyframes(bool show);

    /* @brief Returns true if the clip can be converted to a video clip */
    bool canBeVideo() const;
    /* @brief Returns true if the clip can be converted to an audio clip */
    bool canBeAudio() const;

    /* @brief Returns a comma separated list of effect names */
    const QString effectNames() const;

    /** @brief Returns the timeline clip status (video / audio only) */
    PlaylistState::ClipState clipState() const;
    /** @brief Returns the bin clip type (image, color, AV, ...) */
    ClipType::ProducerType clipType() const;
    /** @brief Sets the timeline clip status (video / audio only) */
    bool setClipState(PlaylistState::ClipState state, Fun &undo, Fun &redo);
    /** @brief The fake track is used in insrt/overwrote mode.
     *  in this case, dragging a clip is always accepted, but the change is not applied to the model.
     *  so we use a 'fake' track id to pass to the qml view
     */
    int getFakeTrackId() const;
    void setFakeTrackId(int fid);
    int getFakePosition() const;
    void setFakePosition(int fid);

    /* @brief Returns an XML representation of the clip with its effects */
    QDomElement toXml(QDomDocument &document);

protected:
    // helper functions that creates the lambda
    Fun setClipState_lambda(PlaylistState::ClipState state);

public:
    /* @brief returns the length of the item on the timeline
     */
    int getPlaytime() const override;

    /** @brief Returns audio cache data from bin clip to display audio thumbs */
    QVariant getAudioWaveform();

    /** @brief Returns the bin clip's id */
    const QString &binId() const;

    void registerClipToBin(std::shared_ptr<Mlt::Producer> service, bool registerProducer);
    void deregisterClipToBin();

    bool addEffect(const QString &effectId);
    bool copyEffect(std::shared_ptr<EffectStackModel> stackModel, int rowId);
    /* @brief Import effects from a different stackModel */
    bool importEffects(std::shared_ptr<EffectStackModel> stackModel);
    /* @brief Import effects from a service that contains some (another clip?) */
    bool importEffects(std::weak_ptr<Mlt::Service> service);

    bool removeFade(bool fromStart);
    /** @brief Adjust effects duration. Should be called after each resize / cut operation */
    bool adjustEffectLength(bool adjustFromEnd, int oldIn, int newIn, int oldDuration, int duration, int offset, Fun &undo, Fun &redo, bool logUndo);
    bool adjustEffectLength(const QString &effectName, int duration, int originalDuration, Fun &undo, Fun &redo);
    void passTimelineProperties(std::shared_ptr<ClipModel> other);
    KeyframeModel *getKeyframeModel();

    int fadeIn() const;
    int fadeOut() const;

    friend class TrackModel;
    friend class TimelineModel;
    friend class TimelineItemModel;
    friend class TimelineController;
    friend struct TimelineFunctions;

protected:
    Mlt::Producer *service() const override;

    /* @brief Performs a resize of the given clip.
       Returns true if the operation succeeded, and otherwise nothing is modified
       This method is protected because it shouldn't be called directly. Call the function in the timeline instead.
       If a snap point is within reach, the operation will be coerced to use it.
       @param size is the new size of the clip
       @param right is true if we change the right side of the clip, false otherwise
       @param undo Lambda function containing the current undo stack. Will be updated with current operation
       @param redo Lambda function containing the current redo queue. Will be updated with current operation
    */
    bool requestResize(int size, bool right, Fun &undo, Fun &redo, bool logUndo = true) override;

    /* @brief This function change the global (timeline-wise) enabled state of the effects
     */
    void setTimelineEffectsEnabled(bool enabled);

    /* @brief This functions should be called when the producer of the binClip changes, to allow refresh
     * @param state corresponds to the state of the clip we want (audio or video)
     * @param speed corresponds to the speed we need. Leave to 0 to keep current speed. Warning: this function doesn't notify the model. Unless you know what
     * you are doing, better use useTimewarProducer to change the speed
     */
    void refreshProducerFromBin(PlaylistState::ClipState state, double speed = 0);
    void refreshProducerFromBin();

    /* @brief This functions replaces the current producer with a slowmotion one
       It also resizes the producer so that set of frames contained in the clip is the same
    */
    bool useTimewarpProducer(double speed, Fun &undo, Fun &redo);
    // @brief Lambda that merely changes the speed (in and out are untouched)
    Fun useTimewarpProducer_lambda(double speed);

    /** @brief Returns the marker model associated with this clip */
    std::shared_ptr<MarkerListModel> getMarkerModel() const;

    /** @brief Returns the number of audio channels for this clip */
    int audioChannels() const;

    bool audioEnabled() const;
    bool isAudioOnly() const;
    double getSpeed() const;

    /*@brief This is a debug function to ensure the clip is in a valid state */
    bool checkConsistency();

protected:
    std::shared_ptr<Mlt::Producer> m_producer;
    std::shared_ptr<Mlt::Producer> getProducer();

    std::shared_ptr<EffectStackModel> m_effectStack;

    QString m_binClipId; // This is the Id of the bin clip this clip corresponds to.

    bool m_endlessResize; // Whether this clip can be freely resized

    bool forceThumbReload; // Used to trigger a forced thumb reload, when producer changes

    PlaylistState::ClipState m_currentState;
    ClipType::ProducerType m_clipType;

    double m_speed = -1; // Speed of the clip

    bool m_canBeVideo, m_canBeAudio;
    // Fake track id, used when dragging in insert/overwrite mode
    int m_fakeTrack;
    int m_fakePosition;
};

#endif
