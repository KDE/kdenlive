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
 If we ever use Quicktime timecode with 50.94 Drop frame, keep in mind that there is a bug inthe Quicktime code

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
int totalMinutes = (60*hours) + minutes; //Total number of minuts
int frameNumber = ((hourFrames * hours) + (minuteFrames * minutes) + (timeBase * seconds) + frames) - (dropFrames * (totalMinutes - (totalMinutes \ 10)));
return frameNumber;

*/


#include <QValidator>

#include <KDebug>
#include <KLocale>

#include "timecode.h"

Timecode::Timecode(Formats format, double framesPerSecond, bool dropFrame)
{
    m_validator = new QRegExpValidator(0);
    setFormat(framesPerSecond, dropFrame, format);
}

Timecode::~Timecode()
{
}

void Timecode::setFormat(double framesPerSecond, bool dropFrame, Formats format)
{
    m_displayedFramesPerSecond = (int)(framesPerSecond + 0.5);
    m_dropFrameTimecode = dropFrame;
    m_format = format;
    m_realFps = framesPerSecond;
    if (dropFrame) {
        m_dropFrames = round(m_realFps * .066666); //Number of frames to drop on the minute marks is the nearest integer to 6% of the framerate
        m_framesPer10Minutes = round(m_realFps * 600); //Number of frames per ten minutes
    }
    QRegExp regExp;
    if (m_dropFrameTimecode)
        regExp.setPattern("^\\d{2}:\\d{2}:\\d{2};\\d{2}$");
    else
        regExp.setPattern("^\\d{2}:\\d{2}:\\d{2}:\\d{2}$");
    m_validator->setRegExp(regExp);
}

double Timecode::fps() const
{
    return m_realFps;
}

bool Timecode::df() const
{
    return m_dropFrameTimecode;
}

const QValidator *Timecode::validator() const
{
    return m_validator;
}

QString Timecode::reformatSeparators(QString duration) const
{
    if (m_dropFrameTimecode)
        return duration.replace(8, 1, ';');
    return duration.replace(8, 1, ':');
}

int Timecode::getDisplayFrameCount(const QString duration, bool frameDisplay) const
{
    if (frameDisplay) return duration.toInt();
    return getFrameCount(duration);
}

int Timecode::getFrameCount(const QString duration) const
{
    if (m_dropFrameTimecode) {

        //CONVERT DROP FRAME TIMECODE TO A FRAME NUMBER
        //Code by David Heidelberger, adapted from Andrew Duncan
        //Given ints called hours, minutes, seconds, frames, and a double called framerate

        //Get Hours, Minutes, Seconds, Frames from timecode
        int hours, minutes, seconds, frames;

        hours = duration.section(':', 0, 0).toInt();
        minutes = duration.section(':', 1, 1).toInt();
        if (duration.contains(';')) {
            seconds = duration.section(';', 0, 0).section(':', 2, 2).toInt();
            frames = duration.section(';', 1, 1).toInt();
        } else {
            //Handle Drop Frame timecode frame calculations, even if the timecode supplied uses incorrect "99:99:99:99" format instead of "99:99:99;99"
            seconds = duration.section(':', 2, 2).toInt();
            frames = duration.section(':', 3, 3).toInt();
        }

        int totalMinutes = (60 * hours) + minutes; //Total number of minutes
        int frameNumber = ((m_displayedFramesPerSecond * 3600 * hours) + (m_displayedFramesPerSecond * 60 * minutes) + (m_displayedFramesPerSecond * seconds) + frames) - (m_dropFrames * (totalMinutes - floor(totalMinutes / 10)));
        return frameNumber;
    }
    return (int)((duration.section(':', 0, 0).toInt()*3600.0 + duration.section(':', 1, 1).toInt()*60.0 + duration.section(':', 2, 2).toInt()) * m_realFps + duration.section(':', 3, 3).toInt());
}

QString Timecode::getDisplayTimecode(const GenTime & time, bool frameDisplay) const
{
    if (frameDisplay) return QString::number((int) time.frames(m_realFps));
    return getTimecode(time);
}

QString Timecode::getTimecode(const GenTime & time) const
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
        kWarning() <<
        "Unknown timecode format specified, defaulting to HH_MM_SS_FF"
        << endl;
        return getTimecodeHH_MM_SS_FF(time);
    }
}

const QString Timecode::getDisplayTimecodeFromFrames(int frames, bool frameDisplay) const
{
    if (frameDisplay) return QString::number(frames);
    return getTimecodeHH_MM_SS_FF(frames);
}

const QString Timecode::getTimecodeFromFrames(int frames) const
{
    return getTimecodeHH_MM_SS_FF(frames);
}


//static
QString Timecode::getStringTimecode(int frames, const double &fps)
{
    // Returns the timecode in an hh:mm:ss format
    int seconds = (int)(frames / fps);
    int minutes = seconds / 60;
    seconds = seconds % 60;
    int hours = minutes / 60;
    minutes = minutes % 60;
    QString text;
    text.append(QString::number(hours).rightJustified(2, '0', false));
    text.append(':');
    text.append(QString::number(minutes).rightJustified(2, '0', false));
    text.append(':');
    text.append(QString::number(seconds).rightJustified(2, '0', false));
    return text;
}


