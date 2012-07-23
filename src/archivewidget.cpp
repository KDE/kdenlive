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
#include <KMessageBox>
#include <KGuiItem>
#include <KIO/NetAccess>
#include <KTar>
#include <KDebug>
#include <KApplication>
#include <kio/directorysizejob.h>
#if KDE_IS_VERSION(4,7,0)
#include <KMessageWidget>
#endif

#include <QTreeWidget>
#include <QtConcurrentRun>
#include "projectsettings.h"


ArchiveWidget::ArchiveWidget(QString projectName, QDomDocument doc, QList <DocClipBase*> list, QStringList luma_list, QWidget * parent) :
        QDialog(parent),
        m_requestedSize(0),
        m_copyJob(NULL),
        m_name(projectName.section('.', 0, -2)),
        m_doc(doc),
        m_abortArchive(false),
        m_extractMode(false),
        m_extractArchive(NULL),
        m_missingClips(0)
{
    setAttribute(Qt::WA_DeleteOnClose);
    setupUi(this);
    setWindowTitle(i18n("Archive Project"));
    archive_url->setUrl(KUrl(QDir::homePath()));
    connect(archive_url, SIGNAL(textChanged (const QString &)), this, SLOT(slotCheckSpace()));
    connect(this, SIGNAL(archivingFinished(bool)), this, SLOT(slotArchivingFinished(bool)));
    connect(this, SIGNAL(archiveProgress(int)), this, SLOT(slotArchivingProgress(int)));
    connect(proxy_only, SIGNAL(stateChanged(int)), this, SLOT(slotProxyOnly(int)));

    // Setup categories
    QTreeWidgetItem *videos = new QTreeWidgetItem(files_list, QStringList() << i18n("Video clips"));
    videos->setIcon(0, KIcon("video-x-generic"));
    videos->setData(0, Qt::UserRole, "videos");
    videos->setExpanded(false);
    QTreeWidgetItem *sounds = new QTreeWidgetItem(files_list, QStringList() << i18n("Audio clips"));
    sounds->setIcon(0, KIcon("audio-x-generic"));
    sounds->setData(0, Qt::UserRole, "sounds");
    sounds->setExpanded(false);
    QTreeWidgetItem *images = new QTreeWidgetItem(files_list, QStringList() << i18n("Image clips"));
    images->setIcon(0, KIcon("image-x-generic"));
    images->setData(0, Qt::UserRole, "images");
    images->setExpanded(false);
    QTreeWidgetItem *slideshows = new QTreeWidgetItem(files_list, QStringList() << i18n("Slideshow clips"));
    slideshows->setIcon(0, KIcon("image-x-generic"));
    slideshows->setData(0, Qt::UserRole, "slideshows");
    slideshows->setExpanded(false);
    QTreeWidgetItem *texts = new QTreeWidgetItem(files_list, QStringList() << i18n("Text clips"));
    texts->setIcon(0, KIcon("text-plain"));
    texts->setData(0, Qt::UserRole, "texts");
    texts->setExpanded(false);
    QTreeWidgetItem *playlists = new QTreeWidgetItem(files_list, QStringList() << i18n("Playlist clips"));
    playlists->setIcon(0, KIcon("video-mlt-playlist"));
    playlists->setData(0, Qt::UserRole, "playlist");
    playlists->setExpanded(false);
    QTreeWidgetItem *others = new QTreeWidgetItem(files_list, QStringList() << i18n("Other clips"));
    others->setIcon(0, KIcon("unknown"));
    others->setData(0, Qt::UserRole, "others");
    others->setExpanded(false);
    QTreeWidgetItem *lumas = new QTreeWidgetItem(files_list, QStringList() << i18n("Luma files"));
    lumas->setIcon(0, KIcon("image-x-generic"));
    lumas->setData(0, Qt::UserRole, "lumas");
    lumas->setExpanded(false);
    
    QTreeWidgetItem *proxies = new QTreeWidgetItem(files_list, QStringList() << i18n("Proxy clips"));
    proxies->setIcon(0, KIcon("video-x-generic"));
    proxies->setData(0, Qt::UserRole, "proxy");
    proxies->setExpanded(false);
    
    // process all files
    QStringList allFonts;
    KUrl::List fileUrls;
    QStringList fileNames;
    QStringList extraImageUrls;
    QStringList otherUrls;
    generateItems(lumas, luma_list);

    QMap <QString, QString> slideUrls;
    QMap <QString, QString> audioUrls;
    QMap <QString, QString>videoUrls;
    QMap <QString, QString>imageUrls;
    QMap <QString, QString>playlistUrls;
    QMap <QString, QString>proxyUrls;

    for (int i = 0; i < list.count(); i++) {
        DocClipBase *clip = list.at(i);
        CLIPTYPE t = clip->clipType();
        QString id = clip->getId();
        if (t == SLIDESHOW) {
            KUrl slideUrl = clip->fileURL();
            //TODO: Slideshow files
            slideUrls.insert(id, slideUrl.path());
        }
        else if (t == IMAGE) imageUrls.insert(id, clip->fileURL().path());
        else if (t == TEXT) {
            QStringList imagefiles = TitleWidget::extractImageList(clip->getProperty("xmldata"));
            QStringList fonts = TitleWidget::extractFontList(clip->getProperty("xmldata"));
            extraImageUrls << imagefiles;
            allFonts << fonts;
        } else if (t == PLAYLIST) {
            playlistUrls.insert(id, clip->fileURL().path());
            QStringList files = ProjectSettings::extractPlaylistUrls(clip->fileURL().path());
            otherUrls << files;
        }
        else if (!clip->fileURL().isEmpty()) {
            if (t == AUDIO) audioUrls.insert(id, clip->fileURL().path());
            else {
                videoUrls.insert(id, clip->fileURL().path());
                // Check if we have a proxy
                QString proxy = clip->getProperty("proxy");
                if (!proxy.isEmpty() && proxy != "-" && QFile::exists(proxy)) proxyUrls.insert(id, proxy);
            }
        }
    }

    generateItems(images, extraImageUrls);
    generateItems(sounds, audioUrls);
    generateItems(videos, videoUrls);
    generateItems(images, imageUrls);
    generateItems(slideshows, slideUrls);
    generateItems(playlists, playlistUrls);
    generateItems(others, otherUrls);
    generateItems(proxies, proxyUrls);
    
    allFonts.removeDuplicates();

#if KDE_IS_VERSION(4,7,0)
        m_infoMessage = new KMessageWidget(this);
        QVBoxLayout *s =  static_cast <QVBoxLayout*> (layout());
        s->insertWidget(5, m_infoMessage);
        m_infoMessage->setCloseButtonVisible(false);
        m_infoMessage->setWordWrap(true);
        m_infoMessage->hide();
#endif
        
        // missing clips, warn user
    if (m_missingClips > 0) {
        QString infoText = i18np("You have %1 missing clip in your project.", "You have %1 missing clips in your project.", m_missingClips);
#if KDE_IS_VERSION(4,7,0)
        m_infoMessage->setMessageType(KMessageWidget::Warning);
        m_infoMessage->setText(infoText);
        m_infoMessage->animatedShow();
#else
        KMessageBox::sorry(this, infoText);
#endif
    }

    //TODO: fonts

    // Hide unused categories, add item count
    int total = 0;
    for (int i = 0; i < files_list->topLevelItemCount(); i++) {
        QTreeWidgetItem *parentItem = files_list->topLevelItem(i);
        int items = parentItem->childCount();
        if (items == 0) {
            files_list->topLevelItem(i)->setHidden(true);
        }
        else {
            if (parentItem->data(0, Qt::UserRole).toString() == "slideshows")
            {
                // Special case: slideshows contain several files
                for (int j = 0; j < items; j++) {
                    total += parentItem->child(j)->data(0, Qt::UserRole + 1).toStringList().count();
                }
            }
            else total += items;
            parentItem->setText(0, files_list->topLevelItem(i)->text(0) + ' ' + i18np("(%1 item)", "(%1 items)", items));
        }
    }
    if (m_name.isEmpty()) m_name = i18n("Untitled");
    compressed_archive->setText(compressed_archive->text() + " (" + m_name + ".tar.gz)");
    project_files->setText(i18np("%1 file to archive, requires %2", "%1 files to archive, requires %2", total, KIO::convertSize(m_requestedSize)));
    buttonBox->button(QDialogButtonBox::Apply)->setText(i18n("Archive"));
    connect(buttonBox->button(QDialogButtonBox::Apply), SIGNAL(clicked()), this, SLOT(slotStartArchiving()));
    buttonBox->button(QDialogButtonBox::Apply)->setEnabled(false);

    slotCheckSpace();
}

