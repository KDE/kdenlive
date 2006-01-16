/***************************************************************************
                          time.cpp  -  description
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

#include "gentime.h"

double GenTime::s_delta = 0.00001;

/** Creates a time object, with a time of 0 seconds. */
GenTime::GenTime()
{
    m_time = 0.0;
}

/** Creates a time object, with time given in seconds. */
GenTime::GenTime(double seconds)
{
    m_time = seconds;
}

/** Creates a time object, by passing number of frames and how many frames per second */
GenTime::GenTime(double frames, double framesPerSecond)
{
    m_time = frames / framesPerSecond;
}

/** Returns the time, in milliseconds */
double GenTime::ms() const
{
    return m_time * 1000;
}

/** Returns the time in frames, after being given the number of frames per second */
double GenTime::frames(double framesPerSecond) const
{
    return m_time * framesPerSecond;
}

GenTime::~GenTime()
{
}
