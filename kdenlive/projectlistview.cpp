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
