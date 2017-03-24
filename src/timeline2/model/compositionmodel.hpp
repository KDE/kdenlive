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

namespace Mlt{
    class Transition;
}
class TimelineModel;
class TrackModel;

/* @brief This class represents a Composition object, as viewed by the backend.
   In general, the Gui associated with it will send modification queries (such as resize or move), and this class authorize them or not depending on the validity of the modifications
*/
class CompositionModel : public AssetParameterModel
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
    static int construct(std::weak_ptr<TimelineModel> parent, const QString &transitionId, int id = -1);

    /* @brief returns (unique) id of current composition
     */
    int getId() const;

    /* @brief returns the id of the track in which this composition is inserted (-1 if none)
     */
    int getCurrentTrackId() const;

    /* @brief returns the current position of the composition (-1 if not inserted)
     */
    int getPosition() const;

    int getPlaytime() const;

    /* @brief returns a property of the current composition
     */
    const QString getProperty(const QString &name) const;

    /* @brief returns the in and out times of the composition
     */
    std::pair<int, int> getInOut() const;
    int getIn() const;
    int getOut() const;
    void setInOut(int in, int out);

    friend class TrackModel;
    friend class TimelineModel;
    /* Implicit conversion operator to access the underlying producer
     */
    operator Mlt::Transition&(){ return *((Mlt::Transition*)m_asset.get());}

    /* Returns true if the underlying producer is valid
     */
    bool isValid();

protected:

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

    /* Split the current composition at the given position
    */
    void slotSplit(int position);

    /* Updates the stored position of the composition
      This function is meant to be called by the trackmodel, not directly by the user.
      If you whish to actually move the composition, use the requestMove slot.
    */
    void setPosition(int position);
    /* Updates the stored track id of the composition
       This function is meant to be called by the timeline, not directly by the user.
       If you whish to actually change the track the composition, use the slot in the timeline
       slot.
    */
    void setCurrentTrackId(int tid);

    /* Helper function to downcast the pointer. Please only use for access, do NOT store this pointer (it would violate ownership)*/
    Mlt::Transition* transition() const;
private:
    std::weak_ptr<TimelineModel> m_parent;
    int m_id; //this is the creation id of the composition, used for book-keeping
    int m_position;
    int m_currentTrackId;

};

#endif
