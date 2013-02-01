/*
Copyright (C) 2012  Till Theato <root@ttill.de>
This file is part of Kdenlive. See www.kdenlive.org.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
*/

#include "timecode.h"
#include "timecodeformatter.h"
#include "core.h"
#include "project/projectmanager.h"
#include "project/project.h"


Timecode::Timecode(TimecodeFormatter const* formatter) :
    m_frames(0)
{
    setFormatter(formatter);
}

Timecode::Timecode(double seconds, TimecodeFormatter const* formatter)
{
    setFormatter(formatter);
    m_frames = qRound(m_formatter->framerate() * seconds);
}

Timecode::Timecode(int frames, TimecodeFormatter const* formatter) :
    m_frames(frames)
{
    setFormatter(formatter);
}

Timecode::Timecode(const QString& timecode, const TimecodeFormatter* formatter)
{
    setFormatter(formatter);
    m_frames = m_formatter->fromString(timecode).frames();
}

void Timecode::setFormatter(TimecodeFormatter const* formatter)
{
    if (formatter) {
        m_formatter = formatter;
    } else {
        if (pCore->projectManager()->current()) {
            m_formatter = pCore->projectManager()->current()->timecodeFormatter();
        } else {
            m_formatter = 0;
        }
    }
}

TimecodeFormatter const *Timecode::formatter()
{
    return m_formatter;
}

double Timecode::seconds() const
{
    return m_frames / m_formatter->framerate().value();
}

double Timecode::milliseconds() const
{
    return seconds() * 1000;
}

int Timecode::frames() const
{
    return m_frames;
}

QString Timecode::formatted(TimecodeFormatter::Formats format) const
{
    return m_formatter->format(*this, format);
}

QString Timecode::mask() const
{
    return m_formatter->mask(*this);
}


Timecode Timecode::operator-() const
{
    return Timecode(-m_frames, m_formatter);
}

Timecode Timecode::operator+(const Timecode& t) const
{
    return Timecode(m_frames + t.frames(), m_formatter);
}

Timecode Timecode::operator-(const Timecode& t) const
{
    return Timecode(m_frames - t.frames(), m_formatter);
}

Timecode Timecode::operator*(const Timecode& t) const
{
    return Timecode(m_frames * t.frames(), m_formatter);
}

Timecode Timecode::operator/(const Timecode& t) const
{
    return Timecode(qRound(m_frames / (double)t.frames()), m_formatter);
}

Timecode& Timecode::operator+=(const Timecode& t)
{
    m_frames += t.frames();
    return *this;
}

Timecode& Timecode::operator-=(const Timecode& t)
{
    m_frames -= t.frames();
    return *this;
}

bool Timecode::operator<(const Timecode& t) const
{
    return m_frames < t.frames();
}

bool Timecode::operator<(int frames) const
{
    return m_frames < frames;
}

bool Timecode::operator>(const Timecode& t) const
{
    return m_frames > t.frames();
}

bool Timecode::operator<=(const Timecode& t) const
{
    return m_frames <= t.frames();
}

bool Timecode::operator>=(const Timecode& t) const
{
    return m_frames >= t.frames();
}

bool Timecode::operator==(const Timecode& t) const
{
    return m_frames == t.frames();
}

bool Timecode::operator!=(const Timecode& t) const
{
    return m_frames != t.frames();
}
