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

class AbstractToolManager : public QObject
{
    Q_OBJECT

public:
    explicit AbstractToolManager(CustomTrackView *view, DocUndoStack *commandStack = NULL);
    virtual bool mousePress(ItemInfo info = ItemInfo(), Qt::KeyboardModifiers modifiers = Qt::NoModifier) = 0;
    virtual void mouseMove(int pos = 0) = 0;
    virtual void mouseRelease(GenTime pos = GenTime()) = 0;

protected:
    CustomTrackView *m_view;
    DocUndoStack *m_commandStack;
};

#endif
