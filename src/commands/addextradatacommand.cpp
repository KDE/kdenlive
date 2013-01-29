/***************************************************************************
                          addextradatacommand.cpp  -  description
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


#include "addextradatacommand.h"
#include "customtrackview.h"

#include <KLocale>

AddExtraDataCommand::AddExtraDataCommand(CustomTrackView *view, const QString&id, const QString&key, const QString &oldData, const QString &newData, QUndoCommand * parent) :
        QUndoCommand(parent),
        m_view(view),
        m_oldData(oldData),
        m_newData(newData),
        m_key(key),
        m_id(id)
{
    if (m_newData.isEmpty()) setText(i18n("Delete data"));
    else setText(i18n("Add data"));
}


// virtual
void AddExtraDataCommand::undo()
{
    m_view->addData(m_id, m_key, m_oldData);
}
// virtual
void AddExtraDataCommand::redo()
{
    m_view->addData(m_id, m_key, m_newData);
}