//static
QString Timecode::getEasyTimecode(const GenTime & time, const double &fps)
{
    // Returns the timecode in an easily read display, like 3 min. 5 sec.
    int frames = (int) time.frames(fps);
    int seconds = (int)(frames / fps);
    frames = frames - ((int)(fps * seconds));

    int minutes = seconds / 60;
    seconds = seconds % 60;
    int hours = minutes / 60;
    minutes = minutes % 60;

    QString text;
    bool trim = false;

    if (hours != 0) {
        text.append(QString::number(hours).rightJustified(2, '0', false));
        text.append(' ' + i18n("hour") + ' ');
        trim = true;
    }
    if (minutes != 0 || trim) {
        if (!trim) {
            text.append(QString::number(minutes));
        } else
            text.append(QString::number(minutes).rightJustified(2, '0', false));
        text.append(' ' + i18n("min.") + ' ');
        trim = true;
    }
    if (seconds != 0 || trim) {
        if (!trim) {
            text.append(QString::number(seconds));
        } else
            text.append(QString::number(seconds).rightJustified(2, '0', false));
        text.append(' ' + i18n("sec."));
        trim = true;
    }
    if (!trim) {
        text.append(QString::number(frames));
        text.append(' ' + i18n("frames"));
    }

    return text;
}


const QString Timecode::getTimecodeHH_MM_SS_FF(const GenTime & time) const
{
    if (m_dropFrameTimecode)
        return getTimecodeDropFrame(time);

    return getTimecodeHH_MM_SS_FF((int) time.frames(m_realFps));
}

const QString Timecode::getTimecodeHH_MM_SS_FF(int frames) const
{
    if (m_dropFrameTimecode) {
        return getTimecodeDropFrame(frames);
    }
    int seconds = frames / m_displayedFramesPerSecond;
    frames = frames % m_displayedFramesPerSecond;

    int minutes = seconds / 60;
    seconds = seconds % 60;
    int hours = minutes / 60;
    minutes = minutes % 60;

    QString text;
    text.append(QString::number(hours).rightJustified(2, '0', false));
    text.append(':');
    text.append(QString::number(minutes).rightJustified(2, '0', false));
    text.append(':');
    text.append(QString::number(seconds).rightJustified(2, '0', false));
    text.append(':');
    text.append(QString::number(frames).rightJustified(2, '0', false));

    return text;
}

QString Timecode::getTimecodeHH_MM_SS_HH(const GenTime & time) const
{
    int hundredths = (int)(time.seconds() * 100);
    int seconds = hundredths / 100;
    hundredths = hundredths % 100;
    int minutes = seconds / 60;
    seconds = seconds % 60;
    int hours = minutes / 60;
    minutes = minutes % 60;

    QString text;

    text.append(QString::number(hours).rightJustified(2, '0', false));
    text.append(':');
    text.append(QString::number(minutes).rightJustified(2, '0', false));
    text.append(':');
    text.append(QString::number(seconds).rightJustified(2, '0', false));
    if (m_dropFrameTimecode)
        text.append(';');
    else
        text.append(':');
    text.append(QString::number(hundredths).rightJustified(2, '0', false));

    return text;
}

QString Timecode::getTimecodeFrames(const GenTime & time) const
{
    return QString::number(time.frames(m_realFps));
}

QString Timecode::getTimecodeSeconds(const GenTime & time) const
{
    return QString::number(time.seconds());
}

QString Timecode::getTimecodeDropFrame(const GenTime & time) const
{
    return getTimecodeDropFrame((int)time.frames(m_realFps));
}

QString Timecode::getTimecodeDropFrame(int framenumber) const
{
    //CONVERT A FRAME NUMBER TO DROP FRAME TIMECODE
    //Based on code by David Heidelberger, adapted from Andrew Duncan
    //Given an int called framenumber and a double called framerate
    //Framerate should be 29.97, 59.94, or 23.976, otherwise the calculations will be off.

    int d = floor(framenumber / m_framesPer10Minutes);
    int m = framenumber % m_framesPer10Minutes;

    if (m > 1) {
        framenumber = framenumber + (m_dropFrames * 9 * d) + m_dropFrames * (floor((m - m_dropFrames) / (round(m_realFps * 60) - m_dropFrames)));
    } else {
        framenumber = framenumber + m_dropFrames * 9 * d;
    }

    int frames = framenumber % m_displayedFramesPerSecond;
    int seconds = (int) floor(framenumber / m_displayedFramesPerSecond) % 60;
    int minutes = (int) floor(floor(framenumber / m_displayedFramesPerSecond) / 60) % 60;
    int hours = floor(floor(floor(framenumber / m_displayedFramesPerSecond) / 60) / 60);

    QString text;
    text.append(QString::number(hours).rightJustified(2, '0', false));
    text.append(':');
    text.append(QString::number(minutes).rightJustified(2, '0', false));
    text.append(':');
    text.append(QString::number(seconds).rightJustified(2, '0', false));
    text.append(';');
    text.append(QString::number(frames).rightJustified(2, '0', false));

    return text;
}
