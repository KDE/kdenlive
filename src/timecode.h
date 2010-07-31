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

class QValidator;
class QRegExpValidator;

/**
Handles the conversion of a GenTime into a nicely formatted string, taking into account things such as drop frame if necessary. Handles multiple formats, such as HH:MM:SS:FF, HH:MM:SS:F, All Frames, All Seconds, etc.

@author Jason Wood
*/
class Timecode
{
public:
    enum Formats { HH_MM_SS_FF, HH_MM_SS_HH, Frames, Seconds };

    explicit Timecode(Formats format = HH_MM_SS_FF, double framesPerSecond = 25,
                      bool dropFrame = false);

    /**
     * Set the current timecode format; this is the output format for this timecode.
     */
    void setFormat(double framesPerSecond, bool dropFrame = false,
                   Formats format = HH_MM_SS_FF);

    Formats format() const {
        return m_format;
    }

    ~Timecode();

    /** Returns the timecode for a given time */
    QString getDisplayTimecode(const GenTime & time, bool frameDisplay) const;
    QString getTimecode(const GenTime & time) const;
    int getDisplayFrameCount(const QString duration, bool frameDisplay) const;
    int getFrameCount(const QString duration) const;
    static QString getEasyTimecode(const GenTime & time, const double &fps);
    static QString getStringTimecode(int frames, const double &fps);
    const QString getDisplayTimecodeFromFrames(int frames, bool frameDisplay) const;
    const QString getTimecodeFromFrames(int frames) const;
    double fps() const;
    bool df() const;
    const QValidator *validator() const;
    QString reformatSeparators(QString duration) const;

private:
    Formats m_format;
    bool m_dropFrame;
    int m_displayedFramesPerSecond;
    double m_realFps;
    double m_dropFrames;
    QRegExpValidator *m_validator;

    const QString getTimecodeHH_MM_SS_FF(const GenTime & time) const;
    const QString getTimecodeHH_MM_SS_FF(int frames) const;

    QString getTimecodeHH_MM_SS_HH(const GenTime & time) const;
    QString getTimecodeFrames(const GenTime & time) const;
    QString getTimecodeSeconds(const GenTime & time) const;
    QString getTimecodeDropFrame(const GenTime & time) const;
    QString getTimecodeDropFrame(int framenumber) const;
};

#endif
