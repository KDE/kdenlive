/***************************************************************************
   Copyright (C) 2006-2008
   by Marco Gulino <marco@kmobiletools.org>
   adapted for MLT video preview by Jean-Baptiste Mardelle <jb@kdenlive.org>

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the
   Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
 ***************************************************************************/


#ifndef WESTLEYPREVIEW_H
#define WESTLEYPREVIEW_H

#include <kio/thumbcreator.h>

#include <mlt++/Mlt.h>

#include <qstring.h>
#include <qstringlist.h>
#include <qobject.h>

class MltPreview : public QObject, public ThumbCreator
{
    Q_OBJECT
public:
    MltPreview();
    virtual ~MltPreview();
    virtual bool create(const QString &path, int width, int height, QImage &img);
    virtual Flags flags() const;

protected:
    static uint imageVariance(QImage image);
    QImage getFrame(Mlt::Producer* producer, int framepos, int width, int height);
};

#endif
