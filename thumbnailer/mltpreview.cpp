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

#include "mltpreview.h"
#include "../src/lib/localeHandling.h"

#include <QImage>
#include <QVarLengthArray>
#include <QtGlobal>

#include <QDebug>
#include <krandomsequence.h>

extern "C" {
Q_DECL_EXPORT ThumbCreator *new_creator()
{
    return new MltPreview;
}
}

MltPreview::MltPreview()
{
    // After initialising the MLT factory, set the locale back from user default to C
    // to ensure numbers are always serialised with . as decimal point.
    Mlt::Factory::init();
    LocaleHandling::resetLocale();
}

MltPreview::~MltPreview()
{
    Mlt::Factory::close();
}

bool MltPreview::create(const QString &path, int width, int height, QImage &img)
{
    auto *profile = new Mlt::Profile();
    Mlt::Producer *producer = new Mlt::Producer(*profile, path.toUtf8().data());

    if (producer->is_blank()) {
        delete producer;
        delete profile;
        return false;
    }
    int frame = 75;
    uint variance = 10;
    int ct = 1;
    double ar = profile->dar();
    if (ar == 0) {
        ar = 1.0;
    }
    int wanted_width = width;
    int wanted_height = width / profile->dar();
    if (wanted_height > height) {
        wanted_height = height;
        wanted_width = height * ar;
    }

    // img = getFrame(producer, frame, width, height);
    while (variance <= 40 && ct < 4) {
        img = getFrame(producer, frame, wanted_width, wanted_height);
        variance = imageVariance(img);
        frame += 100 * ct;
        ct++;
    }

    delete producer;
    delete profile;
    return (!img.isNull());
}

QImage MltPreview::getFrame(Mlt::Producer *producer, int framepos, int width, int height)
{
    QImage mltImage(width, height, QImage::Format_ARGB32_Premultiplied);
    if (producer == nullptr) {
        return mltImage;
    }

    producer->seek(framepos);
    Mlt::Frame *frame = producer->get_frame();
    if (frame == nullptr) {
        return mltImage;
    }

    mlt_image_format format = mlt_image_rgb24a;

    const uchar *imagedata = frame->get_image(format, width, height);
    if (imagedata != nullptr) {
        memcpy(mltImage.bits(), imagedata, width * height * 4);
        mltImage = mltImage.rgbSwapped();
    }

    delete frame;
    return mltImage;
}

uint MltPreview::imageVariance(const QImage &image)
{
    if (image.isNull()) {
        return 0;
    }
    uint delta = 0;
    uint avg = 0;
    uint bytes = image.sizeInBytes();
    uint STEPS = bytes / 2;
    if (STEPS < 1) {
        return 0;
    }
    QVarLengthArray<uchar> pivot(STEPS);
    qDebug() << "Using " << STEPS << " steps\n";
    const uchar *bits = image.bits();
    // First pass: get pivots and taking average
    for (uint i = 0; i < STEPS; i++) {
        pivot[i] = bits[2 * i];
        avg += pivot.at(i);
    }
    avg = avg / STEPS;
    // Second Step: calculate delta (average?)
    for (uint i = 0; i < STEPS; ++i) {
        int curdelta = abs(int(avg - pivot.at(i)));
        delta += curdelta;
    }
    return delta / STEPS;
}

ThumbCreator::Flags MltPreview::flags() const
{
    return None;
}
