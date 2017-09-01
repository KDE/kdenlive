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

#include "keyframemodel.hpp"
#include "doc/docundostack.hpp"
#include "core.h"
#include "effects/effectstack/model/effectitemmodel.hpp"
#include "macros.hpp"

#include <QDebug>


KeyframeModel::KeyframeModel(double init_value, std::weak_ptr<EffectItemModel> effect, std::weak_ptr<DocUndoStack> undo_stack, QObject *parent)
    : QAbstractListModel(parent)
    , m_effect(std::move(effect))
    , m_undoStack(std::move(undo_stack))
    , m_lock(QReadWriteLock::Recursive)
{
    m_keyframeList.insert({GenTime(), {KeyframeType::Linear, init_value}});
    setup();
}


void KeyframeModel::setup()
{
    // We connect the signals of the abstractitemmodel to a more generic one.
    connect(this, &KeyframeModel::columnsMoved, this, &KeyframeModel::modelChanged);
    connect(this, &KeyframeModel::columnsRemoved, this, &KeyframeModel::modelChanged);
    connect(this, &KeyframeModel::columnsInserted, this, &KeyframeModel::modelChanged);
    connect(this, &KeyframeModel::rowsMoved, this, &KeyframeModel::modelChanged);
    connect(this, &KeyframeModel::rowsRemoved, this, &KeyframeModel::modelChanged);
    connect(this, &KeyframeModel::rowsInserted, this, &KeyframeModel::modelChanged);
    connect(this, &KeyframeModel::modelReset, this, &KeyframeModel::modelChanged);
    connect(this, &KeyframeModel::dataChanged, this, &KeyframeModel::modelChanged);
}

bool KeyframeModel::addKeyframe(GenTime pos, KeyframeType type, double value, Fun &undo, Fun &redo)
{
    QWriteLocker locker(&m_lock);
    Fun local_undo = []() { return true; };
    Fun local_redo = []() { return true; };
    if (m_keyframeList.count(pos) > 0) {
        if (std::pair<KeyframeType, double>({type, value}) == m_keyframeList.at(pos)) {
            return true; // nothing to do
        }
        // In this case we simply change the type and value
        KeyframeType oldType = m_keyframeList[pos].first;
        double oldValue = m_keyframeList[pos].second;
        local_undo = updateKeyframe_lambda(pos, oldType, oldValue);
        local_redo = updateKeyframe_lambda(pos, type, value);
    } else {
        local_redo = addKeyframe_lambda(pos, type, value);
        local_undo = deleteKeyframe_lambda(pos);
    }
    if (local_redo()) {
        UPDATE_UNDO_REDO(local_redo, local_undo, undo, redo);
        return true;
    }
    return false;
}

bool KeyframeModel::addKeyframe(GenTime pos, KeyframeType type, double value)
{
    QWriteLocker locker(&m_lock);
    Fun undo = []() { return true; };
    Fun redo = []() { return true; };

    bool update = (m_keyframeList.count(pos) > 0);
    bool res = addKeyframe(pos, type, value, undo, redo);
    if (res) {
        PUSH_UNDO(undo, redo, update ? i18n("Change keyframe type") : i18n("Add keyframe"));
    }
    return res;
}

bool KeyframeModel::removeKeyframe(GenTime pos, Fun &undo, Fun &redo)
{
    QWriteLocker locker(&m_lock);
    Q_ASSERT(m_keyframeList.count(pos) > 0);
    KeyframeType oldType = m_keyframeList[pos].first;
    double oldValue = m_keyframeList[pos].second;
    Fun local_undo = addKeyframe_lambda(pos, oldType, oldValue);
    Fun local_redo = deleteKeyframe_lambda(pos);
    if (local_redo()) {
        UPDATE_UNDO_REDO(local_redo, local_undo, undo, redo);
        return true;
    }
    return false;
}

bool KeyframeModel::removeKeyframe(GenTime pos)
{
    QWriteLocker locker(&m_lock);
    Fun undo = []() { return true; };
    Fun redo = []() { return true; };

    if (pos == GenTime()) {
        return false;  // initial point must stay
    }

    bool res = removeKeyframe(pos, undo, redo);
    if (res) {
        PUSH_UNDO(undo, redo, i18n("Delete keyframe"));
    }
    return res;
}

bool KeyframeModel::moveKeyframe(GenTime oldPos, GenTime pos)
{
    QWriteLocker locker(&m_lock);
    Q_ASSERT(m_keyframeList.count(oldPos) > 0);
    KeyframeType oldType = m_keyframeList[oldPos].first;
    double oldValue = m_keyframeList[pos].second;
    if (oldPos == pos ) return true;
    Fun undo = []() { return true; };
    Fun redo = []() { return true; };
    bool res = removeKeyframe(oldPos, undo, redo);
    if (res) {
        res = addKeyframe(pos, oldType, oldValue, undo, redo);
    }
    if (res) {
        PUSH_UNDO(undo, redo, i18n("Move keyframe"));
    } else {
        bool undone = undo();
        Q_ASSERT(undone);
    }
    return res;
}

bool KeyframeModel::updateKeyframe(GenTime pos, double value)
{
    QWriteLocker locker(&m_lock);
    Q_ASSERT(m_keyframeList.count(pos) > 0);
    KeyframeType oldType = m_keyframeList[pos].first;
    double oldValue = m_keyframeList[pos].second;
    if (qAbs(oldValue - value) < 1e-6) return true;
    Fun undo = updateKeyframe_lambda(pos, oldType, oldValue);
    Fun redo = updateKeyframe_lambda(pos, oldType, value);
    bool res = redo();
    if (res) {
        PUSH_UNDO(undo, redo, i18n("Update keyframe"));
    }
    return res;
}