// Constructor for extract widget
ArchiveWidget::ArchiveWidget(const KUrl &url, QWidget * parent):
    QDialog(parent),
    m_extractMode(true),
    m_extractUrl(url)
{
    //setAttribute(Qt::WA_DeleteOnClose);

    setupUi(this);
    connect(this, SIGNAL(extractingFinished()), this, SLOT(slotExtractingFinished()));
    connect(this, SIGNAL(showMessage(const QString &, const QString &)), this, SLOT(slotDisplayMessage(const QString &, const QString &)));
    
    compressed_archive->setHidden(true);
    proxy_only->setHidden(true);
    project_files->setHidden(true);
    files_list->setHidden(true);
    label->setText(i18n("Extract to"));
    setWindowTitle(i18n("Open Archived Project"));
    archive_url->setUrl(KUrl(QDir::homePath()));
    buttonBox->button(QDialogButtonBox::Apply)->setText(i18n("Extract"));
    connect(buttonBox->button(QDialogButtonBox::Apply), SIGNAL(clicked()), this, SLOT(slotStartExtracting()));
    buttonBox->button(QDialogButtonBox::Apply)->setEnabled(true);
    adjustSize();
    m_archiveThread = QtConcurrent::run(this, &ArchiveWidget::openArchiveForExtraction);
}


