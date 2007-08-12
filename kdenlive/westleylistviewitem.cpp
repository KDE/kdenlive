/***************************************************************************
                          avlistviewitem.cpp  -  description
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

#include <qpixmap.h>

#include <kglobal.h>
#include <kiconloader.h>
#include <klocale.h>

#include "westleylistviewitem.h"


WestleyListViewItem::WestleyListViewItem(QListViewItem * parent, QString itemName):
BaseListViewItem(parent, BaseListViewItem::PLAYLISTITEM)
{
    setText(1, itemName);
}

WestleyListViewItem::WestleyListViewItem(QListView * parent, QString itemName):
BaseListViewItem(parent, BaseListViewItem::PLAYLISTITEM)
{
    setText(1, itemName);
}

WestleyListViewItem::~WestleyListViewItem()
{
}

QString WestleyListViewItem::getInfo() const
{
    QString text = "<b>"+i18n("Playlist Item")+"</b><br>";
    return text;
}
