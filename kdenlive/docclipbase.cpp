/***************************************************************************
                          DocClipBase.cpp  -  description
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

#include "docclipbase.h"

DocClipBase::DocClipBase()
{
}

DocClipBase::~DocClipBase()
{
}

/** Returns where the start of this clip is on the track it resides on. */
long DocClipBase::trackStartSeconds(){
	return m_trackStart.seconds;
}

long DocClipBase::trackStartMs() {
  return m_trackStart.ms;
}

long DocClipBase::trackStart() {
  return (m_trackStart.seconds * 1000) + m_trackStart.ms;
}

void DocClipBase::setTrackStart(long seconds, long ms)
{
}

QString DocClipBase::name() {
	return m_name;
}

long DocClipBase::durationSeconds() {
	return duration() / 1000;
}

long DocClipBase::durationMs() {
	return duration() % 1000;
}

