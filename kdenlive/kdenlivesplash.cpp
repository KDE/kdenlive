//////////////////////////////////////////////////////////////////////////////
//
//    KDENLIVESPLASH.CPP
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


#include "kdenlivesplash.h"


/////////////////////////////////////////////////////////////////////////////////////////////

KdenliveSplash::KdenliveSplash(QString PNGImageFileName) : QWidget( 0L, "Kdenlive Splash",
               WStyle_NoBorder | WStyle_StaysOnTop |
               WStyle_Customize | WDestructiveClose )
{
  QString file = locate( "appdata", PNGImageFileName );
    
  QPixmap pixmap(file);

  // The size of the splashscreen image.

  int H=pixmap.width();
  int L=pixmap.height();

  setGeometry( QApplication::desktop()->width ()/2-(H/2),
               QApplication::desktop()->height()/2-(L/2), H, L );

  setFixedSize( H, L );

  setPaletteBackgroundPixmap(pixmap);

  mTimer = new QTimer();
  connect(mTimer, SIGNAL(timeout()), this, SLOT(slot_close()));

  mTimer->start(2000, true);
}


/////////////////////////////////////////////////////////////////////////////////////////////

KdenliveSplash::~KdenliveSplash()
{
  delete mTimer;
}


/////////////////////////////////////////////////////////////////////////////////////////////

void KdenliveSplash::slot_close()
{
  close();
}

