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

#include "exportwidget.h"


exportWidget::exportWidget( const GenTime &duration, QWidget* parent, const char* name, WFlags fl ):
                exportBaseWidget_UI(parent,name), m_duration(duration)
{
/*    m_node = -1;
    m_port = -1;
    m_guid = 0;
    m_avc = 0;*/
    
    initEncoders();
    m_isRunning = false;
    
    tabWidget->page(1)->setEnabled(false);
#ifdef ENABLE_FIREWIRE
    tabWidget->page(2)->setEnabled(true);
#else
    tabWidget->page(2)->setEnabled(false);
#endif

    initDvConnection();
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
    tabWidget->page(2)->setEnabled(true);
    
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
    encoders->insertItem(i18n("dv"));
    //container->hide();
    convertProgress->hide();
    convert_label->hide();
    container->setEnabled(false);
    //flayout = 0;
    encoders->insertItem(i18n("mpeg"));
    encoders->insertItem(i18n("westley"));
}

void exportWidget::slotAdjustWidgets(int pos)
{
    if (pos==1) {
        container->setEnabled(true);
        convertProgress->hide();
    }
    else {
        container->setEnabled(false);
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
        if (fileExportUrl->url().isEmpty()) {
            KMessageBox::sorry(this, i18n("Please enter a file name"));
            return;
        }
        processProgress->setProgress(0);
        if (m_isRunning) {
            stopExport();
            return;
        }
        //exportButton->setEnabled(false);
        exportButton->setText(i18n("Stop"));
        m_isRunning = true;
        QString format;
        switch (encoders->currentItem()) {
            case 0: 
                format = "dv";
                break;
            case 1:
                format = "mpeg";
                break;
            case 2:
                format = "westley";
                break;
            default:
                format = "dv";
        }
        if (format == "mpeg") {
            QString vsize = videoSize->currentText();
            emit exportTimeLine(fileExportUrl->url(), format, vsize, GenTime(0), m_duration);
        }
        else {
            emit exportTimeLine(fileExportUrl->url(), format, "", GenTime(0), m_duration);
        }
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
            emit exportToFirewire(fw_URL->url(), firewireport->value());
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
    processProgress->setProgress(0);
    if (autoPlay->isChecked ()) {
        KRun *run=new KRun(KURL(fileExportUrl->url()));
    }
}

void exportWidget::exportFileToTheora(QString dstFileName, int width, int height, int audio, int video)
{
    QString command = "ffmpeg2theora /home/one/output.dv -a "+QString::number(audio)+" -v "+QString::number(video)+" -f dv -x "+QString::number(width)+" -y "+QString::number(height)+" -o "+dstFileName;
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
	kdDebug() << "******* THEORA : "<< QString(line).stripWhiteSpace ().section(" ",0,0) <<endl;
    }
    kdDebug() << "******* FINISHED : "<< endl;
    pclose(fp);
}

