/***************************************************************************
                          avformatdesccodec.h  -  description
                             -------------------
    begin                : Tue Feb 4 2003
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

#ifndef AVFORMATDESCCODEC_H
#define AVFORMATDESCCODEC_H

#include <qstring.h>

#include <avformatdescbase.h>

/**Holds the description of a particular file codec, including parameters for that codec.
  *@author Jason Wood
  */

class AVFormatDescCodec : public AVFormatDescBase  {
public: 
	AVFormatDescCodec(const QString &description, const QString &name);
	~AVFormatDescCodec();
  /** Generates a widget that holds the specified value(s). */
  AVFormatWidgetBase * createWidget(QWidget * parent);
};

#endif
