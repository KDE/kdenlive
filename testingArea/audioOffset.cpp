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
#include "audioEnvelope.h"

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
    Mlt::Producer prodSub(prof, fileSub);
    if (!prodSub.is_valid()) {
        std::cout << fileSub << " is invalid." << std::endl;
        return 2;
    }

    AudioEnvelope envelopeMain(&prodMain);
    envelopeMain.loadEnvelope();
    envelopeMain.loadStdDev();
    envelopeMain.dumpInfo();
    envelopeMain.normalizeEnvelope();
    envelopeMain.dumpInfo();

    AudioEnvelope envelopeSub(&prodSub);
    envelopeSub.loadEnvelope();
    envelopeMain.normalizeEnvelope();
    envelopeSub.dumpInfo();


    QString outImg = QString("envelope-%1.png")
            .arg(QDateTime::currentDateTime().toString("yyyy-MM-dd-hh:mm:ss"));
    envelopeMain.drawEnvelope().save(outImg);
    std::cout << "Saved volume envelope as "
              << QFileInfo(outImg).absoluteFilePath().toStdString()
              << std::endl;



    const int sizeX = envelopeMain.envelopeSize();
    const int sizeY = envelopeSub.envelopeSize();
    int64_t correlation[sizeX + sizeY + 1];
    const int64_t *envX = envelopeMain.envelope();
    const int64_t *envY = envelopeSub.envelope();
    int64_t const* left;
    int64_t const* right;
    int size;
    int64_t sum;
    int64_t max = 0;

    QTime t;
    t.start();
    for (int shift = -sizeX; shift <= sizeY; shift++) {

        if (shift <= 0) {
            left = envX-shift;
            right = envY;
            size = std::min(sizeX+shift, sizeY);
        } else {
            left = envX;
            right = envY+shift;
            size = std::min(sizeX, sizeY-shift);
        }

        sum = 0;
        for (int i = 0; i < size; i++) {
            sum += (*left) * (*right);
            left++;
            right++;
        }
        correlation[sizeX+shift] = std::abs(sum);
        std::cout << sum << " ";

        if (sum > max) {
            max = sum;
        }

    }
    std::cout << "Correlation calculated. Time taken: " << t.elapsed() << " ms." << std::endl;

    int val;
    QImage img(sizeX + sizeY + 1, 400, QImage::Format_ARGB32);
    img.fill(qRgb(255,255,255));
    for (int x = 0; x < sizeX+sizeY+1; x++) {
        val = correlation[x]/double(max)*img.height();
        for (int y = img.height()-1; y > img.height() - val - 1; y--) {
            img.setPixel(x, y, qRgb(50, 50, 50));
        }
    }

    outImg = QString("correlation-%1.png")
            .arg(QDateTime::currentDateTime().toString("yyyy-MM-dd-hh:mm:ss"));
    img.save(outImg);
    std::cout << "Saved volume envelope as "
              << QFileInfo(outImg).absoluteFilePath().toStdString()
              << std::endl;


    return 0;

}



