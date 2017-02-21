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

#ifndef SPACERMANAGER_H
#define SPACERMANAGER_H

#include "abstracttoolmanager.h"

/**
 * @namespace SpacerManager
 * @brief Provides convenience methods to handle selection tool.
 */

class SpacerManager : public AbstractToolManager
{
    Q_OBJECT

public:
    explicit SpacerManager(CustomTrackView *view, DocUndoStack *commandStack = nullptr);
    bool mousePress(QMouseEvent *event, const ItemInfo &info = ItemInfo(), const QList<QGraphicsItem *> &list = QList<QGraphicsItem *>()) Q_DECL_OVERRIDE;
    bool mouseMove(QMouseEvent *event, int pos = 0, int track = -1) Q_DECL_OVERRIDE;
    void mouseRelease(QMouseEvent *event, GenTime pos = GenTime()) Q_DECL_OVERRIDE;
    void initTool(double trackHeight) Q_DECL_OVERRIDE;

private:
    int m_track;
    GenTime m_startPos;
    GenTime m_spacerOffset;
    bool m_dragMoved;
    QPoint m_clickPoint;
};

#endif

