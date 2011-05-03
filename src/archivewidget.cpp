/***************************************************************************
 *   Copyright (C) 2011 by Jean-Baptiste Mardelle (jb@kdenlive.org)        *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA          *
 ***************************************************************************/


#include "archivewidget.h"
#include "titlewidget.h"

#include <KLocale>
#include <KDiskFreeSpaceInfo>
#include <KUrlRequester>
#include <KFileDialog>

#include <KDebug>
#include <QListWidget>
#include "projectsettings.h"


ArchiveWidget::ArchiveWidget(QList <DocClipBase*> list, QStringList lumas, QWidget * parent) :
        QDialog(parent),
        m_requestedSize(0),
        m_copyJob(NULL)
{
    setupUi(this);
    setWindowTitle(i18n("Archive Project"));
    archive_url->setUrl(KUrl(QDir::homePath()));
    connect(archive_url, SIGNAL(textChanged (const QString &)), this, SLOT(slotCheckSpace()));

    // process all files
    QStringList allFonts;
    foreach(const QString & file, lumas) {
        kDebug()<<"LUMA: "<<file;
        files_list->addItem(file);
        m_requestedSize += QFileInfo(file).size();
    }

    for (int i = 0; i < list.count(); i++) {
        DocClipBase *clip = list.at(i);
        if (clip->clipType() == SLIDESHOW) {    
            QStringList subfiles = ProjectSettings::extractSlideshowUrls(clip->fileURL());
            foreach(const QString & file, subfiles) {
                kDebug()<<"SLIDE: "<<file;
                files_list->addItem(file);
                m_requestedSize += QFileInfo(file).size();
            }
        } else if (!clip->fileURL().isEmpty()) {
            files_list->addItem(clip->fileURL().path());
            m_requestedSize += QFileInfo(clip->fileURL().path()).size();
        }
        if (clip->clipType() == TEXT) {
            QStringList imagefiles = TitleWidget::extractImageList(clip->getProperty("xmldata"));
            QStringList fonts = TitleWidget::extractFontList(clip->getProperty("xmldata"));
            foreach(const QString & file, imagefiles) {
                kDebug()<<"TXT IMAGE: "<<file;
                files_list->addItem(file);
                m_requestedSize += QFileInfo(file).size();
            }
            allFonts << fonts;
        } else if (clip->clipType() == PLAYLIST) {
            QStringList files = ProjectSettings::extractPlaylistUrls(clip->fileURL().path());
            foreach(const QString & file, files) {
                kDebug()<<"PLAYLIST: "<<file;
                files_list->addItem(file);
                m_requestedSize += QFileInfo(file).size();
            }
        }
    }
#if QT_VERSION >= 0x040500
    allFonts.removeDuplicates();
#endif
    project_files->setText(i18n("%1 files to archive, requires %2", files_list->count(), KIO::convertSize(m_requestedSize)));
    buttonBox->button(QDialogButtonBox::Apply)->setText(i18n("Archive"));
    connect(buttonBox->button(QDialogButtonBox::Apply), SIGNAL(clicked()), this, SLOT(slotStartArchiving()));
    buttonBox->button(QDialogButtonBox::Apply)->setEnabled(false);
    
    slotCheckSpace();
}

ArchiveWidget::~ArchiveWidget()
{
}

void ArchiveWidget::slotCheckSpace()
{
    KDiskFreeSpaceInfo inf = KDiskFreeSpaceInfo::freeSpaceInfo( archive_url->url().path());
    KIO::filesize_t freeSize = inf.available();;
    if (freeSize > m_requestedSize) {
        // everything is ok
        buttonBox->button(QDialogButtonBox::Apply)->setEnabled(true);
        icon_info->setPixmap(KIcon("dialog-ok").pixmap(16, 16));
        text_info->setText(i18n("Available space on drive: %1", KIO::convertSize(freeSize)));
    }
    else {
        buttonBox->button(QDialogButtonBox::Apply)->setEnabled(false);
        icon_info->setPixmap(KIcon("dialog-close").pixmap(16, 16));
        text_info->setText(i18n("Not enough space on drive, free space: %1", KIO::convertSize(freeSize)));
    }
}

void ArchiveWidget::slotStartArchiving()
{
    if (m_copyJob) {
        // archiving in progress, abort
        m_copyJob->kill(KJob::EmitResult);
        return;
    }
    KUrl::List files;
    for (int i = 0; i < files_list->count(); i++) {
        if (files_list->item(i))
            files << KUrl(files_list->item(i)->text());
    }
    
    progressBar->setValue(0);
    buttonBox->button(QDialogButtonBox::Apply)->setText(i18n("Abort"));
    m_copyJob = KIO::copy (files, archive_url->url(), KIO::HideProgressInfo);
    connect(m_copyJob, SIGNAL(result(KJob *)), this, SLOT(slotArchivingFinished(KJob *)));
    connect(m_copyJob, SIGNAL(processedSize(KJob *, qulonglong)), this, SLOT(slotArchivingProgress(KJob *, qulonglong)));
}

void ArchiveWidget::slotArchivingFinished(KJob *job)
{
    progressBar->setValue(100);
    buttonBox->button(QDialogButtonBox::Apply)->setText(i18n("Archive"));
    if (job->error() == 0) text_info->setText(i18n("Project was successfully archived."));
    else {
        icon_info->setPixmap(KIcon("dialog-close").pixmap(16, 16));
        text_info->setText(i18n("There was an error while copying the files: %1", job->errorString()));
    }
    m_copyJob = NULL;
}

void ArchiveWidget::slotArchivingProgress(KJob *, qulonglong size)
{
    progressBar->setValue((int) 100 * size / m_requestedSize);
}

