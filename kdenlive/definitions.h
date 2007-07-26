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

class formatTemplate
    {
    public:
	formatTemplate() : m_width( 720 ), m_height( 576 ), m_fps_num(25), m_fps_den(1), m_aspect_num(59), m_aspect_den(54), m_display_num(4), m_display_den(3), m_profile("dv_pal") {
	    m_aspect = (double) m_aspect_num / m_aspect_den;
	    m_display = (double) m_display_num / m_display_den;
	    m_fps = (double) m_fps_num / m_fps_den;
	}

        formatTemplate( int width, int height, int fps_num, int fps_den, int aspect_num, int aspect_den, int display_num, int display_den, int progressive, QString profile)
            : m_width( width ), m_height( height ), m_fps_num( fps_num ), m_fps_den( fps_den ), m_aspect_num( aspect_num ), m_aspect_den( aspect_den ), m_display_num( display_num ), m_display_den( display_den ), m_progressive( progressive ), m_profile( profile )
        { 
	    m_aspect = (double) m_aspect_num / m_aspect_den;
	    m_display = (double) m_display_num / m_display_den;
	    m_fps = (double) m_fps_num / m_fps_den;
	}

	virtual ~formatTemplate(){};

        int width()   const          { return m_width;}
        int height()   const          { return m_height;}
        double fps()   const          { return m_fps;}
        double aspect()   const          { return m_aspect;}
        double display()   const          { return m_display;}
	QString profile() const { return m_profile;}

    private:
        int m_width;
        int m_height;
        double m_fps;
        int m_fps_num;
        int m_fps_den;
        double m_aspect;
        int m_aspect_num;
        int m_aspect_den;
        double m_display;
        int m_display_num;
        int m_display_den;
        int m_progressive;
	QString m_profile;
    };

#endif

