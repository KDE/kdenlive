/***************************************************************************
                          avformatdesccodec.cpp  -  description
                             -------------------
    begin                : Tue Feb 4 2003
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

#include "avformatdesccodec.h"
#include "avformatwidgetcodec.h"

AVFormatDescCodec::AVFormatDescCodec(const QString &description, const QString &name) :
                      AVFormatDescBase(description, name)
{
}

AVFormatDescCodec::~AVFormatDescCodec()
{
}

QWidget * AVFormatDescCodec::createWidget(QWidget * parent)
{
  return new AVFormatWidgetCodec(this, parent, m_name);
}
