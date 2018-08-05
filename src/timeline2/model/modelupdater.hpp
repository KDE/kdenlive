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

#ifndef MODELUPDATER_H
#define MODELUPDATER_H

#include "undohelper.hpp"
#include <QVector>
#include <memory>
#include <vector>

/* @brief

*/
class TimelineModel;

class AbstractUpdate
{
public:
    AbstractUpdate() {}

    virtual ~AbstractUpdate() = default;

    virtual std::shared_ptr<AbstractUpdate> reverse() = 0;

    virtual int getItemId() const = 0;
    virtual bool isDelete() const { return false; }
    virtual bool isInsert() const { return false; }
    virtual bool isChange() const { return false; }
    virtual bool isMove() const { return false; }

    // for debug purposes
    virtual void print() const = 0;
};

class DeleteUpdate : public AbstractUpdate
{
public:
    DeleteUpdate(int itemId, std::weak_ptr<TimelineModel> timeline, int trackId, int pos, bool isClip);
    bool isDelete() const override;
    int getItemId() const;
    int getTrackId() const;
    int getPos() const;
    bool isClip() const;
    std::weak_ptr<TimelineModel> getTimeline() const;

    std::shared_ptr<AbstractUpdate> reverse() override;
    void print() const override;

protected:
    std::weak_ptr<TimelineModel> m_timeline;
    int m_itemId, m_trackId, m_pos;
    bool m_isClip;
};

class InsertUpdate : public AbstractUpdate
{
public:
    InsertUpdate(int itemId, std::weak_ptr<TimelineModel> timeline, int trackId, int pos, bool isClip);
    bool isInsert() const override;
    int getItemId() const;
    int getTrackId() const;
    int getPos() const;
    bool isClip() const;
    std::weak_ptr<TimelineModel> getTimeline() const;

    std::shared_ptr<AbstractUpdate> reverse() override;
    void print() const override;

protected:
    std::weak_ptr<TimelineModel> m_timeline;
    int m_itemId, m_trackId, m_pos;
    bool m_isClip;
};

class MoveUpdate : public AbstractUpdate
{
public:
    MoveUpdate(int itemId, std::weak_ptr<TimelineModel> sourceTimeline, int sourceTrackId, int sourcePos, std::weak_ptr<TimelineModel> targetTimeline,
               int targetTrackId, int targetPos);
    bool isMove() const override;
    int getItemId() const;
    std::weak_ptr<TimelineModel> getSourceTimeline() const;
    std::weak_ptr<TimelineModel> getTargetTimeline() const;
    int getSourceTrackId() const;
    int getSourcePos() const;
    int getTargetTrackId() const;
    int getTargetPos() const;

    std::shared_ptr<AbstractUpdate> reverse() override;
    void print() const override;

protected:
    int m_itemId;
    std::weak_ptr<TimelineModel> m_sourceTimeline;
    int m_sourceTrackId, m_sourcePos;
    std::weak_ptr<TimelineModel> m_targetTimeline;
    int m_targetTrackId, m_targetPos;
};

class ChangeUpdate : public AbstractUpdate
{
public:
    ChangeUpdate(int itemId, std::weak_ptr<TimelineModel> timeline, const QVector<int> &roles);
    bool isChange() const override;
    int getItemId() const;
    QVector<int> getRoles() const;
    std::weak_ptr<TimelineModel> getTimeline() const;

    std::shared_ptr<AbstractUpdate> reverse() override;
    void print() const override;

protected:
    std::weak_ptr<TimelineModel> m_timeline;
    int m_itemId;
    QVector<int> m_roles;
};

using Updates = std::vector<std::shared_ptr<AbstractUpdate>>;
class ModelUpdater
{
public:
    static void applyUpdates(Fun &undo, Fun &redo, const std::vector<std::shared_ptr<AbstractUpdate>> &list);

protected:
    static Fun preApply_lambda(const std::vector<std::shared_ptr<AbstractUpdate>> &list);
    static Fun postApply_lambda(const std::vector<std::shared_ptr<AbstractUpdate>> &list);
};

#endif
