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
class KdenliveApp;
class KdenliveDoc;
class KTimeLine;
class DocTrackBase;

/**
@author Jason Wood
*/
class TrackPanelMarkerFunction : public TrackPanelFunction
{
Q_OBJECT
public:
    TrackPanelMarkerFunction(KdenliveApp *app, KTimeLine *timeline, KdenliveDoc *document);

    ~TrackPanelMarkerFunction();

    virtual bool mouseApplies(KTrackPanel *panel, QMouseEvent* event) const;
    virtual bool mouseMoved(KTrackPanel *panel, QMouseEvent* event);
    virtual bool mousePressed(KTrackPanel *panel, QMouseEvent* event);
    virtual bool mouseReleased(KTrackPanel *panel, QMouseEvent* event);
    virtual QCursor getMouseCursor(KTrackPanel *panel, QMouseEvent* event);
private:
	KdenliveApp *m_app;
	KTimeLine *m_timeline;
	KdenliveDoc *m_document;
};

#endif
