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

#ifndef RAZORMANAGER_H
#define RAZORMANAGER_H

#include "definitions.h"

class QGraphicsItem;
class QMouseEvent;
class CustomTrackView;
class QGraphicsLineItem;

#include "abstracttoolmanager.h"

/**
 * @namespace RazorManager
 * @brief Provides convenience methods to handle selection tool.
 */

class RazorManager : public AbstractToolManager
{
    Q_OBJECT

public:
    explicit RazorManager(CustomTrackView *view, std::shared_ptr<DocUndoStack> commandStack);
    bool mousePress(QMouseEvent *event, const ItemInfo &info = ItemInfo(), const QList<QGraphicsItem *> &list = QList<QGraphicsItem *>()) override;
    bool mouseMove(QMouseEvent *event, int pos = 0, int track = -1) override;
    void mouseRelease(QMouseEvent *event, GenTime pos = GenTime()) override;
    void enterEvent(int pos, double trackHeight) override;
    void leaveEvent() override;
    void initTool(double trackHeight) override;
    void closeTool() override;
    /** @brief Check if a guide operation is applicable on items under mouse.
     * @param items The list of items under mouse
     * @param operationMode Will be set to MoveGuide if applicable
     * @param abort Will be set to true if an operation matched and the items list should not be tested for further operation modes
     **/
    static void checkOperation(QGraphicsItem *item, CustomTrackView *view, QMouseEvent *event, int eventPos, OperationType &operationMode, bool &abort);

public slots:
    void slotRefreshCutLine();
    void updateTimelineItems() override;

private:
    QGraphicsLineItem *m_cutLine;
    QCursor m_cursor;
    void buildCutLine(double trackHeight);
};

#endif

