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

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include "videodev2.h"
#include "src.h"

#ifdef HAVE_V4L2

typedef struct {
	void *start;
	size_t length;
} v4l2_buffer_t;

typedef struct {
	
	int fd;
	char map;
	
	struct v4l2_capability cap;
	struct v4l2_format fmt;
	struct v4l2_requestbuffers req;
	struct v4l2_buffer buf;
	
	v4l2_buffer_t *buffer;
	
	int pframe;
	
} src_v4l2_t;

static int src_v4l2_close(src_t *src);

typedef struct {
	uint16_t src;
	uint32_t v4l2;
} v4l2_palette_t;

v4l2_palette_t v4l2_palette[] = {
	{ SRC_PAL_JPEG,    V4L2_PIX_FMT_JPEG   },
	{ SRC_PAL_MJPEG,   V4L2_PIX_FMT_MJPEG  },
	{ SRC_PAL_S561,    V4L2_PIX_FMT_SPCA561 },
	{ SRC_PAL_RGB24,   V4L2_PIX_FMT_RGB24  },
	{ SRC_PAL_BGR24,   V4L2_PIX_FMT_BGR24  },
	{ SRC_PAL_RGB32,   V4L2_PIX_FMT_RGB32  },
	{ SRC_PAL_BGR32,   V4L2_PIX_FMT_BGR32  },
	{ SRC_PAL_YUYV,    V4L2_PIX_FMT_YUYV   },
	{ SRC_PAL_UYVY,    V4L2_PIX_FMT_UYVY   },
	{ SRC_PAL_YUV420P, V4L2_PIX_FMT_YUV420 },
	{ SRC_PAL_BAYER,   V4L2_PIX_FMT_SBGGR8 },
	{ SRC_PAL_SGBRG8,  V4L2_PIX_FMT_SGBRG8 },
	{ SRC_PAL_SGRBG8,  V4L2_PIX_FMT_SGRBG8 },
	{ SRC_PAL_RGB565,  V4L2_PIX_FMT_RGB565 },
	{ SRC_PAL_RGB555,  V4L2_PIX_FMT_RGB555 },
	{ SRC_PAL_Y16,     V4L2_PIX_FMT_Y16    },
	{ SRC_PAL_GREY,    V4L2_PIX_FMT_GREY   },
	{ 0, 0 }
};

int src_v4l2_get_capability(src_t *src)
{
	src_v4l2_t *s = (src_v4l2_t *) src->state;
	
	if(ioctl(s->fd, VIDIOC_QUERYCAP, &s->cap) < 0)
	{
		/*ERROR("%s: Not a V4L2 device?", src->source);*/
		return(-1);
	}
	fprintf(stderr, "cap.card: \"%s\"", s->cap.card);
	/*DEBUG("%s information:", src->source);
	DEBUG("cap.driver: \"%s\"", s->cap.driver);
	DEBUG("cap.card: \"%s\"", s->cap.card);
	DEBUG("cap.bus_info: \"%s\"", s->cap.bus_info);
	DEBUG("cap.capabilities=0x%08X", s->cap.capabilities);*/
	/*if(s->cap.capabilities & V4L2_CAP_VIDEO_CAPTURE) DEBUG("- VIDEO_CAPTURE");
	if(s->cap.capabilities & V4L2_CAP_VIDEO_OUTPUT)  DEBUG("- VIDEO_OUTPUT");
	if(s->cap.capabilities & V4L2_CAP_VIDEO_OVERLAY) DEBUG("- VIDEO_OVERLAY");
	if(s->cap.capabilities & V4L2_CAP_VBI_CAPTURE)   DEBUG("- VBI_CAPTURE");
	if(s->cap.capabilities & V4L2_CAP_VBI_OUTPUT)    DEBUG("- VBI_OUTPUT");
	if(s->cap.capabilities & V4L2_CAP_RDS_CAPTURE)   DEBUG("- RDS_CAPTURE");
	if(s->cap.capabilities & V4L2_CAP_TUNER)         DEBUG("- TUNER");
	if(s->cap.capabilities & V4L2_CAP_AUDIO)         DEBUG("- AUDIO");
	if(s->cap.capabilities & V4L2_CAP_RADIO)         DEBUG("- RADIO");
	if(s->cap.capabilities & V4L2_CAP_READWRITE)     DEBUG("- READWRITE");
	if(s->cap.capabilities & V4L2_CAP_ASYNCIO)       DEBUG("- ASYNCIO");
	if(s->cap.capabilities & V4L2_CAP_STREAMING)     DEBUG("- STREAMING");
	if(s->cap.capabilities & V4L2_CAP_TIMEPERFRAME)  DEBUG("- TIMEPERFRAME");*/
	
	if(!s->cap.capabilities & V4L2_CAP_VIDEO_CAPTURE)
	{
		/*ERROR("Device does not support capturing.");*/
		return(-1);
	}
	
	return(0);
}

