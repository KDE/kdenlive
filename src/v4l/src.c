/* fswebcam - FireStorm.cx's webcam generator                 */
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

#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <sys/stat.h>
#include <errno.h>
#include "src.h"

#ifdef HAVE_V4L2
extern src_mod_t src_v4l2;
#endif
#ifdef HAVE_V4L1
extern src_mod_t src_v4l1;
#endif

/* Supported palette types. */
src_palette_t src_palette[] = {
	{ "PNG" },
	{ "JPEG" },
	{ "MJPEG" },
	{ "S561" },
	{ "RGB32" },
	{ "BGR32" },
	{ "RGB24" },
	{ "BGR24" },
	{ "YUYV" },
	{ "UYVY" },
	{ "YUV420P" },
	{ "NV12MB" },
	{ "BAYER" },
	{ "SGBRG8" },
	{ "SGRBG8" },
	{ "RGB565" },
	{ "RGB555" },
	{ "Y16" },
	{ "GREY" },
	{ NULL }
};


int src_open(src_t *src, char *source)
{
	int i = 0;
	struct stat st;
	
	if(!source)
	{
		fprintf(stderr, "No source was specified.......");
		return(-1);
	}
	src->source = source;
	
	i = 0;
	int r = src_v4l2.flags;
	if(S_ISCHR(st.st_mode) && r & SRC_TYPE_DEVICE) r = -1;
	else if(!S_ISCHR(st.st_mode) && r & SRC_TYPE_FILE) r = -1;
	else r = 0;
	src->type = 0;
	r = src_v4l2.open(src);
	if(r == -2) return(-1);

	/*
	int frame;
	for(frame = 0; frame < config->skipframes; frame++)
	    if(src_grab(src) == -1) break;*/

	return 0;
}

const char *src_query(src_t *src, char *source)
{
    src->source = source;
    return src_v4l2.query(src);
}

int src_close(src_t *src)
{
	int r;
	
	if(src->captured_frames)
	{
		double seconds =
		   (src->tv_last.tv_sec  + src->tv_last.tv_usec  / 1000000.0) -
		   (src->tv_first.tv_sec + src->tv_first.tv_usec / 1000000.0);
		
		/* Display FPS if enough frames where captured. */
		if(src->captured_frames == 1)
		{
			/*MSG("Captured frame in %0.2f seconds.", seconds);*/
		}
		else if(src->captured_frames < 3)
		{
			/*MSG("Captured %i frames in %0.2f seconds.",
			    src->captured_frames, seconds);*/
		}
		else
		{
			/*MSG("Captured %i frames in %0.2f seconds. (%i fps)",
			    src->captured_frames, seconds,
			    (int) (src->captured_frames / seconds));*/
		}
	}
	
	r = src_v4l2.close(src);
	
	if(src->source) free(src->source);
	
	return(r);
}

int src_grab(src_t *src)
{
	int r = src_v4l2.grab(src);
	
	if(!r)
	{
		if(!src->captured_frames) gettimeofday(&src->tv_first, NULL);
		gettimeofday(&src->tv_last, NULL);
		
		src->captured_frames++;
	}
	
	return(r);
}

/* Pointers are great things. Terrible things yes, but great. */
/* These work but are very ugly and will be re-written soon. */

int src_set_option(src_option_t ***options, char *name, char *value)
{
	src_option_t **opts, *opt;
	int count;
	
	if(!options) return(-1);
	if(!*options)
	{
		*options = malloc(sizeof(src_option_t *));
		if(!*options)
		{
			/*ERROR("Out of memory.");*/
			return(-1);
		}
		
		*options[0] = NULL;
	}
	
	count = 0;
	opts = *options;
	while(*opts)
	{
		if((*opts)->name) if(!strcasecmp(name, (*opts)->name)) break;
		opts++;
		count++;
	}
	
	if(!*opts)
	{
		void *new;
		
		opt = (src_option_t *) malloc(sizeof(src_option_t));
		if(!opt)
		{
			/*ERROR("Out of memory.");*/
			return(-1);
		}
		
		new = realloc(*options, sizeof(src_option_t *) * (count + 2));
		if(!new)
		{
			free(opt);
			/*ERROR("Out of memory.");*/
			return(-1);
		}
		
		*options = (src_option_t **) new;
		(*options)[count++] = opt;
		(*options)[count++] = NULL;
		
		opt->name = strdup(name);
		opt->value = NULL;
	}
	else opt = *opts;
	
	if(opt->value)
	{
		free(opt->value);
		opt->value = NULL;
	}
	if(value) opt->value = strdup(value);
	
	return(0);
}

int src_get_option_by_number(src_option_t **opt, int number,
                             char **name, char **value)
{
	int i;
	
	if(!opt || !name || !value) return(-1);
	
	i = 0;
	while(*opt)
	{
		if(i == number)
		{
			*name  = (*opt)->name;
			*value = (*opt)->value;
			return(0);
		}
		
		i++;
	}
	
	return(-1);
}

int src_get_option_by_name(src_option_t **opt, char *name, char **value)
{
	if(!opt || !name || !value) return(-1);
	
	while(*opt)
	{
		if((*opt)->name)
		{
			if(!strcasecmp(name, (*opt)->name))
			{
				*value = (*opt)->value;
				return(0);
			}
		}
		
		opt++;
	}
	
	return(-1);
}

int src_free_options(src_option_t ***options)
{
	src_option_t **opts;
	
	if(!options || !*options) return(-1);
	
	opts = *options;
        while(*opts)
	{
		if((*opts)->name)  free((*opts)->name);
		if((*opts)->value) free((*opts)->value);
		
		free(*opts);
		
		opts++;
	}
	
	free(*options);
	*options = NULL;
	
	return(0);
}

