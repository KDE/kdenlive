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
#include <kprocess.h>

#include "docclipproject.h"
#include "exportdvd_ui.h"
#include "exportwidget.h"
#include "definitions.h"

namespace Gui {

/**The export dialog alllows the user to choose a file format and the relevant parameters with which they want to save their project.
  *@author Jason Wood
  */


class ExportDvdDialog:public ExportDvd_UI {
  Q_OBJECT public:
    ExportDvdDialog(DocClipProject *proj, exportWidget *render_widget, formatTemplate format, QWidget * parent = 0, const char *name = 0);
    virtual ~ExportDvdDialog();

    public slots:		// Public slots
	void fillStructure(QDomDocument xml);

    private slots:
	void generateDvdXml();
	void generateMenuMovie();
	void previewDvd();
	void burnDvd();
	void endExport(KProcess *);
	void checkFolder();
	void slotFinishExport(bool isOk);
	void slotNextPage();
	void movieMenuDone(KProcess *);
	void spuMenuDone(KProcess *);
	void generateMenuImages();
	void generateMenuPreview();
	void refreshPreview();
	void slotCheckRendered();
	void slotCheckMenuImage();
	void slotCheckMenuMovie();
	void generateImage(QString imageName, QString buttonText, QColor color);
	void generateTranspImage(QString imageName, QString buttonText, QColor color);
	void slotSetStandard(int std);
	void openWithQDvdauthor();
	void generateDvd();
	void dvdFailed();
	void receivedStderr(KProcess *, char *buffer, int len);
	void slotUpdateNextButton(bool isOn);

    private:			// Private attributes
	GenTime timeFromString(QString timeString);
	DocClipProject *m_project;
	double m_fps;
	KProcess *m_exportProcess;
	QString xml_file;
	QString spuxml_file;
	exportWidget *m_render_widget;
	QString m_movie_file;
	QString m_menu_movie_file;
	QString m_processlog;
	formatTemplate m_format;
	bool m_isNTSC;
};

}				// namespace Gui

#endif
