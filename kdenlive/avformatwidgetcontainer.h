/***************************************************************************
                          avformatwidgetcontainer.h  -  description
                             -------------------
    begin                : Fri Jan 24 2003
    copyright            : (C) 2003 by Jason Wood
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

#ifndef AVFORMATWIDGETCONTAINER_H
#define AVFORMATWIDGETCONTAINER_H

#include <qgroupbox.h>

#include "avformatwidgetbase.h"

class AVFormatDescContainer;

/**A widget that will contain and display the information required by an AVFormatDescContainer.
  *@author Jason Wood
  */

class AVFormatWidgetContainer : public QGroupBox, public AVFormatWidgetBase  {
   Q_OBJECT
public: 
	AVFormatWidgetContainer(AVFormatDescContainer *desc, QWidget *parent=0, const char *name=0);
	~AVFormatWidgetContainer();
  virtual QWidget *widget();
};

#endif
