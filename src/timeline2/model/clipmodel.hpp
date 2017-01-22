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

#include <memory>
#include <QObject>

namespace Mlt{
    class Producer;
}
class TimelineModel;
class TrackModel;

/* @brief This class represents a Clip object, as viewed by the backend.
   In general, the Gui associated with it will send modification queries (such as resize or move), and this class authorize them or not depending on the validity of the modifications
*/
class ClipModel : public QObject
{
    Q_OBJECT
    ClipModel() = delete;

protected:
    /* This constructor is not meant to be called, call the static construct instead */
    ClipModel(std::weak_ptr<TimelineModel> parent, std::weak_ptr<Mlt::Producer> prod);

public:


    /* @brief Creates a clip, which references itself to the parent timeline
       Returns the (unique) id of the created clip
       @param parent is a pointer to the timeline
       @param producer is the producer to be inserted
    */
    static int construct(std::weak_ptr<TimelineModel> parent, std::shared_ptr<Mlt::Producer> prod);

    /* @brief The destructor. It asks the parent to be deleted
     */
    void destruct();

    /* @brief returns (unique) id of current clip
     */
    int getId() const;

    /* @brief returns the length of the clip on the timeline
     */
    int getPlaytime();
    /* @brief returns the id of the track in which this clips is inserted (-1 if none)
     */
    int getCurrentTrackId() const;
    /* @brief returns the current position of the clip (-1 if not inserted)
     */
    int getPosition() const;

    friend class TrackModel;
    friend class TimelineModel;
    /* Implicit conversion operator to access the underlying producer
     */
    operator Mlt::Producer&(){ return *m_producer;}
public:

    /* This is called whenever a resize of the clip is issued
       If the resize is accepted, it should send back a signal to the QML interface.
       If a snap point is withing reach, the operation will be coerced to use it.
    */
    void slotRequestResize(int newSize);

    /* This is called whenever a move of the clip is issued
       If the move is accepted, it should send back a signal to the QML interface.
       If a snap point is withing reach, the operation will be coerced to use it.
    */
    void slotRequestMove(int newPosition);

    /* Delete the current clip
    */
    void slotDelete();

    /* Split the current clip at the given position
    */
    void slotSplit(int position);

signals:
    void signalSizeChanged(int);
    void signalPositionChanged(int);

protected:
    /* Updates the stored position of the clip
      This function is meant to be called by the trackmodel, not directly by the user.
      If you whish to actually move the clip, use the requestMove slot.
    */
    void setPosition(int position);
    /* Updates the stored track id of the clip
       This function is meant to be called by the timeline, not directly by the user.
       If you whish to actually change the track the clip, use the slot in the timeline
       slot.
    */
    void setCurrentTrackId(int tid);
private:
    std::weak_ptr<TimelineModel> m_parent;
    int m_id; //this is the creation id of the clip, used for book-keeping
    int m_position;
    int m_currentTrackId;

    std::shared_ptr<Mlt::Producer> m_producer;

};
