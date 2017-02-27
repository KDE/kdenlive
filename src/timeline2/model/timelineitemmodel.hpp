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

#ifndef TIMELINEITEMMODEL_H
#define TIMELINEITEMMODEL_H

#include <QAbstractItemModel>
#include "undohelper.hpp"
#include "timelinemodel.hpp"


/* @brief This class is the thin wrapper around the TimelineModel that provides interface for the QML.

   It derives from AbstractItemModel to provide the model to the QML interface. An itemModel is organized with row and columns that contain the data. It can be hierarchical, meaning that a given index (row,column) can contain another level of rows and column.
   Our organization is as follows: at the top level, each row contains a track. These rows are in the same order as in the actual timeline.
   Then each of this row contains itself sub-rows that correspond to the clips.
   Here the order of these sub-rows is unrelated to the chronological order of the clips,
   but correspond to their Id order. For example, if you have three clips, with ids 12, 45 and 150, they will receive row index 0,1 and 2.
   This is because the order actually doesn't matter since the clips are rendered based on their positions rather than their row order.
   The id order has been choosed because it is consistant with a valid ordering of the clips.
   The columns are never used, so the data is always in column 0

   An ModelIndex in the ItemModel consists of a row number, a column number, and a parent index. In our case, tracks have always an empty parent, and the clip have a track index as parent.
   A ModelIndex can also store one additional integer, and we exploit this feature to store the unique ID of the object it corresponds to. 

*/
class TimelineItemModel : public QAbstractItemModel, public TimelineModel
{
Q_OBJECT

public:
    /* @brief construct a timeline object and returns a pointer to the created object
       @param undo_stack is a weak pointer to the undo stack of the project
     */
    static std::shared_ptr<TimelineItemModel> construct(Mlt::Profile *profile, std::weak_ptr<DocUndoStack> undo_stack, bool populate = false);

protected:
    /* @brief this constructor should not be called. Call the static construct instead
     */
    TimelineItemModel(Mlt::Profile *profile, std::weak_ptr<DocUndoStack> undo_stack);

public:

    ~TimelineItemModel();
    /// Two level model: tracks and clips on track
    enum {
        NameRole = Qt::UserRole + 1,
        ResourceRole,    /// clip only
        ServiceRole,     /// clip only
        IsBlankRole,     /// clip only
        StartRole,       /// clip only
        DurationRole,
        InPointRole,     /// clip only
        OutPointRole,    /// clip only
        FramerateRole,   /// clip only
        GroupedRole,     /// clip only
        IsMuteRole,      /// track only
        IsHiddenRole,    /// track only
        IsAudioRole,
        AudioLevelsRole, /// clip only
        IsCompositeRole, /// track only
        IsLockedRole,    /// track only
        HeightRole,      /// track only
        FadeInRole,      /// clip only
        FadeOutRole,     /// clip only
        IsTransitionRole,/// clip only
        FileHashRole,    /// clip only
        SpeedRole,       /// clip only
        ItemIdRole
    };

    int rowCount(const QModelIndex & parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;
    QModelIndex index(int row, int column = 0, const QModelIndex &parent = QModelIndex()) const override;
    QModelIndex makeIndex(int trackIndex, int clipIndex) const;
    /* @brief Creates an index based on the ID of the clip*/
    QModelIndex makeClipIndexFromID(int cid) const override;
    /* @brief Creates an index based on the ID of the track*/
    QModelIndex makeTrackIndexFromID(int tid) const override;
    QModelIndex parent(const QModelIndex &index) const override;
    /* @brief Returns true if track tid is blank from pos to pos + duration*/
    bool availableSpace(int tid, int pos, int duration) const;

    void notifyChange(const QModelIndex& topleft, const QModelIndex& bottomright, bool start, bool duration, bool updateThumb) override;

    virtual void _beginRemoveRows(const QModelIndex&, int , int) override;
    virtual void _beginInsertRows(const QModelIndex&, int , int) override;
    virtual void _endRemoveRows() override;
    virtual void _endInsertRows() override;
    virtual void _resetView() override;
};
#endif

