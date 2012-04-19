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

/**
  This class holds the correlation of two audio samples.
  It is mainly a container for data, the correlation itself is calculated
  in the class AudioCorrelation.
  */
class AudioCorrelationInfo
{
public:
    AudioCorrelationInfo(int mainSize, int subSize);
    ~AudioCorrelationInfo();

    int size() const;
    int64_t* correlationVector();
    int64_t const* correlationVector() const;

    /**
      Returns the maximum value in the correlation vector.
      If it has not been set before with setMax(), it will be calculated.
      */
    int64_t max() const;
    void setMax(int64_t max); ///< Can be set to avoid calculating the max again in this function

    /**
      Returns the index of the largest value in the correlation vector
      */
    int maxIndex() const;

    QImage toImage(int height = 400) const;

private:
    int m_mainSize;
    int m_subSize;

    int64_t *m_correlationVector;
    int64_t m_max;

};

#endif // AUDIOCORRELATIONINFO_H
