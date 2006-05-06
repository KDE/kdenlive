/***************************************************************************
                          capturemonitor  -  description
                             -------------------
    begin                : Sun Jun 12 2005
    copyright            : (C) 2005 by Jason Wood
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
#include "capturemonitor.h"

#include <qtoolbutton.h>

#include <kled.h>
#include <klocale.h>
#include <kmmscreen.h>
#include <kurlrequester.h>
#include <klistview.h>
#include <kiconloader.h>

namespace Gui {

    CaptureMonitor::CaptureMonitor(KdenliveApp * app, QWidget * parent,
	const char *name)
    :KMonitor(parent, name), m_mainLayout(new QHBox(this, "capLayout")),
	m_saveLayout(new QVBox(m_mainLayout, "saveLayout")),
	m_saveBrowser(new KURLRequester(m_saveLayout, "saveUrl")),
	m_playListView(new KListView(m_saveLayout, "playlistView")),
	m_rightLayout(new QVBox(m_mainLayout, "rightLayout")),
	m_screen(new KMMScreen(app, m_rightLayout, name)),
	m_buttonLayout(new QHBox(m_rightLayout, "buttonLayout")),
	m_rewindButton(new QToolButton(m_buttonLayout, "rewind")),
	m_reverseButton(new QToolButton(m_buttonLayout, "reverse")),
	m_reverseSlowButton(new QToolButton(m_buttonLayout,
	    "reverseSlow")), m_stopButton(new QToolButton(m_buttonLayout,
	    "stop")), m_playSlowButton(new QToolButton(m_buttonLayout,
	    "playSlow")), m_playButton(new QToolButton(m_buttonLayout,
	    "play")), m_forwardButton(new QToolButton(m_buttonLayout,
	    "forward")), m_recordButton(new QToolButton(m_buttonLayout,
	    "record")), m_renderStatus(new KLed(m_buttonLayout, name))
    {
	m_playListView->addColumn(i18n("Name"));
	m_playListView->addColumn(i18n("Inpoint"));
	m_playListView->addColumn(i18n("Outpoint"));

	KIconLoader loader;

	 m_renderStatus->off();

	 m_rewindButton->setPixmap(loader.loadIcon("player_rew",
		KIcon::Toolbar));
	 m_reverseButton->setPixmap(loader.loadIcon("", KIcon::Toolbar));
	 m_reverseSlowButton->setPixmap(loader.loadIcon("",
		KIcon::Toolbar));
	 m_stopButton->setPixmap(loader.loadIcon("player_stop",
		KIcon::Toolbar));
	 m_playSlowButton->setPixmap(loader.loadIcon("", KIcon::Toolbar));
	 m_playButton->setPixmap(loader.loadIcon("player_play",
		KIcon::Toolbar));
	 m_forwardButton->setPixmap(loader.loadIcon("player_fwd",
		KIcon::Toolbar));
	 m_recordButton->setPixmap(loader.loadIcon("record",
		KIcon::Toolbar));

	 connect(m_screen, SIGNAL(rendererConnected()), this,
	    SLOT(slotSetupScreen()));
	 connect(m_screen, SIGNAL(rendererConnected()), m_renderStatus,
	    SLOT(on()));
	 connect(m_screen, SIGNAL(rendererDisconnected()), m_renderStatus,
	    SLOT(off()));

	 connect(m_rewindButton, SIGNAL(clicked()), this,
	    SLOT(slotRewind()));
	 connect(m_reverseButton, SIGNAL(clicked()), this,
	    SLOT(slotReverse()));
	 connect(m_reverseSlowButton, SIGNAL(clicked()), this,
	    SLOT(slotReverseSlow()));
	 connect(m_stopButton, SIGNAL(clicked()), this, SLOT(slotStop()));
	 connect(m_playSlowButton, SIGNAL(clicked()), this,
	    SLOT(slotPlaySlow()));
	 connect(m_playButton, SIGNAL(clicked()), this, SLOT(slotPlay()));
	 connect(m_forwardButton, SIGNAL(clicked()), this,
	    SLOT(slotForward()));
    } CaptureMonitor::~CaptureMonitor() {
    }
    
    void CaptureMonitor::exportCurrentFrame(KURL url) const {
	// TODO FIXME
    } 

    KMMEditPanel *CaptureMonitor::editPanel() const {
	// TODO FIXME
	return NULL;
    } 
    
    KMMScreen *CaptureMonitor::screen() const {
	// TODO FIXME
	return NULL;
    } 
    
    DocClipRef *CaptureMonitor::clip() const {
	return NULL;
    } 
    
    void CaptureMonitor::slotSetupScreen() {
	//m_screen->setCapture();
    }

    void CaptureMonitor::slotRewind() {
	m_screen->play(-2.0);
    }

    void CaptureMonitor::slotReverse() {
	m_screen->play(-1.0);
    }

    void CaptureMonitor::slotReverseSlow() {
	m_screen->play(-0.5);
    }

    void CaptureMonitor::slotStop() {
	m_screen->play(0.0);
    }

    void CaptureMonitor::slotPlaySlow() {
	m_screen->play(0.5);
    }

    void CaptureMonitor::slotPlay() {
	m_screen->play(1.0);
    }

    void CaptureMonitor::slotForward() {
	m_screen->play(2.0);
    }

}

#include "capturemonitor.moc"
