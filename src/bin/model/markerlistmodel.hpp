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

#include "definitions.h"
#include "gentime.h"
#include "undohelper.hpp"

#include <QAbstractListModel>
#include <QReadWriteLock>

#include <array>
#include <map>
#include <memory>

class ClipController;
class DocUndoStack;
class SnapInterface;

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
    explicit MarkerListModel(QString clipId, std::weak_ptr<DocUndoStack> undo_stack, QObject *parent = nullptr);

    /* @brief Construct a guide list (bound to the timeline) */
    MarkerListModel(std::weak_ptr<DocUndoStack> undo_stack, QObject *parent = nullptr);

    enum { CommentRole = Qt::UserRole + 1, PosRole, FrameRole, ColorRole, TypeRole };

    /* @brief Adds a marker at the given position. If there is already one, the comment will be overridden
       @param pos defines the position of the marker, relative to the clip
       @param comment is the text associated with the marker
       @param type is the type (color) associated with the marker. If -1 is passed, then the value is pulled from kdenlive's defaults
     */
    bool addMarker(GenTime pos, const QString &comment, int type = -1);
    bool addMarkers(QMap <GenTime, QString> markers, int type = -1);

protected:
    /* @brief Same function but accumulates undo/redo */
    bool addMarker(GenTime pos, const QString &comment, int type, Fun &undo, Fun &redo);

public:
    /* @brief Removes the marker at the given position.
       Returns false if no marker was found at given pos
     */
    bool removeMarker(GenTime pos);
    /* @brief Delete all the markers of the model */
    bool removeAllMarkers();

    /* @brief Same function but accumulates undo/redo */
    bool removeMarker(GenTime pos, Fun &undo, Fun &redo);

public:
    /* @brief Edit a marker
       @param oldPos is the old position of the marker
       @param pos defines the new position of the marker, relative to the clip
       @param comment is the text associated with the marker
       @param type is the type (color) associated with the marker. If -1 is passed, then the value is pulled from kdenlive's defaults
    */
    bool editMarker(GenTime oldPos, GenTime pos, QString comment = QString(), int type = -1);

    /* @brief This describes the available markers type and their corresponding colors */
    static std::array<QColor, 5> markerTypes;

    /* @brief Returns a marker data at given pos */
    CommentedTime getMarker(const GenTime &pos, bool *ok) const;

    /* @brief Returns all markers in model */
    QList<CommentedTime> getAllMarkers() const;

    /* @brief Returns all markers positions in model */
    std::vector<int> getSnapPoints() const;

    /* @brief Returns true if a marker exists at given pos
       Notice that add/remove queries are done in real time (gentime), but this request is made in frame
     */
    Q_INVOKABLE bool hasMarker(int frame) const;

    /* @brief Registers a snapModel to the marker model.
       This is intended to be used for a guide model, so that the timelines can register their snapmodel to be updated when the guide moves. This is also used
       on the clip monitor to keep tracking the clip markers
       The snap logic for clips is managed from the Timeline
       Note that no deregistration is necessary, the weak_ptr will be discarded as soon as it becomes invalid.
    */
    void registerSnapModel(const std::weak_ptr<SnapInterface> &snapModel);

    /* @brief Exports the model to json using format above */
    QString toJson() const;

    /* @brief Shows a dialog to edit a marker/guide
       @param pos: position of the marker to edit, or new position for a marker
       @param widget: qt widget that will be the parent of the dialog
       @param createIfNotFound: if true, we create a marker if none is found at pos
       @param clip: pointer to the clip if we are editing a marker
       @return true if dialog was accepted and modification successful
     */
    bool editMarkerGui(const GenTime &pos, QWidget *parent, bool createIfNotFound, ClipController *clip = nullptr, bool createOnly = false);

    // Mandatory overloads
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;

public slots:
    /* @brief Imports a list of markers from json data
   The data should be formatted as follows:
   [{"pos":0.2, "comment":"marker 1", "type":1}, {...}, ...]
   return true on success and logs undo object
   @param ignoreConflicts: if set to false, it aborts if the data contains a marker with same position but different comment and/or type. If set to true,
   such markers are overridden silently
   @param pushUndo: if true, create an undo object
 */
    bool importFromJson(const QString &data, bool ignoreConflicts, bool pushUndo = true);
    bool importFromJson(const QString &data, bool ignoreConflicts, Fun &undo, Fun &redo);

protected:
    /* @brief Adds a snap point at marker position in the registered snap models
     (those that are still valid)*/
    void addSnapPoint(GenTime pos);

    /* @brief Deletes a snap point at marker position in the registered snap models
       (those that are still valid)*/
    void removeSnapPoint(GenTime pos);

    /** @brief Helper function that generate a lambda to change comment / type of given marker */
    Fun changeComment_lambda(GenTime pos, const QString &comment, int type);

    /** @brief Helper function that generate a lambda to add given marker */
    Fun addMarker_lambda(GenTime pos, const QString &comment, int type);

    /** @brief Helper function that generate a lambda to remove given marker */
    Fun deleteMarker_lambda(GenTime pos);

    /** @brief Helper function that retrieves a pointer to the markermodel, given whether it's a guide model and its clipId*/
    static std::shared_ptr<MarkerListModel> getModel(bool guide, const QString &clipId);

    /* @brief Connects the signals of this object */
    void setup();

private:
    std::weak_ptr<DocUndoStack> m_undoStack;

    bool m_guide;     // whether this model represents timeline-wise guides
    QString m_clipId; // the Id of the clip this model corresponds to, if any.

    mutable QReadWriteLock m_lock; // This is a lock that ensures safety in case of concurrent access

    std::map<GenTime, std::pair<QString, int>> m_markerList;
    std::vector<std::weak_ptr<SnapInterface>> m_registeredSnaps;

signals:
    void modelChanged();

public:
    // this is to enable for range loops
    auto begin() -> decltype(m_markerList.begin()) { return m_markerList.begin(); }
    auto end() -> decltype(m_markerList.end()) { return m_markerList.end(); }
};
Q_DECLARE_METATYPE(MarkerListModel *)

#endif
