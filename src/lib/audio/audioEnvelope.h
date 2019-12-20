/***************************************************************************
 *   Copyright (C) 2012 by Simon Andreas Eugster (simon.eu@gmail.com)      *
 *   This file is part of kdenlive. See www.kdenlive.org.                  *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 ***************************************************************************/

#ifndef AUDIOENVELOPE_H
#define AUDIOENVELOPE_H

#include "audioInfo.h"
#include <QFutureWatcher>
#include <QObject>
#include <memory>
#include <mlt++/Mlt.h>
#include <vector>

class QImage;

/**
  The audio envelope is a simplified version of an audio track
  with frame resolution. One entry is calculated by the sum
  of the absolute values of all samples in the current frame.

  See also: http://web.archive.org/web/20180626235917/http://bemasc.net/wordpress/2011/07/26/an-auto-aligner-for-pitivi/
  */
class AudioEnvelope : public QObject
{
    Q_OBJECT

public:
    explicit AudioEnvelope(const QString &binId, int clipId, size_t offset = 0, size_t length = 0, size_t startPos = 0);
    ~AudioEnvelope() override;
    /**
       Starts the asynchronous computation that computes the
       envelope. When the computations are done, the signal
       'envelopeReady' will be emitted.
    */
    void startComputeEnvelope();

    /**
       Returns whether startComputeEnvelope() has been called.
    */
    bool hasComputationStarted() const;

    /**
       Returns the envelope data. Blocks until the computation of the
       envelope is done.
       REQUIRES: startComputeEnvelope() has been called.
    */
    const std::vector<qint64> &envelope();

    QImage drawEnvelope();

    size_t offset();

    void dumpInfo();

    int clipId() const;
    size_t startPos() const;

private:
    struct AudioSummary
    {
        explicit AudioSummary(size_t size)
            : audioAmplitudes(size)
        {
        }
        AudioSummary() = default;
        // This is the envelope data. There is one element for each
        // frame, which contains the sum of the absolute amplitudes of
        // the audio signal for that frame.
        std::vector<qint64> audioAmplitudes;
        // Maximum absolute value of the elements in 'audioAmplitudes'.
        qint64 amplitudeMax = 0;
    };

    /**
       Blocks until the AudioSummary has been computed.
       REQUIRES: startComputeEnvelope() has been called.
    */
    const AudioSummary &audioSummary();

    /**
     Actually computes the envelope data, synchronously.
    */
    AudioSummary loadAndNormalizeEnvelope() const;

    std::shared_ptr<Mlt::Producer> m_producer;
    std::unique_ptr<AudioInfo> m_info;
    QFutureWatcher<AudioSummary> m_watcher;
    QFuture<AudioSummary> m_audioSummary;

    size_t m_offset;
    const int m_clipId;
    const size_t m_startpos;
    size_t m_envelopeSize;

signals:
    void envelopeReady(AudioEnvelope *envelope);
};

#endif // AUDIOENVELOPE_H
