/***************************************************************************
 *   Copyright (C) 2012 by Simon Andreas Eugster (simon.eu@gmail.com)      *
 *   This file is part of kdenlive. See www.kdenlive.org.                  *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 ***************************************************************************/


#include <QMap>
#include <QFile>
#include <QTime>
#include <QImage>
#include <QFileInfo>
#include <QDateTime>
#include <mlt++/Mlt.h>
#include <iostream>
#include <cstdlib>
#include <cmath>

#include "audioInfo.h"
#include "audioStreamInfo.h"

int main(int argc, char *argv[])
{
    char *fileMain;
    char *fileSub;
    if (argc > 2) {
        fileMain = argv[1];
        fileSub = argv[2];
    } else {
        std::cout << "Usage: " << argv[0] << " <main audio file> <second audio file>" << std::endl;
        return 0;
    }
    std::cout << "Trying to align (1)\n\t" << fileSub << "\nto fit on (2)\n\t" << fileMain
              << "\n, result will indicate by how much (1) has to be moved." << std::endl;

    // Initialize MLT
    Mlt::Factory::init(NULL);

    // Load an arbitrary profile
    Mlt::Profile prof("hdv_1080_25p");

    // Load the MLT producers
    Mlt::Producer prodMain(prof, fileMain);
    if (!prodMain.is_valid()) {
        std::cout << fileMain << " is invalid." << std::endl;
        return 2;
    }
    Mlt::Producer profSub(prof, fileSub);
    if (!profSub.is_valid()) {
        std::cout << fileSub << " is invalid." << std::endl;
        return 2;
    }

    AudioInfo infoMain(&prodMain);
    AudioInfo infoSub(&profSub);
    infoMain.dumpInfo();
    infoSub.dumpInfo();

    prodMain.get_fps();


    int framesToFetch = prodMain.get_length();
    std::cout << "Length: " << framesToFetch
              << " (Seconds: " << framesToFetch/prodMain.get_fps() << ")"
              << std::endl;
    if (framesToFetch > 5000) {
        framesToFetch = 5000;
    }

    mlt_audio_format format_s16 = mlt_audio_s16;
    int samplingRate = infoMain.info(0)->samplingRate();
    int channels = 1;

    Mlt::Frame *frame;
    int64_t position;
    int samples;

    uint64_t envelope[framesToFetch];
    uint64_t max = 0;

    QTime t;
    t.start();
    for (int i = 0; i < framesToFetch; i++) {

        frame = prodMain.get_frame(i);
        position = mlt_frame_get_position(frame->get_frame());
        samples = mlt_sample_calculator(prodMain.get_fps(), infoMain.info(0)->samplingRate(), position);

        int16_t *data = static_cast<int16_t*>(frame->get_audio(format_s16, samplingRate, channels, samples));

        uint64_t sum = 0;
        for (int k = 0; k < samples; k++) {
            sum += fabs(data[k]);
        }
        envelope[i] = sum;

        if (sum > max) {
            max = sum;
        }
    }
    std::cout << "Calculating the envelope (" << framesToFetch << " frames) took "
              << t.elapsed() << " ms." << std::endl;

    QImage img(framesToFetch, 400, QImage::Format_ARGB32);
    img.fill(qRgb(255,255,255));
    double fy;
    for (int x = 0; x < img.width(); x++) {
        fy = envelope[x]/double(max) * img.height();
        for (int y = img.height()-1; y > img.height()-1-fy; y--) {
            img.setPixel(x,y, qRgb(50, 50, 50));
        }
    }

    QString outImg = QString("envelope-%1.png")
            .arg(QDateTime::currentDateTime().toString("yyyy-MM-dd-hh:mm:ss"));
    img.save(outImg);
    std::cout << "Saved volume envelope as "
              << QFileInfo(outImg).absoluteFilePath().toStdString()
              << std::endl;

    return 0;

}
