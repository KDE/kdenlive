/***************************************************************************
                          trackpanelfunction.cpp  -  description
                             -------------------
    begin                : Sun May 18 2003
    copyright            : (C) 2003 Jason Wood, jasonwood@blueyonder.co.uk
			 : (C) 2006 Jean-Baptiste Mardelle, jb@ader.ch
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/
#include "trackpanelkeyframefunction.h"

#include "kdebug.h"

#include "doctrackbase.h"
#include "kdenlive.h"
#include "ktimeline.h"
#include "kresizecommand.h"
#include "kselectclipcommand.h"
#include "effectparameter.h"
#include "effectparamdesc.h"
#include "effectkeyframe.h"
#include "effectdoublekeyframe.h"

#include <cmath>

// static
const uint TrackPanelKeyFrameFunction::s_resizeTolerance = 10;

TrackPanelKeyFrameFunction::TrackPanelKeyFrameFunction(Gui::KdenliveApp * app, Gui::KTimeLine * timeline, KdenliveDoc * document):
m_app(app),
m_timeline(timeline),
m_document(document),
m_clipUnderMouse(0),
 m_selectedKeyframe(-1),
m_resizeCommand(0),
 m_snapToGrid(), m_refresh(false), m_offset(0)
{
}

TrackPanelKeyFrameFunction::~TrackPanelKeyFrameFunction()
{
}


bool TrackPanelKeyFrameFunction::mouseApplies(Gui::KTrackPanel * panel,
    QMouseEvent * event) const
{
    bool result = false;
    if (panel->hasDocumentTrackIndex()) {
	if (panel->isTrackCollapsed()) return false; 
	DocTrackBase *track =
	    m_document->track(panel->documentTrackIndex());
	if (track) {
		GenTime mouseTime((int)m_timeline->mapLocalToValue(event->x()),
		m_document->framesPerSecond());
	    DocClipRef *clip = track->getClipAt(mouseTime);
	    if (clip && clip->hasEffect()) {

		// #TODO: Currently only works for the first parameter
		uint effectIndex = 0;
		Effect *effect = clip->selectedEffect();
		if (!effect) {
		    kdDebug() << "////// ERROR, EFFECT NOT FOUND" << endl;
		    return false;
		}

		if (!effect->parameter(effectIndex)) return false;

		if (effect->effectDescription().parameter(effectIndex)->type() == "double" || effect->effectDescription().parameter(effectIndex)->type() == "complex") {
		    if (event->state() & Qt::ControlButton) // Press ctrl to add keyframe
                        return true;
                    
		    uint count =
			effect->parameter(effectIndex)->numKeyFrames();
		    for (uint i = 0; i < count; i++) {
			uint dx1 =
			    effect->parameter(effectIndex)->keyframe(i)->
			    time() *
			    clip->cropDuration().frames(m_document->
			    framesPerSecond());

			uint dy1;
			if (effect->effectDescription().parameter(effectIndex)->type() == "complex")
			  dy1 = panel->y() - 2000 + panel->height()/2;
			// #WARNING: I don't understand why the panel->y() for first track is 2000 !!!
			else dy1 = panel->y() - 2000 + panel->height() - panel->height() * effect->parameter(effectIndex)->keyframe(i)->toDoubleKeyFrame()->value() / 100;

			if ((fabs(m_timeline->mapValueToLocal(clip->trackStart().frames(m_document->framesPerSecond()) + dx1) - event->x()) < s_resizeTolerance) && (fabs(dy1 - event->y()) < s_resizeTolerance))
			    return true;
                    }
		}
	    }
	}
    }
    return result;
}


QCursor TrackPanelKeyFrameFunction::getMouseCursor(Gui::KTrackPanel *
    panel, QMouseEvent * event)
{
//return QCursor(Qt::SizeVerCursor);
    return QCursor(Qt::PointingHandCursor);
}

bool TrackPanelKeyFrameFunction::mousePressed(Gui::KTrackPanel * panel,
    QMouseEvent * event)
{
    bool result = false;
    m_offset = 0;
    if (panel->hasDocumentTrackIndex()) {
	DocTrackBase *track =
	    m_document->track(panel->documentTrackIndex());
	if (track) {
	    GenTime mouseTime((int)m_timeline->mapLocalToValue(event->x()), m_document->framesPerSecond());
	    m_clipUnderMouse = track->getClipAt(mouseTime);
	    if (m_clipUnderMouse) {
		if (!track->clipSelected(m_clipUnderMouse))
		    track->selectClip(m_clipUnderMouse, true);

		// #TODO: Currently only works for the first effect
		uint effectIndex = 0;
		Effect *effect = m_clipUnderMouse->selectedEffect();
		if (!effect || !m_clipUnderMouse->hasEffect()) {
		    kdDebug() << "////// ERROR, EFFECT NOT FOUND" << endl;
		    return false;
		}

		if (effect->parameter(0)) {
		    m_offset = panel->y() - 2000;
		    uint count = effect->parameter(effectIndex)->numKeyFrames();
		    for (uint i = 0; i < count; i++) {
			uint dx1 =(uint)( effect->parameter(effectIndex)->keyframe(i)-> time() *m_clipUnderMouse->cropDuration().frames(m_document->framesPerSecond()));

			uint dy1;
                        if (effect->effectDescription().parameter(effectIndex)->
                            type() == "double")
				dy1 = (uint)(panel->height() - panel->height() * effect->parameter(effectIndex)->keyframe(i)->toDoubleKeyFrame()->value() / 100);
			else dy1 = panel->height() / 2;

			if ((fabs(m_timeline->
				    mapValueToLocal(m_clipUnderMouse->
					trackStart().frames(m_document->
					    framesPerSecond()) + dx1) -
				    event->x()) < s_resizeTolerance)) {
			    m_selectedKeyframe = i;
			    m_selectedKeyframeValue = event->y();
			    //m_app->addCommand(Command::KSelectClipCommand::selectNone(m_document), true);

			    /*m_app->addCommand(
			       Command::KSelectClipCommand::selectClipAt(
			       m_document,
			       *track,
			       (m_clipUnderMouse->trackStart() + m_clipUnderMouse->trackEnd())/2.0)); */

			    //m_snapToGrid.setCursorTimes(cursor);
			    //m_resizeCommand = new Command::KResizeCommand(m_document, *m_clipUnderMouse);

			    return true;
			}
		    }
		    double dx =
			(m_timeline->mapLocalToValue(event->x()) -
			m_clipUnderMouse->trackStart().frames(m_document->
			    framesPerSecond())) /
			m_clipUnderMouse->cropDuration().
			frames(m_document->framesPerSecond());
		    m_refresh = true;
		    if (effect->effectDescription().parameter(effectIndex)->                           type() == "double")
		    m_selectedKeyframe =
			effect->addKeyFrame(effectIndex, dx, (panel->height() - (event->y() - m_offset)) * 100.0 / panel->height());
		    else m_selectedKeyframe = effect->addKeyFrame(effectIndex, dx);


		    //double dy1 = 100 - ((event->y() - m_offset)* 100 / panel->height());

		    //effect->parameter(effectIndex)->interpolateKeyFrame(0.7)->value());

		    return true;

		}
	    }
	}
    }
    return result;
}

