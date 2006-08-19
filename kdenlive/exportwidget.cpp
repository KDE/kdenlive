/***************************************************************************
                          exportwidget  -  description
                             -------------------
    begin                : Tue Nov 15 2005
    copyright            : (C) 2005 by Jason Wood
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

#include <qtabwidget.h>
#include <qpushbutton.h>
#include <qradiobutton.h>
#include <qslider.h>
#include <qspinbox.h>
#include <qapplication.h>
#include <qcheckbox.h>
#include <qlabel.h>

#include <kurlrequester.h>
#include <kcombobox.h>
#include <kprogress.h>
#include <klocale.h>
#include <krun.h>
#include <kservice.h>
#include <kuserprofile.h>
#include <kmimetype.h>
#include <kdebug.h>
#include <kmessagebox.h>
#include <kglobal.h>
#include <kstandarddirs.h>
#include <klineedit.h>
#include <kfiledialog.h>
#include <kiconloader.h>
#include <kpassivepopup.h>
#include <kio/netaccess.h>

#include "kdenlive.h"
#include "gentime.h"
#include "exportwidget.h"
#include "kdenlivesettings.h"

exportWidget::exportWidget(Gui::KMMScreen *screen, Gui::KTimeLine *timeline, QWidget* parent, const char* name): exportBaseWidget_UI(parent,name), m_duration(0), m_exportProcess(NULL), m_convertProcess(NULL), m_screen(screen), m_timeline(timeline), m_tmpFile(NULL)
{
/*    m_node = -1;
    m_port = -1;
    m_guid = 0;
    m_avc = 0;*/
    
    initEncoders();
    m_isRunning = false;
    fileExportFolder->setMode(KFile::Directory);
    fileExportFolder->fileDialog()->setOperationMode(KFileDialog::Saving);
    
#ifdef ENABLE_FIREWIRE
    tabWidget->page(1)->setEnabled(true);
#else
    tabWidget->page(1)->setEnabled(false);
#endif

    initDvConnection();
    connect(check_size, SIGNAL(toggled(bool)), videoSize, SLOT(setEnabled(bool)));
    connect(check_vbitrate, SIGNAL(toggled(bool)), videoBitrate, SLOT(setEnabled(bool)));
    connect(check_fps, SIGNAL(toggled(bool)), fps, SLOT(setEnabled(bool)));
    connect(check_abitrate, SIGNAL(toggled(bool)), audioBitrate, SLOT(setEnabled(bool)));
    connect(check_freq, SIGNAL(toggled(bool)), frequency, SLOT(setEnabled(bool)));
//    connect(check_fps, SIGNAL(toggled(bool)), fps, SLOT(setEnabled(bool)));
    connect(exportButton,SIGNAL(clicked()),this,SLOT(startExport()));
    connect(encoders,SIGNAL(activated(int)),this,SLOT(slotAdjustWidgets(int)));
}

exportWidget::~exportWidget()
{}

void exportWidget::initDvConnection()
{
#ifdef ENABLE_FIREWIRE

    
    /*m_node = discoverAVC( &m_port, m_guid );

    if ( m_node == -1 ) {
        tabWidget->page(2)->setEnabled(false);
        firewire_status->setText(i18n("No Firewire device detected"));
        return;
}*/
    tabWidget->page(1)->setEnabled(true);
    
    /*
    if ( m_port != -1 )
    {
        AVC::CheckConsistency( m_port, m_node );
        m_avc = new AVC( m_port );
        if ( ! m_avc ) {
            firewire_status->setText(i18n("failed to initialize AV/C on port %1").arg(m_port));
            return;
    }
        
        firewire_status->setText(i18n("Found device on port %1").arg(m_port));
        firewireport->setValue(m_port);
    }
    else firewire_status->setText(i18n("Cannot communicate on port %1").arg(m_port));
    */
#else
    firewire_status->setText(i18n("Firewire support not enabled. Please install the required libraries..."));
#endif
}


void exportWidget::initEncoders()
{
    fileExportName->setText("untitled.dv");
    fileExportFolder->setURL(KdenliveSettings::currentdefaultfolder());
    encoders->insertItem("dv");
    encodersList["dv"] << "extension=dv"<<"bypass=true";

    encoders->insertItem("theora");
    encodersList["theora"] << "extension=ogg"<<"bypass=true";

    //container->setEnabled(false);


    // Find all profiles and add them to the list
    
    QStringList profilesList = KGlobal::dirs()->KStandardDirs::findAllResources("data", "kdenlive/profiles/*.profile");
    for ( QStringList::Iterator it = profilesList.begin(); it != profilesList.end(); ++it ) {
        //create list of encoders and associate extensions
        QString fileName = KURL(*it).fileName().section(".",0,0);
        parseFileForParameters(fileName);
        encoders->insertItem(fileName);
    }
    
    slotAdjustWidgets(0);
}

