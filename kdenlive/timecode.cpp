/***************************************************************************
                          timecode  -  description
                             -------------------
    begin                : Wed Dec 17 2003
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
#include "timecode.h"

#include <kdebug.h>

Timecode::Timecode(Formats format, int framesPerSecond, bool dropFrame) :
			m_format(format),
			m_displayedFramesPerSecond(framesPerSecond),
			m_dropFrame(dropFrame)
{
}


Timecode::~Timecode()
{
}

QString Timecode::getTimecode(const GenTime &time, double fps) const
{
	switch (m_format) {
		case HH_MM_SS_FF :
			return getTimecodeHH_MM_SS_FF(time, fps);
			break;
		case HH_MM_SS_HH :
			return getTimecodeHH_MM_SS_HH(time);
			break;
		case Frames :
			return getTimecodeFrames(time, fps);
			break;
		case Seconds :
			return getTimecodeSeconds(time);
			break;
		default:
			kdWarning() << "Unknown timecode format specified, defaulting to HH_MM_SS_FF" << endl;
			return getTimecodeHH_MM_SS_FF(time, fps);
	}
}

QString Timecode::getTimecodeHH_MM_SS_FF(const GenTime &time, double fps) const
{
	if(m_dropFrame) return getTimecodeDropFrame(time, fps);

	int frames = time.frames(fps);
	int seconds = frames / m_displayedFramesPerSecond;
	frames = frames % m_displayedFramesPerSecond;

	int minutes = seconds / 60;
	seconds = seconds % 60;
	int hours = minutes / 60;
	minutes = minutes % 60;

	QString text;

	text.append(QString::number(hours).rightJustify(2, '0', FALSE));
	text.append(":");
	text.append(QString::number(minutes).rightJustify(2, '0', FALSE));
	text.append(":");
  	text.append(QString::number(seconds).rightJustify(2, '0', FALSE));
	text.append(":");
  	text.append(QString::number(frames).rightJustify(2, '0', FALSE));

	return text;
}

QString Timecode::getTimecodeHH_MM_SS_HH(const GenTime &time) const
{
	int hundredths = time.seconds() * 100;
	int seconds = hundredths / 100;
	hundredths = hundredths % 100;
	int minutes = seconds / 60;
	seconds = seconds % 60;
	int hours = minutes / 60;
	minutes = minutes % 60;

	QString text;

	text.append(QString::number(hours).rightJustify(2, '0', FALSE));
	text.append(":");
	text.append(QString::number(minutes).rightJustify(2, '0', FALSE));
	text.append(":");
  	text.append(QString::number(seconds).rightJustify(2, '0', FALSE));
	text.append(":");
  	text.append(QString::number(hundredths).rightJustify(2, '0', FALSE));

	return text;
}

QString Timecode::getTimecodeFrames(const GenTime &time, double fps) const
{
	return QString::number(time.frames(fps));
}

QString Timecode::getTimecodeSeconds(const GenTime &time) const
{
	return QString::number(time.seconds());
}

QString Timecode::getTimecodeDropFrame(const GenTime &time, double fps) const
{
	// Calculate the timecode using dropframes to remove the difference in fps. Note that this algorithm should work
	// for NTSC times, but is untested for any others - it is in no way an "official" algorithm, unless it's by fluke.

	int frames = time.frames(fps);

	// calculate how many frames need to be dropped every minute.
	int toDrop = (m_displayedFramesPerSecond - fps)*600;

	int perMinute = toDrop/9;
	int tenthMinute = toDrop % 9;

	// calculate how many frames are in a normal minute, and how many are in a tenth minute.
	int normalMinuteFrames = (m_displayedFramesPerSecond * 60) - perMinute;
	int tenthMinuteFrames = (m_displayedFramesPerSecond * 60) - tenthMinute;;

	// Number of actual frames in a 10 minute interval :
	int tenMinutes = (normalMinuteFrames * 9) + tenthMinuteFrames;

	int tenMinuteIntervals = frames / tenMinutes;
	frames = frames % tenMinutes;

	int hours = tenMinuteIntervals / 6;
	tenMinuteIntervals = tenMinuteIntervals % 6;

	// At the point, we have figured out HH:M?:??:??

	int numMinutes;

	frames += tenthMinute;

	if(frames < tenthMinuteFrames) {

		// tenth minute logic applies.
		numMinutes = 0;
	} else {
		// normal minute logic applies.
		numMinutes = 1 + (frames-tenthMinuteFrames) / normalMinuteFrames;

		frames = (frames - tenthMinuteFrames) % normalMinuteFrames;
	}

	frames += tenthMinute + (perMinute * numMinutes);

	// We now have HH:MM:??:??

	int seconds = frames / m_displayedFramesPerSecond;
	frames = frames % m_displayedFramesPerSecond;

	// We now have HH:MM:SS:FF

	// THANK FUCK FOR THAT.

	QString text;

	text.append(QString::number(hours).rightJustify(2, '0', FALSE));
	text.append(":");
	text.append(QString::number(tenMinuteIntervals));
	text.append(QString::number(numMinutes));
	text.append(":");
  	text.append(QString::number(seconds).rightJustify(2, '0', FALSE));
	text.append(":");
  	text.append(QString::number(frames).rightJustify(2, '0', FALSE));

	return text;
}
