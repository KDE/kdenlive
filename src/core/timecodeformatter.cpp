/*
Copyright (C) 2012  Till Theato <root@ttill.de>
This file is part of Kdenlive. See www.kdenlive.org.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
*/

#include "timecodeformatter.h"
#include "core.h"
#include "timecode.h"
#include "kdenlivesettings.h"
#include <KLocale>
#include <math.h>


TimecodeFormatter::TimecodeFormatter(const Fraction& framerate, TimecodeFormatter::Formats defaultFormat, QObject *parent) :
    QObject(parent)
{
    setFramerate(framerate);
    setDefaultFormat(defaultFormat);
}

void TimecodeFormatter::setFramerate(const Fraction& framerate)
{
    m_framerate = framerate;

    m_isDropFrame = m_framerate.numerator % m_framerate.denominator;
    if (m_isDropFrame) {
        m_dropFrames = qRound(m_framerate * .066666);
        m_framesPer10Minutes = qRound(m_framerate * 600);
    }

    emit framerateChanged();
}

Fraction TimecodeFormatter::framerate() const
{
    return m_framerate;
}

void TimecodeFormatter::setDefaultFormat(TimecodeFormatter::Formats format)
{
    if (format == DefaultFormat) {
        m_defaultFormat = KdenliveSettings::frametimecode() ? Seconds : HH_MM_SS_FF;
    } else {
        m_defaultFormat = format;
    }

    emit defaultFormatChanged();
}

TimecodeFormatter::Formats TimecodeFormatter::defaultFormat() const
{
    return m_defaultFormat;
}

QString TimecodeFormatter::format(const Timecode& timecode, TimecodeFormatter::Formats format) const
{
    if (format == DefaultFormat) {
        format = m_defaultFormat;
    }

    switch (format) {
        case HH_MM_SS_FF:
            return formatHH_MM_SS_FF(timecode);
        case HH_MM_SS_HH:
            return formatHH_MM_SS_HH(timecode);
        case Frames:
            return formatFrames(timecode);
        case Seconds:
            return formatSeconds(timecode);
        case Milliseconds:
            return formatMilliseconds(timecode);
        case Time:
            return formatTime(timecode);
        default:
            return QString();
    }
}

QString TimecodeFormatter::mask(const Timecode& timecode) const
{
    QString mask;
    if (timecode < 0) {
        mask.append("#");
    }
    if (m_isDropFrame) {
        mask.append("99:99:99,99");
    } else {
        mask.append("99:99:99:99");
    }
    return mask;
}

Timecode TimecodeFormatter::fromString(const QString& timecode, TimecodeFormatter::Formats format) const
{
    if (format == DefaultFormat) {
        format = m_defaultFormat;
    }

    switch (format) {
        case HH_MM_SS_FF:
            return fromHH_MM_SS_FF(timecode);
        case HH_MM_SS_HH:
            return fromHH_MM_SS_HH(timecode);
        case Frames:
            return fromFrames(timecode);
        case Seconds:
            return fromSeconds(timecode);
        case Milliseconds:
            return fromMilliseconds(timecode);
        case Time:
            return fromTime(timecode);
        default:
            return Timecode(this);
    }
}

QString TimecodeFormatter::formatHH_MM_SS_FF(const Timecode& timecode) const
{
    int frames = timecode.frames();

    bool isNegative = false;
    if (frames < 0) {
        isNegative = true;
        frames = qAbs(frames);
    }

    if (m_isDropFrame) {
        int d = frames / m_framesPer10Minutes;
        int m = frames % m_framesPer10Minutes;

        frames += m_dropFrames * 9 * d;
        if (m > m_dropFrames) {
            frames += m_dropFrames * floor((m - m_dropFrames) / (round(m_framerate * 60) - m_dropFrames));
        }
    }

    int seconds;
    int minutes;
    int hours;
    fillComponents(qRound(m_framerate.value()), frames, seconds, minutes, hours);

    return fillMask(hours, minutes, seconds, frames, isNegative);
}

QString TimecodeFormatter::formatHH_MM_SS_HH(const Timecode& timecode) const
{
    int hundredths = timecode.seconds() * 100;

    bool isNegative = false;
    if (hundredths < 0) {
        isNegative = true;
        hundredths = qAbs(hundredths);
    }

    int seconds;
    int minutes;
    int hours;
    fillComponents(100, hundredths, seconds, minutes, hours);

    return fillMask(hours, minutes, seconds, hundredths, isNegative);
}

