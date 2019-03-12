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

#ifndef MLTPREVIEW_H
#define MLTPREVIEW_H

#include <kio/thumbcreator.h>

#include <mlt++/Mlt.h>

#include <QObject>

class MltPreview : public ThumbCreator
{
public:
    MltPreview();
    virtual ~MltPreview();
    bool create(const QString &path, int width, int height, QImage &img) override;
    Flags flags() const override;

protected:
    static uint imageVariance(const QImage &image);
    QImage getFrame(Mlt::Producer *producer, int framepos, int width, int height);
};

#endif
