/***************************************************************************
                          projectlist.h  -  description
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

#ifndef PROJECTLIST_H
#define PROJECTLIST_H

#include <qwidget.h>
#include <qvbox.h>
#include <qlistview.h>

/**
  *@author Jason Wood
  */

class ProjectList : public QVBox  {
   Q_OBJECT
public: 
	ProjectList(QWidget *parent=0, const char *name=0);
	~ProjectList();
private:
	QListView	listView;
};

#endif
