/*
    SPDX-FileCopyrightText: 2003-2004 Mark Borgerding <Mark@Borgerding.net>
    SPDX-License-Identifier: BSD-3-Clause
*/

#pragma once

#include "kiss_fft.h"
#ifdef __cplusplus
extern "C" {
#endif

/*

 Real optimized version can save about 45% cpu time vs. complex fft of a real seq.



 */

typedef struct kiss_fftr_state *kiss_fftr_cfg;

kiss_fftr_cfg KISS_FFT_API kiss_fftr_alloc(int nfft, int inverse_fft, void *mem, size_t *lenmem);
/*
 nfft must be even

 If you don't care to allocate space, use mem = lenmem = NULL
*/

void KISS_FFT_API kiss_fftr(kiss_fftr_cfg cfg, const kiss_fft_scalar *timedata, kiss_fft_cpx *freqdata);
/*
 input timedata has nfft scalar points
 output freqdata has nfft/2+1 complex points
*/

void KISS_FFT_API kiss_fftri(kiss_fftr_cfg cfg, const kiss_fft_cpx *freqdata, kiss_fft_scalar *timedata);
/*
 input freqdata has  nfft/2+1 complex points
 output timedata has nfft scalar points
*/

#define kiss_fftr_free KISS_FFT_FREE

#ifdef __cplusplus
}
#endif
