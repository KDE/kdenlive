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
#include <qcursor.h>
#include <qptrlist.h>
#include <qmap.h>

#include "doctrackbase.h"
#include "ktrackclippanel.h"
#include "trackviewdecorator.h"

class KdenliveDoc;
class KTimeLine;
class TrackPanelFunction;

/**Base class for all Track panels and their associated views.
  *@author Jason Wood
  */

class KMMTrackPanel : public KTrackClipPanel  {
	Q_OBJECT
public:
	enum ResizeState {None, Start, End};

	KMMTrackPanel(KTimeLine *timeline,
			KdenliveDoc *document,
			DocTrackBase *docTrack,
			QWidget *parent,
			const char *name);
	~KMMTrackPanel();

  	/** returns the document track which is displayed by this track */
  	DocTrackBase * docTrack();

	/**
	Paints the backbuffer into the relevant place using the painter supplied. The
	track should be drawn into the area provided in area
	*/
	void drawToBackBuffer(QPainter &painter, QRect &rect);

	/** Returns true if this track panel has a document track index. */
    	virtual bool hasDocumentTrackIndex() const { return true; }

    	/** Returns the track index into the underlying document model used by this track. Returns -1 if this is inapplicable. */
    	virtual int documentTrackIndex()  const;
protected: // Protected methods
	KdenliveDoc *document() { return m_document; }
private:	// private methods
	/** The track document class that should be queried to build up this track view. */
	DocTrackBase *m_docTrack;

	/** A reference to the document this function applies to. */
	KdenliveDoc *m_document;
};

#endif
