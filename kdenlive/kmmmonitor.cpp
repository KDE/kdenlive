/***************************************************************************
                          kmmmonitor.cpp  -  description
                             -------------------
    begin                : Sun Mar 24 2002
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

#include "kmmmonitor.h"

#include <kdebug.h>

#include "qcolor.h"

#include "kdenlivedoc.h"
#include "docclipbase.h"
#include "docclipavfile.h"
#include "avfile.h"
#include "clipdrag.h"


KMMMonitor::KMMMonitor(KdenliveApp *app, KdenliveDoc *document, QWidget *parent, const char *name ) :
						QVBox(parent,name),
						m_document(document),
						m_screenHolder(new QVBox(this, name)),
						m_screen(new KMMScreen(app, m_screenHolder, name)),
						m_editPanel(new KMMEditPanel(document, this, name)),
						m_clip(0),
						m_noSeek(false)
{
	connect(m_editPanel, SIGNAL(seekPositionChanged(const GenTime &)), 
					this, SIGNAL(seekPositionChanged(const GenTime &)));
	connect(m_editPanel, SIGNAL(inpointPositionChanged(const GenTime &)), 
					this, SIGNAL(inpointPositionChanged(const GenTime &)));
	connect(m_editPanel, SIGNAL(outpointPositionChanged(const GenTime &)), 
					this, SIGNAL(outpointPositionChanged(const GenTime &)));

	m_screenHolder->setMargin(3);
	m_screenHolder->setPaletteBackgroundColor(QColor(0, 0, 0));
	
	connectScreen();
}

KMMMonitor::~KMMMonitor()
{
  if(m_editPanel) delete m_editPanel;
  if(m_screen) delete m_screen;
  if(m_screenHolder) delete m_screenHolder;
}

void KMMMonitor::seek(const GenTime &time)
{
  m_editPanel->seek(time);
}

void KMMMonitor::screenPositionChanged(const GenTime &time)
{
	disconnect(m_editPanel, SIGNAL(seekPositionChanged(const GenTime &)), 
					m_screen, SLOT(seek(const GenTime &)));    
	m_editPanel->seek(time);
	connect(m_editPanel, SIGNAL(seekPositionChanged(const GenTime &)), 
					m_screen, SLOT(seek(const GenTime &)));  
}

void KMMMonitor::swapScreens(KMMMonitor *monitor)
{
	disconnectScreen();
	monitor->disconnectScreen();

	std::swap(monitor->m_screen, m_screen);

	monitor->m_screen->reparent(monitor->m_screenHolder, 0, QPoint(0,0), true);
	m_screen->reparent(m_screenHolder, 0, QPoint(0,0), true);
	connectScreen();
	monitor->connectScreen();
}

void KMMMonitor::disconnectScreen()
{
	disconnect(m_editPanel, SIGNAL(seekPositionChanged(const GenTime &)), m_screen, SLOT(seek(const GenTime &)));
	disconnect(m_editPanel, SIGNAL(playSpeedChanged(double)), m_screen, SLOT(play(double)));
	disconnect(m_screen, SIGNAL(rendererConnected()), m_editPanel, SLOT(rendererConnected()));
	disconnect(m_screen, SIGNAL(rendererDisconnected()), m_editPanel, SLOT(rendererDisconnected()));
	disconnect(m_screen, SIGNAL(seekPositionChanged(const GenTime &)), this, SLOT(screenPositionChanged(const GenTime &)));  
	disconnect(m_screen, SIGNAL(mouseClicked()), this, SLOT(slotClickMonitor()));
	disconnect(m_screen, SIGNAL(mouseDragged()), this, SLOT(slotStartDrag()));
}

void KMMMonitor::connectScreen()
{
	connect(m_editPanel, SIGNAL(seekPositionChanged(const GenTime &)), m_screen, SLOT(seek(const GenTime &)));
	connect(m_editPanel, SIGNAL(playSpeedChanged(double)), m_screen, SLOT(play(double)));
	connect(m_screen, SIGNAL(rendererConnected()), m_editPanel, SLOT(rendererConnected()));
	connect(m_screen, SIGNAL(rendererDisconnected()), m_editPanel, SLOT(rendererDisconnected()));
	connect(m_screen, SIGNAL(seekPositionChanged(const GenTime &)), this, SLOT(screenPositionChanged(const GenTime &)));  
	connect(m_screen, SIGNAL(mouseClicked()), this, SLOT(slotClickMonitor()));
	connect(m_screen, SIGNAL(mouseDragged()), this, SLOT(slotStartDrag()));
}

void KMMMonitor::setSceneList(const QDomDocument &scenelist)
{
  m_screen->setSceneList(scenelist);
}

void KMMMonitor::togglePlay()
{
	if(m_screen->playSpeed() == 0.0) {
		m_screen->play(1.0);
	} else {
		m_screen->play(0.0);
	}
}

const GenTime &KMMMonitor::seekPosition() const
{
	return m_screen->seekPosition();
}

void KMMMonitor::slotSetInpoint()
{
	m_editPanel->setInpoint();
}

void KMMMonitor::slotSetInpoint(const GenTime &inpoint)
{
	m_editPanel->setInpoint(inpoint);
}

void KMMMonitor::slotSetOutpoint()
{
	m_editPanel->setOutpoint();
}

void KMMMonitor::slotSetOutpoint(const GenTime &outpoint)
{
	m_editPanel->setOutpoint(outpoint);
}

void KMMMonitor::slotSetActive()
{
	m_screenHolder->setPaletteBackgroundColor(QColor(0, 255, 0));
}

void KMMMonitor::slotSetInactive()
{
	m_screenHolder->setPaletteBackgroundColor(QColor(0, 0, 0));
}

void KMMMonitor::mousePressEvent(QMouseEvent *e)
{
	emit monitorClicked(this);	
}

void KMMMonitor::slotClickMonitor()
{
	emit monitorClicked(this);
}

void KMMMonitor::slotSetClip(AVFile *file)
{
	static QString str_inpoint="inpoint";
	static QString str_outpoint="outpoint";
	static QString str_file="file";

	kdDebug() << "Setting clip monitor source" << endl;

	if(!file->durationKnown()) {
		kdWarning() << "Cannot create scenelist for clip monitor - clip duration unknown" << endl;
		return;
	}
	DocClipBase *clip = new DocClipAVFile(m_document, file);
	slotSetClip(clip);
	delete clip;
}

void KMMMonitor::slotSetClip(DocClipBase *clip)
{
	if(!clip) {
		kdError() << "Null clip passed, not setting monitor." << endl;
		return;
	}

	if(m_clip != 0) {
		delete m_clip;
		m_clip = 0;
	}

	// create a copy of the clip.
	m_clip = DocClipBase::createClip(m_document, clip->toXML().documentElement());

	if(!m_clip) {
		kdError() << "KMMMonitor : Could not copy clip - drag 'n' drop will not work!" << endl;
	}

	QDomDocument scenelist = clip->generateSceneList();
	setSceneList(scenelist);
	m_screen->setClipLength(clip->duration().frames(m_document->framesPerSecond()));
	m_editPanel->setClipLength(clip->duration().frames(m_document->framesPerSecond()));

	m_editPanel->setInpoint(clip->cropStartTime());
	m_editPanel->setOutpoint(clip->cropStartTime() + clip->cropDuration());

	if( (!m_noSeek) || 
	    (seekPosition() < clip->cropStartTime()) || 
	    (seekPosition() > clip->cropStartTime() + clip->cropDuration())) {
		seek(GenTime(clip->cropStartTime()));
	}
}

void KMMMonitor::slotStartDrag()
{
	if(!m_clip)
	{
		kdWarning() << "KMMMonitor cannot start drag - m_clip is null!" << endl;
		return;
	}
	
	m_clip->setCropStartTime(GenTime(m_editPanel->inpoint()));
	m_clip->setTrackStart(GenTime(0));
	m_clip->setTrackEnd(m_editPanel->outpoint() - m_editPanel->inpoint());

	kdWarning() << "Track Start is " << m_clip->trackStart().seconds() << endl;
	kdWarning() << "Track End is " << m_clip->trackEnd().seconds() << endl;
	kdWarning() << "Crop Start is " << m_clip->cropStartTime().seconds() << endl;
	kdWarning() << "Crop duration is " << m_clip->cropDuration().seconds() << endl;

	ClipDrag *drag = new ClipDrag(m_clip, this, "Clip from monitor");
	drag->dragCopy();
}

void KMMMonitor::setNoSeek(bool noSeek)
{
	m_noSeek = noSeek;
}
