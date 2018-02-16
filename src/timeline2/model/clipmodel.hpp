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
class ProjectClip;
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
    ClipModel(std::shared_ptr<TimelineModel> parent, std::shared_ptr<Mlt::Producer> prod, const QString &binClipId, int id = -1);

public:
    ~ClipModel();
    /* @brief Creates a clip, which references itself to the parent timeline
       Returns the (unique) id of the created clip
       @param parent is a pointer to the timeline
       @param binClip is the id of the bin clip associated
       @param id Requested id of the clip. Automatic if -1
    */
    static int construct(const std::shared_ptr<TimelineModel> &parent, const QString &binClipId, int id = -1,
                         PlaylistState::ClipState state = PlaylistState::Original);
    /* @brief Creates a clip from an instance in MLT's playlist,
       Returns the (unique) id of the created clip
       @param parent is a pointer to the timeline
       @param binClip is the id of the bin clip associated
       @param producer is the producer to be inserted
       @param id Requested id of the clip. Automatic if -1
    */
    static int construct(const std::shared_ptr<TimelineModel> &parent, const QString &binClipId, std::shared_ptr<Mlt::Producer> producer, int id = -1);

    /* @brief returns a property of the clip, or from it's parent if it's a cut
     */
    const QString getProperty(const QString &name) const override;
    int getIntProperty(const QString &name) const;
    double getDoubleProperty(const QString &name) const;
    QSize getFrameSize() const;
    Q_INVOKABLE bool showKeyframes() const;
    Q_INVOKABLE void setShowKeyframes(bool show);
    /** @brief Returns the timeline clip status (video / audio only) */
    PlaylistState::ClipState clipState() const;
    /** @brief Sets the timeline clip status (video / audio only) */
    bool setClipState(PlaylistState::ClipState state);

    /* @brief returns the length of the item on the timeline
     */
    int getPlaytime() const override;

    /** @brief Returns audio cache data from bin clip to display audio thumbs */
    QVariant getAudioWaveform();

    /** @brief Returns the bin clip's id */
    const QString &binId() const;

    void registerClipToBin();
    void deregisterClipToBin();

    bool addEffect(const QString &effectId);
    bool copyEffect(std::shared_ptr<EffectStackModel> stackModel, int rowId);
    bool importEffects(std::shared_ptr<EffectStackModel> stackModel);
    bool removeFade(bool fromStart);
    /** @brief Adjust effects duration. Should be called after each resize / cut operation */
    bool adjustEffectLength(bool adjustFromEnd, int oldIn, int newIn, int duration, Fun &undo, Fun &redo, bool logUndo);
    bool adjustEffectLength(const QString &effectName, int duration, int originalDuration, Fun &undo, Fun &redo);
    void passTimelineProperties(std::shared_ptr <ClipModel> other);
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

    /* @brief This functions should be called when the producer of the binClip changes, to allow refresh */
    void refreshProducerFromBin(PlaylistState::ClipState state = PlaylistState::Original);
    /* @brief This functions replaces the current producer with a slowmotion one */
    bool useTimewarpProducer(double speed, int extraSpace, Fun &undo, Fun &redo);
    Fun useTimewarpProducer_lambda(double speed, int extraSpace);

    /** @brief Returns the marker model associated with this clip */
    std::shared_ptr<MarkerListModel> getMarkerModel() const;

    bool hasAudio() const;
    bool isAudioOnly() const;
    double getSpeed() const;

protected:
    std::shared_ptr<Mlt::Producer> m_producer;

    std::shared_ptr<EffectStackModel> m_effectStack;

    QString m_binClipId; // This is the Id of the bin clip this clip corresponds to.

    bool m_endlessResize; // Whether this clip can be freely resized

    bool forceThumbReload; // Used to trigger a forced thumb reload, when producer changes
};

#endif
