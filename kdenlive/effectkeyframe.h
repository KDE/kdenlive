/***************************************************************************
                          effectkeyframe  -  description
                             -------------------
    begin                : Fri Jan 2 2004
    copyright            : (C) 2004 by Jason Wood
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
#ifndef EFFECTKEYFRAME_H
#define EFFECTKEYFRAME_H

#include "gentime.h"

class EffectDoubleKeyFrame;

/**
Base class for effect keyframe values. A keyframe specifies a specific value at a particular point in time.

@author Jason Wood
*/
class EffectKeyFrame
{
public:
    enum InterpolationTypes { None, Linear };

    EffectKeyFrame();
    EffectKeyFrame(double time);

    virtual ~EffectKeyFrame();

    virtual EffectDoubleKeyFrame *toDoubleKeyFrame() { return 0; }

    void setTime(double time) { m_time = time; }
    void setTime(const GenTime &startTime, const GenTime &endTime, const GenTime &time) { m_time = (time - startTime).seconds() / (endTime - startTime).seconds(); }

    double time() const { return m_time; }

    /** Given the start and end times, returns the keyframe time */
    GenTime time(const GenTime &start, const GenTime &end) const { return start + ((end - start) * m_time); }

    /** Return a keyfrane that interpolates between this and the passed keyframe, and is a keyframe that would exist at the
       specified time. */
    virtual EffectKeyFrame *interpolateKeyFrame(EffectKeyFrame *keyframe, double time) const = 0;

    /** Return a clone of this keyframe */
    virtual EffectKeyFrame *clone() const = 0;
private:
	/* Keyframe times are expressed as a double value between 0 and 1. 0 is the earliest that the keyframe could possibly be (for example, at the beginning
	of the clip), and 1 is at the latest that the keyframe could possibly be (for example, the end of the clip.) */
	double m_time;
};

#endif
