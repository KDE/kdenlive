/***************************************************************************
 *   Copyright (C) 2018 by Nicolas Carion                                  *
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

#include "modelupdater.hpp"
#include "timelinemodel.hpp"
#include "trackmodel.hpp"
#include <unordered_map>

namespace {
// This function assumes all the updates in the list correspond to the same item
// It will merge move update together. We also merge insert + move in an insert to the right position
std::vector<std::shared_ptr<AbstractUpdate>> mergeMoves(const std::vector<std::shared_ptr<AbstractUpdate>> &list)
{
    if (list.size() == 0) {
        return {};
    }
    std::vector<std::shared_ptr<AbstractUpdate>> result;
    int itemId = list[0]->getItemId();

    int sourceTrackId = -1;
    int sourcePos = -1;
    std::weak_ptr<TimelineModel> sourceTimeline;
    int targetTrackId = -1;
    int targetPos = -1;
    std::weak_ptr<TimelineModel> targetTimeline;

    bool firstMoveFound = false;
    bool isOperationInsert = false;
    bool isClip = true;

    for (const auto &update : list) {
        Q_ASSERT(update->getItemId() == itemId);
        if (!update->isMove()) {
            if (update->isInsert()) {
                if (firstMoveFound) {
                    // impossible to have an insert on something already inserted...
                    Q_ASSERT(false);
                } else {
                    isOperationInsert = true;
                    auto ins = std::static_pointer_cast<InsertUpdate>(update);
                    targetPos = ins->getPos();
                    targetTrackId = ins->getTrackId();
                    targetTimeline = ins->getTimeline();
                    isClip = ins->isClip();
                }
            } else if (update->isDelete() && firstMoveFound) {
                // move then delete should have been merged into delete at this point.
                Q_ASSERT(false);
            } else {
                result.push_back(update);
            }
        } else {
            firstMoveFound = true;
            auto move = std::static_pointer_cast<MoveUpdate>(update);
            if (sourcePos == -1) {
                sourcePos = move->getSourcePos();
                sourceTrackId = move->getSourceTrackId();
                sourceTimeline = move->getSourceTimeline();
                targetPos = move->getTargetPos();
                targetTrackId = move->getTargetTrackId();
                targetTimeline = move->getTargetTimeline();
            } else {
                targetPos = move->getTargetPos();
                targetTrackId = move->getTargetTrackId();
                targetTimeline = move->getTargetTimeline();
            }
        }
    }

    if (isOperationInsert) {
        result.emplace_back(new InsertUpdate(itemId, targetTimeline, targetTrackId, targetPos, isClip));
    } else if (sourcePos != -1) {
        // we do have a move, merge
        if (sourceTimeline.lock() == targetTimeline.lock() && sourceTrackId == targetTrackId) {
            // same track move, replace with change
            result.emplace_back(new ChangeUpdate(itemId, sourceTimeline, {TimelineModel::StartRole}));
        } else {
            result.emplace_back(new MoveUpdate(itemId, sourceTimeline, sourceTrackId, sourcePos, targetTimeline, targetTrackId, targetPos));
        }
    }
    return result;
}

// This function assumes all the updates in the list correspond to the same item, and that at this point we don't have any moves
// It will merge pairs of delete + insert into a move
// It also cleans the following situations:
//  - Last operation is delete: all the previous ones can be safely discarded
//  - First operation is insert, last one is delete: that is equivalent to doing nothing
std::vector<std::shared_ptr<AbstractUpdate>> mergeDeleteInsert(const std::vector<std::shared_ptr<AbstractUpdate>> &list)
{
    if (list.size() == 0) {
        return {};
    }
    std::vector<std::shared_ptr<AbstractUpdate>> result;
    int itemId = list[0]->getItemId();

    int sourceTrackId = -1;
    int sourcePos = -1;
    bool isClip = true;
    std::weak_ptr<TimelineModel> sourceTimeline;

    for (const auto &update : list) {
        Q_ASSERT(update->getItemId() == itemId);
        Q_ASSERT(!update->isMove());
        if (update->isDelete() && sourceTrackId == -1) {
            auto del = std::static_pointer_cast<DeleteUpdate>(update);
            sourceTrackId = del->getTrackId();
            sourcePos = del->getPos();
            sourceTimeline = del->getTimeline();
            isClip = del->isClip();
            Q_ASSERT(sourceTrackId != -1);
        } else if (sourceTrackId != -1 && update->isInsert()) {
            auto insert = std::static_pointer_cast<InsertUpdate>(update);
            result.emplace_back(
                new MoveUpdate(itemId, sourceTimeline, sourceTrackId, sourcePos, insert->getTimeline(), insert->getTrackId(), insert->getPos()));
            sourceTrackId = -1;
            sourcePos = -1;
            isClip = insert->isClip();
        } else if (sourceTrackId != -1 && update->isDelete()) {
            // we found a double delete, problematic...
            Q_ASSERT(false);
        } else {
            result.push_back(update);
        }
    }

    if (sourcePos != -1) {
        // If we reach this, it means that the last operation was a delete, but we didn't find any subsequent insert.
        // That means we can remove any update before the delete, since they are not doing anything

        // If the first operation is an insert, it means that overall this sequence inserts then deletes, which is equivalent to doing nothing.
        std::vector<std::shared_ptr<AbstractUpdate>> result_clean;

        bool isFirstOperationInsert = false;
        for (const auto &update : result) {
            if (update->isDelete() || update->isInsert() || update->isMove()) {
                isFirstOperationInsert = update->isInsert();
                break;
            }
        }
        if (!isFirstOperationInsert) {
            result_clean.emplace_back(new DeleteUpdate(itemId, sourceTimeline, sourceTrackId, sourcePos, isClip));
        }
    }
    return result;
}

// This function assumes all the updates in the list correspond to the same item
// It will merge change updates together. If changes are constructed with different timeline pointers, this is a bit problematic, it means that the item was
// moved to a different timeline along the way. We track the insert/moves to check what is the last timeline it was seen in. This function must be called after
// move/insert/delete filters. That means that there is at most one move/insert/delete operation is the update list
std::vector<std::shared_ptr<AbstractUpdate>> mergeChanges(const std::vector<std::shared_ptr<AbstractUpdate>> &list)
{
    qDebug() << "merging changes";
    if (list.size() == 0) {
        return {};
    }
    std::vector<std::shared_ptr<AbstractUpdate>> result;
    int itemId = list[0]->getItemId();
    std::unordered_set<int> roles;
    bool seenMove = false;
    std::weak_ptr<TimelineModel> timeline;

    for (const auto &update : list) {
        Q_ASSERT(update->getItemId() == itemId);
        update->print();
        if (update->isDelete()) {
            // in case TimelineModel::of a delete, we don't care about changes.
            std::vector<std::shared_ptr<AbstractUpdate>> res;
            std::copy_if(list.begin(), list.end(), std::back_inserter(res), [](const std::shared_ptr<AbstractUpdate> &u) { return !u->isChange(); });
            qDebug() << "found delete, aborting";
            return res;
        } else if (update->isInsert()) {
            Q_ASSERT(!seenMove);
            auto insert = std::static_pointer_cast<InsertUpdate>(update);
            timeline = insert->getTimeline();
            seenMove = true;
            result.push_back(update);
        } else if (update->isMove()) {
            Q_ASSERT(!seenMove);
            auto move = std::static_pointer_cast<MoveUpdate>(update);
            timeline = move->getTargetTimeline();
            seenMove = true;
            result.push_back(update);
        } else if (update->isChange()) {
            auto change = std::static_pointer_cast<ChangeUpdate>(update);
            auto curRoles = change->getRoles();
            roles.insert(curRoles.begin(), curRoles.end());
            if (!seenMove) {
                timeline = change->getTimeline();
            }
        } else {
            // not implemented?
            Q_ASSERT(false);
        }
    }
    if (roles.size() > 0) {
        QVector<int> rolesVec;
        rolesVec.reserve((int)roles.size());
        for (int role : roles) {
            rolesVec.push_back(role);
        }
        result.push_back(std::make_shared<ChangeUpdate>(itemId, timeline, rolesVec));
    }
    return result;
}
// This function cleans a list of updates to be applied.
// After this function is applied, each item should have at most one insert/delete or move operation
std::vector<std::shared_ptr<AbstractUpdate>> simplify(const std::vector<std::shared_ptr<AbstractUpdate>> &list)
{
    qDebug() << "starting to simplify updates";

    // First, we simply regroup updates by item
    std::unordered_map<int, std::vector<std::shared_ptr<AbstractUpdate>>> updatesByItem;

    for (const auto &u : list) {
        updatesByItem[u->getItemId()].push_back(u);
    }

    // Then, we simplify updates element by element

    std::vector<std::shared_ptr<AbstractUpdate>> res;
    for (const auto &u : updatesByItem) {
        auto curated = mergeChanges(mergeMoves(mergeDeleteInsert(u.second)));
        res.insert(res.end(), curated.begin(), curated.end());
    }

    // Finally, we sort, in order to move the ChangeUpdates to the end
    std::sort(res.begin(), res.end(), [](const auto &a, const auto &b) {
        if (a->isChange() == b->isChange()) {
            return false;
        }
        return !a->isChange();
    });
    return res;
}

// This function takes a simplified list of updates and compute the reverse operations
std::vector<std::shared_ptr<AbstractUpdate>> reverse(const std::vector<std::shared_ptr<AbstractUpdate>> &list)
{
#ifdef QT_DEBUG
    // in debug mode, we check whether the actions are indeed simplified
    std::unordered_set<int> idSeenChange, idSeenMove;
#endif

    std::vector<std::shared_ptr<AbstractUpdate>> res;
    for (int i = (int)list.size() - 1; i >= 0; --i) {
        std::shared_ptr<AbstractUpdate> update = list[size_t(i)];
        res.emplace_back(update->reverse());
#ifdef QT_DEBUG
        update->print();

        if (update->isChange()) {
            if (idSeenChange.count(update->getItemId()) > 0) {
                Q_ASSERT(false);
            }
            idSeenChange.insert(update->getItemId());
        } else {
            if (idSeenMove.count(update->getItemId()) > 0) {
                Q_ASSERT(false);
            }
            idSeenMove.insert(update->getItemId());
        }
#endif
    }
    return res;
}
} // namespace

// This creates a lambda to be executed before the operation to prepare the model for update
Fun ModelUpdater::preApply_lambda(const std::vector<std::shared_ptr<AbstractUpdate>> &list)
{
    auto getCurrentRow = [](std::shared_ptr<TimelineModel> timeline, int trackId, int itemId) {
        int row = -1;
        if (timeline->isClip(itemId)) {
            row = timeline->getTrackById(trackId)->getRowfromClip(itemId);
        } else {
            Q_ASSERT(timeline->isComposition(itemId));
            row = timeline->getTrackById(trackId)->getRowfromComposition(itemId);
        }
        return row;
    };
    auto getTentativeRow = [](std::shared_ptr<TimelineModel> timeline, int trackId, int itemId, bool isClip) {
        int row = -1;
        if (isClip) {
            row = timeline->getTrackById(trackId)->getTentativeRowfromClip(itemId);
        } else {
            row = timeline->getTrackById(trackId)->getTentativeRowfromComposition(itemId);
        }
        return row;
    };
    return [ list, getCurrentRow = std::move(getCurrentRow), getTentativeRow = std::move(getTentativeRow) ]()
    {
        for (const auto &u : list) {
            if (u->isDelete()) {
                auto del = std::static_pointer_cast<DeleteUpdate>(u);
                if (auto timeline = del->getTimeline().lock()) {
                    int trackId = del->getTrackId(), itemId = del->getItemId();
                    int row = getCurrentRow(timeline, trackId, itemId);
                    timeline->_beginRemoveRows(timeline->makeTrackIndexFromID(trackId), row, row);
                } else {
                    qDebug() << "ERROR: impossible to lock timeline";
                    Q_ASSERT(false);
                }
            } else if (u->isInsert()) {
                auto ins = std::static_pointer_cast<InsertUpdate>(u);
                if (auto timeline = ins->getTimeline().lock()) {
                    int trackId = ins->getTrackId(), itemId = ins->getItemId();
                    int row = getTentativeRow(timeline, trackId, itemId, ins->isClip());
                    timeline->_beginInsertRows(timeline->makeTrackIndexFromID(trackId), row, row);
                } else {
                    qDebug() << "ERROR: impossible to lock timeline";
                    Q_ASSERT(false);
                }
            } else if (u->isMove()) {
                auto move = std::static_pointer_cast<MoveUpdate>(u);
                if (auto sTimeline = move->getSourceTimeline().lock()) {
                    if (auto tTimeline = move->getTargetTimeline().lock()) {
                        int sTrackId = move->getSourceTrackId(), itemId = move->getItemId();
                        int tTrackId = move->getTargetTrackId();
                        int sRow = getCurrentRow(sTimeline, sTrackId, itemId);
                        int tRow = getTentativeRow(tTimeline, tTrackId, itemId, sTimeline->isClip(itemId));
                        if (sTimeline == tTimeline) {
                            // we have a "true" move, within the timeline
                            sTimeline->_beginMoveRows(sTimeline->makeTrackIndexFromID(sTrackId), sRow, sRow, tTimeline->makeTrackIndexFromID(tTrackId), tRow);
                        } else {
                            // we move to a different timeline, we need to delete + insert
                            sTimeline->_beginRemoveRows(sTimeline->makeTrackIndexFromID(sTrackId), sRow, sRow);
                            tTimeline->_beginInsertRows(tTimeline->makeTrackIndexFromID(tTrackId), tRow, tRow);
                        }
                    } else {
                        qDebug() << "ERROR: impossible to lock target timeline";
                        Q_ASSERT(false);
                    }
                } else {
                    qDebug() << "ERROR: impossible to lock source timeline";
                    Q_ASSERT(false);
                }
            } else if (u->isChange()) {
                // nothing to do here, will be done in post apply
            } else {
                // not implemented?
                Q_ASSERT(false);
            }
        }
        return true;
    };
}

// This creates a lambda to be executed after the operation to finalize the update
Fun ModelUpdater::postApply_lambda(const std::vector<std::shared_ptr<AbstractUpdate>> &list)
{
    return [list]() {
        for (const auto &u : list) {
            if (u->isDelete()) {
                auto del = std::static_pointer_cast<DeleteUpdate>(u);
                if (auto timeline = del->getTimeline().lock()) {
                    timeline->_endRemoveRows();
                } else {
                    qDebug() << "ERROR: impossible to lock timeline";
                    Q_ASSERT(false);
                }
            } else if (u->isInsert()) {
                auto ins = std::static_pointer_cast<InsertUpdate>(u);
                if (auto timeline = ins->getTimeline().lock()) {
                    timeline->_endInsertRows();
                } else {
                    qDebug() << "ERROR: impossible to lock timeline";
                    Q_ASSERT(false);
                }
            } else if (u->isMove()) {
                auto move = std::static_pointer_cast<MoveUpdate>(u);
                if (auto sTimeline = move->getSourceTimeline().lock()) {
                    if (auto tTimeline = move->getTargetTimeline().lock()) {
                        if (sTimeline == tTimeline) {
                            sTimeline->_endMoveRows();
                        } else {
                            sTimeline->_endRemoveRows();
                            tTimeline->_endInsertRows();
                        }
                    } else {
                        qDebug() << "ERROR: impossible to lock target timeline";
                        Q_ASSERT(false);
                    }
                } else {
                    qDebug() << "ERROR: impossible to lock source timeline";
                    Q_ASSERT(false);
                }
            } else if (u->isChange()) {
                auto change = std::static_pointer_cast<ChangeUpdate>(u);
                if (auto timeline = change->getTimeline().lock()) {
                    int itemId = change->getItemId();
                    QModelIndex idx;
                    if (timeline->isClip(itemId)) {
                        idx = timeline->makeClipIndexFromID(itemId);
                    } else {
                        Q_ASSERT(timeline->isComposition(itemId));
                        idx = timeline->makeCompositionIndexFromID(itemId);
                    }
                    timeline->notifyChange(idx, idx, change->getRoles());
                }
            } else {
                // not implemented?
                Q_ASSERT(false);
            }
        }
        return true;
    };
}

void ModelUpdater::applyUpdates(Fun &undo, Fun &redo, const std::vector<std::shared_ptr<AbstractUpdate>> &list)
{
    if (list.size() == 0) {
        // Nothing todo, we can pass
        return;
    }
    auto updates = simplify(list);
    auto rev_updates = reverse(updates);

    // firstly, we need to undo the action
    bool undone = undo();
    Q_ASSERT(undone);

    redo = [ redo, pre = preApply_lambda(updates), post = postApply_lambda(updates) ]()
    {
        pre();
        auto res = redo();
        post();
        return res;
    };
    undo = [ undo, pre = preApply_lambda(rev_updates), post = postApply_lambda(rev_updates) ]()
    {
        pre();
        auto res = undo();
        post();
        return res;
    };
    redo();
}

DeleteUpdate::DeleteUpdate(int itemId, std::weak_ptr<TimelineModel> timeline, int trackId, int pos, bool isClip)
    : m_timeline(timeline)
    , m_itemId(itemId)
    , m_trackId(trackId)
    , m_pos(pos)
    , m_isClip(isClip)
{
}
bool DeleteUpdate::isDelete() const
{
    return true;
}
int DeleteUpdate::getItemId() const
{
    return m_itemId;
}
int DeleteUpdate::getTrackId() const
{
    return m_trackId;
}
int DeleteUpdate::getPos() const
{
    return m_pos;
}
bool DeleteUpdate::isClip() const
{
    return m_isClip;
}
std::weak_ptr<TimelineModel> DeleteUpdate::getTimeline() const
{
    return m_timeline;
}

std::shared_ptr<AbstractUpdate> DeleteUpdate::reverse()
{
    return std::shared_ptr<AbstractUpdate>(new InsertUpdate(m_itemId, m_timeline, m_trackId, m_pos, m_isClip));
}

void DeleteUpdate::print() const
{
    qDebug() << "Delete Update of item " << m_itemId << "from track" << m_trackId << "at pos" << m_pos;
}

InsertUpdate::InsertUpdate(int itemId, std::weak_ptr<TimelineModel> timeline, int trackId, int pos, bool isClip)
    : m_timeline(timeline)
    , m_itemId(itemId)
    , m_trackId(trackId)
    , m_pos(pos)
    , m_isClip(isClip)
{
}
bool InsertUpdate::isInsert() const
{
    return true;
}
int InsertUpdate::getItemId() const
{
    return m_itemId;
}
int InsertUpdate::getTrackId() const
{
    return m_trackId;
}
int InsertUpdate::getPos() const
{
    return m_pos;
}
bool InsertUpdate::isClip() const
{
    return m_isClip;
}
std::weak_ptr<TimelineModel> InsertUpdate::getTimeline() const
{
    return m_timeline;
}

std::shared_ptr<AbstractUpdate> InsertUpdate::reverse()
{
    return std::shared_ptr<AbstractUpdate>(new DeleteUpdate(m_itemId, m_timeline, m_trackId, m_pos, m_isClip));
}

void InsertUpdate::print() const
{
    qDebug() << "Insert Update of item " << m_itemId << "to track" << m_trackId << "at pos" << m_pos;
}

MoveUpdate::MoveUpdate(int itemId, std::weak_ptr<TimelineModel> sourceTimeline, int sourceTrackId, int sourcePos, std::weak_ptr<TimelineModel> targetTimeline,
                       int targetTrackId, int targetPos)
    : m_itemId(itemId)
    , m_sourceTimeline(sourceTimeline)
    , m_sourceTrackId(sourceTrackId)
    , m_sourcePos(sourcePos)
    , m_targetTimeline(targetTimeline)
    , m_targetTrackId(targetTrackId)
    , m_targetPos(targetPos)
{
}
bool MoveUpdate::isMove() const
{
    return true;
}
int MoveUpdate::getItemId() const
{
    return m_itemId;
}
std::weak_ptr<TimelineModel> MoveUpdate::getSourceTimeline() const
{
    return m_sourceTimeline;
}
std::weak_ptr<TimelineModel> MoveUpdate::getTargetTimeline() const
{
    return m_targetTimeline;
}
int MoveUpdate::getSourceTrackId() const
{
    return m_sourceTrackId;
}
int MoveUpdate::getSourcePos() const
{
    return m_sourcePos;
}
int MoveUpdate::getTargetTrackId() const
{
    return m_targetTrackId;
}
int MoveUpdate::getTargetPos() const
{
    return m_targetPos;
}

std::shared_ptr<AbstractUpdate> MoveUpdate::reverse()
{
    return std::make_shared<MoveUpdate>(m_itemId, m_targetTimeline, m_targetTrackId, m_targetPos, m_targetTimeline, m_targetTrackId, m_targetPos);
}

void MoveUpdate::print() const
{
    qDebug() << "Move Update of item " << m_itemId << "from track" << m_sourceTrackId << "at pos" << m_sourcePos << "to track" << m_targetTrackId << "at pos"
             << m_targetPos;
}

ChangeUpdate::ChangeUpdate(int itemId, std::weak_ptr<TimelineModel> timeline, const QVector<int> &roles)
    : m_timeline(timeline)
    , m_itemId(itemId)
    , m_roles(roles)
{
}
bool ChangeUpdate::isChange() const
{
    return true;
}
int ChangeUpdate::getItemId() const
{
    return m_itemId;
}
QVector<int> ChangeUpdate::getRoles() const
{
    return m_roles;
}
std::weak_ptr<TimelineModel> ChangeUpdate::getTimeline() const
{
    return m_timeline;
}

std::shared_ptr<AbstractUpdate> ChangeUpdate::reverse()
{
    return std::make_shared<ChangeUpdate>(m_itemId, m_timeline, m_roles);
}

void ChangeUpdate::print() const
{
    qDebug() << "Change Update of item " << m_itemId << "roles:";
    for (int role : m_roles) {
        switch (role) {
        case TimelineModel::NameRole:
            qDebug() << "NameRole";
            break;
        case TimelineModel::ResourceRole:
            qDebug() << "ResourceRole";
            break;
        case TimelineModel::ServiceRole:
            qDebug() << "ServiceRole";
            break;
        case TimelineModel::IsBlankRole:
            qDebug() << "IsBlankRole";
            break;
        case TimelineModel::StartRole:
            qDebug() << "StartRole";
            break;
        case TimelineModel::BinIdRole:
            qDebug() << "BinIdRole";
            break;
        case TimelineModel::MarkersRole:
            qDebug() << "MarkersRole";
            break;
        case TimelineModel::StatusRole:
            qDebug() << "StatusRole";
            break;
        case TimelineModel::TypeRole:
            qDebug() << "TypeRole";
            break;
        case TimelineModel::KeyframesRole:
            qDebug() << "KeyframesRole";
            break;
        case TimelineModel::DurationRole:
            qDebug() << "DurationRole";
            break;
        case TimelineModel::InPointRole:
            qDebug() << "InPointRole";
            break;
        case TimelineModel::OutPointRole:
            qDebug() << "OutPointRole";
            break;
        case TimelineModel::FramerateRole:
            qDebug() << "FramerateRole";
            break;
        case TimelineModel::GroupedRole:
            qDebug() << "GroupedRole";
            break;
        case TimelineModel::HasAudio:
            qDebug() << "HasAudio";
            break;
        case TimelineModel::CanBeAudioRole:
            qDebug() << "CanBeAudioRole";
            break;
        case TimelineModel::CanBeVideoRole:
            qDebug() << "CanBeVideoRole";
            break;
        case TimelineModel::IsDisabledRole:
            qDebug() << "IsDisabledRole";
            break;
        case TimelineModel::IsAudioRole:
            qDebug() << "IsAudioRole";
            break;
        case TimelineModel::SortRole:
            qDebug() << "SortRole";
            break;
        case TimelineModel::ShowKeyframesRole:
            qDebug() << "ShowKeyframesRole";
            break;
        case TimelineModel::AudioLevelsRole:
            qDebug() << "AudioLevelsRole";
            break;
        case TimelineModel::IsCompositeRole:
            qDebug() << "IsCompositeRole";
            break;
        case TimelineModel::IsLockedRole:
            qDebug() << "IsLockedRole";
            break;
        case TimelineModel::HeightRole:
            qDebug() << "HeightRole";
            break;
        case TimelineModel::TrackTagRole:
            qDebug() << "TrackTagRole";
            break;
        case TimelineModel::FadeInRole:
            qDebug() << "FadeInRole";
            break;
        case TimelineModel::FadeOutRole:
            qDebug() << "FadeOutRole";
            break;
        case TimelineModel::IsCompositionRole:
            qDebug() << "IsCompositionRole";
            break;
        case TimelineModel::FileHashRole:
            qDebug() << "FileHashRole";
            break;
        case TimelineModel::SpeedRole:
            qDebug() << "SpeedRole";
            break;
        case TimelineModel::ReloadThumbRole:
            qDebug() << "ReloadThumbRole";
            break;
        case TimelineModel::ItemATrack:
            qDebug() << "ItemATrack";
            break;
        case TimelineModel::ItemIdRole:
            qDebug() << "ItemIdRole";
            break;
        case TimelineModel::ThumbsFormatRole:
            qDebug() << "ThumbsFormatRole";
            break;
        }
    }
}
