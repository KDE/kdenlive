/***************************************************************************
                          avlistviewitem.h  -  description
                             -------------------
    begin                : Wed Mar 20 2002
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

#ifndef AVLISTVIEWITEM_H
#define AVLISTVIEWITEM_H

#include <qwidget.h>
#include <qlistview.h>

#include <avfile.h>

/**Allows AVFiles to be displayed in a QListView
  *@author Jason Wood
  */

class AVListViewItem : public QListViewItem  {
//   Q_OBJECT
private:
	QListView *m_listView;
	AVFile *m_avFile;
public: 
	AVListViewItem(QListView *parent, AVFile *file);
	~AVListViewItem();
	QString text ( int column ) const;
};

#endif
