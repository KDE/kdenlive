/*
    SPDX-FileCopyrightText: 2006-2008 Marco Gulino <marco@kmobiletools.org>
    SPDX-FileCopyrightText: Jean-Baptiste Mardelle <jb@kdenlive.org>

    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "mltpreview.h"
#include "../src/lib/localeHandling.h"

#include <QDebug>
#include <QImage>
#include <QVarLengthArray>
#include <QtGlobal>

#include <KPluginFactory>

K_PLUGIN_CLASS_WITH_JSON(MltPreview, "mltpreview.json")

MltPreview::MltPreview(QObject *parent, const QVariantList &args)
    : KIO::ThumbnailCreator(parent, args)
{
    // block MLT Qt5 modules to prevent crashes
    qputenv("MLT_REPOSITORY_DENY", "libmltqt:libmltglaxnimate");

    // After initialising the MLT factory, set the locale back from user default to C
    // to ensure numbers are always serialised with . as decimal point.
    Mlt::Factory::init();
    LocaleHandling::resetLocale();
}

MltPreview::~MltPreview()
{
    Mlt::Factory::close();
}

KIO::ThumbnailResult MltPreview::create(const KIO::ThumbnailRequest &request)
{
    int width = request.targetSize().width();
    int height = request.targetSize().height();
    std::unique_ptr<Mlt::Profile> profile(new Mlt::Profile());
    std::shared_ptr<Mlt::Producer> producer(new Mlt::Producer(*profile.get(), request.url().toLocalFile().toUtf8().data()));

    if (producer == nullptr || !producer->is_valid() || producer->is_blank()) {
        return KIO::ThumbnailResult::fail();
    }

    uint variance = 10;
    int ct = 1;
    double ar = profile->dar();
    if (ar < 1e-6) {
        ar = 1.0;
    }
    int wanted_width = width;
    int wanted_height = int(width / ar);
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

    QImage img;
    int length = producer->get_length();
    if (length < 1) {
        return KIO::ThumbnailResult::fail();
    }
    int frame = qMin(75, length - 1);
    while (variance <= 40 && ct < 4 && frame < length) {
        img = getFrame(producer, frame, wanted_width, wanted_height);
        variance = uint(imageVariance(img));
        frame += 100 * ct;
        ct++;
    }

    if (img.isNull()) {
        return KIO::ThumbnailResult::fail();
    }

    return KIO::ThumbnailResult::pass(img);
}

QImage MltPreview::getFrame(std::shared_ptr<Mlt::Producer> producer, int framepos, int width, int height)
{
    QImage mltImage(width, height, QImage::Format_ARGB32);
    if (producer == nullptr) {
        return mltImage;
    }
    producer->seek(framepos);
    std::shared_ptr<Mlt::Frame> frame(producer->get_frame());
    if (frame == nullptr || !frame->is_valid()) {
        return mltImage;
    }

    mlt_image_format format = mlt_image_rgba;
    const uchar *imagedata = frame->get_image(format, width, height);
    if (imagedata != nullptr) {
        memcpy(mltImage.bits(), imagedata, size_t(width * height * 4));
        mltImage = mltImage.rgbSwapped();
    }
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

#include "mltpreview.moc"
