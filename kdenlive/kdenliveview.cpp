/***************************************************************************
                          kdenliveview.cpp  -  description
                             -------------------
    begin                : Fri Feb 15 01:46:16 GMT 2002
    copyright            : (C) 2002 by Jason Wood
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

// include files for Qt
#include <qprinter.h>
#include <qpainter.h>
#include <qpixmap.h>

#include <klocale.h>

#include <kdockwidget.h>

// application specific includes
#include "kdenliveview.h"
#include "kdenlivedoc.h"
#include "kdenlive.h"

KdenliveView::KdenliveView(QWidget *parent, const char *name) :
                      QWidget(parent, name)
{

}

KdenliveView::~KdenliveView()
{
}

