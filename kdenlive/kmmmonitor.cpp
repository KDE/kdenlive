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
#include <kinputdialog.h>

#include <qcolor.h>
#include <qcursor.h>
#include <qpopupmenu.h>

#include "kdenlive.h"
#include "kdenlivedoc.h"
#include "docclipbase.h"
#include "docclipavfile.h"
#include "avfile.h"
#include "clipdrag.h"
#include "krendermanager.h"
#include "kaddmarkercommand.h"

#include <algorithm>

namespace Gui {

    KMMMonitor::KMMMonitor(KdenliveApp * app, KdenliveDoc * document,
	QWidget * parent, const char *name):KMonitor(parent, name),
	m_app(app), m_document(document), m_screen(new KMMScreen(this, m_screenHolder, name)), m_screenHolder(new QVBox(this,
	    name)), m_editPanel(new KMMEditPanel(document, this, name)),
	m_noSeek(false), m_clip(NULL), m_referredClip(NULL) {

	connect(m_editPanel, SIGNAL(seekPositionChanged(const GenTime &)),
	    this, SIGNAL(seekPositionChanged(const GenTime &)));
	 connect(m_editPanel, SIGNAL(seekPositionChanged(const GenTime &)),
	    this, SLOT(updateEditPanel(const GenTime &)));
	 connect(m_editPanel,
	    SIGNAL(setVolume(double)), this, SLOT(slotSetVolume(double)));
	 connect(m_editPanel,
	    SIGNAL(inpointPositionChanged(const GenTime &)), this,
	    SIGNAL(inpointPositionChanged(const GenTime &)));
	 connect(m_editPanel,
	    SIGNAL(activatedSlider(int)), this,
	    SIGNAL(activatedSlider(int)));
	 connect(m_editPanel,
	    SIGNAL(outpointPositionChanged(const GenTime &)), this,
         SIGNAL(outpointPositionChanged(const GenTime &)));

	 connect(m_editPanel, SIGNAL(toggleSnapMarker()),
	    this, SLOT(slotToggleSnapMarker()));
	 connect(m_editPanel, SIGNAL(nextSnapMarkerClicked()),
	    this, SLOT(slotNextSnapMarker()));
	 connect(m_editPanel, SIGNAL(previousSnapMarkerClicked()),
	    this, SLOT(slotPreviousSnapMarker()));

	 m_screenHolder->setMargin(3);
	 m_screenHolder->setPaletteBackgroundColor(QColor(0, 0, 0));
	 m_screenHolder->setSizePolicy(QSizePolicy(QSizePolicy::Expanding,
		QSizePolicy::Expanding, FALSE));
	//m_editPanel->setSizePolicy(QSizePolicy (QSizePolicy::Expanding, QSizePolicy::Maximum, FALSE));
	 connectScreen();
	 //setSceneList(QDomDocument());
    } 
    
    KMMMonitor::~KMMMonitor() {
	if (m_editPanel)
	    delete m_editPanel;
	if (m_screen)
	    delete m_screen;
	if (m_screenHolder)
	    delete m_screenHolder;
	if (m_clip)
	    delete m_clip;
    }

    void KMMMonitor::screenPositionChanged(const GenTime & time) {
	disconnect(m_editPanel,
	    SIGNAL(seekPositionChanged(const GenTime &)), m_screen,
	    SLOT(seek(const GenTime &)));
	m_editPanel->seek(time);
	connect(m_editPanel, SIGNAL(seekPositionChanged(const GenTime &)),
	    m_screen, SLOT(seek(const GenTime &)));
	updateEditPanel(time);
    }
/*
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
*/

    void KMMMonitor::disconnectScreen() {
	disconnect(m_editPanel,
	    SIGNAL(seekPositionChanged(const GenTime &)), m_screen,
	    SLOT(seek(const GenTime &)));
    }

    void KMMMonitor::connectScreen() {
	connect(m_editPanel, SIGNAL(seekPositionChanged(const GenTime &)),
	    m_screen, SLOT(seek(const GenTime &)));
    }

    void KMMMonitor::slotPlaySpeedChanged(double speed)
    {
	m_editPanel->screenPlaySpeedChanged(speed);
    }

    int KMMMonitor::externalMonitor()
    {
	return m_app->externalMonitor();
    }

    void KMMMonitor::playingStopped()
    {
	m_editPanel->screenPlayStopped();
    }

    void KMMMonitor::setSceneList(const QDomDocument & scenelist, int position) {
	m_screen->setSceneList(scenelist, position);
    }
    
