/***************************************************************************
                          avformatwidgetcodec.h  -  description
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

#ifndef AVFORMATWIDGETCODEC_H
#define AVFORMATWIDGETCODEC_H

#include <qvbox.h>

#include <avformatwidgetbase.h>

class AVFormatDescCodec;

/**A widget that encapsulates the information in an avformatdesccodec object.
  *@author Jason Wood
  */

class AVFormatWidgetCodec:public QVBox, public AVFormatWidgetBase {
  Q_OBJECT public:
    AVFormatWidgetCodec(AVFormatDescCodec * desc, QWidget * parent =
	0, const char *name = 0);
    ~AVFormatWidgetCodec();
    virtual QWidget *widget();
};

#endif
