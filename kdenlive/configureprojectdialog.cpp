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

ConfigureProjectDialog::ConfigureProjectDialog(Gui::KdenliveApp * parent, const char *name, WFlags f):ConfigureProjectPanel_UI(parent, name), m_app(parent)
{

    loadTemplates();
    connect ( templatesList, SIGNAL(activated( int )), this, SLOT(updateDisplay()));
}


ConfigureProjectDialog::~ConfigureProjectDialog()
{
}

void ConfigureProjectDialog::loadTemplates()
{
    templatesList->clear();
    templatesList->insertStringList(m_app->videoProjectFormats());
    projectFolder->setURL(KdenliveSettings::currentdefaultfolder());
    templatesList->setCurrentText(m_app->projectFormatName(m_app->projectVideoFormat()));
    updateDisplay();
}

QString ConfigureProjectDialog::selectedFolder()
{
    return projectFolder->url();
}

QString ConfigureProjectDialog::selectedFormat()
{
    return templatesList->currentText();
}

void ConfigureProjectDialog::updateDisplay()
{
    formatTemplate params = m_app->projectFormatParameters((int) m_app->projectFormatFromName(templatesList->currentText()));

    framespersecond->setText(QString::number(params.fps()));
    framewidth->setText(QString::number(params.width()));
    frameheight->setText(QString::number(params.height()));
    aspectratio->setText(QString::number(params.aspect()));
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