ArchiveWidget::~ArchiveWidget()
{
    if (m_extractArchive) delete m_extractArchive;
}

void ArchiveWidget::slotDisplayMessage(const QString &icon, const QString &text)
{    
    icon_info->setPixmap(KIcon(icon).pixmap(16, 16));
    text_info->setText(text);
}

void ArchiveWidget::slotJobResult(bool success, const QString &text)
{
#if KDE_IS_VERSION(4,7,0)
    m_infoMessage->setMessageType(success ? KMessageWidget::Positive : KMessageWidget::Warning);
    m_infoMessage->setText(text);
    m_infoMessage->animatedShow();
#else
    if (success) icon_info->setPixmap(KIcon("dialog-ok").pixmap(16, 16));
    else icon_info->setPixmap(KIcon("dialog-close").pixmap(16, 16));
    text_info->setText(text);
#endif
}

void ArchiveWidget::openArchiveForExtraction()
{
    emit showMessage("system-run", i18n("Opening archive..."));
    m_extractArchive = new KTar(m_extractUrl.path());
    if (!m_extractArchive->isOpen() && !m_extractArchive->open( QIODevice::ReadOnly )) {
        emit showMessage("dialog-close", i18n("Cannot open archive file:\n %1", m_extractUrl.path()));
        groupBox->setEnabled(false);
        return;
    }

    // Check that it is a kdenlive project archive
    bool isProjectArchive = false;
    QStringList files = m_extractArchive->directory()->entries();
    for (int i = 0; i < files.count(); i++) {
        if (files.at(i).endsWith(".kdenlive")) {
            m_projectName = files.at(i);
            isProjectArchive = true;
            break;
        }
    }

    if (!isProjectArchive) {
        emit showMessage("dialog-close", i18n("File %1\n is not an archived Kdenlive project", m_extractUrl.path()));
        groupBox->setEnabled(false);
        return;
    }
    buttonBox->button(QDialogButtonBox::Apply)->setEnabled(true);
    emit showMessage("dialog-ok", i18n("Ready"));
}

void ArchiveWidget::done ( int r )
{
    if (closeAccepted()) QDialog::done(r);
}

void ArchiveWidget::closeEvent ( QCloseEvent * e )
{

    if (closeAccepted()) e->accept();
    else e->ignore();
}


bool ArchiveWidget::closeAccepted()
{
    if (!m_extractMode && !archive_url->isEnabled()) {
        // Archiving in progress, should we stop?
        if (KMessageBox::warningContinueCancel(this, i18n("Archiving in progress, do you want to stop it?"), i18n("Stop Archiving"), KGuiItem(i18n("Stop Archiving"))) != KMessageBox::Continue) {
            return false;
        }
        if (m_copyJob) m_copyJob->kill();
    }
    return true;
}


void ArchiveWidget::generateItems(QTreeWidgetItem *parentItem, QStringList items)
{
    QStringList filesList;
    QString fileName;
    int ix = 0;
    bool isSlideshow = parentItem->data(0, Qt::UserRole).toString() == "slideshows";
    foreach(const QString & file, items) {
        QTreeWidgetItem *item = new QTreeWidgetItem(parentItem, QStringList() << file);
        fileName = KUrl(file).fileName();
        if (isSlideshow) {
            // we store each slideshow in a separate subdirectory
            item->setData(0, Qt::UserRole, ix);
            ix++;
            KUrl slideUrl(file);
            QDir dir(slideUrl.directory(KUrl::AppendTrailingSlash));
            if (slideUrl.fileName().startsWith(".all.")) {
                // mimetype slideshow (for example *.png)
                    QStringList filters;
                    QString extension;
                    // TODO: improve jpeg image detection with extension like jpeg, requires change in MLT image producers
                    filters << "*." + slideUrl.fileName().section('.', -1);
                    dir.setNameFilters(filters);
                    QFileInfoList resultList = dir.entryInfoList(QDir::Files);
                    QStringList slideImages;
                    qint64 totalSize = 0;
                    for (int i = 0; i < resultList.count(); i++) {
                        totalSize += resultList.at(i).size();
                        slideImages << resultList.at(i).absoluteFilePath();
                    }
                    item->setData(0, Qt::UserRole + 1, slideImages);
                    item->setData(0, Qt::UserRole + 3, totalSize);
                    m_requestedSize += totalSize;
            }
            else {
                // pattern url (like clip%.3d.png)
                QStringList result = dir.entryList(QDir::Files);
                QString filter = slideUrl.fileName();
                QString ext = filter.section('.', -1);
                filter = filter.section('%', 0, -2);
                QString regexp = '^' + filter + "\\d+\\." + ext + '$';
                QRegExp rx(regexp);
                QStringList slideImages;
                QString directory = dir.absolutePath();
                if (!directory.endsWith('/')) directory.append('/');
                qint64 totalSize = 0;
                foreach(const QString & path, result) {
                    if (rx.exactMatch(path)) {
                        totalSize += QFileInfo(directory + path).size();
                        slideImages <<  directory + path;
                    }
                }
                item->setData(0, Qt::UserRole + 1, slideImages);
                item->setData(0, Qt::UserRole + 3, totalSize);
                m_requestedSize += totalSize;
            }                    
        }
        else if (filesList.contains(fileName)) {
            // we have 2 files with same name
            int ix = 0;
            QString newFileName = fileName.section('.', 0, -2) + '_' + QString::number(ix) + '.' + fileName.section('.', -1);
            while (filesList.contains(newFileName)) {
                ix ++;
                newFileName = fileName.section('.', 0, -2) + '_' + QString::number(ix) + '.' + fileName.section('.', -1);
            }
            fileName = newFileName;
            item->setData(0, Qt::UserRole, fileName);
        }
        if (!isSlideshow) {
            qint64 fileSize = QFileInfo(file).size();
            if (fileSize <= 0) {
                item->setIcon(0, KIcon("edit-delete"));
                m_missingClips++;
            }
            else {
                m_requestedSize += fileSize;
                item->setData(0, Qt::UserRole + 3, fileSize);
            }
            filesList << fileName;
        }
    }
}

