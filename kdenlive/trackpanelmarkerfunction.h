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
class KMMTimeLine;
class DocTrackBase;

/**
@author Jason Wood
*/
class TrackPanelMarkerFunction : public TrackPanelFunction
{
Q_OBJECT
public:
    TrackPanelMarkerFunction(KMMTimeLine *timeline, DocTrackBase *docTrack, KdenliveDoc *document);

    ~TrackPanelMarkerFunction();

    virtual bool mouseApplies(QMouseEvent* event) const;
    virtual bool mouseMoved(QMouseEvent* event);
    virtual bool mousePressed(QMouseEvent* event);
    virtual bool mouseReleased(QMouseEvent* event);
    virtual QCursor getMouseCursor(QMouseEvent* event);
private:
	KMMTimeLine *m_timeline;
	KdenliveDoc *m_document;
	DocTrackBase *m_docTrack;
};

#endif
