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
#include <qcursor.h>
#include <qobjectlist.h>
#include <qradiobutton.h>
#include <qspinbox.h>

#include <klocale.h>
#include <kcolorbutton.h>
#include <kdebug.h>
#include <klistview.h>
#include <kurlrequester.h>
#include <kmessagebox.h>
#include <kfiledialog.h>
#include <kio/netaccess.h>
#include <kurlrequesterdlg.h>

#include "timecode.h"
#include "kdenlivesettings.h"
#include "exportdvddialog.h"

namespace Gui {

ExportDvdDialog::ExportDvdDialog(DocClipProject *proj, exportWidget *render_widget, QWidget * parent, const char *name):ExportDvd_UI(parent, name), m_project(proj), m_exportProcess(0), m_render_widget(render_widget)
{
    dvd_folder->setURL(KdenliveSettings::currentdefaultfolder() + "/dvd/");
    m_fps = m_project->framesPerSecond();
    xml_file = KdenliveSettings::currentdefaultfolder() + "/dvdauthor.xml";
    setCaption(i18n("Create DVD"));
    setTitle(page(0), i18n("Rendering"));
    setTitle(page(1), i18n("Menu"));
    setTitle(page(3), i18n("Burning"));

    render_file->fileDialog()->setURL(KURL(KdenliveSettings::currentdefaultfolder()));
    render_file->fileDialog()->setFilter("video/mpeg");
    chapter_list->setItemsRenameable(true);
    chapter_list->setRenameable(0, false);
    chapter_list->setRenameable(3, true);
    chapter_list->setColumnWidthMode(2, QListView::Manual);
    chapter_list->setColumnWidth(2, 0);
    connect(dvd_folder, SIGNAL(  textChanged (const QString &)), this, SLOT(checkFolder()));
    connect(button_generate, SIGNAL(clicked()), this, SLOT(generateDvdXml()));
    connect(nextButton(), SIGNAL(clicked()), this, SLOT(slotNextPage()));
    connect(button_preview, SIGNAL(clicked()), this, SLOT(previewDvd()));
    connect(button_burn, SIGNAL(clicked()), this, SLOT(burnDvd()));
    connect(render_widget, SIGNAL(dvdExportOver(bool)), this, SLOT(slotFinishExport(bool)));
    checkFolder();
}

ExportDvdDialog::~ExportDvdDialog()
{
    if (m_exportProcess) {
	m_exportProcess->kill();
	delete m_exportProcess;
    }
}


void ExportDvdDialog::slotNextPage() {
    if (indexOf(currentPage()) == 1) {
    
    if (render_now->isChecked()) {
	KURL moviePath = KURLRequesterDlg::getURL(KdenliveSettings::currentdefaultfolder() + "/movie.mpg", this, i18n("Enter name for rendered movie file"));
	if (moviePath.isEmpty()) return;
	m_movie_file = moviePath.path();
	setEnabled(false);
	m_render_widget->generateDvdFile(m_movie_file , timeFromString(chapter_list->firstChild()->text(1)), timeFromString(chapter_list->lastItem()->text(2)));
    }
    else m_movie_file = render_file->url();
    }
    else if (indexOf(currentPage()) == 2) {
	generateMenuXml();
    }



}

void ExportDvdDialog::slotFinishExport(bool isOk) {
    checkFolder();
    if (!isOk) {
	KMessageBox::sorry(this, i18n("The export terminated unexpectedly.\nOutput file will probably be corrupted..."));
	return;
    }
    render_file->setURL(m_movie_file);
    use_existing->setChecked(true);
    setEnabled(true);
}

void ExportDvdDialog::checkFolder() {
    bool isOk = KIO::NetAccess::exists(KURL(dvd_folder->url() + "/VIDEO_TS/"), false, this);
    button_preview->setEnabled(isOk);
    button_burn->setEnabled(isOk);
}

void ExportDvdDialog::fillStructure(QDomDocument xml) {
    QDomElement docElem = xml.documentElement();
    QDomNode node = docElem.firstChild();
    int currentStart = 0;
    int currentChapter = 0;
    chapter_list->clear();
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
		(void) new KListViewItem(chapter_list, i18n("Chapter %1").arg(currentChapter), st, QString::null, element.attribute("comment"));
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

void ExportDvdDialog::burnDvd() {
    if (!KIO::NetAccess::exists(KURL(dvd_folder->url() + "/VIDEO_TS/"), false, this)) {
	KMessageBox::sorry(this, i18n("You need to generate the DVD structure before burning."));
	return;
    }
    KProcess *previewProcess = new KProcess;
    *previewProcess << "k3b";
    *previewProcess << "--videodvd";
    *previewProcess << dvd_folder->url() + "/VIDEO_TS";
    *previewProcess << dvd_folder->url() + "/AUDIO_TS";
/*    QApplication::connect(m_exportProcess, SIGNAL(processExited(KProcess *)), this, SLOT(endExport(KProcess *)));
    QApplication::connect(m_exportProcess, SIGNAL(receivedStderr (KProcess *, char *, int )), this, SLOT(receivedStderr(KProcess *, char *, int)));*/
    previewProcess->start();
}

void ExportDvdDialog::previewDvd() {
    KProcess *previewProcess = new KProcess;
    *previewProcess << "xine";
    *previewProcess << "dvd:/" + dvd_folder->url();
/*    QApplication::connect(m_exportProcess, SIGNAL(processExited(KProcess *)), this, SLOT(endExport(KProcess *)));
    QApplication::connect(m_exportProcess, SIGNAL(receivedStderr (KProcess *, char *, int )), this, SLOT(receivedStderr(KProcess *, char *, int)));*/
    previewProcess->start();
}

void ExportDvdDialog::generateDvdXml() {
    if (KIO::NetAccess::exists(KURL(dvd_folder->url() + "/VIDEO_TS/"), false, this)) {
	if (KMessageBox::questionYesNo(this, i18n("The specified dvd folder already exists.\nOverwite it ?")) != KMessageBox::Yes)
	    return;
	KIO::NetAccess::del(KURL(dvd_folder->url()));
    }
    button_preview->setEnabled(false);
    button_burn->setEnabled(false);
    if (!KIO::NetAccess::exists(KURL(dvd_folder->url()), false, this))
	KIO::NetAccess::mkdir(KURL(dvd_folder->url()), this);

    if (use_existing->isChecked() && render_file->url().isEmpty()) {
	KMessageBox::sorry(this, i18n("You didn't select the video file for the DVD.\n Please choose one before starting any operation."));
	return;
    }
    //button_generate->setEnabled(false);

    Timecode tc;
    QString chapterTimes;
    if (chapter_list->childCount() > 1) {
	chapterTimes = "0";
	
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
    vob.setAttribute("file", m_movie_file);
    if (!chapterTimes.isEmpty()) vob.setAttribute("chapters", chapterTimes);
    if (chapter_pause->value() > 0) vob.setAttribute("pause", chapter_pause->value());
    pgc.appendChild(vob);
    doc.appendChild(main);

    QFile *file = new QFile();
    file->setName(xml_file);
    file->open(IO_WriteOnly);
    QTextStream stream( file );
    stream.setEncoding (QTextStream::UnicodeUTF8);
    stream << doc.toString();
    file->close();
    setCursor(QCursor(Qt::WaitCursor));
    m_exportProcess = new KProcess;
    *m_exportProcess << "dvdauthor";
    *m_exportProcess << "-o";
    *m_exportProcess << dvd_folder->url();
    *m_exportProcess << "-x";
    *m_exportProcess << xml_file;

    connect(m_exportProcess, SIGNAL(processExited(KProcess *)), this, SLOT(endExport(KProcess *)));
    m_exportProcess->start(KProcess::NotifyOnExit, KProcess::AllOutput);
    //if (!render_now->isChecked()) slotFinishExport(true);
    //QApplication::connect(m_exportProcess, SIGNAL(receivedStderr (KProcess *, char *, int )), this, SLOT(receivedStderr(KProcess *, char *, int)));
    //kdDebug()<<"- - - - - - -"<<endl;
    //kdDebug()<<doc.toString()<<endl;
}


void ExportDvdDialog::generateMenuXml() {
    if (create_menu->isChecked()) {
	if (color_background->isChecked()) {
	    QString color = bg_color->color().name();
	    //kdDebug()<<"+ + + EXPORT DVD: "<<color<<endl;
	    color = "colour=\"" + color.replace(0, 1, "0x") + "ff\"";
	    KProcess *p = new KProcess();
	    *p<<"inigo";
	    *p<<"colour";
	    *p<<color.ascii();
	    *p<<"in=0";
	    *p<<"out=100";
	    *p<<"-consumer";
	    *p<<"avformat:" + KdenliveSettings::currentdefaultfolder() + "/tmp/menu.vob";
	    *p<<"vcodec=mpeg2video";
	    *p<<"acodec=ac3";
	    *p<<"format=dvd";
	    *p<<"frame_size=720x576";
	    *p<<"frame_rate=25";
	    *p<<"gop_size=15";
	    *p<<"video_bit_rate=6000000";
	    *p<<"video_rc_max_rate=9000000";
	    *p<<"video_rc_min_rate=0";
	    *p<<"video_rc_buffer_size=1835008";
	    *p<<"mux_packet_size=2048";
	    *p<<"mux_rate=10080000";
	    *p<<"audio_bit_rate=448000";
	    *p<<"audio_sample_rate=48000";
	    connect(p, SIGNAL(processExited(KProcess *)), this, SLOT(movieMenuDone(KProcess *)));
	    setCursor(QCursor(Qt::WaitCursor));
	    p->start();
	}
    }
}

void ExportDvdDialog::movieMenuDone(KProcess *)
{
    setCursor(QCursor(Qt::ArrowCursor));
}

void ExportDvdDialog::endExport(KProcess *)
{
    bool finishedOK = true;
    checkFolder();
//    button_generate->setEnabled(true);
    setCursor(QCursor(Qt::ArrowCursor));
    if (!m_exportProcess->normalExit()) {
	KMessageBox::sorry(this, i18n("The creation of DVD structure failed."));
	return;
    }
    delete m_exportProcess;
    m_exportProcess = 0;
}


GenTime ExportDvdDialog::timeFromString(QString timeString) {
    int frames = (int) ((timeString.section(":",0,0).toInt()*3600 + timeString.section(":",1,1).toInt()*60 + timeString.section(":",2,2).toInt()) * m_fps + timeString.section(":",3,3).toInt());
    return GenTime(frames, m_fps);
}

} // End Gui

