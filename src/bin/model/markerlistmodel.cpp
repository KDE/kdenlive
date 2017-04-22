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
#include "doc/docundostack.hpp"
#include "doc/kdenlivedoc.h"
#include "kdenlivesettings.h"
#include "macros.hpp"
#include "project/projectmanager.h"
#include "timeline2/model/snapmodel.hpp"

#include <QDebug>
#include <klocalizedstring.h>

std::array<QColor, 5> MarkerListModel::markerTypes = {{Qt::red, Qt::blue, Qt::green, Qt::yellow, Qt::cyan}};

MarkerListModel::MarkerListModel(const QString &clipId, std::weak_ptr<DocUndoStack> undo_stack, QObject *parent)
    : QAbstractListModel(parent)
    , m_undoStack(std::move(undo_stack))
    , m_guide(false)
    , m_clipId(clipId)
    , m_lock(QReadWriteLock::Recursive)
{
}

MarkerListModel::MarkerListModel(std::weak_ptr<DocUndoStack> undo_stack, QObject *parent)
    : QAbstractListModel(parent)
    , m_undoStack(std::move(undo_stack))
    , m_guide(true)
    , m_lock(QReadWriteLock::Recursive)
{
}

void MarkerListModel::addMarker(GenTime pos, const QString &comment, int type)
{
    QWriteLocker locker(&m_lock);
    if (type == -1) type = KdenliveSettings::default_marker_type();
    Q_ASSERT(type >= 0 && type < (int)markerTypes.size());
    if (m_markerList.count(pos) > 0) {
        // In this case we simply change the comment and type
        QString oldComment = m_markerList[pos].first;
        int oldType = m_markerList[pos].second;
        Fun undo = changeComment_lambda(pos, oldComment, oldType);
        Fun redo = changeComment_lambda(pos, comment, type);
        if (redo()) {
            PUSH_UNDO(undo, redo, m_guide ? i18n("Rename guide") : i18n("Rename marker"));
        }
    } else {
        // In this case we create one
        Fun redo = addMarker_lambda(pos, comment, type);
        Fun undo = deleteMarker_lambda(pos);
        if (redo()) {
            PUSH_UNDO(undo, redo, m_guide ? i18n("Add guide") : i18n("Add marker"));
        }
    }
}

void MarkerListModel::removeMarker(GenTime pos)
{
    QWriteLocker locker(&m_lock);
    Q_ASSERT(m_markerList.count(pos) > 0);
    QString oldComment = m_markerList[pos].first;
    int oldType = m_markerList[pos].second;
    Fun undo = addMarker_lambda(pos, oldComment, oldType);
    Fun redo = deleteMarker_lambda(pos);
    if (redo()) {
        PUSH_UNDO(undo, redo, m_guide ? i18n("Delete guide") : i18n("Delete marker"));
    }
}

Fun MarkerListModel::changeComment_lambda(GenTime pos, const QString &comment, int type)
{
    auto guide = m_guide;
    auto clipId = m_clipId;
    return [guide, clipId, pos, comment, type]() {
        auto model = getModel(guide, clipId);
        Q_ASSERT(model->m_markerList.count(pos) > 0);
        int row = static_cast<int>(std::distance(model->m_markerList.begin(), model->m_markerList.find(pos)));
        model->m_markerList[pos].first = comment;
        model->m_markerList[pos].second = type;
        emit model->dataChanged(model->index(row), model->index(row), QVector<int>() << CommentRole << ColorRole);
        return true;
    };
}

Fun MarkerListModel::addMarker_lambda(GenTime pos, const QString &comment, int type)
{
    auto guide = m_guide;
    auto clipId = m_clipId;
    return [guide, clipId, pos, comment, type]() {
        auto model = getModel(guide, clipId);
        Q_ASSERT(model->m_markerList.count(pos) == 0);
        // We determine the row of the newly added marker
        auto insertionIt = model->m_markerList.lower_bound(pos);
        int insertionRow = static_cast<int>(model->m_markerList.size());
        if (insertionIt != model->m_markerList.end()) {
            insertionRow = static_cast<int>(std::distance(model->m_markerList.begin(), insertionIt));
        }
        model->beginInsertRows(QModelIndex(), insertionRow, insertionRow);
        model->m_markerList[pos] = {comment, type};
        model->endInsertRows();
        model->addSnapPoint(pos);
        return true;
    };
}

Fun MarkerListModel::deleteMarker_lambda(GenTime pos)
{
    auto guide = m_guide;
    auto clipId = m_clipId;
    return [guide, clipId, pos]() {
        auto model = getModel(guide, clipId);
        Q_ASSERT(model->m_markerList.count(pos) > 0);
        int row = static_cast<int>(std::distance(model->m_markerList.begin(), model->m_markerList.find(pos)));
        model->beginRemoveRows(QModelIndex(), row, row);
        model->m_markerList.erase(pos);
        model->endRemoveRows();
        model->removeSnapPoint(pos);
        return true;
    };
}

std::shared_ptr<MarkerListModel> MarkerListModel::getModel(bool guide, const QString &clipId)
{
    if (guide) {
        return pCore->projectManager()->current()->getGuideModel();
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
    return roles;
}

void MarkerListModel::addSnapPoint(GenTime pos)
{
    std::vector<std::weak_ptr<SnapModel>> validSnapModels;
    for (const auto& snapModel : m_registeredSnaps) {
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
    std::vector<std::weak_ptr<SnapModel>> validSnapModels;
    for (const auto& snapModel : m_registeredSnaps) {
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
        return it->second.first;
    case PosRole:
        return it->first.seconds();
    case FrameRole:
        return it->first.frames(pCore->getCurrentFps());
    case ColorRole:
        return markerTypes[(size_t)it->second.second];
    }
    return QVariant();
}

int MarkerListModel::rowCount(const QModelIndex &parent) const
{
    READ_LOCK();
    if (parent.isValid()) return 0;
    return static_cast<int>(m_markerList.size());
}

CommentedTime MarkerListModel::getMarker(const GenTime &pos) const
{
    READ_LOCK();
    if (m_markerList.count(pos) <= 0) {
        // return empty marker
        return CommentedTime();
    }
    CommentedTime t(pos, m_markerList.at(pos).first, m_markerList.at(pos).second);
    return t;
}

bool MarkerListModel::hasMarker(int frame) const
{
    READ_LOCK();
    return m_markerList.count(GenTime(frame, pCore->getCurrentFps())) > 0;
}



void MarkerListModel::registerSnapModel(std::weak_ptr<SnapModel> snapModel)
{
    READ_LOCK();
    Q_ASSERT(m_guide);
    // make sure ptr is valid
    if (auto ptr = snapModel.lock()) {
        // ptr is valid, we store it
        m_registeredSnaps.push_back(snapModel);

        // we now add the already existing markers to the snap
        for (const auto &marker : m_markerList) {
            GenTime pos = marker.first;
            ptr->addPoint(pos.frames(pCore->getCurrentFps()));
        }
    } else {
        qDebug() << "Error: added snapmodel is null";
        Q_ASSERT(false);
    }
}
