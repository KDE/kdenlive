/***************************************************************************
                          avformatwidgetlist.h  -  description
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

#ifndef AVFORMATWIDGETLIST_H
#define AVFORMATWIDGETLIST_H

#include <qwidget.h>
#include <qhbox.h>

#include "avformatwidgetbase.h"

class QLabel;
class QComboBox;

/**A widget which handles the selection of a list, provided by the relevant AVFileDescList object.
  *@author Jason Wood
  */
class AVFormatDescList;
  
class AVFormatWidgetList : public QHBox, public AVFormatWidgetBase {
   Q_OBJECT
public: 
	AVFormatWidgetList(AVFormatDescList *desc, QWidget *parent=0, const char *name=0);
	~AVFormatWidgetList();
  virtual QWidget *widget();
private:
  QLabel *m_label;
  QComboBox *m_comboBox;
};

#endif
