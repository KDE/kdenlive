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


class QWidget;
class KURL;
class AVFileFormatWidget;

/**A base class for all AVFormat related widgets.

This class provides a standard set of functions for building and reading an AVFileFormat from them.
  *@author Jason Wood
  */

class AVFormatWidgetBase {
public: 
	AVFormatWidgetBase();
	virtual ~AVFormatWidgetBase();

  /** Returns a pointer to this widget, as a QWidget. */
  virtual QWidget *widget() = 0;
  /** Returns a pointer to this widget cast as an AVFileFormatWidget if that is what this widget it, otherwise it returns 0. */
  virtual AVFileFormatWidget * fileFormatWidget();
};

#endif
