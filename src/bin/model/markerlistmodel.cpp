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

#include "markerlistmodel.hpp"
#include "bin/bin.h"
#include "bin/projectclip.h"
#include "core.h"
#include "dialogs/markerdialog.h"
#include "doc/docundostack.hpp"
#include "kdenlivesettings.h"
#include "macros.hpp"
#include "project/projectmanager.h"
#include "timeline2/model/snapmodel.hpp"

#include <QDebug>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <utility>

std::array<QColor, 9> MarkerListModel::markerTypes{{QColor(QLatin1String("#9b59b6")), QColor(QLatin1String("#3daee9")),QColor(QLatin1String("#1abc9c")),QColor(QLatin1String("#1cdc9a")),QColor(QLatin1String("#c9ce3b")),QColor(QLatin1String("#fdbc4b")),QColor(QLatin1String("#f39c1f")),QColor(QLatin1String("#f47750")),QColor(QLatin1String("#da4453"))}};

MarkerListModel::MarkerListModel(QString clipId, std::weak_ptr<DocUndoStack> undo_stack, QObject *parent)
    : QAbstractListModel(parent)
    , m_undoStack(std::move(undo_stack))
    , m_guide(false)
    , m_clipId(std::move(clipId))
    , m_lock(QReadWriteLock::Recursive)
{
    setup();
}

MarkerListModel::MarkerListModel(std::weak_ptr<DocUndoStack> undo_stack, QObject *parent)
    : QAbstractListModel(parent)
    , m_undoStack(std::move(undo_stack))
    , m_guide(true)
    , m_lock(QReadWriteLock::Recursive)
{
    setup();
}

void MarkerListModel::setup()
{
    // We connect the signals of the abstractitemmodel to a more generic one.
    connect(this, &MarkerListModel::columnsMoved, this, &MarkerListModel::modelChanged);
    connect(this, &MarkerListModel::columnsRemoved, this, &MarkerListModel::modelChanged);
    connect(this, &MarkerListModel::columnsInserted, this, &MarkerListModel::modelChanged);
    connect(this, &MarkerListModel::rowsMoved, this, &MarkerListModel::modelChanged);
    connect(this, &MarkerListModel::rowsRemoved, this, &MarkerListModel::modelChanged);
    connect(this, &MarkerListModel::rowsInserted, this, &MarkerListModel::modelChanged);
    connect(this, &MarkerListModel::modelReset, this, &MarkerListModel::modelChanged);
    connect(this, &MarkerListModel::dataChanged, this, &MarkerListModel::modelChanged);
}

bool MarkerListModel::hasMarker(GenTime pos) const
{
    std::map<int, CommentedTime>::const_iterator it = m_markerList.begin();
    while (it != m_markerList.end()) {
        if (it->second.time() == pos) {
            return true;
        }
        it++;
    }
    return false;
}


CommentedTime MarkerListModel::marker(GenTime pos) const
{
    std::map<int, CommentedTime>::const_iterator it = m_markerList.begin();
    while (it != m_markerList.end()) {
        if (it->second.time() == pos) {
            return it->second;
        }
        it++;
    }
    return CommentedTime();
}

bool MarkerListModel::addMarker(GenTime pos, const QString &comment, int type, Fun &undo, Fun &redo)
{
    QWriteLocker locker(&m_lock);
    Fun local_undo = []() { return true; };
    Fun local_redo = []() { return true; };
    if (type == -1) type = KdenliveSettings::default_marker_type();
    Q_ASSERT(type >= 0 && type < int(markerTypes.size()));
    
    if (hasMarker(pos)) {
        // In this case we simply change the comment and type
        CommentedTime current = marker(pos);
        local_undo = changeComment_lambda(pos, current.comment(), current.markerType());
        local_redo = changeComment_lambda(pos, comment, type);
    } else {
        // In this case we create one
        local_redo = addMarker_lambda(pos, comment, type);
        local_undo = deleteMarker_lambda(pos);
    }
    if (local_redo()) {
        UPDATE_UNDO_REDO(local_redo, local_undo, undo, redo);
        return true;
    }
    return false;
}

bool MarkerListModel::addMarkers(QMap <GenTime, QString> markers, int type)
{
    QWriteLocker locker(&m_lock);
    Fun undo = []() { return true; };
    Fun redo = []() { return true; };

    QMapIterator<GenTime, QString> i(markers);
    bool rename = false;
    bool res = true;
    while (i.hasNext() && res) {
        i.next();
        if (hasMarker(i.key())) {
            rename = true;
        }
        res = addMarker(i.key(), i.value(), type, undo, redo);
    }
    if (res) {
        if (rename) {
            PUSH_UNDO(undo, redo, m_guide ? i18n("Rename guide") : i18n("Rename marker"));
        } else {
            PUSH_UNDO(undo, redo, m_guide ? i18n("Add guide") : i18n("Add marker"));
        }
    }
    return res;
}

