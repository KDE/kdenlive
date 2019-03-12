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
    m_time = (double)frames / framesPerSecond;
}

double GenTime::seconds() const
{
    return m_time;
}

double GenTime::ms() const
{
    return m_time * 1000;
}

int GenTime::frames(double framesPerSecond) const
{
    return (int)floor(m_time * framesPerSecond + 0.5);
}

QString GenTime::toString() const
{
    return QStringLiteral("%1 s").arg(m_time, 0, 'f', 2);
}

GenTime GenTime::operator-()
{
    return GenTime(-m_time);
}

GenTime &GenTime::operator+=(GenTime op)
{
    m_time += op.m_time;
    return *this;
}

GenTime &GenTime::operator-=(GenTime op)
{
    m_time -= op.m_time;
    return *this;
}

GenTime GenTime::operator+(GenTime op) const
{
    return GenTime(m_time + op.m_time);
}

GenTime GenTime::operator-(GenTime op) const
{
    return GenTime(m_time - op.m_time);
}

GenTime GenTime::operator*(double op) const
{
    return GenTime(m_time * op);
}

GenTime GenTime::operator/(double op) const
{
    return GenTime(m_time / op);
}

bool GenTime::operator<(GenTime op) const
{
    return m_time + s_delta < op.m_time;
}

bool GenTime::operator>(GenTime op) const
{
    return m_time > op.m_time + s_delta;
}

bool GenTime::operator>=(GenTime op) const
{
    return m_time + s_delta >= op.m_time;
}

bool GenTime::operator<=(GenTime op) const
{
    return m_time <= op.m_time + s_delta;
}

bool GenTime::operator==(GenTime op) const
{
    return fabs(m_time - op.m_time) < s_delta;
}

bool GenTime::operator!=(GenTime op) const
{
    return fabs(m_time - op.m_time) >= s_delta;
}

// static
void GenTime::setFps(double fps)
{
    s_delta = 0.9 / fps;
}
