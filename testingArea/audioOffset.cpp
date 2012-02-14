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

    AudioEnvelope envelopeMain(&prodMain);
    envelopeMain.loadEnvelope();
    envelopeMain.loadStdDev();
    envelopeMain.dumpInfo();


    QString outImg = QString("envelope-%1.png")
            .arg(QDateTime::currentDateTime().toString("yyyy-MM-dd-hh:mm:ss"));
    envelopeMain.drawEnvelope().save(outImg);
    std::cout << "Saved volume envelope as "
              << QFileInfo(outImg).absoluteFilePath().toStdString()
              << std::endl;


    return 0;

}