void exportWidget::parseFileForParameters(const QString & fName)
{
    QString fullName = locate("data", "kdenlive/profiles/"+fName+".profile");
    QFile file(fullName);
    QString line;
    if ( file.open( IO_ReadOnly ) ) {
        QTextStream stream( &file );
        //int i = 1;
        while ( !stream.atEnd() ) {
            line = stream.readLine(); // line of text excluding '\n'
            if (line.startsWith("## ")) encodersList[fName].append(line.section(" ",1));
	    else if (line.startsWith("### ")) encodersFixedList[fName].append(line.section(" ",1));
        }
        file.close();
    }
}

QString exportWidget::profileParameter(const QString & profile, const QString &param)
{
    QStringList params = encodersList[profile];
    for ( QStringList::Iterator it = params.begin(); it != params.end(); ++it ) {
        if ((*it).startsWith(param)) return (*it).section("=",1);
    }
    return QString::null;
}

void exportWidget::slotAdjustWidgets(int pos)
{
    if (!fileExportName->text().isEmpty()) {
        QString currentName=fileExportName->text();
        int i = currentName.findRev(".");
        if (i!=-1) currentName = currentName.left(i);
        QString extension = profileParameter(encoders->currentText(), "extension");
        fileExportName->setText(currentName+"." + extension);
                //encodersList[encoders->currentText()]);
    }
    
    videoBitrate->clear();
    audioBitrate->clear();
    frequency->clear();
    videoSize->clear();
    
    // display all profile parameters
    if (profileParameter(encoders->currentText(), "video_bit_rate").isEmpty()) {
        check_vbitrate->setEnabled(false);
        check_vbitrate->setChecked(false);
        check_vbitrate->hide();
        videoBitrate->hide();
    }
    else {
        check_vbitrate->setEnabled(true);
        check_vbitrate->setChecked(false);
        QStringList params = QStringList::split(",",profileParameter(encoders->currentText(), "video_bit_rate").section(" ",0,0));
        videoBitrate->insertStringList(params);
        videoBitrate->setCurrentText(profileParameter(encoders->currentText(), "video_bit_rate").section(" ",1,1));
        check_vbitrate->show();
        videoBitrate->show();
    }
    
    if (profileParameter(encoders->currentText(), "frame_rate_num").isEmpty()) {
        check_fps->setEnabled(false);
        check_fps->setChecked(false);
        check_fps->hide();
        fps->hide();
    }
    else {
        check_fps->setEnabled(true);
        check_fps->setChecked(false);
        QStringList params = QStringList::split(",",profileParameter(encoders->currentText(), "frame_rate_num").section(" ",0,0));
        fps->insertStringList(params);
        fps->setCurrentText(profileParameter(encoders->currentText(), "frame_rate_num").section(" ",1,1));
        check_fps->show();
        fps->show();
    }
    
    if (profileParameter(encoders->currentText(), "audio_bit_rate").isEmpty()) {
        check_abitrate->setEnabled(false);
        check_abitrate->setChecked(false);
        check_abitrate->hide();
        audioBitrate->hide();
    }
    else {
        check_abitrate->setEnabled(true);
        check_abitrate->setChecked(false);
        QStringList params = QStringList::split(",",profileParameter(encoders->currentText(), "audio_bit_rate").section(" ",0,0));
        audioBitrate->insertStringList(params);
        audioBitrate->setCurrentText(profileParameter(encoders->currentText(), "audio_bit_rate").section(" ",1,1));
        check_abitrate->show();
        audioBitrate->show();
    }
    
    if (profileParameter(encoders->currentText(), "frequency").isEmpty()) {
        check_freq->setEnabled(false);
        check_freq->setChecked(false);
        check_freq->hide();
        frequency->hide();
    }
    else {
        check_freq->setEnabled(true);
        check_freq->setChecked(false);
        QStringList params = QStringList::split(",",profileParameter(encoders->currentText(), "frequency").section(" ",0,0));
        frequency->insertStringList(params);
        frequency->setCurrentText(profileParameter(encoders->currentText(), "frequency").section(" ",1,1));
        check_freq->show();
        frequency->show();
    }
    
    if (profileParameter(encoders->currentText(), "size").isEmpty()) {
        check_size->setEnabled(false);
        check_size->setChecked(false);
        check_size->hide();
        videoSize->hide();
    }
    else {
        check_size->setEnabled(true);
        check_size->setChecked(false);
        QStringList params = QStringList::split(",",profileParameter(encoders->currentText(), "size").section(" ",0,0));
        videoSize->insertStringList(params);
        videoSize->setCurrentText(profileParameter(encoders->currentText(), "size").section(" ",1,1));
        check_size->show();
        videoSize->show();
    }

    if (!profileParameter(encoders->currentText(), "aquality").isEmpty()) {
    	check_aquality->setEnabled(true);
	aquality->setEnabled(true);
	QStringList params = QStringList::split(",",profileParameter(encoders->currentText(), "aquality").section(" ",0,0));
        aquality->insertStringList(params);
        aquality->setCurrentText(profileParameter(encoders->currentText(), "aquality").section(" ",1,1));
        aquality->show();
        check_aquality->show();
    }
    else {
	check_aquality->setEnabled(false);
	check_aquality->setChecked(false);
        aquality->hide();
        check_aquality->hide();
    }

    if (!profileParameter(encoders->currentText(), "vquality").isEmpty()) {
    	check_vquality->setEnabled(true);
	vquality->setEnabled(true);
	QStringList params = QStringList::split(",",profileParameter(encoders->currentText(), "vquality").section(" ",0,0));
        vquality->insertStringList(params);
        vquality->setCurrentText(profileParameter(encoders->currentText(), "vquality").section(" ",1,1));
        vquality->show();
        check_vquality->show();
    }
    else {
	check_vquality->setEnabled(false);
	check_vquality->setChecked(false);
        vquality->hide();
        check_vquality->hide();
    }
        
    if (pos==0) {
        container->setEnabled(false);
    }
    else {
        container->setEnabled(true);
    }
}

