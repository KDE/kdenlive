/***************************************************************************
                          timecode  -  description
                             -------------------
    begin                : Wed Dec 17 2003
    copyright            : (C) 2003 by Jason Wood
    email                : jasonwood@blueyonder.co.uk
 *   Copyright (C) 2010 by Jean-Baptiste Mardelle (jb@kdenlive.org)        *
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

/*

 Timecode calculation code for reference
 If we ever use Quicktime timecode with 50.94 Drop frame, keep in mind that there is a bug in the Quicktime code

//CONVERT A FRAME NUMBER TO DROP FRAME TIMECODE
//Code by David Heidelberger, adapted from Andrew Duncan
//Given an int called framenumber and a double called framerate
//Framerate should be 29.97, 59.94, or 23.976, otherwise the calculations will be off.

int d;
int m;

int dropFrames = round(framerate * .066666); //Number of frames to drop on the minute marks is the nearest integer to 6% of the framerate
int framesPerHour = round(framerate*60*60); //Number of frames in an hour
int framesPer24Hours = framesPerHour*24; //Number of frames in a day - timecode rolls over after 24 hours
int framesPer10Minutes = round(framerate * 60 * 10); //Number of frames per ten minutes
int framesPerMinute = round(framerate)*60)-  dropFrames; //Number of frames per minute is the round of the framerate * 60 minus the number of dropped frames

if (framenumber<0) //Negative time. Add 24 hours.
{
    framenumber=framesPer24Hours+framenumber;
}

//If framenumber is greater than 24 hrs, next operation will rollover clock
framenumber = framenumber % framesPer24Hours; //% is the modulus operator, which returns a remainder. a % b = the remainder of a/b

d = framenumber\framesPer10Minutes; // \ means integer division, which is a/b without a remainder. Some languages you could use floor(a/b)
m = framenumber % framesPer10Minutes;

if (m>1)
{
    framenumber=framenumber + (dropFrames*9*d) + dropFrames*((m-dropFrames)\framesPerMinute);
}
else
{
    framenumber = framenumber + dropFrames*9*d;
}

int frRound = round(framerate);
int frames = framenumber % frRound;
int seconds = (framenumber \ frRound) % 60;
int minutes = ((framenumber \ frRound) \ 60) % 60;
int hours = (((framenumber \ frRound) \ 60) \ 60);

------------------------------------------------------------------------------------

//CONVERT DROP FRAME TIMECODE TO A FRAME NUMBER
//Code by David Heidelberger, adapted from Andrew Duncan
//Given ints called hours, minutes, seconds, frames, and a double called framerate

int dropFrames = round(framerate*.066666); //Number of drop frames is 6% of framerate rounded to nearest integer
int timeBase = round(framerate); //We don’t need the exact framerate anymore, we just need it rounded to nearest integer

int hourFrames = timeBase*60*60; //Number of frames per hour (non-drop)
int minuteFrames = timeBase*60; //Number of frames per minute (non-drop)
int totalMinutes = (60*hours) + minutes; //Total number of minutes
int frameNumber = ((hourFrames * hours) + (minuteFrames * minutes) + (timeBase * seconds) + frames) - (dropFrames * (totalMinutes - (totalMinutes \ 10)));
return frameNumber;

*/

#include "timecode.h"

#include "kdenlive_debug.h"

Timecode::Timecode(Formats format, double framesPerSecond)
{
    setFormat(framesPerSecond, format);
}

Timecode::~Timecode() = default;

void Timecode::setFormat(double framesPerSecond, Formats format)
{
    m_displayedFramesPerSecond = (int)(framesPerSecond + 0.5);
    m_dropFrameTimecode = qFuzzyCompare(framesPerSecond, 30000.0/1001.0);
    m_format = format;
    m_realFps = framesPerSecond;
    if (m_dropFrameTimecode) {
        m_dropFrames = round(m_realFps * .066666);     // Number of frames to drop on the minute marks is the nearest integer to 6% of the framerate
        m_framesPer10Minutes = round(m_realFps * 600); // Number of frames per ten minutes
    }
}

Timecode::Formats Timecode::format() const
{
    return m_format;
}

double Timecode::fps() const
{
    return m_realFps;
}

