/*
Copyright (c) 2003-2004, Mark Borgerding (Mark@Borgerding.net)

All rights reserved.

Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:

    * Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or
other materials provided with the distribution.
    * Neither the author nor the names of any contributors may be used to endorse or promote products derived from this software without specific prior written
permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#ifndef KISS_FFTNDR_H
#define KISS_FFTNDR_H

#include "../kiss_fft.h"
#include "kiss_fftnd.h"
#include "kiss_fftr.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct kiss_fftndr_state *kiss_fftndr_cfg;

kiss_fftndr_cfg kiss_fftndr_alloc(const int *dims, int ndims, int inverse_fft, void *mem, size_t *lenmem);
/*
 dims[0] must be even

 If you don't care to allocate space, use mem = lenmem = NULL
*/

void kiss_fftndr(kiss_fftndr_cfg cfg, const kiss_fft_scalar *timedata, kiss_fft_cpx *freqdata);
/*
 input timedata has dims[0] X dims[1] X ... X  dims[ndims-1] scalar points
 output freqdata has dims[0] X dims[1] X ... X  dims[ndims-1]/2+1 complex points
*/

void kiss_fftndri(kiss_fftndr_cfg cfg, const kiss_fft_cpx *freqdata, kiss_fft_scalar *timedata);
/*
 input and output dimensions are the exact opposite of kiss_fftndr
*/

#define kiss_fftr_free free

#ifdef __cplusplus
}
#endif

#endif
