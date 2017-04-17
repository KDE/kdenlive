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
#include "macros.hpp"
#include "project/projectmanager.h"

#include <QDebug>
#include <klocalizedstring.h>

MarkerListModel::MarkerListModel(const QString &clipId, std::weak_ptr<DocUndoStack> undo_stack, QObject *parent)
    : QAbstractListModel(parent)
    , m_undoStack(std::move(undo_stack))
    , m_guide(false)
    , m_clipId(clipId)
{
}

MarkerListModel::MarkerListModel(std::weak_ptr<DocUndoStack> undo_stack, QObject *parent)
    : QAbstractListModel(parent)
    , m_undoStack(std::move(undo_stack))
    , m_guide(true)
{
}

void MarkerListModel::addMarker(GenTime pos, const QString &comment)
{
    QWriteLocker locker(&m_lock);
    if (m_markerList.count(pos) > 0) {
        // In this case we simply change the comment
        QString oldComment = m_markerList[pos];
        Fun undo = changeComment_lambda(pos, oldComment);
        Fun redo = changeComment_lambda(pos, comment);
        if (redo()) {
            PUSH_UNDO(undo, redo, i18n("Rename marker"));
        }
    } else {
        // In this case we create one
        Fun redo = addMarker_lambda(pos, comment);
        Fun undo = deleteMarker_lambda(pos);
        if (redo()) {
            PUSH_UNDO(undo, redo, i18n("Add marker"));
        }
    }
}

void MarkerListModel::removeMarker(GenTime pos)
{
    QWriteLocker locker(&m_lock);
    Q_ASSERT(m_markerList.count(pos) > 0);
    QString oldComment = m_markerList[pos];
    Fun undo = addMarker_lambda(pos, oldComment);
    Fun redo = deleteMarker_lambda(pos);
    if (redo()) {
        PUSH_UNDO(undo, redo, i18n("Delete marker"));
    }
}

Fun MarkerListModel::changeComment_lambda(GenTime pos, const QString &comment)
{
    auto guide = m_guide;
    auto clipId = m_clipId;
    return [guide, clipId, pos, comment]() {
        auto model = getModel(guide, clipId);
        Q_ASSERT(model->m_markerList.count(pos) > 0);
        int row = static_cast<int>(std::distance(model->m_markerList.begin(), model->m_markerList.find(pos)));
        emit model->dataChanged(model->index(row), model->index(row));
        model->m_markerList[pos] = comment;
        return true;
    };
}

Fun MarkerListModel::addMarker_lambda(GenTime pos, const QString &comment)
{
    auto guide = m_guide;
    auto clipId = m_clipId;
    return [guide, clipId, pos, comment]() {
        auto model = getModel(guide, clipId);
        Q_ASSERT(model->m_markerList.count(pos) == 0);
        // We determine the row of the newly added marker
        auto insertionIt = model->m_markerList.lower_bound(pos);
        int insertionRow = static_cast<int>(model->m_markerList.size());
        if (insertionIt != model->m_markerList.end()) {
            insertionRow = static_cast<int>(std::distance(model->m_markerList.begin(), insertionIt));
        }
        model->beginInsertRows(QModelIndex(), insertionRow, insertionRow);
        model->m_markerList[pos] = comment;
        model->endInsertRows();
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

QVariant MarkerListModel::data(const QModelIndex &index, int role) const
{
    if (index.row() < 0 || index.row() >= static_cast<int>(m_markerList.size()) || !index.isValid()) {
        return QVariant();
    }
    auto it = m_markerList.begin();
    std::advance(it, index.row());
    switch (role) {
    case Qt::DisplayRole:
    case Qt::EditRole:
    case CommentRole:
        return it->second;
    case PosRole:
        return it->first.seconds();
    }
    return QVariant();
}

int MarkerListModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) return 0;
    return static_cast<int>(m_markerList.size());
}