void exportWidget::stopExport()
{
    if (m_exportProcess) {
	m_exportProcess->kill();
    }
    if (m_convertProcess) {
	m_convertProcess->kill();
    }
    //emit stopTimeLineExport();
}

void exportWidget::startExport()
{
    if (tabWidget->currentPageIndex () == 0) { // export to file
        if (fileExportName->text().isEmpty()) {
            KMessageBox::sorry(this, i18n("Please enter a file name"));
            return;
        }
        //processProgress->setProgress(0);
        if (m_isRunning) {
            stopExport();
            return;
        }
        
        if (KIO::NetAccess::exists(KURL(fileExportFolder->url()+"/"+fileExportName->text()), false, this))
            if (KMessageBox::questionYesNo(this, i18n("File already exists.\nDo you want to overwrite it ?")) ==  KMessageBox::No) return;
        
        if (export_selected->isChecked()) {
            startExportTime = m_timeline->inpointPosition();
            endExportTime = m_timeline->outpointPosition();
        }
        else {
            startExportTime = GenTime(0);
            endExportTime = m_timeline->projectLength();
        }
        m_duration = endExportTime - startExportTime;
        exportButton->setText(i18n("Stop"));
        m_isRunning = true;
        if (profileParameter(encoders->currentText(), "bypass") != "true") {
            // AVformat (FFmpeg) export, build parameters
            QStringList params;
            if (!videoSize->currentText().isEmpty() && videoSize->isEnabled()) params.append("size="+videoSize->currentText());
            if (!videoBitrate->currentText().isEmpty() && videoBitrate->isEnabled()) params.append("video_bit_rate="+videoBitrate->currentText());
            if (!fps->currentText().isEmpty() && fps->isEnabled()) params.append("frame_rate_num="+fps->currentText());
            if (!audioBitrate->currentText().isEmpty() && audioBitrate->isEnabled()) params.append("audio_bit_rate="+audioBitrate->currentText());
            if (!frequency->currentText().isEmpty() && frequency->isEnabled()) params.append("frequency="+frequency->currentText());

	    QStringList fixedParams = encodersFixedList[encoders->currentText()];
	    params += fixedParams;
	    kdDebug()<<"-- PARAMS: "<<params.join(";")<<endl; 
	    doExport(fileExportFolder->url()+"/"+fileExportName->text(), params);
            //emit exportTimeLine(fileExportFolder->url()+"/"+fileExportName->text(), encoders->currentText(), startExportTime, endExportTime, params);
        }
        else {
            // Libdv export
	    if (encoders->currentText() == "theora") 
		doExport(fileExportFolder->url()+"/"+fileExportName->text()+".dv", QStringList(), true);
	    	//emit exportTimeLine(fileExportFolder->url()+"/"+fileExportName->text() + ".dv", "dv", startExportTime, endExportTime, "");
            else doExport(fileExportFolder->url()+"/"+fileExportName->text(), QStringList(), true);
		//emit exportTimeLine(fileExportFolder->url()+"/"+fileExportName->text(), encoders->currentText(), startExportTime, endExportTime, "");
        }
        tabWidget->page(0)->setEnabled(false);
    }
    else if (tabWidget->currentPageIndex () == 2) { // Firewire export
        kdDebug()<<"++++++++++++++ FIREWIRE EXPORT"<<endl;
        if ( fw_timelinebutton->isChecked()) { // export timeline
        }
        else { // export existing file
            if (fw_URL->url().isEmpty()) {
                KMessageBox::sorry(this, i18n("Please enter a file name"));
                return;
            }
            emit exportToFirewire(fw_URL->url(), firewireport->value(), startExportTime, endExportTime);
        }
    }
}

