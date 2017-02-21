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

#ifndef SELECTMANAGER_H
#define SELECTMANAGER_H

#include "abstracttoolmanager.h"

class QGraphicsItem;
class QMouseEvent;
class CustomTrackView;
class AbstractGroupItem;

/**
 * @namespace SelectManager
 * @brief Provides convenience methods to handle selection tool.
 */

class SelectManager : public AbstractToolManager
{
    Q_OBJECT

public:
    explicit SelectManager(CustomTrackView *view, DocUndoStack *commandStack = nullptr);
    bool mousePress(QMouseEvent *event, const ItemInfo &info = ItemInfo(), const QList<QGraphicsItem *> &list = QList<QGraphicsItem *>()) Q_DECL_OVERRIDE;
    void mouseRelease(QMouseEvent *event, GenTime pos = GenTime()) Q_DECL_OVERRIDE;
    bool mouseMove(QMouseEvent *event, int pos, int) Q_DECL_OVERRIDE;
    /** @brief Check if a guide operation is applicable on items under mouse.
     * @param item The item under mouse
     * @param operationMode Will be set to under mouse operation if applicable
     * @param abort Will be set to true if an operation matched and the items list should not be tested for further operation modes
     **/
    static void checkOperation(QGraphicsItem *item, CustomTrackView *view, QMouseEvent *event, AbstractGroupItem *group, OperationType &operationMode, OperationType moveOperation);
    void initTool(double trackHeight) Q_DECL_OVERRIDE;
private:
    Qt::KeyboardModifiers m_modifiers;
    bool m_dragMoved;
    QPoint m_clickPoint;
};

#endif
