/***************************************************************************
                          TrackPanelTransitionMoveFunction.cpp  -  description
                             -------------------
    begin                : Sun Feb 9 2006
    copyright            : (C) 2006 Jean-Baptiste Mardelle, jb@ader.ch
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/
#include "trackpaneltransitionmovefunction.h"

#include "kdebug.h"

#include "doctrackbase.h"
#include "kdenlive.h"
#include "ktimeline.h"
#include "kresizecommand.h"
#include "kselectclipcommand.h"
#include "docclipproject.h"
#include "transitionstack.h"
#include "transitiondialog.h"

#include <cmath>

// static
const uint TrackPanelTransitionMoveFunction::s_resizeTolerance = 5;

TrackPanelTransitionMoveFunction::TrackPanelTransitionMoveFunction(Gui::KdenliveApp * app, Gui::KTimeLine * timeline, KdenliveDoc * document):
m_app(app),
m_timeline(timeline),
m_document(document),
m_clipUnderMouse(0),
m_resizeCommand(0),
m_selectedTransition(0),
m_startedTransitionMove(false), m_dragging(false),
m_snapToGrid(), m_refresh(false)
{
    
}

TrackPanelTransitionMoveFunction::~TrackPanelTransitionMoveFunction()
{
}


bool TrackPanelTransitionMoveFunction::mouseApplies(Gui::KTrackPanel * panel,
    QMouseEvent * event) const
{
    bool result = false;

    if (panel->hasDocumentTrackIndex()) {
	DocTrackBase *track =
	    m_document->track(panel->documentTrackIndex());
	if (track) {
	    GenTime mouseTime(m_timeline->mapLocalToValue(event->x()),
		m_document->framesPerSecond());
	    DocClipRef *clip = track->getClipAt(mouseTime);
	    if (clip) {
                TransitionStack m_transitions = clip->clipTransitions();
                if (m_transitions.isEmpty()) return false;

                TransitionStack::iterator itt = m_transitions.begin();
                //  Loop through the clip's transitions
                while (itt) {
                    uint dx1 = m_timeline->mapValueToLocal((*itt)->transitionStartTime().frames(m_document->framesPerSecond()));
                    uint dx2 = m_timeline->mapValueToLocal((*itt)->transitionEndTime().frames(m_document->framesPerSecond()));

                    if ((event->x() > dx1+s_resizeTolerance) && (event->x()+s_resizeTolerance< dx2))
                            return true;
                ++itt;
                }
	    }
	}
    }
    return result;
}


QCursor TrackPanelTransitionMoveFunction::getMouseCursor(Gui::KTrackPanel *
    panel, QMouseEvent * event)
{
    return QCursor(Qt::SizeAllCursor);
}



bool TrackPanelTransitionMoveFunction::mouseDoubleClicked(Gui::KTrackPanel * panel, QMouseEvent * event)
{
    if (panel->hasDocumentTrackIndex()) {
	DocTrackBase *track = m_document->track(panel->documentTrackIndex());
	if (track) {
	    GenTime mouseTime(m_timeline->mapLocalToValue(event->x()), m_document->framesPerSecond());
	    DocClipRef *clip = track->getClipAt(mouseTime);
	    if (clip) {
    		Gui::TransitionDialog *dia = new Gui::TransitionDialog();
                dia->setActivePage(clip->clipTransitions().first()->transitionType());
                dia->setTransitionDirection(clip->clipTransitions().first()->invertTransition());
                dia->setTransitionParameters(clip->clipTransitions().first()->transitionParameters());
    			if (dia->exec() == QDialog::Accepted) {
        			clip->clipTransitions().first()->setTransitionType(dia->selectedTransition());
                                clip->clipTransitions().first()->setTransitionParameters(dia->transitionParameters());
                                clip->clipTransitions().first()->setTransitionDirection(dia->transitionDirection());
			emit transitionChanged(true);
    			}
    		delete dia;
	    }
	}
    }
    return true;
}

bool TrackPanelTransitionMoveFunction::mousePressed(Gui::KTrackPanel * panel,
    QMouseEvent * event)
{
    bool result = false;

    if (panel->hasDocumentTrackIndex()) {
	DocTrackBase *track =
	    m_document->track(panel->documentTrackIndex());
	if (track) {
	    GenTime mouseTime(m_timeline->mapLocalToValue(event->x()),
		m_document->framesPerSecond());
	    m_clipUnderMouse = track->getClipAt(mouseTime);
	    if (m_clipUnderMouse) {
                TransitionStack m_transitions = m_clipUnderMouse->clipTransitions();
                if (m_transitions.isEmpty()) return false;

                TransitionStack::iterator itt = m_transitions.begin();
                //  Loop through the clip's transitions
                while (itt) {
                    uint dx1 = m_timeline->mapValueToLocal((*itt)->transitionStartTime().frames(m_document->framesPerSecond()));
                    uint dx2 = m_timeline->mapValueToLocal((*itt)->transitionEndTime().frames(m_document->framesPerSecond()));

                    if ((event->x() > dx1+s_resizeTolerance) && (event->x()+s_resizeTolerance< dx2))
                    {
                        m_selectedTransition = (*itt);
                        m_dragging = true;
                        m_clipOffset = m_timeline->timeUnderMouse((double) event->x()) - m_selectedTransition->transitionStartTime();
                        m_transitionOffset = m_selectedTransition->transitionStartTime(); 
                        break;
                    }
                    ++itt;
                }
                

                m_snapToGrid.clearSnapList();
                if (m_timeline->snapToSeekTime())
                    m_snapToGrid.addToSnapList(m_timeline->seekPosition());
                m_snapToGrid.setSnapToFrame(m_timeline->snapToFrame());

                m_snapToGrid.addToSnapList(m_document->
                        getSnapTimes(m_timeline->snapToBorders(),
                                     m_timeline->snapToMarkers(), true, false));

                m_snapToGrid.setSnapTolerance(GenTime(m_timeline->
                        mapLocalToValue(Gui::KTimeLine::snapTolerance) -
                        m_timeline->mapLocalToValue(0),
                m_document->framesPerSecond()));

                QValueVector < GenTime > cursor;

                    cursor.append((*itt)->transitionStartTime());
                    cursor.append((*itt)->transitionEndTime());

                m_snapToGrid.setCursorTimes(cursor);
                
                       result = true;
            }
        }
    }

    return result;
}

bool TrackPanelTransitionMoveFunction::mouseReleased(Gui::KTrackPanel * panel,
    QMouseEvent * event)
{
    bool result = false;
    m_selectedTransition = 0;
    m_dragging = false;
    emit transitionChanged(true);
    // Select the keyframe
    /*
    Effect *effect = m_clipUnderMouse->selectedEffect();
    uint effectIndex = 0;
    effect->parameter(effectIndex)->
	setSelectedKeyFrame(m_selectedKeyframe);
    */
    //emit redrawTrack();