void ArchiveWidget::generateItems(QTreeWidgetItem *parentItem, QMap <QString, QString> items)
{
    QStringList filesList;
    QString fileName;
    int ix = 0;
    bool isSlideshow = parentItem->data(0, Qt::UserRole).toString() == "slideshows";
    QMap<QString, QString>::const_iterator it = items.constBegin();
    while (it != items.constEnd()) {
        QString file = it.value();
        QTreeWidgetItem *item = new QTreeWidgetItem(parentItem, QStringList() << file);
        // Store the clip's id
        item->setData(0, Qt::UserRole + 2, it.key());
        fileName = KUrl(file).fileName();
        if (isSlideshow) {
            // we store each slideshow in a separate subdirectory
            item->setData(0, Qt::UserRole, ix);
            ix++;
            KUrl slideUrl(file);
            QDir dir(slideUrl.directory(KUrl::AppendTrailingSlash));
            if (slideUrl.fileName().startsWith(".all.")) {
                // mimetype slideshow (for example *.png)
                    QStringList filters;
                    QString extension;
                    // TODO: improve jpeg image detection with extension like jpeg, requires change in MLT image producers
                    filters << "*." + slideUrl.fileName().section('.', -1);
                    dir.setNameFilters(filters);
                    QFileInfoList resultList = dir.entryInfoList(QDir::Files);
                    QStringList slideImages;
                    qint64 totalSize = 0;
                    for (int i = 0; i < resultList.count(); i++) {
                        totalSize += resultList.at(i).size();
                        slideImages << resultList.at(i).absoluteFilePath();
                    }
                    item->setData(0, Qt::UserRole + 1, slideImages);
                    item->setData(0, Qt::UserRole + 3, totalSize);
                    m_requestedSize += totalSize;
            }
            else {
                // pattern url (like clip%.3d.png)
                QStringList result = dir.entryList(QDir::Files);
                QString filter = slideUrl.fileName();
                QString ext = filter.section('.', -1);
                filter = filter.section('%', 0, -2);
                QString regexp = '^' + filter + "\\d+\\." + ext + '$';
                QRegExp rx(regexp);
                QStringList slideImages;
                qint64 totalSize = 0;
                QString directory = dir.absolutePath();
                if (!directory.endsWith('/')) directory.append('/');
                foreach(const QString & path, result) {
                    if (rx.exactMatch(path)) {
                        totalSize += QFileInfo(directory + path).size();
                        slideImages <<  directory + path;
                    }
                }
                item->setData(0, Qt::UserRole + 1, slideImages);
                item->setData(0, Qt::UserRole + 3, totalSize);
                m_requestedSize += totalSize;
            }                    
        }
        else if (filesList.contains(fileName)) {
            // we have 2 files with same name
            int ix = 0;
            QString newFileName = fileName.section('.', 0, -2) + '_' + QString::number(ix) + '.' + fileName.section('.', -1);
            while (filesList.contains(newFileName)) {
                ix ++;
                newFileName = fileName.section('.', 0, -2) + "_" + QString::number(ix) + "." + fileName.section('.', -1);
            }
            fileName = newFileName;
            item->setData(0, Qt::UserRole, fileName);
        }
        if (!isSlideshow) {
            qint64 fileSize = QFileInfo(file).size();
            if (fileSize <= 0) {
                item->setIcon(0, KIcon("edit-delete"));
                m_missingClips++;
            }
            else {
                m_requestedSize += fileSize;
                item->setData(0, Qt::UserRole + 3, fileSize);
            }
            filesList << fileName;
        }
        ++it;
    }
}

