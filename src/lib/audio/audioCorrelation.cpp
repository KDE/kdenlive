/*
Copyright (C) 2012  Simon A. Eugster (Granjow)  <simon.eu@gmail.com>
This file is part of kdenlive. See www.kdenlive.org.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
*/

#include "audioCorrelation.h"
#include "fftCorrelation.h"

#include "klocalizedstring.h"
#include "kdenlive_debug.h"
#include <QTime>
#include <cmath>
#include <iostream>

AudioCorrelation::AudioCorrelation(AudioEnvelope *mainTrackEnvelope) :
    m_mainTrackEnvelope(mainTrackEnvelope)
{
    m_mainTrackEnvelope->normalizeEnvelope();
    connect(m_mainTrackEnvelope, &AudioEnvelope::envelopeReady, this, &AudioCorrelation::slotAnnounceEnvelope);
}

AudioCorrelation::~AudioCorrelation()
{
    delete m_mainTrackEnvelope;
    foreach (AudioEnvelope *envelope, m_children) {
        delete envelope;
    }
    foreach (AudioCorrelationInfo *info, m_correlations) {
        delete info;
    }

    qCDebug(KDENLIVE_LOG) << "Envelope deleted.";
}

void AudioCorrelation::slotAnnounceEnvelope()
{
    emit displayMessage(i18n("Audio analysis finished"), OperationCompletedMessage);
}

void AudioCorrelation::addChild(AudioEnvelope *envelope)
{
    envelope->normalizeEnvelope();
    connect(envelope, &AudioEnvelope::envelopeReady, this, &AudioCorrelation::slotProcessChild);
}

void AudioCorrelation::slotProcessChild(AudioEnvelope *envelope)
{
    const int sizeMain = m_mainTrackEnvelope->envelopeSize();
    const int sizeSub = envelope->envelopeSize();

    AudioCorrelationInfo *info = new AudioCorrelationInfo(sizeMain, sizeSub);
    qint64 *correlation = info->correlationVector();

    const qint64 *envMain = m_mainTrackEnvelope->envelope();
    const qint64 *envSub = envelope->envelope();
    qint64 max = 0;

    if (sizeSub > 200) {
        FFTCorrelation::correlate(envMain, sizeMain,
                                  envSub, sizeSub,
                                  correlation);
    } else {
        correlate(envMain, sizeMain,
                  envSub, sizeSub,
                  correlation,
                  &max);
        info->setMax(max);
    }

    m_children.append(envelope);
    m_correlations.append(info);

    Q_ASSERT(m_correlations.size() == m_children.size());
    int index = m_children.indexOf(envelope);
    int shift = getShift(index);
    emit gotAudioAlignData(envelope->track(), envelope->startPos(), shift);
}

int AudioCorrelation::getShift(int childIndex) const
{
    Q_ASSERT(childIndex >= 0);
    Q_ASSERT(childIndex < m_correlations.size());

    int indexOffset = m_correlations.at(childIndex)->maxIndex();
    indexOffset -= m_children.at(childIndex)->envelopeSize();

    return indexOffset;
}

AudioCorrelationInfo const *AudioCorrelation::info(int childIndex) const
{
    Q_ASSERT(childIndex >= 0);
    Q_ASSERT(childIndex < m_correlations.size());

    return m_correlations.at(childIndex);
}

void AudioCorrelation::correlate(const qint64 *envMain, int sizeMain,
                                 const qint64 *envSub, int sizeSub,
                                 qint64 *correlation,
                                 qint64 *out_max)
{
    Q_ASSERT(correlation != nullptr);

    qint64 const *left;
    qint64 const *right;
    int size;
    qint64 sum;
    qint64 max = 0;

    /*
      Correlation:

      SHIFT \in [-sS..sM]

      <--sS----
      [  sub  ]----sM--->[ sub ]
               [  main  ]

            ^ correlation vector index = SHIFT + sS

      main is fixed, sub is shifted along main.

    */

    QTime t;
    t.start();
    for (int shift = -sizeSub; shift <= sizeMain; ++shift) {

        if (shift <= 0) {
            left = envSub - shift;
            right = envMain;
            size = std::min(sizeSub + shift, sizeMain);
        } else {
            left = envSub;
            right = envMain + shift;
            size = std::min(sizeSub, sizeMain - shift);
        }

        sum = 0;
        for (int i = 0; i < size; ++i) {
            sum += (*left) * (*right);
            left++;
            right++;
        }
        correlation[sizeSub + shift] = qAbs(sum);

        if (sum > max) {
            max = sum;
        }

    }
    qCDebug(KDENLIVE_LOG) << "Correlation calculated. Time taken: " << t.elapsed() << " ms.";

    if (out_max != nullptr) {
        *out_max = max;
    }
}