int src_v4l2_set_input(src_t *src)
{
	src_v4l2_t *s = (src_v4l2_t *) src->state;
	struct v4l2_input input;
	int count = 0, i = -1;
	
	memset(&input, 0, sizeof(input));
	
	if(src->list & SRC_LIST_INPUTS)
	{
		/*HEAD("--- Available inputs:");*/
		
		input.index = count;
		while(!ioctl(s->fd, VIDIOC_ENUMINPUT, &input))
		{
			/*MSG("%i: %s", count, input.name);*/
			input.index = ++count;
		}
	}
	
	/* If no input was specified, use input 0. */
	if(!src->input)
	{
		/*MSG("No input was specified, using the first.");*/
		count = 1;
		i = 0;
	}
	
	/* Check if the input is specified by name. */
	if(i == -1)
	{
		input.index = count;
		while(!ioctl(s->fd, VIDIOC_ENUMINPUT, &input))
		{
			if(!strncasecmp((char *) input.name, src->input, 32))
				i = count;
			input.index = ++count;
		}
	}
	
	if(i == -1)
	{
		char *endptr;
		
		/* Is the input specified by number? */
		i = strtol(src->input, &endptr, 10);
		
		if(endptr == src->input) i = -1;
	}
	
	if(i == -1 || i >= count)
	{
		/* The specified input wasn't found! */
		/*ERROR("Unrecognised input \"%s\"", src->input);*/
		return(-1);
	}
	
	/* Set the input. */
	input.index = i;
	if(ioctl(s->fd, VIDIOC_ENUMINPUT, &input) == -1)
	{
		/*ERROR("Unable to query input %i.", i);
		ERROR("VIDIOC_ENUMINPUT: %s", strerror(errno));*/
		return(-1);
	}
	
	/*DEBUG("%s: Input %i information:", src->source, i);
	DEBUG("name = \"%s\"", input.name);
	DEBUG("type = %08X", input.type);
	if(input.type & V4L2_INPUT_TYPE_TUNER) DEBUG("- TUNER");
	if(input.type & V4L2_INPUT_TYPE_CAMERA) DEBUG("- CAMERA");
	DEBUG("audioset = %08X", input.audioset);
	DEBUG("tuner = %08X", input.tuner);
	DEBUG("status = %08X", input.status);
	if(input.status & V4L2_IN_ST_NO_POWER) DEBUG("- NO_POWER");
	if(input.status & V4L2_IN_ST_NO_SIGNAL) DEBUG("- NO_SIGNAL");
	if(input.status & V4L2_IN_ST_NO_COLOR) DEBUG("- NO_COLOR");
	if(input.status & V4L2_IN_ST_NO_H_LOCK) DEBUG("- NO_H_LOCK");
	if(input.status & V4L2_IN_ST_COLOR_KILL) DEBUG("- COLOR_KILL");
	if(input.status & V4L2_IN_ST_NO_SYNC) DEBUG("- NO_SYNC");
	if(input.status & V4L2_IN_ST_NO_EQU) DEBUG("- NO_EQU");
	if(input.status & V4L2_IN_ST_NO_CARRIER) DEBUG("- NO_CARRIER");
	if(input.status & V4L2_IN_ST_MACROVISION) DEBUG("- MACROVISION");
	if(input.status & V4L2_IN_ST_NO_ACCESS) DEBUG("- NO_ACCESS");
	if(input.status & V4L2_IN_ST_VTR) DEBUG("- VTR");*/
	
	if(ioctl(s->fd, VIDIOC_S_INPUT, &i) == -1)
	{
		/*ERROR("Error selecting input %i", i);
		ERROR("VIDIOC_S_INPUT: %s", strerror(errno));*/
		return(-1);
	}
	
	/* If this input is attached to a tuner, set the frequency. */
	if(input.type & V4L2_INPUT_TYPE_TUNER)
	{
		char *range;
		struct v4l2_tuner tuner;
		struct v4l2_frequency freq;
		
		/* Query the tuners capabilities. */
		
		memset(&tuner, 0, sizeof(tuner));
		tuner.index = input.tuner;
		
		if(ioctl(s->fd, VIDIOC_G_TUNER, &tuner) == -1)
		{
			/*WARN("Error querying tuner %i.", input.tuner);
			WARN("VIDIOC_G_TUNER: %s", strerror(errno));*/
			return(0);
		}
		
		if(tuner.capability & V4L2_TUNER_CAP_LOW) range = "kHz";
		else range = "MHz";
		
		/*DEBUG("%s: Tuner %i information:", src->source, input.tuner);
		DEBUG("name = \"%s\"", tuner.name);
		DEBUG("type = %08X", tuner.type);
		if(tuner.type == V4L2_TUNER_RADIO) DEBUG("- RADIO");
		if(tuner.type == V4L2_TUNER_ANALOG_TV) DEBUG("- ANALOG_TV");
		DEBUG("capability = %08X", tuner.capability);
		if(tuner.capability & V4L2_TUNER_CAP_LOW) DEBUG("- LOW");
		if(tuner.capability & V4L2_TUNER_CAP_NORM) DEBUG("- NORM");
		if(tuner.capability & V4L2_TUNER_CAP_STEREO) DEBUG("- STEREO");
		if(tuner.capability & V4L2_TUNER_CAP_LANG1) DEBUG("- LANG1");
		if(tuner.capability & V4L2_TUNER_CAP_LANG2) DEBUG("- LANG2");
		if(tuner.capability & V4L2_TUNER_CAP_SAP) DEBUG("- SAP");
		DEBUG("rangelow = %08X, (%.3f%s)", tuner.rangelow, (double) tuner.rangelow * 16 / 1000, range);
		DEBUG("rangehigh = %08X, (%.3f%s)", tuner.rangehigh, (double) tuner.rangehigh * 16 / 1000, range);
		DEBUG("signal = %08X", tuner.signal);
		DEBUG("afc = %08X", tuner.afc);*/
		
		/* Set the frequency. */
		memset(&freq, 0, sizeof(freq));
		freq.tuner = input.tuner;
		freq.type = V4L2_TUNER_ANALOG_TV;
		freq.frequency = (src->frequency / 1000) * 16;
		
		if(ioctl(s->fd, VIDIOC_S_FREQUENCY, &freq) == -1)
		{
			/*WARN("Error setting frequency %.3f%s", src->frequency / 16.0, range);
			WARN("VIDIOC_S_FREQUENCY: %s", strerror(errno));*/
			return(0);
		}
		
		/*MSG("Set frequency to %.3f%s",
		    (double) src->frequency / 1000, range);*/
	}
	
	return(0);
}

