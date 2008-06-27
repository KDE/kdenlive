/***************************************************************************
                          editkeyframecommand.h  -  description
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

#ifndef KEYFRAMECOMMAND_H
#define KEYFRAMECOMMAND_H

#include <QUndoCommand>
#include <QGraphicsView>
#include <QPointF>
#include <QDomElement>
#include <KDebug>

#include "gentime.h"
#include "definitions.h"
class CustomTrackView;


class EditKeyFrameCommand : public QUndoCommand {
public:
    EditKeyFrameCommand(CustomTrackView *view, const int track, GenTime pos, const int effectIndex, const QString& oldkeyframes, const QString& newkeyframes, bool doIt);
    virtual void undo();
    virtual void redo();

private:
    CustomTrackView *m_view;
    const QString m_oldkfr;
    const QString m_newkfr;
    const int m_track;
    const int m_index;
    const GenTime m_pos;
    bool m_doIt;
};

#endif

