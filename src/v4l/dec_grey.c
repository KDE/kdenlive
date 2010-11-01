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

int fswc_add_image_y16(src_t *src, avgbmp_t *abitmap)
{
	uint16_t *bitmap = (uint16_t *) src->img;
	uint32_t i = src->width * src->height;
	
	if(src->length < i) return(-1);
	
	while(i-- > 0)
	{
		*(abitmap++) += *bitmap >> 8;
		*(abitmap++) += *bitmap >> 8;
		*(abitmap++) += *(bitmap++) >> 8;
	}
	
	return(0);
}

int fswc_add_image_grey(src_t *src, avgbmp_t *abitmap)
{
	uint8_t *bitmap = (uint8_t *) src->img;
	uint32_t i = src->width * src->height;
	
	if(src->length < i) return(-1);
	
	while(i-- > 0)
	{
		*(abitmap++) += *bitmap;
		*(abitmap++) += *bitmap;
		*(abitmap++) += *(bitmap++);
	}
	
	return(0);
}

