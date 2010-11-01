/* fswebcam - Small and simple webcam for *nix                */
/*============================================================*/
/* Copyright (C)2005-2010 Philip Heron <phil@sanslogic.co.uk> */
/*                                                            */
/* This program is distributed under the terms of the GNU     */
/* General Public License, version 2. You may use, modify,    */
/* and redistribute it under the terms of this license. A     */
/* copy should be included with this source.                  */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdint.h>

#include "src.h"

/* The following YUV functions are based on code by Vincent Hourdin.
 * http://vinvin.dyndns.org/projects/
 *
 * Faster integer maths from camE by Tom Gilbert.
 * http://linuxbrit.co.uk/camE/
*/

int fswc_add_image_yuyv(src_t *src, avgbmp_t *abitmap)
{
	uint8_t *ptr;
	uint32_t x, y, z;
	
	if(src->length < (src->width * src->height * 2)) return(-1);
	
	/* YUYV and UYVY are very similar and so  *
	 * are both handled by this one function. */
	
	ptr = (uint8_t *) src->img;
	z = 0;
	
	for(y = 0; y < src->height; y++)
	{
		for(x = 0; x < src->width; x++)
		{
			int r, g, b;
			int y, u, v;
			
			if(src->palette == SRC_PAL_UYVY)
			{
				if(!z) y = ptr[1] << 8;
				else   y = ptr[3] << 8;
				
				u = ptr[0] - 128;
				v = ptr[2] - 128;
			}
			else /* SRC_PAL_YUYV */
			{
				if(!z) y = ptr[0] << 8;
				else   y = ptr[2] << 8;
				
				u = ptr[1] - 128;
				v = ptr[3] - 128;
			}
			
			r = (y + (359 * v)) >> 8;
			g = (y - (88 * u) - (183 * v)) >> 8;
			b = (y + (454 * u)) >> 8;
			
			*(abitmap++) += CLIP(r, 0x00, 0xFF);
			*(abitmap++) += CLIP(g, 0x00, 0xFF);
			*(abitmap++) += CLIP(b, 0x00, 0xFF);
			
			if(z++)
			{
				z = 0;
				ptr += 4;
			}
		}
	}
	
        return(0);
}

int fswc_add_image_yuv420p(src_t *src, avgbmp_t *abitmap)
{
	uint8_t *yptr, *uptr, *vptr;
	uint32_t x, y, p, o;
	
	if(src->length < (src->width * src->height * 3) / 2) return(-1);
	
	/* Setup pointers to Y, U and V buffers. */
	yptr = (uint8_t *) src->img;
	uptr = yptr + (src->width * src->height);
	vptr = uptr + (src->width * src->height / 4);
	o = 0;
	p = 0;
	
	for(y = 0; y < src->height; y++)
	{
		for(x = 0; x < src->width; x++)
		{
			int r, g, b;
			int y, u, v;
			
			y = *(yptr++) << 8;
			u = uptr[p] - 128;
			v = vptr[p] - 128;
			
			r = (y + (359 * v)) >> 8;
			g = (y - (88 * u) - (183 * v)) >> 8;
			b = (y + (454 * u)) >> 8;
			
			*(abitmap++) += CLIP(r, 0x00, 0xFF);
			*(abitmap++) += CLIP(g, 0x00, 0xFF);
			*(abitmap++) += CLIP(b, 0x00, 0xFF);
			
			if(x & 1) p++;
		}
		
		if(!(y & 1)) p -= src->width / 2;
	}
	
	return(0);
}

int fswc_add_image_nv12mb(src_t *src, avgbmp_t *abitmap)
{
	uint32_t x, y;
	uint32_t bw;
	
	if(src->length != (src->width * src->height * 3) / 2) return(-1);
	
	bw = src->width >> 4;
	
	for(y = 0; y < src->height; y++)
	{
		for(x = 0; x < src->width; x++)
		{
			uint32_t bx, by;
			int cy, cu, cv;
			int cr, cg, cb;
			uint8_t *py, *puv;
			
			bx = x >> 4;
			by = y >> 4;
			
			py  = src->img;
			py += ((by * bw) + bx) * 0x100;
			py += ((y - (by << 4)) * 0x10) + (x - (bx << 4));
			
			by /= 2;
			
			puv  = (avgbmp_t *)src->img + (src->width * src->height);
			puv += ((by * bw) + bx) * 0x100;
			puv += (((y / 2) - (by << 4)) * 0x10) + ((x - (bx << 4)) &~ 1);
			
			cy = *py << 8;
			cu = puv[0] - 128;
			cv = puv[1] - 128;
			
			cr = (cy + (359 * cv)) >> 8;
			cg = (cy - (88 * cu) - (183 * cv)) >> 8;
			cb = (cy + (454 * cu)) >> 8;
			
			*(abitmap++) += CLIP(cr, 0x00, 0xFF);
			*(abitmap++) += CLIP(cg, 0x00, 0xFF);
			*(abitmap++) += CLIP(cb, 0x00, 0xFF);
		}
	}
	
	return(0);
}

