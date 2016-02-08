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
#include "../customtrackview.h"
#include "../abstractclipitem.h"
#include "../gentime.h"
#include "bin/projectclip.h"
#include "mltcontroller/clipcontroller.h"

#include <QMouseEvent>
#include <QGraphicsItem>

void RazorManager::checkOperation(QGraphicsItem *item, CustomTrackView *view, QMouseEvent *event, int eventPos, OperationType &operationMode, bool &abort)
{
    if (item && event->buttons() == Qt::NoButton && operationMode != ZoomTimeline) {
        // razor tool over a clip, display current frame in monitor
        if (event->modifiers() == Qt::ShiftModifier && item->type() == AVWidget) {
            AbstractClipItem *clip = static_cast <AbstractClipItem*>(item);
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

