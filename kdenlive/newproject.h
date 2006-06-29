/***************************************************************************
                          newproject  -  description
                             -------------------
    begin                : Wed Jun 28 2006
    copyright            : (C) 2006 by Jason Wood
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


#ifndef NEWPROJECT_H
#define NEWPROJECT_H

#include <kurl.h>

#include "newproject_ui.h"

namespace Gui {

    class newProject:public newProject_UI {
      Q_OBJECT public:
		/** construtor of KdenliveApp, calls all init functions to create the application.
		  */
	newProject(QWidget * parent = 0, const char *name = 0, QStringList list = 0);
	~newProject();

	KURL m_selectedProject;
	bool m_isNewFile;

    public slots:
	void adjustButton();
	bool isNewFile();
	KURL selectedFile();

    private slots:
	void openProject();
    	void checkFile();
    };

}				// namespace Gui
#endif
