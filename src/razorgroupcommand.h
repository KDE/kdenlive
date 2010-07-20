/***************************************************************************
 *   Copyright (C) 2010 by Till Theato (root@ttill.de)                     *
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


#ifndef RAZORGROUPCOMMAND_H
#define RAZORGROUPCOMMAND_H

#include <QUndoCommand>

#include "definitions.h"

class CustomTrackView;

class RazorGroupCommand : public QUndoCommand
{
public:
    RazorGroupCommand(CustomTrackView *view, QList <ItemInfo> clips1, QList <ItemInfo> transitions1, QList <ItemInfo> clipsCut, QList <ItemInfo> transitionsCut, QList <ItemInfo> clips2, QList <ItemInfo> transitions2, GenTime cutPos, QUndoCommand * parent = 0);
    virtual void undo();
    virtual void redo();

private:
    CustomTrackView *m_view;
    QList <ItemInfo> m_clips1;
    QList <ItemInfo> m_transitions1;
    QList <ItemInfo> m_clipsCut;
    QList <ItemInfo> m_transitionsCut;
    QList <ItemInfo> m_clips2;
    QList <ItemInfo> m_transitions2;
    GenTime m_cutPos;
};

#endif
