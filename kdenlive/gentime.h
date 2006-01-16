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

#include <cmath>

/**Encapsulates a time, which can be set in various forms and outputted in various forms.
  *@author Jason Wood
  */

class GenTime {
  public:
	/** Creates a time object, with a time of 0 seconds. */
    GenTime();

	/** Creates a time object, with time given in seconds. */
    explicit GenTime(double seconds);

	/** Creates a time object, by passing number of frames and how many frames per second */
     GenTime(double frames, double framesPerSecond);

	/** returns the time, in seconds */
    double seconds() const {
	return m_time;
    }
	/** Returns the time, in milliseconds */ double ms() const;

	/** Returns the time in frames, after being given the number of frames per second */
    double frames(double framesPerSecond) const;

     GenTime & operator+=(GenTime op) {
	m_time += op.m_time;
	return *this;
    }
	/** Adds two GenTimes */ GenTime operator+(GenTime op) const {
	return GenTime(m_time + op.m_time);
    }
	/** Subtracts one genTime from another */ GenTime operator-(GenTime op) const {
	return GenTime(m_time - op.m_time);
    }
	/** Multiplies one GenTime by a double value, returning a GenTime */
	GenTime operator*(double op) const {
	return GenTime(m_time * op);
    }
	/** Divides one GenTime by a double value, returning a GenTime */
	GenTime operator/(double op) const {
	return GenTime(m_time / op);
    }
    /* Implementation of < operator; Works identically as with basic types. */
	bool operator<(GenTime op) const {
	return m_time + s_delta < op.m_time;
    }
    /* Implementation of > operator; Works identically as with basic types. */
	bool operator>(GenTime op) const {
	return m_time > op.m_time + s_delta;
    }
    /* Implementation of >= operator; Works identically as with basic types. */
	bool operator>=(GenTime op) const {
	return m_time + s_delta >= op.m_time;
    }
    /* Implementation of <= operator; Works identically as with basic types. */
	bool operator<=(GenTime op) const {
	return m_time <= op.m_time + s_delta;
    }
    /* Implementation of == operator; Works identically as with basic types. */
	bool operator==(GenTime op) const {
	return fabs(m_time - op.m_time) < s_delta;
    }
    /* Implementation of != operator; Works identically as with basic types. */
	bool operator!=(GenTime op) const {
	return fabs(m_time - op.m_time) >= s_delta;
    }
    /* Rounds the GenTIme's value to the nearest frame */
	GenTime & roundNearestFrame(double framesPerSecond) {
	m_time = floor((m_time * framesPerSecond) + 0.5) / framesPerSecond;
	return *this;
    }

    ~GenTime();
  private:			// Private attributes
  /** Holds the time for this object. */
    double m_time;

  /** A delta value that is used to get around floating point rounding issues. */
    static double s_delta;
};

#endif
