/***************************************************************************
                          effectparameter  -  description
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
#ifndef EFFECTPARAMETER_H
#define EFFECTPARAMETER_H

#include <qstring.h>
#include <qptrlist.h>

#include "gentime.h"

class EffectKeyFrame;

/**
A parameter in an effect. Contains a list of keyframes that make up that parameter, and which conforms to the parameter description.

@author Jason Wood
*/

typedef QPtrList < EffectKeyFrame > KeyFrameList;
typedef QPtrListIterator < EffectKeyFrame > KeyFrameListIterator;

class EffectParameter {
  public:
    EffectParameter(const QString & name);

    ~EffectParameter();

    /** @returns the number of keyframes in this effect parameter. */
    const int numKeyFrames() const;

    /** @returns the keyframe in the list. */
    EffectKeyFrame *keyframe(int ix) const;

    uint addKeyFrame(EffectKeyFrame * effectKeyFrame);

    void deleteKeyFrame(int ix);

    /* @returns a keyframe at the given time. If the keyframe already exists,
     * it is returned. If it does not exist, a keyframe is created via interpolation.
     * This returned keyframe will not alter the shape of the keyframe graph - so
     * special care is (will be) taken in the implementation for bezier keyframes
     * and other spline-based keyframes.
     * @note that the returned keyframe must be deleted by the caller to avoid memory
     * leaks.
     */
    EffectKeyFrame *interpolateKeyFrame(double time) const;

    void setSelectedKeyFrame(int ix);
    const int selectedKeyFrame() const;

  private:
     QString m_name;
    KeyFrameList m_keyFrames;
    int m_selectedKeyFrame;

};

#endif
