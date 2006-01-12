/***************************************************************************
                          effectdoublekeyframe  -  description
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
#ifndef EFFECTDOUBLEKEYFRAME_H
#define EFFECTDOUBLEKEYFRAME_H

#include <effectkeyframe.h>

/**
A keyframe value that holds a double.

@author Jason Wood
*/
class EffectDoubleKeyFrame : public EffectKeyFrame
{
public:
    EffectDoubleKeyFrame();
    EffectDoubleKeyFrame(double time, double value);

    virtual ~EffectDoubleKeyFrame();

    virtual EffectDoubleKeyFrame *toDoubleKeyFrame() { return this; }

    double value() const;
    void setValue(double value);

    /** Return a keyfrane that interpolates between this and the passed keyframe, and is a keyframe that would exist at the
    specified time. */
    virtual EffectKeyFrame *interpolateKeyFrame(EffectKeyFrame *keyframe, double time) const;

    virtual EffectKeyFrame *clone() const;

private:
	double m_value;
};

#endif