void ArchiveWidget::slotCheckSpace()
{
    KDiskFreeSpaceInfo inf = KDiskFreeSpaceInfo::freeSpaceInfo( archive_url->url().path());
    KIO::filesize_t freeSize = inf.available();
    if (freeSize > m_requestedSize) {
        // everything is ok
        buttonBox->button(QDialogButtonBox::Apply)->setEnabled(true);
        slotDisplayMessage("dialog-ok", i18n("Available space on drive: %1", KIO::convertSize(freeSize)));
    }
    else {
        buttonBox->button(QDialogButtonBox::Apply)->setEnabled(false);
        slotDisplayMessage("dialog-close", i18n("Not enough space on drive, free space: %1", KIO::convertSize(freeSize)));
    }
}

bool ArchiveWidget::slotStartArchiving(bool firstPass)
{
    if (firstPass && (m_copyJob || m_archiveThread.isRunning())) {
        // archiving in progress, abort
        if (m_copyJob) m_copyJob->kill(KJob::EmitResult);
        m_abortArchive = true;
        return true;
    }
    bool isArchive = compressed_archive->isChecked();
    if (!firstPass) m_copyJob = NULL;
    else {
        //starting archiving
        m_abortArchive = false;
        m_duplicateFiles.clear();
        m_replacementList.clear();
        m_foldersList.clear();
        m_filesList.clear();
        slotDisplayMessage("system-run", i18n("Archiving..."));
        repaint();
        archive_url->setEnabled(false);
        proxy_only->setEnabled(false);
        compressed_archive->setEnabled(false);
    }
    KUrl::List files;
    KUrl destUrl;
    QString destPath;
    QTreeWidgetItem *parentItem;
    bool isSlideshow = false;
    int items = 0;
    
    // We parse all files going into one folder, then start the copy job
    
    for (int i = 0; i < files_list->topLevelItemCount(); i++) {
        parentItem = files_list->topLevelItem(i);
        if (parentItem->isDisabled()) {
            parentItem->setExpanded(false);
            continue;
        }
        if (parentItem->childCount() > 0) {
            if (parentItem->data(0, Qt::UserRole).toString() == "slideshows") {
                KUrl slideFolder(archive_url->url().path(KUrl::AddTrailingSlash) + "slideshows");
                if (isArchive) m_foldersList.append("slideshows");
                else KIO::NetAccess::mkdir(slideFolder, this);
                isSlideshow = true;
            }
            else isSlideshow = false;
            files_list->setCurrentItem(parentItem);
            parentItem->setExpanded(true);
            destPath = parentItem->data(0, Qt::UserRole).toString() + '/';
            destUrl = KUrl(archive_url->url().path(KUrl::AddTrailingSlash) + destPath);
            QTreeWidgetItem *item;
            for (int j = 0; j < parentItem->childCount(); j++) {
                item = parentItem->child(j);
                if (item->isDisabled()) continue;
                // Special case: slideshows
		items++;
                if (isSlideshow) {
                    destPath += item->data(0, Qt::UserRole).toString() + '/';
                    destUrl = KUrl(archive_url->url().path(KUrl::AddTrailingSlash) + destPath);
                    QStringList srcFiles = item->data(0, Qt::UserRole + 1).toStringList();
                    for (int k = 0; k < srcFiles.count(); k++) {
                        files << KUrl(srcFiles.at(k));
                    }
                    item->setDisabled(true);
                    if (parentItem->indexOfChild(item) == parentItem->childCount() - 1) {
                        // We have processed all slideshows
                        parentItem->setDisabled(true);
                    }
                    break;
                }
                else if (item->data(0, Qt::UserRole).isNull()) {
                    files << KUrl(item->text(0));
                }
                else {
                    // We must rename the destination file, since another file with same name exists
                    //TODO: monitor progress
                    if (isArchive) {
                        m_filesList.insert(item->text(0), destPath + item->data(0, Qt::UserRole).toString());
                    }
                    else m_duplicateFiles.insert(KUrl(item->text(0)), KUrl(destUrl.path(KUrl::AddTrailingSlash) + item->data(0, Qt::UserRole).toString()));
                }
            }
            if (!isSlideshow) parentItem->setDisabled(true);
            break;
        }
    }

    if (items == 0) {
	// No clips to archive
	if (isArchive) slotArchivingFinished(NULL, true);
	return true;
    }
    
    if (destPath.isEmpty()) {
        if (m_duplicateFiles.isEmpty()) return false;        
        QMapIterator<KUrl, KUrl> i(m_duplicateFiles);
        if (i.hasNext()) {
            i.next();
            KUrl startJobSrc = i.key();
            KUrl startJobDst = i.value();
            m_duplicateFiles.remove(startJobSrc);
            KIO::CopyJob *job = KIO::copyAs(startJobSrc, startJobDst, KIO::HideProgressInfo);
            connect(job, SIGNAL(result(KJob *)), this, SLOT(slotArchivingFinished(KJob *)));
            connect(job, SIGNAL(processedSize(KJob *, qulonglong)), this, SLOT(slotArchivingProgress(KJob *, qulonglong)));
        }
        return true;
    }

    if (isArchive) {
        m_foldersList.append(destPath);
        for (int i = 0; i < files.count(); i++) {
            m_filesList.insert(files.at(i).path(), destPath + files.at(i).fileName());
        }
        slotArchivingFinished();
    }
    else if (files.isEmpty()) {
        slotStartArchiving(false);
    }
    else {
        KIO::NetAccess::mkdir(destUrl, this);
        m_copyJob = KIO::copy (files, destUrl, KIO::HideProgressInfo);
        connect(m_copyJob, SIGNAL(result(KJob *)), this, SLOT(slotArchivingFinished(KJob *)));
        connect(m_copyJob, SIGNAL(processedSize(KJob *, qulonglong)), this, SLOT(slotArchivingProgress(KJob *, qulonglong)));
    }
    if (firstPass) {
        progressBar->setValue(0);
        buttonBox->button(QDialogButtonBox::Apply)->setText(i18n("Abort"));
    }
    return true;
}

