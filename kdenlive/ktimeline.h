/***************************************************************************
                        kmmtimeline.h  -  description
                           -------------------
  begin                : Fri Feb 15 2002
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

#ifndef KTIMELINE_H
#define KTIMELINE_H

#include <qvaluelist.h>
#include <qvbox.h>
#include <qtimer.h>

#include "gentime.h"

#include "ktrackpanel.h"

class QHBox;
class QScrollView;
class QScrollBar;
class KMacroCommand;
class KCommand;

namespace Command {
    class KMoveClipsCommand;
} namespace Gui {

    class KTrackView;
    class KScalableRuler;

/** This is the timeline. It gets populated by tracks, which in turn are populated
  * by video and audio clips, or transitional clips, or any other clip imaginable.
  * @author Jason Wood
  */

    class KTimeLine:public QVBox {
      Q_OBJECT public:
	KTimeLine(QWidget * rulerToolWidget, QWidget * scrollToolWidget,
	    QWidget * parent = 0, const char *name = 0);
	~KTimeLine();
	/**
	The snap tolerance specifies how many pixels away a selection is from a
	snap point before the snap takes effect.

	FIXME : this should not be *quite* as public as this :-/
	*/
	static uint snapTolerance;

	/** Clear all tracks from the timeline. */
	void clearTrackList();

	void resizeTracks();

	/** This method adds a new track to the trackGrid. */
	void appendTrack(KTrackPanel * track);

	void resizeEvent(QResizeEvent * event);

	/** Inserts a track at the position specified by index */
	void insertTrack(int index, KTrackPanel * track);

	/** No descriptions */
	void polish();

	/** Returns m_trackList
	Warning - this method is a bit of a hack, not good OOP practice, and should be removed at
	some point. */
	 QPtrList < KTrackPanel > &trackList();

	/** Returns the seek position of the timeline - this is the currently playing frame, or
	the currently seeked frame. */
	GenTime seekPosition() const;

	//Returns the inpoint/outpoing position of the timeline
	GenTime inpointPosition() const;
	GenTime outpointPosition() const;
	GenTime midpointPosition() const;
	//set difference between mid point slider and inpoint/outpoint
	void setMidValueDiff(const GenTime & time);

	/** Set the current edit mode of the timeline */
	void setEditMode(const QString & editMode);

	/** Returns the edit mode of the timeline. */
	const QString & editMode() const;

	/** Returns the correct "time under mouse", taking into account whether or not snap to frame is on or off, and other relevant effects. */
	GenTime timeUnderMouse(double posX);

	/**
	Takes the value that we wish to find the coordinate for, and returns the x
	coordinate. In cases where a single value covers multiple pixels, the left-most
	pixel is returned.
	*/
	double mapValueToLocal(double value) const;

	/** This method maps a local coordinate value to the corresponding
	value that should be represented at that position. By using this, there is no need to
	calculate scale factors yourself. Takes the x coordinate, and returns the value associated
	with it.
	*/
	double mapLocalToValue(double coordinate) const;

	/**
	Returns true if we should snap to clip borders
	*/
	bool snapToBorders() const;
	/**
	Returns true if we snap to frames
	*/
	bool snapToFrame() const;
	/**
	Set whether or not we snap to frames.
	*/
	void setSnapToFrame(bool snapToFrame);

	/**
	Set whether or not we snap to borders.
	*/
	void setSnapToBorder(bool snapToBorder);

	/**
	Set whether or not we snap to markerss.
	*/
	void setSnapToMarker(bool snapToMarker);

	/**
	Returns true if we snap to seek times
	*/
	bool snapToSeekTime() const;
	/**
	Returns true if we snap to markers.
	*/
	bool snapToMarkers() const;

	/** Return the current length of the project */
	GenTime projectLength() const;

	KTrackView *trackView() {
	    return m_trackViewArea;
	}
	/** Return the scroll threshold - the number of pixels at either side of the timeline that will start the display scrolling. */
	    uint scrollThreshold() const;

	/** Return the timer delay - the number of milliseconds between "paging" increments. */
	uint scrollTimerDelay() const;

	/** If we are in the scrolling region, we start the scroll timer, else we stop it */
	void checkScrolling(const QPoint & pos);

	/** Stop the scroll timer */
	void stopScrollTimer();

	/** Set the width of the panels that accompany the timeline's tracks. This also affects the width
	of the ruler widget and the scrollbar widget.*/
	void setPanelWidth(int width);
        
        void placeScrollBar(int pos);
        int scrollBarPosition();

	/** Return the list of all timeline guides */
	QValueList < int > timelineGuides();

    protected:
	/** @returns the ruler tool widget. */
	 QWidget * rulerToolWidget() const {
	    return m_rulerToolWidget;
	}
	/** @returns the frames Per Second of this timeline. */
	    double framesPerSecond() const {
	    return m_framesPerSecond;
	}
	/** Returns the width of the view area. The view area is the area of the timeline where clips and tracks reside.
	  * @returns the width of the view area. */
	    int viewWidth() const;


    private:
	/** GUI elements */
	 QHBox * m_rulerBox;	// Horizontal box holding the ruler
	QScrollView *m_trackScroll;	// Scrollview holding the tracks
	QHBox *m_scrollBox;	// Horizontal box holding the horizontal scrollbar.
	/** A custom widget that can appear to the side of the ruler. */
	QWidget *m_rulerToolWidget;
	KScalableRuler *m_ruler;
	QWidget *m_scrollToolWidget;	// This widget is supplied by the constructor and appears to the left of the bottom scrollbar.
	QScrollBar *m_scrollBar;	// this scroll bar's movement is measured in pixels, not frames.
	/** track varables */
	 QPtrList < KTrackPanel > m_trackList;

	/** The track view area is the area under the ruler where tracks are displayed. */
	KTrackView *m_trackViewArea;

	/** Timer for timeline scroll functionality */
	QTimer m_scrollTimer;

	/** Controls scroll direction. */
	bool m_scrollingRight;

	/** The frames-per-second that the timeline works at. */
	double m_framesPerSecond;
	// We snap to frame if this is true;
	bool m_snapToFrame;
	bool m_snapToBorder;
	bool m_snapToMarker;

	/** Current edit mode */
	QString m_editMode;

	/** The width of the panels at the left hand side of the timeline. */
	int m_panelWidth;

	/** difference between midpoint and inpoint/outpoint when inpoint or outpoint moved */
	GenTime m_midPoint;

    public slots:		// Public slots
	/** Update the back buffer for the track views, and tell the trackViewArea widget to
	repaint itself. */
	void drawTrackViewBackBuffer();

	/** Invalidates the entire back buffer. */
	void invalidateBackBuffer();

	/** Sets a new time scale for the timeline. This in turn calls the correct kruler funtion and
	updates the display. */
	void setTimeScale(double scale);
        
        /** Get the current timescale */
        double timeScale();
        
	/** Set the length of the project */
	void slotSetProjectLength(const GenTime & size);
	/** A ruler slider has moved - do something! */
	void slotSliderMoved(int slider, int value);
	/** Seek the timeline to the current position. */
	void seek(const GenTime & time);
	//move horizontal slider to the current position based on inpoint/outpoint -reh
	void horizontalSlider(const GenTime & inpoint, const GenTime & outpoint);

	/** Scroll the timeline left */
	void slotScrollLeft();
	/** Scroll the timeline Right */
	void slotScrollRight();
	void slotScrollUp();
	void slotScrollDown();
	void deleteGuide();
	void addGuide();

	/** Set the number of frames per second */
	void slotSetFramesPerSecond(double fps);

	/** Re-syncs the scrollbar project size with the ruler project size. */
	void resetProjectSize();

	//set inpoint/outpoint -reh
	void setInpointTimeline(const GenTime & inpoint);
	void setOutpointTimeline(const GenTime & outpoint);
	//get difference between inpoint or outpoint and midpoint when inpoint/outpoint slider moved
	void setMidValueDiff();
	/** automatically scroll the timeline while playing */
	void autoScroll();
	/** automatically scroll the timeline to make sure cursor is visible*/
	void ensureCursorVisible();
	void slotHeaderRightButtonPressed();

    private slots:		// Private slots
	/** Scroll the timeline by a set amount. Should be connected to m_scrollTimer */
	void slotTimerScroll();
	/** Collapse selected track */
	void collapseTrack(KTrackPanel * panel, bool);
        void slotMoveForward(bool fast);
        void slotMoveBackward(bool fast);
        
    signals:		// Signals
	/** emitted when the length of the project has changed. */
	void projectLengthChanged(int);
	/** Emitted when the seek position on the timeline changes. */
	void seekPositionChanged(const GenTime &);
	//Emitted when the inpoint changes
	void inpointPositionChanged(const GenTime &);
	//Emitted when the outpoint changes
	void outpointPositionChanged(const GenTime &);
	/** Emitted when the right mouse button is pressed over the timeline. */
	void rightButtonPressed();
	/** Emitted when the right mouse button is pressed over the tracks header. */
	void headerRightButtonPressed();
	/** Emitted when the right mouse button is pressed over the timeline ruler. */
	void rulerRightButtonPressed();
    };

}				// namespace Gui

#endif