QString TimecodeFormatter::formatFrames(const Timecode& timecode) const
{
    return QString::number(timecode.frames());
}

QString TimecodeFormatter::formatSeconds(const Timecode& timecode) const
{
    QLocale locale;
    return locale.toString(timecode.seconds());
}

QString TimecodeFormatter::formatMilliseconds(const Timecode& timecode) const
{
    QLocale locale;
    return locale.toString(timecode.milliseconds());
}

QString TimecodeFormatter::formatTime(const Timecode& timecode) const
{
    int frames = timecode.frames();

    bool isNegative = false;
    if (frames < 0) {
        isNegative = true;
        frames = qAbs(frames);
    }

    // FIXME: needs dropframe adjustments

    int seconds;
    int minutes;
    int hours;
    fillComponents(qRound(m_framerate.value()), frames, seconds, minutes, hours);

    QString text;
    bool trim = false;

    if (isNegative)
        text.append('-');

    if (hours) {
        text.append(QString::number(hours).rightJustified(2, '0', false));
        text.append(' ' + i18np("hour", "hours", hours) + ' ');
        trim = true;
    }
    if (minutes || trim) {
        if (!trim) {
            text.append(QString::number(minutes));
        } else
            text.append(QString::number(minutes).rightJustified(2, '0', false));
        text.append(' ' + i18n("min.") + ' ');
        trim = true;
    }
    if (seconds || trim) {
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

Timecode TimecodeFormatter::fromHH_MM_SS_FF(const QString& timecode) const
{
    if (timecode.isEmpty()) {
        return Timecode(this);
    }

    int totalFrames;

    int hours;
    int offset = 0;
    if (timecode.at(0) == QLatin1Char('-')) {
        // FIXME: preserve "-"
        offset = 1;
        hours = timecode.mid(1, 2).toInt();
    } else {
        hours = timecode.left(2).toInt();
    }
    int minutes = timecode.mid(3 + offset, 2).toInt();
    int seconds = timecode.mid(6 + offset, 2).toInt();
    int frames = timecode.right(2).toInt();

    totalFrames = qRound(m_framerate.value()) * (hours * 3600 + minutes * 60 + seconds) + frames;

    if (m_isDropFrame) {
        int totalMinutes = (60 * hours) + minutes;
        totalFrames -= m_dropFrames * (totalMinutes - floor(totalMinutes / 10));
    }

    return Timecode(totalFrames, this);
}

Timecode TimecodeFormatter::fromHH_MM_SS_HH(const QString& timecode) const
{
    return Timecode();
}

Timecode TimecodeFormatter::fromFrames(const QString& timecode) const
{
    return Timecode(timecode.toInt(), this);
}

Timecode TimecodeFormatter::fromSeconds(const QString& timecode) const
{
    return Timecode();
}

Timecode TimecodeFormatter::fromMilliseconds(const QString& timecode) const
{
    return Timecode();
}

Timecode TimecodeFormatter::fromTime(const QString& timecode) const
{
    return Timecode();
}

void TimecodeFormatter::fillComponents(int factor, int &lastComponent, int &seconds, int &minutes, int &hours) const
{
    seconds = lastComponent / factor;
    lastComponent %= factor;
    minutes = seconds / 60;
    seconds %= 60;
    hours = minutes / 60;
    minutes %= 60;
}

QString TimecodeFormatter::fillMask(int hours, int minutes, int seconds, int lastComponent, bool isNegative) const
{
    QString timecode;

    if (isNegative) {
        timecode.append('-');
    }

    timecode.append(QString::number(hours).rightJustified(2, '0', false));
    timecode.append(':');
    timecode.append(QString::number(minutes).rightJustified(2, '0', false));
    timecode.append(':');
    timecode.append(QString::number(seconds).rightJustified(2, '0', false));
    if (m_isDropFrame) {
        timecode.append(',');
    } else {
        timecode.append(':');
    }
    timecode.append(QString::number(lastComponent).rightJustified(2, '0', false));

    return timecode;
}


#include "timecodeformatter.moc"
