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

#include <klocale.h>
#include <kiconloader.h>
#include <kfiledialog.h>

#include "arts/core.h"

#include <iostream>
#include <map>

ProjectList::ProjectList(QWidget *parent, const char *name ) :
									QVBox(parent,name),
									listView(this, name, 0),
									m_menu()
{
	listView.addColumn("Filename", -1);
	listView.addColumn("Type", -1);
	listView.addColumn("Duration", -1);
	listView.addColumn("Usage count", -1);	
	listView.addColumn("Size", -1);	
		
	init_menu();
}

ProjectList::~ProjectList(){
}

void ProjectList::init_menu(){
	m_menu.insertItem(i18n("&Add File..."),	this, SLOT(slot_AddFile()), 0);
	
	connect(&listView, SIGNAL(rightButtonPressed ( QListViewItem *, const QPoint &, int )),
					this, SLOT(rightButtonPressed ( QListViewItem *, const QPoint &, int )));
}

void ProjectList::slot_AddFile() {
	// determine file types supported by Arts
	std::map<std::string, bool> done;	
		
	Arts::TraderQuery query;
	
  query.supports("Interface", "Arts::PlayObject");
	std::vector<Arts::TraderOffer> *results = query.query();	

	for(std::vector<Arts::TraderOffer>::iterator i = results->begin(); i != results->end(); i++)
	{
		std::vector<string> *ext = (*i).getProperty("Extension");
				
		for(std::vector<string>::iterator it = ext->begin(); it != ext->end(); it++) {
			if(!(*it).length() || done[*it]) continue;

  		done[*it] = true;
			cout << *it << endl;
		}
		
		delete ext;		
	}
	
	delete results;	
		
	KURL url=KFileDialog::getOpenURL(QString::null,
        i18n("*|All files"), this, i18n("Open File..."));
	if(!url.isEmpty())
	{
  	emit signal_AddFile(url);
	}
}

/** No descriptions */
void ProjectList::rightButtonPressed ( QListViewItem *listViewItem, const QPoint &pos, int column) {
	m_menu.popup(QCursor::pos());		
}
