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
#include <QProcess>
#include <kdebug.h>
#include <ktempdir.h>
#include <kurl.h>
#include <qfileinfo.h>
#include <KTemporaryFile>

#include <unistd.h>


#define DBG_AREA

//#include "config.h"
extern "C"
{
    KDE_EXPORT ThumbCreator *new_creator() {
        return new MltPreview;
    }
}

MltPreview::MltPreview() :
        QObject(),
        ThumbCreator(),
        m_meltProcess(0),
        m_rand(0)
{
}

MltPreview::~MltPreview()
{
    delete m_rand;
    delete m_meltProcess;
}

bool MltPreview::startAndWaitProcess(const QStringList &args)
{
    kDebug(DBG_AREA) << "MltPreview: starting process with args: " << args << endl;
    m_meltProcess->start(args.join(" "));
    if (! m_meltProcess->waitForStarted()) {
        kDebug(DBG_AREA) << "MltPreview: PROCESS NOT STARTED!!! exiting\n";
        return false;
    }
    if (! m_meltProcess->waitForFinished()) {
        kDebug(DBG_AREA) << "MltPreview: PROCESS DIDN'T FINISH!! exiting\n";
        m_meltProcess->close();
        return false;
    }
    kDebug() << "MltPreview: process started and ended correctly\n";
    return true;
}

bool MltPreview::create(const QString &path, int width, int /*height*/, QImage &img)
{
    QFileInfo fi(path);
    m_playerBin = KStandardDirs::findExe("melt");
    if (m_playerBin.isEmpty()) {
        kDebug(DBG_AREA) << "MltPreview: melt not found, exiting.\n";
        return false;
    }

    fileinfo.seconds = 0;
    fileinfo.fps = 0;

    m_rand = new KRandomSequence(QDateTime::currentDateTime().toTime_t());
    m_meltProcess = new QProcess();
    KUrl furl(path);
    kDebug(DBG_AREA) << "videopreview: url=" << furl << "; local:" << furl.isLocalFile() << endl;
    fileinfo.towidth = width;
    fileinfo.toheight = width * 3 / 4;
    QImage pix;
//    if(furl.isLocalFile())
//    {
    QStringList args;
    //TODO: modify melt so that it can return some infos about a MLT XML clip (duration, track number,fps,...)
    // without actually playing the file
    /*
        args << playerBin << QString("\"" + path + "\"") << "-file-info";

        kDebug(DBG_AREA) << "videopreview: starting process: --_" << " " << args.join(" ") << "_--\n";
        if (! startAndWaitProcess(args) ) return NULL;

        QString information=QString(meltProcess->readAllStandardOutput() );
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

    const int LASTTRY = 3;
    for (int i = 0; i <= LASTTRY; i++) {
        pix = getFrame(path);
        if (!pix.isNull()) {
            uint variance = imageVariance(pix);
            kDebug(DBG_AREA) << "videopreview: " << QFileInfo(path).fileName() << " frame variance: " << variance << "; " <<
            ((variance <= 40 && (i != LASTTRY - 1)) ? "!!!DROPPING!!!" : "GOOD :-)") << endl;
            if (variance > 40 || i == LASTTRY - 1) break;
        }
    }
    if (pix.isNull()) {
        return false;
    }

    if (pix.depth() != 32)
        img = pix.convertToFormat(QImage::Format_RGB32);
    else img = pix;
    return true;
}

QImage MltPreview::getFrame(const QString &path)
{
    QStringList args;
    const int START = 25;
    const int RANGE = 500;
    args.clear();
    args << m_playerBin << "\"" + path + "\"";

    unsigned long start = (unsigned long)(START + (m_rand->getDouble() * RANGE));
    args << QString("in=%1").arg(start) << QString("out=%1").arg(start) << "-consumer";

    KTemporaryFile temp;
    temp.setSuffix(".png");
    temp.open();
    args << QString("avformat:%1").arg(temp.fileName()) << "vframes=1" << "f=rawvideo" << "vcodec=png" << QString("s=%1x%2").arg(fileinfo.towidth).arg(fileinfo.toheight);
    if (! startAndWaitProcess(args)) return QImage();
    QImage retpix(temp.fileName());
    temp.close();
    return retpix;
}


uint MltPreview::imageVariance(QImage image)
{
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

ThumbCreator::Flags MltPreview::flags() const
{
    return None;
}


#include "westleypreview.moc"

