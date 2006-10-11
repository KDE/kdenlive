/***************************************************************************
                          ExportDvdDialog.cpp  -  description
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

#include <qlayout.h>
#include <qobjectlist.h>

#include <klocale.h>
#include <kdebug.h>
#include <klistview.h>
#include <kurlrequester.h>
#include <kio/netaccess.h>

#include "timecode.h"
#include "kdenlivesettings.h"
#include "exportdvddialog.h"

namespace Gui {

ExportDvdDialog::ExportDvdDialog(DocClipProject *proj, QWidget * parent, const char *name):ExportDvd_UI(parent, name), m_project(proj)
{
    if (!KIO::NetAccess::exists(KURL(KdenliveSettings::currentdefaultfolder() + "/dvd/"), false, this))
	KIO::NetAccess::mkdir(KURL(KdenliveSettings::currentdefaultfolder() + "/dvd/"), this);
    m_fps = m_project->framesPerSecond();
    xml_file->setURL(KdenliveSettings::currentdefaultfolder() + "/dvd/dvdauthor.xml" );
    render_file->setURL(KdenliveSettings::currentdefaultfolder() + "/dvd/movie.vob" );

    connect(buttonOk, SIGNAL(clicked()), this, SLOT(generateDvdXml()));
}

ExportDvdDialog::~ExportDvdDialog()
{
}

void ExportDvdDialog::fillStructure(QDomDocument xml) {
    QDomElement docElem = xml.documentElement();
    QDomNode node = docElem.firstChild();
    bool chapterOpen = false;
    int currentStart = 0;
    int currentChapter = 0;
    Timecode tc;

    while (!node.isNull()) {
	QDomElement element = node.toElement();
	if (!element.isNull()) {
	    if (element.tagName() == "guide" && element.attribute("chapter").toInt() != -1) {
		int position = element.attribute("position").toInt();
		int chapter = element.attribute("chapter").toInt();
		if (!chapterOpen) {
		    currentStart = position;
		    currentChapter = chapter;
		    chapterOpen = true;
		}
		else {
		    if (chapter == 1000) chapterOpen = false;
		    QString st = tc.getTimecode(GenTime(currentStart, m_fps), m_fps);
		    QString en = tc.getTimecode(GenTime(position, m_fps), m_fps);
		    (void) new KListViewItem(chapter_list, i18n("Chapter %1").arg(currentChapter), st, en);
		}
	    }
	}
	node = node.nextSibling();
    }
}

void ExportDvdDialog::generateDvdXml() {
	QString chapterTimes = "00:00:00.00";
        QListViewItem * myChild = chapter_list->firstChild();
        while( myChild ) {
            chapterTimes += "," + myChild->text(1).replace(8,1,".");
            myChild = myChild->nextSibling();
        }
	QDomDocument doc;
	QDomElement main = doc.createElement("dvdauthor");
	main.appendChild(doc.createElement("vmgm"));
	QDomElement titleset = doc.createElement("titleset");
	main.appendChild(titleset);
	QDomElement titles = doc.createElement("titles");
	titleset.appendChild(titles);
	QDomElement pgc = doc.createElement("pgc");
	titles.appendChild(pgc);
	QDomElement vob = doc.createElement("vob");
	vob.setAttribute("file", render_file->url());
	vob.setAttribute("chapters", chapterTimes);
	pgc.appendChild(vob);
	doc.appendChild(main);

	QFile *file = new QFile();
        file->setName(xml_file->url());
        file->open(IO_WriteOnly);
        QTextStream stream( file );
        stream.setEncoding (QTextStream::UnicodeUTF8);
        stream << doc.toString();
        file->close();
	//kdDebug()<<"- - - - - - -"<<endl;
	//kdDebug()<<doc.toString()<<endl;
}

} // End Gui

