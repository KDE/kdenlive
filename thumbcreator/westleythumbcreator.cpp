/***************************************************************************
                          kfile_westley  -  description
                             -------------------
    begin                : Sat Aug 4 2007
    copyright            : (C) 2007 by Jean-Baptiste Mardelle
    email                : jb@kdenlive.org
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/


#include <config.h>

#include <qfile.h>
#include <qpixmap.h>
#include <qimage.h>
#include <qcolor.h>
#include <qcstring.h>

#include <kdemacros.h>
#include <kdebug.h>
#include <kgenericfactory.h>

#include <mlt++/Mlt.h>

#include "westleythumbcreator.h"



extern "C" {
	ThumbCreator * KDE_EXPORT new_creator()
   	{
     	    return new WestleyThumbCreator();
   	}
}

bool WestleyThumbCreator::create(const QString &path, int width, int height, QImage &img) {

    bool result = false;
    char *tmp = qstrdup(QFile::encodeName(path));
    //char *tmp = new char[fn.length() + 1];
    //strcpy(tmp, (const char *)fn);

	
    if (Mlt::Factory::init(NULL) != 0) return false;
    Mlt::Producer prod(tmp);
    delete[] tmp;

    if (prod.is_blank()) {
	return false;
    }

    Mlt::Filter m_convert("avcolour_space");
    m_convert.set("forced", mlt_image_rgb24a);
    prod.attach(m_convert);

    Mlt::Frame * frame = prod.get_frame();
    mlt_image_format format = mlt_image_rgb24a;
    if (frame && frame->is_valid()) {

	int frame_width = mlt_properties_get_int( frame->get_properties(), "width");
	int frame_height = mlt_properties_get_int( frame->get_properties(), "height");
	img.create(frame_width, frame_height, 32);
	
    	uint8_t *thumb = frame->get_image(format, frame_width, frame_height);
	//img.loadFromData(thumb, sizeof( thumb ));
    	QImage tmpimage(thumb, frame_width, frame_height, 32, NULL, 0, QImage::IgnoreEndian);
    	if (!tmpimage.isNull()) bitBlt(&img, 0, 0, &tmpimage, 0, 0, frame_width, frame_height);
	result = true;
    }
    if (frame) delete frame;
    return result;

}

ThumbCreator::Flags WestleyThumbCreator::flags() const {
    return static_cast<Flags>(None);
}

