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
#include "docclipavfile.h"

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
						ProjectList_UI(parent,name),
						m_document(document),
						m_app(app)
{
	if(!document) {
		kdError() << "ProjectList created with no document - expect a crash shortly" << endl;
	}

	m_listView->setDocument(document);

 	connect (m_listView, SIGNAL(dragDropOccured(QDropEvent *)), this, SIGNAL(dragDropOccured(QDropEvent *)));

	connect(m_listView, SIGNAL(rightButtonPressed ( QListViewItem *, const QPoint &, int )),
				this, SLOT(rightButtonPressed ( QListViewItem *, const QPoint &, int )));

	connect(m_listView, SIGNAL(executed(QListViewItem *)), this, SLOT(projectListSelectionChanged(QListViewItem *)));
	connect(m_listView, SIGNAL(dragStarted(QListViewItem *)), this, SLOT(projectListSelectionChanged(QListViewItem *)));
}

ProjectList::~ProjectList()
{
}

/** No descriptions */
void ProjectList::rightButtonPressed ( QListViewItem *listViewItem, const QPoint &pos, int column) 
{
	QPopupMenu *menu = contextMenu();
	if(menu) {
		menu->popup(QCursor::pos());
	}
}

/** Get a fresh copy of files from KdenliveDoc and display them. */
void ProjectList::slot_UpdateList() 
{
	m_listView->clear();

	DocumentBaseNode *node = m_document->clipHierarch();

	if(node) {
		new AVListViewItem(m_document, m_listView, node);
	}
}

/** The clip specified has changed - update the display.
 */ 
void ProjectList::slot_clipChanged(DocClipRef *clip)
{
	slot_UpdateList();
	m_listView->triggerUpdate();
}

/** Called when the project list changes. */
void ProjectList::projectListSelectionChanged(QListViewItem *item)
{
  const AVListViewItem *avitem = (AVListViewItem *)item;

  emit clipSelected(avitem->clip());  
}

DocClipRef *ProjectList::currentSelection()
{
	const AVListViewItem *avitem = static_cast<AVListViewItem *>(m_listView->selectedItem());
	if(avitem) {
		return avitem->clip();
	}
	return 0;
}

QPopupMenu *ProjectList::contextMenu()
{
	QPopupMenu *menu = (QPopupMenu *)m_app->factory()->container("projectlist_context", m_app);

	return menu;
}
