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

#ifndef __THUMB_WESTLEY_H__
#define __THUMB_WESTLEY_H__


#include <kio/thumbcreator.h>

namespace Mlt {
    class Frame;
    class Producer;
    class Filter;
};

class WestleyThumbCreator : public ThumbCreator {
public:
    WestleyThumbCreator() {};
    virtual bool create(const QString &path, int, int, QImage &img);
    virtual Flags flags() const;
};


#endif // __THUMB_WESTLEY_H__
