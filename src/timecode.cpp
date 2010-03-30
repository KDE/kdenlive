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
#include <klocale.h>

Timecode::Timecode(Formats format, double framesPerSecond, bool dropFrame) :
        m_format(format),
        m_dropFrame(dropFrame),
        m_displayedFramesPerSecond(framesPerSecond + 0.5),
        m_realFps(framesPerSecond)
{
}

Timecode::~Timecode()
{
}

double Timecode::fps() const
{
    return m_realFps; //m_displayedFramesPerSecond;
}

bool Timecode::df() const
{
    return m_dropFrame;
}

QString Timecode::inputMask() const
{
    if (m_dropFrame) return "99:99.99:99";
    return "99:99:99:99";
}

QString Timecode::reformatSeparators(QString duration) const
{
    if (m_dropFrame) {
        return duration.replace(5, 1, '.');
    }
    return duration.replace(5, 1, ':');
}

int Timecode::getDisplayFrameCount(const QString duration, bool frameDisplay) const
{
    if (frameDisplay) return duration.toInt();
    return getFrameCount(duration);
}

int Timecode::getFrameCount(const QString duration) const
{
    if (m_dropFrame) {
        //Get Hours, Minutes, Seconds, Frames from timecode
        int hours, minutes, seconds, frames;

        hours = duration.section(':', 0, 0).toInt();
        if (duration.contains('.')) {
            minutes = duration.section('.', 0, 0).section(':', 1, 1).toInt();
            seconds = duration.section('.', 1, 1).section(':', 0, 0).toInt();
            frames = duration.section('.', 1, 1).section(':', 1, 1).toInt();
        } else {
            //Handle Drop Frame timecode frame calculations, even if the timecode supplied uses incorrect "99:99:99:99" format instead of "99:99.99:99"
            minutes = duration.section(':', 1, 1).toInt();
            seconds = duration.section(':', 2, 2).toInt();
            frames = duration.section(':', 3, 3).toInt();
        }

        //Calculate the frame count
        int dropRate = (int)((ceil(m_displayedFramesPerSecond) / 30) * 2);
        frames += ((hours * 60 + minutes) * 60 + seconds) * m_displayedFramesPerSecond;
        frames -= dropRate * ((hours * 60 + minutes) - (floor((hours * 60 + minutes) / 10)));
        return frames;
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
    if (m_dropFrame)
        return getTimecodeDropFrame(time);

    return getTimecodeHH_MM_SS_FF((int) time.frames(m_realFps));
}

const QString Timecode::getTimecodeHH_MM_SS_FF(int frames) const
{
    if (m_dropFrame) {
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
    if (m_dropFrame) {
        text.append('.');
    } else {
        text.append(':');
    }
    text.append(QString::number(seconds).rightJustified(2, '0', false));
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

QString Timecode::getTimecodeDropFrame(int frames) const
{
    // Calculate the timecode using dropframes to remove the difference in fps. Note that this algorithm should work
    // for NTSC times, but is untested for any others - it is in no way an "official" algorithm, unless it's by fluke.

    // calculate how many frames need to be dropped every minute.
    int dropRate = 0;
    if (m_dropFrame) {
        dropRate = (int)((ceil(m_displayedFramesPerSecond) / 30) * 2);
    }

    // calculate how many frames are in a normal minute, and how many are in a tenth minute.
    int normalMinuteFrames = (m_displayedFramesPerSecond * 60) - dropRate;
    int tenthMinuteFrames = (m_displayedFramesPerSecond * 60);

    // Number of actual frames in a 10 minute interval :
    int tenMinutes = (normalMinuteFrames * 9) + tenthMinuteFrames;

    int tenMinuteIntervals = frames / tenMinutes;
    frames = frames % tenMinutes;

    int hours = tenMinuteIntervals / 6;
    tenMinuteIntervals = tenMinuteIntervals % 6;

    // At the point, we have figured out HH:M?:??:??

    int numMinutes;

    if (frames < tenthMinuteFrames) {
        // tenth minute logic applies.
        numMinutes = 0;
    } else {
        // normal minute logic applies.
        numMinutes = 1 + (frames - tenthMinuteFrames) / normalMinuteFrames;
        frames = (frames - tenthMinuteFrames) % normalMinuteFrames;
        frames +=  dropRate;
    }
    // We now have HH:MM:??:??

    int seconds = frames / m_displayedFramesPerSecond;
    frames = frames % m_displayedFramesPerSecond;

    // We now have HH:MM:SS:FF

    // THANK FUCK FOR THAT.

    QString text;
    text.append(QString::number(hours).rightJustified(2, '0', false));
    text.append(':');
    text.append(QString::number(tenMinuteIntervals));
    text.append(QString::number(numMinutes));
    if (m_dropFrame) {
        text.append('.');
    } else {
        text.append(':');
    }
    text.append(QString::number(seconds).rightJustified(2, '0', false));
    text.append(':');
    text.append(QString::number(frames).rightJustified(2, '0', false));

    return text;
}