int src_v4l2_show_control(src_t *src, struct v4l2_queryctrl *queryctrl)
{
	src_v4l2_t *s = (src_v4l2_t *) src->state;
	struct v4l2_querymenu querymenu;
	struct v4l2_control control;
	char *t;
	int m;
	
	if(queryctrl->flags & V4L2_CTRL_FLAG_DISABLED) return(0);
	
	memset(&querymenu, 0, sizeof(querymenu));
	memset(&control, 0, sizeof(control));
	
	if(queryctrl->type != V4L2_CTRL_TYPE_BUTTON)
	{
		control.id = queryctrl->id;
		if(ioctl(s->fd, VIDIOC_G_CTRL, &control))
		{
			/*ERROR("Error reading value of control '%s'.", queryctrl->name);
			ERROR("VIDIOC_G_CTRL: %s", strerror(errno));*/
		}
	}
	
	switch(queryctrl->type)
	{
	case V4L2_CTRL_TYPE_INTEGER:
		
		t = malloc(64); /* Ick ... TODO: re-write this. */
		if(!t)
		{
			/*ERROR("Out of memory.");*/
			return(-1);
		}
		
		if(queryctrl->maximum - queryctrl->minimum <= 10)
		{
			snprintf(t, 63, "%i", control.value);
		}
		else
		{
			snprintf(t, 63, "%i (%i%%)",
			         control.value,
			         SCALE(0, 100,
			               queryctrl->minimum,
			               queryctrl->maximum,
			               control.value));
		}
		
		/*MSG("%-25s %-15s %i - %i", queryctrl->name, t,
		    queryctrl->minimum, queryctrl->maximum);*/
		
		free(t);
		
		break;
		
	case V4L2_CTRL_TYPE_BOOLEAN:
		/*MSG("%-25s %-15s True | False", queryctrl->name,
		    (control.value ? "True" : "False"));*/
		break;
		
	case V4L2_CTRL_TYPE_MENU:
		
		querymenu.id = queryctrl->id;
		
		t = calloc((queryctrl->maximum - queryctrl->minimum) + 1, 34);
		m = queryctrl->minimum;
		for(m = queryctrl->minimum; m <= queryctrl->maximum; m++)
		{
			querymenu.index = m;
			if(!ioctl(s->fd, VIDIOC_QUERYMENU, &querymenu))
			{
				strncat(t, (char *) querymenu.name, 32);
				if(m < queryctrl->maximum) strncat(t, " | ", 3);
			}
		}
		
		querymenu.index = control.value;
		if(ioctl(s->fd, VIDIOC_QUERYMENU, &querymenu))
		{
			free(t);
			/*ERROR("Error reading value of menu item %i for control '%s'",
			      control.value, queryctrl->name);
			ERROR("VIDIOC_QUERYMENU: %s", strerror(errno));*/
			return(0);
		}
		
		/*MSG("%-25s %-15s %s", queryctrl->name, querymenu.name, t);*/
		free(t);
		
		break;
	
	case V4L2_CTRL_TYPE_BUTTON:
		/*MSG("%-25s %-15s %s", queryctrl->name, "-", "[Button]");*/
		break;
		
	default:
		/*MSG("%-25s %-15s %s", queryctrl->name, "N/A", "[Unknown Control Type]");*/
		break;
	}
	
	return(0);
}

