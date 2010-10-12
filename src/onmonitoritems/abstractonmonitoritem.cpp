/***************************************************************************
 *   Copyright (C) 2010 by Till Theato (root@ttill.de)                     *
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

#include "abstractonmonitoritem.h"
#include "monitorscene.h"

#include <QGraphicsSceneMouseEvent>


AbstractOnMonitorItem::AbstractOnMonitorItem(MonitorScene* scene) :
    m_modified(false)
{
    connect(scene, SIGNAL(mousePressed(QGraphicsSceneMouseEvent*)), this, SLOT(slotMousePressed(QGraphicsSceneMouseEvent*)));
    connect(scene, SIGNAL(mouseReleased(QGraphicsSceneMouseEvent*)), this, SLOT(slotMouseReleased(QGraphicsSceneMouseEvent*)));
    connect(scene, SIGNAL(mouseMoved(QGraphicsSceneMouseEvent*)), this, SLOT(slotMouseMoved(QGraphicsSceneMouseEvent*)));
    connect(this, SIGNAL(actionFinished()), scene, SIGNAL(actionFinished()));
    connect(this, SIGNAL(requestCursor(const QCursor &)), scene, SLOT(slotSetCursor(const QCursor &)));
}

void AbstractOnMonitorItem::slotMouseReleased(QGraphicsSceneMouseEvent* event)
{
    if (m_modified) {
        m_modified = false;
        emit actionFinished();
    }
    event->accept();
}

#include "abstractonmonitoritem.moc"
