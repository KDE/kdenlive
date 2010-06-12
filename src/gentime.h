/***************************************************************************
                          time.h  -  description
                             -------------------
    begin                : Sat Sep 14 2002
    copyright            : (C) 2002 by Jason Wood
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

#ifndef GENTIME_H
#define GENTIME_H

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
    double frames(double framesPerSecond) const;

    /** @brief Rounds the GenTime's value to the nearest frame.
    * @param framesPerSecond Number of frames per second */
    GenTime & roundNearestFrame(double framesPerSecond);


    /*
     * Operators.
     */
    
    GenTime & operator+=(GenTime op) {
        m_time += op.m_time;
        return *this;
    }

    GenTime & operator-=(GenTime op) {
        m_time -= op.m_time;
        return *this;
    }

    /** @brief Adds two GenTimes. */
    GenTime operator+(GenTime op) const {
        return GenTime(m_time + op.m_time);
    }

    /** @brief Subtracts one genTime from another. */
    GenTime operator-(GenTime op) const {
        return GenTime(m_time - op.m_time);
    }

    /** @brief Multiplies one GenTime by a double value, returning a GenTime. */
    GenTime operator*(double op) const {
        return GenTime(m_time * op);
    }

    /** @brief Divides one GenTime by a double value, returning a GenTime. */
    GenTime operator/(double op) const {
        return GenTime(m_time / op);
    }

    bool operator<(GenTime op) const {
        return m_time + s_delta < op.m_time;
    }

    bool operator>(GenTime op) const {
        return m_time > op.m_time + s_delta;
    }

    bool operator>=(GenTime op) const {
        return m_time + s_delta >= op.m_time;
    }

    bool operator<=(GenTime op) const {
        return m_time <= op.m_time + s_delta;
    }

    bool operator==(GenTime op) const {
        return fabs(m_time - op.m_time) < s_delta;
    }

    bool operator!=(GenTime op) const {
        return fabs(m_time - op.m_time) >= s_delta;
    }

private:
    /** Holds the time for this object. */
    double m_time;

    /** A delta value that is used to get around floating point rounding issues. */
    static double s_delta;
};

#endif
