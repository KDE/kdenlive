/***************************************************************************
                          clippropertiesdialog.h  -  description
                             -------------------
    begin                : Mon Mar 17 2003
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

#ifndef CLIPPROPERTIESDIALOG_H
#define CLIPPROPERTIESDIALOG_H

#include <qwidget.h>
#include <kdialog.h>

class AVFile;

/**Displays the properties of a sepcified clip.
  *@author Jason Wood
  */

class ClipPropertiesDialog : public KDialog  {
   Q_OBJECT
public: 
	ClipPropertiesDialog(QWidget *parent=0, const char *name=0);
	~ClipPropertiesDialog();
  /** Specifies the clip that we wish to display the properties of. */
  void setClip(const AVFile *clip);
private:
  	AVFile *m_clip;
};

#endif
