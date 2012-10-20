/***************************************************************************
                          addmarkercommand.h  -  description
                             -------------------
    begin                : 2008
    copyright            : (C) 2008 by Jean-Baptiste Mardelle
    email                : jb@kdenlive.org
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef MARKERCOMMAND_H
#define MARKERCOMMAND_H

#include <QUndoCommand>
#include <QGraphicsView>
#include <QPointF>
#include <QDomElement>
#include <KDebug>

#include "gentime.h"
#include "definitions.h"
class CustomTrackView;


class AddMarkerCommand : public QUndoCommand
{
public:
    AddMarkerCommand(CustomTrackView *view, const CommentedTime oldMarker, const CommentedTime newMarker, const QString &id, QUndoCommand * parent = 0);
    virtual void undo();
    virtual void redo();

private:
    CustomTrackView *m_view;
    CommentedTime m_oldMarker;
    CommentedTime m_newMarker;
    QString m_id;
};

#endif

