/***************************************************************************
                          avformatwidgetbase.cpp  -  description
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

#include "avformatwidgetbase.h"

AVFormatWidgetBase::AVFormatWidgetBase()
{
}

AVFormatWidgetBase::~AVFormatWidgetBase()
{
}

/** Returns a pointer to this widget cast as an AVFileFormatWidget if that is what this widget it, otherwise it returns 0. */
AVFileFormatWidget *AVFormatWidgetBase::fileFormatWidget()
{
    return 0;
}
