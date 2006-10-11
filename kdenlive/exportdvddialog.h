/***************************************************************************
                          exportdvddialog.h  -  description
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

#ifndef EXPORTDVDDIALOG_H
#define EXPORTDVDDIALOG_H

#include <qwidget.h>
#include <qdom.h>

#include <kdialogbase.h>
#include <kurl.h>

#include "docclipproject.h"
#include "exportdvd_ui.h"

namespace Gui {

/**The export dialog alllows the user to choose a file format and the relevant parameters with which they want to save their project.
  *@author Jason Wood
  */


class ExportDvdDialog:public ExportDvd_UI {
  Q_OBJECT public:
    ExportDvdDialog(DocClipProject *proj, QWidget * parent = 0, const char *name = 0);
    ~ExportDvdDialog();

    public slots:		// Public slots
	void fillStructure(QDomDocument xml);

    private slots:
	void generateDvdXml();

    private:			// Private attributes
	GenTime timeFromString(QString timeString);
	DocClipProject *m_project;
	double m_fps;
};

}				// namespace Gui

#endif
