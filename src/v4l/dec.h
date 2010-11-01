/* fswebcam - Small and simple webcam for *nix                */
/*============================================================*/
/* Copyright (C)2005-2010 Philip Heron <phil@sanslogic.co.uk> */
/*                                                            */
/* This program is distributed under the terms of the GNU     */
/* General Public License, version 2. You may use, modify,    */
/* and redistribute it under the terms of this license. A     */
/* copy should be included with this source.                  */

#ifdef __cplusplus
extern "C" {
 #endif

#ifndef INC_DEC_H
#define INC_DEC_H

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

extern int fswc_add_image_bayer(avgbmp_t *dst, uint8_t *img, uint32_t length, uint32_t w, uint32_t h, int palette);

extern int fswc_add_image_y16(src_t *src, avgbmp_t *abitmap);
extern int fswc_add_image_grey(src_t *src, avgbmp_t *abitmap);

extern int verify_jpeg_dht(uint8_t *src,  uint32_t lsrc, uint8_t **dst, uint32_t *ldst);

extern int fswc_add_image_png(src_t *src, avgbmp_t *abitmap);

extern int fswc_add_image_rgb32(src_t *src, avgbmp_t *abitmap);
extern int fswc_add_image_bgr32(src_t *src, avgbmp_t *abitmap);
extern int fswc_add_image_rgb24(src_t *src, avgbmp_t *abitmap);
extern int fswc_add_image_bgr24(src_t *src, avgbmp_t *abitmap);
extern int fswc_add_image_rgb565(src_t *src, avgbmp_t *abitmap);
extern int fswc_add_image_rgb555(src_t *src, avgbmp_t *abitmap);

extern int fswc_add_image_yuyv(src_t *src, avgbmp_t *abitmap);
extern int fswc_add_image_yuv420p(src_t *src, avgbmp_t *abitmap);
extern int fswc_add_image_nv12mb(src_t *src, avgbmp_t *abitmap);

extern int fswc_add_image_s561(avgbmp_t *dst, uint8_t *img, uint32_t length, uint32_t width, uint32_t height, int palette);

#endif

#ifdef __cplusplus
}
#endif


