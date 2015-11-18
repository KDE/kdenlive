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

GenTime::GenTime()
{
    m_time = 0.0;
}

GenTime::GenTime(double seconds)
{
    m_time = seconds;
}

GenTime::GenTime(int frames, double framesPerSecond)
{
    m_time = (double) frames / framesPerSecond;
}

double GenTime::seconds() const
{
    return m_time;
}

double GenTime::ms() const
{
    return m_time * 1000;
}

double GenTime::frames(double framesPerSecond) const
{
    return floor(m_time * framesPerSecond + 0.5);
}

QString GenTime::toString() const
{
    return QStringLiteral("%1 s").arg(m_time, 0, 'f', 2);
}
