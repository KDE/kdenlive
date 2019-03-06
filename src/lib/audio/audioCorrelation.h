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
#include "definitions.h"
#include <QList>

/**
  This class does the correlation between two tracks
  in order to synchronize (align) them.

  It uses one main track (used in the initializer); further tracks will be
  aligned relative to this main track.
  */
class AudioCorrelation : public QObject
{
    Q_OBJECT
public:
    /**
      @param mainTrackEnvelope Envelope of the reference track. Its
                               actual computation will be started in
                               this constructor
                               (i.e. mainTrackEnvelope->StartComputeEnvelope()
                               will be called). The computation of the
                               envelop must not be started when passed
                               to this constructor
                               (i.e. mainTrackEnvelope->HasComputationStarted()
                               must return false).
    */
    explicit AudioCorrelation(std::unique_ptr<AudioEnvelope> mainTrackEnvelope);
    ~AudioCorrelation() override;

    /**
      Adds a child envelope that will be aligned to the reference
      envelope. This function returns immediately, the alignment
      computation is done asynchronously. When done, the signal
      gotAudioAlignData will be emitted. Similarly to the main
      envelope, the computation of the envelope must not be started
      when it is passed to this object.

      This object will take ownership of the passed envelope.
      */
    void addChild(AudioEnvelope *envelope);

    const AudioCorrelationInfo *info(int childIndex) const;
    int getShift(int childIndex) const;

    /**
      Correlates the two vectors envMain and envSub.
      \c correlation must be a pre-allocated vector of size sizeMain+sizeSub+1.
      */
    static void correlate(const qint64 *envMain, size_t sizeMain, const qint64 *envSub, size_t sizeSub, qint64 *correlation, qint64 *out_max = nullptr);

private:
    std::unique_ptr<AudioEnvelope> m_mainTrackEnvelope;

    QList<AudioEnvelope *> m_children;
    QList<AudioCorrelationInfo *> m_correlations;

private slots:
    /**
     This is invoked when the child envelope is computed. This
     triggers the actual computations of the cross-correlation for
     aligning the envelope to the reference envelope.

     Takes ownership of @p envelope.
   */
    void slotProcessChild(AudioEnvelope *envelope);
    void slotAnnounceEnvelope();

signals:
    void gotAudioAlignData(int, int);
    void displayMessage(const QString &, MessageType, int);
};

#endif // AUDIOCORRELATION_H
