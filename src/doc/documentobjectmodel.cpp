/***************************************************************************
 *   Copyright (C) 2021 by Jean-Baptiste Mardelle                          *
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

#include "documentobjectmodel.h"
#include "timeline2/view/timelinewidget.h"
#include "timeline2/view/timelinecontroller.h"
#include "bin/projectitemmodel.h"

DocumentObjectModel::DocumentObjectModel(TimelineWidget *timeline, std::shared_ptr<ProjectItemModel> projectModel, QObject *parent)
    : QObject(parent)
    , m_timeline(timeline)
    , m_projectModel(projectModel)
{
}

TimelineWidget *DocumentObjectModel::timeline()
{
    return m_timeline;
}

std::pair<int,int> DocumentObjectModel::getItemInOut(const ObjectId &id)
{
    switch (id.first) {
    case ObjectType::TimelineClip:
        if (m_timeline->controller()->getModel()->isClip(id.second)) {
            return m_timeline->controller()->getModel()->getClipInOut(id.second);
        }
        break;
    case ObjectType::TimelineComposition:
        if (m_timeline->controller()->getModel()->isComposition(id.second)) {
            return {0, m_timeline->controller()->getModel()->getCompositionPlaytime(id.second)};
        }
        break;
    case ObjectType::BinClip:
        return {0, m_projectModel->getClipDuration(id.second)};
    case ObjectType::TimelineTrack:
    case ObjectType::Master:
        return {0, m_timeline->controller()->duration() - 1};
    case ObjectType::TimelineMix:
        if (m_timeline->controller()->getModel()->isClip(id.second)) {
            return m_timeline->controller()->getModel()->getMixInOut(id.second);
        } else {
            qWarning() << "querying non clip properties";
        }
        break;
    default:
        qWarning() << "unhandled object type";
    }
    return {-1, -1};
}

int DocumentObjectModel::getItemPosition(const ObjectId &id)
{
    switch (id.first) {
    case ObjectType::TimelineClip:
        if (m_timeline->controller()->getModel()->isClip(id.second)) {
            return m_timeline->controller()->getModel()->getClipPosition(id.second);
        }
        break;
    case ObjectType::TimelineComposition:
        if (m_timeline->controller()->getModel()->isComposition(id.second)) {
            return m_timeline->controller()->getModel()->getCompositionPosition(id.second);
        }
        break;
    case ObjectType::TimelineMix:
        if (m_timeline->controller()->getModel()->isClip(id.second)) {
            return m_timeline->controller()->getModel()->getMixInOut(id.second).first;
        } else {
            qWarning() << "querying non clip properties";
        }
        break;
    case ObjectType::BinClip:
    case ObjectType::TimelineTrack:
    case ObjectType::Master:
        return 0;
    default:
        qWarning() << "unhandled object type";
    }
    return 0;
}

