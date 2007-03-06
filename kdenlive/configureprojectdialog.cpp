/***************************************************************************
                          configureprojectdialog  -  description
                             -------------------
    begin                : Sat Nov 15 2003
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
#include "configureprojectdialog.h"

#include <qhbox.h>
#include <qsplitter.h>
#include <qvbox.h>
#include <qlayout.h>

#include <kiconloader.h>
#include <klineedit.h>
#include <kjanuswidget.h>
#include <kurlrequester.h>
#include <klistbox.h>
#include <klocale.h>
#include <kpushbutton.h>
#include <knuminput.h>


#include "configureproject.h"
#include "exportconfig.h"

ConfigureProjectDialog::ConfigureProjectDialog(Gui::KdenliveApp * parent, const char *name, WFlags f):ConfigureProjectPanel_UI(parent, name), m_app(parent)
{

    loadTemplates();
    
/*    connect ( m_config->templatesList, SIGNAL(highlighted( const QString & )), this, SLOT(loadSettings(const QString & )));*/
}


ConfigureProjectDialog::~ConfigureProjectDialog()
{
}

void ConfigureProjectDialog::loadTemplates()
{
     templatesList->clear();
     templatesList->insertStringList(m_app->videoProjectFormats());
     projectFolder->setURL(KdenliveSettings::currentdefaultfolder());

    /*projectList.setAutoDelete(true);
    ProjectTemplate *pal = new ProjectTemplate("PAL", 25.0, 720, 576);
    projectList.append(pal);
    ProjectTemplate *ntsc = new ProjectTemplate("NTSC", 30.0, 720, 480);
    projectList.append(ntsc);*/
    updateDisplay();
}

QString ConfigureProjectDialog::selectedFolder()
{
    return projectFolder->url();
}

void ConfigureProjectDialog::updateDisplay()
{
    /*ProjectTemplate *project;
    for ( project = projectList.first(); project; project = projectList.next() ){
        m_presetList->insertItem(project->name());
    }*/
}

void ConfigureProjectDialog::setValues(const double &fps, const int &width, const int &height, KURL folder)
{
    framespersecond->setText(QString::number(fps));
    framewidth->setText(QString::number(width));
    frameheight->setText(QString::number(height));
    projectFolder->setURL(folder.url());
}

void ConfigureProjectDialog::loadSettings(const QString & name)
{
/*    ProjectTemplate *project;
    for ( project = projectList.first(); project; project = projectList.next() ){
        if (project->name() == name) {
            m_config->framespersecond->setText(QString::number(project->fps()));
            m_config->framewidth->setText(QString::number(project->width()));
            m_config->frameheight->setText(QString::number(project->height()));
            break;
        }
    }*/
}

/** Occurs when the apply button is clicked. */
void ConfigureProjectDialog::slotApply()
{
    //m_renderDlg->writeSettings();
}

/** Called when the ok button is clicked. */
void ConfigureProjectDialog::slotOk()
{
    //m_renderDlg->writeSettings();
    accept();
}

/** Called when the cancel button is clicked. */
void ConfigureProjectDialog::slotCancel()
{
    reject();
}

/** Called when the "Default" button is pressed. */
void ConfigureProjectDialog::slotDefault()
{
    //m_renderDlg->readSettings();
}

#include "configureprojectdialog.moc"
