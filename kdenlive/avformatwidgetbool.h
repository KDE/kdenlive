/***************************************************************************
                          avformatwidgetbool.h  -  description
                             -------------------
    begin                : Thu Jan 23 2003
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

#ifndef AVFORMATWIDGETBOOL_H
#define AVFORMATWIDGETBOOL_H

#include <qwidget.h>
#include <qcheckbox.h>

#include "avformatwidgetbase.h"

class AVFormatDescBool;

/**This widget displays a boolean value, in alignment with what the AVFormatDescBool class describes.
  *@author Jason Wood
  */

class AVFormatWidgetBool : public QCheckBox, public AVFormatWidgetBase  {
   Q_OBJECT
public: 
	AVFormatWidgetBool(AVFormatDescBool *desc, QWidget *parent=0, const char *name=0);
	~AVFormatWidgetBool();
  virtual QWidget *widget();
  const KURL & fileUrl() const;  
};

#endif
