/***************************************************************************
                          addtransitioncommand.h  -  description
                             -------------------
    begin                : 2008
    copyright            : (C) 2008 by Marco Gittler
    email                : g.marco@freenet.de
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef ADDTRANSITIONCOMMAND_H
#define ADDTRANSITIONCOMMAND_H

#include <QUndoCommand>
#include <QGraphicsView>
#include <QPointF>
#include <QDomElement>
#include <KDebug>

#include "gentime.h"
#include "definitions.h"
class CustomTrackView;


class AddTransitionCommand : public QUndoCommand
{
public:
    AddTransitionCommand(CustomTrackView *view, ItemInfo info, int transitiontrack, QDomElement params, bool remove, bool doIt, QUndoCommand * parent = 0);
    virtual void undo();
    virtual void redo();

private:
    CustomTrackView *m_view;
    ItemInfo m_info;
    QDomElement m_params;
    int m_track;
    bool m_doIt;
    bool m_remove;
    bool m_refresh;
};

#endif

