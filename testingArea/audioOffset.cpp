/*
Copyright (C) 2012  Simon A. Eugster (Granjow)  <simon.eu@gmail.com>
This file is part of kdenlive. See www.kdenlive.org.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
*/

#include <QCoreApplication>
#include <QDateTime>
#include <QDebug>
#include <QFile>
#include <QFileInfo>
#include <QStringList>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <mlt++/Mlt.h>

#include "../src/lib/audio/audioCorrelation.h"
#include "../src/lib/audio/audioEnvelope.h"
#include "../src/lib/audio/audioInfo.h"
#include "../src/lib/audio/audioStreamInfo.h"

void printUsage(const char *path)
{
    std::cout << "This executable takes two audio/video files A and B and determines " << std::endl
              << "how much B needs to be shifted in order to be synchronized with A." << std::endl
              << std::endl
              << path << " <main audio file> <second audio file>" << std::endl
              << "\t-h, --help\n\t\tDisplay this help" << std::endl
              << "\t--fft\n\t\tUse Fourier Transform (FFT) to calculate the offset. This only takes" << std::endl
              << "\t\tO(n log n) time compared to O(n²) when using normal correlation and should be " << std::endl
              << "\t\tfaster for large data (several minutes)." << std::endl
              << "\t--profile=<profile>\n\t\tUse the given profile for calculation (run: melt -query profiles)" << std::endl
              << "\t--no-images\n\t\tDo not save envelope and correlation images" << std::endl;
}

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    QStringList args = app.arguments();
    args.removeAt(0);

    std::string profile = "atsc_1080p_24";
    bool saveImages = true;
    bool useFFT = false;

    // Load arguments
    foreach (const QString &str, args) {

        if (str.startsWith(QLatin1String("--profile="))) {
            QString s = str;
            s.remove(0, QString("--profile=").length());
            profile = s.toStdString();
            args.removeOne(str);

        } else if (str == "-h" || str == "--help") {
            printUsage(argv[0]);
            return 0;

        } else if (str == "--no-images") {
            saveImages = false;
            args.removeOne(str);

        } else if (str == "--fft") {
            useFFT = true;
            args.removeOne(str);
        }
    }

    if (args.length() < 2) {
        printUsage(argv[0]);
        return 1;
    }

    std::string fileMain(args.at(0).toStdString());
    args.removeFirst();
    std::string fileSub = args.at(0).toStdString();
    args.removeFirst();

    qDebug() << "Unused arguments: " << args;

    if (argc > 2) {
        fileMain = argv[1];
        fileSub = argv[2];
    } else {
        std::cout << "Usage: " << argv[0] << " <main audio file> <second audio file>" << std::endl;
        return 0;
    }
    std::cout << "Trying to align (2)\n\t" << fileSub << "\nto fit on (1)\n\t" << fileMain << "\n, result will indicate by how much (2) has to be moved."
              << std::endl
              << "Profile used: " << profile << std::endl;
    if (useFFT) {
        std::cout << "Will use FFT based correlation." << std::endl;
    }

    // Initialize MLT
    Mlt::Factory::init(NULL);

    // Load an arbitrary profile
    Mlt::Profile prof(profile.c_str());

    // Load the MLT producers
    Mlt::Producer prodMain(prof, fileMain.c_str());
    if (!prodMain.is_valid()) {
        std::cout << fileMain << " is invalid." << std::endl;
        return 2;
    }
    Mlt::Producer prodSub(prof, fileSub.c_str());
    if (!prodSub.is_valid()) {
        std::cout << fileSub << " is invalid." << std::endl;
        return 2;
    }

    // Build the audio envelopes for the correlation
    AudioEnvelope *envelopeMain = new AudioEnvelope(fileMain.c_str(), &prodMain);
    envelopeMain->loadEnvelope();
    envelopeMain->loadStdDev();
    envelopeMain->dumpInfo();

    AudioEnvelope *envelopeSub = new AudioEnvelope(fileSub.c_str(), &prodSub);
    envelopeSub->loadEnvelope();
    envelopeSub->loadStdDev();
    envelopeSub->dumpInfo();

    // Calculate the correlation and hereby the audio shift
    AudioCorrelation corr(envelopeMain);
    int index = 0;
    corr.addChild(envelopeSub /*, useFFT*/);

    int shift = corr.getShift(index);
    std::cout << " Should be shifted by " << shift << " frames: " << fileSub << std::endl
              << "\trelative to " << fileMain << std::endl
              << "\tin a " << prodMain.get_fps() << " fps profile (" << profile << ")." << std::endl;

    if (saveImages) {
        QString outImg = QString::fromLatin1("envelope-main-%1.png").arg(QDateTime::currentDateTime().toString("yyyy-MM-dd-hh:mm:ss"));
        envelopeMain->drawEnvelope().save(outImg);
        std::cout << "Saved volume envelope as " << QFileInfo(outImg).absoluteFilePath().toStdString() << std::endl;
        outImg = QString::fromLatin1("envelope-sub-%1.png").arg(QDateTime::currentDateTime().toString("yyyy-MM-dd-hh:mm:ss"));
        envelopeSub->drawEnvelope().save(outImg);
        std::cout << "Saved volume envelope as " << QFileInfo(outImg).absoluteFilePath().toStdString() << std::endl;
        outImg = QString::fromLatin1("correlation-%1.png").arg(QDateTime::currentDateTime().toString("yyyy-MM-dd-hh:mm:ss"));
        corr.info(index)->toImage().save(outImg);
        std::cout << "Saved correlation image as " << QFileInfo(outImg).absoluteFilePath().toStdString() << std::endl;
    }

    //    Mlt::Factory::close();

    return 0;
}
