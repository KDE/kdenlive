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

#ifndef GUIDECOMMAND_H
#define GUIDECOMMAND_H

#include <QUndoCommand>
#include <QGraphicsView>
#include <QPointF>
#include <QDomElement>
#include <KDebug>

#include "gentime.h"
#include "definitions.h"
class CustomTrackView;


class EditGuideCommand : public QUndoCommand {
public:
    EditGuideCommand(CustomTrackView *view, const GenTime oldPos, const QString &oldcomment, const GenTime pos, const QString &comment, bool doIt);
    virtual void undo();
    virtual void redo();

private:
    CustomTrackView *m_view;
    QString m_oldcomment;
    QString m_comment;
    GenTime m_oldPos;
    GenTime m_pos;
    bool m_doIt;
};

#endif

