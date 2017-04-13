/*
Copyright (C) 2012  Simon A. Eugster (Granjow)  <simon.eu@gmail.com>
This file is part of kdenlive. See www.kdenlive.org.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
*/

#include "fftCorrelation.h"

extern "C"
{
#include "../external/kiss_fft/tools/kiss_fftr.h"
}

#include "kdenlive_debug.h"
#include <QTime>
#include <algorithm>
#include <vector>

void FFTCorrelation::correlate(const qint64 *left, const int leftSize,
                               const qint64 *right, const int rightSize,
                               qint64 *out_correlated)
{
    float correlatedFloat[leftSize + rightSize + 1];
    correlate(left, leftSize, right, rightSize, correlatedFloat);

    // The correlation vector will have entries up to N (number of entries
    // of the vector), so converting to integers will not lose that much
    // of precision.
    for (int i = 0; i < leftSize + rightSize + 1; ++i) {
        out_correlated[i] = correlatedFloat[i];
    }
}

void FFTCorrelation::correlate(const qint64 *left, const int leftSize,
                               const qint64 *right, const int rightSize,
                               float *out_correlated)
{
    QTime t;
    t.start();

    float leftF[leftSize];
    float rightF[rightSize];

    // First the qint64 values need to be normalized to floats
    // Dividing by the max value is maybe not the best solution, but the
    // maximum value after correlation should not be larger than the longest
    // vector since each value should be at most 1
    qint64 maxLeft = 1;
    qint64 maxRight = 1;
    for (int i = 0; i < leftSize; ++i) {
        if (labs(left[i]) > maxLeft) {
            maxLeft = labs(left[i]);
        }
    }
    for (int i = 0; i < rightSize; ++i) {
        if (labs(right[i]) > maxRight) {
            maxRight = labs(right[i]);
        }
    }

    // One side needs to be reversed, since multiplication in frequency domain (fourier space)
    // calculates the convolution: \sum l[x]r[N-x] and not the correlation: \sum l[x]r[x]
    for (int i = 0; i < leftSize; ++i) {
        leftF[i] = double(left[i]) / maxLeft;
    }
    for (int i = 0; i < rightSize; ++i) {
        rightF[rightSize - 1 - i] = double(right[i]) / maxRight;
    }

    // Now we can convolve to get the correlation
    convolve(leftF, leftSize, rightF, rightSize, out_correlated);

    qCDebug(KDENLIVE_LOG) << "Correlation (FFT based) computed in " << t.elapsed() << " ms.";
}

void FFTCorrelation::convolve(const float *left, const int leftSize,
                              const float *right, const int rightSize,
                              float *out_convolved)
{
    QTime time;
    time.start();

    // To avoid issues with repetition (we are dealing with cosine waves
    // in the fourier domain) we need to pad the vectors to at least twice their size,
    // otherwise convolution would convolve with the repeated pattern as well
    int largestSize = leftSize;
    if (rightSize > largestSize) {
        largestSize = rightSize;
    }

    // The vectors must have the same size (same frequency resolution!) and should
    // be a power of 2 (for FFT).
    int size = 64;
    while (size / 2 < largestSize) {
        size = size << 1;
    }
    const int fft_size = size / 2 + 1;
    kiss_fftr_cfg fftConfig = kiss_fftr_alloc(size, false, nullptr, nullptr);
    kiss_fftr_cfg ifftConfig = kiss_fftr_alloc(size, true, nullptr, nullptr);
    std::vector<kiss_fft_cpx> leftFFT(fft_size);
    std::vector<kiss_fft_cpx> rightFFT(fft_size);
    std::vector<kiss_fft_cpx> correlatedFFT(fft_size);

    // Fill in the data into our new vectors with padding
    std::vector<float> leftData(size, 0);
    std::vector<float> rightData(size, 0);
    std::vector<float> convolved(size);

    std::copy(left, left + leftSize, leftData.begin());
    std::copy(right, right + rightSize, rightData.begin());

    // Fourier transformation of the vectors
    kiss_fftr(fftConfig, &leftData[0], &leftFFT[0]);
    kiss_fftr(fftConfig, &rightData[0], &rightFFT[0]);

    // Convolution in spacial domain is a multiplication in fourier domain. O(n).
    for (int i = 0; i < correlatedFFT.size(); ++i) {
        correlatedFFT[i].r = leftFFT[i].r * rightFFT[i].r - leftFFT[i].i * rightFFT[i].i;
        correlatedFFT[i].i = leftFFT[i].r * rightFFT[i].i + leftFFT[i].i * rightFFT[i].r;
    }

    // Inverse fourier tranformation to get the convolved data.
    // Insert one element at the beginning to obtain the same result
    // that we also get with the nested for loop correlation.
    *out_convolved = 0;
    int out_size = leftSize + rightSize + 1;

    kiss_fftri(ifftConfig, &correlatedFFT[0], &convolved[0]);
    std::copy(convolved.begin(), convolved.begin() + out_size - 1, out_convolved + 1);

    // Finally some cleanup.
    kiss_fftr_free(fftConfig);
    kiss_fftr_free(ifftConfig);

    qCDebug(KDENLIVE_LOG) << "FFT convolution computed. Time taken: " << time.elapsed() << " ms";
}
