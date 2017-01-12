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

#include "spacermanager.h"
#include "timeline/customtrackview.h"
#include "timeline/clipitem.h"
#include "timeline/abstractgroupitem.h"

#include <QGraphicsItem>
#include <QApplication>

SpacerManager::SpacerManager(CustomTrackView *view, DocUndoStack *commandStack) : AbstractToolManager(SpacerType, view, commandStack)
    , m_track(0)
    , m_dragMoved(false)
{
}

bool SpacerManager::mousePress(QMouseEvent *event, const ItemInfo &info, const QList<QGraphicsItem *> &)
{
    m_view->clearSelection();
    m_view->updateClipTypeActions(nullptr);
    m_view->setOperationMode(Spacer);
    m_dragMoved = false;
    m_clickPoint = event->pos();
    QList<QGraphicsItem *> selection;
    if (event->modifiers() & Qt::ControlModifier) {
        // Ctrl + click, select all items on track after click position
        m_track = info.track;
        if (m_view->spaceToolSelectTrackOnly(info.track, selection)) {
            event->accept();
            return false;
        }
    } else {
        m_track = -1;
        // Select all items on all tracks after click position
        selection = m_view->selectAllItemsToTheRight(info.startPos.frames(m_view->fps()));
    }
    m_startPos = m_view->createGroupForSelectedItems(selection);
    m_spacerOffset = m_startPos - info.startPos;
    event->accept();
    return false;
}

void SpacerManager::initTool(double)
{
    qCDebug(KDENLIVE_LOG) << "* ** INIT SPACER";
    m_view->setCursor(Qt::SplitHCursor);
}

bool SpacerManager::mouseMove(QMouseEvent *event, int pos, int)
{
    if (event->buttons() & Qt::LeftButton) {
        if (!m_dragMoved) {
            if ((m_clickPoint - event->pos()).manhattanLength() < QApplication::startDragDistance()) {
                event->ignore();
                return false;
            }
            m_dragMoved = true;
        }
        int snappedPos = m_view->getSnapPointForPos(pos + m_spacerOffset.frames(m_view->fps()));
        if (snappedPos < 0) {
            snappedPos = 0;
        }
        m_view->spaceToolMoveToSnapPos(snappedPos);
        event->accept();
        return true;
    }
    return false;
}

void SpacerManager::mouseRelease(QMouseEvent *, GenTime pos)
{
    //GenTime timeOffset = pos - m_startPos;
    Q_UNUSED(pos);
    if (!m_dragMoved || !m_view->selectionGroup()) {
        m_view->clearSelection();
        return;
    }
    GenTime timeOffset = GenTime(m_view->selectionGroup()->sceneBoundingRect().left(), m_view->fps()) - m_startPos;
    m_view->completeSpaceOperation(m_track, timeOffset);
}

