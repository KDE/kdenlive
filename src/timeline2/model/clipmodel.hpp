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

/* @brief This class represents a Clip object, as viewed by the backend.
   In general, the Gui associated with it will send modification queries (such as resize or move), and this class authorize them or not depending on the validity of the modifications
*/
class ClipModel : public QObject
{
    Q_OBJECT
    ClipModel() = delete;

public:
    /* This constructor is not meant to be called, call the static construct instead */
    ClipModel(std::weak_ptr<TimelineModel> parent, std::weak_ptr<Mlt::Producer> prod);


    /* @brief Creates a clip, which references itself to the parent timeline
       Returns the (unique) id of the created clip
       @param parent is a pointer to the timeline
       @param producer is the producer to be inserted
    */
    static int construct(std::weak_ptr<TimelineModel> parent, std::weak_ptr<Mlt::Producer> prod);

    /* @brief returns (unique) id of current clip
     */
    int getId() const;
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

private:
    std::weak_ptr<TimelineModel> m_parent;
    int m_id; //this is the creation id of the clip, used for book-keeping

    std::weak_ptr<Mlt::Producer> m_producer;
    static int next_id; //next valid id to assign

};
