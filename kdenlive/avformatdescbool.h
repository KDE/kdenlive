/***************************************************************************
                          avformatdescbool.h  -  description
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

#ifndef AVFORMATDESCBOOL_H
#define AVFORMATDESCBOOL_H

#include <avformatdescbase.h>

/**Holds a boolean value in an AVFormat description.
  *@author Jason Wood
  */

class AVFormatDescBool : public AVFormatDescBase  {
public: 
	AVFormatDescBool(const QString &description, const QString &name);
	~AVFormatDescBool();
  /** Create a widget to handle a boolean value. Most likely, this will be a check box. */
  QWidget * createWidget(QWidget *parent);
};

#endif
