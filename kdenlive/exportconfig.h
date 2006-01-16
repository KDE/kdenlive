/***************************************************************************
                          exportconfig  -  description
                             -------------------
    begin                : Sun Nov 16 2003
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
#ifndef EXPORTCONFIG_H
#define EXPORTCONFIG_H

#include <kjanuswidget.h>
#include <kurl.h>

#include <avfileformatdesc.h>


/**
The Export Configuration page

@author Jason Wood
*/
class ExportConfig:public KJanusWidget {
  Q_OBJECT public:
    ExportConfig(QPtrList < AVFileFormatDesc > &formatList,
	QWidget * parent = 0, const char *name = 0, WFlags f = 0);

    ~ExportConfig();

	/** Generate a layout for this dialog, based upon the values that have been passed to it. */
    void generateLayout();

	/** Returns the url set inside of this export dialog. */
    KURL url();
    public slots:		// Public slots
	/** Specify a new file format list, and reconstruct the dialog box. */
    void setFormatList(const QPtrList < AVFileFormatDesc > &list);
  private:
	/** A list of all known file formats */
     QPtrList < AVFileFormatDesc > m_formatList;
	/** A list of all pages that have been created */
     QPtrList < AVFormatWidgetBase > m_pageList;
};

#endif
