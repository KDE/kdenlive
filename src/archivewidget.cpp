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
#include <KApplication>
#include <KIO/NetAccess>

#include <KDebug>
#include <QTreeWidget>
#include "projectsettings.h"


ArchiveWidget::ArchiveWidget(QDomDocument doc, QList <DocClipBase*> list, QStringList luma_list, QWidget * parent) :
        QDialog(parent),
        m_requestedSize(0),
        m_copyJob(NULL),
        m_doc(doc)
{
    setupUi(this);
    setWindowTitle(i18n("Archive Project"));
    archive_url->setUrl(KUrl(QDir::homePath()));
    connect(archive_url, SIGNAL(textChanged (const QString &)), this, SLOT(slotCheckSpace()));

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
    generateItems(lumas, luma_list);

    QStringList slideUrls;
    QStringList audioUrls;
    QStringList videoUrls;
    QStringList imageUrls;
    QStringList otherUrls;
    QStringList proxyUrls;

    for (int i = 0; i < list.count(); i++) {
        DocClipBase *clip = list.at(i);
        CLIPTYPE t = clip->clipType();
        if (t == SLIDESHOW) {
            KUrl slideUrl = clip->fileURL();
            //TODO: Slideshow files
            slideUrls << slideUrl.path();
        }
        else if (t == IMAGE) imageUrls << clip->fileURL().path();
        else if (t == TEXT) {
            QStringList imagefiles = TitleWidget::extractImageList(clip->getProperty("xmldata"));
            QStringList fonts = TitleWidget::extractFontList(clip->getProperty("xmldata"));
            imageUrls << imagefiles;
            allFonts << fonts;
        } else if (t == PLAYLIST) {
            QStringList files = ProjectSettings::extractPlaylistUrls(clip->fileURL().path());
            otherUrls << files;
        }
        else if (!clip->fileURL().isEmpty()) {
            if (t == AUDIO) audioUrls << clip->fileURL().path();
            else {
                videoUrls << clip->fileURL().path();
                // Check if we have a proxy
                QString proxy = clip->getProperty("proxy");
                if (!proxy.isEmpty() && proxy != "-" && QFile::exists(proxy)) proxyUrls << proxy;
            }
        }
    }

    generateItems(sounds, audioUrls);
    generateItems(videos, videoUrls);
    generateItems(images, imageUrls);
    generateItems(slideshows, slideUrls);
    generateItems(others, otherUrls);
    generateItems(proxies, proxyUrls);
    
#if QT_VERSION >= 0x040500
    allFonts.removeDuplicates();
#endif

    //TODO: fonts

    // Hide unused categories, add item count
    int total = 0;
    for (int i = 0; i < files_list->topLevelItemCount(); i++) {
        int items = files_list->topLevelItem(i)->childCount();
        if (items == 0) {
            files_list->topLevelItem(i)->setHidden(true);
        }
        else {
            total += items;
            files_list->topLevelItem(i)->setText(0, files_list->topLevelItem(i)->text(0) + " " + i18np("(%1 item)", "(%1 items)", items));
        }
    }
    
    project_files->setText(i18n("%1 files to archive, requires %2", total, KIO::convertSize(m_requestedSize)));
    buttonBox->button(QDialogButtonBox::Apply)->setText(i18n("Archive"));
    connect(buttonBox->button(QDialogButtonBox::Apply), SIGNAL(clicked()), this, SLOT(slotStartArchiving()));
    buttonBox->button(QDialogButtonBox::Apply)->setEnabled(false);
    
    slotCheckSpace();
}

ArchiveWidget::~ArchiveWidget()
{
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
                    for (int i = 0; i < resultList.count(); i++) {
                        m_requestedSize += resultList.at(i).size();
                        slideImages << resultList.at(i).absoluteFilePath();
                    }
                    item->setData(0, Qt::UserRole + 1, slideImages);
            }
            else {
                // pattern url (like clip%.3d.png)
                QStringList result = dir.entryList(QDir::Files);
                QString filter = slideUrl.fileName();
                QString ext = filter.section('.', -1);
                filter = filter.section('%', 0, -2);
                QString regexp = "^" + filter + "\\d+\\." + ext + "$";
                QRegExp rx(regexp);
                QStringList slideImages;
                QString directory = dir.absolutePath();
                if (!directory.endsWith('/')) directory.append('/');
                foreach(const QString & path, result) {
                    if (rx.exactMatch(path)) {
                        m_requestedSize += QFileInfo(directory + path).size();
                        slideImages <<  directory + path;
                    }
                }
                item->setData(0, Qt::UserRole + 1, slideImages);
            }                    
        }
        else if (filesList.contains(fileName)) {
            // we have 2 files with same name
            int ix = 0;
            QString newFileName = fileName.section('.', 0, -2) + "_" + QString::number(ix) + "." + fileName.section('.', -1);
            while (filesList.contains(newFileName)) {
                ix ++;
                newFileName = fileName.section('.', 0, -2) + "_" + QString::number(ix) + "." + fileName.section('.', -1);
            }
            fileName = newFileName;
            item->setData(0, Qt::UserRole, fileName);
        }
        if (!isSlideshow) {
            m_requestedSize += QFileInfo(file).size();
            filesList << fileName;
        }
    }
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

