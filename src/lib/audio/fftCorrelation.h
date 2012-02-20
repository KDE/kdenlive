#ifndef FFTCORRELATION_H
#define FFTCORRELATION_H

class FFTCorrelation
{
public:
    static void correlate(float *left, int leftSize, float *right, int rightSize, float **out, int &out_size);
};

#endif // FFTCORRELATION_H
