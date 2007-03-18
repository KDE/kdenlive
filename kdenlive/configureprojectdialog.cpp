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

#include <klineedit.h>
#include <kurlrequester.h>
#include <klistbox.h>
#include <klocale.h>
#include <kpushbutton.h>
#include <knuminput.h>

#include "kdenlivedoc.h"
#include "editmetadata_ui.h"
#include "configureprojectdialog.h"

ConfigureProjectDialog::ConfigureProjectDialog(Gui::KdenliveApp * parent, const char *name, WFlags f):ConfigureProjectPanel_UI(parent, name), m_app(parent)
{

    loadTemplates();
    connect ( templatesList, SIGNAL(activated( int )), this, SLOT(updateDisplay()));
    connect( edit_metadata, SIGNAL( clicked()), this, SLOT( slotEditMetadata()));
}


ConfigureProjectDialog::~ConfigureProjectDialog()
{
}


void ConfigureProjectDialog::slotEditMetadata()
{
    QStringList metaValues = m_app->getDocument()->metadata();
    editMetadata_UI *editMeta = new editMetadata_UI(this);
    editMeta->meta_author->setText(metaValues[0]);
    editMeta->meta_title->setText(metaValues[1]);
    editMeta->meta_comment->setText(metaValues[2]);
    editMeta->meta_copyright->setText(metaValues[3]);
    editMeta->meta_album->setText(metaValues[4]);
    editMeta->meta_track->setValue(metaValues[5].toInt());
    editMeta->meta_year->setText(metaValues[6]);
    if (editMeta->exec() == QDialog::Accepted ) {
	metaValues.clear();
	metaValues << editMeta->meta_author->text();
	metaValues << editMeta->meta_title->text();
	metaValues << editMeta->meta_comment->text();
	metaValues << editMeta->meta_copyright->text();
	metaValues << editMeta->meta_album->text();
	metaValues << QString::number(editMeta->meta_track->value());
	metaValues << editMeta->meta_year->text();
	m_app->getDocument()->slotSetMetadata(metaValues);
	m_app->documentModified(true);
    }
    delete editMeta;
}

void ConfigureProjectDialog::loadTemplates()
{
    templatesList->clear();
    templatesList->insertStringList(m_app->videoProjectFormats());
    projectFolder->setURL(KdenliveSettings::currentdefaultfolder());
    templatesList->setCurrentText(m_app->projectFormatName((uint) m_app->projectVideoFormat()));
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


#include "configureprojectdialog.moc"
