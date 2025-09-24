/*
    SPDX-FileCopyrightText: 2017 Nicolas Carion
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "markerlistmodel.hpp"
#include "bin/projectclip.h"
#include "bin/projectitemmodel.h"
#include "core.h"
#include "dialogs/exportguidesdialog.h"
#include "dialogs/markerdialog.h"
#include "doc/docundostack.hpp"
#include "doc/kdenlivedoc.h"
#include "kdenlivesettings.h"
#include "macros.hpp"
#include "project/projectmanager.h"
#include "timeline2/model/snapmodel.hpp"

#include <QDebug>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <utility>

MarkerListModel::MarkerListModel(QString clipId, std::weak_ptr<DocUndoStack> undo_stack, QObject *parent)
    : QAbstractListModel(parent)
    , m_undoStack(std::move(undo_stack))
    , m_clipId(std::move(clipId))
    , m_lock(QReadWriteLock::Recursive)
{
    setup();
}

const QString &MarkerListModel::ownerId() const
{
    return m_clipId;
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

void MarkerListModel::loadCategoriesWithUndo(const QStringList &categories, const QStringList &currentCategories, const QMap<int, int> remapCategories)
{
    // Remove all markers of deleted category
    Fun local_undo = []() { return true; };
    Fun local_redo = []() { return true; };
    QList<int> deletedCategories = loadCategories(categories);
    while (!deletedCategories.isEmpty()) {
        int ix = deletedCategories.takeFirst();
        const QList<CommentedTime> toDelete = getAllMarkers(ix);
        if (remapCategories.contains(ix)) {
            int newType = remapCategories.value(ix);
            for (const auto &c : toDelete) {
                addMarker(c.time(), c.comment(), newType, local_undo, local_redo);
            }
        } else {
            for (const auto &c : toDelete) {
                removeMarker(c.time(), local_undo, local_redo);
            }
        }
    }
    Fun undo = [this, currentCategories]() {
        loadCategories(currentCategories);
        return true;
    };
    Fun redo = [this, categories]() {
        loadCategories(categories);
        return true;
    };
    PUSH_FRONT_LAMBDA(local_redo, redo);
    PUSH_LAMBDA(local_undo, undo);
    pCore->pushUndo(undo, redo, i18n("Update timeline markers categories"));
}

QList<int> MarkerListModel::loadCategories(const QStringList &categories, bool notify)
{
    QList<int> previousCategories = pCore->markerTypes.keys();
    pCore->markerTypes.clear();
    for (const QString &cat : categories) {
        if (cat.count(QLatin1Char(':')) < 2) {
            // Invalid guide data found
            qDebug() << "Invalid guide data found: " << cat;
            continue;
        }
        const QColor color(cat.section(QLatin1Char(':'), -1).simplified());
        const QString name = cat.section(QLatin1Char(':'), 0, -3);
        int ix = cat.section(QLatin1Char(':'), -2, -2).toInt();
        previousCategories.removeAll(ix);
        pCore->markerTypes.insert(ix, {color, name});
    }
    Q_EMIT categoriesChanged();
    // Trigger a refresh of all markers
    if (notify) {
        Q_EMIT dataChanged(index(0), index(m_markerList.size() - 1), {ColorRole});
        // Save Categories in document
        Q_EMIT pCore->saveGuideCategories();
    }
    return previousCategories;
}

const QStringList MarkerListModel::categoriesToStringList() const
{
    QStringList categories;
    QMapIterator<int, Core::MarkerCategory> i(pCore->markerTypes);
    while (i.hasNext()) {
        i.next();
        categories << QStringLiteral("%1:%2:%3").arg(i.value().displayName, QString::number(i.key()), i.value().color.name());
    }
    return categories;
}

QStringList MarkerListModel::guideCategoriesToStringList(const QString &categoriesData)
{
    QStringList categories;
    if (categoriesData.isEmpty()) {
        return categories;
    }
    auto json = QJsonDocument::fromJson(categoriesData.toUtf8());
    if (!json.isArray()) {
        qDebug() << "Error : Json file should be an array";
        return categories;
    }
    auto list = json.array();
    for (const auto &entry : std::as_const(list)) {
        if (!entry.isObject()) {
            qDebug() << "Warning : Skipping invalid category data";
            continue;
        }
        auto entryObj = entry.toObject();
        if (!entryObj.contains(QLatin1String("index"))) {
            qDebug() << "Warning : Skipping invalid category data (does not contain index)";
            continue;
        }
        int ix = entryObj[QLatin1String("index")].toInt();
        QString comment = entryObj[QLatin1String("comment")].toString();
        QString color = entryObj[QLatin1String("color")].toString();
        categories << QStringLiteral("%1:%2:%3").arg(comment, QString::number(ix), color);
    }
    return categories;
}

const QString MarkerListModel::categoriesListToJSon(const QStringList categories)
{
    QJsonArray list;
    for (auto &cat : categories) {
        QJsonObject currentMarker;
        const QColor color(cat.section(QLatin1Char(':'), -1).simplified());
        const QString name = cat.section(QLatin1Char(':'), 0, -3);
        int ix = cat.section(QLatin1Char(':'), -2, -2).toInt();
        currentMarker.insert(QLatin1String("index"), QJsonValue(ix));
        currentMarker.insert(QLatin1String("comment"), QJsonValue(name));
        currentMarker.insert(QLatin1String("color"), QJsonValue(color.name()));
        list.push_back(currentMarker);
    }
    QJsonDocument json(list);
    return QString::fromUtf8(json.toJson());
}

const QString MarkerListModel::categoriesToJSon() const
{
    QJsonArray list;
    QMapIterator<int, Core::MarkerCategory> i(pCore->markerTypes);
    while (i.hasNext()) {
        i.next();
        QJsonObject currentMarker;
        currentMarker.insert(QLatin1String("index"), QJsonValue(i.key()));
        currentMarker.insert(QLatin1String("comment"), QJsonValue(i.value().displayName));
        currentMarker.insert(QLatin1String("color"), QJsonValue(i.value().color.name()));
        list.push_back(currentMarker);
    }
    QJsonDocument json(list);
    return QString::fromUtf8(json.toJson());
}

int MarkerListModel::markerIdAtFrame(int pos) const
{
    if (m_markerPositions.contains(pos)) {
        return m_markerPositions.value(pos);
    }
    return -1;
}

bool MarkerListModel::hasMarker(GenTime pos) const
{
    int frame = pos.frames(pCore->getCurrentFps());
    return hasMarker(frame);
}

CommentedTime MarkerListModel::markerById(int mid) const
{
    Q_ASSERT(std::find(m_markerPositions.cbegin(), m_markerPositions.cend(), mid) != m_markerPositions.cend());
    return m_markerList.at(mid);
}

CommentedTime MarkerListModel::marker(int frame) const
{
    int mid = markerIdAtFrame(frame);
    if (mid > -1) {
        return m_markerList.at(mid);
    }
    return CommentedTime();
}

CommentedTime MarkerListModel::marker(GenTime pos) const
{
    int mid = markerIdAtFrame(pos.frames(pCore->getCurrentFps()));
    if (mid > -1) {
        return m_markerList.at(mid);
    }
    return CommentedTime();
}

bool MarkerListModel::addMarker(GenTime pos, const QString &comment, int type, Fun &undo, Fun &redo)
{
    QWriteLocker locker(&m_lock);
    Fun local_undo = []() { return true; };
    Fun local_redo = []() { return true; };
    if (type == -1) type = KdenliveSettings::default_marker_type();
    Q_ASSERT(pCore->markerTypes.contains(type));
    if (hasMarker(pos)) {
        // In this case we simply change the comment and type
        CommentedTime current = marker(pos);
        local_undo = addOrUpdateRangeMarker_lambda(pos, current.duration(), current.comment(), current.markerType(), &current);
        local_redo = addOrUpdateRangeMarker_lambda(pos, GenTime(0), comment, type, &current);
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

bool MarkerListModel::addMarkers(const QMap<GenTime, QString> &markers, int type)
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
            PUSH_UNDO(undo, redo, i18n("Rename marker"));
        } else {
            PUSH_UNDO(undo, redo, i18n("Add marker"));
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
            PUSH_UNDO(undo, redo, i18n("Rename marker"));
        } else {
            PUSH_UNDO(undo, redo, i18n("Add marker"));
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
    Fun local_undo = addOrUpdateRangeMarker_lambda(pos, current.duration(), current.comment(), current.markerType(), nullptr);
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
        PUSH_UNDO(undo, redo, i18n("Delete marker"));
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
    return editMarker(oldPos, pos, comment, type, current.duration());
}

bool MarkerListModel::editMarker(GenTime oldPos, GenTime pos, QString comment, int type, GenTime duration)
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
    if (oldPos == pos && current.comment() == comment && current.markerType() == type && current.duration() == duration) return true;
    Fun undo = []() { return true; };
    Fun redo = []() { return true; };
    bool res = true;
    CommentedTime oldState = current;
    int mid = getIdFromPos(oldPos);
    Q_ASSERT(mid != -1);
    Fun local_redo = [this, oldPos, pos, comment, type, duration]() {
        CommentedTime newMarker(pos, comment, type, duration);
        int mid = getIdFromPos(oldPos);
        if (oldPos != pos) {
            Q_ASSERT(m_markerPositions.contains(oldPos.frames(pCore->getCurrentFps())));
            int row = getRowfromId(mid);
            beginRemoveRows(QModelIndex(), row, row);
            m_markerList.erase(mid);
            m_markerPositions.remove(oldPos.frames(pCore->getCurrentFps()));
            endRemoveRows();
            removeSnapPoint(oldPos);
            mid = TimelineModel::getNextId();
            int insertionRow = static_cast<int>(m_markerList.size());
            beginInsertRows(QModelIndex(), insertionRow, insertionRow);
            m_markerList.insert({mid, newMarker});
            m_markerPositions.insert(pos.frames(pCore->getCurrentFps()), mid);
            endInsertRows();
            addSnapPoint(pos);
        } else {
            m_markerList[mid] = newMarker;
        }
        int row = getRowfromId(mid);
        Q_EMIT dataChanged(index(row), index(row), {CommentRole, ColorRole, DurationRole, EndPosRole, HasRangeRole, PosRole, FrameRole});
        return true;
    };

    res = local_redo();
    if (res) {
        Fun local_undo = [this, oldPos, pos, oldState]() {
            int mid = getIdFromPos(pos);
            if (oldPos != pos) {
                Q_ASSERT(m_markerPositions.contains(pos.frames(pCore->getCurrentFps())));
                int row = getRowfromId(mid);
                beginRemoveRows(QModelIndex(), row, row);
                m_markerList.erase(mid);
                m_markerPositions.remove(pos.frames(pCore->getCurrentFps()));
                endRemoveRows();
                removeSnapPoint(pos);
                mid = TimelineModel::getNextId();
                int insertionRow = static_cast<int>(m_markerList.size());
                beginInsertRows(QModelIndex(), insertionRow, insertionRow);
                m_markerList.insert({mid, oldState});
                m_markerPositions.insert(oldPos.frames(pCore->getCurrentFps()), mid);
                endInsertRows();
                addSnapPoint(oldPos);

            } else {
                m_markerList[mid] = oldState;
            }
            int row = getRowfromId(mid);
            Q_EMIT dataChanged(index(row), index(row), {CommentRole, ColorRole, DurationRole, EndPosRole, HasRangeRole, PosRole, FrameRole});
            return true;
        };
        UPDATE_UNDO_REDO(local_redo, local_undo, undo, redo);
        QString undoText;
        if (oldPos != pos) {
            undoText = i18n("Move marker");
        } else if (current.duration() != duration) {
            undoText = i18n("Resize marker");
        } else {
            undoText = i18n("Edit marker");
        }
        PUSH_UNDO(undo, redo, undoText);
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
    int frame = pos.frames(pCore->getCurrentFps());
    return getIdFromPos(frame);
}

int MarkerListModel::getIdFromPos(int frame) const
{
    if (m_markerPositions.contains(frame)) {
        return m_markerPositions.value(frame);
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
    int oldPos = m_markerList.at(mid).time().frames(pCore->getCurrentFps());
    m_markerList[mid].setTime(pos);
    m_markerPositions.remove(oldPos);
    m_markerPositions.insert(pos.frames(pCore->getCurrentFps()), mid);
    Q_EMIT dataChanged(index(row), index(row), {FrameRole});
    return true;
}

void MarkerListModel::moveMarkersWithoutUndo(const QVector<int> &markersId, int offset, bool updateView)
{
    QWriteLocker locker(&m_lock);
    if (markersId.length() <= 0) {
        return;
    }
    int firstRow = -1;
    int lastRow = -1;
    for (auto mid : markersId) {
        Q_ASSERT(m_markerList.count(mid) > 0);
        GenTime t = m_markerList.at(mid).time();
        m_markerPositions.remove(t.frames(pCore->getCurrentFps()));
        t += GenTime(offset, pCore->getCurrentFps());
        m_markerPositions.insert(t.frames(pCore->getCurrentFps()), mid);
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
        Q_EMIT dataChanged(index(firstRow), index(lastRow), {FrameRole});
    }
}

bool MarkerListModel::moveMarkers(const QList<CommentedTime> &markers, GenTime fromPos, GenTime toPos, Fun &undo, Fun &redo)
{
    QWriteLocker locker(&m_lock);

    if (markers.length() <= 0) {
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
    return [pos, comment, type, this]() {
        Q_ASSERT(hasMarker(pos));
        int mid = getIdFromPos(pos);
        int row = getRowfromId(mid);
        m_markerList[mid].setComment(comment);
        m_markerList[mid].setMarkerType(type);
        Q_EMIT dataChanged(index(row), index(row), {CommentRole, ColorRole});
        return true;
    };
}

Fun MarkerListModel::addMarker_lambda(GenTime pos, const QString &comment, int type)
{
    QWriteLocker locker(&m_lock);
    return [pos, comment, type, this]() {
        Q_ASSERT(hasMarker(pos) == false);
        // We determine the row of the newly added marker
        int mid = TimelineModel::getNextId();
        int insertionRow = static_cast<int>(m_markerList.size());
        beginInsertRows(QModelIndex(), insertionRow, insertionRow);
        m_markerList[mid] = CommentedTime(pos, comment, type);
        m_markerPositions.insert(pos.frames(pCore->getCurrentFps()), mid);
        endInsertRows();
        addSnapPoint(pos);
        return true;
    };
}

Fun MarkerListModel::addOrUpdateRangeMarker_lambda(GenTime pos, GenTime duration, const QString &comment, int type, const CommentedTime *existingMarker)
{
    QWriteLocker locker(&m_lock);
    return [pos, duration, comment, type, existingMarker, this]() {
        int mid;
        if (existingMarker) {
            mid = getIdFromPos(pos);
            Q_ASSERT(mid != -1);
            int row = getRowfromId(mid);
            m_markerList[mid].setComment(comment);
            m_markerList[mid].setMarkerType(type);
            m_markerList[mid].setDuration(duration);
            Q_EMIT dataChanged(index(row), index(row), {CommentRole, ColorRole, DurationRole, EndPosRole, HasRangeRole});
        } else {
            Q_ASSERT(hasMarker(pos) == false);
            mid = TimelineModel::getNextId();
            int insertionRow = static_cast<int>(m_markerList.size());
            beginInsertRows(QModelIndex(), insertionRow, insertionRow);
            m_markerList[mid] = CommentedTime(pos, comment, type, duration);
            m_markerPositions.insert(pos.frames(pCore->getCurrentFps()), mid);
            endInsertRows();
            addSnapPoint(pos);
        }
        return true;
    };
}

Fun MarkerListModel::deleteMarker_lambda(GenTime pos)
{
    QWriteLocker locker(&m_lock);
    return [pos, this]() {
        Q_ASSERT(hasMarker(pos));
        int mid = getIdFromPos(pos);
        int row = getRowfromId(mid);
        beginRemoveRows(QModelIndex(), row, row);
        m_markerList.erase(mid);
        m_markerPositions.remove(pos.frames(pCore->getCurrentFps()));
        endRemoveRows();
        removeSnapPoint(pos);
        return true;
    };
}

std::shared_ptr<MarkerListModel> MarkerListModel::getModel(const QString &clipId)
{
    return pCore->projectItemModel()->getClipByBinID(clipId)->getMarkerModel();
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
    roles[TCRole] = "timecode";
    roles[ClipIdRole] = "clipId";
    roles[DurationRole] = "duration";
    roles[EndPosRole] = "endPos";
    roles[HasRangeRole] = "hasRange";
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
    case CommentRole: {
        return it->second.comment();
    }
    case PosRole:
        return it->second.time().seconds();
    case FrameRole:
    case Qt::UserRole:
        return it->second.time().frames(pCore->getCurrentFps());
    case ColorRole:
    case Qt::DecorationRole:
        return pCore->markerTypes.value(it->second.markerType()).color;
    case TypeRole:
        return it->second.markerType();
    case IdRole:
        return it->first;
    case TCRole:
        return pCore->timecode().getDisplayTimecode(it->second.time(), false);
    case ClipIdRole:
        return m_clipId;
    case DurationRole:
        return it->second.duration().frames(pCore->getCurrentFps());
    case EndPosRole:
        return it->second.endTime().frames(pCore->getCurrentFps());
    case HasRangeRole:
        return it->second.hasRange();
    }
    return QVariant();
}

int MarkerListModel::rowCount(const QModelIndex &parent) const
{
    READ_LOCK();
    if (parent.isValid()) return 0;
    return static_cast<int>(m_markerList.size());
}

CommentedTime MarkerListModel::getMarker(int frame, bool *ok) const
{
    READ_LOCK();
    if (hasMarker(frame) == false) {
        // return empty marker
        *ok = false;
        return CommentedTime();
    }
    *ok = true;
    return marker(frame);
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

QList<CommentedTime> MarkerListModel::getAllMarkers(int type) const
{
    READ_LOCK();
    QList<CommentedTime> markers;
    for (const auto &marker : m_markerList) {
        if (type == -1 || marker.second.markerType() == type) {
            markers << marker.second;
        }
    }
    std::sort(markers.begin(), markers.end());
    return markers;
}

QList<CommentedTime> MarkerListModel::getMarkersInRange(int start, int end) const
{
    QList<CommentedTime> markers;
    const QVector<int> mids = getMarkersIdInRange(start, end);
    // Now extract markers
    READ_LOCK();
    for (const auto &marker : mids) {
        markers << m_markerList.at(marker);
    }
    std::sort(markers.begin(), markers.end());
    return markers;
}

int MarkerListModel::getMarkerPos(int mid) const
{
    READ_LOCK();
    Q_ASSERT(m_markerList.count(mid) > 0);
    return m_markerPositions.key(mid);
}

QVector<int> MarkerListModel::getMarkersIdInRange(int start, int end) const
{
    READ_LOCK();
    // First find marker ids in range
    QVector<int> markers;
    QMap<int, int>::const_iterator i = m_markerPositions.constBegin();
    while (i != m_markerPositions.constEnd()) {
        if (end > -1 && i.key() > end) {
            break;
        }
        if (i.key() >= start) {
            markers << i.value();
        }
        ++i;
    }
    return markers;
}

std::vector<int> MarkerListModel::getSnapPoints() const
{
    READ_LOCK();
    const QList<int> positions = m_markerPositions.keys();
    std::vector<int> markers(positions.cbegin(), positions.cend());
    return markers;
}

bool MarkerListModel::hasMarker(int frame) const
{
    READ_LOCK();
    return m_markerPositions.contains(frame);
}

void MarkerListModel::registerSnapModel(const std::weak_ptr<SnapInterface> &snapModel)
{
    READ_LOCK();
    // make sure ptr is valid
    if (auto ptr = snapModel.lock()) {
        // ptr is valid, we store it
        m_registeredSnaps.push_back(snapModel);

        // we now add the already existing markers to the snap
        QMap<int, int>::const_iterator i = m_markerPositions.constBegin();
        while (i != m_markerPositions.constEnd()) {
            ptr->addPoint(i.key());
            ++i;
        }
    } else {
        qDebug() << "Error: added snapmodel is null";
        Q_ASSERT(false);
    }
}

bool MarkerListModel::importFromFile(const QString &fileData, bool ignoreConflicts)
{
    Fun undo = []() { return true; };
    Fun redo = []() { return true; };
    bool res = importFromJson(fileData, ignoreConflicts, undo, redo);
    if (!res) {
        res = importFromTxt(fileData, undo, redo);
    }
    if (res) {
        PUSH_UNDO(undo, redo, i18n("Import markers"));
    }
    return res;
}

bool MarkerListModel::importFromJson(const QString &data, bool ignoreConflicts, bool pushUndo)
{
    Fun undo = []() { return true; };
    Fun redo = []() { return true; };
    bool result = importFromJson(data, ignoreConflicts, undo, redo);
    if (result && pushUndo) {
        PUSH_UNDO(undo, redo, i18n("Import markers"));
    }
    return result;
}

bool MarkerListModel::importFromJson(const QString &data, bool ignoreConflicts, Fun &undo, Fun &redo)
{
    if (data.isEmpty()) {
        return false;
    }
    QWriteLocker locker(&m_lock);
    auto json = QJsonDocument::fromJson(data.toUtf8());
    if (!json.isArray()) {
        qDebug() << "Error : Json file should be an array";
        return false;
    }
    auto list = json.array();
    for (const auto &entry : std::as_const(list)) {
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
        int duration = entryObj[QLatin1String("duration")].toInt(0); // Default to 0 for backward compatibility
        if (!pCore->markerTypes.contains(type)) {
            qDebug() << "Warning : invalid type found:" << type << " Recovering category";
            type = type % 9;
            if (!pCore->markerTypes.contains(type)) {
                QString originalCategory = KdenliveDoc::getDefaultGuideCategories().at(type);
                QColor color(originalCategory.section(QLatin1Char(':'), -1));
                pCore->markerTypes.insert(type, {color, i18n("Recovered %1", type)});
                Q_EMIT categoriesChanged();
                Q_EMIT pCore->updateDefaultMarkerCategory();
            }
        }
        bool res = true;
        if (!ignoreConflicts && hasMarker(GenTime(pos, pCore->getCurrentFps()))) {
            // potential conflict found, checking
            CommentedTime oldMarker = marker(GenTime(pos, pCore->getCurrentFps()));
            res = (oldMarker.comment() == comment) && (type == oldMarker.markerType()) && (oldMarker.duration().frames(pCore->getCurrentFps()) == duration);
        }
        qDebug() << "// ADDING MARKER AT POS: " << pos << ", FPS: " << pCore->getCurrentFps();
        if (duration > 0) {
            res = res && addRangeMarker(GenTime(pos, pCore->getCurrentFps()), GenTime(duration, pCore->getCurrentFps()), comment, type, undo, redo);
        } else {
            res = res && addMarker(GenTime(pos, pCore->getCurrentFps()), comment, type, undo, redo);
        }
        if (!res) {
            bool undone = undo();
            Q_ASSERT(undone);
            return false;
        }
    }
    return true;
}

bool MarkerListModel::importFromTxt(const QString &fileData, Fun &undo, Fun &redo)
{
    QWriteLocker locker(&m_lock);
    bool res = false;
    bool lineRead = false;
    int type = KdenliveSettings::default_marker_type();
    const QStringList lines = fileData.split(QLatin1Char('\n'));
    for (auto &line : lines) {
        if (line.isEmpty()) {
            continue;
        }
        QString pos = line.section(QLatin1Char(' '), 0, 0);
        GenTime position;
        // Try to read timecode
        bool ok = false;
        int separatorsCount = pos.count(QLatin1Char(':'));
        switch (separatorsCount) {
        case 0:
            // assume we are dealing with seconds
            position = GenTime(pos.toDouble(&ok));
            break;
        case 1: {
            // assume min:sec
            QString sec = pos.section(QLatin1Char(':'), 1);
            QString min = pos.section(QLatin1Char(':'), 0, 0);
            double seconds = sec.toDouble(&ok);
            int minutes = ok ? min.toInt(&ok) : 0;
            position = GenTime(seconds + 60 * minutes);
            break;
        }
        case 2:
        default: {
            // assume hh:min:sec
            QString sec = pos.section(QLatin1Char(':'), 2, 2);
            QString min = pos.section(QLatin1Char(':'), 1, 1);
            QString hours = pos.section(QLatin1Char(':'), 0, 0);
            double seconds = sec.toDouble(&ok);
            int minutes = ok ? min.toInt(&ok) : 0;
            int h = ok ? hours.toInt(&ok) : 0;
            position = GenTime(seconds + (60 * minutes) + (3600 * h));
            break;
        }
        }
        if (!ok) {
            // Could not read timecode
            qDebug() << "::: Could not read timecode from line: " << line;
            continue;
        }

        QString comment = line.section(QLatin1Char(' '), 1);
        GenTime duration(0);

        if (comment.endsWith(']')) {
            int bracketStart = comment.lastIndexOf('[');
            if (bracketStart != -1 && bracketStart > 0) {
                QString durationStr = comment.mid(bracketStart + 1, comment.length() - bracketStart - 2);

                bool ok;
                double durationSeconds = durationStr.toDouble(&ok);
                if (ok && durationSeconds > 0) {
                    duration = GenTime(durationSeconds);
                    comment = comment.left(bracketStart).trimmed();
                }
            }
        }

        if (duration > GenTime(0)) {
            res = addRangeMarker(position, duration, comment, type, undo, redo);
        } else {
            res = addMarker(position, comment, type, undo, redo);
        }

        if (!res) {
            break;
        } else if (!lineRead) {
            lineRead = true;
        }
    }
    return res && lineRead;
}

QString MarkerListModel::toJson(QList<int> categories) const
{
    READ_LOCK();
    QJsonArray list;
    bool exportAllCategories = categories.isEmpty() || categories == (QList<int>() << -1);
    QList<CommentedTime> markers;
    for (const auto &marker : m_markerList) {
        if (exportAllCategories || categories.contains(marker.second.markerType())) {
            markers << marker.second;
        }
    }
    std::sort(markers.begin(), markers.end());
    for (const auto &marker : std::as_const(markers)) {
        QJsonObject currentMarker;
        currentMarker.insert(QLatin1String("pos"), QJsonValue(marker.time().frames(pCore->getCurrentFps())));
        currentMarker.insert(QLatin1String("comment"), QJsonValue(marker.comment()));
        currentMarker.insert(QLatin1String("type"), QJsonValue(marker.markerType()));
        currentMarker.insert(QLatin1String("duration"), QJsonValue(marker.duration().frames(pCore->getCurrentFps())));
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
    PUSH_UNDO(local_undo, local_redo, i18n("Delete all markers"));
    return true;
}

bool MarkerListModel::editMultipleMarkersGui(const QList<GenTime> positions, QWidget *parent)
{
    bool exists;
    auto marker = getMarker(positions.first(), &exists);
    if (!exists) {
        pCore->displayMessage(i18n("No marker found at current position"), InformationMessage);
    }
    QDialog d(parent);
    d.setWindowTitle(i18n("Edit Markers Category"));
    QDialogButtonBox *buttonBox = new QDialogButtonBox(QDialogButtonBox::Cancel | QDialogButtonBox::Ok);
    auto *l = new QVBoxLayout;
    d.setLayout(l);
    d.connect(buttonBox, &QDialogButtonBox::rejected, &d, &QDialog::reject);
    d.connect(buttonBox, &QDialogButtonBox::accepted, &d, &QDialog::accept);
    QLabel lab(i18n("Markers Category"), &d);
    MarkerCategoryChooser chooser(&d);
    chooser.setMarkerModel(this);
    chooser.setAllowAll(false);
    chooser.setCurrentCategory(marker.markerType());
    l->addWidget(&lab);
    l->addWidget(&chooser);
    l->addWidget(buttonBox);
    if (d.exec() == QDialog::Accepted) {
        int category = chooser.currentCategory();
        Fun undo = []() { return true; };
        Fun redo = []() { return true; };
        for (auto &pos : positions) {
            marker = getMarker(pos, &exists);
            if (exists) {
                if (marker.duration() > GenTime(0)) {
                    addRangeMarker(pos, marker.duration(), marker.comment(), category, undo, redo);
                } else {
                    addMarker(pos, marker.comment(), category, undo, redo);
                }
            }
        }
        PUSH_UNDO(undo, redo, i18n("Edit markers"));
        return true;
    }
    return false;
}

bool MarkerListModel::editMarkerGui(const GenTime &pos, QWidget *parent, bool createIfNotFound, ProjectClip *clip, bool createOnly)
{
    bool exists;
    auto marker = getMarker(pos, &exists);
    if (!exists && !createIfNotFound) {
        pCore->displayMessage(i18n("No marker found at current position"), InformationMessage);
    }

    if (!exists && createIfNotFound) {
        marker = CommentedTime(pos, QString(), KdenliveSettings::default_marker_type());
    }

    QScopedPointer<MarkerDialog> dialog(new MarkerDialog(clip, marker, i18n("Edit Marker"), false, parent));

    if (dialog->exec() == QDialog::Accepted) {
        marker = dialog->newMarker();
        Q_EMIT pCore->updateDefaultMarkerCategory();
        dialog->cacheThumbnail();
        if (exists && !createOnly) {
            return editMarker(pos, marker.time(), marker.comment(), marker.markerType(), marker.duration());
        }
        if (marker.duration() > GenTime(0)) {
            return addRangeMarker(marker.time(), marker.duration(), marker.comment(), marker.markerType());
        } else {
            return addMarker(marker.time(), marker.comment(), marker.markerType());
        }
    }
    return false;
}

bool MarkerListModel::addMultipleMarkersGui(const GenTime &pos, QWidget *parent, bool createIfNotFound, ProjectClip *clip)
{
    bool exists;
    auto marker = getMarker(pos, &exists);
    if (!exists && !createIfNotFound) {
        pCore->displayMessage(i18n("No marker found at current position"), InformationMessage);
    }

    if (!exists && createIfNotFound) {
        marker = CommentedTime(pos, QString(), KdenliveSettings::default_marker_type());
    }

    QScopedPointer<MarkerDialog> dialog(new MarkerDialog(clip, marker, i18n("Add Markers"), true, parent));
    if (dialog->exec() == QDialog::Accepted) {
        int max = dialog->addMultiMarker() ? dialog->getOccurrences() : 1;
        GenTime interval = dialog->getInterval();
        KdenliveSettings::setMultipleguidesinterval(interval.seconds());
        marker = dialog->newMarker();
        Q_EMIT pCore->updateDefaultMarkerCategory();
        GenTime startTime = marker.time();
        QWriteLocker locker(&m_lock);
        Fun undo = []() { return true; };
        Fun redo = []() { return true; };
        for (int i = 0; i < max; i++) {
            if (marker.duration() > GenTime(0)) {
                addRangeMarker(startTime, marker.duration(), marker.comment(), marker.markerType(), undo, redo);
            } else {
                addMarker(startTime, marker.comment(), marker.markerType(), undo, redo);
            }
            startTime += interval;
        }
        dialog->cacheThumbnail();
        PUSH_UNDO(undo, redo, i18n("Add markers"));
    }
    return false;
}

void MarkerListModel::exportGuidesGui(QWidget *parent, GenTime projectDuration) const
{
    QScopedPointer<ExportGuidesDialog> dialog(new ExportGuidesDialog(this, projectDuration, parent));
    dialog->exec();
}

bool MarkerListModel::addRangeMarker(GenTime pos, GenTime duration, const QString &comment, int type)
{
    QWriteLocker locker(&m_lock);
    Fun undo = []() { return true; };
    Fun redo = []() { return true; };
    bool res = addRangeMarker(pos, duration, comment, type, undo, redo);
    if (res) {
        if (hasMarker(pos) && marker(pos).duration().seconds() > 0) {
            PUSH_UNDO(undo, redo, i18n("Update range marker"));
        } else if (hasMarker(pos)) {
            PUSH_UNDO(undo, redo, i18n("Convert to range marker"));
        } else {
            PUSH_UNDO(undo, redo, i18n("Add range marker"));
        }
    }
    return res;
}

bool MarkerListModel::addRangeMarker(GenTime pos, GenTime duration, const QString &comment, int type, Fun &undo, Fun &redo)
{
    QWriteLocker locker(&m_lock);
    Fun local_undo = []() { return true; };
    Fun local_redo = []() { return true; };
    if (type == -1) type = KdenliveSettings::default_marker_type();
    Q_ASSERT(pCore->markerTypes.contains(type));

    if (hasMarker(pos)) {
        CommentedTime current = marker(pos);
        local_undo = addOrUpdateRangeMarker_lambda(pos, current.duration(), current.comment(), current.markerType(), &current);
        local_redo = addOrUpdateRangeMarker_lambda(pos, duration, comment, type, &current);
    } else {
        local_redo = addOrUpdateRangeMarker_lambda(pos, duration, comment, type, nullptr);
        local_undo = deleteMarker_lambda(pos);
    }

    if (local_redo()) {
        UPDATE_UNDO_REDO(local_redo, local_undo, undo, redo);
        return true;
    }
    return false;
}
