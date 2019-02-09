/*
Copyright (C) 2012  Simon A. Eugster (Granjow)  <simon.eu@gmail.com>
This file is part of kdenlive. See www.kdenlive.org.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
*/

#ifndef FFTCORRELATION_H
#define FFTCORRELATION_H

#include <QtGlobal>
/**
  This class provides methods to calculate convolution
  and correlation of two vectors by means of FFT, which
  is O(n log n) (convolution in spacial domain would be
  O(n²)).
  */
class FFTCorrelation
{
public:
    /**
      Computes the convolution between \c left and \c right.
      \c out_correlated must be a pre-allocated vector of size
      \c leftSize + \c rightSize + 1.
      */
    static void convolve(const float *left, const size_t leftSize, const float *right, const size_t rightSize, float *out_convolved);

    /**
      Computes the correlation between \c left and \c right.
      \c out_correlated must be a pre-allocated vector of size
      \c leftSize + \c rightSize + 1.
      */
    static void correlate(const qint64 *left, const size_t leftSize, const qint64 *right, const size_t rightSize, float *out_correlated);

    static void correlate(const qint64 *left, const size_t leftSize, const qint64 *right, const size_t rightSize, qint64 *out_correlated);
};

#endif // FFTCORRELATION_H
