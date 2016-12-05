/***************************************************************************
 *   Copyright (C) 2016 by Jean-Baptiste Mardelle (jb@kdenlive.org)        *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA          *
 ***************************************************************************/

#include "abstracttoolmanager.h"
#include "timeline/customtrackview.h"

AbstractToolManager::AbstractToolManager(ToolManagerType type, CustomTrackView *view, DocUndoStack *commandStack) : QObject()
    , m_managerType(type)
    , m_view(view)
    , m_commandStack(commandStack)
{
}

AbstractToolManager::ToolManagerType AbstractToolManager::type() const
{
    return m_managerType;
}

bool AbstractToolManager::mouseMove(QMouseEvent *, int, int)
{
    return true;
}

bool AbstractToolManager::isCurrent(OperationType type) const
{
    switch (type) {
    case MoveOperation:
        return m_managerType == MoveType;
        break;
    case ResizeStart:
    case ResizeEnd:
        return m_managerType == ResizeType;
        break;
    case RollingStart:
    case RollingEnd:
    case RippleStart:
    case RippleEnd:
        return m_managerType == TrimType;
        break;
    case MoveGuide:
        return m_managerType == GuideType;
        break;
    case Spacer:
        return m_managerType == SpacerType;
        break;
    default:
        return m_managerType == SelectType;
        break;
    }
}

AbstractToolManager::ToolManagerType AbstractToolManager::getTool(OperationType type) const
{
    switch (type) {
    case MoveOperation:
        return MoveType;
        break;
    case ResizeStart:
    case ResizeEnd:
        return ResizeType;
        break;
    case RollingStart:
    case RollingEnd:
    case RippleStart:
    case RippleEnd:
        return TrimType;
        break;
    case MoveGuide:
        return GuideType;
        break;
    case Spacer:
        return SpacerType;
        break;
    default:
        return SelectType;
        break;
    }
}

void AbstractToolManager::updateTimelineItems()
{
}

void AbstractToolManager::initTool(double)
{
}

void AbstractToolManager::enterEvent(int, double)
{
}

void AbstractToolManager::leaveEvent()
{
}

void AbstractToolManager::closeTool()
{
}
