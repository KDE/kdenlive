/***************************************************************************
                          projectlistview.cpp  -  description
                             -------------------
    begin                : Sun Jun 30 2002
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

#include "projectlistview.h"

#include <avlistviewitem.h>
#include <clipdrag.h>

#include <klocale.h>

ProjectListView::ProjectListView(QWidget *parent, const char *name):
								KListView(parent, name)
{
    addColumn( i18n( "Filename" ) );
    addColumn( i18n( "Type" ) );
    addColumn( i18n( "Duration" ) );
    addColumn( i18n( "Usage Count" ) );
    addColumn( i18n( "Size" ) );

    setDragEnabled(true);
    setAcceptDrops(true);
    setDropVisualizer(true);

    connect(this, SIGNAL(dropped(QDropEvent*, QListViewItem*, QListViewItem*)), this,
    						  SLOT(dragDropped(QDropEvent*, QListViewItem*, QListViewItem*)));
}

ProjectListView::~ProjectListView()
{
}

/** returns a drag object which is used for drag operations. */
QDragObject *ProjectListView::dragObject()
{
	AVListViewItem *item = (AVListViewItem *)selectedItem();

	return new ClipDrag(item->clip(), parentWidget(), "drag object");
}

bool ProjectListView::acceptDrag (QDropEvent* event) const
{
	return ClipDrag::canDecode(event);
}

/** This slot function should be called whenever a drag has been dropped
	onto the class. The cliplist produced will delete the clips within it
 	unless the signal is caught and told otherwise. Note that problems
  	will arise is multiple documents attempt to catch this signal and have
	different ideas about what to do with the clip list.*/
void ProjectListView::dragDropped(QDropEvent* e, QListViewItem* parent, QListViewItem* after)
{
	QPtrList<DocClipBase> cliplist = ClipDrag::decode(e);
 	cliplist.setAutoDelete(false);

	emit clipListDropped(cliplist);
}
