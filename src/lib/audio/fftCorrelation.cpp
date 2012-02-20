#include "fftCorrelation.h"

extern "C"
{
#include "../external/kiss_fft/tools/kiss_fftr.h"
}

#include <QTime>
#include <QDebug>
#include <algorithm>

void FFTCorrelation::correlate(float *left, int leftSize, float *right, int rightSize,
                          float **out_correlationData, int &out_size)
{
    QTime time;
    time.start();

    int largestSize = leftSize;
    if (rightSize > largestSize) {
        largestSize = rightSize;
    }

    int size = 64;
    while (size/2 < largestSize) {
        size = size << 1;
    }

    kiss_fftr_cfg fftConfig = kiss_fftr_alloc(size, false,NULL,NULL);
    kiss_fftr_cfg ifftConfig = kiss_fftr_alloc(size, true, NULL,NULL);
    kiss_fft_cpx leftFFT[size/2];
    kiss_fft_cpx rightFFT[size/2];
    kiss_fft_cpx correlatedFFT[size/2];


    float leftData[size];
    float rightData[size];
    *out_correlationData = new float[size];

    std::fill(leftData, leftData+size, 0);
    std::fill(rightData, rightData+size, 0);

    std::copy(left, left+leftSize, leftData);
    std::copy(right, right+rightSize, rightData);

    kiss_fftr(fftConfig, leftData, leftFFT);
    kiss_fftr(fftConfig, rightData, rightFFT);

    for (int i = 0; i < size/2; i++) {
        correlatedFFT[i].r = leftFFT[i].r*rightFFT[i].r - leftFFT[i].i*rightFFT[i].i;
        correlatedFFT[i].i = leftFFT[i].r*rightFFT[i].i + leftFFT[i].i*rightFFT[i].r;
    }

    kiss_fftri(ifftConfig, correlatedFFT, *out_correlationData);
    out_size = size;

    qDebug() << "FFT correlation computed. Time taken: " << time.elapsed() << " ms";
}
