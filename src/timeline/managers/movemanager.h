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

#ifndef MOVEMANAGER_H
#define MOVEMANAGER_H

#include "abstracttoolmanager.h"

#include <QTimer>
#include <QPoint>

class TransitionHandler;

/**
 * @namespace MoveManager
 * @brief Provides convenience methods to handle selection tool.
 */

class MoveManager : public AbstractToolManager
{
    Q_OBJECT

public:
    explicit MoveManager(TransitionHandler *handler, CustomTrackView *view, DocUndoStack *commandStack = nullptr);
    bool mousePress(QMouseEvent *event, const ItemInfo &info = ItemInfo(), const QList<QGraphicsItem *> &list = QList<QGraphicsItem *>()) Q_DECL_OVERRIDE;
    void mouseRelease(QMouseEvent *event, GenTime pos = GenTime()) Q_DECL_OVERRIDE;
    bool mouseMove(QMouseEvent *event, int pos, int) Q_DECL_OVERRIDE;

private:
    ItemInfo m_dragItemInfo;
    TransitionHandler *m_transitionHandler;
    int m_scrollOffset;
    int m_scrollTrigger;
    QTimer m_scrollTimer;
    bool m_dragMoved;
    QPoint m_clickPoint;

private slots:
    void slotCheckMouseScrolling();
};

#endif

