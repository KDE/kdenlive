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
				m_topSplitter(Horizontal, this, name),
				m_projectList(&m_topSplitter, name),
				m_monitor(&m_topSplitter, name),
				m_timeline(getDocument(), this, name)
{
  setBackgroundMode(PaletteBase);

  connect(&m_projectList, SIGNAL(signal_AddFile(const KURL &)), getDocument(), SLOT(slot_InsertAVFile(const KURL &)));
  connect(&m_projectList, SIGNAL(clipListDropped(QPtrList<DocClipBase>)), getDocument(), SLOT(slot_insertClips(QPtrList<DocClipBase>)));

  connect(getDocument(), SIGNAL(avFileListUpdated(QPtrList<AVFile>)), &m_projectList, SLOT(slot_UpdateList(QPtrList<AVFile>)));
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