int src_v4l2_set_control(src_t *src, struct v4l2_queryctrl *queryctrl)
{
	src_v4l2_t *s = (src_v4l2_t *) src->state;
	struct v4l2_control control;
	struct v4l2_querymenu querymenu;
	char *sv;
	int iv;
	
	if(queryctrl->flags & V4L2_CTRL_FLAG_DISABLED) return(0);
	if(src_get_option_by_name(src->option, (char *) queryctrl->name, &sv))
		return(0);
	
	memset(&querymenu, 0, sizeof(querymenu));
	memset(&control, 0, sizeof(control));
	
	control.id = queryctrl->id;
	
	switch(queryctrl->type)
	{
	case V4L2_CTRL_TYPE_INTEGER:
		
		/* Convert the value to an integer. */
		iv = atoi(sv);
		
		/* Is the value a precentage? */
		if(strchr(sv, '%'))
		{
			/* Adjust the precentage to fit the controls range. */
			iv = SCALE(queryctrl->minimum, queryctrl->maximum,
			           0, 100, iv);
		}
		
		/*MSG("Setting %s to %i (%i%%).", queryctrl->name, iv,
		    SCALE(0, 100, queryctrl->minimum, queryctrl->maximum, iv));*/
		
		/*if(iv < queryctrl->minimum || iv > queryctrl->maximum)
			WARN("Value is out of range. Setting anyway.");*/
		
		control.value = iv;
		ioctl(s->fd, VIDIOC_S_CTRL, &control);
		break;
	
	case V4L2_CTRL_TYPE_BOOLEAN:
		
		iv = -1;
		if(!strcasecmp(sv, "1") || !strcasecmp(sv, "true")) iv = 1;
		if(!strcasecmp(sv, "0") || !strcasecmp(sv, "false")) iv = 0;
		
		if(iv == -1)
		{
			/*WARN("Unknown boolean value '%s' for %s.",
			     sv, queryctrl->name);*/
			return(-1);
		}
		
		/*MSG("Setting %s to %s (%i).", queryctrl->name, sv, iv);*/
		
		control.value = iv;
		ioctl(s->fd, VIDIOC_S_CTRL, &control);
		
		break;
	
	case V4L2_CTRL_TYPE_MENU:
		
		/* Scan for a matching value. */
		querymenu.id = queryctrl->id;
		
		for(iv = queryctrl->minimum; iv <= queryctrl->maximum; iv++)
		{
			querymenu.index = iv;
			
			if(ioctl(s->fd, VIDIOC_QUERYMENU, &querymenu))
			{
				/*ERROR("Error querying menu.");*/
				continue;
			}
			
			if(!strncasecmp((char *) querymenu.name, sv, 32))
				break;
		}
		
		if(iv > queryctrl->maximum)
		{
			/*MSG("Unknown value '%s' for %s.", sv, queryctrl->name);*/
			return(-1);
		}
		
		/*MSG("Setting %s to %s (%i).",
		    queryctrl->name, querymenu.name, iv);*/
		
		control.value = iv;
		ioctl(s->fd, VIDIOC_S_CTRL, &control);
		
		break;
	
	case V4L2_CTRL_TYPE_BUTTON:
		
		/*MSG("Triggering %s control.", queryctrl->name);
		ioctl(s->fd, VIDIOC_S_CTRL, &control);*/
		
		break;
	
	default:
		/*WARN("Not setting unknown control type %i (%s).",
		     queryctrl->name);*/
		break;
	}
	
	return(0);
}