void exportWidget::doExport(QString file, QStringList params, bool isDv)
{
    if (m_tmpFile) delete m_tmpFile;
    m_tmpFile = new KTempFile( QString::null, ".westley");
    m_progress = 0;
    if (m_exportProcess) {
    	m_exportProcess->kill();
    	delete m_exportProcess;
    }
    kdDebug()<<"++++++  PREPARE TO WRITE TO: "<<m_tmpFile->name()<<endl;
    //QFile file = tmp.file();
    //if ( tmp.file()->open( IO_WriteOnly ) ) {
        QTextStream stream( m_tmpFile->file() );
        stream << m_screen->sceneList().toString() << "\n";
        m_tmpFile->file()->close();

    m_exportProcess = new KProcess;
    *m_exportProcess << "inigo";
    *m_exportProcess << m_tmpFile->name();
    *m_exportProcess << "real_time=0";
    *m_exportProcess << QString("in=%1").arg(startExportTime.frames(KdenliveSettings::defaultfps()));
    *m_exportProcess << QString("out=%1").arg(endExportTime.frames(KdenliveSettings::defaultfps()));
    *m_exportProcess << "-consumer";
    if (isDv) {
	*m_exportProcess << QString("libdv:%1").arg(file);
	*m_exportProcess << "terminate_on_pause=1";
    }
    else *m_exportProcess << QString("avformat:%1").arg(file);
    *m_exportProcess << params;
    QApplication::connect(m_exportProcess, SIGNAL(processExited(KProcess *)), this, SLOT(endExport(KProcess *)));
    QApplication::connect(m_exportProcess, SIGNAL(receivedStderr (KProcess *, char *, int )), this, SLOT(receivedStderr(KProcess *, char *, int)));
    m_exportProcess->start(KProcess::NotifyOnExit, KProcess::AllOutput);
    //tmp.setAutoDelete(true);
}

void exportWidget::receivedStderr(KProcess *, char *buffer, int )
{
	QString result = QString(buffer);
	result = result.simplifyWhiteSpace();
	result = result.section(" ", -1);
	int progress = result.toInt();
	if (progress > 0 && progress > m_progress) {
		m_progress = progress;
		QApplication::postEvent(qApp->mainWidget(), new ProgressEvent((int) (100.0 * progress / m_duration.frames(KdenliveSettings::defaultfps())), 10007));
	}
}

void exportWidget::receivedConvertStderr(KProcess *, char *buffer, int )
{
	QString result = QString(buffer);
	result = result.simplifyWhiteSpace();
	result = result.section(" ", 0, 0);
	int hours = result.section(":", 0, 0).toInt();
	int minutes = result.section(":", 1, 1).toInt();
	int seconds = result.section(":", 2, 2).section(".", 0, 0).toInt();
	int milliseconds = result.section(":", 2, 2).section(".", 1, 1).toInt();
	int progress = hours * 3600 * KdenliveSettings::defaultfps() + minutes * 60 * KdenliveSettings::defaultfps() + seconds * KdenliveSettings::defaultfps() + milliseconds * KdenliveSettings::defaultfps() / 100.0;
	//kdDebug()<<"++ THEORA: "<<result<<", FRAMES: "<<progress<<", DURATION: "<<m_duration.frames(KdenliveSettings::defaultfps())<<endl;

	if (progress > 0 && progress > m_progress) {
		m_progress = progress;
		QApplication::postEvent(qApp->mainWidget(), new ProgressEvent((int) (100.0 * progress / m_duration.frames(KdenliveSettings::defaultfps())), 10007));
	}
}

void exportWidget::reportProgress(GenTime progress)
{
    int prog = (int)((100 * progress.frames(KdenliveSettings::defaultfps()))/m_duration.frames(KdenliveSettings::defaultfps()));
    //processProgress->setProgress(prog);
}

