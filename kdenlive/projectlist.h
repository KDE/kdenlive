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

#include "projectlist_ui.h"

#include <qlist.h>
#include <qpopupmenu.h>
#include <qevent.h>

#include <kurl.h>

#include <avfile.h>

/**
  *@author Jason Wood
  */

class ProjectList : public ProjectList_UI  {
   Q_OBJECT
public: 
	ProjectList(QWidget *parent=0, const char *name=0);
	~ProjectList();
private: // Private methods
  /** Initialise the popup menu */
  void init_menu();
	/** The popup menu */	
	QPopupMenu m_menu;	
public slots: // Public slots
  /** User is requesting to open a file. Open file dialog and let the user pick. */
  void slot_AddFile();
  /** No descriptions */
  void rightButtonPressed ( QListViewItem *listViewItem, const QPoint &pos, int column) ;
  /** Get a fresh copy of files from KdenliveDoc and display them. */
  void slot_UpdateList(QList<AVFile> list);
signals: // Signals
  /** emitted whenever a file is added to the project list */
  void signal_AddFile(const KURL &url);
};

#endif
