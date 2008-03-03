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


#ifndef TIMELINECLIPCOMMAND_H
#define TIMELINECLIPCOMMAND_H

#include <QUndoCommand>
#include <QGraphicsView>
#include <QPointF>

#include <KDebug>

#include "projectlist.h"
#include "customtrackview.h"

class AddTimelineClipCommand : public QUndoCommand {
public:
    AddTimelineClipCommand(CustomTrackView *view, QDomElement xml, int clipId, int track, int startpos, QRectF rect, int duration, bool doIt, bool doRemove);
    virtual void undo();
    virtual void redo();

private:
    CustomTrackView *m_view;
    int m_clipDuration;
    int m_clipId;
    QDomElement m_xml;
    int m_clipTrack;
    int m_clipPos;
    QRectF m_clipRect;
    bool m_doIt;
    bool m_remove;
};

#endif

