/***************************************************************************
   Copyright (C) 2006-2008
   by Marco Gulino <marco@kmobiletools.org>
   adapted for MLT video preview by Jean-Baptiste Mardelle <jb@kdenlive.org>

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


#ifndef _westleypreview_H_
#define _westleypreview_H_

#include <qstring.h>

#include <qpixmap.h>
#include <kio/thumbcreator.h>
#include <qobject.h>

class QProcess;
class KTempDir;
class KRandomSequence;




class WestleyPreview : public QObject, public ThumbCreator {
    Q_OBJECT
public:
    WestleyPreview();
    virtual ~WestleyPreview();
    virtual bool create(const QString &path, int width, int height, QImage &img);
    virtual Flags flags() const;

protected:
    QPixmap getFrame(const QString &path);
    static uint imageVariance(QImage image);

private:
    QPixmap m_pixmap;
    QProcess *m_inigoprocess;
    QStringList customargs;
    KRandomSequence *m_rand;
    QString playerBin;
    bool startAndWaitProcess(const QStringList &args);
    enum frameflags { framerandom = 0x1, framestart = 0x2, frameend = 0x4 };
    struct {
        int towidth;
        int toheight;
        int fps;
        int seconds;
    } fileinfo;
};

#endif
