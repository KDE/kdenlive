/***************************************************************************
                          avfileformatdesc.cpp  -  description
                             -------------------
    begin                : Tue Jan 21 2003
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

#include <qwidget.h>
 
#include "avfileformatdesc.h"
#include "avfileformatwidget.h"

AVFileFormatDesc::AVFileFormatDesc(const QString &description, const QString &name) :
                            AVFormatDescContainer(description, name),
                            m_fileExtension(QString::null)
{
}

AVFileFormatDesc::~AVFileFormatDesc()
{
}

/** Create and return a widget that embodies this file format description. */
QWidget * AVFileFormatDesc::createWidget(QWidget *parent)
{
  AVFileFormatWidget *widget = new AVFileFormatWidget(this, parent, m_name);
  return widget;  
}

/** Sets the file extenstion for this description. */
void AVFileFormatDesc::setFileExtension(QString extension)
{
  m_fileExtension = extension;
}

/** Returns the file extension for this file format. */
const QString &AVFileFormatDesc::fileExtension()
{
  return m_fileExtension;
}
