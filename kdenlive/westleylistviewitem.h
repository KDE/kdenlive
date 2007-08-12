/***************************************************************************
                          folderlistviewitem.h  -  description
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

#ifndef WESTLEYLISTVIEWITEM_H
#define WESTLEYLISTVIEWITEM_H

#include <klistview.h>
#include "baselistviewitem.h"


/** Allows folders to be displayed in the project view
  *@author Jean-Baptiste Mardelle
  */

class WestleyListViewItem:public BaseListViewItem {
  public:
    WestleyListViewItem(QListViewItem * parent, QString folderName);
    WestleyListViewItem(QListView * parent, QString folderName);
    ~WestleyListViewItem();

    virtual QString getInfo() const;

};

#endif
