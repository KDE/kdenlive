/***************************************************************************
                          avformatwidgetbool.cpp  -  description
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

#include "avformatwidgetbool.h"
#include "avformatdescbool.h"

AVFormatWidgetBool::AVFormatWidgetBool(AVFormatDescBool * desc, QWidget * parent, const char *name):
QCheckBox(desc->name(), parent, name), AVFormatWidgetBase()
{
}

AVFormatWidgetBool::~AVFormatWidgetBool()
{
}

QWidget *AVFormatWidgetBool::widget()
{
    return this;
}
