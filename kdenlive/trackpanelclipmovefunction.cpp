/***************************************************************************
                       TrackPanelClipMoveFunction.cpp  -  description
                          -------------------
 begin                : Sun May 18 2003
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
#include "trackpanelclipmovefunction.h"

#include <qnamespace.h>

#include <kdebug.h>

#include "clipdrag.h"
#include "docclipproject.h"
#include "doctrackbase.h"
#include "effectdrag.h"
#include "kaddrefclipcommand.h"
#include "kdenlive.h"
#include "ktimeline.h"
#include "ktrackview.h"
#include "kselectclipcommand.h"
#include "kaddeffectcommand.h"

TrackPanelClipMoveFunction::TrackPanelClipMoveFunction(Gui::KdenliveApp * app, Gui::KTimeLine * timeline, KdenliveDoc * document):
m_app(app),
m_timeline(timeline),
m_document(document),
m_dragging(false), m_startedClipMove(false), m_masterClip(0)
{
    m_moveClipsCommand = 0;
    m_deleteClipsCommand = 0;
    m_addingClips = false;
}


TrackPanelClipMoveFunction::~TrackPanelClipMoveFunction()
{
}

bool TrackPanelClipMoveFunction::mouseApplies(Gui::KTrackPanel * panel,
    QMouseEvent * event) const
{
    return mouseApplies(event->pos());
}

bool TrackPanelClipMoveFunction::mouseApplies(const QPoint & pos) const
{
    DocClipRef *clipUnderMouse = 0;

    Gui::KTrackPanel * panel = m_timeline->trackView()->panelAt(pos.y());

    if (panel) {
	if (panel->hasDocumentTrackIndex()) {
	    DocTrackBase *track =
		m_document->track(panel->documentTrackIndex());
	    if (track) {
		GenTime mouseTime(m_timeline->mapLocalToValue(pos.x()),
		    m_document->framesPerSecond());
		clipUnderMouse = track->getClipAt(mouseTime);
	    }
	}
    }

    return clipUnderMouse;
}

QCursor TrackPanelClipMoveFunction::getMouseCursor(Gui::KTrackPanel *
    panel, QMouseEvent * event)
{
    return QCursor(Qt::SizeAllCursor);
}

bool TrackPanelClipMoveFunction::mousePressed(Gui::KTrackPanel * panel,
    QMouseEvent * event)
{
    bool result = false;

    if (panel->hasDocumentTrackIndex()) {
	DocTrackBase *track =
	    m_document->track(panel->documentTrackIndex());
	if (track) {
	    GenTime mouseTime(m_timeline->mapLocalToValue(event->x()),
		m_document->framesPerSecond());
	    m_clipUnderMouse = 0;
	    m_clipUnderMouse = track->getClipAt(mouseTime);

	    if (m_clipUnderMouse) {
		if (event->state() & Qt::ControlButton) {
		    m_app->
			addCommand(Command::KSelectClipCommand::
			toggleSelectClipAt(m_document, *track, mouseTime),
			true);
		} else if (event->state() & Qt::ShiftButton) {
                    // Shift clicking on a clip creates or deletes a transition with the other selected clip
                    m_app->addCommand(Command::KSelectClipCommand::selectClipAt(m_document, *track, mouseTime), true);
                    m_app->switchTransition();
                    m_app->addCommand(Command::KSelectClipCommand::selectNone(m_document), true);
                    m_app->addCommand(Command::KSelectClipCommand::selectClipAt(m_document, *track, mouseTime), true);
			//addCommand(Command::KSelectClipCommand::
			//selectClipAt(m_document, *track, mouseTime), true);
		}
		result = true;
	    }
	}
    }

    return result;
}

bool TrackPanelClipMoveFunction::mouseDoubleClicked(Gui::KTrackPanel * panel, QMouseEvent * event)
{
return true;
}

bool TrackPanelClipMoveFunction::mouseReleased(Gui::KTrackPanel * panel,
    QMouseEvent * event)
{
    bool result = false;

    if (panel->hasDocumentTrackIndex()) {
	DocTrackBase *track =
	    m_document->track(panel->documentTrackIndex());
	if (track) {
	    if (m_clipUnderMouse) {
		if (event->state() & Qt::ControlButton) {
		} else if (event->state() & Qt::ShiftButton) {
		} else {
		    GenTime mouseTime(m_timeline->mapLocalToValue(event->
			    x()), m_document->framesPerSecond());
		    m_app->
			addCommand(Command::KSelectClipCommand::
			selectNone(m_document), true);
		    m_app->
			addCommand(Command::KSelectClipCommand::
			selectClipAt(m_document, *track, mouseTime),
			true);
		}
		result = true;
	    }
	}
    }

    return result;
}

bool TrackPanelClipMoveFunction::mouseMoved(Gui::KTrackPanel * panel,
    QMouseEvent * event)
{
    bool result = false;

    if (panel->hasDocumentTrackIndex()) {
	DocTrackBase *track =
	    m_document->track(panel->documentTrackIndex());
	if (track) {
	    GenTime mouseTime(m_timeline->mapLocalToValue(event->x()),
		m_document->framesPerSecond());

	    if (m_dragging) {
		m_dragging = false;
		result = true;
	    } else {
		if (m_clipUnderMouse) {
		    if (!m_document->projectClip().
			clipSelected(m_clipUnderMouse)) {
			if ((event->state() & Qt::ControlButton)
			    || (event->state() & Qt::ShiftButton)) {
			    m_app->
				addCommand(Command::KSelectClipCommand::
				selectClipAt(m_document, *track,
				    mouseTime), true);
			} else {
			    m_app->
				addCommand(Command::KSelectClipCommand::
				selectNone(m_document), true);
			    m_app->
				addCommand(Command::KSelectClipCommand::
				selectClipAt(m_document, *track,
				    mouseTime), true);
			}
		    }
		    m_dragging = true;
		    initiateDrag(m_clipUnderMouse, mouseTime);

		    result = true;
		}
	    }
	}
    }

    return result;
}

// virtual
bool TrackPanelClipMoveFunction::dragEntered(Gui::KTrackPanel * panel,
    QDragEnterEvent * event)
{

    if (m_startedClipMove) {
	m_document->activateSceneListGeneration(false);
	event->accept(true);
//        return true;
    } else if (ClipDrag::canDecode(event)) {
	m_document->activateSceneListGeneration(false);
	m_selection =
	    ClipDrag::decode(m_document->effectDescriptions(),
	    m_document->clipManager(), event);

	/*bool allowed = false;

	   kdDebug()<<"+++++++++++++DRAG CLIP: "<<m_selection.getFirst()->clipType()<<endl;
	   kdDebug()<<"+++++++++++++DRAG PANEL: "<<panel->trackType()<<endl;

	   if ((m_selection.getFirst()->clipType() == DocClipBase::VIDEO || m_selection.getFirst()->clipType() == DocClipBase::AV) && panel->trackType() == Gui::VIDEOTRACK) allowed = true;

	   if (m_selection.getFirst()->clipType() == DocClipBase::AUDIO && panel->trackType() == Gui::SOUNDTRACK) allowed = true; */

	if (!m_selection.isEmpty()) {
	    if (m_selection.masterClip() == 0)
		m_selection.setMasterClip(m_selection.first());
	    m_masterClip = m_selection.masterClip();
	    m_clipOffset = GenTime();
	    if (m_selection.isEmpty()) {
		event->accept(false);
	    } else {
		setupSnapToGrid();
		event->accept(true);
	    }
	} else {
	    kdError() <<
		"ERROR! ERROR! ERROR! ClipDrag:decode decoded a null clip!!!"
		<< endl;
	}
    } else if (EffectDrag::canDecode(event)) {
	event->accept(true);
    } else {
	event->accept(false);
    }
    //m_startedClipMove = false;

    return true;
}