int src_v4l2_set_controls(src_t *src)
{
	src_v4l2_t *s = (src_v4l2_t *) src->state;
	struct v4l2_queryctrl queryctrl;
	int c;
	
	memset(&queryctrl, 0, sizeof(queryctrl));
	
	if(src->list & SRC_LIST_CONTROLS)
	{
		/*HEAD("%-25s %-15s %s", "Available Controls", "Current Value", "Range");
		MSG("%-25s %-15s %s",  "------------------", "-------------", "-----");*/
		
		/* Display normal controls. */
		for(c = V4L2_CID_BASE; c < V4L2_CID_LASTP1; c++)
		{
			queryctrl.id = c;
			
			if(ioctl(s->fd, VIDIOC_QUERYCTRL, &queryctrl)) continue;
			src_v4l2_show_control(src, &queryctrl);
		}
		
		/* Display device-specific controls. */
		for(c = V4L2_CID_PRIVATE_BASE; ; c++)
		{
			queryctrl.id = c;
			
			if(ioctl(s->fd, VIDIOC_QUERYCTRL, &queryctrl)) break;
			src_v4l2_show_control(src, &queryctrl);
		}
	}
	
	/* Scan normal controls. */
	for(c = V4L2_CID_BASE; c < V4L2_CID_LASTP1; c++)
	{
		queryctrl.id = c;
		
		if(ioctl(s->fd, VIDIOC_QUERYCTRL, &queryctrl)) continue;
		src_v4l2_set_control(src, &queryctrl);
	}
	
	/* Scan device-specific controls. */
	for(c = V4L2_CID_PRIVATE_BASE; ; c++)
	{
		queryctrl.id = c;
		
		if(ioctl(s->fd, VIDIOC_QUERYCTRL, &queryctrl)) break;
		src_v4l2_set_control(src, &queryctrl);
	}
	
	return(0);
}

