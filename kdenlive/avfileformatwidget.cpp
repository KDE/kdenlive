/***************************************************************************
                          avfileformatwidget.cpp  -  description
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

#include <qptrlist.h>

#include <kdebug.h>
#include <klocale.h>
#include <kurlrequester.h>
#include <qlabel.h>
#include <qhbox.h>

#include "avfileformatdesc.h"
#include "avfileformatwidget.h"

AVFileFormatWidget::AVFileFormatWidget(AVFileFormatDesc *desc, QWidget *parent, const char *name ) :
                                            QVBox(parent,name),
                                            m_fileHBox(new QHBox(this, "file_hbox")),
                                            m_fileLabel(new QLabel(i18n("Filename"), m_fileHBox, "file_label")),
                                            m_filename(new KURLRequester(m_fileHBox, "browser")),
                                            m_desc(desc)
{
  QPtrListIterator<AVFormatDescBase> itt(desc->list());

  while(itt.current()) {
    itt.current()->createWidget(this);
    ++itt;
  }
}

AVFileFormatWidget::~AVFileFormatWidget()
{
}

QWidget *AVFileFormatWidget::widget()
{
  return this;
}

KURL AVFileFormatWidget::fileUrl() const
{
  return m_filename->url();
}

AVFileFormatWidget * AVFileFormatWidget::fileFormatWidget()
{
  return this;
}
