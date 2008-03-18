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

#ifndef TIMELINETRANSITIONCOMMAND_H
#define TIMELINETRANSITIONCOMMAND_H

#include <QUndoCommand>
#include <QGraphicsView>
#include <QPointF>

#include <KDebug>

#include "gentime.h"
#include "projectlist.h"
#include "customtrackview.h"

class AddTransitionCommand : public QUndoCommand {
public:
    AddTransitionCommand(CustomTrackView *view, int track, QDomElement xml , GenTime pos, bool doIt);
    virtual void undo();
    virtual void redo();

private:
    CustomTrackView *m_view;
    GenTime m_pos;
    QDomElement m_xml;
    int m_track;
    bool m_doIt;
};

#endif

