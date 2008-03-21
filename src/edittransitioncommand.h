/***************************************************************************
                          edittransitioncommand.h  -  description
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

#ifndef EDITTRANSITIONCOMMAND_H
#define EDITTRANSITIONCOMMAND_H

#include <QUndoCommand>
#include <KDebug>
#include <QDomElement>
#include "gentime.h"

class CustomTrackView;

class EditTransitionCommand : public QUndoCommand {
public:
    EditTransitionCommand(CustomTrackView *view, const int track, GenTime pos, QDomElement oldeffect, QDomElement effect, bool doIt);

    virtual int id() const;
    virtual bool mergeWith(const QUndoCommand * command);
    virtual void undo();
    virtual void redo();

private:
    CustomTrackView *m_view;
    int m_track;
    QDomElement m_effect;
    QDomElement m_oldeffect;
    GenTime m_pos;
    bool m_doIt;
};

#endif

