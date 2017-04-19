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

#ifndef MARKERLISTMODEL_H
#define MARKERLISTMODEL_H

#include "gentime.h"
#include "definitions.h"
#include "undohelper.hpp"

#include <QAbstractListModel>
#include <QReadWriteLock>

#include <map>
#include <memory>

class DocUndoStack;

/* @brief This class is the model for a list of markers.
   A marker is defined by a time, a type (the color used to represent it) and a comment string.
   We store them in a sorted fashion using a std::map

   A marker is essentially bound to a clip. We can also define guides, that are timeline-wise markers. For that, use the constructors without clipId

 */

class MarkerListModel : public QAbstractListModel
{
    Q_OBJECT

public:
    /* @brief Construct a marker list bound to the bin clip with given id */
    explicit MarkerListModel(const QString &clipId, std::weak_ptr<DocUndoStack> undo_stack, QObject *parent = nullptr);

    /* @brief Construct a guide list (bound to the timeline) */
    MarkerListModel(std::weak_ptr<DocUndoStack> undo_stack, QObject *parent = nullptr);

    enum { CommentRole = Qt::UserRole + 1, PosRole, FrameRole, ColorRole };

    /* @brief Adds a marker at the given position. If there is already one, the comment will be overriden
       @param pos defines the position of the marker, relative to the clip
       @param comment is the text associated with the marker
       @param type is the type (color) associated with the marker. If -1 is passed, then the value is pulled from kdenlive's defaults
     */
    void addMarker(GenTime pos, const QString &comment, int type = -1);

    /* @brief Removes the marker at the given position. */
    void removeMarker(GenTime pos);

    /** This describes the available markers type and their corresponding colors */
    static std::array<QColor, 5> markerTypes;

    /** Returns a marker data at given pos */
    CommentedTime getMarker(const GenTime &pos) const;

    // Mandatory overloads
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;

protected:
    /** @brief Helper function that generate a lambda to change comment / type of given marker */
    Fun changeComment_lambda(GenTime pos, const QString &comment, int type);

    /** @brief Helper function that generate a lambda to add given marker */
    Fun addMarker_lambda(GenTime pos, const QString &comment, int type);

    /** @brief Helper function that generate a lambda to remove given marker */
    Fun deleteMarker_lambda(GenTime pos);

    /** @brief Helper function that retrieves a pointer to the markermodel, given whether it's a guide model and its clipId*/
    static std::shared_ptr<MarkerListModel> getModel(bool guide, const QString &clipId);

private:
    std::weak_ptr<DocUndoStack> m_undoStack;

    bool m_guide;     // whether this model represents timeline-wise guides
    QString m_clipId; // the Id of the clip this model corresponds to, if any.

    mutable QReadWriteLock m_lock; // This is a lock that ensures safety in case of concurrent access

    std::map<GenTime, std::pair<QString, int>> m_markerList;

public:
    // this is to enable for range loops
    auto begin() -> decltype(m_markerList.begin()) { return m_markerList.begin(); }
    auto end() -> decltype(m_markerList.end()) { return m_markerList.end(); }
};
Q_DECLARE_METATYPE(MarkerListModel *)

#endif