bool MarkerListModel::addMarker(GenTime pos, const QString &comment, int type)
{
    QWriteLocker locker(&m_lock);
    Fun undo = []() { return true; };
    Fun redo = []() { return true; };

    bool rename = hasMarker(pos);
    bool res = addMarker(pos, comment, type, undo, redo);
    if (res) {
        if (rename) {
            PUSH_UNDO(undo, redo, m_guide ? i18n("Rename guide") : i18n("Rename marker"));
        } else {
            PUSH_UNDO(undo, redo, m_guide ? i18n("Add guide") : i18n("Add marker"));
        }
    }
    return res;
}

bool MarkerListModel::removeMarker(GenTime pos, Fun &undo, Fun &redo)
{
    QWriteLocker locker(&m_lock);
    if (hasMarker(pos) == false) {
        return false;
    }
    CommentedTime current = marker(pos);
    Fun local_undo = addMarker_lambda(pos, current.comment(), current.markerType());
    Fun local_redo = deleteMarker_lambda(pos);
    if (local_redo()) {
        UPDATE_UNDO_REDO(local_redo, local_undo, undo, redo);
        return true;
    }
    return false;
}

bool MarkerListModel::removeMarker(GenTime pos)
{
    QWriteLocker locker(&m_lock);
    Fun undo = []() { return true; };
    Fun redo = []() { return true; };

    bool res = removeMarker(pos, undo, redo);
    if (res) {
        PUSH_UNDO(undo, redo, m_guide ? i18n("Delete guide") : i18n("Delete marker"));
    }
    return res;
}

bool MarkerListModel::editMarker(GenTime oldPos, GenTime pos, QString comment, int type)
{
    QWriteLocker locker(&m_lock);
    Q_ASSERT(hasMarker(oldPos));
    CommentedTime current = marker(oldPos);
    if (comment.isEmpty()) {
        comment = current.comment();
    }
    if (type == -1) {
        type = current.markerType();
    }
    if (oldPos == pos && current.comment() == comment && current.markerType() == type) return true;
    Fun undo = []() { return true; };
    Fun redo = []() { return true; };
    bool res = removeMarker(oldPos, undo, redo);
    if (res) {
        res = addMarker(pos, comment, type, undo, redo);
    }
    if (res) {
        PUSH_UNDO(undo, redo, m_guide ? i18n("Edit guide") : i18n("Edit marker"));
    } else {
        bool undone = undo();
        Q_ASSERT(undone);
    }
    return res;
}

int MarkerListModel::getRowfromId(int mid) const
{
    READ_LOCK();
    Q_ASSERT(m_markerList.count(mid) > 0);
    return (int)std::distance(m_markerList.begin(), m_markerList.find(mid));
}

int MarkerListModel::getIdFromPos(const GenTime &pos) const
{
    READ_LOCK();
    std::map<int, CommentedTime>::const_iterator it = m_markerList.begin();
    while (it != m_markerList.end()) {
        if (it->second.time() == pos) {
            return it->first;
        }
        it++;
    }
    return -1;
}

bool MarkerListModel::moveMarker(int mid, GenTime pos)
{
    QWriteLocker locker(&m_lock);
    Q_ASSERT(m_markerList.count(mid) > 0);
    if (hasMarker(pos)) {
        // A marker / guide already exists at new position, abort
        return false;
    }
    int row = getRowfromId(mid);
    m_markerList[mid].setTime(pos);
    emit dataChanged(index(row), index(row), {FrameRole});
    return true;
}

void MarkerListModel::moveMarkersWithoutUndo(QVector<int> markersId, int offset, bool updateView)
{
    QWriteLocker locker(&m_lock);
    if(markersId.length() <= 0) {
        return;
    }
    int firstRow = -1;
    int lastRow = -1;
    for (int &mid : markersId) {
        Q_ASSERT(m_markerList.count(mid) > 0);
        GenTime t = m_markerList.at(mid).time() + GenTime(offset, pCore->getCurrentFps());
        m_markerList[mid].setTime(t);
        if (!updateView) {
            continue;
        }
        if (firstRow == -1) {
            firstRow = getRowfromId(mid);
            lastRow = firstRow;
        } else {
            int row = getRowfromId(mid);
            if (row > lastRow) {
                lastRow = row;
            } else if (row < firstRow) {
                firstRow = row;
            }
        }
    }
    if (updateView) {
        emit dataChanged(index(firstRow), index(lastRow), {FrameRole});
    }
}

