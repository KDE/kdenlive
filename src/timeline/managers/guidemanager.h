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

#ifndef GUIDEMANAGER_H
#define GUIDEMANAGER_H

#include "abstracttoolmanager.h"

class QGraphicsItem;
class QMouseEvent;
class CustomTrackView;
class Guide;

/**
 * @namespace GuideManager
 * @brief Provides convenience methods to handle timeline guides.
 */

class GuideManager : public AbstractToolManager
{
    Q_OBJECT

public:
    explicit GuideManager(CustomTrackView *view, DocUndoStack *commandStack = nullptr);
    bool mousePress(QMouseEvent *event, const ItemInfo &info = ItemInfo(), const QList<QGraphicsItem *> &list = QList<QGraphicsItem *>()) Q_DECL_OVERRIDE;
    void mouseRelease(QMouseEvent *event, GenTime pos = GenTime()) Q_DECL_OVERRIDE;
    bool mouseMove(QMouseEvent *event, int pos, int) Q_DECL_OVERRIDE;
    /** @brief Check if a guide operation is applicable on items under mouse.
     * @param items The list of items under mouse
     * @param operationMode Will be set to MoveGuide if applicable
     * @param abort Will be set to true if an operation matched and the items list should not be tested for further operation modes
     **/
    static void checkOperation(const QList<QGraphicsItem *> &items, CustomTrackView *parent, QMouseEvent * /*event*/, OperationType &operationMode, bool &abort);

private:
    QList<QGraphicsItem *> m_collisionList;
    Guide *m_dragGuide;
};

#endif
