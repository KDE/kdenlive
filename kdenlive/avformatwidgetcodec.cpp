/***************************************************************************
                          avformatwidgetcodec.cpp  -  description
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

#include <qlabel.h>
 
#include "avformatwidgetcodec.h"
#include "avformatdesccodec.h"

AVFormatWidgetCodec::AVFormatWidgetCodec(AVFormatDescCodec *desc, QWidget *parent, const char *name ) :
                                              QVBox(parent,name),
                                              AVFormatWidgetBase()
{
    new QLabel(desc->description(), this, "label");
}

AVFormatWidgetCodec::~AVFormatWidgetCodec()
{
}
