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

#include <qcolor.h>
#include <qcursor.h>
#include <qpopupmenu.h>

#include "kdenlive.h"
#include "kdenlivedoc.h"
#include "docclipbase.h"
#include "docclipavfile.h"
#include "avfile.h"
#include "clipdrag.h"

#include "kaddmarkercommand.h"

#include <algorithm>

KMMMonitor::KMMMonitor(KdenliveApp *app, KdenliveDoc *document, QWidget *parent, const char *name ) :
						QVBox(parent,name),
						m_app(app),
						m_document(document),
						m_screenHolder(new QVBox(this, name)),
						m_screen(new KMMScreen(app, m_screenHolder, name)),
						m_editPanel(new KMMEditPanel(document, this, name)),
						m_noSeek(false),
						m_clip(0),
						m_referredClip(0)
{
	connect(m_editPanel, SIGNAL(seekPositionChanged(const GenTime &)),
					this, SIGNAL(seekPositionChanged(const GenTime &)));
	connect(m_editPanel, SIGNAL(seekPositionChanged(const GenTime &)),
					this, SLOT(updateEditPanel(const GenTime &)));
	connect(m_editPanel, SIGNAL(inpointPositionChanged(const GenTime &)),
					this, SIGNAL(inpointPositionChanged(const GenTime &)));
	connect(m_editPanel, SIGNAL(outpointPositionChanged(const GenTime &)),
					this, SIGNAL(outpointPositionChanged(const GenTime &)));

	connect(m_editPanel, SIGNAL(toggleSnapMarker()),
					this, SLOT(slotToggleSnapMarker()));
	connect(m_editPanel, SIGNAL(nextSnapMarkerClicked()),
					this, SLOT(slotNextSnapMarker()));
	connect(m_editPanel, SIGNAL(previousSnapMarkerClicked()),
					this, SLOT(slotPreviousSnapMarker()));

	m_screenHolder->setMargin(3);
	m_screenHolder->setPaletteBackgroundColor(QColor(0, 0, 0));
	m_screenHolder->setSizePolicy(QSizePolicy (QSizePolicy::Expanding, QSizePolicy::Expanding, FALSE));
	//m_editPanel->setSizePolicy(QSizePolicy (QSizePolicy::Expanding, QSizePolicy::Maximum, FALSE));

	connectScreen();
}

KMMMonitor::~KMMMonitor()
{
  if(m_editPanel) delete m_editPanel;
  if(m_screen) delete m_screen;
  if(m_screenHolder) delete m_screenHolder;
  if(m_clip) delete m_clip;
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

	updateEditPanel(time);
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
	disconnect(m_editPanel, SIGNAL(playSpeedChanged(double, const GenTime &)), m_screen, SLOT(play(double, const GenTime &)));
	disconnect(m_editPanel, SIGNAL(playSpeedChanged(double, const GenTime &, const GenTime &)), m_screen, SLOT(play(double, const GenTime &, const GenTime &)));

	disconnect(m_screen, SIGNAL(rendererConnected()), m_editPanel, SLOT(rendererConnected()));
	disconnect(m_screen, SIGNAL(rendererDisconnected()), m_editPanel, SLOT(rendererDisconnected()));
	disconnect(m_screen, SIGNAL(seekPositionChanged(const GenTime &)), this, SLOT(screenPositionChanged(const GenTime &)));
	disconnect(m_screen, SIGNAL(playSpeedChanged(double)), m_editPanel, SLOT(screenPlaySpeedChanged(double)));
	disconnect(m_screen, SIGNAL(mouseClicked()), this, SLOT(slotClickMonitor()));
	disconnect(m_screen, SIGNAL(mouseRightClicked()), this, SLOT(slotRightClickMonitor()));
	disconnect(m_screen, SIGNAL(mouseDragged()), this, SLOT(slotStartDrag()));
}

void KMMMonitor::connectScreen()
{
	connect(m_editPanel, SIGNAL(seekPositionChanged(const GenTime &)), m_screen, SLOT(seek(const GenTime &)));
	connect(m_editPanel, SIGNAL(playSpeedChanged(double)), m_screen, SLOT(play(double)));
	connect(m_editPanel, SIGNAL(playSpeedChanged(double, const GenTime &)), m_screen, SLOT(play(double, const GenTime &)));
	connect(m_editPanel, SIGNAL(playSpeedChanged(double, const GenTime &, const GenTime &)), m_screen, SLOT(play(double, const GenTime &, const GenTime &)));

	connect(m_screen, SIGNAL(rendererConnected()), m_editPanel, SLOT(rendererConnected()));
	connect(m_screen, SIGNAL(rendererDisconnected()), m_editPanel, SLOT(rendererDisconnected()));
	connect(m_screen, SIGNAL(seekPositionChanged(const GenTime &)), this, SLOT(screenPositionChanged(const GenTime &)));
	connect(m_screen, SIGNAL(playSpeedChanged(double)), m_editPanel, SLOT(screenPlaySpeedChanged(double)));
	connect(m_screen, SIGNAL(mouseClicked()), this, SLOT(slotClickMonitor()));
	connect(m_screen, SIGNAL(mouseRightClicked()), this, SLOT(slotRightClickMonitor()));
	connect(m_screen, SIGNAL(mouseDragged()), this, SLOT(slotStartDrag()));
}

void KMMMonitor::setSceneList(const QDomDocument &scenelist)
{
  m_screen->setSceneList(scenelist);
}

