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
    std::unique_ptr<Mlt::Profile> profile(new Mlt::Profile());
    std::shared_ptr<Mlt::Producer>producer(new Mlt::Producer(*profile.get(), path.toUtf8().data()));

    if (producer->is_blank()) {
        return false;
    }
    int frame = 75;
    uint variance = 10;
    int ct = 1;
    double ar = profile->dar();
    if (ar < 1e-6) {
        ar = 1.0;
    }
    int wanted_width = width;
    int wanted_height = int(width / profile->dar());
    if (wanted_height > height) {
        wanted_height = height;
        wanted_width = int(height * ar);
    }
    // We don't need audio
    producer->set("audio_index", -1);

    // Add normalizers    
    Mlt::Filter scaler(*profile.get(), "swscale");
    Mlt::Filter padder(*profile.get(), "resize");
    Mlt::Filter converter(*profile.get(), "avcolor_space");

    if (scaler.is_valid()) {
        producer->attach(scaler);
    }
    if (padder.is_valid()) {
        producer->attach(padder);
    }
    if (converter.is_valid()) {
        producer->attach(converter);
    }
    

    // img = getFrame(producer, frame, width, height);
    while (variance <= 40 && ct < 4) {
        img = getFrame(producer, frame, wanted_width, wanted_height);
        variance = uint(imageVariance(img));
        frame += 100 * ct;
        ct++;
    }
    return (!img.isNull());
}

QImage MltPreview::getFrame(std::shared_ptr<Mlt::Producer> producer, int framepos, int width, int height)
{
    QImage mltImage(width, height, QImage::Format_ARGB32);
    if (producer == nullptr) {
        return mltImage;
    }

    producer->seek(framepos);
    Mlt::Frame *frame = producer->get_frame();
    if (frame == nullptr || !frame->is_valid()) {
        return mltImage;
    }

    mlt_image_format format = mlt_image_rgba;
    const uchar *imagedata = frame->get_image(format, width, height);
    if (imagedata != nullptr) {
        memcpy(mltImage.bits(), imagedata, size_t(width * height * 4));
        mltImage = mltImage.rgbSwapped();
    }

    delete frame;
    return mltImage;
}

int MltPreview::imageVariance(const QImage &image)
{
    if (image.isNull()) {
        return 0;
    }
    int delta = 0;
    int avg = 0;
    int bytes = int(image.sizeInBytes());
    int STEPS = bytes / 2;
    if (STEPS < 1) {
        return 0;
    }
    QVarLengthArray<uchar> pivot(STEPS);
    qDebug() << "Using " << STEPS << " steps\n";
    const uchar *bits = image.bits();
    // First pass: get pivots and taking average
    for (int i = 0; i < STEPS; i++) {
        pivot[i] = bits[2 * i];
        avg += pivot.at(i);
    }
    avg = avg / STEPS;
    // Second Step: calculate delta (average?)
    for (int i = 0; i < STEPS; ++i) {
        int curdelta = abs(avg - pivot.at(i));
        delta += curdelta;
    }
    return delta / STEPS;
}

ThumbCreator::Flags MltPreview::flags() const
{
    return None;
}
