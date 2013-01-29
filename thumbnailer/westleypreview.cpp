/***************************************************************************
   Copyright (C) 2006-2008
   by Marco Gulino <marco@kmobiletools.org>

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

#include "westleypreview.h"

#include <qfile.h>
#include <qimage.h>
#include <QtCore/QVarLengthArray>

#include <kstandarddirs.h>
#include <krandomsequence.h>
#include <qdatetime.h>
#include <QColor>
#include <kdebug.h>
#include <ktempdir.h>
#include <kurl.h>
#include <qfileinfo.h>
#include <KTemporaryFile>

#include <unistd.h>


#define DBG_AREA

//#include "config.h"
extern "C" {
    KDE_EXPORT ThumbCreator *new_creator() {
        return new MltPreview;
    }
}

MltPreview::MltPreview() :
        QObject(),
        ThumbCreator()
{
    Mlt::Factory::init();
}

MltPreview::~MltPreview()
{
    Mlt::Factory::close();
}


bool MltPreview::create(const QString &path, int width, int height, QImage &img)
{
    Mlt::Profile *profile = new Mlt::Profile("dv_pal");
    Mlt::Producer *producer = new Mlt::Producer(*profile, path.toUtf8().data());


    if (producer->is_blank()) {
        delete producer;
        return false;
    }
    int frame = 75;
    uint variance = 10;
    int ct = 1;

    //img = getFrame(producer, frame, width, height);

    while (variance <= 40 && ct < 4) {
        img = getFrame(producer, frame, width, height);
        variance = imageVariance(img);
        frame += 100 * ct;
        ct++;
    }

    delete producer;
    delete profile;
    return (img.isNull() == false);
}

QImage MltPreview::getFrame(Mlt::Producer *producer, int framepos, int /*width*/, int height)
{
    QImage result;
    if (producer == NULL) {
        return result;
    }

    producer->seek(framepos);
    Mlt::Frame *frame = producer->get_frame();
    if (frame == NULL) {
        return result;
    }

    mlt_image_format format = mlt_image_rgb24a;
    height = 200;
    double ar = frame->get_double("aspect_ratio");
    if (ar == 0.0) ar = 1.33;
    int calculated_width = (int)((double) height * ar);
    uint8_t *data = frame->get_image(format, calculated_width, height, 0);
    QImage image((uchar *)data, calculated_width, height, QImage::Format_ARGB32);

    if (!image.isNull()) {
        result = image.rgbSwapped().convertToFormat(QImage::Format_RGB32);
    }

    delete frame;
    return result;
}


uint MltPreview::imageVariance(QImage image)
{
    if (image.isNull()) return 0;
    uint delta = 0;
    uint avg = 0;
    uint bytes = image.numBytes();
    if (bytes == 0) return 0;
    uint STEPS = bytes / 2;
    QVarLengthArray<uchar> pivot(STEPS);
    kDebug(DBG_AREA) << "Using " << STEPS << " steps\n";
    const uchar *bits=image.bits();
    // First pass: get pivots and taking average
    for( uint i=0; i<STEPS ; i++ ){
        pivot[i] = bits[2 * i];
#if QT_VERSION >= 0x040700
        avg+=pivot.at(i);
#else
        avg+=pivot[i];
#endif
    }
    avg=avg/STEPS;
    // Second Step: calculate delta (average?)
    for (uint i=0; i<STEPS; i++)
    {
#if QT_VERSION >= 0x040700
        int curdelta=abs(int(avg - pivot.at(i)));
#else
        int curdelta=abs(int(avg - pivot[i]));
#endif
        delta+=curdelta;
    }
    return delta / STEPS;
}

ThumbCreator::Flags MltPreview::flags() const
{
    return None;
}


