/***************************************************************************
                          doctrackbase.h  -  description
                             -------------------
    begin                : Fri Apr 12 2002
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

#ifndef DOCTRACKBASE_H
#define DOCTRACKBASE_H


/**DocTrackBase is a base class for all real track entries into the database.
It provides core functionality that all tracks must posess.
  *@author Jason Wood
  */

class DocTrackBase {
public: 
	DocTrackBase();
	~DocTrackBase();
};

#endif
