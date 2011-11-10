/***************************************************************************
 *   Copyright (C) 2007 by Jean-Baptiste Mardelle (jb@kdenlive.org)        *
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


#ifndef ADDEFFECTCOMMAND_H
#define ADDEFFECTCOMMAND_H

#include <QUndoCommand>
#include <KDebug>
#include <QDomElement>
#include "gentime.h"

class CustomTrackView;

class AddEffectCommand : public QUndoCommand
{
public:
    AddEffectCommand(CustomTrackView *view, const int track, GenTime pos, QDomElement effect, bool doIt, QUndoCommand * parent = 0);

    virtual void undo();
    virtual void redo();

private:
    CustomTrackView *m_view;
    int m_track;
    QDomElement m_effect;
    GenTime m_pos;
    bool m_doIt;
};

#endif