const QString Timecode::mask(const GenTime &t) const
{
    if (t < GenTime()) {
        if (m_dropFrameTimecode) {
            return QStringLiteral("#99:99:99,99");
        }
        return QStringLiteral("#99:99:99:99");
    }
    if (m_dropFrameTimecode) {
        return QStringLiteral("99:99:99,99");
    }
    return QStringLiteral("99:99:99:99");
}

QString Timecode::reformatSeparators(QString duration) const
{
    if (m_dropFrameTimecode) {
        return duration.replace(8, 1, ';');
    }
    return duration.replace(8, 1, ':');
}

int Timecode::getFrameCount(const QString &duration) const
{
    if (duration.isEmpty()) {
        return 0;
    }
    int hours, minutes, seconds, frames;
    int offset = 0;
    if (duration.at(0) == '-') {
        offset = 1;
        hours = duration.midRef(1, 2).toInt();
    } else {
        hours = duration.leftRef(2).toInt();
    }
    minutes = duration.midRef(3 + offset, 2).toInt();
    seconds = duration.midRef(6 + offset, 2).toInt();
    frames = duration.rightRef(2).toInt();
    if (m_dropFrameTimecode) {
        // CONVERT DROP FRAME TIMECODE TO A FRAME NUMBER
        // Code by David Heidelberger, adapted from Andrew Duncan
        // Given ints called hours, minutes, seconds, frames, and a double called framerate

        int totalMinutes = (60 * hours) + minutes; // Total number of minutes
        int frameNumber =
            ((m_displayedFramesPerSecond * 3600 * hours) + (m_displayedFramesPerSecond * 60 * minutes) + (m_displayedFramesPerSecond * seconds) + frames) -
            (m_dropFrames * (totalMinutes - floor(totalMinutes / 10)));
        return frameNumber;
    }
    return (int)((hours * 3600.0 + minutes * 60.0 + seconds) * m_realFps + frames);
}

QString Timecode::getDisplayTimecode(const GenTime &time, bool frameDisplay) const
{
    if (frameDisplay) {
        return QString::number((int)time.frames(m_realFps));
    }
    return getTimecode(time);
}

QString Timecode::getTimecode(const GenTime &time) const
{
    switch (m_format) {
    case HH_MM_SS_FF:
        return getTimecodeHH_MM_SS_FF(time);
        break;
    case HH_MM_SS_HH:
        return getTimecodeHH_MM_SS_HH(time);
        break;
    case Frames:
        return getTimecodeFrames(time);
        break;
    case Seconds:
        return getTimecodeSeconds(time);
        break;
    default:
        qCWarning(KDENLIVE_LOG) << "Unknown timecode format specified, defaulting to HH_MM_SS_FF" << endl;
        return getTimecodeHH_MM_SS_FF(time);
    }
}

const QString Timecode::getDisplayTimecodeFromFrames(int frames, bool frameDisplay) const
{
    if (frameDisplay) {
        return QString::number(frames);
    }
    return getTimecodeHH_MM_SS_FF(frames);
}

const QString Timecode::getTimecodeFromFrames(int frames) const
{
    return getTimecodeHH_MM_SS_FF(frames);
}

// static
QString Timecode::getStringTimecode(int frames, const double &fps, bool showFrames)
{
    // Returns the timecode in an hh:mm:ss format

    bool negative = false;
    if (frames < 0) {
        negative = true;
        frames = qAbs(frames);
    }

    auto seconds = (int)(frames / fps);
    int frms = frames % (int)(fps + 0.5);
    int minutes = seconds / 60;
    seconds = seconds % 60;
    int hours = minutes / 60;
    minutes = minutes % 60;
    QString text = showFrames ? QString("%1:%2:%3.%4").arg(hours, 2, 10, QLatin1Char('0')).arg(minutes, 2, 10, QLatin1Char('0')).arg(seconds, 2, 10, QLatin1Char('0')).arg(frms, 2, 10, QLatin1Char('0')) : QString("%1:%2:%3").arg(hours, 2, 10, QLatin1Char('0')).arg(minutes, 2, 10, QLatin1Char('0')).arg(seconds, 2, 10, QLatin1Char('0'));
    if (negative) {
        text.prepend('-');
    }
    return text;
}