bool TrackPanelKeyFrameFunction::mouseDoubleClicked(Gui::KTrackPanel * panel, QMouseEvent * event)
{
    return true;
}

bool TrackPanelKeyFrameFunction::mouseReleased(Gui::KTrackPanel * panel,
    QMouseEvent * event)
{
    bool result = false;
    // Select the keyframe
    Effect *effect = m_clipUnderMouse->selectedEffect();
    uint effectIndex = 0;
    effect->parameter(effectIndex)->
	setSelectedKeyFrame(m_selectedKeyframe);

    emit redrawTrack();
    if (m_refresh)
	emit signalKeyFrameChanged(true);
    m_refresh = false;
/*	m_resizeCommand->setEndSize(*m_clipUnderMouse);
	m_app->addCommand(m_resizeCommand, false);
	m_document->indirectlyModified();
	m_resizeCommand = 0;*/

    result = true;
    return result;
}


bool TrackPanelKeyFrameFunction::mouseMoved(Gui::KTrackPanel * panel,
    QMouseEvent * event)
{
    bool result = false;

    if (panel->hasDocumentTrackIndex()) {
	DocTrackBase *track =
	    m_document->track(panel->documentTrackIndex());
	if (track) {
	    GenTime mouseTime =
		m_snapToGrid.getSnappedTime(m_timeline->
		timeUnderMouse(event->x()));

	    if (m_clipUnderMouse && m_selectedKeyframe != -1) {
		result = true;


		uint effectIndex = 0;
		Effect *effect = m_clipUnderMouse->selectedEffect();
		if (!effect) {
		    kdDebug() << "////// ERROR, EFFECT NOT FOUND" << endl;
		    return false;
		}
		if (effect->parameter(effectIndex)) {
		    m_refresh = true;
		    int dy1 = 100 - ((event->y() - m_offset)* 100 / panel->height()); 

		    // If keyframe is moved out of the clip, remove it (but there should never be less than 2 keyframes
		    int delta = event->y() - m_offset;

		    if ((delta < -20 || (delta - panel->height()) > 20) && effect->parameter(effectIndex)->numKeyFrames() > 2) {
			effect->parameter(effectIndex)->
			    deleteKeyFrame(m_selectedKeyframe);
			m_selectedKeyframe = -1;
			m_refresh = false;
			emit redrawTrack();
			emit signalKeyFrameChanged(true);
			return true;
		    }
		    if (delta < 0) dy1 = 100;

		    if (dy1 < 0)
			dy1 = 0;
		    if (dy1 > 100)
			dy1 = 100;
		    //double dy2 =  (panel->height() - (event->y()-m_selectedKeyframeValue)) / panel->height();

		    if (effect->effectDescription().parameter(effectIndex)->type() == "double") {
			effect->parameter(effectIndex)->keyframe(m_selectedKeyframe)->   toDoubleKeyFrame()->setValue(dy1);
			if (m_selectedKeyframe > 0 && m_selectedKeyframe < effect->parameter(effectIndex)->numKeyFrames() - 1) {
				double currentTime = (m_timeline->mapLocalToValue(event->x()) - m_clipUnderMouse->trackStart().frames(m_document->framesPerSecond())) /m_clipUnderMouse->cropDuration().frames(m_document->framesPerSecond());
				double prevTime = effect->parameter(effectIndex)->keyframe(m_selectedKeyframe - 1)->time();
				double nextTime = effect->parameter(effectIndex)->keyframe(m_selectedKeyframe + 1)->time();
				if (currentTime > prevTime && currentTime < nextTime)
					effect->parameter(effectIndex)->keyframe(m_selectedKeyframe)->setTime(currentTime);
			}
		    	emit redrawTrack();
		    }
		    result = true;
		}

	    } else {
		kdError() <<
		    "Unknown resize state reached in KMMTimeLineTrackView::mouseMoveEvent()"
		    << endl;
		kdError() << "(this message should never be seen!)" <<
		    endl;
	    }
	}
    }

    return result;
}
