/***************************************************************************
                          trackpanelmarkerfunction  -  description
                             -------------------
    begin                : Thu Nov 27 2003
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
#ifndef TRACKPANELMARKERFUNCTION_H
#define TRACKPANELMARKERFUNCTION_H

#include <trackpanelfunction.h>

class QMouseEvent;

class KdenliveDoc;
class DocTrackBase;

namespace Gui {
    class KdenliveApp;
    class KTimeLine;
}
/**
@author Jason Wood
*/ class TrackPanelMarkerFunction:public TrackPanelFunction
{
  Q_OBJECT public:
    TrackPanelMarkerFunction(Gui::KdenliveApp * app,
	Gui::KTimeLine * timeline, KdenliveDoc * document);

    ~TrackPanelMarkerFunction();

    virtual bool mouseApplies(Gui::KTrackPanel * panel,
	QMouseEvent * event) const;
    virtual bool mouseMoved(Gui::KTrackPanel * panel, QMouseEvent * event);
    virtual bool mousePressed(Gui::KTrackPanel * panel,
	QMouseEvent * event);

	/** Processes Mouse double click.*/
    virtual bool mouseDoubleClicked(Gui::KTrackPanel * panel, QMouseEvent * event);

    virtual bool mouseReleased(Gui::KTrackPanel * panel,
	QMouseEvent * event);
    virtual QCursor getMouseCursor(Gui::KTrackPanel * panel,
	QMouseEvent * event);
  private:
     Gui::KdenliveApp * m_app;
     Gui::KTimeLine * m_timeline;
    KdenliveDoc *m_document;
};

#endif
