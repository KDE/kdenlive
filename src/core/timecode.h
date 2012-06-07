/*
Copyright (C) 2012  Till Theato <root@ttill.de>
This file is part of Kdenlive. See www.kdenlive.org.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
*/

#ifndef TIMECODE_H
#define TIMECODE_H

#include "timecodeformatter.h"
#include <QString>


class Timecode
{
public:
    Timecode(TimecodeFormatter const *formatter = 0);
    Timecode(double seconds, TimecodeFormatter const *formatter = 0);
    Timecode(int frames, TimecodeFormatter const *formatter = 0);

    void setFormatter(TimecodeFormatter const *formatter);
    TimecodeFormatter const *formatter();

    double seconds() const;
    double milliseconds() const;
    int frames() const;

    QString formatted(TimecodeFormatter::Formats format = TimecodeFormatter::DefaultFormat) const;

    QString mask() const;

    Timecode operator-() const;
    Timecode operator+(const Timecode &t) const;
    Timecode operator-(const Timecode &t) const;
    Timecode operator*(const Timecode &t) const;
    Timecode operator/(const Timecode &t) const;
    Timecode &operator+=(const Timecode &t);
    Timecode &operator-=(const Timecode &t);
    bool operator<(const Timecode &t) const;
    bool operator<(int frames) const;
    bool operator>(const Timecode &t) const;
    bool operator<=(const Timecode &t) const;
    bool operator>=(const Timecode &t) const;
    bool operator==(const Timecode &t) const;
    bool operator!=(const Timecode &t) const;

private:
    int m_frames;
    TimecodeFormatter const *m_formatter;
};

#endif
