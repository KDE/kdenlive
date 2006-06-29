/***************************************************************************
                          newproject  -  description
                             -------------------
    begin                : Wed Jun 28 2006
    copyright            : (C) 2006 by Jason Wood
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

#include <qstringlist.h>
#include <qtabwidget.h>
#include <qpushbutton.h>

#include <klistbox.h>
#include <klineedit.h>
#include <kurlrequester.h>
#include <kfiledialog.h>
#include <klocale.h>
#include <kmessagebox.h>
#include <kio/netaccess.h>

#include "newproject.h"

namespace Gui {

    newProject::newProject(QString defaultFolder, QStringList list, QWidget *parent, const char *name):newProject_UI(parent, name)
    {
	recentFiles->insertStringList(list);
	m_isNewFile = false;
	m_defaultFolder = defaultFolder;
	projectFolder->setMode(KFile::Directory);
	projectFolder->setURL(defaultFolder);
	if (!list.isEmpty()) recentFiles->setCurrentItem(0);
	connect(tabWidget, SIGNAL(currentChanged ( QWidget * )), this, SLOT(adjustButton()));
	connect(projectName, SIGNAL(textChanged (const QString &)), this, SLOT(adjustButton()));
	connect(projectFolder, SIGNAL(textChanged (const QString &)), this, SLOT(adjustButton()));
	connect(buttonQuit, SIGNAL(clicked ()), this, SLOT(reject()));
	connect(buttonOpen, SIGNAL(clicked ()), this, SLOT(openProject()));
	connect(buttonOk, SIGNAL(clicked ()), this, SLOT(checkFile()));
	connect(recentFiles, SIGNAL(doubleClicked(QListBoxItem *item, const QPoint &pos)), this, SLOT(checkFile()));
    }

    newProject::~newProject() {}

    void newProject::adjustButton()
    {
	if (tabWidget->currentPageIndex() == 0) {
		if (projectName->text().isEmpty() || projectFolder->url().isEmpty())
			buttonOk->setEnabled(false);
		else buttonOk->setEnabled(true);
	}
	else if (recentFiles->count()>0) buttonOk->setEnabled(true);
    }

    void newProject::openProject()
    {
	KURL url = KFileDialog::getOpenURL(QString(), i18n( "*.kdenlive|Kdenlive Project Files (*.kdenlive)" ), this, i18n("Open File..."));
	if (!url.isEmpty()) {
		m_selectedProject = url;
		accept();
	}
    }

    void newProject::checkFile()
    {
	if (tabWidget->currentPageIndex() == 0) {
		m_isNewFile = true;
		if (!KIO::NetAccess::exists(KURL(projectFolder->url()), false, this)) {
			KIO::NetAccess::mkdir(KURL(projectFolder->url()), this);
			if (!KIO::NetAccess::exists(KURL(projectFolder->url()), false, this)) {
				KMessageBox::sorry(this, i18n("Unable to access the selected folder.\nPlease chose another project folder."));
				return;
			}
		}
		if (!KIO::NetAccess::exists(KURL(projectFolder->url() + projectName->text()), false, this)) {
			KIO::NetAccess::mkdir(KURL(projectFolder->url() + projectName->text()), this);
			if (!KIO::NetAccess::exists(KURL(projectFolder->url() + projectName->text()), false, this)) {
				KMessageBox::sorry(this, i18n("Unable to access the selected folder.\nPlease chose another project folder."));
				return;
			}
		}

		accept();
	}
	else {
		m_selectedProject = KURL(recentFiles->currentText());
		accept();
	}
    }

    KURL newProject::selectedFile()
    {
	return m_selectedProject;
    }

    bool newProject::isNewFile()
    {
	return m_isNewFile;
    }

    KURL newProject::projectFolderPath()
    {
	return KURL(projectFolder->url() + projectName->text());
    }

}
