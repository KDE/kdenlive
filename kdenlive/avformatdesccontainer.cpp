/***************************************************************************
                          avformatdesccontainer.cpp  -  description
                             -------------------
    begin                : Fri Jan 24 2003
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

#include "avformatdesccontainer.h"
#include "avformatwidgetcontainer.h"

#include <kdebug.h>

AVFormatDescContainer::AVFormatDescContainer(const QString &description, const QString &name) :
                            AVFormatDescBase(description, name)
{
  m_descList.setAutoDelete(true);
}

AVFormatDescContainer::~AVFormatDescContainer()
{
}

/** Constructs a widget to display this container. Most likely, a qgroupbox. */
AVFormatWidgetBase * AVFormatDescContainer::createWidget(QWidget *parent)
{
  return new AVFormatWidgetContainer(this, parent, m_name);
}

/** Appends a new description element into this container. */
void AVFormatDescContainer::append(AVFormatDescBase *elem)
{
  m_descList.append(elem);
}

/** Returns the format list. */
const QPtrList<AVFormatDescBase> &AVFormatDescContainer::list()
{
  return m_descList;
}