void KMMMonitor::togglePlay()
{
	m_editPanel->togglePlay();
}

void KMMMonitor::togglePlaySelected()
{
	m_editPanel->togglePlaySelected();
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
	if(e->button() == RightButton) popupContextMenu();
}

void KMMMonitor::slotClickMonitor()
{
	emit monitorClicked(this);
}

void KMMMonitor::slotSetClip(DocClipRef *clip)
{
	if(!clip) {
		kdError() << "Null clip passed, not setting monitor." << endl;
		return;
	}

	m_referredClip = 0;
	if(m_clip != 0) {
		delete m_clip;
		m_clip = 0;
	}

	// create a copy of the clip.
	m_clip = clip->clone(m_document->effectDescriptions(), m_document->clipManager());

	if(!m_clip) {
		kdError() << "KMMMonitor : Could not copy clip - you won't be able to see it!!!" << endl;
	} else {
		m_referredClip = clip;
		doCommonSetClip();
	}
}

void KMMMonitor::slotSetClip(DocClipBase *clip)
{
	if(!clip) {
		kdError() << "Null clip passed, not setting monitor." << endl;
		return;
	}

	m_referredClip = 0;
	if(m_clip != 0) {
		delete m_clip;
		m_clip = 0;
	}

	// create a copy of the clip.
	m_clip = new DocClipRef(clip);

	if(!m_clip) {
		kdError() << "KMMMonitor : Could not copy clip - drag 'n' drop will not work!" << endl;
	}

	doCommonSetClip();
}

void KMMMonitor::doCommonSetClip()
{
	QDomDocument scenelist = m_clip->generateSceneList();
	setSceneList(scenelist);
	m_screen->setClipLength((int)m_clip->duration().frames(m_document->framesPerSecond()));
	m_editPanel->setClipLength((int)m_clip->duration().frames(m_document->framesPerSecond()));

	//COMMENTED BY ROBERT 08-13-2004 --WAS RESETTING SEEK AND INPOINT/OUTPOINT MARKERS WHEN MOVING A CLIP
	//m_editPanel->setInpoint(m_clip->cropStartTime());
	//m_editPanel->setOutpoint(m_clip->cropStartTime() + m_clip->cropDuration());

	/*if( (!m_noSeek) ||
	    (seekPosition() < m_clip->cropStartTime()) ||
	    (seekPosition() > m_clip->cropStartTime() + m_clip->cropDuration())) {
		seek(m_clip->cropStartTime());
		m_screen->seek(m_clip->cropStartTime());
		
	}*/
}

void KMMMonitor::slotClearClip()
{
	m_referredClip = 0;
	if(m_clip != 0) {
		delete m_clip;
		m_clip = 0;
	}
	m_screen->setClipLength(0);
	m_editPanel->setClipLength(0);
	m_editPanel->setInpoint(GenTime(0));
	m_editPanel->setOutpoint(GenTime(0));

	seek(GenTime(0));
	m_screen->seek(GenTime(0));
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

	ClipDrag *drag = new ClipDrag(m_clip, this, "Clip from monitor");
	drag->dragCopy();
}

void KMMMonitor::setNoSeek(bool noSeek)
{
	m_noSeek = noSeek;
}

DocClipRef *KMMMonitor::clip()
{
	return m_clip;
}

void KMMMonitor::slotUpdateClip(DocClipRef *clip)
{
	if(m_referredClip == clip) {
		m_clip->setCropStartTime(clip->cropStartTime());
		m_clip->setCropDuration(clip->cropDuration());

		m_editPanel->setInpoint(m_clip->cropStartTime());
		m_editPanel->setOutpoint(m_clip->cropStartTime() + m_clip->cropDuration());
	}
}

void KMMMonitor::slotClipCropStartChanged(DocClipRef *clip)
{
	if(m_referredClip == clip) {
		slotUpdateClip(clip);
		seek(clip->cropStartTime());
	}
}

void KMMMonitor::slotClipCropEndChanged(DocClipRef *clip)
{
	if(m_referredClip == clip) {
		slotUpdateClip(clip);
		seek(clip->cropStartTime() + clip->cropDuration());
	}
}

void KMMMonitor::popupContextMenu()
{
	QPopupMenu *menu = (QPopupMenu *)m_app->factory()->container("monitor_context", m_app);
	if(menu) {
		menu->popup(QCursor::pos());
	}
}

void KMMMonitor::slotRightClickMonitor()
{
	popupContextMenu();
}

void KMMMonitor::slotToggleSnapMarker()
{
	if(m_referredClip) {
		Command::KAddMarkerCommand *command;

		if(m_referredClip->hasSnapMarker(seekPosition())) {
			command = new Command::KAddMarkerCommand(*m_document, m_referredClip, seekPosition(), false);
		} else {
			command = new Command::KAddMarkerCommand(*m_document, m_referredClip, seekPosition(), true);
		}

		m_app->addCommand(command, true);
	}

	updateEditPanel(seekPosition());
}

void KMMMonitor::slotPreviousSnapMarker()
{
	if(m_referredClip) {
		seek(m_referredClip->findPreviousSnapMarker(seekPosition()));
	}
}

void KMMMonitor::slotNextSnapMarker()
{
	if(m_referredClip) {
		seek(m_referredClip->findNextSnapMarker(seekPosition()));
	}
}

void KMMMonitor::updateEditPanel(const GenTime &time)
{
	if(m_referredClip) {
		m_editPanel->setSnapMarker(m_referredClip->hasSnapMarker(time));
	}
}