/*	m_resizeCommand->setEndSize(*m_clipUnderMouse);
	m_app->addCommand(m_resizeCommand, false);
	m_document->indirectlyModified();
	m_resizeCommand = 0;*/

    result = true;
    return result;
}


bool TrackPanelTransitionMoveFunction::mouseMoved(Gui::KTrackPanel * panel,
                                            QMouseEvent * event)
{
    bool result = false;

    if (panel->hasDocumentTrackIndex()) {
        DocTrackBase *track =
                m_document->track(panel->documentTrackIndex());
        if (track) {
            
            GenTime mouseTime =
                    m_timeline->
                    timeUnderMouse(event->x()) - m_clipOffset;
            
            mouseTime = m_snapToGrid.getSnappedTime(mouseTime);

            if (m_dragging && m_clipUnderMouse) {
                         m_startedTransitionMove = true;
                         m_clipUnderMouse->moveTransition(m_selectedTransition, mouseTime - m_transitionOffset);
                         m_transitionOffset = mouseTime;
                         result = true;
                }
        }
    }

    return result;
}



/*

void TrackPanelTransitionMoveFunction::initiateDrag(DocClipRef * clipUnderMouse,
                                              GenTime mouseTime)
{
    kdDebug()<<"+++++++++++++++++++++++++++++++++++++++++++++++"<<endl;
    kdDebug()<<"INITIATE DRAG"<<endl;
    kdDebug()<<"+++++++++++++++++++++++++++++++++++++++++++++++"<<endl;
    m_startedTransitionMove = true;
    m_clipOffset = mouseTime - m_clipUnderMouse->trackStart();
}

// virtual
bool TrackPanelTransitionMoveFunction::dragEntered(Gui::KTrackPanel * panel,
                                             QDragEnterEvent * event)
{
    kdDebug()<<"DRAGGING ENTERED"<<endl;
    if (m_startedTransitionMove) {
        kdDebug()<<"+++++++++++++++++++++++++++++++++++++++++++++++"<<endl;
        kdDebug()<<"DRAG ENTERED"<<endl;
        kdDebug()<<"+++++++++++++++++++++++++++++++++++++++++++++++"<<endl;
        m_document->activateSceneListGeneration(false);
        event->accept(true);
    }
    else event->accept(false);
    m_startedTransitionMove = false;
    return true;
}

// virtual
bool TrackPanelTransitionMoveFunction::dragMoved(Gui::KTrackPanel * panel,
                                           QDragMoveEvent * event)
{
    kdDebug()<<"DRAGGING MOVED"<<endl;
    if (!m_clipUnderMouse) {
        event->ignore();
        return false;
    }
    kdDebug()<<"+++++++++++++++++++++++++++++++++++++++++++++++"<<endl;
    kdDebug()<<"DRAGGING"<<endl;
    kdDebug()<<"+++++++++++++++++++++++++++++++++++++++++++++++"<<endl;
    QPoint pos = event->pos();
    GenTime mouseTime =
    m_timeline->timeUnderMouse((double) pos.x()) - m_clipOffset;
    mouseTime = m_snapToGrid.getSnappedTime(mouseTime);
    mouseTime = mouseTime + m_clipOffset;

    m_clipUnderMouse->moveTransition(m_selectedTransition, mouseTime - m_clipOffset);
    event->accept();
    return true;
}
*/

