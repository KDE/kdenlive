/***************************************************************************
                          kmmscreen.h  -  description
                             -------------------
    begin                : Mon Mar 25 2002
    copyright            : (C) 2002 by Jason Wood
    email                : jasonwood@blueyonder.co.uk
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef KMMSCREEN_H
#define KMMSCREEN_H

#include <qwidget.h>

/**KMMScreen can display any video that arts plays.
If audio is being played, it will display a visual representation
of the music, and if it is a part of the file that has not
been rendered, it will say "not rendered" or something
like that.
  *@author Jason Wood
  */

class KMMScreen : public QWidget  {
   Q_OBJECT
public: 
	KMMScreen(QWidget *parent=0, const char *name=0);
	~KMMScreen();
};

#endif