// virtual
bool TrackPanelClipMoveFunction::dragMoved(Gui::KTrackPanel * panel,
    QDragMoveEvent * event)
{
    QPoint pos = event->pos();
    if (ClipDrag::canDecode(event)) {
	GenTime mouseTime =
	    m_timeline->timeUnderMouse((double) pos.x()) - m_clipOffset;
	mouseTime = m_snapToGrid.getSnappedTime(mouseTime);
	mouseTime = mouseTime + m_clipOffset;

	int trackUnder = trackUnderPoint(pos);

	if (m_selection.isEmpty()) {
	    moveSelectedClips(trackUnder, mouseTime - m_clipOffset);
	} else {

	    if (m_document->projectClip().canAddClipsToTracks(m_selection,
		    trackUnder, mouseTime)) {
		addClipsToTracks(m_selection, trackUnder, mouseTime, true);
		//addClipsToTracks( m_selection, trackUnder+1, mouseTime , true );

		setupSnapToGrid();
		m_selection.clear();
	    }
	}

	m_timeline->trackView()->repaint();
    } else if (EffectDrag::canDecode(event)) {
	if (mouseApplies(pos)) {
	    event->accept();
	} else {
	    event->ignore();
	}
    } else {
	event->ignore();
    }

    m_timeline->checkScrolling(pos);

    return true;
}

