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
				m_projectList(getDocument(), &m_topSplitter, name),
				m_monitor(getDocument(), &m_topSplitter, name),				
				m_rulerPanel(new KMMRulerPanel(NULL, "Ruler Panel")),
				m_timeline(m_rulerPanel, NULL, getDocument(), this, name)
{
  setBackgroundMode(PaletteBase);

  connect(m_rulerPanel, SIGNAL(timeScaleChanged(int)), &m_timeline, SLOT(setTimeScale(int)));

  connect(&m_projectList, SIGNAL(signal_AddFile(const KURL &)), getDocument(), SLOT(slot_InsertAVFile(const KURL &)));
  connect(&m_projectList, SIGNAL(dragDropOccured(QDropEvent *)), getDocument(), SLOT(slot_insertClips(QDropEvent *)));

  connect(&m_timeline, SIGNAL(projectLengthChanged(int)), &m_monitor, SLOT(setClipLength(int)));

  connect(getDocument(), SIGNAL(avFileListUpdated()), &m_projectList, SLOT(slot_UpdateList()));
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
