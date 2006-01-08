/***************************************************************************
                          trackpanelfunction.cpp  -  description
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
#include "trackpanelkeyframefunction.h"

#include "kdebug.h"

#include "doctrackbase.h"
#include "kdenlive.h"
#include "ktimeline.h"
#include "kresizecommand.h"
#include "kselectclipcommand.h"
#include "effectparameter.h"
#include "effectkeyframe.h"
#include "effectdoublekeyframe.h"

#include <cmath>

// static
const uint TrackPanelKeyFrameFunction::s_resizeTolerance = 5;

TrackPanelKeyFrameFunction::TrackPanelKeyFrameFunction(Gui::KdenliveApp *app,
								Gui::KTimeLine *timeline,
								KdenliveDoc *document) :
								m_app(app),
								m_timeline(timeline),
								m_document(document),
								m_clipUnderMouse(0),
								m_resizeCommand(0),
								m_snapToGrid(),
								m_selectedKeyframe(-1)
{
}

TrackPanelKeyFrameFunction::~TrackPanelKeyFrameFunction()
{
}


bool TrackPanelKeyFrameFunction::mouseApplies(Gui::KTrackPanel *panel, QMouseEvent *event) const
{
	bool result = false;

	if(panel->hasDocumentTrackIndex()) {
		DocTrackBase *track = m_document->track(panel->documentTrackIndex());
		if(track) {
			GenTime mouseTime(m_timeline->mapLocalToValue(event->x()), m_document->framesPerSecond());
			DocClipRef *clip = track->getClipAt(mouseTime);
			if(clip && clip->hasEffect()) {

			// #TODO: Currently only works for the first effect
			uint effectIndex = 0;
			Effect *effect = clip->effectAt(effectIndex);
			if (! effect) {
			kdDebug()<<"////// ERROR, EFFECT NOT FOUND"<<endl;
			return false;
			}

			if (effect->parameter(effectIndex))
			{
			uint count = effect->parameter(effectIndex)->numKeyFrames();
			for (uint i = 0; i<count; i++)
			{
			uint dx1 = effect->parameter(effectIndex)->keyframe(i)->time()*clip->cropDuration().frames(m_document->framesPerSecond());
			uint dy1 = panel->height() -  panel->height()*effect->parameter(effectIndex)->keyframe(i)->toDoubleKeyFrame()->value()/100;
				
				if(( fabs(m_timeline->mapValueToLocal(clip->trackStart().frames(m_document->framesPerSecond())+ dx1) - event->x()) < s_resizeTolerance)) //(fabs(m_timeline->mapValueToLocal(clip->trackStart().frames(m_document->framesPerSecond())) + dy1 - event->y()) < s_resizeTolerance))
				return true;
				}
				}
			result = true;
			}
		}
	}
	return result;
}

void TrackPanelKeyFrameFunction::setKeyFrame(int i)
{
m_selectedKeyframe = i;
}

QCursor TrackPanelKeyFrameFunction::getMouseCursor(Gui::KTrackPanel *panel, QMouseEvent *event)
{
//return QCursor(Qt::SizeVerCursor);
return QCursor(Qt::Qt::PointingHandCursor);
}

bool TrackPanelKeyFrameFunction::mousePressed(Gui::KTrackPanel *panel, QMouseEvent *event)
{
kdDebug()<<"+++++++ KEYFRAME MOUSE PRESSED"<<endl;
	bool result = false;

	if(panel->hasDocumentTrackIndex()) {
		DocTrackBase *track = m_document->track(panel->documentTrackIndex());
		if(track) {
			GenTime mouseTime(m_timeline->mapLocalToValue(event->x()), m_document->framesPerSecond());
			m_clipUnderMouse = track->getClipAt(mouseTime);
			if(m_clipUnderMouse) {

			// #TODO: Currently only works for the first effect
			uint effectIndex = 0;
			Effect *effect = m_clipUnderMouse->effectAt(0);
			if (!effect || !m_clipUnderMouse->hasEffect()) {
			kdDebug()<<"////// ERROR, EFFECT NOT FOUND"<<endl;
			return false;
			}

			if (effect->parameter(0))
			{
			uint count = effect->parameter(effectIndex)->numKeyFrames();
			for (uint i = 0; i<count; i++)
			{
			uint dx1 = effect->parameter(effectIndex)->keyframe(i)->time()*m_clipUnderMouse->cropDuration().frames(m_document->framesPerSecond());
			uint dy1 = panel->height() -  panel->height()*effect->parameter(effectIndex)->keyframe(i)->toDoubleKeyFrame()->value()/100;
				
				if(( fabs(m_timeline->mapValueToLocal(m_clipUnderMouse->trackStart().frames(m_document->framesPerSecond())+ dx1) - event->x()) < s_resizeTolerance))
				{
				m_selectedKeyframe = i;
				m_selectedKeyframeValue = event->y();
				//m_app->addCommand(Command::KSelectClipCommand::selectNone(m_document), true);

				/*m_app->addCommand(
							Command::KSelectClipCommand::selectClipAt(
										m_document,
										*track,
										(m_clipUnderMouse->trackStart() + m_clipUnderMouse->trackEnd())/2.0));*/

				//m_snapToGrid.setCursorTimes(cursor);
				//m_resizeCommand = new Command::KResizeCommand(m_document, *m_clipUnderMouse);
				
				return true;
				}
				}
				kdDebug()<<"////// NEW KEYFRAME mouse: "<<m_timeline->mapLocalToValue(event->x())<<", CLIP: "<<m_clipUnderMouse->trackStart().frames(m_document->framesPerSecond())<<endl;
				double dx = (m_timeline->mapLocalToValue(event->x()) - m_clipUnderMouse->trackStart().frames(m_document->framesPerSecond()))/m_clipUnderMouse->cropDuration().frames(m_document->framesPerSecond());
				kdDebug()<<"+++++++++++++ POSITON: "<<dx<<endl;
				
				effect->addKeyFrame(effectIndex,dx,0.5); //effect->parameter(effectIndex)->interpolateKeyFrame(0.7)->value());
				//effect->parameter(effectIndex)->addKeyFrame(0.0);
				//effect->parameter(effectIndex)->);
				emit redrawTrack();
				return true;
				
			}
		}
	}
	}
	return result;
}

