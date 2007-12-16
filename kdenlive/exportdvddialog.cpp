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
#include <qcheckbox.h>
#include <qspinbox.h>
#include <qpixmap.h>
#include <qimage.h>
#include <qpainter.h>
#include <qbitmap.h>
#include <qtextedit.h>

#include <klocale.h>
#include <kiconloader.h>
#include <kfontcombo.h>
#include <klineedit.h>
#include <kcolorbutton.h>
#include <kdebug.h>
#include <klistview.h>
#include <kurlrequester.h>
#include <kmessagebox.h>
#include <kfiledialog.h>
#include <kio/netaccess.h>
#include <kurlrequesterdlg.h> 
#include <kstandarddirs.h>
#include <knotifyclient.h>
#include <kprogress.h>
#include <ktempdir.h>

#include "timecode.h"
#include "kdenlivesettings.h"
#include "exportdvddialog.h"

namespace Gui {

ExportDvdDialog::ExportDvdDialog(DocClipProject *proj, exportWidget *render_widget, formatTemplate format, QWidget * parent, const char *name):ExportDvd_UI(parent, name), m_project(proj), m_exportProcess(0), m_render_widget(render_widget), m_format(format)
{
    if (KStandardDirs::findExe("dvdauthor") == QString::null) KMessageBox::sorry(this, i18n("Cannot find the program \"dvdauthor\" on your system. Install it if you want to be able to create a DVD"));
    dvd_folder->setURL(KdenliveSettings::currentdefaultfolder() + "/dvd/");
    dvd_folder->setMode(KFile::Directory);
    m_fps = m_project->framesPerSecond();
    xml_file = KdenliveSettings::currenttmpfolder() + "/dvdauthor.xml";
    spuxml_file = KdenliveSettings::currenttmpfolder() + "/spumux.xml";
    setCaption(i18n("Create DVD"));
    setTitle(page(0), i18n("Rendering"));
    setTitle(page(1), i18n("Menu"));
    setTitle(page(2), i18n("Burning"));
    setFinishEnabled (page(2), false);
    finishButton ()->hide();
    dvd_size->setTotalSteps(4700);
    dvd_size->setFormat("%v M");

    dvd_standard->insertItem(i18n("PAL"));
    dvd_standard->insertItem(i18n("PAL 16:9"));
    dvd_standard->insertItem(i18n("NTSC"));
    dvd_standard->insertItem(i18n("NTSC 16:9"));

    m_isNTSC = (format.fps() == 30000.0 / 1001.0);

    /*if (m_format == PAL_WIDE) dvd_standard->setCurrentItem(1);
    else if (m_format == NTSC_VIDEO) dvd_standard->setCurrentItem(2);
    else if (m_format == NTSC_WIDE) dvd_standard->setCurrentItem(3);*/

    cancelButton()->setText(i18n("Close"));

    render_file->fileDialog()->setURL(KURL(KdenliveSettings::currentdefaultfolder()));
    render_file->fileDialog()->setFilter("*.vob | DVD vob file");
    chapter_list->setItemsRenameable(true);
    chapter_list->setRenameable(0, false);
    chapter_list->setRenameable(3, true);
    chapter_list->setColumnWidthMode(2, QListView::Manual);
    chapter_list->setColumnWidth(2, 0);

    connect(dvd_folder, SIGNAL(  textChanged (const QString &)), this, SLOT(checkFolder()));
    connect(button_generate, SIGNAL(clicked()), this, SLOT(generateDvd()));
    connect(nextButton(), SIGNAL(clicked()), this, SLOT(slotNextPage()));
    connect(button_preview, SIGNAL(clicked()), this, SLOT(previewDvd()));
    connect(button_burn, SIGNAL(clicked()), this, SLOT(burnDvd()));
    connect(button_qdvd, SIGNAL(clicked()), this, SLOT(openWithQDvdauthor()));

    connect(use_existing, SIGNAL(toggled (bool)), this, SLOT(slotUpdateNextButton()));
    connect(render_file, SIGNAL(textChanged(const QString &)), this, SLOT(slotUpdateNextButton()));

    connect(render_widget, SIGNAL(dvdExportOver(bool)), this, SLOT(slotFinishExport(bool)));
    connect(create_menu, SIGNAL(toggled ( bool )), this, SLOT( refreshPreview()));
    connect(color_background, SIGNAL(toggled ( bool )), this, SLOT( refreshPreview()));
    connect(bg_color, SIGNAL(changed (const QColor &)), this, SLOT( refreshPreview()));
    connect(normal_color, SIGNAL(changed (const QColor &)), this, SLOT( refreshPreview()));
    connect(font_combo, SIGNAL( activated ( int )), this, SLOT( refreshPreview()));
    connect(font_size, SIGNAL( valueChanged ( int )), this, SLOT( refreshPreview()));
    connect(dvd_standard, SIGNAL( activated ( int )), this, SLOT( slotSetStandard(int)));
    connect(render_file, SIGNAL(urlSelected( const QString & )), this, SLOT(slotCheckRendered()));
    connect(menu_image, SIGNAL(urlSelected( const QString & )), this, SLOT(slotCheckMenuImage()));
    connect(menu_movie, SIGNAL(urlSelected( const QString & )), this, SLOT(slotCheckMenuMovie()));
    connect(play_text, SIGNAL(textChanged ( const QString & )), this, SLOT(generateMenuPreview()));

    refreshTimer = new QTimer( this );
    connect( refreshTimer, SIGNAL(timeout()), this, SLOT(generateMenuPreview()) );

    checkFolder();
}

ExportDvdDialog::~ExportDvdDialog()
{
    if (m_exportProcess) {
	m_exportProcess->kill();
	delete m_exportProcess;
    }
    delete refreshTimer;
}

void ExportDvdDialog::slotUpdateNextButton()
{
    if (use_existing->isChecked() && render_file->url().isEmpty()) nextButton()->setEnabled(false);
    else nextButton()->setEnabled(true);
}

void ExportDvdDialog::slotSetStandard(int std)
{
/*
    switch (m_format)
    {
	case 1:
	    m_format = PAL_WIDE;
	    break;
	case 2:
	    m_format = NTSC_VIDEO;
	    break;
	case 3:
	    m_format = NTSC_WIDE;
	    break;
	default:
	    m_format = PAL_VIDEO;
	    break;
    }
*/
}

void ExportDvdDialog::slotCheckMenuMovie()
{
    movie_background->setChecked(true);
    generateMenuPreview();
}

void ExportDvdDialog::slotCheckMenuImage()
{
    image_background->setChecked(true);
    generateMenuPreview();
}

void ExportDvdDialog::slotCheckRendered()
{
    use_existing->setChecked(true);
}


void ExportDvdDialog::refreshPreview()
{
    if (create_menu->isChecked()) {
	generateMenuPreview();
    }
}

void ExportDvdDialog::generateDvd() {
    menu_ok->setPixmap(QPixmap());
    dvd_ok->setPixmap(QPixmap());
    button_generate->setEnabled(false);
    button_burn->setEnabled(false);
    button_preview->setEnabled(false);
    button_qdvd->setEnabled(false);
    dvd_info->setText(i18n("You can now close this dialog and keep on working on your project while the DVD is being created."));
    if (render_now->isChecked()) {
	    main_ok->setPixmap(KGlobal::iconLoader()->loadIcon("1rightarrow", KIcon::Toolbar));
	    m_render_widget->generateDvdFile(m_movie_file , timeFromString(chapter_list->firstChild()->text(1)), timeFromString(chapter_list->lastItem()->text(2)), m_isNTSC);
    }
    else {
	uint dvdsize = 0;
        QFileInfo movieInfo(render_file->url());
        dvdsize = movieInfo.size();
        dvd_size->setProgress(dvdsize / 1024000);
	generateMenuMovie();
    }
}


void ExportDvdDialog::slotNextPage() {

    if (indexOf(currentPage()) == 1) {
        if (use_existing->isChecked()) {
	    main_ok->setPixmap(KGlobal::iconLoader()->loadIcon("button_ok", KIcon::Toolbar));
	    m_movie_file = render_file->url();
	    movie_path->setText(m_movie_file);
	}
        else if (render_now->isChecked()) {
	    KURLRequesterDlg *getUrl = new KURLRequesterDlg(KdenliveSettings::currenttmpfolder() + "/movie.vob", i18n("Enter name for rendered movie file"), this, "dvd_file");
	    getUrl->exec();
	    KURL moviePath = getUrl->selectedURL ();
	    if (moviePath.isEmpty()) {
		showPage(page(0));
		return;
	    }
	    delete getUrl;
	    m_movie_file = moviePath.path();
	    movie_path->setText(m_movie_file);
        }
    }
}

void ExportDvdDialog::slotFinishExport(bool isOk) {
    checkFolder();
    if (!isOk) {
	KMessageBox::sorry(this, i18n("The export terminated unexpectedly.\nOutput file will probably be corrupted..."));
	main_ok->setPixmap(KGlobal::iconLoader()->loadIcon("button_cancel", KIcon::Toolbar));
    	enableButtons();
	return;
    }
    main_ok->setPixmap(KGlobal::iconLoader()->loadIcon("button_ok", KIcon::Toolbar));
    render_file->setURL(m_movie_file);
    use_existing->setChecked(true);
    uint dvdsize = 0;
    QFileInfo movieInfo(render_file->url());
    dvdsize = movieInfo.size();
    dvd_size->setProgress(dvdsize / 1024000);
    generateMenuMovie();
    //setEnabled(true);
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
    if (m_isNTSC) tc.setFormat(30, true);
    else tc.setFormat(25);
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
	//KMessageBox::information(this, i18n("You didn't define any DVD chapters (markers). The complete timeline will be exported as a single chapter"));
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
    if (KStandardDirs::findExe("k3b") == QString::null) {
	KMessageBox::sorry(this, i18n("You need the program \"K3b\" to burn your DVD. Please install it if you want to use this feature."));
	return;
    }
    if (!KIO::NetAccess::exists(KURL(dvd_folder->url() + "/VIDEO_TS/"), false, this)) {
	KMessageBox::sorry(this, i18n("You need to generate the DVD structure before burning."));
	return;
    }
    KProcess *previewProcess = new KProcess;
    *previewProcess << "k3b";
    *previewProcess << "--videodvd";
    *previewProcess << dvd_folder->url() + "/VIDEO_TS";
    *previewProcess << dvd_folder->url() + "/AUDIO_TS";
    previewProcess->start();
    previewProcess->detach();
    delete previewProcess;
}

void ExportDvdDialog::previewDvd() {
    QString player;
    if (KStandardDirs::findExe("kaffeine") != QString::null) player = "kaffeine";
    else if (KStandardDirs::findExe("xine") != QString::null) player = "xine";
    else {
	KMessageBox::sorry(this, i18n("You need the video player \"kaffeine\" or \"xine\" to preview your DVD. Please install one of them if you want to use this feature."));
	return;
    }

    KProcess *previewProcess = new KProcess;
    *previewProcess << player;
    *previewProcess << "dvd:/" + dvd_folder->url();
    previewProcess->start();
    previewProcess->detach();
    delete previewProcess;

}

void ExportDvdDialog::openWithQDvdauthor() {
    if (KStandardDirs::findExe("qdvdauthor") == QString::null) {
	KMessageBox::sorry(this, i18n("You need the program \"QDvdAuthor\" to author your DVD. Please install it if you want to use this feature."));
	return;
    }
    KProcess *previewProcess = new KProcess;
    *previewProcess << "qdvdauthor";
    *previewProcess << "-d";
    *previewProcess << xml_file;
    if (create_menu->isChecked()) {
	*previewProcess << "-s";
	*previewProcess << spuxml_file;
    }
    previewProcess->start();
    previewProcess->detach();
    delete previewProcess;
}

void ExportDvdDialog::generateDvdXml() {
    dvd_ok->setPixmap(KGlobal::iconLoader()->loadIcon("1rightarrow", KIcon::Toolbar));
    if (KIO::NetAccess::exists(KURL(dvd_folder->url() + "/VIDEO_TS/"), false, this)) {
	if (KMessageBox::questionYesNo(this, i18n("The specified dvd folder ( %1 ) already exists.\nOverwite it ?").arg(dvd_folder->url())) != KMessageBox::Yes) {
	    dvd_ok->setPixmap(KGlobal::iconLoader()->loadIcon("button_cancel", KIcon::Toolbar));
	    dvdFailed();
	    return;
	}
	if (!KIO::NetAccess::del(KURL(dvd_folder->url()), this)) 
	    kdDebug()<<"/ / / / / CANNOT REMOVE DVD FOLDER"<<endl;
    }
    button_preview->setEnabled(false);
    button_burn->setEnabled(false);
    if (!KIO::NetAccess::exists(KURL(dvd_folder->url()), false, this))
	KIO::NetAccess::mkdir(KURL(dvd_folder->url()), this);

    if (use_existing->isChecked() && render_file->url().isEmpty()) {
	KMessageBox::sorry(this, i18n("You didn't select the video file for the DVD.\n Please choose one before starting any operation."));
	dvd_ok->setPixmap(KGlobal::iconLoader()->loadIcon("button_cancel", KIcon::Toolbar));
    	enableButtons();
	return;
    }
    //button_generate->setEnabled(false);


    Timecode tc;
    if (m_isNTSC) tc.setFormat(30, true);
    else tc.setFormat(25);
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

    // Generate dvdauthor xml file
    QDomDocument doc;
    QDomElement main = doc.createElement("dvdauthor");
    QDomElement vmgm = doc.createElement("vmgm");
    main.appendChild(vmgm);

    QDomElement intro_pgc = doc.createElement("pgc");

    if (intro_movie->isChecked()) {
        QDomElement fpc = doc.createElement("fpc");
	QDomText intro = doc.createTextNode("{ jump menu 2; }");
	fpc.appendChild(intro);
	vmgm.appendChild(fpc);

	QDomElement menuvob = doc.createElement("vob");	
	menuvob.setAttribute("file", intro_url->url());
	intro_pgc.appendChild(menuvob);

	QDomElement post = doc.createElement("post");
 	QDomText t2 = doc.createTextNode( "jump vmgm menu 1;" );
    	post.appendChild( t2 );
	intro_pgc.appendChild( post );
    }

    QDomElement titleset = doc.createElement("titleset");
    main.appendChild(titleset);

    if (create_menu->isChecked()) {
	// add spumux file
	QDomElement menus = doc.createElement("menus");	
	vmgm.appendChild(menus);
	QDomElement pgc = doc.createElement("pgc");
	pgc.setAttribute("entry", "title");
	menus.appendChild(pgc);

	if (intro_movie->isChecked()) menus.appendChild(intro_pgc);

	QDomElement playButton = doc.createElement ( "button" );
	playButton.setAttribute ( "name", "PlayButton" ); // Hardcoded. Only one button ?
 	QDomText t = doc.createTextNode( "jump title 1;" );
    	playButton.appendChild( t );
	pgc.appendChild(playButton);

	QDomElement menuvob = doc.createElement("vob");	
	menuvob.setAttribute ( "file",  KdenliveSettings::currenttmpfolder() + "menu_final.vob" );
	menuvob.setAttribute ( "pause", movie_background->isChecked ( ) ? "0" : "inf" );
	pgc.appendChild(menuvob);

	if (movie_background->isChecked()) {
	    // Menu with movie background, let's loop the movie
	    QDomElement post = doc.createElement("post");
 	    QDomText t2 = doc.createTextNode( "jump menu 1;" );
    	    post.appendChild( t2 );
	    pgc.appendChild( post );
	}
    }

    QDomElement titles = doc.createElement("titles");
    titleset.appendChild(titles);
    QDomElement pgc = doc.createElement("pgc");
    titles.appendChild(pgc);
    QDomElement vob = doc.createElement("vob");
    vob.setAttribute("file", m_movie_file);
    if (!chapterTimes.isEmpty()) {
	vob.setAttribute("chapters", chapterTimes);
    	if (chapter_pause->value() > 0) vob.setAttribute("pause", chapter_pause->value());
    }
    pgc.appendChild(vob);
    if (create_menu->isChecked()) {
        QDomElement post = doc.createElement("post");
        QDomText t = doc.createTextNode( "call vmgm menu 1;" );
        post.appendChild( t );
        pgc.appendChild(post);
    }
    doc.appendChild(main);

    QFile *file = new QFile(xml_file);
    file->open(IO_WriteOnly);
    QTextStream stream( file );
    stream.setEncoding (QTextStream::UnicodeUTF8);
    stream << doc.toString();
    file->close();
    setCursor(QCursor(Qt::WaitCursor));

    // Execute dvdauthor to create the DVD folders
    m_exportProcess = new KProcess;
    *m_exportProcess << "dvdauthor";
    *m_exportProcess << "-o";
    *m_exportProcess << dvd_folder->url();
    *m_exportProcess << "-x";
    *m_exportProcess << xml_file;

    m_processlog += "\n\nDVDAuthor output\n\n";

    connect(m_exportProcess, SIGNAL(processExited(KProcess *)), this, SLOT(endExport(KProcess *)));
    connect(m_exportProcess, SIGNAL(receivedStderr (KProcess *, char *, int )), this, SLOT(receivedStderr(KProcess *, char *, int)));
    connect(m_exportProcess, SIGNAL(receivedStdout (KProcess *, char *, int )), this, SLOT(receivedStderr(KProcess *, char *, int)));

    disconnect(button_generate, SIGNAL(clicked()), this, SLOT(generateDvd()));
    connect(button_generate, SIGNAL(clicked()), this, SLOT(stopDvd()));
    m_exportProcess->start(KProcess::NotifyOnExit, KProcess::AllOutput);
    button_generate->setText(i18n("Stop"));
    button_generate->setEnabled(true);
    
    //if (!render_now->isChecked()) slotFinishExport(true);
    //QApplication::connect(m_exportProcess, SIGNAL(receivedStderr (KProcess *, char *, int )), this, SLOT(receivedStderr(KProcess *, char *, int)));
    //kdDebug()<<"- - - - - - -"<<endl;
    //kdDebug()<<doc.toString()<<endl;
}

void ExportDvdDialog::stopDvd() {
    m_exportProcess->kill();
}


void ExportDvdDialog::generateMenuMovie() {
    menu_ok->setPixmap(KGlobal::iconLoader()->loadIcon("1rightarrow", KIcon::Toolbar));
    if (create_menu->isChecked()) {

    if (intro_movie->isChecked()) {
	QFileInfo introInfo(intro_url->url());
	uint dvdsize = introInfo.size();
	dvd_size->setProgress(dvd_size->progress() + dvdsize / 1024000);
    }

    if (KStandardDirs::findExe("convert") == QString::null) {
	    KMessageBox::sorry(this, i18n("You need the program \"convert\" which is included in ImageMagick to create DVD menus. Install ImageMagick if you want to make a DVD with menu."));
	    showPage(page(1));
	    create_menu->setChecked(false);
	    menu_ok->setPixmap(KGlobal::iconLoader()->loadIcon("button_cancel", KIcon::Toolbar));
    	    enableButtons();
	    return;
    	}

	generateTranspImage("menu.png", play_text->text(), normal_color->color());
	QString aspect_ratio;
	QString size;
	QString fps;
	QString gop;
	aspect_ratio = m_format.aspect();
	fps = m_format.fps();
	gop = "18";
	if (m_isNTSC) {
	    size = "720x480";
	}
	else {
	    size = "720x576";
	}

	m_menu_movie_file = KdenliveSettings::currenttmpfolder() + "menu.vob";
	KProcess *p = new KProcess;
	*p<<"kdenlive_renderer";

	if (color_background->isChecked()) {
	    QString color = bg_color->color().name();
	    //kdDebug()<<"+ + + EXPORT DVD: "<<color<<endl;
	    color = "colour=\"" + color.replace(0, 1, "0x") + "ff\"";
	    *p<<"colour";
	    *p<<color.ascii();
	    *p<<"in=0";
	    *p<<"out=50";
	    *p<<"-track";
	    *p<<KdenliveSettings::currenttmpfolder() + "menu.png";
	    *p<<"aspect_ratio=" + aspect_ratio;
	    *p<<"in=0";
	    *p<<"out=50";
	    *p<<"-transition";
	    *p<<"composite";
	    *p<<"progressive=1";
	    *p<<"a_track=0";
	    *p<<"b_track=1";
	    *p<<"in=0";
	    *p<<"out=50";

	}
	else if (image_background->isChecked()) {
	    //kdDebug()<<"+ + + EXPORT DVD: "<<color<<endl;
	    *p<<menu_image->url();
	    *p<<"in=0";
	    *p<<"out=50";
	    *p<<"-track";
	    *p<<KdenliveSettings::currenttmpfolder() + "menu.png";
	    *p<<"aspect_ratio=" + aspect_ratio;
	    *p<<"in=0";
	    *p<<"out=50";
	    *p<<"-transition";
	    *p<<"composite";
	    *p<<"progressive=1";
	    *p<<"a_track=0";
	    *p<<"b_track=1";
	    *p<<"in=0";
	    *p<<"out=50";
	}
	else {
	    *p<<menu_movie->url();
	    *p<<"in=0";
	    *p<<"out=100";
	    *p<<"-track";
	    *p<<KdenliveSettings::currenttmpfolder() + "menu.png";
	    *p<<"aspect_ratio=" + aspect_ratio;
	    *p<<"in=0";
	    // TODO: Calculate movie duration
	    *p<<"out=100";
	    *p<<"-transition";
	    *p<<"composite";
	    *p<<"progressive=1";
	    *p<<"a_track=0";
	    *p<<"b_track=1";
	    *p<<"in=0";
	    *p<<"out=100";
	}

	    *p<<"-consumer";
	    *p<<"avformat:" + m_menu_movie_file;
	    *p<<"vcodec=mpeg2video";
	    *p<<"acodec=ac3";
	    *p<<"format=dvd";
	    *p<<"frame_size=" + size;
	    *p<<"frame_rate=" + fps;
	    *p<<"gop_size=" + gop;
	    *p<<"video_bit_rate=6000000";
	    *p<<"video_rc_max_rate=9000000";
	    *p<<"video_rc_min_rate=0";
	    *p<<"video_rc_buffer_size=1835008";
	    *p<<"mux_packet_size=2048";
	    *p<<"mux_rate=10080000";
	    *p<<"audio_bit_rate=448000";
	    *p<<"audio_sample_rate=48000";

	    if (m_isNTSC) *p<<"profile=dv_ntsc";
	    else  *p<<"profile=dv_pal";

	    connect(p, SIGNAL(processExited(KProcess *)), this, SLOT(movieMenuDone(KProcess *)));
	    setCursor(QCursor(Qt::WaitCursor));
	    p->start();
    }
    else {
	menu_ok->setPixmap(KGlobal::iconLoader()->loadIcon("button_ok", KIcon::Toolbar));
	generateDvdXml();
    }
}

void ExportDvdDialog::movieMenuDone(KProcess *p)
{
    if (p->normalExit()) {
	QFileInfo menuInfo(KdenliveSettings::currenttmpfolder() + "menu_final.vob");
	uint dvdsize = menuInfo.size();
	dvd_size->setProgress(dvd_size->progress() + dvdsize / 1024000);
	generateMenuImages();
	delete p;
    }
    else {
	delete p;
	menu_ok->setPixmap(KGlobal::iconLoader()->loadIcon("button_cancel", KIcon::Toolbar));
	dvdFailed();
    }
}

void ExportDvdDialog::spuMenuDone(KProcess *p)
{
    if (!p->normalExit()) {
	menu_ok->setPixmap(KGlobal::iconLoader()->loadIcon("button_cancel", KIcon::Toolbar));
	delete p;
	dvdFailed();
	return;
    }
    menu_ok->setPixmap(KGlobal::iconLoader()->loadIcon("button_ok", KIcon::Toolbar));
    delete p;
    generateDvdXml();
    //button_generate->setEnabled(true);
}

void ExportDvdDialog::resizeEvent ( QResizeEvent * e )
{
    if (title(currentPage()) ==  i18n("Menu")) refreshTimer->start(200, TRUE);;
}


void ExportDvdDialog::generateMenuPreview()
{
	QPainter p;
	int width = 720;
	int height;
	if (m_isNTSC) height = 480;
	else height = 576;

	QPixmap pix(width, height);
	if (color_background->isChecked()) pix.fill(bg_color->color());
	else if (image_background->isChecked()) {
		QImage im(menu_image->url());
		pix = im.smoothScale(width, height);
	}
	else pix.fill(black);
	p.begin(&pix);
	p.setPen(normal_color->color());
	QFont f = p.font();
	f.setFamily(font_combo->currentFont());
	f.setPointSize(font_size->value());
	p.setFont(f);
	p.drawText(0, 0, width, height, Qt::AlignHCenter | Qt::AlignVCenter,play_text->text());
	p.flush();
	p.end();
	QImage im = pix.convertToImage();
	preview_pixmap->setPixmap(im.smoothScale((preview_pixmap->height() - 2) * width / height, preview_pixmap->height() - 2));
}


void ExportDvdDialog::generateMenuImages()
{
    QRect boundingRect = generateImage ( "highlight.png",  play_text->text ( ), select_color->color ( ) );
    generateImage ( "select.png", play_text->text ( ), active_color->color ( ) );

    // Generate spumux xml file
    QDomDocument doc;
    QDomElement main = doc.createElement("subpictures");
    QDomElement stream = doc.createElement("stream");
    main.appendChild(stream);
    QDomElement spu = doc.createElement("spu");
    stream.appendChild(spu);
    spu.setAttribute("start", "0");
    spu.setAttribute("force", "yes");
    spu.setAttribute("transparent", "000000");
    //spu.setAttribute("image", KdenliveSettings::currentdefaultfolder() + "/tmp/menu.png");
    spu.setAttribute("highlight", KdenliveSettings::currenttmpfolder() + "highlight.png");
    spu.setAttribute("select", KdenliveSettings::currenttmpfolder() + "select.png");
    //spu.setAttribute("autooutline", "infer");
    spu.setAttribute("outlinewidth", "20");
    spu.setAttribute("autoorder", "rows");

    //<button y0="152" y1="280" x0="197" name="01_Button_1" x1="356" />
    if ( !boundingRect.isValid ( ) )
	boundingRect = QRect ( 260, 200, 200, 80 );

    QDomElement button = doc.createElement ( "button" );
    button.setAttribute ( "x0",    boundingRect.left   ( ) );
    button.setAttribute ( "x1",    boundingRect.right  ( ) );
    button.setAttribute ( "y0",    boundingRect.top    ( ) );
    button.setAttribute ( "y1",    boundingRect.bottom ( ) );
    button.setAttribute ( "name", "PlayButton" );  
    spu.appendChild ( button );

    doc.appendChild ( main );

    QFile *file = new QFile(spuxml_file);
    file->open(IO_WriteOnly);
    QTextStream textStream( file );
    textStream.setEncoding (QTextStream::UnicodeUTF8);
    textStream << doc.toString();
    file->close();
	
    //spumux menu.xml <menu_temp.vob> menufinal.mpg
    m_processlog = QString("SPUMUX OUTPUT:\n\n");
    KProcess *p = new KProcess();
    p->setUseShell(true);
    *p<<"spumux";
    *p<<spuxml_file;
    *p<<"<";
    *p<<m_menu_movie_file;
    *p<<">";
    *p<<QString(KdenliveSettings::currenttmpfolder() + "menu_final.vob");
    connect(p, SIGNAL(processExited(KProcess *)), this, SLOT(spuMenuDone(KProcess *)));
    connect(p, SIGNAL(receivedStderr (KProcess *, char *, int )), this, SLOT(receivedStderr(KProcess *, char *, int)));
    connect(p, SIGNAL(receivedStdout (KProcess *, char *, int )), this, SLOT(receivedStderr(KProcess *, char *, int)));
    p->start(KProcess::NotifyOnExit, KProcess::AllOutput);
}


void ExportDvdDialog::receivedStderr(KProcess *, char *buffer, int len)
{
	QCString res(buffer, len);
	m_processlog += res;
	if (show_log->isChecked()) dvd_info->setText(m_processlog);
}

QRect ExportDvdDialog::generateImage(QString imageName, QString buttonText, QColor color)
{
	QPainter p;
	QRect boundingRect;
	int width = 720;
	int height;
	if (m_isNTSC) height = 480;
	else height = 576;

	QPixmap pix(width, height);
	//QBitmap mask(KdenliveSettings::defaultwidth(), KdenliveSettings::defaultheight());
	//mask.fill(color0);
	pix.fill(black);
	p.begin(&pix);
	p.setPen(color);
	QFont f = p.font();
	f.setFamily(font_combo->currentFont());
	f.setPointSize(font_size->value());
	p.setFont(f);
	p.drawText(0, 0, width, height, Qt::AlignHCenter | Qt::AlignVCenter, buttonText, -1, &boundingRect );
	p.flush();
	p.end();
/*
	p.begin(&mask);
	p.setPen(color);
	f = p.font();
	f.setFamily(font_combo->currentFont());
	f.setPointSize(font_size->value());
	p.setFont(f);
	p.drawText(0, 0, KdenliveSettings::defaultwidth(), KdenliveSettings::defaultheight(), Qt::AlignHCenter | Qt::AlignVCenter, buttonText);
	p.flush();
	p.end();
	pix.setMask(mask);*/
	QImage im;
 	im = pix.convertToImage();
	im.setNumColors(1);
	//im.convertDepth(1);
	im.save(KdenliveSettings::currenttmpfolder() + "tmp_" + imageName, "PNG");
	KProcess conv;
    	conv<<"convert";
    	conv<<KdenliveSettings::currenttmpfolder() + "tmp_" + imageName;
	conv<<"-colors";
	conv<<"2";
	conv<<KdenliveSettings::currenttmpfolder() + imageName;
	conv.start(KProcess::Block);
	
	return boundingRect;
}

void ExportDvdDialog::generateTranspImage(QString imageName, QString buttonText, QColor color)
{
	QPainter p;
	int width = 720;
	int height;
	if (m_isNTSC) height = 480;
	else height = 576;

	QPixmap pix(width, height);
	QBitmap mask(width, height);
	mask.fill(color0);
	p.begin(&pix);
	p.setPen(color);
	QFont f = p.font();
	f.setFamily(font_combo->currentFont());
	f.setPointSize(font_size->value());
	p.setFont(f);
	p.drawText(0, 0, width, height, Qt::AlignHCenter | Qt::AlignVCenter, buttonText);
	p.flush();
	p.end();

	p.begin(&mask);
	p.setPen(color);
	f = p.font();
	f.setFamily(font_combo->currentFont());
	f.setPointSize(font_size->value());
	p.setFont(f);
	p.drawText(0, 0, width, height, Qt::AlignHCenter | Qt::AlignVCenter, buttonText);
	p.flush();
	p.end();
	pix.setMask(mask);

	QImage im;
 	im = pix.convertToImage();
	im.save(KdenliveSettings::currenttmpfolder() + imageName, "PNG");
}

void ExportDvdDialog::endExport(KProcess *)
{
    bool finishedOK = true;
    checkFolder();
    button_generate->setText(i18n("Generate DVD"));
    
    disconnect(button_generate, SIGNAL(clicked()), this, SLOT(stopDvd()));
    connect(button_generate, SIGNAL(clicked()), this, SLOT(generateDvd()));
    enableButtons();

    setCursor(QCursor(Qt::ArrowCursor));
    if (!m_exportProcess->normalExit()) {
	dvd_ok->setPixmap(KGlobal::iconLoader()->loadIcon("button_cancel", KIcon::Toolbar));
	dvdFailed();
	return;
    }
    dvd_ok->setPixmap(KGlobal::iconLoader()->loadIcon("button_ok", KIcon::Toolbar));
    KNotifyClient::event(winId(), "DvdOk", i18n("The creation of DVD structure is finished."));
    delete m_exportProcess;
    m_exportProcess = 0;
}

void ExportDvdDialog::dvdFailed()
{
    setCursor(QCursor(Qt::ArrowCursor));
    KNotifyClient::event(winId(), "DvdError", i18n("The creation of DVD structure failed."));
    //KMessageBox::sorry(this, );
    enableButtons();
    
}

void ExportDvdDialog::enableButtons() {
    button_generate->setEnabled(true);
    button_burn->setEnabled(true);
    button_preview->setEnabled(true);
    button_qdvd->setEnabled(true);
}

GenTime ExportDvdDialog::timeFromString(QString timeString) {
    Timecode tc;
    if (m_isNTSC) tc.setFormat(30, true);
    else tc.setFormat(25);
    int frames = tc.getFrameNumber(timeString, m_fps);
    return GenTime(frames, m_fps);
}

} // End Gui

