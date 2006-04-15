/***************************************************************************
                          doctrackdecorator  -  description
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
#ifndef DOCTRACKDECORATOR_H
#define DOCTRACKDECORATOR_H

#include <trackviewdecorator.h>

namespace Gui {

/**
An abstract class for all decorators that implement a view based on a DocTrack.

@author Jason Wood
*/
    class DocTrackDecorator:public TrackViewDecorator {
      public:
	DocTrackDecorator(KTimeLine * timeline, KdenliveDoc * doc);

	~DocTrackDecorator();
      protected:
	KdenliveDoc * document() {
	    return m_document;
      } 
        private:
	 KdenliveDoc * m_document;
    };

}				// namespace Gui

#endif
