/***************************************************************************
                          avfilelist.h  -  description
                             -------------------
    begin                : Thu Nov 21 2002
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

#ifndef AVFILELIST_H
#define AVFILELIST_H

#include <qptrlist.h>
#include <qdom.h>

#include "avfile.h"

/**Holds a list of AVFiles.

Works like QPtrList<AVFile>, but
contains some convenience functions, such as toXML()
  *@author Jason Wood
  */

class AVFileList : public QPtrList<AVFile>  {
public: 
	AVFileList();	
	~AVFileList();
  /** Returns an XML representation of the list. */
  QDomDocument toXML();
};

#endif
