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

#include <qlistview.h>

class AVFile;

/**Allows clips to be displayed in a QListView
  *@author Jason Wood
  */

class AVListViewItem : public QListViewItem  {
private:
	QListView *m_listView;
	AVFile *m_clip;
public: 
	AVListViewItem(QListView *parent, AVFile *clip);
	~AVListViewItem();
	QString text ( int column ) const;
	AVFile *clip() const;
};

#endif
