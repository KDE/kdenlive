/***************************************************************************
                          avformatdescbase.h  -  description
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

#ifndef AVFORMATDESCBASE_H
#define AVFORMATDESCBASE_H

#include <qstring.h>

class AVFormatWidgetBase;
class QWidget;

/**The base class for a file format parameter. A parameter specifies, amongst other things, what values the parameter can take, and how the widget that contains it should be created.

  *@author Jason Wood
  */

class AVFormatDescBase {
  public:
    AVFormatDescBase(const QString & description, const char *&name);
     virtual ~ AVFormatDescBase();
  /** Generates a widget that holds the specified value(s). */
    virtual AVFormatWidgetBase *createWidget(QWidget * parent) = 0;
  /** Returns the name of this description element. */
    const char *&name();
  /** Sets the name of this Desc element to the one specified */
    void setName(const char *&name);
  /** Sets the description of this desc element. */
    void setDescription(const QString & description);
  /** Returns the description of this desc element. */
    const QString & description();
  protected:			// Protected attributes
  /** The text description for the widget created - displayed in tooltips. */
     QString m_description;
  /** The name (text label) to be used by the widget created. */
    const char *m_name;
};

#endif
