/***************************************************************************
                          kmmeditpanel.h  -  description
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

#ifndef KMMEDITPANEL_H
#define KMMEDITPANEL_H

#include <qwidget.h>

/**KMMEditPanel contains all of the controls necessary to select areas of video, audio, etc.
  *@author Jason Wood
  */

class KMMEditPanel : public QWidget  {
   Q_OBJECT
public: 
	KMMEditPanel(QWidget *parent=0, const char *name=0);
	~KMMEditPanel();
};

#endif
