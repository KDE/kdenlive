/***************************************************************************
                          avformatdesccontainer.h  -  description
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

#ifndef AVFORMATDESCCONTAINER_H
#define AVFORMATDESCCONTAINER_H

#include "avformatdescbase.h"

#include <qptrlist.h>

/**A container description. A container groups and contains other descriptive elements.
  *@author Jason Wood
  */

class AVFormatDescContainer : public AVFormatDescBase {
public: 
	AVFormatDescContainer(const QString &description, const QString &name);
	~AVFormatDescContainer();
  /** Constructs a widget to display this container. Most likely, a qgroupbox. */
  virtual QWidget * createWidget(QWidget *parent);
  /** Appends a new description element into this container. */
  void append(AVFormatDescBase *elem);
  /** Returns the format list. */
  QPtrList<AVFormatDescBase> &list();
protected:
  /** A list of all dsecription elements contained within this description. */
  QPtrList<AVFormatDescBase> m_descList;
};

#endif
