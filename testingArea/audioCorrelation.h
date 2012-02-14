#ifndef AUDIOCORRELATION_H
#define AUDIOCORRELATION_H

#include "audioCorrelationInfo.h"
#include "audioEnvelope.h"
#include <QList>

class AudioCorrelationInfo;
class AudioCorrelation
{
public:
    AudioCorrelation(AudioEnvelope *mainTrackEnvelope);
    ~AudioCorrelation();

    int addChild(AudioEnvelope *envelope);
//    int childIndex(AudioEnvelope *envelope) const;

    const AudioCorrelationInfo *info(int childIndex) const;
    int getShift(int childIndex) const;


private:
    AudioEnvelope *m_mainTrackEnvelope;

    QList<AudioEnvelope*> m_children;
    QList<AudioCorrelationInfo*> m_correlations;
};

#endif // AUDIOCORRELATION_H
