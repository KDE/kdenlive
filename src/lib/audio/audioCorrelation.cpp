/*
SPDX-FileCopyrightText: 2012 Simon A. Eugster (Granjow)  <simon.eu@gmail.com>
SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "audioCorrelation.h"
#include "fftCorrelation.h"

#include "kdenlive_debug.h"
#include "klocalizedstring.h"
#include <QElapsedTimer>
#include <cmath>
#include <iostream>

AudioCorrelation::AudioCorrelation(std::unique_ptr<AudioEnvelope> mainTrackEnvelope)
    : m_mainTrackEnvelope(std::move(mainTrackEnvelope))
{
    // Q_ASSERT(!mainTrackEnvelope->hasComputationStarted());
    connect(m_mainTrackEnvelope.get(), &AudioEnvelope::envelopeReady, this, &AudioCorrelation::slotAnnounceEnvelope);
    m_mainTrackEnvelope->startComputeEnvelope();
}

AudioCorrelation::~AudioCorrelation()
{
    for (AudioEnvelope *envelope : std::as_const(m_children)) {
        delete envelope;
    }
    for (AudioCorrelationInfo *info : std::as_const(m_correlations)) {
        delete info;
    }

    qCDebug(KDENLIVE_LOG) << "Envelope deleted.";
}

void AudioCorrelation::slotAnnounceEnvelope()
{
    Q_EMIT displayMessage(i18n("Audio analysis finished"), OperationCompletedMessage, 300);
}

void AudioCorrelation::addChild(AudioEnvelope *envelope)
{
    // We need to connect before starting the computation, to make sure
    // there is no race condition where the signal 'envelopeReady' is
    // lost.
    Q_ASSERT(!envelope->hasComputationStarted());
    connect(envelope, &AudioEnvelope::envelopeReady, this, &AudioCorrelation::slotProcessChild);
    envelope->startComputeEnvelope();
}

void AudioCorrelation::slotProcessChild(AudioEnvelope *envelope)
{
    // Note that at this point the computation of the envelope of the
    // main track might not be finished. envelope() will block until
    // the computation is done.
    const size_t sizeMain = m_mainTrackEnvelope->envelope().size();
    const size_t sizeSub = envelope->envelope().size();

    auto *info = new AudioCorrelationInfo(sizeMain, sizeSub);
    qint64 *correlation = info->correlationVector();

    const std::vector<qint64> &envMain = m_mainTrackEnvelope->envelope();
    const std::vector<qint64> &envSub = envelope->envelope();
    qint64 max = 0;

    if (sizeSub > 200) {
        FFTCorrelation::correlate(&envMain[0], sizeMain, &envSub[0], sizeSub, correlation);
    } else {
        correlate(&envMain[0], sizeMain, &envSub[0], sizeSub, correlation, &max);
        info->setMax(max);
    }

    m_children.append(envelope);
    m_correlations.append(info);

    Q_ASSERT(m_correlations.size() == m_children.size());
    int index = m_children.indexOf(envelope);
    int shift = getShift(index);
    Q_EMIT gotAudioAlignData(envelope->clipId(), shift);
}

int AudioCorrelation::getShift(int childIndex) const
{
    Q_ASSERT(childIndex >= 0);
    Q_ASSERT(childIndex < m_correlations.size());

    size_t indexOffset = m_correlations.at(childIndex)->maxIndex();
    indexOffset -= m_children.at(childIndex)->envelope().size();
    indexOffset += m_children.at(childIndex)->offset();

    return int(indexOffset);
}

AudioCorrelationInfo const *AudioCorrelation::info(int childIndex) const
{
    Q_ASSERT(childIndex >= 0);
    Q_ASSERT(childIndex < m_correlations.size());

    return m_correlations.at(childIndex);
}

void AudioCorrelation::correlate(const qint64 *envMain, size_t sizeMain, const qint64 *envSub, size_t sizeSub, qint64 *correlation, qint64 *out_max)
{
    Q_ASSERT(correlation != nullptr);

    qint64 const *left;
    qint64 const *right;
    size_t size;
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

    QElapsedTimer t;
    t.start();
    for (size_t shift = -sizeSub; shift <= sizeMain; ++shift) {

        if (shift == 0) {
            left = envSub - shift;
            right = envMain;
            size = std::min(sizeSub + shift, sizeMain);
        } else {
            left = envSub;
            right = envMain + shift;
            size = std::min(sizeSub, sizeMain - shift);
        }

        sum = 0;
        for (size_t i = 0; i < size; ++i) {
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
