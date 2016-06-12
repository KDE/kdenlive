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

#ifndef TRIMMANAGER_H
#define TRIMMANAGER_H

#include "abstracttoolmanager.h"

class ClipItem;

/**
 * @namespace TrimManager
 * @brief Provides convenience methods to handle selection tool.
 */

class TrimManager : public AbstractToolManager
{
    Q_OBJECT

public:
    explicit TrimManager(CustomTrackView *view, DocUndoStack *commandStack = NULL);
    bool mousePress(ItemInfo info = ItemInfo(), Qt::KeyboardModifiers modifiers = Qt::NoModifier);
    void mouseMove(int pos);
    void mouseRelease(GenTime pos = GenTime());

private:
    ClipItem *m_firstClip;
    ClipItem *m_secondClip;
    ItemInfo m_firstInfo;
    ItemInfo m_secondInfo;
};

#endif

