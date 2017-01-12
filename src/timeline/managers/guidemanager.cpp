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

#include "guidemanager.h"
#include "timeline/customtrackview.h"
#include "timeline/timelinecommands.h"
#include "timeline/abstractclipitem.h"

#include <QMouseEvent>
#include <QGraphicsItem>

GuideManager::GuideManager(CustomTrackView *view, DocUndoStack *commandStack) : AbstractToolManager(GuideType, view, commandStack)
    , m_dragGuide(nullptr)
{
}

bool GuideManager::mousePress(QMouseEvent *, const ItemInfo &info, const QList<QGraphicsItem *> &list)
{
    Q_UNUSED(info);
    m_collisionList = list;
    // if a guide and a clip were pressed, just select the guide
    for (int i = 0; i < m_collisionList.count(); ++i) {
        if (m_collisionList.at(i)->type() == GUIDEITEM) {
            // a guide item was pressed
            m_dragGuide = static_cast<Guide *>(m_collisionList.at(i));
            m_dragGuide->setFlag(QGraphicsItem::ItemIsMovable, true);
            m_view->setOperationMode(MoveGuide);
            // deselect all clips so that only the guide will move
            m_view->clearSelection();
            m_view->updateSnapPoints(nullptr);
            return true;
        }
    }
    return false;
}

bool GuideManager::mouseMove(QMouseEvent *event, int, int)
{
    event->accept();
    return false;
}

void GuideManager::mouseRelease(QMouseEvent *, GenTime pos)
{
    m_view->setCursor(Qt::ArrowCursor);
    m_dragGuide->setFlag(QGraphicsItem::ItemIsMovable, false);
    GenTime newPos = GenTime(m_dragGuide->pos().x(), m_view->fps());
    if (newPos != m_dragGuide->position()) {
        EditGuideCommand *command = new EditGuideCommand(m_view, m_dragGuide->position(), m_dragGuide->label(), newPos, m_dragGuide->label(), false);
        m_commandStack->push(command);
        m_dragGuide->updateGuide(GenTime(m_dragGuide->pos().x(), m_view->fps()));
        m_view->guidesUpdated();
        m_dragGuide = nullptr;
        AbstractClipItem *dragItem = m_view->dragItem();
        if (dragItem) {
            dragItem->setMainSelectedClip(false);
        }
        dragItem = nullptr;
        m_view->setOperationMode(None);
    }
    m_view->sortGuides();
    m_collisionList.clear();
    Q_UNUSED(pos);
}

//static
void GuideManager::checkOperation(const QList<QGraphicsItem *> &items, CustomTrackView *parent, QMouseEvent * /*event*/, OperationType &operationMode, bool &abort)
{
    if (items.count() == 1 && items.at(0)->type() == GUIDEITEM) {
        operationMode = MoveGuide;
        parent->setCursor(Qt::SplitHCursor);
        abort = true;
    }
}
