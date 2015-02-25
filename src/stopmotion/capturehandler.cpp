/***************************************************************************
 *   Copyright (C) 2010 by Jean-Baptiste Mardelle (jb@kdenlive.org)        *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA          *
 ***************************************************************************/

#include "capturehandler.h"
#include "kdenlivesettings.h"


CaptureHandler::CaptureHandler(QVBoxLayout *lay, QWidget *parent):
    m_layout(lay),
    m_parent(parent),
    m_analyseFrame(KdenliveSettings::analyse_stopmotion())
{
}

CaptureHandler::~CaptureHandler()
{
    stopCapture();
}

void CaptureHandler::stopCapture()
{
}

//static
void CaptureHandler::uyvy2rgb(unsigned char *yuv_buffer, unsigned char *rgb_buffer, int width, int height)
{
    int len;
    int rgb_ptr, y_ptr;

    len = width * height / 2;

    rgb_ptr = 0;
    y_ptr = 0;

    for (int t = 0; t < len; ++t) { /* process 2 pixels at a time */
        /* Compute parts of the UV components */

        int U = yuv_buffer[y_ptr];
        int Y = yuv_buffer[y_ptr+1];
        int V = yuv_buffer[y_ptr+2];
        int Y2 = yuv_buffer[y_ptr+3];
        y_ptr += 4;

        int r = ((298 * (Y - 16)               + 409 * (V - 128) + 128) >> 8);
        int g = ((298 * (Y - 16) - 100 * (U - 128) - 208 * (V - 128) + 128) >> 8);
        int b = ((298 * (Y - 16) + 516 * (U - 128)               + 128) >> 8);

        if (r > 255) r = 255;
        if (g > 255) g = 255;
        if (b > 255) b = 255;

        if (r < 0) r = 0;
        if (g < 0) g = 0;
        if (b < 0) b = 0;

        rgb_buffer[rgb_ptr] = b;
        rgb_buffer[rgb_ptr+1] = g;
        rgb_buffer[rgb_ptr+2] = r;
        rgb_buffer[rgb_ptr+3] = 255;

        rgb_ptr += 4;
        /*r = 1.164*(Y2-16) + 1.596*(V-128);
        g = 1.164*(Y2-16) - 0.813*(V-128) - 0.391*(U-128);
        b = 1.164*(Y2-16) + 2.018*(U-128);*/


        r = ((298 * (Y2 - 16)               + 409 * (V - 128) + 128) >> 8);

        g = ((298 * (Y2 - 16) - 100 * (U - 128) - 208 * (V - 128) + 128) >> 8);

        b = ((298 * (Y2 - 16) + 516 * (U - 128)               + 128) >> 8);

        if (r > 255) r = 255;
        if (g > 255) g = 255;
        if (b > 255) b = 255;

        if (r < 0) r = 0;
        if (g < 0) g = 0;
        if (b < 0) b = 0;

        rgb_buffer[rgb_ptr] = b;
        rgb_buffer[rgb_ptr+1] = g;
        rgb_buffer[rgb_ptr+2] = r;
        rgb_buffer[rgb_ptr+3] = 255;
        rgb_ptr += 4;
    }
}


