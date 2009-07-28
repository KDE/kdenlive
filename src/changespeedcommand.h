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


#ifndef CHANGESPEEDCOMMAND_H
#define CHANGESPEEDCOMMAND_H

#include <QUndoCommand>
#include <QGraphicsView>
#include <KDebug>

#include "gentime.h"
#include "definitions.h"

class CustomTrackView;

class ChangeSpeedCommand : public QUndoCommand
{
public:
    ChangeSpeedCommand(CustomTrackView *view, ItemInfo info, double old_speed, double new_speed, int old_strobe, int new_strobe, const QString &clipId, QUndoCommand * parent = 0);
    virtual void undo();
    virtual void redo();

private:
    CustomTrackView *m_view;
    ItemInfo m_clipInfo;
    QString m_clipId;
    double m_old_speed;
    double m_new_speed;
    int m_old_strobe;
    int m_new_strobe;
};

#endif

