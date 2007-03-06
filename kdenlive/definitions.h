/***************************************************************************
                          definitions  -  description
                             -------------------
    begin                : Fri Oct 13 2006
    copyright            : (C) 2006 by Jean-Baptiste Mardelle
    email                : jb@ader.ch
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
1 ***************************************************************************/

#ifndef KDENLIVEDEFS_H
#define KDENLIVEDEFS_H

    enum VIDEOFORMAT { PAL_VIDEO = 0, NTSC_VIDEO = 1, PAL_WIDE = 2, NTSC_WIDE = 3, HDV1080PAL_VIDEO = 4, HDV1080NTSC_VIDEO = 5, HDV720PAL_VIDEO = 6, HDV720NTSC_VIDEO = 7};

class formatTemplate
    {
    public:
	formatTemplate() : m_width( 720 ), m_height( 576 ), m_fps( 25.0 ), m_aspect( 59.0 / 54.0 ), m_normalisation("MLT_NORMALISATION=PAL"), m_videoformat(PAL_VIDEO) {}
        formatTemplate( int width, int height, double fps, double aspect_ratio, char *normalisation, VIDEOFORMAT format)
            : m_width( width ), m_height( height ), m_fps( fps ), m_aspect( aspect_ratio ), m_normalisation( normalisation ), m_videoformat( format )
        { }

	virtual ~formatTemplate(){};

        int width()   const          { return m_width;}
        int height()   const          { return m_height;}
        double fps()   const          { return m_fps;}
        double aspect()   const          { return m_aspect;}
        char *normalisation()   const          { return m_normalisation;}
	VIDEOFORMAT videoFormat() const { return m_videoformat;}
    private:
        int m_width;
        int m_height;
        double m_fps;
        double m_aspect;
	char * m_normalisation;
	VIDEOFORMAT m_videoformat;
    };

#endif

