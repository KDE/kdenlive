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
#ifndef KDENLIVESPLASH_H
#define KDENLIVESPLASH_H

#include <ksplashscreen.h>

class QPixmap;

/**
@author Lúcio Flávio Corrêa
*/
class KdenliveSplash : public KSplashScreen
{
public:
    KdenliveSplash(const QPixmap& pixmap, WFlags f = 0);
    ~KdenliveSplash();
};

#endif