    void KMMMonitor::exportCurrentFrame(KURL url, bool notify) const {
        m_screen->exportCurrentFrame(url, notify);
    }

    const GenTime & KMMMonitor::seekPosition() const {
	return m_screen->seekPosition();
    } 

    void KMMMonitor::slotSetVolume(double volume) const {
	m_screen->setVolume( volume );
    }
    
    void KMMMonitor::activateMonitor() {
	m_app->activateMonitor(this);
    }

    KRender *KMMMonitor::findRenderer(const char *name) {
	return m_app->renderManager()->findRenderer(name);
    }

    void KMMMonitor::slotSetActive() const {
        m_editPanel->rendererConnected();
	//m_screenHolder->setPaletteBackgroundColor(QColor(16, 32, 71));
	m_screen->startRenderer();
    }

    void KMMMonitor::slotSetInactive() const {
        m_editPanel->rendererDisconnected();
	m_screen->stopRenderer();
	//m_screenHolder->setPaletteBackgroundColor(QColor(0, 0, 0));
    }

    void KMMMonitor::mousePressEvent(QMouseEvent * e) {
	emit monitorClicked(this);
	if (e->button() == RightButton)
	    popupContextMenu();
    }

    void KMMMonitor::clickMonitor() {
	emit monitorClicked(this);
    }

    void KMMMonitor::refreshClip() {
	    if (m_clip) doCommonSetClip(false);
    }

    void KMMMonitor::refreshDisplay() const {
	    m_screen->refreshDisplay();
    }

    void KMMMonitor::slotSetClip(DocClipRef * clip) {
	if (!clip) {
	    kdError() << "Null clip passed, not setting monitor." << endl;
	    return;
	}
        if (m_referredClip == clip) return;
	m_referredClip = 0;
	if (m_clip != 0) {
	    delete m_clip;
	    m_clip = 0;
	}
	// create a copy of the clip.
	m_clip = clip->clone(m_document);

	if (!m_clip) {
	    kdError() <<
		"KMMMonitor : Could not copy clip - you won't be able to see it!!!"
		<< endl;
	} else {
	    m_referredClip = clip;
	    doCommonSetClip();
	}
    }

    void KMMMonitor::slotSetClip(DocClipBase * clip) {
	if (!clip) {
	    kdError() << "Null clip passed, not setting monitor." << endl;
	    return;
	}
        //activateMonitor();
	m_referredClip = 0;
	if (m_clip) {
	    delete m_clip;
	    m_clip = 0;
	}
	// create a copy of the clip.
	m_clip = new DocClipRef(clip);
	if (!m_clip) {
	    kdError() <<
		"KMMMonitor : Could not copy clip - drag 'n' drop will not work!"
		<< endl;
	} else {
	    doCommonSetClip(false);
	}
    }

    void KMMMonitor::slotCheckLength() {
	m_screen->updateClipLength();
    }

    void KMMMonitor::slotSetClip(QDomDocument playlist, GenTime duration) {
	if (m_clip) {
	    delete m_clip;
	    m_clip = 0;
	}
	m_referredClip = 0;
	m_editPanel->doStop();
	setSceneList(playlist);
	m_screen->updateClipLength();
	//m_editPanel->setClipLength((int) duration.frames(m_document->framesPerSecond()));
	activateMonitor();
    }	

    void KMMMonitor::doCommonSetClip(bool resetCropPosition) {
	m_editPanel->doStop();
	QDomDocument scenelist = m_clip->generateSceneList();
	setSceneList(scenelist, (int) m_app->cursorPosition().frames(m_document->framesPerSecond()));
	GenTime clipDuration(m_clip->duration() / m_clip->speed());
	m_screen->updateClipLength();
	m_editPanel->setClipLength((int) clipDuration.frames(m_document->framesPerSecond()));
	activateMonitor();
        if (resetCropPosition) {
	   m_editPanel->setInpoint(m_clip->cropStartTime());
	   m_editPanel->setOutpoint(m_clip->cropStartTime() + m_clip->cropDuration());
        }
	if ((!m_noSeek) || (seekPosition() < m_clip->cropStartTime()) ||
	    (seekPosition() > clipDuration)) {
	//cropStartTime() + m_clip->cropDuration()))) {
	    m_screen->seek(m_clip->cropStartTime());
	}
    }

