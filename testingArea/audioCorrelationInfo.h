#ifndef AUDIOCORRELATIONINFO_H
#define AUDIOCORRELATIONINFO_H

#include <QImage>

class AudioCorrelationInfo
{
public:
    AudioCorrelationInfo(int mainSize, int subSize);
    ~AudioCorrelationInfo();

    int size() const;
    int64_t* correlationVector();
    int64_t const* correlationVector() const;

    int64_t max() const;
    void setMax(int64_t max); ///< Can be set to avoid calculating the max again in this function

    int maxIndex() const;

    QImage toImage(int height = 400) const;

private:
    int m_mainSize;
    int m_subSize;

    int64_t *m_correlationVector;
    int64_t m_max;

};

#endif // AUDIOCORRELATIONINFO_H
