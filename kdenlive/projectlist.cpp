/***************************************************************************
                          projectlist.cpp  -  description
                             -------------------
    begin                : Sat Feb 16 2002
    copyright            : (C) 2002 by Jason Wood
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

#include "projectlist.h"

ProjectList::ProjectList(QWidget *parent, const char *name ) :
									QVBox(parent,name),
									listView(this, name, 0)
{
	listView.addColumn("Filename", -1);
	listView.addColumn("Type", -1);
	listView.addColumn("Duration", -1);
	listView.addColumn("Usage count", -1);	
	listView.addColumn("Size", -1);	
}

ProjectList::~ProjectList(){
}
