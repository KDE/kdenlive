/***************************************************************************
                          avformatwidgetbase.h  -  description
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

#ifndef AVFORMATWIDGETBASE_H
#define AVFORMATWIDGETBASE_H


/**A base class for all AVFormat related widgets.

This class provides a standard set of functions for building and reading an AVFileFormat from them.
  *@author Jason Wood
  */

class AVFormatWidgetBase {
public: 
	AVFormatWidgetBase();
	virtual ~AVFormatWidgetBase();
};

#endif
