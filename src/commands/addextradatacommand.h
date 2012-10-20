/***************************************************************************
                          addextradatacommand.h  -  description
                             -------------------
    begin                : 2012
    copyright            : (C) 2012 by Jean-Baptiste Mardelle
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

#ifndef EXTRADATACOMMAND_H
#define EXTRADATACOMMAND_H

#include <QUndoCommand>
#include <QGraphicsView>
#include <QPointF>
#include <QDomElement>
#include <KDebug>

#include "gentime.h"
#include "definitions.h"
class CustomTrackView;


class AddExtraDataCommand : public QUndoCommand
{
public:
    AddExtraDataCommand(CustomTrackView *view, const QString&id, const QString&key, const QString &oldData, const QString &newData, QUndoCommand * parent = 0);
    virtual void undo();
    virtual void redo();

private:
    CustomTrackView *m_view;
    QString m_oldData;
    QString m_newData;
    QString m_key;
    QString m_id;
};

#endif

