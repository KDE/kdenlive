/***************************************************************************
                          clipmanager.cpp  -  description
                             -------------------
    begin                : Wed Sep 17 08:36:16 GMT 2003
    copyright            : (C) 2003 by Jason Wood
    email                : jasonwood@blueyonder.co.uk
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "documentgroupnode.h"

// include files for KDE
#include <klocale.h>
#include <kdebug.h>

DocumentGroupNode::DocumentGroupNode(DocumentBaseNode * parent, const QString & groupName):
DocumentBaseNode(parent), m_name(groupName)
{
}

DocumentGroupNode::~DocumentGroupNode()
{
}

// virtual 
const QString & DocumentGroupNode::name() const
{
    return m_name;
}
