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

#include <QString>

#include "gentime.h"

/**
 * Handles the conversion of a GenTime into a nicely formatted string, taking into account things such as drop frame if necessary.
 * Handles multiple formats, such as HH:MM:SS:FF, HH:MM:SS:F, All Frames, All Seconds, etc.
 *
 * For 29.97, 59.94, or 23.976 fps, the drop-frame timecode is used: For this frame rate, frames 0 and 1 are dropped every minute
 * but not every 10th to fix time shift; see https://en.wikipedia.org/wiki/SMPTE_timecode for details.
 *
 * To distinguish between normal and drop-frame timecode, they use a different format.
 *
 * HH:MM:SS:FF → Normal time code in hour/minute/second/frame format
 * HH:MM:SS,FF → Drop-frame time code
 *
 * @author Jason Wood
*/
class Timecode
{
public:
    enum Formats { HH_MM_SS_FF, HH_MM_SS_HH, Frames, Seconds };

    explicit Timecode(Formats format = HH_MM_SS_FF, double framesPerSecond = 25);

    /**
     * Set the current timecode format; this is the output format for this timecode.
     */
    void setFormat(double framesPerSecond, Formats format = HH_MM_SS_FF);

    Formats format() const;

    ~Timecode();

    /** Returns the timecode for a given time */
    QString getDisplayTimecode(const GenTime &time, bool frameDisplay) const;
    QString getTimecode(const GenTime &time) const;
    int getFrameCount(const QString &duration) const;
    static QString getStringTimecode(int frames, const double &fps, bool showFrames = false);
    const QString getDisplayTimecodeFromFrames(int frames, bool frameDisplay) const;
    const QString getTimecodeFromFrames(int frames) const;
    double fps() const;
    const QString mask(const GenTime &t = GenTime()) const;
    QString reformatSeparators(QString duration) const;

private:
    Formats m_format;
    bool m_dropFrameTimecode;
    int m_displayedFramesPerSecond;
    double m_realFps;
    double m_dropFrames;
    int m_framesPer10Minutes;

    const QString getTimecodeHH_MM_SS_FF(const GenTime &time) const;
    const QString getTimecodeHH_MM_SS_FF(int frames) const;

    const QString getTimecodeHH_MM_SS_HH(const GenTime &time) const;
    const QString getTimecodeFrames(const GenTime &time) const;
    const QString getTimecodeSeconds(const GenTime &time) const;
    const QString getTimecodeDropFrame(const GenTime &time) const;
    const QString getTimecodeDropFrame(int framenumber) const;
};

#endif
