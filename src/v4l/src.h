/* fswebcam - FireStorm.cx's webcam generator                 */
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

#include <stdint.h>
#include <sys/time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <fcntl.h>

#ifndef INC_SRC_H
#define INC_SRC_H

#define SRC_TYPE_NONE   (0)
#define SRC_TYPE_DEVICE (1 << 0) /* Can capture from a device */
#define SRC_TYPE_FILE   (1 << 1) /* Can capture from a file */

/* When updating the palette list remember to update src_palette[] in src.c */

#define SRC_PAL_ANY     (-1)
#define SRC_PAL_PNG     (0)
#define SRC_PAL_JPEG    (1)
#define SRC_PAL_MJPEG   (2)
#define SRC_PAL_S561    (3)
#define SRC_PAL_RGB32   (4)
#define SRC_PAL_BGR32   (5)
#define SRC_PAL_RGB24   (6)
#define SRC_PAL_BGR24   (7)
#define SRC_PAL_YUYV    (8)
#define SRC_PAL_UYVY    (9)
#define SRC_PAL_YUV420P (10)
#define SRC_PAL_NV12MB  (11)
#define SRC_PAL_BAYER   (12)
#define SRC_PAL_SGBRG8  (13)
#define SRC_PAL_SGRBG8  (14)
#define SRC_PAL_RGB565  (15)
#define SRC_PAL_RGB555  (16)
#define SRC_PAL_Y16     (17)
#define SRC_PAL_GREY    (18)

#define SRC_LIST_INPUTS     (1 << 1)
#define SRC_LIST_TUNERS     (1 << 2)
#define SRC_LIST_FORMATS    (1 << 3)
#define SRC_LIST_CONTROLS   (1 << 4)
#define SRC_LIST_FRAMESIZES (1 << 5)
#define SRC_LIST_FRAMERATES (1 << 6)

/* The SCALE macro converts a value (sv) from one range (sf -> sr)
   to another (df -> dr). */
#define SCALE(df, dr, sf, sr, sv) (((sv - sf) * (dr - df) / (sr - sf)) + df)

typedef struct {
        char    *name;
} src_palette_t;

extern src_palette_t src_palette[];

typedef struct {
	char *name;
	char *value;
} src_option_t;

typedef struct {
	
	/* Source Options */
	char *source;
	uint8_t type;
	
	void *state;
	
	/* Last captured image */
	uint32_t length;
	void *img;
	
	/* Input Options */
	char    *input;
	uint8_t  tuner;
	uint32_t frequency;
	uint32_t delay;
	uint32_t timeout;
	char     use_read;
	
	/* List Options */
	uint8_t list;
	
	/* Image Options */
	int palette;
	uint32_t width;
	uint32_t height;
	uint32_t fps;
	
	src_option_t **option;
	
	/* For calculating capture FPS */
	uint32_t captured_frames;
	struct timeval tv_first;
	struct timeval tv_last;
	
} src_t;

typedef struct {
	
	char *name;
	
	uint8_t flags;
	
	int (*open)(src_t *);
	int (*close)(src_t *);
	int (*grab)(src_t *);
	const char *(*query)(src_t *);
	
} src_mod_t;

typedef struct {
	
	/* List of options. */
	char *opts;
	const struct option *long_opts;
	
	/* When reading from the command line. */
	int opt_index;
	
	/* When reading from a configuration file. */
	char *filename;
	FILE *f;
	size_t line;
	
} fswc_getopt_t;

typedef struct {
	uint16_t id;
	char    *options;
} fswebcam_job_t;

typedef struct {

	/* General options. */
	unsigned long loop;
	signed long offset;
	unsigned char background;
	char *pidfile;
	char *logfile;
	char gmt;

	/* Capture start time. */
	time_t start;

	/* Device options. */
	char *device;
	char *input;
	unsigned char tuner;
	unsigned long frequency;
	unsigned long delay;
	char use_read;
	uint8_t list;

	/* Image capture options. */
	unsigned int width;
	unsigned int height;
	unsigned int frames;
	unsigned int fps;
	unsigned int skipframes;
	int palette;
	src_option_t **option;
	char *dumpframe;

	/* Job queue. */
	uint8_t jobs;
	fswebcam_job_t **job;

	/* Banner options. */
	char banner;
	uint32_t bg_colour;
	uint32_t bl_colour;
	uint32_t fg_colour;
	char *title;
	char *subtitle;
	char *timestamp;
	char *info;
	char *font;
	int fontsize;
	char shadow;

	/* Overlay options. */
	char *underlay;
	char *overlay;

	/* Output options. */
	char *filename;
	char format;
	char compression;

} fswebcam_config_t;

extern int src_open(src_t *src, char *source);
extern int src_close(src_t *src);
extern int src_grab(src_t *src);
extern const char *src_query(src_t *src, char *source);

extern int src_set_option(src_option_t ***options, char *name, char *value);
extern int src_get_option_by_number(src_option_t **opt, int number, char **name, char **value);
extern int src_get_option_by_name(src_option_t **opt, char *name, char **value);
extern int src_free_options(src_option_t ***options);

#endif


#ifdef __cplusplus
}
#endif

