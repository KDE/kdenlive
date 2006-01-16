/***************************************************************************
                          avformatdesclist.h  -  description
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

#ifndef AVFORMATDESCLIST_H
#define AVFORMATDESCLIST_H

#include <avformatdescbase.h>

#include <qvaluevector.h>
#include <qstring.h>

/**Holds a list of possible values in an AVFormat Description.
  *@author Jason Wood
  */

class AVFormatDescList:public AVFormatDescBase {
  public:
    AVFormatDescList(const QString & description, const char *&name);
    ~AVFormatDescList();
  /** Create a widget to handle a list value. Most likely, this will be a listbox. */
    AVFormatWidgetBase *createWidget(QWidget * parent);
  /** Adds the specified string to the item list. */
    void addItem(const QString & item);
  /** Returns the item list. */
     QValueVector < QString > itemList();
  private:
     QValueVector < QString > m_itemList;
};

#endif
