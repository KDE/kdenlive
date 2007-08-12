/***************************************************************************
                          baselistviewitem.cpp  -  description
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

#include "baselistviewitem.h"

BaseListViewItem::BaseListViewItem(QListViewItem * parent, ITEMTYPE type):
KListViewItem(parent),
m_type(type)
{}

BaseListViewItem::BaseListViewItem(QListView * parent, ITEMTYPE type):
KListViewItem(parent), m_type(type)
{}


BaseListViewItem::ITEMTYPE BaseListViewItem::getType() const
{
    return m_type;
}

BaseListViewItem::~BaseListViewItem()
{
}

