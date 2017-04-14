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

#include <memory>
#include <QObject>
#include "undohelper.hpp"
#include "assets/model/assetparametermodel.hpp"
#include "moveableItem.hpp"

namespace Mlt{
    class Transition;
}
class TimelineModel;
class TrackModel;

/* @brief This class represents a Composition object, as viewed by the backend.
   In general, the Gui associated with it will send modification queries (such as resize or move), and this class authorize them or not depending on the validity of the modifications
*/
class CompositionModel : public AssetParameterModel, public MoveableItem<Mlt::Transition>
{
    CompositionModel() = delete;

protected:
    /* This constructor is not meant to be called, call the static construct instead */
    CompositionModel(std::weak_ptr<TimelineModel> parent, Mlt::Transition* transition, int id, const QDomElement &transitionXml, const QString &transitionId);

public:
    /* @brief Creates a composition, which then registers itself to the parent timeline
       Returns the (unique) id of the created composition
       @param parent is a pointer to the timeline
       @param transitionId is the id of the transition to be inserted
       @param id Requested id of the clip. Automatic if -1
    */
    static int construct(const std::weak_ptr<TimelineModel>& parent, const QString &transitionId, int id = -1);

    friend class TrackModel;
    friend class TimelineModel;

    /* @brief returns the length of the item on the timeline
     */
    int getPlaytime() const override;

    /* @brief Returns the id of the second track involved in the composition (a_track in mlt's vocabulary, the b_track beeing the track where the composition is inserted)
       Important: this function returns a kdenlive id, you cannot use it directly in Mlt functions. Use timelinemodel::getTrackPosition to retrieve mlt's index
     */
    int getATrack() const;

    /* @brief Sets the id of the second track involved in the composition*/
    void setATrack(int trackId);

    /* @brief returns a property of the current item
     */
    const QString getProperty(const QString &name) const override;

protected:
    Mlt::Transition *service() const override;

    /* @brief Performs a resize of the given composition.
       Returns true if the operation succeeded, and otherwise nothing is modified
       This method is protected because it shouldn't be called directly. Call the function in the timeline instead.
       If a snap point is within reach, the operation will be coerced to use it.
       @param size is the new size of the composition
       @param right is true if we change the right side of the composition, false otherwise
       @param undo Lambda function containing the current undo stack. Will be updated with current operation
       @param redo Lambda function containing the current redo queue. Will be updated with current operation
    */
    bool requestResize(int size, bool right, Fun& undo, Fun& redo);


    int m_atrack;

};

#endif