bool ArchiveWidget::slotStartArchiving(bool firstPass)
{
    if (firstPass && m_copyJob) {
        // archiving in progress, abort
        m_copyJob->kill(KJob::EmitResult);
        return true;
    }
    if (!firstPass) m_copyJob = NULL;
    else {
        //starting archiving
        m_duplicateFiles.clear();
        m_replacementList.clear();
    }
    KUrl::List files;
    KUrl destUrl;
    QTreeWidgetItem *parentItem;
    bool isSlideshow = false;
    for (int i = 0; i < files_list->topLevelItemCount(); i++) {
        parentItem = files_list->topLevelItem(i);
        if (parentItem->data(0, Qt::UserRole).toString() == "slideshows") {
            KIO::NetAccess::mkdir(KUrl(archive_url->url().path(KUrl::AddTrailingSlash) + "slideshows"), this);
            isSlideshow = true;
        }
        if (parentItem->childCount() > 0 && !parentItem->isDisabled()) {
            files_list->setCurrentItem(parentItem);
            if (!isSlideshow) parentItem->setDisabled(true);
            destUrl = KUrl(archive_url->url().path(KUrl::AddTrailingSlash) + parentItem->data(0, Qt::UserRole).toString());
            QTreeWidgetItem *item;
            for (int j = 0; j < parentItem->childCount(); j++) {
                item = parentItem->child(j);
                // Special case: slideshows
                if (isSlideshow) {
                    if (item->isDisabled()) {
                        continue;
                    }
                    destUrl = KUrl(destUrl.path(KUrl::AddTrailingSlash) + item->data(0, Qt::UserRole).toString() + "/");
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
                    m_duplicateFiles.insert(KUrl(item->text(0)), KUrl(destUrl.path(KUrl::AddTrailingSlash) + item->data(0, Qt::UserRole).toString()));
                }
            }
            break;
        }
    }

    if (destUrl.isEmpty()) {
        if (m_duplicateFiles.isEmpty()) return false;        
        QMapIterator<KUrl, KUrl> i(m_duplicateFiles);
        KUrl startJob;
        if (i.hasNext()) {
            i.next();
            startJob = i.key();
            KIO::CopyJob *job = KIO::copyAs(startJob, i.value(), KIO::HideProgressInfo);
            connect(job, SIGNAL(result(KJob *)), this, SLOT(slotArchivingFinished(KJob *)));
            connect(job, SIGNAL(processedSize(KJob *, qulonglong)), this, SLOT(slotArchivingProgress(KJob *, qulonglong)));
            m_duplicateFiles.remove(startJob);
        }
        return true;
    }

    if (files.isEmpty()) slotStartArchiving(false);

    KIO::NetAccess::mkdir(destUrl, this);
    m_copyJob = KIO::copy (files, destUrl, KIO::HideProgressInfo);
    connect(m_copyJob, SIGNAL(result(KJob *)), this, SLOT(slotArchivingFinished(KJob *)));
    connect(m_copyJob, SIGNAL(processedSize(KJob *, qulonglong)), this, SLOT(slotArchivingProgress(KJob *, qulonglong)));

    if (firstPass) {
        progressBar->setValue(0);
        buttonBox->button(QDialogButtonBox::Apply)->setText(i18n("Abort"));
    }
    return true;
}

void ArchiveWidget::slotArchivingFinished(KJob *job)
{
    if (job->error() == 0) {
        if (slotStartArchiving(false)) {
            // We still have files to archive
            return;
        }
        else {
            // Archiving finished
            progressBar->setValue(100);
            if (processProjectFile()) {
                icon_info->setPixmap(KIcon("dialog-ok").pixmap(16, 16));
                text_info->setText(i18n("Project was successfully archived."));
            }
            else {
                icon_info->setPixmap(KIcon("dialog-close").pixmap(16, 16));
                text_info->setText(i18n("There was an error processing project file"));
            }
        }
    }
    else {
        m_copyJob = NULL;
        icon_info->setPixmap(KIcon("dialog-close").pixmap(16, 16));
        text_info->setText(i18n("There was an error while copying the files: %1", job->errorString()));
    }
    buttonBox->button(QDialogButtonBox::Apply)->setText(i18n("Archive"));
    for (int i = 0; i < files_list->topLevelItemCount(); i++) {
        files_list->topLevelItem(i)->setDisabled(false);
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
                    dest = KUrl(destUrl.path(KUrl::AddTrailingSlash) + item->data(0, Qt::UserRole).toString() + "/" + src.fileName());
                }
                else if (item->data(0, Qt::UserRole).isNull()) {
                    dest.addPath(src.fileName());
                }
                else {
                    dest.addPath(item->data(0, Qt::UserRole).toString());
                }
                m_replacementList.insert(src, dest);
                kDebug()<<"___ PROCESS ITEM :"<<src << "="<<dest;
            }
        }
    }
    
    // process kdenlive producers           
    QDomElement mlt = m_doc.documentElement();
    QString root = mlt.attribute("root") + "/";
    
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

    QString path = archive_url->url().path(KUrl::AddTrailingSlash) + "project.kdenlive";
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        kWarning() << "//////  ERROR writing to file: " << path;
        KMessageBox::error(kapp->activeWindow(), i18n("Cannot write to file %1", path));
        return false;
    }

    file.write(m_doc.toString().toUtf8());
    if (file.error() != QFile::NoError) {
        KMessageBox::error(kapp->activeWindow(), i18n("Cannot write to file %1", path));
        file.close();
        return false;
    }
    file.close();
    return true;
}

