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
#include "utils/KoIconUtils.h"

#include <QMouseEvent>
#include <QIcon>
#include <QGraphicsItem>
#include <QGraphicsLineItem>
#include <klocalizedstring.h>

RazorManager::RazorManager(CustomTrackView *view, DocUndoStack *commandStack) : AbstractToolManager(RazorType, view, commandStack)
    , m_cutLine(nullptr)
{
    QIcon razorIcon = KoIconUtils::themedIcon(QStringLiteral("edit-cut"));
    m_cursor = QCursor(razorIcon.pixmap(32, 32));
}

bool RazorManager::mousePress(QMouseEvent *, const ItemInfo &info, const QList<QGraphicsItem *> &)
{
    AbstractClipItem *dragItem = m_view->dragItem();
    if (!dragItem) {
        return false;
    }
    QList<QGraphicsItem *> items;
    if (dragItem->parentItem()) {
        items << dragItem->parentItem()->childItems();
    } else {
        items << dragItem;
    }
    m_view->cutSelectedClips(items, info.startPos);
    return true;
}

void RazorManager::enterEvent(int, double trackHeight)
{
    buildCutLine(trackHeight);
}

void RazorManager::initTool(double trackHeight)
{
    buildCutLine(trackHeight);
    m_view->setCursor(m_cursor);
}

void RazorManager::buildCutLine(double trackHeight)
{
    if (!m_cutLine) {
        m_cutLine = m_view->scene()->addLine(0, 0, 0, trackHeight);
        m_cutLine->setZValue(1000);
        QPen pen1 = QPen();
        pen1.setWidth(1);
        QColor line(Qt::red);
        pen1.setColor(line);
        m_cutLine->setPen(pen1);
        m_cutLine->setFlag(QGraphicsItem::ItemIgnoresTransformations, true);
        slotRefreshCutLine();
    }
}

void RazorManager::leaveEvent()
{
    delete m_cutLine;
    m_cutLine = nullptr;
}

void RazorManager::closeTool()
{
    delete m_cutLine;
    m_cutLine = nullptr;
}

bool RazorManager::mouseMove(QMouseEvent *, int pos, int track)
{
    if (m_cutLine) {
        m_cutLine->setPos(pos, track);
    }
    return true;
}

void RazorManager::updateTimelineItems()
{
    if (m_cutLine) {
        slotRefreshCutLine();
    }
}

void RazorManager::mouseRelease(QMouseEvent *, GenTime pos)
{
    Q_UNUSED(pos);
    m_view->setOperationMode(None);
}

//static
void RazorManager::checkOperation(QGraphicsItem *item, CustomTrackView *view, QMouseEvent *event, int eventPos, OperationType &operationMode, bool &abort)
{
    if (item && event->buttons() == Qt::NoButton && operationMode != ZoomTimeline) {
        // razor tool over a clip, display current frame in monitor
        if (event->modifiers() == Qt::ShiftModifier && item->type() == AVWidget) {
            ClipItem *clip = static_cast <ClipItem *>(item);
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

void RazorManager::slotRefreshCutLine()
{
    if (m_cutLine) {
        QPointF pos = m_view->mapToScene(m_view->mapFromGlobal(QCursor::pos()));
        int mappedXPos = qMax((int)(pos.x()), 0);
        m_cutLine->setPos(mappedXPos, m_view->getPositionFromTrack(m_view->getTrackFromPos(pos.y())));
    }
}
