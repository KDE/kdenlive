/***************************************************************************
                          avformatdesclist.cpp  -  description
                             -------------------
    begin                : Thu Jan 23 2003
    copyright            : (C) 2003 by Jason Wood
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
 
#include "avformatdesclist.h"
#include "avformatwidgetlist.h"

AVFormatDescList::AVFormatDescList(const QString &description, const QString &name) :
                                          AVFormatDescBase(description, name)
{
}

AVFormatDescList::~AVFormatDescList()
{
}

QWidget *AVFormatDescList::createWidget(QWidget *parent)
{
  return new AVFormatWidgetList(this, parent, m_name);  
}

/** Adds the specified string to the item list. */
void AVFormatDescList::addItem(const QString &item)
{
  m_itemList.append(item);
}

/** Returns the item list. */
QValueVector<QString> AVFormatDescList::itemList()
{
  return m_itemList;
}
