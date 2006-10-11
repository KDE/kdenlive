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
#include <qlabel.h>
#include <qobjectlist.h>

#include <klocale.h>
#include <kdebug.h>
#include <klistview.h>
#include <kurlrequester.h>
#include <kmessagebox.h>
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
    int currentStart = 0;
    int currentChapter = 0;
    Timecode tc;

    while (!node.isNull()) {
	QDomElement element = node.toElement();
	if (!element.isNull()) {
	    if (element.tagName() == "guide" && element.attribute("chapter").toInt() != -1) {
		int position = element.attribute("position").toInt();
		int chapter = element.attribute("chapter").toInt();
		QString st = tc.getTimecode(GenTime(position, m_fps), m_fps);
		if (chapter_list->childCount() != 0) chapter_list->lastItem()->setText(2, st);
		if (chapter == 1000) {
		    if (chapter_list->childCount() == 0)
			(void) new KListViewItem(chapter_list, i18n("Chapter %1").arg(currentChapter), "00:00:00:00", st);
		    break;
		}
		(void) new KListViewItem(chapter_list, i18n("Chapter %1").arg(currentChapter), st);
	        currentChapter++;
	    }
	}
	node = node.nextSibling();
    }

    if (chapter_list->childCount() == 0) {
	KMessageBox::information(this, i18n("You didn't define any DVD chapters (markers). The complete timeline will be exported as a single chapter"));
	(void) new KListViewItem(chapter_list, i18n("Chapter 0"), "00:00:00:00", tc.getTimecode(m_project->duration(), m_fps));
    }

    if (chapter_list->lastItem()->text(2).isEmpty()) {
	// User didn't set the DVD end, use last frame of project
	QString st = tc.getTimecode(m_project->duration(), m_fps);
	chapter_list->lastItem()->setText(2, st);
    }
    // Calculate dvd duration
    QString startF = chapter_list->firstChild()->text(1);
    QString endF = chapter_list->lastItem()->text(2);
    GenTime startFrame = timeFromString(startF);
    GenTime endFrame = timeFromString(endF);
    total_duration->setText(tc.getTimecode(endFrame - startFrame, m_fps));
}

void ExportDvdDialog::generateDvdXml() {
    Timecode tc;
    QString chapterTimes;
    if (chapter_list->childCount() > 1) {
	chapterTimes = "00:00:00.00";
	
	QListViewItem * myChild = chapter_list->firstChild();
	GenTime offset = timeFromString(myChild->text(1));
	myChild = myChild->nextSibling();
	while( myChild ) {
	    QString ct = tc.getTimecode(timeFromString(myChild->text(1)) - offset, m_fps);
            chapterTimes += "," + ct.replace(8,1,".");
            myChild = myChild->nextSibling();
    	}
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
    if (!chapterTimes.isEmpty()) vob.setAttribute("chapters", chapterTimes);
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

GenTime ExportDvdDialog::timeFromString(QString timeString) {
    int frames = (int) ((timeString.section(":",0,0).toInt()*3600 + timeString.section(":",1,1).toInt()*60 + timeString.section(":",2,2).toInt()) * m_fps + timeString.section(":",3,3).toInt());
    return GenTime(frames, m_fps);
}

} // End Gui