void exportWidget::endExport(KProcess *)
{
    bool finishedOK = true;
    bool twoPassEncoding = false;
    m_tmpFile->unlink();
    delete m_tmpFile;
    m_tmpFile = 0;
    if (encoders->currentText() == "theora") twoPassEncoding = true; 

    if (!m_exportProcess->normalExit()) {
	KMessageBox::sorry(this, i18n("The export terminated unexpectedly.\nOutput file will probably be corrupted..."));
	finishedOK = false;
    }
    else if (!twoPassEncoding) {
	QPixmap px(KGlobal::iconLoader()->loadIcon("kdenlive", KIcon::Toolbar));
	KPassivePopup::message( "Kdenlive", i18n("Export of %1 is finished").arg(fileExportName->text()), px, (QWidget*) parent() );
    }
    delete m_exportProcess;
    m_exportProcess = 0;

    if (encoders->currentText() == "theora") {
	QApplication::postEvent(qApp->mainWidget(), new ProgressEvent(0, 10007));
	exportFileToTheora(KURL(fileExportFolder->url()+"/"+fileExportName->text() + ".dv").path(), vquality->currentText().toInt(), aquality->currentText().toInt(), videoSize->currentText());
    }
    else {
	exportButton->setText(i18n("Export"));
    	m_isRunning = false;
	QApplication::postEvent(qApp->mainWidget(), new ProgressEvent(0, 10007));
    	//processProgress->setProgress(0);
    	tabWidget->page(0)->setEnabled(true);
    	if (autoPlay->isChecked() && finishedOK) {
	        (void) new KRun(KURL(fileExportFolder->url()+"/"+fileExportName->text()));
    	}
    }
}

void exportWidget::endConvert(KProcess *)
{
    bool finishedOK = true;
    if (!m_convertProcess->normalExit()) {
	KMessageBox::sorry(this, i18n("The conversion terminated unexpectedly.\nOutput file will probably be corrupted..."));
	finishedOK = false;
    }
    delete m_convertProcess;
    m_convertProcess = 0;
    exportButton->setText(i18n("Export"));
    KIO::NetAccess::del(KURL(fileExportFolder->url()+"/"+fileExportName->text() + ".dv"), this);
    m_isRunning = false;
    QApplication::postEvent(qApp->mainWidget(), new ProgressEvent(0, 10007));
    //processProgress->setProgress(0);
    tabWidget->page(0)->setEnabled(true);
    if (autoPlay->isChecked() && finishedOK) {
	(void) new KRun(KURL(fileExportFolder->url()+"/"+fileExportName->text()));
    }

}

void exportWidget::endExport()
{
    exportButton->setText(i18n("Export"));
    m_isRunning = false;
    if (encoders->currentText() == "theora") {
	exportFileToTheora(KURL(fileExportFolder->url()+"/"+fileExportName->text() + ".dv").path(), vquality->currentText().toInt(), aquality->currentText().toInt(), videoSize->currentText());
    }
    else {
    	//processProgress->setProgress(0);
    	tabWidget->page(0)->setEnabled(true);
    	if (autoPlay->isChecked ()) {
	        //KRun *run=new KRun(KURL(fileExportFolder->url()+"/"+fileExportName->text()));
    	}
    }
}

void exportWidget::exportFileToTheora(QString srcFileName, int video, int audio, QString size)
{
    if (m_convertProcess) {
    	m_convertProcess->kill();
    	delete m_convertProcess;
    }
    m_progress = 0;
    QString dstFileName = srcFileName.left(srcFileName.findRev("."));

    m_convertProcess = new KProcess;
    *m_convertProcess << "ffmpeg2theora";
    *m_convertProcess << srcFileName;
    *m_convertProcess << "-a";
    *m_convertProcess << QString::number(audio);
    *m_convertProcess << "-v";
    *m_convertProcess << QString::number(video);
    *m_convertProcess << "-f";
    *m_convertProcess << "dv";
    *m_convertProcess << "-x";
    *m_convertProcess << size.section("x", 0, 0);
    *m_convertProcess << "-y";
    *m_convertProcess << size.section("x", 1, 1);
    *m_convertProcess << "-o";
    *m_convertProcess << dstFileName;

    QApplication::connect(m_convertProcess, SIGNAL(processExited(KProcess *)), this, SLOT(endConvert(KProcess *)));
    QApplication::connect(m_convertProcess, SIGNAL(receivedStderr (KProcess *, char *, int )), this, SLOT(receivedConvertStderr(KProcess *, char *, int)));
    m_convertProcess->start(KProcess::NotifyOnExit, KProcess::AllOutput);
}

