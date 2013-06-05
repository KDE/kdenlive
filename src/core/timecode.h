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
#include <kdemacros.h>

/**
 * @class Timecode
 * @brief Represents a timecode value.
 */


class KDE_EXPORT Timecode
{
public:
    /**
     * @brief Initializes the timecode with 0.
     * @param formatter formatter to use. By default the project's one is used
     */
    Timecode(TimecodeFormatter const *formatter = 0);
    /**
     * @brief Constructor.
     * @param seconds value of timecode in seconds
     * @param formatter formatter to use. By default the project's one is used
     */
    Timecode(double seconds, TimecodeFormatter const *formatter = 0);
    /**
     * @brief Constructor.
     * @param frames value of timecode in frames
     * @param formatter formatter to use. By default the project's one is used
     */
    Timecode(int frames, TimecodeFormatter const *formatter = 0);
    /**
     * @brief Constructor.
     * @param timecode value of timecode in the current default format of @param formatter
     * @param formatter formatter to use. By default the project's one is used
     */
    Timecode(const QString &timecode, TimecodeFormatter const *formatter = 0);

    /**
     * @brief Sets a new formatter.
     * @param formatter formatter to use. If 0 then the project's one is used.
     */
    void setFormatter(TimecodeFormatter const *formatter);
    /** @brief Returns the formatter used. */
    TimecodeFormatter const *formatter();

    /** @brief Returns the current value in seconds. */
    double seconds() const;
    /** @brief Returns the current value in milliseconds. */
    double milliseconds() const;
    /** @brief Returns the current value in frames. */
    int frames() const;

    /** @brief Returns the current value formatted.
     * @param format format to use. DefaultFormat refers to the used formatter's default format. */
    QString formatted(TimecodeFormatter::Formats format = TimecodeFormatter::DefaultFormat) const;

    /** @brief Returns a mask according to the current value and the formatter. */
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
