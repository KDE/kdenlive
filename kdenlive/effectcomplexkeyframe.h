/***************************************************************************
                          effectcomplexkeyframe  -  description
                             -------------------
    begin                : Fri Jan 16 2006
    copyright            : (C) 2006 by Jean-Baptiste Mardelle
    email                : jb@ader.ch
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/
#ifndef EFFECTCOMPLEXKEYFRAME_H
#define EFFECTCOMPLEXKEYFRAME_H

#include <effectkeyframe.h>

/**
A keyframe value that holds a double.

@author Jason Wood
*/
class EffectComplexKeyFrame:public EffectKeyFrame {
  public:
    EffectComplexKeyFrame();
    EffectComplexKeyFrame(double time, QStringList values);

     virtual ~ EffectComplexKeyFrame();

    virtual EffectComplexKeyFrame *toComplexKeyFrame() {
	return this;
    } int value(int ix) const;
    void setValue(int ix, QString);
    const QString processComplexKeyFrame() const;

    /** Return a keyfrane that interpolates between this and the passed keyframe, and is a keyframe that would exist at the
    specified time. */
    virtual EffectKeyFrame *interpolateKeyFrame(EffectKeyFrame * keyframe,
	double time) const;

    virtual EffectKeyFrame *clone() const;

  private:
    QStringList m_values;
};

#endif