void ArchiveWidget::slotArchivingFinished(KJob *job, bool finished)
{
    if (job == NULL || job->error() == 0) {
        if (!finished && slotStartArchiving(false)) {
            // We still have files to archive
            return;
        }
        else if (!compressed_archive->isChecked()) {
            // Archiving finished
            progressBar->setValue(100);
            if (processProjectFile()) {
                slotJobResult(true, i18n("Project was successfully archived."));
            }
            else {
                slotJobResult(false, i18n("There was an error processing project file"));
            }
        } else processProjectFile();
    }
    else {
        m_copyJob = NULL;
        slotJobResult(false, i18n("There was an error while copying the files: %1", job->errorString()));
    }
    if (!compressed_archive->isChecked()) {
        buttonBox->button(QDialogButtonBox::Apply)->setText(i18n("Archive"));
        archive_url->setEnabled(true);
        proxy_only->setEnabled(true);
        compressed_archive->setEnabled(true);
        for (int i = 0; i < files_list->topLevelItemCount(); i++) {
            files_list->topLevelItem(i)->setDisabled(false);
            for (int j = 0; j < files_list->topLevelItem(i)->childCount(); j++)
                files_list->topLevelItem(i)->child(j)->setDisabled(false);        
        }
    }
}

void ArchiveWidget::slotArchivingProgress(KJob *, qulonglong size)
{
    progressBar->setValue((int) 100 * size / m_requestedSize);
}


bool ArchiveWidget::processProjectFile()
{
    KUrl destUrl;
    QTreeWidgetItem *item;
    bool isArchive = compressed_archive->isChecked();

    for (int i = 0; i < files_list->topLevelItemCount(); i++) {
        QTreeWidgetItem *parentItem = files_list->topLevelItem(i);
        if (parentItem->childCount() > 0) {
            destUrl = KUrl(archive_url->url().path(KUrl::AddTrailingSlash) + parentItem->data(0, Qt::UserRole).toString());
            bool isSlideshow = parentItem->data(0, Qt::UserRole).toString() == "slideshows";
            for (int j = 0; j < parentItem->childCount(); j++) {
                item = parentItem->child(j);
                KUrl src(item->text(0));
                KUrl dest = destUrl;
                if (isSlideshow) {
                    dest = KUrl(destUrl.path(KUrl::AddTrailingSlash) + item->data(0, Qt::UserRole).toString() + '/' + src.fileName());
                }
                else if (item->data(0, Qt::UserRole).isNull()) {
                    dest.addPath(src.fileName());
                }
                else {
                    dest.addPath(item->data(0, Qt::UserRole).toString());
                }
                m_replacementList.insert(src, dest);
            }
        }
    }
    
    QDomElement mlt = m_doc.documentElement();
    QString root = mlt.attribute("root") + '/';

    // Adjust global settings
    QString basePath;
    if (isArchive) basePath = "$CURRENTPATH";
    else basePath = archive_url->url().path(KUrl::RemoveTrailingSlash);
    mlt.setAttribute("root", basePath);
    QDomElement project = mlt.firstChildElement("kdenlivedoc");
    project.setAttribute("projectfolder", basePath);

    // process kdenlive producers
    QDomNodeList prods = mlt.elementsByTagName("kdenlive_producer");
    for (int i = 0; i < prods.count(); i++) {
        QDomElement e = prods.item(i).toElement();
        if (e.isNull()) continue;
        if (e.hasAttribute("resource")) {
            KUrl src(e.attribute("resource"));
            KUrl dest = m_replacementList.value(src);
            if (!dest.isEmpty()) e.setAttribute("resource", dest.path());
        }
        if (e.hasAttribute("proxy") && e.attribute("proxy") != "-") {
            KUrl src(e.attribute("proxy"));
            KUrl dest = m_replacementList.value(src);
            if (!dest.isEmpty()) e.setAttribute("proxy", dest.path());
        }
    }

    // process mlt producers
    prods = mlt.elementsByTagName("producer");
    for (int i = 0; i < prods.count(); i++) {
        QDomElement e = prods.item(i).toElement();
        if (e.isNull()) continue;
        QString src = EffectsList::property(e, "resource");
        if (!src.isEmpty()) {
            if (!src.startsWith('/')) src.prepend(root);
            KUrl srcUrl(src);
            KUrl dest = m_replacementList.value(src);
            if (!dest.isEmpty()) EffectsList::setProperty(e, "resource", dest.path());
        }
    }

    // process mlt transitions (for luma files)
    prods = mlt.elementsByTagName("transition");
    QString attribute;
    for (int i = 0; i < prods.count(); i++) {
        QDomElement e = prods.item(i).toElement();
        if (e.isNull()) continue;
        attribute = "resource";
        QString src = EffectsList::property(e, attribute);
        if (src.isEmpty()) attribute = "luma";
        src = EffectsList::property(e, attribute);
        if (!src.isEmpty()) {
            if (!src.startsWith('/')) src.prepend(root);
            KUrl srcUrl(src);
            KUrl dest = m_replacementList.value(src);
            if (!dest.isEmpty()) EffectsList::setProperty(e, attribute, dest.path());
        }
    }

    QString playList = m_doc.toString();
    if (isArchive) {
        QString startString("\"");
        startString.append(archive_url->url().path(KUrl::RemoveTrailingSlash));
        QString endString("\"");
        endString.append(basePath);
        playList.replace(startString, endString);
        startString = '>' + archive_url->url().path(KUrl::RemoveTrailingSlash);
        endString = '>' + basePath;
        playList.replace(startString, endString);
    }

    if (isArchive) {
        m_temp = new KTemporaryFile;
        if (!m_temp->open()) KMessageBox::error(this, i18n("Cannot create temporary file"));
        m_temp->write(playList.toUtf8());
        m_temp->close();
        m_archiveThread = QtConcurrent::run(this, &ArchiveWidget::createArchive);
        return true;
    }
    
    QString path = archive_url->url().path(KUrl::AddTrailingSlash) + m_name + ".kdenlive";
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        kWarning() << "//////  ERROR writing to file: " << path;
        KMessageBox::error(this, i18n("Cannot write to file %1", path));
        return false;
    }

    file.write(m_doc.toString().toUtf8());
    if (file.error() != QFile::NoError) {
        KMessageBox::error(this, i18n("Cannot write to file %1", path));
        file.close();
        return false;
    }
    file.close();
    return true;
}

