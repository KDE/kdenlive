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

#include <qfile.h>
#include <qimage.h>
#include <QtCore/QVarLengthArray>

#include <kstandarddirs.h>
#include <krandomsequence.h>
#include <qdatetime.h>
#include <QProcess>
#include <kdebug.h>
#include <ktempdir.h>
#include <kurl.h>
#include <qfileinfo.h>
#include <KTemporaryFile>

#include <unistd.h>

#include "westleypreview.h"

#define DBG_AREA

//#include "config.h"
extern "C" {
    KDE_EXPORT ThumbCreator *new_creator() {
        return new WestleyPreview;
    }
}

WestleyPreview::WestleyPreview()
        : m_rand(0), m_inigoprocess(0) {
}

WestleyPreview::~WestleyPreview() {
    delete m_rand;
    delete m_inigoprocess;
}

bool WestleyPreview::startAndWaitProcess(const QStringList &args) {
    kDebug(DBG_AREA) << "westleypreview: starting process with args: " << args << endl;
    m_inigoprocess->start(args.join(" "));
    if (! m_inigoprocess->waitForStarted()) {
        kDebug(DBG_AREA) << "westleypreview: PROCESS NOT STARTED!!! exiting\n";
        return false;
    }
    if (! m_inigoprocess->waitForFinished()) {
        kDebug(DBG_AREA) << "westleypreview: PROCESS DIDN'T FINISH!! exiting\n";
        m_inigoprocess->close();
        return false;
    }
    kDebug() << "westleypreview: process started and ended correctly\n";
    return true;
}

bool WestleyPreview::create(const QString &path, int width, int height, QImage &img) {
    QFileInfo fi(path);
    /*if (fi.suffix().trimmed() != "westley" && fi.suffix().trimmed() != "kdenlive")
    {
        kDebug(DBG_AREA) << "westleypreview: matched extension " << fi.suffix().prepend('.') << "; exiting.\n";
        return false;
    }*/
    playerBin = KStandardDirs::findExe("inigo");
    if (playerBin.isEmpty()) {
        kDebug(DBG_AREA) << "westleypreview: inigo not found, exiting.\n";
        return false;
    }

    fileinfo.seconds = 0;
    fileinfo.fps = 0;

    m_rand = new KRandomSequence(QDateTime::currentDateTime().toTime_t());
    m_inigoprocess = new QProcess();
    KUrl furl(path);
    kDebug(DBG_AREA) << "videopreview: url=" << furl << "; local:" << furl.isLocalFile() << endl;
    fileinfo.towidth = width;
    fileinfo.toheight = height;
    QPixmap pix;
//    if(furl.isLocalFile())
//    {
    QStringList args;
    //TODO: modify inigo so that it can return some infos about a westley clip (duration, track number,fps,...)
    // without actually playing the file
    /*
        args << playerBin << QString("\"" + path + "\"") << "-file-info";

        kDebug(DBG_AREA) << "videopreview: starting process: --_" << " " << args.join(" ") << "_--\n";
        if (! startAndWaitProcess(args) ) return NULL;

        QString information=QString(inigoprocess->readAllStandardOutput() );
        QRegExp findInfos("ID_VIDEO_FPS=([\\d]*).*ID_LENGTH=([\\d]*).*");
        if(findInfos.indexIn( information) == -1 )
        {
            kDebug(DBG_AREA) << "videopreview: No information found, exiting\n";
            return NULL;
        }
        fileinfo.seconds =findInfos.cap(2).toInt();
        fileinfo.fps=findInfos.cap(1).toInt();
        */
    fileinfo.seconds = 250;
    fileinfo.fps = 25;

    //kDebug(DBG_AREA) << "videopreview: find length=" << fileinfo.seconds << ", fps=" << fileinfo.fps << endl;

    const int LASTTRY = 3;
    for (int i = 0; i <= LASTTRY; i++) {
        pix = getFrame(path);
        if (!pix.isNull()) {
            uint variance = imageVariance(pix.toImage()/*.bits(),( (width+ 7) & ~0x7), width, height, 1 */);
            kDebug(DBG_AREA) << "videopreview: " << QFileInfo(path).fileName() << " frame variance: " << variance << "; " <<
            ((variance <= 40 && (i != LASTTRY - 1)) ? "!!!DROPPING!!!" : "GOOD :-)") << endl;
            if (variance > 40 || i == LASTTRY - 1) break;
        }
    }
    if (pix.isNull()) {
        return false;
    }
    img = pix.toImage();
    return true;
}

QPixmap WestleyPreview::getFrame(const QString &path) {
    QStringList args;
#define START ((fileinfo.seconds*15)/100)
#define END ((fileinfo.seconds*70)/100)
    args.clear();
    args << playerBin << "\"" + path + "\"";
    if (fileinfo.towidth > fileinfo.toheight) fileinfo.toheight = -2;
    else fileinfo.towidth = -2;
//     switch( flags ){
//         case random
//     }
    unsigned long start = (unsigned long)(START + (m_rand->getDouble() * (END - START)));
    args << QString("in=%1").arg(start) << QString("out=%1").arg(start) << "-consumer";

    KTemporaryFile temp;
    temp.setSuffix(".png");
    temp.open();
    args << QString("avformat:%1").arg(temp.fileName()) << "vframes=1" << "f=rawvideo" << "vcodec=png" << QString("s=%1x%2").arg(fileinfo.towidth).arg(fileinfo.toheight);
    if (! startAndWaitProcess(args)) return NULL;
    QPixmap retpix(temp.fileName());
    temp.close();
    return retpix;
}


uint WestleyPreview::imageVariance(QImage image) {
    uint delta = 0;
    uint avg = 0;
    uint bytes = image.numBytes();
    uint STEPS = bytes / 2;
    QVarLengthArray<uchar> pivot(STEPS);
    kDebug(DBG_AREA) << "Using " << STEPS << " steps\n";
    uchar *bits = image.bits();
    // First pass: get pivots and taking average
    for (uint i = 0; i < STEPS ; i++) {
        pivot[i] = bits[i*(bytes/STEPS)];
        avg += pivot[i];
    }
    avg = avg / STEPS;
    // Second Step: calculate delta (average?)
    for (uint i = 0; i < STEPS; i++) {
        int curdelta = abs(int(avg - pivot[i]));
        delta += curdelta;
    }
    return delta / STEPS;
}

ThumbCreator::Flags WestleyPreview::flags() const {
    return DrawFrame;
}

#include "westleypreview.moc"

