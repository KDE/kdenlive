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
#include "avlistviewitem.h"

/* This define really should go in projectlist_ui, but qt just puts the classname there at the moment :-( */
#include <qpushbutton.h>

#include <qcursor.h>

#include <klocale.h>
#include <kiconloader.h>
#include <kfiledialog.h>

#include "arts/core.h"

#include <iostream>
#include <string>
#include <map>

ProjectList::ProjectList(QWidget *parent, const char *name ) :
						ProjectList_UI(parent,name),
						m_menu()
{
	connect (m_addButton, SIGNAL(clicked()), this, SLOT(slot_AddFile()));

 	connect (m_listView, SIGNAL(clipListDropped(QPtrList<DocClipBase>)), this, SIGNAL(clipListDropped(QPtrList<DocClipBase>)));
		
	init_menu();
}

ProjectList::~ProjectList(){
}

void ProjectList::init_menu(){
	m_menu.insertItem(i18n("&Add File..."),	this, SLOT(slot_AddFile()), 0);
	
	connect(m_listView, SIGNAL(rightButtonPressed ( QListViewItem *, const QPoint &, int )),
					this, SLOT(rightButtonPressed ( QListViewItem *, const QPoint &, int )));
}

void ProjectList::slot_AddFile() {
	// determine file types supported by Arts
	QString filter = "";
	
	std::map<std::string, bool> done;	
		
	Arts::TraderQuery query;

	query.supports("Interface", "Arts::PlayObject");
	std::vector<Arts::TraderOffer> *results = query.query();	

	for(std::vector<Arts::TraderOffer>::iterator i = results->begin(); i != results->end(); i++)
	{			
		std::vector<std::string> *mime = (*i).getProperty("MimeType");
		std::vector<std::string> *ext = (*i).getProperty("Extension");

		std::vector<std::string>::iterator extIt = ext->begin();					
		std::vector<std::string>::iterator mimeIt = mime->begin();				
		
		while((extIt != ext->end()) && (mimeIt != mime->end())) {			
			if( ((*extIt).length()) && (!done[*extIt]) ) {
				if((*mimeIt).find("audio/", 0) == 0) {
			  		done[*extIt] = true;
					filter += (*mimeIt).c_str();
					filter += " ";
				}
			}			
					
			extIt++;
			mimeIt++;
		}
		
		delete ext;		
	}	
	
	delete results;	
				
	KURL::List urlList=KFileDialog::getOpenURLs(	QString::null,
							filter,
							this,
							i18n("Open File..."));
		
	KURL::List::Iterator it;
	KURL url;
	
	for(it = urlList.begin(); it != urlList.end(); it++) {
		url =  (*it);
		if(!url.isEmpty()) {
		  	emit signal_AddFile(url);
		}
	}	
}

/** No descriptions */
void ProjectList::rightButtonPressed ( QListViewItem *listViewItem, const QPoint &pos, int column) {
	m_menu.popup(QCursor::pos());		
}

/** Get a fresh copy of files from KdenliveDoc and display them. */
void ProjectList::slot_UpdateList(QPtrList<AVFile> list) {
	m_listView->clear();

	QListIterator<AVFile> itt(list);
	AVFile *av;
	
	for(; (av = itt.current()); ++itt) {
		new AVListViewItem(m_listView, av);
	}
}
