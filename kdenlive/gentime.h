/***************************************************************************
                          time.h  -  description
                             -------------------
    begin                : Sat Sep 14 2002
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

#ifndef GENTIME_H
#define GENTIME_H

#include <arts/kmedia2.h>

/**Encapsulates a time, which can be set in various forms and outputted in various forms. 
  *@author Jason Wood
  */

class GenTime {
public:
	/** Creates a time object, with time given in seconds. */
	GenTime(double seconds);
	/** Creates a time object, with the time given by an Arts::poTime structure */
	GenTime(const Arts::poTime &time);
	/** Creates a time object, by passing number of frames and how many frames per second */
	GenTime(double frames, double framesPerSecond);

	/** returns the time, in seconds */
	double seconds();

	/** Returns the time, in milliseconds */
	double ms();

	/** Returns the time in frames, after being given the number of frames per second */
	double frames(double framesPerSecond);
	
	/** Returns the time as an Arts::poTime structure */
	Arts::poTime artsTime();
			
	~GenTime();
private: // Private attributes
  /** Holds the time for this object. */
  double m_time;
};

#endif
