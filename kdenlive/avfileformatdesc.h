/***************************************************************************
                          avfileformatdesc.h  -  description
                             -------------------
    begin                : Tue Jan 21 2003
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

#ifndef AVFILEFORMATDESC_H
#define AVFILEFORMATDESC_H

#include <qstring.h>
#include "avformatdesccontainer.h"

/** Describes a file format. That is to say, it says what can and cannot exist within a
    file format, what parameters exist and can be set, etc.
  *@author Jason Wood
  */

class QWidget;

class AVFileFormatDesc : public AVFormatDescContainer {
public: 
	AVFileFormatDesc(const QString &description, const QString &name);
	~AVFileFormatDesc();
  /** Create and return a widget that embodies this file format description. */
  AVFormatWidgetBase * createWidget(QWidget *parent);
  /** Sets the file extenstion for this description. */
  void setFileExtension(QString extension);
  /** Returns the file extension for this file format. */
  const QString &fileExtension();
private: // Private attributes
  /** Specifies the file extension used by this file format. */
  QString m_fileExtension;
};

#endif
