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
    initEncoders();
    m_isRunning = false;
    
    tabWidget->page(1)->setEnabled(false);
    tabWidget->page(2)->setEnabled(false);
    connect(exportButton,SIGNAL(clicked()),this,SLOT(startExport()));
    connect(encoders,SIGNAL(activated(int)),this,SLOT(slotAdjustWidgets(int)));
}

exportWidget::~exportWidget()
{}


void exportWidget::initEncoders()
{
    encoders->insertItem(i18n("dv"));
    //container->hide();
    convertProgress->hide();
    convert_label->hide();
    container->setEnabled(false);
    //flayout = 0;
    encoders->insertItem(i18n("mpeg"));
}

void exportWidget::slotAdjustWidgets(int pos)
{
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
        case 3:
            format = "theora";
            break;
        default:
            format = "dv";
    }
    if (format == "mpeg") {
        QString vsize = videoSize->currentText();
        emit exportTimeLine(fileExportUrl->url(), format, vsize, GenTime(0), GenTime(20));
    }
    else {
        emit exportTimeLine(fileExportUrl->url(), format, "", GenTime(0), GenTime(20));
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

