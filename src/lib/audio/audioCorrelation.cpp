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

#include <QTime>
#include <cmath>
#include <iostream>

AudioCorrelation::AudioCorrelation(AudioEnvelope *mainTrackEnvelope) :
    m_mainTrackEnvelope(mainTrackEnvelope)
{
    m_mainTrackEnvelope->normalizeEnvelope();
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

    std::cout << "Envelope deleted." << std::endl;
}

int AudioCorrelation::addChild(AudioEnvelope *envelope, bool useFFT)
{
    envelope->normalizeEnvelope();

    const int sizeMain = m_mainTrackEnvelope->envelopeSize();
    const int sizeSub = envelope->envelopeSize();


    AudioCorrelationInfo *info = new AudioCorrelationInfo(sizeMain, sizeSub);
    int64_t *correlation = info->correlationVector();

    const int64_t *envMain = m_mainTrackEnvelope->envelope();
    const int64_t *envSub = envelope->envelope();
    int64_t max = 0;

    if (useFFT) {
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

    return m_children.indexOf(envelope);
}

int AudioCorrelation::getShift(int childIndex) const
{
    Q_ASSERT(childIndex >= 0);
    Q_ASSERT(childIndex < m_correlations.size());

    int indexOffset = m_correlations.at(childIndex)->maxIndex();
    indexOffset -= m_children.at(childIndex)->envelopeSize();

    return indexOffset;
}

AudioCorrelationInfo const* AudioCorrelation::info(int childIndex) const
{
    Q_ASSERT(childIndex >= 0);
    Q_ASSERT(childIndex < m_correlations.size());

    return m_correlations.at(childIndex);
}


void AudioCorrelation::correlate(const int64_t *envMain, int sizeMain,
                                 const int64_t *envSub, int sizeSub,
                                 int64_t *correlation,
                                 int64_t *out_max)
{
    Q_ASSERT(correlation != NULL);

    int64_t const* left;
    int64_t const* right;
    int size;
    int64_t sum;
    int64_t max = 0;


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
    for (int shift = -sizeSub; shift <= sizeMain; shift++) {

        if (shift <= 0) {
            left = envSub-shift;
            right = envMain;
            size = std::min(sizeSub+shift, sizeMain);
        } else {
            left = envSub;
            right = envMain+shift;
            size = std::min(sizeSub, sizeMain-shift);
        }

        sum = 0;
        for (int i = 0; i < size; i++) {
            sum += (*left) * (*right);
            left++;
            right++;
        }
        correlation[sizeSub+shift] = std::abs(sum);

        if (sum > max) {
            max = sum;
        }

    }
    std::cout << "Correlation calculated. Time taken: " << t.elapsed() << " ms." << std::endl;

    if (out_max != NULL) {
        *out_max = max;
    }
}
