/***************************************************************************
                          ktrackpanel  -  description
                             -------------------
    begin                : Wed Jan 7 2004
    copyright            : (C) 2004 by Jason Wood
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
#ifndef KTRACKPANEL_H
#define KTRACKPANEL_H

#include <qhbox.h>




class KPlacer;
class KTimeLine;

class TrackPanelFunction;
class TrackViewDecorator;

/**
Abstract baseclass for track panels. Provides the interface that a track panel must implement. A KTrackPanel controls a track on the timline. It provides the track dialog that appears to the left of the track, and implements the display and functionality that occurs on the track itself.

@author Jason Wood
*/
class KTrackPanel : public QHBox {
	Q_OBJECT
public:
    KTrackPanel(KTimeLine *timeline,
					KPlacer *placer,
                              		QWidget *parent,
                              		const char *name);

    virtual ~KTrackPanel();

    /** Returns true if this track panel has a document track index. */
    virtual bool hasDocumentTrackIndex() const;

    /** Returns the track index into the underlying document model used by this track. Returns -1 if this is inapplicable. */
    virtual int documentTrackIndex()  const;

    virtual void drawToBackBuffer( QPainter &painter, QRect &rect );

	/**
	Returns the names of those functions that exist for the given mode.
	*/
	QStringList applicableFunctions(const QString &mode);

protected:
	/**
	Add a TrackPanelFunction decorator to this panel. By adding decorators, we give the
	class it's desired functionality.
	*/
	void addFunctionDecorator(const QString &mode, const QString &function);

	/**
	Adds a new TrackViewDecorator to this panel. Each decorator adds it's own draw commands to each clip,
	so you can piece together what it is you want to draw.
	*/
	void addViewDecorator(TrackViewDecorator *view);

    	KTimeLine *timeline() { return m_timeline; }
private:
	/** The KMMTrackPanel needs access to various methods from it's parents Timeline.
	 *  The parent timeline is stored in this variable. */
	KTimeLine *m_timeline;

	QPtrList<TrackViewDecorator> m_trackViewDecorators;

	/** A map of lists of track panel functions. */
	QMap<QString , QStringList> m_trackPanelFunctions;

	/** The currently applied function. This lasts from mousePressed
		until mouseRelease. */
	TrackPanelFunction *m_function;

	/** A placer defines the physical layout of a track, what clips are on it and how functions should know what is on it and where. */
	KPlacer *m_placer;
};

#endif
