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

#include "keyframemodellist.hpp"
#include "assets/model/assetparametermodel.hpp"
#include "core.h"
#include "doc/docundostack.hpp"
#include "keyframemodel.hpp"
#include "klocalizedstring.h"
#include "macros.hpp"

#include <QDebug>

KeyframeModelList::KeyframeModelList(std::weak_ptr<AssetParameterModel> model, const QModelIndex &index, std::weak_ptr<DocUndoStack> undo_stack)
    : m_model(model)
    , m_undoStack(undo_stack)
    , m_lock(QReadWriteLock::Recursive)
{
    qDebug() << "Construct keyframemodellist. Checking model:" << m_model.expired();
    addParameter(index);
    connect(m_parameters.begin()->second.get(), &KeyframeModel::modelChanged, this, &KeyframeModelList::modelChanged);
}

void KeyframeModelList::addParameter(const QModelIndex &index)
{
    std::shared_ptr<KeyframeModel> parameter(new KeyframeModel(m_model, index, m_undoStack));
    m_parameters.insert({index, std::move(parameter)});
}

bool KeyframeModelList::applyOperation(const std::function<bool(std::shared_ptr<KeyframeModel>, Fun &, Fun &)> &op, const QString &undoString)
{
    QWriteLocker locker(&m_lock);
    Q_ASSERT(m_parameters.size() > 0);
    Fun undo = []() { return true; };
    Fun redo = []() { return true; };

    bool res = true;
    for (const auto &param : m_parameters) {
        res = op(param.second, undo, redo);
        if (!res) {
            bool undone = undo();
            Q_ASSERT(undone);
            return res;
        }
    }
    if (res && !undoString.isEmpty()) {
        PUSH_UNDO(undo, redo, undoString);
    }
    return res;
}

bool KeyframeModelList::addKeyframe(GenTime pos, KeyframeType type)
{
    QWriteLocker locker(&m_lock);
    Q_ASSERT(m_parameters.size() > 0);
    bool update = (m_parameters.begin()->second->hasKeyframe(pos) > 0);
    auto op = [pos, type](std::shared_ptr<KeyframeModel> param, Fun &undo, Fun &redo) {
        QVariant value = param->getInterpolatedValue(pos);
        return param->addKeyframe(pos, type, value, true, undo, redo);
    };
    return applyOperation(op, update ? i18n("Change keyframe type") : i18n("Add keyframe"));
}

bool KeyframeModelList::removeKeyframe(GenTime pos)
{
    QWriteLocker locker(&m_lock);
    Q_ASSERT(m_parameters.size() > 0);
    auto op = [pos](std::shared_ptr<KeyframeModel> param, Fun &undo, Fun &redo) { return param->removeKeyframe(pos, undo, redo); };
    return applyOperation(op, i18n("Delete keyframe"));
}

bool KeyframeModelList::removeAllKeyframes()
{
    QWriteLocker locker(&m_lock);
    Q_ASSERT(m_parameters.size() > 0);
    auto op = [](std::shared_ptr<KeyframeModel> param, Fun &undo, Fun &redo) { return param->removeAllKeyframes(undo, redo); };
    return applyOperation(op, i18n("Delete all keyframes"));
}

bool KeyframeModelList::moveKeyframe(GenTime oldPos, GenTime pos, bool logUndo)
{
    QWriteLocker locker(&m_lock);
    Q_ASSERT(m_parameters.size() > 0);
    auto op = [oldPos, pos](std::shared_ptr<KeyframeModel> param, Fun &undo, Fun &redo) { return param->moveKeyframe(oldPos, pos, -1, undo, redo); };
    return applyOperation(op, logUndo ? i18n("Move keyframe") : QString());
}

bool KeyframeModelList::updateKeyframe(GenTime pos, QVariant value, const QPersistentModelIndex &index)
{
    QWriteLocker locker(&m_lock);
    Q_ASSERT(m_parameters.count(index) > 0);
    Fun undo = []() { return true; };
    Fun redo = []() { return true; };
    if (singleKeyframe()) {
        bool ok = false;
        Keyframe kf = m_parameters.begin()->second->getNextKeyframe(GenTime(-1), &ok);
        pos = kf.first;
    }
    bool res = m_parameters.at(index)->updateKeyframe(pos, value, undo, redo);
    if (res) {
        PUSH_UNDO(undo, redo, i18n("Update keyframe"));
    }
    return res;
}

bool KeyframeModelList::updateKeyframeType(GenTime pos, int type, const QPersistentModelIndex &index)
{
    QWriteLocker locker(&m_lock);
    Q_ASSERT(m_parameters.count(index) > 0);
    Fun undo = []() { return true; };
    Fun redo = []() { return true; };
    if (singleKeyframe()) {
        bool ok = false;
        Keyframe kf = m_parameters.begin()->second->getNextKeyframe(GenTime(-1), &ok);
        pos = kf.first;
    }
    bool res = m_parameters.at(index)->updateKeyframeType(pos, type, undo, redo);
    if (res) {
        PUSH_UNDO(undo, redo, i18n("Update keyframe"));
    }
    return res;
}

KeyframeType KeyframeModelList::keyframeType(GenTime pos) const
{
    QWriteLocker locker(&m_lock);
    if (singleKeyframe()) {
        bool ok = false;
        Keyframe kf = m_parameters.begin()->second->getNextKeyframe(GenTime(-1), &ok);
        return kf.second;
    }
    bool ok = false;
    Keyframe kf = m_parameters.begin()->second->getKeyframe(pos, &ok);
    return kf.second;
}

Keyframe KeyframeModelList::getKeyframe(const GenTime &pos, bool *ok) const
{
    READ_LOCK();
    Q_ASSERT(m_parameters.size() > 0);
    return m_parameters.begin()->second->getKeyframe(pos, ok);
}

bool KeyframeModelList::singleKeyframe() const
{
    READ_LOCK();
    Q_ASSERT(m_parameters.size() > 0);
    return m_parameters.begin()->second->singleKeyframe();
}

Keyframe KeyframeModelList::getNextKeyframe(const GenTime &pos, bool *ok) const
{
    READ_LOCK();
    Q_ASSERT(m_parameters.size() > 0);
    return m_parameters.begin()->second->getNextKeyframe(pos, ok);
}

Keyframe KeyframeModelList::getPrevKeyframe(const GenTime &pos, bool *ok) const
{
    READ_LOCK();
    Q_ASSERT(m_parameters.size() > 0);
    return m_parameters.begin()->second->getPrevKeyframe(pos, ok);
}

Keyframe KeyframeModelList::getClosestKeyframe(const GenTime &pos, bool *ok) const
{
    READ_LOCK();
    Q_ASSERT(m_parameters.size() > 0);
    return m_parameters.begin()->second->getClosestKeyframe(pos, ok);
}

bool KeyframeModelList::hasKeyframe(int frame) const
{
    READ_LOCK();
    Q_ASSERT(m_parameters.size() > 0);
    return m_parameters.begin()->second->hasKeyframe(frame);
}

void KeyframeModelList::refresh()
{
    QWriteLocker locker(&m_lock);
    for (const auto &param : m_parameters) {
        param.second->refresh();
    }
}

QVariant KeyframeModelList::getInterpolatedValue(int pos, const QPersistentModelIndex &index) const
{
    READ_LOCK();
    Q_ASSERT(m_parameters.count(index) > 0);
    return m_parameters.at(index)->getInterpolatedValue(pos);
}

KeyframeModel *KeyframeModelList::getKeyModel()
{
    if (m_parameters.size() > 0) {
        return m_parameters.begin()->second.get();
    }
    return nullptr;
}
