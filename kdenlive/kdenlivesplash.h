//////////////////////////////////////////////////////////////////////////////
//
//    KDENLIVESPLASH.H
//
//    Copyright (C) 2003 Gilles CAULIER <caulier.gilles@free.fr>
//
//    This program is free software; you can redistribute it and/or modify
//    it under the terms of the GNU General Public License as published by
//    the Free Software Foundation; either version 2 of the License, or
//    (at your option) any later version.
//
//    This program is distributed in the hope that it will be useful,
//    but WITHOUT ANY WARRANTY; without even the implied warranty of
//    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//    GNU General Public License for more details.
//
//    You should have received a copy of the GNU General Public License
//    along with this program; if not, write to the Free Software
//    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
//
//////////////////////////////////////////////////////////////////////////////


#ifndef KDENLIVESPLASH_H
#define KDENLIVESPLASH_H

// Qt includes

#include <qwidget.h>
#include <qstring.h>
#include <qpixmap.h>
#include <qlabel.h>
#include <qapplication.h>
#include <qtimer.h>

// KDElib includes

#include <klocale.h>
#include <kstandarddirs.h>


class QTimer;

class KdenliveSplash : public QWidget
{
Q_OBJECT

public:
    KdenliveSplash(QString PNGImageFileName);
    ~KdenliveSplash();

public slots:
    void slot_close();

private:
    QTimer* mTimer;
};

#endif // KDENLIVESPLASH_H