bool MarkerListModel::moveMarkers(QList<CommentedTime> markers, GenTime fromPos, GenTime toPos, Fun &undo, Fun &redo)
{
    QWriteLocker locker(&m_lock);

    if(markers.length() <= 0) {
        return false;
    }

    bool res = false;
    for (const auto &marker : markers) {

        GenTime oldPos = marker.time();
        QString oldComment = marker.comment();
        int oldType = marker.markerType();
        GenTime newPos = oldPos.operator+(toPos.operator-(fromPos));

        res = removeMarker(oldPos, undo, redo);
        if (res) {
            res = addMarker(newPos, oldComment, oldType, undo, redo);
        } else {
            break;
        }
    }
    return res;
}

Fun MarkerListModel::changeComment_lambda(GenTime pos, const QString &comment, int type)
{
    QWriteLocker locker(&m_lock);
    auto guide = m_guide;
    auto clipId = m_clipId;
    return [guide, clipId, pos, comment, type]() {
        auto model = getModel(guide, clipId);
        Q_ASSERT(model->hasMarker(pos));
        int mid = model->getIdFromPos(pos);
        int row = model->getRowfromId(mid);
        model->m_markerList[mid].setComment(comment);
        model->m_markerList[mid].setMarkerType(type);
        emit model->dataChanged(model->index(row), model->index(row), {CommentRole,ColorRole});
        return true;
    };
}

Fun MarkerListModel::addMarker_lambda(GenTime pos, const QString &comment, int type)
{
    QWriteLocker locker(&m_lock);
    auto guide = m_guide;
    auto clipId = m_clipId;
    return [guide, clipId, pos, comment, type]() {
        auto model = getModel(guide, clipId);
        Q_ASSERT(model->hasMarker(pos) == false);
        // We determine the row of the newly added marker
        int mid = TimelineModel::getNextId();
        int insertionRow = static_cast<int>(model->m_markerList.size());
        model->beginInsertRows(QModelIndex(), insertionRow, insertionRow);
        model->m_markerList[mid] = CommentedTime(pos, comment, type);
        model->endInsertRows();
        model->addSnapPoint(pos);
        return true;
    };
}

Fun MarkerListModel::deleteMarker_lambda(GenTime pos)
{
    QWriteLocker locker(&m_lock);
    auto guide = m_guide;
    auto clipId = m_clipId;
    return [guide, clipId, pos]() {
        auto model = getModel(guide, clipId);
        Q_ASSERT(model->hasMarker(pos));
        int mid = model->getIdFromPos(pos);
        int row = model->getRowfromId(mid);
        model->beginRemoveRows(QModelIndex(), row, row);
        model->m_markerList.erase(mid);
        model->endRemoveRows();
        model->removeSnapPoint(pos);
        return true;
    };
}

std::shared_ptr<MarkerListModel> MarkerListModel::getModel(bool guide, const QString &clipId)
{
    if (guide) {
        return pCore->projectManager()->getGuideModel();
    }
    return pCore->bin()->getBinClip(clipId)->getMarkerModel();
}

QHash<int, QByteArray> MarkerListModel::roleNames() const
{
    QHash<int, QByteArray> roles;
    roles[CommentRole] = "comment";
    roles[PosRole] = "position";
    roles[FrameRole] = "frame";
    roles[ColorRole] = "color";
    roles[TypeRole] = "type";
    roles[IdRole] = "id";
    return roles;
}

void MarkerListModel::addSnapPoint(GenTime pos)
{
    QWriteLocker locker(&m_lock);
    std::vector<std::weak_ptr<SnapInterface>> validSnapModels;
    for (const auto &snapModel : m_registeredSnaps) {
        if (auto ptr = snapModel.lock()) {
            validSnapModels.push_back(snapModel);
            ptr->addPoint(pos.frames(pCore->getCurrentFps()));
        }
    }
    // Update the list of snapModel known to be valid
    std::swap(m_registeredSnaps, validSnapModels);
}