int src_v4l2_set_pix_format(src_t *src)
{
	src_v4l2_t *s = (src_v4l2_t *) src->state;
	struct v4l2_fmtdesc fmt;
	int v4l2_pal;
	
	/* Dump a list of formats the device supports. */
	/*DEBUG("Device offers the following V4L2 pixel formats:");*/
	
	v4l2_pal = 0;
	memset(&fmt, 0, sizeof(fmt));
	fmt.index = v4l2_pal;
	fmt.type  = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	
	while(ioctl(s->fd, VIDIOC_ENUM_FMT, &fmt) != -1)
	{
		/*DEBUG("%i: [0x%08X] '%c%c%c%c' (%s)", v4l2_pal,
		      fmt.pixelformat,
		      fmt.pixelformat >> 0,  fmt.pixelformat >> 8,
		      fmt.pixelformat >> 16, fmt.pixelformat >> 24,
		      fmt.description);*/
		
		memset(&fmt, 0, sizeof(fmt));
		fmt.index = ++v4l2_pal;
		fmt.type  = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	}
	
	/* Step through each palette type. */
	v4l2_pal = 0;
	
	if(src->palette != -1)
	{
		while(v4l2_palette[v4l2_pal].v4l2)
		{
			if(v4l2_palette[v4l2_pal].src == src->palette) break;
			v4l2_pal++;
		}
		
		if(!v4l2_palette[v4l2_pal].v4l2)
		{
			/*ERROR("Unable to handle palette format %s.",
			      src_palette[src->palette]);*/
			
			return(-1);
		}
	}
	
	while(v4l2_palette[v4l2_pal].v4l2)
	{
		/* Try the palette... */
		memset(&s->fmt, 0, sizeof(s->fmt));
		s->fmt.type                = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		s->fmt.fmt.pix.width       = src->width;
		s->fmt.fmt.pix.height      = src->height;
		s->fmt.fmt.pix.pixelformat = v4l2_palette[v4l2_pal].v4l2;
		s->fmt.fmt.pix.field       = V4L2_FIELD_ANY;
		
		if(ioctl(s->fd, VIDIOC_TRY_FMT, &s->fmt) != -1 &&
		   s->fmt.fmt.pix.pixelformat == v4l2_palette[v4l2_pal].v4l2)
		{
			src->palette = v4l2_palette[v4l2_pal].src;
			
			/*INFO("Using palette %s", src_palette[src->palette].name);*/
			
			if(s->fmt.fmt.pix.width != src->width ||
			   s->fmt.fmt.pix.height != src->height)
			{
				/*MSG("Adjusting resolution from %ix%i to %ix%i.",
				    src->width, src->height,
				    s->fmt.fmt.pix.width,
				    s->fmt.fmt.pix.height);*/
				src->width = s->fmt.fmt.pix.width;
				src->height = s->fmt.fmt.pix.height;
                        }
			
			if(ioctl(s->fd, VIDIOC_S_FMT, &s->fmt) == -1)
			{
				/*ERROR("Error setting pixel format.");
				ERROR("VIDIOC_S_FMT: %s", strerror(errno));*/
				return(-1);
			}
			
			if(v4l2_palette[v4l2_pal].v4l2 == V4L2_PIX_FMT_MJPEG)
			{
				struct v4l2_jpegcompression jpegcomp;
				
				memset(&jpegcomp, 0, sizeof(jpegcomp));
				ioctl(s->fd, VIDIOC_G_JPEGCOMP, &jpegcomp);
				jpegcomp.jpeg_markers |= V4L2_JPEG_MARKER_DHT;
				ioctl(s->fd, VIDIOC_S_JPEGCOMP, &jpegcomp);
			}
			
			return(0);
		}
		
		if(src->palette != -1) break;
		
		v4l2_pal++;
	}
	
	/*ERROR("Unable to find a compatible palette format.");*/
	
	return(-1);
}

int src_v4l2_set_fps(src_t *src)
{
	src_v4l2_t *s = (src_v4l2_t *) src->state;
	struct v4l2_streamparm setfps;
	
	memset(&setfps, 0, sizeof(setfps));
	
	setfps.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	setfps.parm.capture.timeperframe.numerator = 1;
	setfps.parm.capture.timeperframe.denominator = src->fps;
	if(ioctl(s->fd, VIDIOC_S_PARM, setfps) == -1)
	{
		/* Not fatal - just warn about it */
		/*WARN("Error setting frame rate:");
		WARN("VIDIOC_S_PARM: %s", strerror(errno));*/
		return(-1);
	}
	
	return(0);
}

int src_v4l2_free_mmap(src_t *src)
{
	src_v4l2_t *s = (src_v4l2_t *) src->state;
	int i;
	
	for(i = 0; i < s->req.count; i++)
		munmap(s->buffer[i].start, s->buffer[i].length);
	
	return(0);
}

