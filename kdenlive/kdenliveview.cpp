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

#include <klocale.h>

// application specific includes
#include "kdenliveview.h"
#include "kdenlivedoc.h"
#include "kdenlive.h"

KdenliveView::KdenliveView(QWidget *parent, const char *name) :
				QSplitter(Vertical, parent, name),
				m_topSplitter(Horizontal, this, name),
        m_tabWidget(&m_topSplitter, name),
				m_projectList((KdenliveApp *)parentWidget(), getDocument(), &m_tabWidget, name),
				m_renderDebugPanel(&m_tabWidget, name),
				m_monitor((KdenliveApp *) parentWidget(), getDocument(), &m_topSplitter, name),
				m_rulerPanel(new KMMRulerPanel(NULL, "Ruler Panel")),
				m_timeline((KdenliveApp *) parentWidget(), m_rulerPanel, NULL, getDocument(), this, name)
{
  m_tabWidget.setTabPosition(QTabWidget::Bottom);
  m_tabWidget.addTab(&m_projectList, i18n("Project List"));
  m_tabWidget.addTab(&m_renderDebugPanel, i18n("Debug"));  

  setBackgroundMode(PaletteBase);

  connect(m_rulerPanel, SIGNAL(timeScaleChanged(int)), &m_timeline, SLOT(setTimeScale(int)));

  connect(&m_projectList, SIGNAL(dragDropOccured(QDropEvent *)), getDocument(), SLOT(slot_insertClips(QDropEvent *)));
  connect(&m_timeline, SIGNAL(projectLengthChanged(int)), &m_monitor, SLOT(setClipLength(int)));

  connect(&m_timeline, SIGNAL(seekPositionChanged(GenTime)), &m_monitor, SLOT(seek(GenTime)));
  connect(&m_monitor, SIGNAL(seekPositionChanged(GenTime)), &m_timeline, SLOT(seek(GenTime)));  
  
  connect(getDocument(), SIGNAL(avFileListUpdated()), &m_projectList, SLOT(slot_UpdateList()));
  connect(getDocument(), SIGNAL(avFileChanged(AVFile *)), &m_projectList, SLOT(slot_avFileChanged(AVFile *)));  

  KdenliveApp *theApp=(KdenliveApp *)parentWidget();
  connect(&m_monitor, SIGNAL(seekPositionChanged(GenTime)), theApp, SLOT(slotUpdateCurrentTime(GenTime)));
  
  m_timeline.calculateProjectSize();
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
