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
#include <mlt++/Mlt.h>

#include <QFutureWatcher>
#include <QObject>

class QImage;

/**
  The audio envelope is a simplified version of an audio track
  with frame resolution. One entry is calculated by the sum
  of the absolute values of all samples in the current frame.

  See also: http://bemasc.net/wordpress/2011/07/26/an-auto-aligner-for-pitivi/
  */
class AudioEnvelope : public QObject
{
    Q_OBJECT
    
public:
    explicit AudioEnvelope(const QString &url, Mlt::Producer *producer, int offset = 0, int length = 0, int track = 0, int startPos = 0);
    virtual ~AudioEnvelope();

    /// Returns the envelope, calculates it if necessary.
    int64_t const* envelope();
    int envelopeSize() const;
    int64_t maxValue() const;

    void loadEnvelope();
    int64_t loadStdDev();
    void normalizeEnvelope(bool clampTo0 = false);

    QImage drawEnvelope();

    void dumpInfo() const;
    
    int track() const;
    int startPos() const;

private:
    int64_t *m_envelope;
    Mlt::Producer *m_producer;
    AudioInfo *m_info;
    QFutureWatcher<void> m_watcher;
    QFuture<void> m_future;

    int m_offset;
    int m_length;
    int m_track;
    int m_startpos;

    int m_envelopeSize;
    int64_t m_envelopeMax;
    int64_t m_envelopeMean;
    int64_t m_envelopeStdDev;

    bool m_envelopeStdDevCalculated;
    bool m_envelopeIsNormalized;
    
private slots:
    void slotProcessEnveloppe();
    
signals:
    void envelopeReady(AudioEnvelope *envelope);
};

#endif // AUDIOENVELOPE_H