    void KMMMonitor::slotClearClip() {
	m_referredClip = 0;
	if (m_clip) {
//	    m_clip->disconnectThumbCreator();
	    delete m_clip;
	    m_clip = 0;
	}
	m_screen->resetRenderer();
	m_screen->updateClipLength();
	//m_editPanel->setClipLength(0);
	m_editPanel->setInpoint(GenTime(0));
	m_editPanel->setOutpoint(GenTime(0));
        m_editPanel->seek(GenTime(0));
	
    }

    void KMMMonitor::startDrag() {
	if (!m_clip) {
	    kdWarning() << "KMMMonitor cannot start drag - m_clip is null!"
		<< endl;
	    return;
	}

	m_clip->setCropStartTime(GenTime(m_editPanel->inpoint()));
	m_clip->setTrackStart(GenTime(0));
	if (m_editPanel->inpoint() >= m_editPanel->outpoint())
	    m_clip->setTrackEnd(m_editPanel->inpoint() +
		m_clip->duration());
	else
	    m_clip->setTrackEnd(m_editPanel->outpoint() -
		m_editPanel->inpoint());

	ClipDrag *drag = new ClipDrag(m_clip, this, "Clip from monitor");

	drag->dragCopy();
    }

    void KMMMonitor::setNoSeek(bool noSeek) {
	m_noSeek = noSeek;
    }

    DocClipRef *KMMMonitor::clip() const {
	return m_clip;
    } 


    void KMMMonitor::slotUpdateClip(DocClipRef * clip) {
	if (m_referredClip == clip) {
	    m_clip->setCropStartTime(clip->cropStartTime());
	    m_clip->setCropDuration(clip->cropDuration());

	    m_editPanel->setInpoint(m_clip->cropStartTime());
	    m_editPanel->setOutpoint(m_clip->cropStartTime() +
		m_clip->cropDuration());
	}
    }

    void KMMMonitor::slotClipCropStartChanged(DocClipRef * clip) {
	if (m_referredClip == clip) {
	    slotUpdateClip(clip);
	    m_editPanel->seek(clip->cropStartTime());
	}
    }

    void KMMMonitor::slotClipCropEndChanged(DocClipRef * clip) {
	if (m_referredClip == clip) {
	    slotUpdateClip(clip);
            m_editPanel->seek(clip->cropStartTime() + clip->cropDuration() - GenTime(1, m_document->framesPerSecond()));
	}
    }

    void KMMMonitor::popupContextMenu() {
	QPopupMenu *menu = NULL;
	if (QString(name()) == "ClipMonitor") menu = (QPopupMenu *) m_app->factory()->container("monitor_clip_context", m_app);
	else menu = (QPopupMenu *) m_app->factory()->container("monitor_context", m_app);
	if (menu) {
	    menu->popup(QCursor::pos());
	}
    }

    void KMMMonitor::rightClickMonitor() {
	popupContextMenu();
    }

    void KMMMonitor::slotPreviousSnapMarker() {
	if (m_referredClip) {
	    m_editPanel->seek(m_referredClip->referencedClip()->findPreviousSnapMarker(seekPosition()));
	} else m_app->slotPreviousSnap();
    }

    void KMMMonitor::slotNextSnapMarker() {
	if (m_referredClip) {
	    m_editPanel->seek(m_referredClip->referencedClip()->findNextSnapMarker(seekPosition()));
	} else m_app->slotNextSnap();
    }

    void KMMMonitor::updateEditPanel(const GenTime & time) {
	if (m_referredClip) {
	    m_editPanel->setSnapMarker(m_referredClip->hasSnapMarker(time) != GenTime(0.0));
	}
    }

    KMMEditPanel *KMMMonitor::editPanel() const {
	return m_editPanel;
    } 

    KMMScreen *KMMMonitor::screen() const {
	return m_screen;
    } 

    void KMMMonitor::slotToggleSnapMarker() {
	if (m_referredClip) {
	    Command::KAddMarkerCommand * command;
	    GenTime currentTime = seekPosition();
	    if (m_referredClip->hasSnapMarker(currentTime) != GenTime(0.0)) {
		command =
		    new Command::KAddMarkerCommand(*m_document,
		    m_referredClip->referencedClip()->getId(), currentTime, QString::null, false);
	    } else {
		bool ok;
		QString comment = KInputDialog::getText(i18n("Add Marker"), i18n("Marker comment: "), i18n("Marker"), &ok);
		if (ok) {
		    command = new Command::KAddMarkerCommand(*m_document, m_referredClip->referencedClip()->getId(), currentTime, comment, true);
		}
		else return;
	    }

	    m_app->addCommand(command, true);
	} else m_app->toggleMarkerUnderCursor();
	
	updateEditPanel(seekPosition());
    }

}				// namespace Gui
