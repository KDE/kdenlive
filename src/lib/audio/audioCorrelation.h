/***************************************************************************
 *   Copyright (C) 2012 by Simon Andreas Eugster (simon.eu@gmail.com)      *
 *   This file is part of kdenlive. See www.kdenlive.org.                  *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 ***************************************************************************/

#ifndef AUDIOCORRELATION_H
#define AUDIOCORRELATION_H

#include "audioCorrelationInfo.h"
#include "audioEnvelope.h"
#include <QList>

class AudioCorrelationInfo;

/**
  This class does the correlation between two tracks
  in order to synchronize (align) them.

  It uses one main track (used in the initializer); further tracks will be
  aligned relative to this main track.
  */
class AudioCorrelation
{
public:
    AudioCorrelation(AudioEnvelope *mainTrackEnvelope);
    ~AudioCorrelation();

    /// \return The child's index
    int addChild(AudioEnvelope *envelope);

    const AudioCorrelationInfo *info(int childIndex) const;
    int getShift(int childIndex) const;


private:
    AudioEnvelope *m_mainTrackEnvelope;

    QList<AudioEnvelope*> m_children;
    QList<AudioCorrelationInfo*> m_correlations;
};

#endif // AUDIOCORRELATION_H