void ArchiveWidget::createArchive()
{
    QFileInfo dirInfo(archive_url->url().path());
    QString user = dirInfo.owner();
    QString group = dirInfo.group();
    KTar archive(archive_url->url().path(KUrl::AddTrailingSlash) + m_name + ".tar.gz", "application/x-gzip");
    archive.open( QIODevice::WriteOnly );

    // Create folders
    foreach(const QString &path, m_foldersList) {
        archive.writeDir(path, user, group);
    }

    // Add files
    int ix = 0;
    QMapIterator<QString, QString> i(m_filesList);
    while (i.hasNext()) {
        i.next();
        archive.addLocalFile(i.key(), i.value());
        emit archiveProgress((int) 100 * ix / m_filesList.count());
        ix++;
    }

    // Add project file
    archive.addLocalFile(m_temp->fileName(), m_name + ".kdenlive");
    bool result = archive.close();
    delete m_temp;
    emit archivingFinished(result);
}

void ArchiveWidget::slotArchivingFinished(bool result)
{
    if (result) {
        slotJobResult(true, i18n("Project was successfully archived."));
        buttonBox->button(QDialogButtonBox::Apply)->setEnabled(false);
    }
    else {
        slotJobResult(false, i18n("There was an error processing project file"));
    }
    progressBar->setValue(100);
    buttonBox->button(QDialogButtonBox::Apply)->setText(i18n("Archive"));
    archive_url->setEnabled(true);
    proxy_only->setEnabled(true);
    compressed_archive->setEnabled(true);
    for (int i = 0; i < files_list->topLevelItemCount(); i++) {
        files_list->topLevelItem(i)->setDisabled(false);
        for (int j = 0; j < files_list->topLevelItem(i)->childCount(); j++)
            files_list->topLevelItem(i)->child(j)->setDisabled(false);
    }
}

void ArchiveWidget::slotArchivingProgress(int p)
{
    progressBar->setValue(p);
}

void ArchiveWidget::slotStartExtracting()
{
    if (m_archiveThread.isRunning()) {
        //TODO: abort extracting
        return;
    }
    QFileInfo f(m_extractUrl.path());
    m_requestedSize = f.size();
    KIO::NetAccess::mkdir(archive_url->url().path(KUrl::RemoveTrailingSlash), this);
    slotDisplayMessage("system-run", i18n("Extracting..."));
    buttonBox->button(QDialogButtonBox::Apply)->setText(i18n("Abort"));
    m_progressTimer = new QTimer;
    m_progressTimer->setInterval(800);
    m_progressTimer->setSingleShot(false);
    connect(m_progressTimer, SIGNAL(timeout()), this, SLOT(slotExtractProgress()));
    m_archiveThread = QtConcurrent::run(this, &ArchiveWidget::doExtracting);
    m_progressTimer->start();
}

