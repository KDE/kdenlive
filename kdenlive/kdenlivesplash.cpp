/***************************************************************************
                          kdenlivesplash  -  description
                             -------------------
    begin                : Qui Out 14 2004
    copyright            : (C) 2004 by Lúcio Flávio Corrêa
    email                : lucio.correa@uol.com.br
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/
#include <klocale.h>
#include "kdenlivesplash.h"

KdenliveSplash::KdenliveSplash(const QPixmap & pixmap, WFlags f):KSplashScreen(pixmap,
    f)
{
    message(i18n("Version 0.3 beta"), AlignLeft, white);
}

KdenliveSplash::~KdenliveSplash()
{
}
