/***************************************************************************
                          kmmtrackpanel.cpp  -  description
                             -------------------
    begin                : Tue Aug 6 2002
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

#include "kmmtrackpanel.h"
#include "kmmtimeline.h"
#include "kdenlivedoc.h"
#include "trackpanelfunction.h"

#include <kdebug.h>
#include <iostream>

KMMTrackPanel::KMMTrackPanel(KMMTimeLine *timeline,
			KdenliveDoc *document,
       			DocTrackBase *docTrack,
		       	QWidget *parent,
		       	const char *name) :
					QHBox(parent,name),
					m_document(document),
					m_docTrack(docTrack),
					m_timeline(timeline)
{
  setMinimumWidth(200);
  setMaximumWidth(200);
  setSizePolicy(QSizePolicy(QSizePolicy::Maximum, QSizePolicy::Expanding));
  setPalette( QPalette( QColor(170, 170, 170) ) );

  setFrameStyle(QFrame::Panel | QFrame::Sunken);
}

KMMTrackPanel::~KMMTrackPanel()
{
}

/** Read property of KMMTimeLine * m_timeline. */
KMMTimeLine * KMMTrackPanel::timeLine()
{
	return m_timeline;
}

/** returns the document track which is displayed by this track */
DocTrackBase * KMMTrackPanel::docTrack()
{
	return m_docTrack;
}

/** Draws the trackto a back buffered QImage, but does not display it. This image can then be
blitted straight to the screen for speedy drawing. */
void KMMTrackPanel::drawToBackBuffer(QPainter &painter, QRect &rect)
{
	QPtrListIterator<TrackViewDecorator> itt(m_trackViewDecorators);

	while(itt.current()) {
		itt.current()->drawToBackBuffer(painter, rect);
		++itt;
	}

	// draw the vertical time marker
	int value = (int)timeLine()->mapValueToLocal(timeLine()->seekPosition().frames(m_document->framesPerSecond()));
	if(value >= rect.x() && value <= rect.x()+rect.width()) {
		painter.drawLine(value, rect.y(), value, rect.y() + rect.height());
	}
}

bool KMMTrackPanel::mousePressed(QMouseEvent *event)
{
	bool result = false;

	m_function = getApplicableFunction(m_timeline->editMode(), event);
	if(m_function) result = m_function->mousePressed(event);

	if(!result) m_function = 0;

	return result;
}

bool KMMTrackPanel::mouseReleased(QMouseEvent *event)
{
	bool result = false;

	if(m_function)
	{
		result = m_function->mouseReleased(event);
		m_function = 0;
	}

	return result;
}

bool KMMTrackPanel::mouseMoved(QMouseEvent *event)
{
	bool result = false;

	if(m_function) result = m_function->mouseMoved(event);

	if(!result) m_function = 0;

	return result;
}

QCursor KMMTrackPanel::getMouseCursor(QMouseEvent *event)
{
	QCursor result(Qt::ArrowCursor);

	TrackPanelFunction *function = getApplicableFunction(m_timeline->editMode(), event);
	if(function) result = function->getMouseCursor(event);

	return result;
}

TrackPanelFunction *KMMTrackPanel::getApplicableFunction(KdenliveApp::TimelineEditMode mode, 
								QMouseEvent *event)
{
	TrackPanelFunction *result = 0;

	QPtrListIterator<TrackPanelFunction> itt(m_trackPanelFunctions[mode]);

	while(itt.current() != 0) {
		if(itt.current()->mouseApplies(event)) {
			result = itt.current();
			break;
		}
		++itt;
	}

	return result;
}

void KMMTrackPanel::addFunctionDecorator(KdenliveApp::TimelineEditMode mode, TrackPanelFunction *function)
{
	// Make sure that every mode added to the map is set to Auto Delete - setting multiple times
	// will not cause an issue, but failing to set it will cause memory leaks.
  	m_trackPanelFunctions[mode].setAutoDelete(true);

	m_trackPanelFunctions[mode].append(function);
}

void KMMTrackPanel::addViewDecorator(TrackViewDecorator *view)
{
	// It is anticipated that we will add extra "modes" at some point. I have put the autodelete line here in the meantime
	// to remind me that each mode needs to be set to autodelete.
	m_trackViewDecorators.setAutoDelete(true);
	m_trackViewDecorators.append(view);
}