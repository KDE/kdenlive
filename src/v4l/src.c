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
#include <sys/mman.h>
#include "src.h"

#include <sys/ioctl.h>


int v4l2_free_mmap(src_t *src)
{
        src_v4l2_t *s = (src_v4l2_t *) src->state;
        int i;

        for(i = 0; i < s->req.count; i++)
                munmap(s->buffer[i].start, s->buffer[i].length);

        return(0);
}

static int close_v4l2(src_t *src)
{
        src_v4l2_t *s = (src_v4l2_t *) src->state;

        if(s->buffer)
        {
                if(!s->map) free(s->buffer[0].start);
                else v4l2_free_mmap(src);
                free(s->buffer);
        }
        if(s->fd >= 0) close(s->fd);
        free(s);

        return(0);
}

//static
const char *query_v4ldevice(src_t *src, char **pixelformatdescription)
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
        char value[200];
        //snprintf( value, sizeof(value), '\0' );
        //strcpy(*pixelformatdescription, (char*) value);
        /* Open the device. */
        s->fd = open(src->source, O_RDWR | O_NONBLOCK);
        if(s->fd < 0)
        {
                fprintf(stderr, "Cannot open device.");
                free(s);
                return NULL;
        }
        char *res = NULL;
        int captureEnabled = 1;
        if(ioctl(s->fd, VIDIOC_QUERYCAP, &s->cap) < 0) {
            fprintf(stderr, "Cannot get capabilities.");
            //return NULL;
        }
        else {
            res = strdup((char*) s->cap.card);
            if(!s->cap.capabilities & V4L2_CAP_VIDEO_CAPTURE) {
                // Device cannot capture
                captureEnabled = 0;
            }
        }
        
        if (captureEnabled) {
            struct v4l2_format format;
            memset(&format,0,sizeof(format));
            format.type  = V4L2_BUF_TYPE_VIDEO_CAPTURE;

            struct v4l2_fmtdesc fmt;
            memset(&fmt,0,sizeof(fmt));
            fmt.index = 0;
            fmt.type  = V4L2_BUF_TYPE_VIDEO_CAPTURE;

            struct v4l2_frmsizeenum sizes;
            memset(&sizes,0,sizeof(sizes));

            struct v4l2_frmivalenum rates;
            memset(&rates,0,sizeof(rates));

            while (ioctl(s->fd, VIDIOC_ENUM_FMT, &fmt) != -1)
            {
                snprintf( value, sizeof(value), ">%c%c%c%c", fmt.pixelformat >> 0,  fmt.pixelformat >> 8, fmt.pixelformat >> 16, fmt.pixelformat >> 24 );
                strcat(*pixelformatdescription, (char *) value);
                fprintf(stderr, "detected format: %s: %c%c%c%c\n", fmt.description, fmt.pixelformat >> 0,  fmt.pixelformat >> 8,
                      fmt.pixelformat >> 16, fmt.pixelformat >> 24);

                sizes.pixel_format = fmt.pixelformat;
                sizes.index = 0;
                // Query supported frame size
                while (ioctl(s->fd, VIDIOC_ENUM_FRAMESIZES, &sizes) != -1) {
                    struct v4l2_frmsize_discrete image_size = sizes.un.discrete;
                    // Query supported frame rates
                    rates.index = 0;
                    rates.pixel_format = fmt.pixelformat;
                    rates.width = image_size.width;
                    rates.height = image_size.height;
                    snprintf( value, sizeof(value), ":%dx%d=", image_size.width, image_size.height );
                    strcat(*pixelformatdescription, (char *) value);
                    fprintf(stderr, "Size: %dx%d: ", image_size.width, image_size.height);
                    while (ioctl(s->fd, VIDIOC_ENUM_FRAMEINTERVALS, &rates) != -1) {
                        snprintf( value, sizeof(value), "%d/%d,", rates.un.discrete.denominator, rates.un.discrete.numerator );
                        strcat(*pixelformatdescription, (char *) value);
                        fprintf(stderr, "%d/%d, ", rates.un.discrete.numerator, rates.un.discrete.denominator);
                        rates.index ++;
                    }
                    fprintf(stderr, "\n");
                    sizes.index++;
                }


                /*[0x%08X] '%c%c%c%c' (%s)", v4l2_pal,
                      fmt.pixelformat,
                      fmt.pixelformat >> 0,  fmt.pixelformat >> 8,
                      fmt.pixelformat >> 16, fmt.pixelformat >> 24*/
                fmt.index++;
            }
            /*else {
                *pixelformatdescription = '\0';
            }*/
        }
        close_v4l2(src);
        return res;
}



