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
#include "assets/model/assetcommand.hpp"
#include "assets/model/assetparametermodel.hpp"
#include "core.h"
#include "doc/docundostack.hpp"
#include "keyframemodel.hpp"
#include "klocalizedstring.h"
#include "macros.hpp"
#include <kdenlivesettings.h>

#include <QDebug>
#include <utility>
KeyframeModelList::KeyframeModelList(std::weak_ptr<AssetParameterModel> model, const QModelIndex &index, std::weak_ptr<DocUndoStack> undo_stack)
    : m_model(std::move(model))
    , m_undoStack(std::move(undo_stack))
    , m_lock(QReadWriteLock::Recursive)
{
    qDebug() << "Construct keyframemodellist. Checking model:" << m_model.expired();
    addParameter(index);
    connect(m_parameters.begin()->second.get(), &KeyframeModel::modelChanged, this, &KeyframeModelList::modelChanged);
}

ObjectId KeyframeModelList::getOwnerId() const
{
    if (auto ptr = m_model.lock()) {
        return ptr->getOwnerId();
    }
    return {};
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

bool KeyframeModelList::addKeyframe(int frame, double val)
{
    QWriteLocker locker(&m_lock);
    GenTime pos(frame, pCore->getCurrentFps());
    Q_ASSERT(m_parameters.size() > 0);
    bool update = (m_parameters.begin()->second->hasKeyframe(pos) > 0);
    bool isRectParam = false;
    if (m_inTimelineIndex.isValid()) {
        if (auto ptr = m_model.lock()) {
            auto tp = ptr->data(m_inTimelineIndex, AssetParameterModel::TypeRole).value<ParamType>();
            if (tp == ParamType::AnimatedRect) {
                isRectParam = true;
            }
        }
    }
    auto op = [this, pos, val, isRectParam](std::shared_ptr<KeyframeModel> param, Fun &undo, Fun &redo) {
        QVariant value;
        if (m_inTimelineIndex.isValid()) {
            if (m_parameters.at(m_inTimelineIndex) == param) {
                if (isRectParam) {
                    value = param->getInterpolatedValue(pos);
                    value = param->updateInterpolated(value, val);
                } else {
                    value = param->getNormalizedValue(val);
                }
            } else {
                value = param->getInterpolatedValue(pos);
            }
        } else if (m_parameters.begin()->second == param) {
            value = param->getNormalizedValue(val);
        } else {
            value = param->getInterpolatedValue(pos);
        }
        return param->addKeyframe(pos, (KeyframeType)KdenliveSettings::defaultkeyframeinterp(), value, true, undo, redo);
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

bool KeyframeModelList::removeNextKeyframes(GenTime pos)
{
    QWriteLocker locker(&m_lock);
    Q_ASSERT(m_parameters.size() > 0);
    auto op = [pos](std::shared_ptr<KeyframeModel> param, Fun &undo, Fun &redo) { return param->removeNextKeyframes(pos, undo, redo); };
    return applyOperation(op, i18n("Delete keyframes"));
}

bool KeyframeModelList::moveKeyframe(GenTime oldPos, GenTime pos, bool logUndo)
{
    QWriteLocker locker(&m_lock);
    Q_ASSERT(m_parameters.size() > 0);
    auto op = [oldPos, pos](std::shared_ptr<KeyframeModel> param, Fun &undo, Fun &redo) { return param->moveKeyframe(oldPos, pos, QVariant(), undo, redo); };
    return applyOperation(op, logUndo ? i18n("Move keyframe") : QString());
}

bool KeyframeModelList::updateKeyframe(GenTime oldPos, GenTime pos, const QVariant &normalizedVal, bool logUndo)
{
    QWriteLocker locker(&m_lock);
    Q_ASSERT(m_parameters.size() > 0);
    bool isRectParam = false;
    if (m_inTimelineIndex.isValid()) {
        if (auto ptr = m_model.lock()) {
            auto tp = ptr->data(m_inTimelineIndex, AssetParameterModel::TypeRole).value<ParamType>();
            if (tp == ParamType::AnimatedRect) {
                isRectParam = true;
            }
        }
    }
    auto op = [this, oldPos, pos, normalizedVal, isRectParam](std::shared_ptr<KeyframeModel> param, Fun &undo, Fun &redo) {
        QVariant value;
        if (m_inTimelineIndex.isValid()) {
            if (m_parameters.at(m_inTimelineIndex) == param) {
                if (isRectParam) {
                    if (normalizedVal.isValid()) {
                        value = param->getInterpolatedValue(oldPos);
                        value = param->updateInterpolated(value, normalizedVal.toDouble());
                    }
                } else {
                    value = normalizedVal;
                }
            }
        } else if (m_parameters.begin()->second == param) {
            value = normalizedVal;
        }
        return param->moveKeyframe(oldPos, pos, value, undo, redo);
    };
    return applyOperation(op, logUndo ? i18n("Move keyframe") : QString());
}

bool KeyframeModelList::updateKeyframe(GenTime pos, const QVariant &value, const QPersistentModelIndex &index)
{
    if (singleKeyframe()) {
        bool ok = false;
        Keyframe kf = m_parameters.begin()->second->getNextKeyframe(GenTime(-1), &ok);
        pos = kf.first;
    }
    if (auto ptr = m_model.lock()) {
        auto *command = new AssetKeyframeCommand(ptr, index, value, pos);
        pCore->pushUndo(command);
    }
    return true;
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
    // Update kf type in all parameters
    bool res = true;
    for (const auto &param : m_parameters) {
        res = res && param.second->updateKeyframeType(pos, type, undo, redo);
    }
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

bool KeyframeModelList::isEmpty() const
{
    READ_LOCK();
    return (m_parameters.size() == 0 || m_parameters.begin()->second->rowCount() == 0);
}

int KeyframeModelList::count() const
{
    READ_LOCK();
    if (m_parameters.size() > 0)
        return m_parameters.begin()->second->rowCount();
    return 0;
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

void KeyframeModelList::reset()
{
    QWriteLocker locker(&m_lock);
    for (const auto &param : m_parameters) {
        param.second->reset();
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
    if (m_inTimelineIndex.isValid()) {
        return m_parameters.at(m_inTimelineIndex).get();
    }
    if (auto ptr = m_model.lock()) {
        for (const auto &param : m_parameters) {
            if (ptr->data(param.first, AssetParameterModel::ShowInTimelineRole) == true) {
                m_inTimelineIndex = param.first;
                return param.second.get();
            }
        }
    }
    return nullptr;
}

KeyframeModel *KeyframeModelList::getKeyModel(const QPersistentModelIndex &index)
{
    if (m_parameters.size() > 0 && m_parameters.find(index) != m_parameters.end()) {
        return m_parameters.at(index).get();
    }
    return nullptr;
}

void KeyframeModelList::resizeKeyframes(int oldIn, int oldOut, int in, int out, int offset, bool adjustFromEnd, Fun &undo, Fun &redo)
{
    bool ok;
    bool ok2;
    QList<GenTime> positions;
    if (!adjustFromEnd) {
        if (offset != 0) {
            // this is an endless resize clip
            GenTime old_in(oldIn, pCore->getCurrentFps());
            GenTime new_in(in + offset, pCore->getCurrentFps());
            getKeyframe(new_in, &ok2);
            positions = m_parameters.begin()->second->getKeyframePos();
            std::sort(positions.begin(), positions.end());
            for (const auto &param : m_parameters) {
                if (offset > 0) {
                    QVariant value = param.second->getInterpolatedValue(new_in);
                    param.second->updateKeyframe(old_in, value, undo, redo);
                }
                for (auto frame : qAsConst(positions)) {
                    if (new_in > GenTime()) {
                        if (frame > new_in) {
                            param.second->moveKeyframe(frame, frame - new_in, QVariant(), undo, redo);
                            continue;
                        }
                    } else if (frame > GenTime()) {
                        param.second->moveKeyframe(frame, frame - new_in, QVariant(), undo, redo);
                        continue;
                    }
                    if (frame != GenTime()) {
                        param.second->removeKeyframe(frame, undo, redo);
                    }
                }
            }
        } else if (oldIn != in) {
            GenTime old_in(oldIn, pCore->getCurrentFps());
            GenTime new_in(in, pCore->getCurrentFps());
            Keyframe kf = getKeyframe(old_in, &ok);
            KeyframeType type = kf.second;
            getKeyframe(new_in, &ok2);
            if (!ok2) {
                // Add new in point
                for (const auto &param : m_parameters) {
                    QVariant value = param.second->getInterpolatedValue(new_in);
                    param.second->addKeyframe(new_in, type, value, true, undo, redo);
                }
            }
            if (ok) {
                // Remove previous in point
                for (const auto &param : m_parameters) {
                    param.second->removeKeyframe(old_in, undo, redo);
                }
            }
            // Remove all keyframes before in
            bool nextOk = false;
            kf = m_parameters.begin()->second->getNextKeyframe(GenTime(-1), &nextOk);
            GenTime pos;
            while (nextOk) {
                pos = kf.first;
                if (pos < new_in) {
                    for (const auto &param : m_parameters) {
                        param.second->removeKeyframe(pos, undo, redo);
                    }
                    kf = m_parameters.begin()->second->getNextKeyframe(pos, &nextOk);
                } else {
                    break;
                }
            }
            // qDebug()<<"/// \n\nKEYS TO DELETE: "<<positions<<"\n------------------------";
        }
    } else {
        GenTime old_out(oldOut, pCore->getCurrentFps());
        GenTime new_out(out, pCore->getCurrentFps());
        Keyframe kf = getKeyframe(old_out, &ok);
        KeyframeType type = kf.second;
        getKeyframe(new_out, &ok2);
        // Check keyframes after last position
        bool ok3;
        Keyframe toDel = getNextKeyframe(new_out, &ok3);
        if (ok && !ok2) {
            // Check if we have only 2 keyframes (in/out), in which case we move the out keyframe to new position
            bool ok4;
            kf = getPrevKeyframe(old_out, &ok4);
            if (ok4) {
                GenTime current_in(oldIn, pCore->getCurrentFps());
                qDebug()<<" = = = = = = = \n\nGOT 2 KF SITUATION: "<<current_in.frames(25)<<" = "<<kf.first.frames(25);
                if (kf.first == current_in) {
                    // We have a 2 keyframes situation, move last one to new out
                    for (const auto &param : m_parameters) {
                        param.second->moveKeyframe(old_out, new_out, QVariant(), undo, redo);
                    }
                    return;
                }
            }
            
            positions << old_out;
        }
        if (toDel.first == GenTime()) {
            // No keyframes
            return;
        }
        while (ok3) {
            if (!positions.contains(toDel.first)) {
                positions << toDel.first;
            }
            toDel = getNextKeyframe(toDel.first, &ok3);
        }
        if ((ok || positions.size() > 0) && !ok2) {
            for (const auto &param : m_parameters) {
                QVariant value = param.second->getInterpolatedValue(new_out);
                param.second->addKeyframe(new_out, type, value, true, undo, redo);
                for (auto frame : qAsConst(positions)) {
                    param.second->removeKeyframe(frame, undo, redo);
                }
            }
        }
    }
}

void KeyframeModelList::checkConsistency()
{
    if (m_parameters.size() < 2) {
        return;
    }
    // Check keyframes in all parameters
    QList<GenTime> fullList;
    for (const auto &param : m_parameters) {
        QList<GenTime> list = param.second->getKeyframePos();
        for (auto &time : list) {
            if (!fullList.contains(time)) {
                fullList << time;
            }
        }
    }
    Fun local_update = []() { return true; };
    KeyframeType type = (KeyframeType)KdenliveSettings::defaultkeyframeinterp();
    for (const auto &param : m_parameters) {
        QList<GenTime> list = param.second->getKeyframePos();
        for (auto &time : fullList) {
            if (!list.contains(time)) {
                qDebug()<<" = = = \n\n = = = = \n\nWARNING; MISSING KF DETECTED AT: "<<time.seconds()<<"\n\n= = = \n\n= = =";
                pCore->displayMessage(i18n("Missing keyframe detected at %1, automatically re-added", time.seconds()), ErrorMessage);
                QVariant missingVal = param.second->getInterpolatedValue(time);
                local_update = param.second->addKeyframe_lambda(time, type, missingVal, false);
                local_update();
            }
        }
    }
}

GenTime KeyframeModelList::getPosAtIndex(int ix)
{
    QList<GenTime> positions = m_parameters.begin()->second->getKeyframePos();
    std::sort(positions.begin(), positions.end());
    if (ix < 0 || ix >= positions.count()) {
        return GenTime();
    }
    return positions.at(ix);
}
