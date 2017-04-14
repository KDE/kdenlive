/*
Copyright (c) 2003-2010, Mark Borgerding (Mark@Borgerding.net)

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

#ifndef KFC_H
#define KFC_H

#include "../kiss_fft.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
KFC -- Kiss FFT Cache

Not needing to deal with kiss_fft_alloc and a config
object may be handy for a lot of programs.

KFC uses the underlying KISS FFT functions, but caches the config object.
The first time kfc_fft or kfc_ifft for a given FFT size, the cfg
object is created for it.  All subsequent calls use the cached
configuration object.

NOTE:
You should probably not use this if your program will be using a lot
of various sizes of FFTs.  There is a linear search through the
cached objects.  If you are only using one or two FFT sizes, this
will be negligible. Otherwise, you may want to use another method
of managing the cfg objects.

 There is no automated cleanup of the cached objects.  This could lead
to large memory usage in a program that uses a lot of *DIFFERENT*
sized FFTs.  If you want to force all cached cfg objects to be freed,
call kfc_cleanup.

 */

/*forward complex FFT */
void kfc_fft(int nfft, const kiss_fft_cpx *fin, kiss_fft_cpx *fout);
/*reverse complex FFT */
void kfc_ifft(int nfft, const kiss_fft_cpx *fin, kiss_fft_cpx *fout);

/*free all cached objects*/
void kfc_cleanup(void);

#ifdef __cplusplus
}
#endif

#endif
