/***************************************************************************
                          projectlistview.h  -  description
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

#ifndef PROJECTLISTVIEW_H
#define PROJECTLISTVIEW_H

#include <klistview.h>

/**
  *@author Jason Wood
  */

class ProjectListView : public KListView  {
   Q_OBJECT
public:
  ProjectListView(QWidget *parent=0, const char *name=0);
	~ProjectListView();
  /** returns a drag object which is used for drag operations. */
  QDragObject *dragObject();
};

#endif