void ArchiveWidget::slotExtractProgress()
{
    KIO::DirectorySizeJob *job = KIO::directorySize(archive_url->url());
    connect(job, SIGNAL(result(KJob*)), this, SLOT(slotGotProgress(KJob*)));
}

void ArchiveWidget::slotGotProgress(KJob* job)
{
    if (!job->error()) {
        KIO::DirectorySizeJob *j = static_cast <KIO::DirectorySizeJob *>(job);
        progressBar->setValue((int) 100 * j->totalSize() / m_requestedSize);
    }
    job->deleteLater();
}

void ArchiveWidget::doExtracting()
{
    m_extractArchive->directory()->copyTo(archive_url->url().path(KUrl::AddTrailingSlash));
    m_extractArchive->close();
    emit extractingFinished();    
}

QString ArchiveWidget::extractedProjectFile()
{
    return archive_url->url().path(KUrl::AddTrailingSlash) + m_projectName;
}

void ArchiveWidget::slotExtractingFinished()
{
    m_progressTimer->stop();
    delete m_progressTimer;
    // Process project file
    QFile file(extractedProjectFile());
    bool error = false;
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        error = true;
    }
    else {
        QString playList = file.readAll();
        file.close();
        if (playList.isEmpty()) {
            error = true;
        }
        else {
            playList.replace("$CURRENTPATH", archive_url->url().path(KUrl::RemoveTrailingSlash));
            if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
                kWarning() << "//////  ERROR writing to file: ";
                error = true;
            }
            else {
                file.write(playList.toUtf8());
                if (file.error() != QFile::NoError) {
                    error = true;
                }
                file.close();
            }
        }
    }
    if (error) {
        KMessageBox::sorry(kapp->activeWindow(), i18n("Cannot open project file %1", extractedProjectFile()), i18n("Cannot open file"));
        reject();
    }
    else accept();
}

void ArchiveWidget::slotProxyOnly(int onlyProxy)
{
    m_requestedSize = 0;
    if (onlyProxy == Qt::Checked) {
        // Archive proxy clips
        QStringList proxyIdList;
        QTreeWidgetItem *parentItem = NULL;

        // Build list of existing proxy ids
        for (int i = 0; i < files_list->topLevelItemCount(); i++) {
            parentItem = files_list->topLevelItem(i);
            if (parentItem->data(0, Qt::UserRole).toString() == "proxy") break;
        }
        if (!parentItem) return;
        int items = parentItem->childCount();
        for (int j = 0; j < items; j++) {
            proxyIdList << parentItem->child(j)->data(0, Qt::UserRole + 2).toString();
        }
        
        // Parse all items to disable original clips for existing proxies
        for (int i = 0; i < proxyIdList.count(); i++) {
            QString id = proxyIdList.at(i);
            if (id.isEmpty()) continue;
            for (int j = 0; j < files_list->topLevelItemCount(); j++) {
                parentItem = files_list->topLevelItem(j);
                if (parentItem->data(0, Qt::UserRole).toString() == "proxy") continue;
                items = parentItem->childCount();
                for (int k = 0; k < items; k++) {
                    if (parentItem->child(k)->data(0, Qt::UserRole + 2).toString() == id) {
                        // This item has a proxy, do not archive it
                        parentItem->child(k)->setFlags(Qt::ItemIsSelectable);
                        break;
                    }
                }
            }
        }
    }
    else {
        // Archive all clips
        for (int i = 0; i < files_list->topLevelItemCount(); i++) {
            QTreeWidgetItem *parentItem = files_list->topLevelItem(i);
            int items = parentItem->childCount();
            for (int j = 0; j < items; j++) {
                parentItem->child(j)->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
            }
        }
    }
    
    // Calculate requested size
    int total = 0;
    for (int i = 0; i < files_list->topLevelItemCount(); i++) {
        QTreeWidgetItem *parentItem = files_list->topLevelItem(i);
        int items = parentItem->childCount();
        int itemsCount = 0;
        bool isSlideshow = parentItem->data(0, Qt::UserRole).toString() == "slideshows";
        
        for (int j = 0; j < items; j++) {
            if (!parentItem->child(j)->isDisabled()) {
                m_requestedSize += parentItem->child(j)->data(0, Qt::UserRole + 3).toInt();
                if (isSlideshow) total += parentItem->child(j)->data(0, Qt::UserRole + 1).toStringList().count();
                else total ++;
                itemsCount ++;
            }
        }
        parentItem->setText(0, parentItem->text(0).section('(', 0, 0) + i18np("(%1 item)", "(%1 items)", itemsCount));
    }
    project_files->setText(i18np("%1 file to archive, requires %2", "%1 files to archive, requires %2", total, KIO::convertSize(m_requestedSize)));
    slotCheckSpace();
}

