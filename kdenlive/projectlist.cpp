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
#include "kdenlivedoc.h"

/* This define really should go in projectlist_ui, but qt just puts the classname there at the moment :-( */
#include <qpushbutton.h>

#include <qcursor.h>

#include <kdebug.h>
#include <klocale.h>
#include <kiconloader.h>
#include <kfiledialog.h>
#include <kmessagebox.h>

#include <iostream>
#include <string>
#include <map>

ProjectList::ProjectList(KdenliveDoc *document, QWidget *parent, const char *name) :
						ProjectList_UI(parent,name),
						m_menu()
{
	m_document = document;
	connect (m_addButton, SIGNAL(clicked()), this, SLOT(slot_AddFile()));
	connect (m_cleanButton, SIGNAL(clicked()), this, SLOT(slot_cleanProject()));	

 	connect (m_listView, SIGNAL(dragDropOccured(QDropEvent *)), this, SIGNAL(dragDropOccured(QDropEvent *)));
		
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
	QString filter = "*";

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
void ProjectList::slot_UpdateList() {
	m_listView->clear();

	QPtrListIterator<AVFile> itt(m_document->avFileList());

	AVFile *av;
	
	for(; (av = itt.current()); ++itt) {
		new AVListViewItem(m_listView, av);
	}
}

/** Removes any AVFiles from the project that have a usage count of 0. */
void ProjectList::slot_cleanProject()
{
	if(KMessageBox::warningContinueCancel(this,
			 i18n("Clean Project removes files from the project that are unused.\
			 Are you sure you want to do this?")) == KMessageBox::Continue) {
		m_document->cleanAVFileList();
	}
}