int TrackPanelClipMoveFunction::trackUnderPoint(const QPoint & pos)
{
    uint y = pos.y();
    Gui::KTrackPanel * panel = m_timeline->trackView()->panelAt(y);

    if (panel) {
	return panel->documentTrackIndex();
    }

    return -1;
}

// virtual
bool TrackPanelClipMoveFunction::dragLeft(Gui::KTrackPanel * panel,
    QDragLeaveEvent * event)
{
    if (!m_selection.isEmpty()) {
	m_selection.setAutoDelete(true);
	m_selection.clear();
	m_selection.setAutoDelete(false);
    }
    
    if (m_addingClips) {
	m_addingClips = false;

	QPtrListIterator < DocTrackBase >
	    trackItt(m_document->trackList());

	while (trackItt.current()) {
	    (*trackItt)->deleteClips(true);
	    ++trackItt;
	}

	m_document->activateSceneListGeneration(true);
    }

    if (m_moveClipsCommand) {
        m_moveClipsCommand->setEndLocation(m_masterClip);
        m_app->addCommand(m_moveClipsCommand, false);

	// In a drag Leave Event, any clips in the selection are removed from the timeline.
	//delete m_moveClipsCommand;
        m_moveClipsCommand = 0;
        m_document->activateSceneListGeneration(true);
    }

    if (m_deleteClipsCommand) {
	m_app->addCommand(m_deleteClipsCommand, false);
	m_deleteClipsCommand = 0;

	QPtrListIterator < DocTrackBase >
	    trackItt(m_document->trackList());

	while (trackItt.current()) {
	    trackItt.current()->deleteClips(true);
	    ++trackItt;
	}
    }

    m_timeline->drawTrackViewBackBuffer();

    m_timeline->stopScrollTimer();
    
    return true;
}

// virtual
bool TrackPanelClipMoveFunction::dragDropped(Gui::KTrackPanel * panel,
    QDropEvent * event)
{
    m_startedClipMove = false;
    
    if (ClipDrag::canDecode(event)) {
	if (!m_selection.isEmpty()) {
	    m_selection.setAutoDelete(true);
	    m_selection.clear();
	    m_selection.setAutoDelete(false);
	}

	if (m_addingClips) {
	    m_app->addCommand(createAddClipsCommand(), false);
	    m_addingClips = false;
            m_app->clipReferenceChanged();
	    m_document->activateSceneListGeneration(true);
	}

	if (m_deleteClipsCommand) {
	    delete m_deleteClipsCommand;
	    m_deleteClipsCommand = 0;
	}

	if (m_moveClipsCommand) {
	    m_moveClipsCommand->setEndLocation(m_masterClip);
	    m_app->addCommand(m_moveClipsCommand, false);
	    m_moveClipsCommand = 0;	// KdenliveApp is now managing this command, we do not need to delete it.
	    m_document->activateSceneListGeneration(true);
	}

	m_timeline->stopScrollTimer();
    } else if (EffectDrag::canDecode(event)) {
	DocClipRef *clipUnderMouse = 0;
	Gui::KTrackPanel * panel =
	    m_timeline->trackView()->panelAt(event->pos().y());
	if (panel) {
	    DocTrackBase *track =
		m_document->track(panel->documentTrackIndex());
	    if (track) {
		GenTime mouseTime(m_timeline->mapLocalToValue(event->pos().
			x()), m_document->framesPerSecond());
		clipUnderMouse = track->getClipAt(mouseTime);
	    }
	}

	if (clipUnderMouse) {
	    Effect *effect = EffectDrag::decode(m_document, event);
	    if (effect) {
		m_app->
		    addCommand(Command::KAddEffectCommand::
		    appendEffect(m_document, clipUnderMouse, effect),
		    true);
	    } else {
		kdWarning() <<
		    "EffectDrag::decode did not return an effect, ignoring drag drop..."
		    << endl;
	    }
	    delete effect;
	}
    }
    return true;
}

