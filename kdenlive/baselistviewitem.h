/***************************************************************************
                          baselistviewitem.h  -  description
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

#ifndef BASELISTVIEWITEM_H
#define BASELISTVIEWITEM_H

#include <klistview.h>


/** Base class for our list view
  *@author Jean-Baptiste Mardelle
  */

class BaseListViewItem:public KListViewItem {

  public:


    enum ITEMTYPE { CLIP = 0, PLAYLISTITEM = 1, FOLDER = 2};

    BaseListViewItem(QListViewItem * parent, ITEMTYPE type);
    BaseListViewItem(QListView * parent, ITEMTYPE type);
    ~BaseListViewItem();

    BaseListViewItem::ITEMTYPE getType() const;

    virtual QString getInfo() const = 0;
    virtual QString key ( int , bool ) const;


  private:
    ITEMTYPE m_type;
};

#endif
