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

int fswc_add_image_rgb32(src_t *src, avgbmp_t *abitmap)
{
	uint8_t *img = (uint8_t *) src->img;
	uint32_t i = src->width * src->height;
	
	if(src->length << 2 < i) return(-1);
	
	while(i-- > 0)
	{
		*(abitmap++) += *(img++);
		*(abitmap++) += *(img++);
		*(abitmap++) += *(img++);
		img++;
	}
	
	return(0);
}

int fswc_add_image_bgr32(src_t *src, avgbmp_t *abitmap)
{
	uint8_t *img = (uint8_t *) src->img;
	uint32_t p, i = src->width * src->height;
	
	if(src->length << 2 < i) return(-1);
	
	for(p = 0; p < i; p += 4)
	{
		abitmap[0] += img[2];
		abitmap[1] += img[1];
		abitmap[2] += img[0];
		abitmap += 3;
		img += 4;
	}
	
	return(0);
}

int fswc_add_image_rgb24(src_t *src, avgbmp_t *abitmap)
{
	uint8_t *img = (uint8_t *) src->img;
	uint32_t i = src->width * src->height * 3;
	
	if(src->length < i) return(-1);
	while(i-- > 0) *(abitmap++) += *(img++);
	
	return(0);
}

int fswc_add_image_bgr24(src_t *src, avgbmp_t *abitmap)
{
	uint8_t *img = (uint8_t *) src->img;
	uint32_t p, i = src->width * src->height * 3;
	
	if(src->length < i) return(-1);
	
	for(p = 0; p < src->length; p += 3)
	{
		abitmap[0] += img[2];
		abitmap[1] += img[1];
		abitmap[2] += img[0];
		abitmap += 3;
		img += 3;
	}
	
	return(0);
}

int fswc_add_image_rgb565(src_t *src, avgbmp_t *abitmap)
{
	uint16_t *img = (uint16_t *) src->img;
	uint32_t i = src->width * src->height;
	
	if(src->length >> 1 < i) return(-1);
	
	while(i-- > 0)
	{
		uint8_t r, g, b;
		
		r = (*img & 0xF800) >> 8;
		g = (*img &  0x7E0) >> 3;
		b = (*img &   0x1F) << 3;
		
		*(abitmap++) += r + (r >> 5);
		*(abitmap++) += g + (g >> 6);
		*(abitmap++) += b + (b >> 5);
		
		img++;
	}
	
	return(0);
}

int fswc_add_image_rgb555(src_t *src, avgbmp_t *abitmap)
{
	uint16_t *img = (uint16_t *) src->img;
	uint32_t i = src->width * src->height;
	
	if(src->length >> 1 < i) return(-1);
	
	while(i-- > 0)
	{
		uint8_t r, g, b;
		
		r = (*img & 0x7C00) >> 7;
		g = (*img &  0x3E0) >> 2;
		b = (*img &   0x1F) << 3;
		
		*(abitmap++) += r + (r >> 5);
		*(abitmap++) += g + (g >> 5);
		*(abitmap++) += b + (b >> 5);
		
		img++;
	}
	
	return(0);
}