bool TrackPanelClipMoveFunction::moveSelectedClips(int newTrack,
    GenTime start)
{
    int trackOffset =
	m_document->trackIndex(m_document->findTrack(m_masterClip));
    GenTime startOffset;

    if ((!m_masterClip) || (trackOffset == -1)) {
	kdError() <<
	    "Trying to move selected clips, master clip is not set." <<
	    endl;
	return false;
    } else {
	startOffset = m_masterClip->trackStart();
    }

    trackOffset = newTrack - trackOffset;
    startOffset = start - startOffset;


    m_document->moveSelectedClips(startOffset, trackOffset);

    m_timeline->drawTrackViewBackBuffer();
    return true;
}


void TrackPanelClipMoveFunction::addClipsToTracks(DocClipRefList & clips,
    int track, GenTime value, bool selected)
{
    if (clips.isEmpty())
	return;

    if (selected) {
	m_app->
	    addCommand(Command::KSelectClipCommand::selectNone(m_document),
	    true);
    }

    DocClipRef *masterClip = clips.masterClip();
    if (!masterClip)
	masterClip = clips.first();

    GenTime startOffset = value - masterClip->trackStart();

    int trackOffset = masterClip->trackNum();

    if (trackOffset == -1)
	trackOffset = 0;
    trackOffset = track - trackOffset;

    QPtrListIterator < DocClipRef > itt(clips);
    int moveToTrack;

    while (itt.current() != 0) {
	moveToTrack = itt.current()->trackNum();

	if (moveToTrack == -1) {
	    moveToTrack = track;
	} else {
	    moveToTrack += trackOffset;
	}

	itt.current()->moveTrackStart(itt.current()->trackStart() +
	    startOffset);

	if ((moveToTrack >= 0) && (moveToTrack < m_document->numTracks())) {
	    m_document->track(moveToTrack)->addClip(itt.current(),
		selected);
	}

	++itt;
    }

    m_addingClips = true;
}

void TrackPanelClipMoveFunction::setupSnapToGrid()
{
    m_snapToGrid.clearSnapList();
    if (m_timeline->snapToSeekTime())
	m_snapToGrid.addToSnapList(m_timeline->seekPosition());
    m_snapToGrid.setSnapToFrame(m_timeline->snapToFrame());

    m_snapToGrid.addToSnapList(m_document->getSnapTimes(m_timeline->
	    snapToBorders(), m_timeline->snapToMarkers(), true, false));

    QValueVector < GenTime > cursor =
	m_document->getSnapTimes(m_timeline->snapToBorders(),
	m_timeline->snapToMarkers(), false, true);
    m_snapToGrid.setCursorTimes(cursor);

    m_snapToGrid.setSnapTolerance(GenTime(m_timeline->
	    mapLocalToValue(Gui::KTimeLine::snapTolerance) -
	    m_timeline->mapLocalToValue(0),
	    m_document->framesPerSecond()));
}

void TrackPanelClipMoveFunction::initiateDrag(DocClipRef * clipUnderMouse,
    GenTime mouseTime)
{
    m_masterClip = clipUnderMouse;
    m_clipOffset = mouseTime - clipUnderMouse->trackStart();

    m_moveClipsCommand =
	new Command::KMoveClipsCommand(m_document, m_masterClip);
    m_deleteClipsCommand =
	Command::KAddRefClipCommand::deleteSelectedClips(m_document);
    setupSnapToGrid();

    m_startedClipMove = true;

    DocClipRefList selection = m_document->listSelected();

    selection.setMasterClip(m_masterClip);
    ClipDrag *clip = new ClipDrag(selection, m_timeline, "Timeline Drag");

    clip->dragCopy();
}

KMacroCommand *TrackPanelClipMoveFunction::createAddClipsCommand()
{
    KMacroCommand *macroCommand = new KMacroCommand(i18n("Delete Clips"));

    for (int count = 0; count < m_document->numTracks(); ++count) {
	DocTrackBase *track = m_document->track(count);

	QPtrListIterator < DocClipRef > itt = track->firstClip(true);

	while (itt.current()) {
	    Command::KAddRefClipCommand * command =
		new Command::KAddRefClipCommand(m_document->
		effectDescriptions(), m_document->clipManager(),
		&m_document->projectClip(), itt.current(), true);
	    macroCommand->addCommand(command);
            (*itt)->referencedClip()->addReference();
	    ++itt;
	}
    }

    return macroCommand;
}
