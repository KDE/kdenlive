/***************************************************************************
                          avformatwidgetcontainer.cpp  -  description
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

#include "avformatwidgetcontainer.h"
#include "avformatdesccontainer.h"

#include <kdebug.h>

AVFormatWidgetContainer::AVFormatWidgetContainer(AVFormatDescContainer *desc, QWidget *parent, const char *name) :
                                                    QGroupBox(1, Horizontal, desc->name(), parent,name),
                                                    AVFormatWidgetBase()
{
  QPtrListIterator<AVFormatDescBase> itt(desc->list());

  while(itt.current()) {
    itt.current()->createWidget(this);
    ++itt;
  }
}

AVFormatWidgetContainer::~AVFormatWidgetContainer()
{
}
