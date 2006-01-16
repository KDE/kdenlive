/***************************************************************************
                          avfileformatwidget.h  -  description
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

#ifndef AVFILEFORMATWIDGET_H
#define AVFILEFORMATWIDGET_H

#include <qvbox.h>
#include <kurl.h>

#include "avformatwidgetbase.h"
/**This widget contains an entire file format description.
  *@author Jason Wood
  */

class AVFileFormatDesc;
class KURLRequester;
class QHBox;
class QLabel;

class AVFileFormatWidget:public QVBox, public AVFormatWidgetBase {
  Q_OBJECT public:
    AVFileFormatWidget(AVFileFormatDesc * desc, QWidget * parent =
	0, const char *name = 0);
    ~AVFileFormatWidget();
    QWidget *widget();
   /** Returns the url of this file widget. */
    KURL fileUrl() const;
    AVFileFormatWidget *fileFormatWidget();
  private:
     QHBox * m_fileHBox;
    QLabel *m_fileLabel;
    KURLRequester *m_filename;
    AVFileFormatDesc *m_desc;
};

#endif
