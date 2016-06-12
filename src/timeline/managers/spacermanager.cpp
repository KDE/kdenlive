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

#include <KLocalizedString>
#include <QProcess>
#include <QGraphicsItem>



SpacerManager::SpacerManager(CustomTrackView *view, DocUndoStack *commandStack) : AbstractToolManager(view, commandStack)
{
}

bool SpacerManager::mousePress(ItemInfo info, Qt::KeyboardModifiers modifiers)
{
    m_view->clearSelection();
    m_view->updateClipTypeActions(NULL);
    m_view->setOperationMode(Spacer);
    QList<QGraphicsItem *> selection;
    if (modifiers & Qt::ControlModifier) {
        // Ctrl + click, select all items on track after click position
        m_track = info.track;
        if (m_view->spaceToolSelectTrackOnly(info.track, selection))
            return true;
    } else {
        m_track = -1;
        // Select all items on all tracks after click position
        selection = m_view->selectAllItemsToTheRight(info.startPos.frames(m_view->fps()));
    }
    m_startPos = m_view->createGroupForSelectedItems(selection);
    m_spacerOffset = m_startPos - info.startPos;
    return true;
}

void SpacerManager::mouseMove(int pos)
{
    m_view->setCursor(Qt::SplitHCursor);
    int snappedPos = m_view->getSnapPointForPos(pos + m_spacerOffset.frames(m_view->fps()));
    if (snappedPos < 0) snappedPos = 0;
    m_view->spaceToolMoveToSnapPos(snappedPos);
}

void SpacerManager::mouseRelease(GenTime pos)
{
    GenTime timeOffset = pos - m_startPos;
    m_view->completeSpaceOperation(m_track, timeOffset);
}