void MarkerListModel::removeSnapPoint(GenTime pos)
{
    QWriteLocker locker(&m_lock);
    std::vector<std::weak_ptr<SnapInterface>> validSnapModels;
    for (const auto &snapModel : m_registeredSnaps) {
        if (auto ptr = snapModel.lock()) {
            validSnapModels.push_back(snapModel);
            ptr->removePoint(pos.frames(pCore->getCurrentFps()));
        }
    }
    // Update the list of snapModel known to be valid
    std::swap(m_registeredSnaps, validSnapModels);
}

QVariant MarkerListModel::data(const QModelIndex &index, int role) const
{
    READ_LOCK();
    if (index.row() < 0 || index.row() >= static_cast<int>(m_markerList.size()) || !index.isValid()) {
        return QVariant();
    }
    auto it = m_markerList.begin();
    std::advance(it, index.row());
    switch (role) {
    case Qt::DisplayRole:
    case Qt::EditRole:
    case CommentRole:
        return it->second.comment();
    case PosRole:
        return it->second.time().seconds();
    case FrameRole:
    case Qt::UserRole:
        return it->second.time().frames(pCore->getCurrentFps());
    case ColorRole:
    case Qt::DecorationRole:
        return markerTypes[size_t(it->second.markerType())];
    case TypeRole:
        return it->second.markerType();
    case IdRole:
        return it->first;
    }
    return QVariant();
}

int MarkerListModel::rowCount(const QModelIndex &parent) const
{
    READ_LOCK();
    if (parent.isValid()) return 0;
    return static_cast<int>(m_markerList.size());
}

CommentedTime MarkerListModel::getMarker(const GenTime &pos, bool *ok) const
{
    READ_LOCK();
    if (hasMarker(pos) == false) {
        // return empty marker
        *ok = false;
        return CommentedTime();
    }
    *ok = true;
    return marker(pos);
}

QList<CommentedTime> MarkerListModel::getAllMarkers() const
{
    READ_LOCK();
    QList<CommentedTime> markers;
    for (const auto &marker : m_markerList) {
        markers << marker.second;
    }
    std::sort(markers.begin(), markers.end());
    return markers;
}

QList<CommentedTime> MarkerListModel::getMarkersInRange(int start, int end) const
{
    READ_LOCK();
    QList<CommentedTime> markers;
    for (const auto &marker : m_markerList) {
        int pos = marker.second.time().frames(pCore->getCurrentFps());
        if(pos >= start && (end == -1 || pos <= end)) {
            markers << marker.second;
        }
    }
    std::sort(markers.begin(), markers.end());
    return markers;
}

int MarkerListModel::getMarkerPos(int mid) const
{
    READ_LOCK();
    Q_ASSERT(m_markerList.count(mid) > 0);
    return m_markerList.at(mid).time().frames(pCore->getCurrentFps());

}

QVector<int> MarkerListModel::getMarkersIdInRange(int start, int end) const
{
    READ_LOCK();
    QVector<int> markers;
    // Ensure we provide sorted markers list
    std::map<CommentedTime,int> sortedList;

    for(std::map<int, CommentedTime>::const_iterator it = m_markerList.begin(); it != m_markerList.end(); ++it)
        sortedList.insert(std::pair<CommentedTime, int>(it -> second, it -> first));

    for (const auto &marker : sortedList) {
        int pos = marker.first.time().frames(pCore->getCurrentFps());
        if(pos >= start && (end == -1 || pos <= end)) {
            markers << marker.second;
        }
    }
    return markers;
}

std::vector<int> MarkerListModel::getSnapPoints() const
{
    READ_LOCK();
    std::vector<int> markers;
    for (const auto &marker : m_markerList) {
        markers.push_back(marker.second.time().frames(pCore->getCurrentFps()));
    }
    return markers;
}

bool MarkerListModel::hasMarker(int frame) const
{
    READ_LOCK();
    return hasMarker(GenTime(frame, pCore->getCurrentFps()));
}

void MarkerListModel::registerSnapModel(const std::weak_ptr<SnapInterface> &snapModel)
{
    READ_LOCK();
    // make sure ptr is valid
    if (auto ptr = snapModel.lock()) {
        // ptr is valid, we store it
        m_registeredSnaps.push_back(snapModel);

        // we now add the already existing markers to the snap
        for (const auto &marker : m_markerList) {
            qDebug() << " *- *-* REGISTERING MARKER: " << marker.second.time().frames(pCore->getCurrentFps());
            ptr->addPoint(marker.second.time().frames(pCore->getCurrentFps()));
        }
    } else {
        qDebug() << "Error: added snapmodel is null";
        Q_ASSERT(false);
    }
}

