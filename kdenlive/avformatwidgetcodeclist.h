/***************************************************************************
                          AVFormatWidgetCodecList.h  -  description
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

#ifndef AVFormatWidgetCodecList_H
#define AVFormatWidgetCodecList_H

#include <qgroupbox.h>

#include "avformatwidgetbase.h"

class QHBox;
class QLabel;
class QComboBox;
class QWidgetStack;

class AVFormatDescCodecList;

/**Displays all information described in AVFormatDescCodec
  *@author Jason Wood
  */

class AVFormatWidgetCodecList : public QGroupBox, public AVFormatWidgetBase  {
   Q_OBJECT
public:
	AVFormatWidgetCodecList(AVFormatDescCodecList *desc, QWidget *parent=0, const char *name=0);
	~AVFormatWidgetCodecList();
   virtual QWidget *widget();
private: // Private attributes
  /** This bnox lays out the codec and label side by side */
  QHBox *m_codecLayout;
  /** This label names the codec select box */
  QLabel *m_codecLabel;
  /** This combo box contains all known combo names. Choosing an entry changes the topmost widget in
   m_widgetStack. */
  QComboBox *m_codecSelect;
  /** The widgetStack contains the various dialogs for different codecs. The top-most is specified by
  QComboBox. */
  QWidgetStack *m_widgetStack;
};

#endif
