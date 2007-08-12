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

#include "folderlistviewitem.h"


FolderListViewItem::FolderListViewItem(QListViewItem * parent, QString folderName):
BaseListViewItem(parent, BaseListViewItem::FOLDER)
{
    setText(1, folderName);
    setText(2, "FOLDER Item");
    setPixmap(0, QPixmap(KGlobal::iconLoader()->loadIcon("folder", KIcon::Toolbar)));
}

FolderListViewItem::FolderListViewItem(QListView * parent, QString folderName):
BaseListViewItem(parent, BaseListViewItem::FOLDER)
{
    setText(1, folderName);
    setText(2, "FOLDER Item");
    setPixmap(0, QPixmap(KGlobal::iconLoader()->loadIcon("folder", KIcon::Toolbar)));
}

FolderListViewItem::~FolderListViewItem()
{
}

QString FolderListViewItem::getInfo() const
{
    QString text = "<b>"+i18n("Folder")+"</b><br>";
    text.append(i18n("%1 clips").arg(childCount()));
    return text;
}