bool MarkerListModel::importFromJson(const QString &data, bool ignoreConflicts, bool pushUndo)
{
    Fun undo = []() { return true; };
    Fun redo = []() { return true; };
    bool result = importFromJson(data, ignoreConflicts, undo, redo);
    if (pushUndo) {
        PUSH_UNDO(undo, redo, m_guide ? i18n("Import guides") : i18n("Import markers"));
    }
    return result;
}

bool MarkerListModel::importFromJson(const QString &data, bool ignoreConflicts, Fun &undo, Fun &redo)
{
    QWriteLocker locker(&m_lock);
    auto json = QJsonDocument::fromJson(data.toUtf8());
    if (!json.isArray()) {
        qDebug() << "Error : Json file should be an array";
        return false;
    }
    auto list = json.array();
    for (const auto &entry : qAsConst(list)) {
        if (!entry.isObject()) {
            qDebug() << "Warning : Skipping invalid marker data";
            continue;
        }
        auto entryObj = entry.toObject();
        if (!entryObj.contains(QLatin1String("pos"))) {
            qDebug() << "Warning : Skipping invalid marker data (does not contain position)";
            continue;
        }
        int pos = entryObj[QLatin1String("pos")].toInt();
        QString comment = entryObj[QLatin1String("comment")].toString(i18n("Marker"));
        int type = entryObj[QLatin1String("type")].toInt(0);
        if (type < 0 || type >= int(markerTypes.size())) {
            qDebug() << "Warning : invalid type found:" << type << " Defaulting to 0";
            type = 0;
        }
        bool res = true;
        if (!ignoreConflicts && hasMarker(GenTime(pos, pCore->getCurrentFps()))) {
            // potential conflict found, checking
            CommentedTime oldMarker = marker(GenTime(pos, pCore->getCurrentFps()));
            res = (oldMarker.comment() == comment) && (type == oldMarker.markerType());
        }
        qDebug() << "// ADDING MARKER AT POS: " << pos << ", FPS: " << pCore->getCurrentFps();
        res = res && addMarker(GenTime(pos, pCore->getCurrentFps()), comment, type, undo, redo);
        if (!res) {
            bool undone = undo();
            Q_ASSERT(undone);
            return false;
        }
    }

    return true;
}

QString MarkerListModel::toJson() const
{
    READ_LOCK();
    QJsonArray list;
    for (const auto &marker : m_markerList) {
        QJsonObject currentMarker;
        currentMarker.insert(QLatin1String("pos"), QJsonValue(marker.second.time().frames(pCore->getCurrentFps())));
        currentMarker.insert(QLatin1String("comment"), QJsonValue(marker.second.comment()));
        currentMarker.insert(QLatin1String("type"), QJsonValue(marker.second.markerType()));
        list.push_back(currentMarker);
    }
    QJsonDocument json(list);
    return QString::fromUtf8(json.toJson());
}

bool MarkerListModel::removeAllMarkers()
{
    QWriteLocker locker(&m_lock);
    std::vector<GenTime> all_pos;
    Fun local_undo = []() { return true; };
    Fun local_redo = []() { return true; };
    for (const auto &m : m_markerList) {
        all_pos.push_back(m.second.time());
    }
    bool res = true;
    for (const auto &p : all_pos) {
        res = removeMarker(p, local_undo, local_redo);
        if (!res) {
            bool undone = local_undo();
            Q_ASSERT(undone);
            return false;
        }
    }
    PUSH_UNDO(local_undo, local_redo, m_guide ? i18n("Delete all guides") : i18n("Delete all markers"));
    return true;
}

bool MarkerListModel::editMarkerGui(const GenTime &pos, QWidget *parent, bool createIfNotFound, ClipController *clip, bool createOnly)
{
    bool exists;
    auto marker = getMarker(pos, &exists);
    if(!exists && !createIfNotFound) {
        pCore->displayMessage(i18n("No guide found at current position"), InformationMessage);
    }

    if (!exists && createIfNotFound) {
        marker = CommentedTime(pos, QString());
    }

    QScopedPointer<MarkerDialog> dialog(
        new MarkerDialog(clip, marker, pCore->bin()->projectTimecode(), m_guide ? i18n("Edit guide") : i18n("Edit marker"), parent));

    if (dialog->exec() == QDialog::Accepted) {
        marker = dialog->newMarker();
        if (exists && !createOnly) {
            return editMarker(pos, marker.time(), marker.comment(), marker.markerType());
        }
        return addMarker(marker.time(), marker.comment(), marker.markerType());
    }
    return false;
}
