/***************************************************************************
                          kmmtrackpanel.h  -  description
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

#ifndef KMMTRACKPANEL_H
#define KMMTRACKPANEL_H

#include <qpixmap.h>
#include <qpainter.h>
#include <qhbox.h>
#include <qcursor.h>
#include <qptrlist.h>
#include <qmap.h>
#include <kdenlive.h>

#include "doctrackbase.h"

class KdenliveDoc;
class KMMTimeLine;
class TrackPanelFunction;

/**Base class for all Track panels and their associated views.
  *@author Jason Wood
  */

class KMMTrackPanel : public QHBox  {
	Q_OBJECT
public:
	enum ResizeState {None, Start, End};

	KMMTrackPanel(KMMTimeLine *timeline, 
			KdenliveDoc *document,
			DocTrackBase *docTrack, 
			QWidget *parent, 
			const char *name);
	~KMMTrackPanel();
	
  	/** Read property of KMMTimeLine * m_timeline. */
  	KMMTimeLine * timeLine();
	
  	/** returns the document track which is displayed by this track */
  	DocTrackBase * docTrack();
	
  	/** This function will paint a clip on screen. This funtion must be provided by a derived class. */
	virtual void paintClip(QPainter &painter, DocClipRef *clip, QRect &rect, bool selected) = 0;
	
	/**
	Paints the backbuffer into the relevant place using the painter supplied. The
	track should be drawn into the area provided in area
	*/
	void drawToBackBuffer(QPainter &painter, QRect &rect);
	
	/**
	A mouse button has been pressed. Returns true if we want to handle this event
	*/
	bool mousePressed(QMouseEvent *event);
	
	/**
	Mouse Release Events in the track view area. Returns true if we have finished
	an operation now.
	*/
	bool mouseReleased(QMouseEvent *event);
	
	/**
	Processes Mouse Move events in the track view area. Returns true if we are
	continuing with the drag.
	*/
	bool mouseMoved(QMouseEvent *event);
	
	/**
	Set the mouse cursor to a relevant shape, depending on it's location within the
	track view area. The location is obtained from event.
	*/
  	QCursor getMouseCursor(QMouseEvent *event);
	
protected: // Protected methods
	/**
	Add a TrackPanelFunction decorator to this panel. By adding decorators, we give the
	class it's desired functionality.
	*/
	void addFunctionDecorator(KdenliveApp::TimelineEditMode mode, TrackPanelFunction *function);

	KdenliveDoc *document() { return m_document; }
	
private:	// private methods
	/**
	Returns the TrackPanelFunction that should handle curent mouse requests. Returns 0
	if no function is applicable.
	*/
	TrackPanelFunction *getApplicableFunction(KdenliveApp::TimelineEditMode mode, QMouseEvent *event);
	
protected: // Protected attributes
	/** The track document class that should be queried to build up this track view. */
	DocTrackBase *m_docTrack;
	
	/** The KMMTrackPanel needs access to various methods from it's parents Timeline.
	 *  The parent timeline is stored in this variable. */
	KMMTimeLine *m_timeline;
 private:
	/** A reference to the document this function applies to. */
	KdenliveDoc *m_document;

	/** A map of lists of track panel functions. */
	QMap<KdenliveApp::TimelineEditMode , QPtrList<TrackPanelFunction> > m_trackPanelFunctions;

	/** The currently applied function. This lasts from mousePressed
		until mouseRelease. */
	TrackPanelFunction *m_function;
signals: // Signals
	/**
	Emitted when an operation moves the clip crop start.
	*/
	void signalClipCropStartChanged(const GenTime &);

	/**
	Emitted when an operation moves the clip crop end.
	*/
	void signalClipCropEndChanged(const GenTime &);

	/**
	emitted when a tool is "looking" at a clip, it signifies to whatever is listening
	that displaying this information in some way would be useful.
	*/
	void lookingAtClip(DocClipRef *, const GenTime &);
};

#endif
