/*
    SPDX-FileCopyrightText: 2022 Jean-Baptiste Mardelle <jb@kdenlive.org>

SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "documentobjectmodel.h"
#include "timeline2/view/timelinewidget.h"
#include "timeline2/view/timelinecontroller.h"
#include "bin/projectitemmodel.h"

KdenliveDocObjectModel::KdenliveDocObjectModel(TimelineWidget *timeline, std::shared_ptr<ProjectItemModel> projectModel, QObject *parent)
    : QObject(parent)
    , m_timeline(timeline)
    , m_projectModel(projectModel)
{
}

TimelineWidget *KdenliveDocObjectModel::timeline()
{
    return m_timeline;
}

std::pair<int,int> KdenliveDocObjectModel::getItemInOut(const ObjectId &id)
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

int KdenliveDocObjectModel::getItemPosition(const ObjectId &id)
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

