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
#include <kio/netaccess.h>

#include "gentime.h"
#include "exportwidget.h"


exportWidget::exportWidget( Gui::KTimeLine *timeline, QWidget* parent, const char* name):
                exportBaseWidget_UI(parent,name), m_duration(0)
{
/*    m_node = -1;
    m_port = -1;
    m_guid = 0;
    m_avc = 0;*/
    
    initEncoders();
    m_isRunning = false;
    m_startTime = GenTime(0);
    m_endTime = timeline->projectLength();
    m_startSelection = timeline->inpointPosition();
    m_endSelection = timeline->outpointPosition();
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
    fileExportFolder->setURL("~");
    encoders->insertItem("dv");
    encodersList["dv"] << "extension=dv"<<"bypass=true";

    encoders->insertItem("theora");
    encodersList["theora"] << "extension=ogg"<<"bypass=true";
    convertProgress->hide();
    convert_label->hide();
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
        int i = 1;
        while ( !stream.atEnd() ) {
            line = stream.readLine(); // line of text excluding '\n'
            if (line.startsWith("## ")) encodersList[fName].append(line.section(" ",1));
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
        convertProgress->hide();
    }
    else {
        container->setEnabled(true);
        convertProgress->hide();
    }
}

void exportWidget::stopExport()
{
    emit stopTimeLineExport();
}

void exportWidget::startExport()
{
    if (tabWidget->currentPageIndex () == 0) { // export to file
        if (fileExportName->text().isEmpty()) {
            KMessageBox::sorry(this, i18n("Please enter a file name"));
            return;
        }
        processProgress->setProgress(0);
        if (m_isRunning) {
            stopExport();
            return;
        }
        
        if (KIO::NetAccess::exists(KURL(fileExportFolder->url()+"/"+fileExportName->text()), false, this))
            if (KMessageBox::questionYesNo(this, i18n("File already exists.\nDo you want to overwrite it ?")) ==  KMessageBox::No) return;
        
        if (export_selected->isChecked()) {
            startExportTime = m_startSelection;
            endExportTime = m_endSelection;
        }
        else {
            startExportTime = m_startTime;
            endExportTime = m_endTime;
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
            emit exportTimeLine(fileExportFolder->url()+"/"+fileExportName->text(), encoders->currentText(), startExportTime, endExportTime, params);
        }
        else {
            // Libdv export
	    if (encoders->currentText() == "theora") 
	    	emit exportTimeLine(fileExportFolder->url()+"/"+fileExportName->text() + ".dv", "dv", startExportTime, endExportTime, "");
            else emit exportTimeLine(fileExportFolder->url()+"/"+fileExportName->text(), encoders->currentText(), startExportTime, endExportTime, "");
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

void exportWidget::reportProgress(GenTime progress)
{
    int prog = (100 * progress.frames(25))/m_duration.frames(25);
    processProgress->setProgress(prog);
}

void exportWidget::endExport()
{
    exportButton->setText(i18n("Export"));
    m_isRunning = false;
    if (encoders->currentText() == "theora") {
	exportFileToTheora(KURL(fileExportFolder->url()+"/"+fileExportName->text() + ".dv").path(), vquality->currentText().toInt(), aquality->currentText().toInt(), videoSize->currentText());
    }
    else {
    	processProgress->setProgress(0);
    	tabWidget->page(0)->setEnabled(true);
    	if (autoPlay->isChecked ()) {
	        KRun *run=new KRun(KURL(fileExportFolder->url()+"/"+fileExportName->text()));
    	}
    }
}

void exportWidget::exportFileToTheora(QString srcFileName, int video, int audio, QString size)
{
    QString dstFileName = srcFileName.left(srcFileName.findRev("."));
    QString command = "ffmpeg2theora " + srcFileName + " -a "+QString::number(audio)+" -v "+QString::number(video) +" -f dv -x "+size.section("x", 0, 0)+" -y "+size.section("x", 1, 1) + " -o " + dstFileName;
    FILE *fp;
    char line[130];
    command +=" 2>&1";
    fp = popen( command.ascii(), "r"); 
    if ( !fp ) {
        kdDebug()<<"Could not execute "<< command <<endl; 
            return;
    }
    else while ( fgets( line, sizeof line, fp)) {
	qApp->processEvents();
	kdDebug() << "******* THEORA : "<< QString(line).stripWhiteSpace().section(" ",0,0) <<endl;
    }
    kdDebug() << "******* FINISHED : "<< endl;
    pclose(fp);
	
    // remove temporary dv file
    KIO::NetAccess::del(KURL(srcFileName), this);

    processProgress->setProgress(0);
    tabWidget->page(0)->setEnabled(true);
    if (autoPlay->isChecked ()) {
	KRun *run=new KRun(KURL(fileExportFolder->url()+"/"+fileExportName->text()));
    }
    
}

