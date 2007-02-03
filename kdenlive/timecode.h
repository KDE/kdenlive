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
#ifndef TIMECODE_H
#define TIMECODE_H

#include <qstring.h>

#include "gentime.h"

/**
Handles the conversion of a GenTime into a nicely formatted string, taking into account things such as drop frame if necessary. Handles multiple formats, such as HH:MM:SS:FF, HH:MM:SS:F, All Frames, All Seconds, etc.

@author Jason Wood
*/
class Timecode {
  public:
    enum Formats { HH_MM_SS_FF, HH_MM_SS_HH, Frames, Seconds };

     Timecode(Formats format = HH_MM_SS_FF, int framesPerSecond =
	25, bool dropFrame = false);

	/** Set the current timecode format; this is the output format for this timecode. */
    void setFormat(int framesPerSecond, bool dropFrame = false, Formats format = HH_MM_SS_FF) {
	m_displayedFramesPerSecond = framesPerSecond;
	m_dropFrame = dropFrame;
	m_format = format;
    } 

    Formats format() const {
	return m_format;
    } 

    ~Timecode();

	/** Returns the timecode for a given time */
    QString getTimecode(const GenTime & time, double fps) const;
    int getFrameNumber(const QString duration, double fps) const;
    static QString getEasyTimecode(const GenTime & time, const double &fps);
  private:
    Formats m_format;
    bool m_dropFrame;
    int m_displayedFramesPerSecond;

    QString getTimecodeHH_MM_SS_FF(const GenTime & time, double fps) const;
    QString getTimecodeHH_MM_SS_HH(const GenTime & time) const;
    QString getTimecodeFrames(const GenTime & time, double fps) const;
    QString getTimecodeSeconds(const GenTime & time) const;
    QString getTimecodeDropFrame(const GenTime & time, double fps) const;
};

#endif
