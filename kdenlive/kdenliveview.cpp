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

// application specific includes
#include "kdenliveview.h"
#include "kdenlivedoc.h"
#include "kdenlive.h"

KdenliveView::KdenliveView(QWidget *parent, const char *name) :
				QSplitter(Vertical, parent, name),
				projectList(this, name),
				timeline(this, name)
{
  setBackgroundMode(PaletteBase);
}

KdenliveView::~KdenliveView()
{
}

KdenliveDoc *KdenliveView::getDocument() const
{
  KdenliveApp *theApp=(KdenliveApp *) parentWidget();

  return theApp->getDocument();
}

void KdenliveView::print(QPrinter *pPrinter)
{
  QPainter printpainter;
  printpainter.begin(pPrinter);
	
  // TODO: add your printing code here

  printpainter.end();
}