int src_v4l2_set_mmap(src_t *src)
{
	src_v4l2_t *s = (src_v4l2_t *) src->state;
	enum v4l2_buf_type type;
	uint32_t b;
	
	/* Does the device support streaming? */
	if(~s->cap.capabilities & V4L2_CAP_STREAMING) return(-1);
	
	memset(&s->req, 0, sizeof(s->req));
	
	s->req.count  = 4;
	s->req.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	s->req.memory = V4L2_MEMORY_MMAP;
	
	if(ioctl(s->fd, VIDIOC_REQBUFS, &s->req) == -1)
	{
		/*ERROR("Error requesting buffers for memory map.");
		ERROR("VIDIOC_REQBUFS: %s", strerror(errno));*/
		return(-1);
	}
	
	/*DEBUG("mmap information:");
	DEBUG("frames=%d", s->req.count);*/
	
	if(s->req.count < 2)
	{
		/*ERROR("Insufficient buffer memory.");*/
		return(-1);
        }
	
	s->buffer = calloc(s->req.count, sizeof(v4l2_buffer_t));
	if(!s->buffer)
	{
		/*ERROR("Out of memory.");*/
		return(-1);
	}
	
	for(b = 0; b < s->req.count; b++)
	{
		struct v4l2_buffer buf;
		
		memset(&buf, 0, sizeof(buf));
		
		buf.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		buf.memory = V4L2_MEMORY_MMAP;
		buf.index  = b;
		
		if(ioctl(s->fd, VIDIOC_QUERYBUF, &buf) == -1)
		{
			/*ERROR("Error querying buffer %i", b);
			ERROR("VIDIOC_QUERYBUF: %s", strerror(errno));*/
			free(s->buffer);
			return(-1);
		}
		
		s->buffer[b].length = buf.length;
		s->buffer[b].start = mmap(NULL, buf.length,
		   PROT_READ | PROT_WRITE, MAP_SHARED, s->fd, buf.m.offset);
		
		if(s->buffer[b].start == MAP_FAILED)
		{
			/*ERROR("Error mapping buffer %i", b);
			ERROR("mmap: %s", strerror(errno));*/
			s->req.count = b;
			src_v4l2_free_mmap(src);
			free(s->buffer);
			return(-1);
		}
		
		/*DEBUG("%i length=%d", b, buf.length);*/
	}
	
	s->map = -1;
	
	for(b = 0; b < s->req.count; b++)
	{
		memset(&s->buf, 0, sizeof(s->buf));
		
		s->buf.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		s->buf.memory = V4L2_MEMORY_MMAP;
		s->buf.index  = b;
		
		if(ioctl(s->fd, VIDIOC_QBUF, &s->buf) == -1)
		{
			/*ERROR("VIDIOC_QBUF: %s", strerror(errno));*/
			src_v4l2_free_mmap(src);
			free(s->buffer);
			return(-1);
		}
	}
	
	type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	
	if(ioctl(s->fd, VIDIOC_STREAMON, &type) == -1)
	{
		/*ERROR("Error starting stream.");
		ERROR("VIDIOC_STREAMON: %s", strerror(errno));*/
		src_v4l2_free_mmap(src);
		free(s->buffer);
		return(-1);
	}
	
	return(0);
}

int src_v4l2_set_read(src_t *src)
{
	src_v4l2_t *s = (src_v4l2_t *) src->state;
	
	if(~s->cap.capabilities & V4L2_CAP_READWRITE) return(-1);
	
	s->buffer = calloc(1, sizeof(v4l2_buffer_t));
	if(!s->buffer)
	{
		/*ERROR("Out of memory.");*/
		return(-1);
	}
	
	s->buffer[0].length = s->fmt.fmt.pix.sizeimage;
	s->buffer[0].start  = malloc(s->buffer[0].length);
	
	if(!s->buffer[0].start)
	{
		/*ERROR("Out of memory.");*/
		
		free(s->buffer);
		s->buffer = NULL;
		
		return(-1);
	}
	
	return(0);
}

static const char *src_v4l2_query(src_t *src)
{
	if(!src->source)
	{
		/*ERROR("No device name specified.");*/
		fprintf(stderr, "No device name specified.");
		return NULL;
	}
	src_v4l2_t *s;	

	/* Allocate memory for the state structure. */
	s = calloc(sizeof(src_v4l2_t), 1);
	if(!s)
	{
		fprintf(stderr, "Out of memory.");
		return NULL;
	}
	
	src->state = (void *) s;

	/* Open the device. */
	s->fd = open(src->source, O_RDWR | O_NONBLOCK);
	if(s->fd < 0)
	{
		fprintf(stderr, "Cannot open device.");
		free(s);
		return NULL;
	}

	if(ioctl(s->fd, VIDIOC_QUERYCAP, &s->cap) < 0) {
	    src_v4l2_close(src);
	    fprintf(stderr, "Cannot get capabilities.");
	    return NULL;
	}
	char * res = (char*) s->cap.card;	
	src_v4l2_close(src);
	return res;
}

