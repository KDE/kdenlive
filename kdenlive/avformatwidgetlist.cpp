/***************************************************************************
                          avformatwidgetlist.cpp  -  description
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

#include "avformatwidgetlist.h"
#include "avformatdesclist.h"

#include <qlabel.h>
#include <qcombobox.h>
#include <qvaluevector.h>

AVFormatWidgetList::AVFormatWidgetList(AVFormatDescList *desc, QWidget *parent, const char *name ) :
                                            QHBox(parent, name),
                                            AVFormatWidgetBase(),                                            
                                            m_label(new QLabel(desc->name(), this, name)),
                                            m_comboBox(new QComboBox(this,name))
{
  const QValueVector<QString> &list = desc->itemList();

  for(unsigned int count=0; count<list.size(); ++count) {
    m_comboBox->insertItem(list[count]);
  }
}

AVFormatWidgetList::~AVFormatWidgetList()
{
}

QWidget *AVFormatWidgetList::widget()
{
  return this;
}