bool TrackPanelKeyFrameFunction::mouseReleased(Gui::KTrackPanel *panel, QMouseEvent *event)
{
	bool result = false;
	kdDebug()<<"KEYFRAME MOUSE RELEASED+++++++++++++++++++++++++++++++"<<endl;
	emit signalKeyFrameChanged(true);
/*	m_resizeCommand->setEndSize(*m_clipUnderMouse);
	m_app->addCommand(m_resizeCommand, false);
	m_document->indirectlyModified();
	m_resizeCommand = 0;*/

	result = true;
	return result;
}


bool TrackPanelKeyFrameFunction::mouseMoved(Gui::KTrackPanel *panel, QMouseEvent *event)
{
	bool result = false;

	if(panel->hasDocumentTrackIndex()) {
		DocTrackBase *track = m_document->track(panel->documentTrackIndex());
		if(track) {
			GenTime mouseTime = m_snapToGrid.getSnappedTime(m_timeline->timeUnderMouse(event->x()));

			if(m_clipUnderMouse && m_selectedKeyframe!=-1) {
				result = true;


			uint effectIndex = 0;
			Effect *effect = m_clipUnderMouse->effectAt(effectIndex);
			if (!effect || !m_clipUnderMouse->hasEffect()) {
			kdDebug()<<"////// ERROR, EFFECT NOT FOUND"<<endl;
			return false;
			}
			if (effect->parameter(effectIndex))
			{
			double dy1 = panel->height() - ((event->y()-m_selectedKeyframeValue)*100/panel->height());	

			// If the keyframe is not the first one nor the last and is dragged away of the track, delete it.
			if ( (dy1<-100 || dy1>200) && m_selectedKeyframe!=0 && m_selectedKeyframe!=effect->parameter(effectIndex)->numKeyFrames()-1) 
			{
			effect->parameter(effectIndex)->deleteKeyFrame(m_selectedKeyframe);
			m_selectedKeyframe = -1;
			emit redrawTrack();
			return true;
			}

			if ( dy1<0 ) dy1 = 0;
			if ( dy1>100 ) dy1 = 100;
			//double dy2 =  (panel->height() - (event->y()-m_selectedKeyframeValue)) / panel->height();
			kdDebug()<< "+++  PANEL HEIGHT: "<<event->y()<<", "<<dy1<<endl;

			effect->parameter(effectIndex)->keyframe(m_selectedKeyframe)->toDoubleKeyFrame()->setValue(dy1);
			emit redrawTrack();
			result = true;
			}

			} else {
					kdError() << "Unknown resize state reached in KMMTimeLineTrackView::mouseMoveEvent()" << endl;
					kdError() << "(this message should never be seen!)" << endl;
				}
		}
	}

	return result;
}
