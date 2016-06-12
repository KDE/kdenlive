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

#include "razormanager.h"
#include "../clipitem.h"
#include "timeline/customtrackview.h"
#include "timeline/clipitem.h"
#include "timeline/abstractgroupitem.h"
#include "timeline/timelinecommands.h"

#include <QMouseEvent>
#include <QGraphicsItem>
#include <klocalizedstring.h>

RazorManager::RazorManager(DocUndoStack *commandStack, CustomTrackView *view) : AbstractToolManager(view)
    , m_commandStack(commandStack)
{
}

bool RazorManager::mousePress(ItemInfo info, Qt::KeyboardModifiers)
{
    AbstractClipItem *dragItem = m_view->dragItem();
    AbstractGroupItem *selectionGroup = m_view->selectionGroup();

    if (!dragItem) {
        // clicked in empty area, ignore
        return false;
    }
    if (dragItem->type() == TransitionWidget) {
        m_view->displayMessage(i18n("Cannot cut a transition"), ErrorMessage);
    } else {
        if (dragItem->parentItem() && dragItem->parentItem() != selectionGroup) {
            m_view->razorGroup(static_cast<AbstractGroupItem*>(dragItem->parentItem()), info.startPos);
        } else {
            ClipItem *clip = static_cast <ClipItem *>(dragItem);
            if (clip->info().contains(info.startPos)) {
                RazorClipCommand* command = new RazorClipCommand(m_view, clip->info(), clip->effectList(), info.startPos);
                m_commandStack->push(command);
            }
        }
    }
    dragItem->setMainSelectedClip(false);
    dragItem = NULL;
    return true;
}

void RazorManager::mouseMove(int pos)
{
    Q_UNUSED(pos);
}

void RazorManager::mouseRelease(DocUndoStack *commandStack, GenTime pos)
{
    Q_UNUSED(pos);
    m_view->setCursor(Qt::OpenHandCursor);
    m_view->setOperationMode(None);
}

//static
void RazorManager::checkOperation(QGraphicsItem *item, CustomTrackView *view, QMouseEvent *event, int eventPos, OperationType &operationMode, bool &abort)
{
    if (item && event->buttons() == Qt::NoButton && operationMode != ZoomTimeline) {
        // razor tool over a clip, display current frame in monitor
        if (event->modifiers() == Qt::ShiftModifier && item->type() == AVWidget) {
            ClipItem *clip = static_cast <ClipItem*>(item);
            QMetaObject::invokeMethod(view, "showClipFrame", Qt::QueuedConnection, Q_ARG(QString, clip->getBinId()), Q_ARG(int, eventPos - (clip->startPos() - clip->cropStart()).frames(view->fps())));
        }
        event->accept();
        abort = true;
    } else {
        // No clip under razor
        event->accept();
        abort = true;
    }
}

