/***************************************************************************
 *   Copyright (C) 2017 by Jean-Baptiste Mardelle                                  *
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

#ifndef COMPOSITIONMODEL_H
#define COMPOSITIONMODEL_H

#include "assets/model/assetparametermodel.hpp"
#include "moveableItem.hpp"
#include "undohelper.hpp"
#include <memory>

namespace Mlt {
class Transition;
}
class TimelineModel;
class TrackModel;
class KeyframeModel;

/* @brief This class represents a Composition object, as viewed by the backend.
   In general, the Gui associated with it will send modification queries (such as resize or move), and this class authorize them or not depending on the
   validity of the modifications
*/
class CompositionModel : public MoveableItem<Mlt::Transition>, public AssetParameterModel
{
    CompositionModel() = delete;

protected:
    /* This constructor is not meant to be called, call the static construct instead */
    CompositionModel(std::weak_ptr<TimelineModel> parent, std::unique_ptr<Mlt::Transition> transition, int id, const QDomElement &transitionXml,
                     const QString &transitionId, const QString &originalDecimalPoint);

public:
    /* @brief Creates a composition, which then registers itself to the parent timeline
       Returns the (unique) id of the created composition
       @param parent is a pointer to the timeline
       @param transitionId is the id of the transition to be inserted
       @param id Requested id of the clip. Automatic if -1
    */
    static int construct(const std::weak_ptr<TimelineModel> &parent, const QString &transitionId, const QString &originalDecimalPoint, int id = -1,
                         std::unique_ptr<Mlt::Properties> sourceProperties = nullptr);

    friend class TrackModel;
    friend class TimelineModel;

    /* @brief returns the length of the item on the timeline
     */
    int getPlaytime() const override;

    /* @brief Returns the id of the second track involved in the composition (a_track in mlt's vocabulary, the b_track being the track where the composition is
       inserted)
     */
    int getATrack() const;
    /* @brief Defines the forced_track property. If true, the a_track will not change when composition
     * is moved to another track. When false, the a_track will automatically change to lower video track
     */
    void setForceTrack(bool force);
    /* @brief Returns the id of the second track involved in the composition (a_track) or -1 if the a_track should be automatically updated when the composition
     * changes track
     */
    int getForcedTrack() const;

    /* @brief Sets the id of the second track involved in the composition*/
    void setATrack(int trackMltPosition, int trackId);

    /* @brief returns a property of the current item
     */
    const QString getProperty(const QString &name) const override;

    /* @brief returns the active effect's keyframe model
     */
    KeyframeModel *getEffectKeyframeModel();
    Q_INVOKABLE bool showKeyframes() const;
    Q_INVOKABLE void setShowKeyframes(bool show);
    const QString &displayName() const;
    Mlt::Properties *properties();

    /* @brief Returns an XML representation of the clip with its effects */
    QDomElement toXml(QDomDocument &document);
    void setGrab(bool grab) override;
    void setSelected(bool sel) override;

protected:
    Mlt::Transition *service() const override;
    void setInOut(int in, int out) override;
    void setCurrentTrackId(int tid, bool finalMove = true) override;
    int getOut() const override;
    int getIn() const override;

    /* @brief Performs a resize of the given composition.
       Returns true if the operation succeeded, and otherwise nothing is modified
       This method is protected because it shouldn't be called directly. Call the function in the timeline instead.
       If a snap point is within reach, the operation will be coerced to use it.
       @param size is the new size of the composition
       @param right is true if we change the right side of the composition, false otherwise
       @param undo Lambda function containing the current undo stack. Will be updated with current operation
       @param redo Lambda function containing the current redo queue. Will be updated with current operation
    */
    bool requestResize(int size, bool right, Fun &undo, Fun &redo, bool logUndo = true) override;

private:
    int m_a_track;
    QString m_compositionName;
    int m_duration;
};

#endif
