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

#ifndef ABSTRACTTOOLMANAGER_H
#define ABSTRACTTOOLMANAGER_H

/**
 * @namespace AbstractToolManager
 * @brief Base class for timeline tools.
 */

#include "definitions.h"
#include <QObject>

class CustomTrackView;
class DocUndoStack;
class QGraphicsItem;

class AbstractToolManager : public QObject
{
    Q_OBJECT

public:
    enum ToolManagerType {
        TrimType = 0,
        SpacerType,
        MoveType,
        ResizeType,
        RazorType,
        SelectType,
        GuideType
    };
    explicit AbstractToolManager(ToolManagerType type, CustomTrackView *view, DocUndoStack *commandStack = nullptr);
    virtual bool mousePress(QMouseEvent *event, const ItemInfo &info = ItemInfo(), const QList<QGraphicsItem *> &list = QList<QGraphicsItem *>()) = 0;
    virtual bool mouseMove(QMouseEvent *event, int pos = 0, int track = -1);
    virtual void mouseRelease(QMouseEvent *event, GenTime pos = GenTime()) = 0;
    virtual void enterEvent(int pos, double trackHeight);
    virtual void leaveEvent();
    virtual void initTool(double trackHeight);
    virtual void closeTool();
    /** @brief returns true if the current tool is adapted to the operation mode */
    bool isCurrent(OperationType type) const;
    /** @brief returns the tool adapted to the operation mode */
    AbstractToolManager::ToolManagerType getTool(OperationType type) const;
    AbstractToolManager::ToolManagerType type() const;

public slots:
    virtual void updateTimelineItems();

protected:
    ToolManagerType m_managerType;
    CustomTrackView *m_view;
    DocUndoStack *m_commandStack;
};

#endif