static int src_v4l2_open(src_t *src)
{
	src_v4l2_t *s;
	
	if(!src->source)
	{
		/*ERROR("No device name specified.");*/
		return(-2);
	}
	
	/* Allocate memory for the state structure. */
	s = calloc(sizeof(src_v4l2_t), 1);
	if(!s)
	{
		/*ERROR("Out of memory.");*/
		return(-2);
	}
	
	src->state = (void *) s;
	
	/* Open the device. */
	s->fd = open(src->source, O_RDWR | O_NONBLOCK);
	if(s->fd < 0)
	{
		/*ERROR("Error opening device: %s", src->source);
		ERROR("open: %s", strerror(errno));*/
		free(s);
		return(-2);
	}
	
	/*MSG("%s opened.", src->source);*/
	
	/* Get the device capabilities. */
	if(src_v4l2_get_capability(src))
	{
		src_v4l2_close(src);
		return(-2);
	}
	
	/* Set the input. */
	if(src_v4l2_set_input(src))
	{
		src_v4l2_close(src);
		return(-1);
	}
	
	/* Set picture options. */
	src_v4l2_set_controls(src);
	
	/* Set the pixel format. */
	if(src_v4l2_set_pix_format(src))
	{
		src_v4l2_close(src);
		return(-1);
	}
	
	/* Set the frame-rate if > 0 */
	if(src->fps) src_v4l2_set_fps(src);
	
	/* Delay to let the image settle down. */
	if(src->delay)
	{
		/*MSG("Delaying %i seconds.", src->delay);*/
		usleep(src->delay * 1000 * 1000);
	}
	
	/* Try to setup mmap. */
	if(!src->use_read && src_v4l2_set_mmap(src))
	{
		/*WARN("Unable to use mmap. Using read instead.");*/
		src->use_read = -1;
	}
	
	/* If unable to use mmap or user requested read(). */
	if(src->use_read)
	{
		if(src_v4l2_set_read(src))
		{
			/*ERROR("Unable to use read.");*/
			src_v4l2_close(src);
			return(-1);
		}
	}
	
	s->pframe = -1;
	
	return(0);
}

static int src_v4l2_close(src_t *src)
{
	src_v4l2_t *s = (src_v4l2_t *) src->state;
	
	if(s->buffer)
	{
		if(!s->map) free(s->buffer[0].start);
		else src_v4l2_free_mmap(src);
		free(s->buffer);
	}
	if(s->fd >= 0) close(s->fd);
	free(s);
	
	return(0);
}

static int src_v4l2_grab(src_t *src)
{
	src_v4l2_t *s = (src_v4l2_t *) src->state;
	
	if(src->timeout)
	{
		fd_set fds;
		struct timeval tv;
		int r;
		
		/* Is a frame ready? */
		FD_ZERO(&fds);
		FD_SET(s->fd, &fds);
		
		tv.tv_sec = src->timeout;
		tv.tv_usec = 0;
		
		r = select(s->fd + 1, &fds, NULL, NULL, &tv);
		
		if(r == -1)
		{
			/*ERROR("select: %s", strerror(errno));*/
			return(-1);
		}
		
		if(!r)
		{
			/*ERROR("Timed out waiting for frame!");*/
			return(-1);
		}
	}
	
	if(s->map)
	{
		if(s->pframe >= 0)
		{
			if(ioctl(s->fd, VIDIOC_QBUF, &s->buf) == -1)
			{
				/*ERROR("VIDIOC_QBUF: %s", strerror(errno));*/
				return(-1);
			}
		}
		
		memset(&s->buf, 0, sizeof(s->buf));
		
		s->buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		s->buf.memory = V4L2_MEMORY_MMAP;
		
		if(ioctl(s->fd, VIDIOC_DQBUF, &s->buf) == -1)
		{
			/*ERROR("VIDIOC_DQBUF: %s", strerror(errno));*/
			return(-1);
		}
		
		src->img    = s->buffer[s->buf.index].start;
		src->length = s->buffer[s->buf.index].length;
		
		s->pframe = s->buf.index;
	}
	else
	{
		ssize_t r;
		
		r = read(s->fd, s->buffer[0].start, s->buffer[0].length);
		if(r <= 0)
		{
			/*ERROR("Unable to read a frame.");
			ERROR("read: %s", strerror(errno));*/
			return(-1);
		}
		
		src->img = s->buffer[0].start;
		src->length = r;
	}
	
	return(0);
}

src_mod_t src_v4l2 = {
	"v4l2", SRC_TYPE_DEVICE,
	src_v4l2_open,
	src_v4l2_close,
	src_v4l2_grab,
	src_v4l2_query
};

#else /* #ifdef HAVE_V4L2 */

src_mod_t src_v4l2 = {
	"", SRC_TYPE_NONE,
        NULL,
        NULL,
        NULL,
	NULL
};

#endif /* #ifdef HAVE_V4L2 */

