/***************************************************************************
                          exportdialog.h  -  description
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

#ifndef EXPORTDIALOG_H
#define EXPORTDIALOG_H

#include <qwidget.h>
#include <qptrlist.h>

#include <kdialogbase.h>
#include <kurl.h>

#include "avfileformatdesc.h"
#include "avformatwidgetbase.h"

/**The export dialog alllows the user to choose a file format and the relevant parameters with which they want to save their project.
  *@author Jason Wood
  */

class ExportDialog : public KDialogBase  {
   Q_OBJECT
public: 
	ExportDialog(QPtrList<AVFileFormatDesc> &formatList, QWidget *parent=0, const char *name=0);
	~ExportDialog();
  /** Generate a layout for this dialog, based upon the values that have been passed to it. */
  void generateLayout();  
  /** Returns the url set inside of this export dialog. */
  KURL url();
private: // Private attributes
  /** A list of all known file formats */
  QPtrList<AVFileFormatDesc> m_formatList;
  /** A list of all pages that have been created */
  QPtrList<AVFormatWidgetBase> m_pageList;
public slots: // Public slots
  /** Specify a new file format list, and reconstruct the dialog box. */
  void setFormatList(const QPtrList<AVFileFormatDesc> &list);
};

#endif