Fun KeyframeModel::updateKeyframe_lambda(GenTime pos, KeyframeType type, double value)
{
    QWriteLocker locker(&m_lock);
    return [this, pos, type, value]() {
        Q_ASSERT(m_keyframeList.count(pos) > 0);
        int row = static_cast<int>(std::distance(m_keyframeList.begin(), m_keyframeList.find(pos)));
        m_keyframeList[pos].first = type;
        m_keyframeList[pos].second = value;
        emit dataChanged(index(row), index(row), QVector<int>() << TypeRole << ValueRole);
        return true;
    };
}

Fun KeyframeModel::addKeyframe_lambda(GenTime pos, KeyframeType type, double value)
{
    QWriteLocker locker(&m_lock);
    return [this, pos, type, value]() {
        Q_ASSERT(m_keyframeList.count(pos) == 0);
        // We determine the row of the newly added marker
        auto insertionIt = m_keyframeList.lower_bound(pos);
        int insertionRow = static_cast<int>(m_keyframeList.size());
        if (insertionIt != m_keyframeList.end()) {
            insertionRow = static_cast<int>(std::distance(m_keyframeList.begin(), insertionIt));
        }
        beginInsertRows(QModelIndex(), insertionRow, insertionRow);
        m_keyframeList[pos].first = type;
        m_keyframeList[pos].second = value;
        endInsertRows();
        return true;
    };
}

Fun KeyframeModel::deleteKeyframe_lambda(GenTime pos)
{
    QWriteLocker locker(&m_lock);
    return [this, pos]() {
        Q_ASSERT(m_keyframeList.count(pos) > 0);
        Q_ASSERT(pos != GenTime()); // cannot delete initial point
        int row = static_cast<int>(std::distance(m_keyframeList.begin(), m_keyframeList.find(pos)));
        beginRemoveRows(QModelIndex(), row, row);
        m_keyframeList.erase(pos);
        endRemoveRows();
        return true;
    };
}

QHash<int, QByteArray> KeyframeModel::roleNames() const
{
    QHash<int, QByteArray> roles;
    roles[PosRole] = "position";
    roles[FrameRole] = "frame";
    roles[TypeRole] = "type";
    roles[ValueRole] = "value";
    return roles;
}


QVariant KeyframeModel::data(const QModelIndex &index, int role) const
{
    READ_LOCK();
    if (index.row() < 0 || index.row() >= static_cast<int>(m_keyframeList.size()) || !index.isValid()) {
        return QVariant();
    }
    auto it = m_keyframeList.begin();
    std::advance(it, index.row());
    switch (role) {
    case Qt::DisplayRole:
    case Qt::EditRole:
    case ValueRole:
        return it->second.second;
    case PosRole:
        return it->first.seconds();
    case FrameRole:
    case Qt::UserRole:
        return it->first.frames(pCore->getCurrentFps());
    case TypeRole:
        return QVariant::fromValue<KeyframeType>(it->second.first);
    }
    return QVariant();
}

int KeyframeModel::rowCount(const QModelIndex &parent) const
{
    READ_LOCK();
    if (parent.isValid()) return 0;
    return static_cast<int>(m_keyframeList.size());
}

Keyframe KeyframeModel::getKeyframe(const GenTime &pos, bool *ok) const
{
    READ_LOCK();
    if (m_keyframeList.count(pos) <= 0) {
        // return empty marker
        *ok = false;
        return {GenTime(), {KeyframeType::Linear, 0}};
    }
    *ok = true;
    return {pos, m_keyframeList.at(pos)};
}

bool KeyframeModel::hasKeyframe(int frame) const
{
    READ_LOCK();
    return m_keyframeList.count(GenTime(frame, pCore->getCurrentFps())) > 0;
}

bool KeyframeModel::removeAllKeyframes()
{
    QWriteLocker locker(&m_lock);
    std::vector<GenTime> all_pos;
    Fun local_undo = []() { return true; };
    Fun local_redo = []() { return true; };
    for (const auto& m : m_keyframeList) {
        all_pos.push_back(m.first);
    }
    bool res = true;
    for (const auto& p : all_pos) {
        res = removeKeyframe(p, local_undo, local_redo);
        if (!res) {
            bool undone = local_undo();
            Q_ASSERT(undone);
            return false;
        }
    }
    PUSH_UNDO(local_undo, local_redo, i18n("Delete all keyframes"));
    return true;
}

QString KeyframeModel::getAnimProperty() const
{
    QString prop;
    bool first = true;
    for (const auto keyframe : m_keyframeList) {
        if (first) {
            first = false;
        } else {
            prop += QStringLiteral(";");
        }
        prop += QString::number(keyframe.first.frames(pCore->getCurrentFps()));
        switch (keyframe.second.first) {
        case KeyframeType::Linear:
            prop += QStringLiteral("=");
            break;
        case KeyframeType::Discrete:
            prop += QStringLiteral("|=");
            break;
        case KeyframeType::Curve:
            prop += QStringLiteral("~=");
            break;
        }
        prop += QString::number(keyframe.second.second);
    }
    return prop;
}
