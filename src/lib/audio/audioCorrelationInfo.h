/***************************************************************************
 *   Copyright (C) 2012 by Simon Andreas Eugster (simon.eu@gmail.com)      *
 *   This file is part of kdenlive. See www.kdenlive.org.                  *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 ***************************************************************************/

#ifndef AUDIOCORRELATIONINFO_H
#define AUDIOCORRELATIONINFO_H

#include <QImage>

#include <sys/types.h>

/**
  This class holds the correlation of two audio samples.
  It is mainly a container for data, the correlation itself is calculated
  in the class AudioCorrelation.
  */
class AudioCorrelationInfo
{
public:
    AudioCorrelationInfo(size_t mainSize, size_t subSize);
    ~AudioCorrelationInfo();

    size_t size() const;
    qint64 *correlationVector();
    qint64 const *correlationVector() const;

    /**
      Returns the maximum value in the correlation vector.
      If it has not been set before with setMax(), it will be calculated.
      */
    qint64 max() const;
    void setMax(qint64 max); ///< Can be set to avoid calculating the max again in this function

    /**
      Returns the index of the largest value in the correlation vector
      */
    size_t maxIndex() const;

    QImage toImage(size_t height = 400) const;

private:
    size_t m_mainSize;
    size_t m_subSize;

    qint64 *m_correlationVector;
    qint64 m_max;
};

#endif // AUDIOCORRELATIONINFO_H
