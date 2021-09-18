/*
    SPDX-FileCopyrightText: 2002 Jason Wood <jasonwood@blueyonder.co.uk>

    SPDX-License-Identifier: LicenseRef-KDE-Accepted-GPL
*/

#ifndef GENTIME_H
#define GENTIME_H

#include <QString>
#include <cmath>

/**
 * @class GenTime
 * @brief Encapsulates a time, which can be set in various forms and outputted in various forms.
 * @author Jason Wood
 */
class GenTime
{
public:
    /** @brief Creates a GenTime object, with a time of 0 seconds. */
    GenTime();

    /** @brief Creates a GenTime object, with time given in seconds. */
    explicit GenTime(double seconds);

    /** @brief Creates a GenTime object, by passing number of frames and how many frames per second. */
    GenTime(int frames, double framesPerSecond);

    /** @brief Gets the time, in seconds. */
    double seconds() const;

    /** @brief Gets the time, in milliseconds */
    double ms() const;

    /** @brief Gets the time in frames.
     * @param framesPerSecond Number of frames per second */
    int frames(double framesPerSecond) const;

    QString toString() const;

    /*
     * Operators.
     */

    /// Unary minus
    GenTime operator-();

    /// Addition
    GenTime &operator+=(GenTime op);

    /// Subtraction
    GenTime &operator-=(GenTime op);

    /** @brief Adds two GenTimes. */
    GenTime operator+(GenTime op) const;

    /** @brief Subtracts one genTime from another. */
    GenTime operator-(GenTime op) const;

    /** @brief Multiplies one GenTime by a double value, returning a GenTime. */
    GenTime operator*(double op) const;

    /** @brief Divides one GenTime by a double value, returning a GenTime. */
    GenTime operator/(double op) const;

    /* All the comparison operators considers that two GenTime that differs by less
    than one frame are equal.
    The fps used to carry this computation must be set using the static function setFps
    */
    bool operator<(GenTime op) const;

    bool operator>(GenTime op) const;

    bool operator>=(GenTime op) const;

    bool operator<=(GenTime op) const;

    bool operator==(GenTime op) const;

    bool operator!=(GenTime op) const;

    /** @brief Sets the fps used to determine if two GenTimes are equal */
    static void setFps(double fps);

private:
    /** Holds the time in seconds for this object. */
    double m_time;

    /** A delta value that is used to get around floating point rounding issues. */
    static double s_delta;
};

#endif