const QString Timecode::getTimecodeHH_MM_SS_FF(const GenTime &time) const
{
    if (m_dropFrameTimecode) {
        return getTimecodeDropFrame(time);
    }
    return getTimecodeHH_MM_SS_FF((int)time.frames(m_realFps));
}

const QString Timecode::getTimecodeHH_MM_SS_FF(int frames) const
{
    if (m_dropFrameTimecode) {
        return getTimecodeDropFrame(frames);
    }

    bool negative = false;
    if (frames < 0) {
        negative = true;
        frames = qAbs(frames);
    }

    int hours = frames / ( m_realFps * 3600 );
    frames -= floor( hours * 3600 * m_realFps );

    int minutes = frames / ( m_realFps * 60 );
    frames -= floor( minutes * 60 * m_realFps );

    int seconds = frames / m_realFps;
    frames -= ceil( seconds * m_realFps );
    QString text = QString("%1:%2:%3:%4").arg(hours, 2, 10, QLatin1Char('0')).arg(minutes, 2, 10, QLatin1Char('0')).arg(seconds, 2, 10, QLatin1Char('0')).arg(frames, 2, 10, QLatin1Char('0'));
    if (negative) {
        text.prepend('-');
    }
    return text;
}

const QString Timecode::getTimecodeHH_MM_SS_HH(const GenTime &time) const
{
    auto hundredths = (int)(time.seconds() * 100);

    bool negative = false;
    if (hundredths < 0) {
        negative = true;
        hundredths = qAbs(hundredths);
    }

    int seconds = hundredths / 100;
    hundredths = hundredths % 100;
    int minutes = seconds / 60;
    seconds = seconds % 60;
    int hours = minutes / 60;
    minutes = minutes % 60;

    QString text = QString("%1:%2:%3%5%4").arg(hours, 2, 10, QLatin1Char('0')).arg(minutes, 2, 10, QLatin1Char('0')).arg(seconds, 2, 10, QLatin1Char('0')).arg(hundredths, 2, 10, QLatin1Char('0')).arg(m_dropFrameTimecode ? QLatin1Char(',') : QLatin1Char(':'));
    if (negative) {
        text.prepend('-');
    }
    return text;
}

const QString Timecode::getTimecodeFrames(const GenTime &time) const
{
    return QString::number((int)time.frames(m_realFps));
}

const QString Timecode::getTimecodeSeconds(const GenTime &time) const
{
    return QString::number(time.seconds(), 'f');
}

const QString Timecode::getTimecodeDropFrame(const GenTime &time) const
{
    return getTimecodeDropFrame((int)time.frames(m_realFps));
}

const QString Timecode::getTimecodeDropFrame(int framenumber) const
{
    // CONVERT A FRAME NUMBER TO DROP FRAME TIMECODE
    // Based on code by David Heidelberger, adapted from Andrew Duncan
    // Given an int called framenumber and a double called framerate
    // Framerate should be 29.97, 59.94, or 23.976, otherwise the calculations will be off.

    bool negative = false;
    if (framenumber < 0) {
        negative = true;
        framenumber = qAbs(framenumber);
    }

    int d = floor(framenumber / m_framesPer10Minutes);
    int m = framenumber % m_framesPer10Minutes;

    if (m > m_dropFrames) {
        framenumber += (m_dropFrames * 9 * d) + m_dropFrames * (floor((m - m_dropFrames) / (round(m_realFps * 60) - m_dropFrames)));
    } else {
        framenumber += m_dropFrames * 9 * d;
    }

    int frames = framenumber % m_displayedFramesPerSecond;
    int seconds = (int)floor(framenumber / m_displayedFramesPerSecond) % 60;
    int minutes = (int)floor(floor(framenumber / m_displayedFramesPerSecond) / 60) % 60;
    int hours = floor(floor(floor(framenumber / m_displayedFramesPerSecond) / 60) / 60);

    QString text = QString("%1:%2:%3,%4").arg(hours, 2, 10, QLatin1Char('0')).arg(minutes, 2, 10, QLatin1Char('0')).arg(seconds, 2, 10, QLatin1Char('0')).arg(frames, 2, 10, QLatin1Char('0'));
    if (negative) {
        text.prepend('-');
    }
    return text;
}
