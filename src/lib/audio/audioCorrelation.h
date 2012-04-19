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


/**
  This class does the correlation between two tracks
  in order to synchronize (align) them.

  It uses one main track (used in the initializer); further tracks will be
  aligned relative to this main track.
  */
class AudioCorrelation
{
public:
    /// AudioCorrelation will take ownership of mainTrackEnvelope
    AudioCorrelation(AudioEnvelope *mainTrackEnvelope);
    ~AudioCorrelation();

    /**
      This object will take ownership of the passed envelope.
      \return The child's index
      */
    int addChild(AudioEnvelope *envelope, bool useFFT = false);

    const AudioCorrelationInfo *info(int childIndex) const;
    int getShift(int childIndex) const;

    /**
      Correlates the two vectors envMain and envSub.
      \c correlation must be a pre-allocated vector of size sizeMain+sizeSub+1.
      */
    static void correlate(const int64_t *envMain, int sizeMain,
                          const int64_t *envSub, int sizeSub,
                          int64_t *correlation,
                          int64_t *out_max = NULL);
private:
    AudioEnvelope *m_mainTrackEnvelope;

    QList<AudioEnvelope*> m_children;
    QList<AudioCorrelationInfo*> m_correlations;
};

#endif // AUDIOCORRELATION_H
