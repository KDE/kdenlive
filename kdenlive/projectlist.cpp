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
#include "kdenlive.h"
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

ProjectList::ProjectList(KdenliveApp *app, KdenliveDoc *document, QWidget *parent, const char *name) :
						ProjectList_UI(parent,name)
{
	if(!document) {
		kdError() << "ProjectList created with no document - expect a crash shortly" << endl;
	}
	
	m_document = document;
	m_listView->setDocument(document);
	
 	connect (m_listView, SIGNAL(dragDropOccured(QDropEvent *)), this, SIGNAL(dragDropOccured(QDropEvent *)));

  m_menu = (QPopupMenu *)app->factory()->container("projectlist_context", app);

	connect(m_listView, SIGNAL(rightButtonPressed ( QListViewItem *, const QPoint &, int )),
					this, SLOT(rightButtonPressed ( QListViewItem *, const QPoint &, int )));

  connect(m_listView, SIGNAL(executed(QListViewItem *)), this, SLOT(projectListSelectionChanged(QListViewItem *)));
}

ProjectList::~ProjectList()
{
  delete m_menu;
}

/** No descriptions */
void ProjectList::rightButtonPressed ( QListViewItem *listViewItem, const QPoint &pos, int column) {
  if(m_menu) {
  	m_menu->popup(QCursor::pos());
  }
}

/** Get a fresh copy of files from KdenliveDoc and display them. */
void ProjectList::slot_UpdateList() {
	m_listView->clear();

	QPtrListIterator<AVFile> itt(m_document->avFileList());

	AVFile *av;
	
	for(; (av = itt.current()); ++itt) {
		new AVListViewItem(m_document, m_listView, av);
	}
}

/** The AVFile specified has changed - update the display.
 */ 
void ProjectList::slot_avFileChanged(AVFile *file)
{
  m_listView->triggerUpdate();
}

/** Called when the project list changes. */
void ProjectList::projectListSelectionChanged(QListViewItem *item)
{
  AVListViewItem *avitem = (AVListViewItem *)item;

  emit AVFileSelected(avitem->clip());  
}
