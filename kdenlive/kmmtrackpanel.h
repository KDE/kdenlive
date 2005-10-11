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
#include "ktrackpanel.h"
#include "trackviewdecorator.h"

class KdenliveDoc;
class TrackPanelFunction;

namespace Gui
{
	class KTimeLine;
	class KPlacer;

/**Base class for all Track panels and their associated views.
  *@author Jason Wood
  */

class KMMTrackPanel : public KTrackPanel  {
	Q_OBJECT
public:
	enum ResizeState {None, Start, End};

	KMMTrackPanel(KTimeLine *timeline,
			KdenliveDoc *document,
			KPlacer *placer,
			QWidget *parent,
			const char *name);
	virtual ~KMMTrackPanel();

	/**
	Paints the backbuffer into the relevant place using the painter supplied. The
	track should be drawn into the area provided in area
	*/
	void drawToBackBuffer(QPainter &painter, QRect &rect);
protected: // Protected methods
	KdenliveDoc *document() { return m_document; }
private:	// private methods

	/** A reference to the document this function applies to. */
	KdenliveDoc *m_document;
};

} // namespace Gui

#endif
