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
#include "bin/bin.h"
#include "bin/projectclip.h"
#include "bin/projectfolder.h"
#include "bin/projectitemmodel.h"
#include "core.h"
#include "projectsettings.h"
#include "titler/titlewidget.h"
#include "xml/xml.hpp"

#include "kdenlive_debug.h"
#include "doc/kdenlivedoc.h"
#include <KDiskFreeSpaceInfo>
#include <KGuiItem>
#include <KMessageBox>
#include <KMessageWidget>
#include <KTar>
#include <kio/directorysizejob.h>
#include <klocalizedstring.h>

#include <QTreeWidget>
#include <QtConcurrent>
#include <utility>
ArchiveWidget::ArchiveWidget(const QString &projectName, const QString xmlData, const QStringList &luma_list, QWidget *parent)
    : QDialog(parent)
    , m_requestedSize(0)
    , m_copyJob(nullptr)
    , m_name(projectName.section(QLatin1Char('.'), 0, -2))
    , m_temp(nullptr)
    , m_abortArchive(false)
    , m_extractMode(false)
    , m_progressTimer(nullptr)
    , m_extractArchive(nullptr)
    , m_missingClips(0)
{
    setAttribute(Qt::WA_DeleteOnClose);
    setupUi(this);
    setWindowTitle(i18n("Archive Project"));
    archive_url->setUrl(QUrl::fromLocalFile(QDir::homePath()));
    connect(archive_url, &KUrlRequester::textChanged, this, &ArchiveWidget::slotCheckSpace);
    connect(this, &ArchiveWidget::archivingFinished, this, &ArchiveWidget::slotArchivingBoolFinished);
    connect(this, &ArchiveWidget::archiveProgress, this, &ArchiveWidget::slotArchivingIntProgress);
    connect(proxy_only, &QCheckBox::stateChanged, this, &ArchiveWidget::slotProxyOnly);

    // Prepare xml
    m_doc.setContent(xmlData);

    // Setup categories
    QTreeWidgetItem *videos = new QTreeWidgetItem(files_list, QStringList() << i18n("Video clips"));
    videos->setIcon(0, QIcon::fromTheme(QStringLiteral("video-x-generic")));
    videos->setData(0, Qt::UserRole, QStringLiteral("videos"));
    videos->setExpanded(false);
    QTreeWidgetItem *sounds = new QTreeWidgetItem(files_list, QStringList() << i18n("Audio clips"));
    sounds->setIcon(0, QIcon::fromTheme(QStringLiteral("audio-x-generic")));
    sounds->setData(0, Qt::UserRole, QStringLiteral("sounds"));
    sounds->setExpanded(false);
    QTreeWidgetItem *images = new QTreeWidgetItem(files_list, QStringList() << i18n("Image clips"));
    images->setIcon(0, QIcon::fromTheme(QStringLiteral("image-x-generic")));
    images->setData(0, Qt::UserRole, QStringLiteral("images"));
    images->setExpanded(false);
    QTreeWidgetItem *slideshows = new QTreeWidgetItem(files_list, QStringList() << i18n("Slideshow clips"));
    slideshows->setIcon(0, QIcon::fromTheme(QStringLiteral("image-x-generic")));
    slideshows->setData(0, Qt::UserRole, QStringLiteral("slideshows"));
    slideshows->setExpanded(false);
    QTreeWidgetItem *texts = new QTreeWidgetItem(files_list, QStringList() << i18n("Text clips"));
    texts->setIcon(0, QIcon::fromTheme(QStringLiteral("text-plain")));
    texts->setData(0, Qt::UserRole, QStringLiteral("texts"));
    texts->setExpanded(false);
    QTreeWidgetItem *playlists = new QTreeWidgetItem(files_list, QStringList() << i18n("Playlist clips"));
    playlists->setIcon(0, QIcon::fromTheme(QStringLiteral("video-mlt-playlist")));
    playlists->setData(0, Qt::UserRole, QStringLiteral("playlist"));
    playlists->setExpanded(false);
    QTreeWidgetItem *others = new QTreeWidgetItem(files_list, QStringList() << i18n("Other clips"));
    others->setIcon(0, QIcon::fromTheme(QStringLiteral("unknown")));
    others->setData(0, Qt::UserRole, QStringLiteral("others"));
    others->setExpanded(false);
    QTreeWidgetItem *lumas = new QTreeWidgetItem(files_list, QStringList() << i18n("Luma files"));
    lumas->setIcon(0, QIcon::fromTheme(QStringLiteral("image-x-generic")));
    lumas->setData(0, Qt::UserRole, QStringLiteral("lumas"));
    lumas->setExpanded(false);

    QTreeWidgetItem *proxies = new QTreeWidgetItem(files_list, QStringList() << i18n("Proxy clips"));
    proxies->setIcon(0, QIcon::fromTheme(QStringLiteral("video-x-generic")));
    proxies->setData(0, Qt::UserRole, QStringLiteral("proxy"));
    proxies->setExpanded(false);

    // process all files
    QStringList allFonts;
    QStringList extraImageUrls;
    QStringList otherUrls;
    generateItems(lumas, luma_list);

    QMap<QString, QString> slideUrls;
    QMap<QString, QString> audioUrls;
    QMap<QString, QString> videoUrls;
    QMap<QString, QString> imageUrls;
    QMap<QString, QString> playlistUrls;
    QMap<QString, QString> proxyUrls;
    QList<std::shared_ptr<ProjectClip>> clipList = pCore->projectItemModel()->getRootFolder()->childClips();
    for (const std::shared_ptr<ProjectClip> &clip : qAsConst(clipList)) {
        ClipType::ProducerType t = clip->clipType();
        QString id = clip->binId();
        if (t == ClipType::Color) {
            continue;
        }
        if (t == ClipType::SlideShow) {
            // TODO: Slideshow files
            slideUrls.insert(id, clip->clipUrl());
        } else if (t == ClipType::Image) {
            imageUrls.insert(id, clip->clipUrl());
        } else if (t == ClipType::QText) {
            allFonts << clip->getProducerProperty(QStringLiteral("family"));
        } else if (t == ClipType::Text) {
            QStringList imagefiles = TitleWidget::extractImageList(clip->getProducerProperty(QStringLiteral("xmldata")));
            QStringList fonts = TitleWidget::extractFontList(clip->getProducerProperty(QStringLiteral("xmldata")));
            extraImageUrls << imagefiles;
            allFonts << fonts;
        } else if (t == ClipType::Playlist) {
            playlistUrls.insert(id, clip->clipUrl());
            QStringList files = ProjectSettings::extractPlaylistUrls(clip->clipUrl());
            otherUrls << files;
        } else if (!clip->clipUrl().isEmpty()) {
            if (t == ClipType::Audio) {
                audioUrls.insert(id, clip->clipUrl());
            } else {
                videoUrls.insert(id, clip->clipUrl());
                // Check if we have a proxy
                QString proxy = clip->getProducerProperty(QStringLiteral("kdenlive:proxy"));
                if (!proxy.isEmpty() && proxy != QLatin1String("-") && QFile::exists(proxy)) {
                    proxyUrls.insert(id, proxy);
                }
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

    m_infoMessage = new KMessageWidget(this);
    auto *s = static_cast<QVBoxLayout *>(layout());
    s->insertWidget(5, m_infoMessage);
    m_infoMessage->setCloseButtonVisible(false);
    m_infoMessage->setWordWrap(true);
    m_infoMessage->hide();

    // missing clips, warn user
    if (m_missingClips > 0) {
        QString infoText = i18np("You have %1 missing clip in your project.", "You have %1 missing clips in your project.", m_missingClips);
        m_infoMessage->setMessageType(KMessageWidget::Warning);
        m_infoMessage->setText(infoText);
        m_infoMessage->animatedShow();
    }

    // TODO: fonts

    // Hide unused categories, add item count
    int total = 0;
    for (int i = 0; i < files_list->topLevelItemCount(); ++i) {
        QTreeWidgetItem *parentItem = files_list->topLevelItem(i);
        int items = parentItem->childCount();
        if (items == 0) {
            files_list->topLevelItem(i)->setHidden(true);
        } else {
            if (parentItem->data(0, Qt::UserRole).toString() == QLatin1String("slideshows")) {
                // Special case: slideshows contain several files
                for (int j = 0; j < items; ++j) {
                    total += parentItem->child(j)->data(0, Qt::UserRole + 1).toStringList().count();
                }
            } else {
                total += items;
            }
            parentItem->setText(0, files_list->topLevelItem(i)->text(0) + QLatin1Char(' ') + i18np("(%1 item)", "(%1 items)", items));
        }
    }
    if (m_name.isEmpty()) {
        m_name = i18n("Untitled");
    }
    compressed_archive->setText(compressed_archive->text() + QStringLiteral(" (") + m_name + QStringLiteral(".tar.gz)"));
    project_files->setText(i18np("%1 file to archive, requires %2", "%1 files to archive, requires %2", total, KIO::convertSize(m_requestedSize)));
    buttonBox->button(QDialogButtonBox::Apply)->setText(i18n("Archive"));
    connect(buttonBox->button(QDialogButtonBox::Apply), &QAbstractButton::clicked, this, &ArchiveWidget::slotStartArchiving);
    buttonBox->button(QDialogButtonBox::Apply)->setEnabled(false);

    slotCheckSpace();

    // Validate some basic project properties
    QDomElement mlt = m_doc.documentElement();
    QDomNodeList tracks = mlt.elementsByTagName(QStringLiteral("track"));
    if (tracks.size() == 0 || !xmlData.contains(QStringLiteral("kdenlive:docproperties.version"))) {
        m_infoMessage->setMessageType(KMessageWidget::Warning);
        m_infoMessage->setText(i18n("There was an error processing project file"));
        m_infoMessage->animatedShow();
        buttonBox->button(QDialogButtonBox::Apply)->setEnabled(false);
    }
}

// Constructor for extract widget
ArchiveWidget::ArchiveWidget(QUrl url, QWidget *parent)
    : QDialog(parent)
    , m_requestedSize(0)
    , m_copyJob(nullptr)
    , m_temp(nullptr)
    , m_abortArchive(false)
    , m_extractMode(true)
    , m_extractUrl(std::move(url))
    , m_extractArchive(nullptr)
    , m_missingClips(0)
    , m_infoMessage(nullptr)
{
    // setAttribute(Qt::WA_DeleteOnClose);

    setupUi(this);
    m_progressTimer = new QTimer;
    m_progressTimer->setInterval(800);
    m_progressTimer->setSingleShot(false);
    connect(m_progressTimer, &QTimer::timeout, this, &ArchiveWidget::slotExtractProgress);
    connect(this, &ArchiveWidget::extractingFinished, this, &ArchiveWidget::slotExtractingFinished);
    connect(this, &ArchiveWidget::showMessage, this, &ArchiveWidget::slotDisplayMessage);

    compressed_archive->setHidden(true);
    proxy_only->setHidden(true);
    project_files->setHidden(true);
    files_list->setHidden(true);
    label->setText(i18n("Extract to"));
    setWindowTitle(i18n("Open Archived Project"));
    archive_url->setUrl(QUrl::fromLocalFile(QDir::homePath()));
    buttonBox->button(QDialogButtonBox::Apply)->setText(i18n("Extract"));
    connect(buttonBox->button(QDialogButtonBox::Apply), &QAbstractButton::clicked, this, &ArchiveWidget::slotStartExtracting);
    buttonBox->button(QDialogButtonBox::Apply)->setEnabled(true);
    adjustSize();
    m_archiveThread = QtConcurrent::run(this, &ArchiveWidget::openArchiveForExtraction);
}

ArchiveWidget::~ArchiveWidget()
{
    delete m_extractArchive;
    delete m_progressTimer;
}

void ArchiveWidget::slotDisplayMessage(const QString &icon, const QString &text)
{
    icon_info->setPixmap(QIcon::fromTheme(icon).pixmap(16, 16));
    text_info->setText(text);
}

void ArchiveWidget::slotJobResult(bool success, const QString &text)
{
    m_infoMessage->setMessageType(success ? KMessageWidget::Positive : KMessageWidget::Warning);
    m_infoMessage->setText(text);
    m_infoMessage->animatedShow();
}

void ArchiveWidget::openArchiveForExtraction()
{
    emit showMessage(QStringLiteral("system-run"), i18n("Opening archive..."));
    m_extractArchive = new KTar(m_extractUrl.toLocalFile());
    if (!m_extractArchive->isOpen() && !m_extractArchive->open(QIODevice::ReadOnly)) {
        emit showMessage(QStringLiteral("dialog-close"), i18n("Cannot open archive file:\n %1", m_extractUrl.toLocalFile()));
        groupBox->setEnabled(false);
        return;
    }

    // Check that it is a kdenlive project archive
    bool isProjectArchive = false;
    QStringList files = m_extractArchive->directory()->entries();
    for (int i = 0; i < files.count(); ++i) {
        if (files.at(i).endsWith(QLatin1String(".kdenlive"))) {
            m_projectName = files.at(i);
            isProjectArchive = true;
            break;
        }
    }

    if (!isProjectArchive) {
        emit showMessage(QStringLiteral("dialog-close"), i18n("File %1\n is not an archived Kdenlive project", m_extractUrl.toLocalFile()));
        groupBox->setEnabled(false);
        buttonBox->button(QDialogButtonBox::Apply)->setEnabled(false);
        return;
    }
    buttonBox->button(QDialogButtonBox::Apply)->setEnabled(true);
    emit showMessage(QStringLiteral("dialog-ok"), i18n("Ready"));
}

void ArchiveWidget::done(int r)
{
    if (closeAccepted()) {
        QDialog::done(r);
    }
}

void ArchiveWidget::closeEvent(QCloseEvent *e)
{

    if (closeAccepted()) {
        e->accept();
    } else {
        e->ignore();
    }
}

bool ArchiveWidget::closeAccepted()
{
    if (!m_extractMode && !archive_url->isEnabled()) {
        // Archiving in progress, should we stop?
        if (KMessageBox::warningContinueCancel(this, i18n("Archiving in progress, do you want to stop it?"), i18n("Stop Archiving"),
                                               KGuiItem(i18n("Stop Archiving"))) != KMessageBox::Continue) {
            return false;
        }
        if (m_copyJob) {
            m_copyJob->kill();
        }
    }
    return true;
}

void ArchiveWidget::generateItems(QTreeWidgetItem *parentItem, const QStringList &items)
{
    QStringList filesList;
    QString fileName;
    int ix = 0;
    bool isSlideshow = parentItem->data(0, Qt::UserRole).toString() == QLatin1String("slideshows");
    for (const QString &file : items) {
        QTreeWidgetItem *item = new QTreeWidgetItem(parentItem, QStringList() << file);
        fileName = QUrl::fromLocalFile(file).fileName();
        if (isSlideshow) {
            // we store each slideshow in a separate subdirectory
            item->setData(0, Qt::UserRole, ix);
            ix++;
            QUrl slideUrl = QUrl::fromLocalFile(file);
            QDir dir(slideUrl.adjusted(QUrl::RemoveFilename).toLocalFile());
            if (slideUrl.fileName().startsWith(QLatin1String(".all."))) {
                // MIME type slideshow (for example *.png)
                QStringList filters;
                // TODO: improve jpeg image detection with extension like jpeg, requires change in MLT image producers
                filters << QStringLiteral("*.") + slideUrl.fileName().section(QLatin1Char('.'), -1);
                dir.setNameFilters(filters);
                QFileInfoList resultList = dir.entryInfoList(QDir::Files);
                QStringList slideImages;
                qint64 totalSize = 0;
                for (int i = 0; i < resultList.count(); ++i) {
                    totalSize += resultList.at(i).size();
                    slideImages << resultList.at(i).absoluteFilePath();
                }
                item->setData(0, Qt::UserRole + 1, slideImages);
                item->setData(0, Qt::UserRole + 3, totalSize);
                m_requestedSize += static_cast<KIO::filesize_t>(totalSize);
            } else {
                // pattern url (like clip%.3d.png)
                QStringList result = dir.entryList(QDir::Files);
                QString filter = slideUrl.fileName();
                QString ext = filter.section(QLatin1Char('.'), -1);
                filter = filter.section(QLatin1Char('%'), 0, -2);
                QString regexp = QLatin1Char('^') + filter + QStringLiteral("\\d+\\.") + ext + QLatin1Char('$');
                QRegExp rx(regexp);
                QStringList slideImages;
                QString directory = dir.absolutePath();
                if (!directory.endsWith(QLatin1Char('/'))) {
                    directory.append(QLatin1Char('/'));
                }
                qint64 totalSize = 0;
                for (const QString &path : qAsConst(result)) {
                    if (rx.exactMatch(path)) {
                        totalSize += QFileInfo(directory + path).size();
                        slideImages << directory + path;
                    }
                }
                item->setData(0, Qt::UserRole + 1, slideImages);
                item->setData(0, Qt::UserRole + 3, totalSize);
                m_requestedSize += static_cast<KIO::filesize_t>(totalSize);
            }
        } else if (filesList.contains(fileName)) {
            // we have 2 files with same name
            int i = 0;
            QString newFileName =
                fileName.section(QLatin1Char('.'), 0, -2) + QLatin1Char('_') + QString::number(i) + QLatin1Char('.') + fileName.section(QLatin1Char('.'), -1);
            while (filesList.contains(newFileName)) {
                i++;
                newFileName = fileName.section(QLatin1Char('.'), 0, -2) + QLatin1Char('_') + QString::number(i) + QLatin1Char('.') +
                              fileName.section(QLatin1Char('.'), -1);
            }
            fileName = newFileName;
            item->setData(0, Qt::UserRole, fileName);
        }
        if (!isSlideshow) {
            qint64 fileSize = QFileInfo(file).size();
            if (fileSize <= 0) {
                item->setIcon(0, QIcon::fromTheme(QStringLiteral("edit-delete")));
                m_missingClips++;
            } else {
                m_requestedSize += static_cast<KIO::filesize_t>(fileSize);
                item->setData(0, Qt::UserRole + 3, fileSize);
            }
            filesList << fileName;
        }
    }
}

void ArchiveWidget::generateItems(QTreeWidgetItem *parentItem, const QMap<QString, QString> &items)
{
    QStringList filesList;
    QString fileName;
    int ix = 0;
    bool isSlideshow = parentItem->data(0, Qt::UserRole).toString() == QLatin1String("slideshows");
    QMap<QString, QString>::const_iterator it = items.constBegin();
    while (it != items.constEnd()) {
        QString file = it.value();
        QTreeWidgetItem *item = new QTreeWidgetItem(parentItem, QStringList() << file);
        // Store the clip's id
        item->setData(0, Qt::UserRole + 2, it.key());
        fileName = QUrl::fromLocalFile(file).fileName();
        if (isSlideshow) {
            // we store each slideshow in a separate subdirectory
            item->setData(0, Qt::UserRole, ix);
            ix++;
            QUrl slideUrl = QUrl::fromLocalFile(file);
            QDir dir(slideUrl.adjusted(QUrl::RemoveFilename).toLocalFile());
            if (slideUrl.fileName().startsWith(QLatin1String(".all."))) {
                // MIME type slideshow (for example *.png)
                QStringList filters;
                // TODO: improve jpeg image detection with extension like jpeg, requires change in MLT image producers
                filters << QStringLiteral("*.") + slideUrl.fileName().section(QLatin1Char('.'), -1);
                dir.setNameFilters(filters);
                QFileInfoList resultList = dir.entryInfoList(QDir::Files);
                QStringList slideImages;
                qint64 totalSize = 0;
                for (int i = 0; i < resultList.count(); ++i) {
                    totalSize += resultList.at(i).size();
                    slideImages << resultList.at(i).absoluteFilePath();
                }
                item->setData(0, Qt::UserRole + 1, slideImages);
                item->setData(0, Qt::UserRole + 3, totalSize);
                m_requestedSize += static_cast<KIO::filesize_t>(totalSize);
            } else {
                // pattern url (like clip%.3d.png)
                QStringList result = dir.entryList(QDir::Files);
                QString filter = slideUrl.fileName();
                QString ext = filter.section(QLatin1Char('.'), -1).section(QLatin1Char('?'), 0, 0);
                filter = filter.section(QLatin1Char('%'), 0, -2);
                QString regexp = QLatin1Char('^') + filter + QStringLiteral("\\d+\\.") + ext + QLatin1Char('$');
                QRegExp rx(regexp);
                QStringList slideImages;
                qint64 totalSize = 0;
                for (const QString &path : qAsConst(result)) {
                    if (rx.exactMatch(path)) {
                        totalSize += QFileInfo(dir.absoluteFilePath(path)).size();
                        slideImages << dir.absoluteFilePath(path);
                    }
                }
                item->setData(0, Qt::UserRole + 1, slideImages);
                item->setData(0, Qt::UserRole + 3, totalSize);
                m_requestedSize += static_cast<KIO::filesize_t>(totalSize);
            }
        } else if (filesList.contains(fileName)) {
            // we have 2 files with same name
            int index2 = 0;
            QString newFileName = fileName.section(QLatin1Char('.'), 0, -2) + QLatin1Char('_') + QString::number(index2) + QLatin1Char('.') +
                                  fileName.section(QLatin1Char('.'), -1);
            while (filesList.contains(newFileName)) {
                index2++;
                newFileName = fileName.section(QLatin1Char('.'), 0, -2) + QLatin1Char('_') + QString::number(index2) + QLatin1Char('.') +
                              fileName.section(QLatin1Char('.'), -1);
            }
            fileName = newFileName;
            item->setData(0, Qt::UserRole, fileName);
        }
        if (!isSlideshow) {
            qint64 fileSize = QFileInfo(file).size();
            if (fileSize <= 0) {
                item->setIcon(0, QIcon::fromTheme(QStringLiteral("edit-delete")));
                m_missingClips++;
            } else {
                m_requestedSize += static_cast<KIO::filesize_t>(fileSize);
                item->setData(0, Qt::UserRole + 3, fileSize);
            }
            filesList << fileName;
        }
        ++it;
    }
}

void ArchiveWidget::slotCheckSpace()
{
    KDiskFreeSpaceInfo inf = KDiskFreeSpaceInfo::freeSpaceInfo(archive_url->url().toLocalFile());
    KIO::filesize_t freeSize = inf.available();
    if (freeSize > m_requestedSize) {
        // everything is ok
        buttonBox->button(QDialogButtonBox::Apply)->setEnabled(true);
        slotDisplayMessage(QStringLiteral("dialog-ok"), i18n("Available space on drive: %1", KIO::convertSize(freeSize)));
    } else {
        buttonBox->button(QDialogButtonBox::Apply)->setEnabled(false);
        slotDisplayMessage(QStringLiteral("dialog-close"), i18n("Not enough space on drive, free space: %1", KIO::convertSize(freeSize)));
    }
}

bool ArchiveWidget::slotStartArchiving(bool firstPass)
{
    if (firstPass && ((m_copyJob != nullptr) || m_archiveThread.isRunning())) {
        // archiving in progress, abort
        if (m_copyJob) {
            m_copyJob->kill(KJob::EmitResult);
        }
        m_abortArchive = true;
        return true;
    }
    bool isArchive = compressed_archive->isChecked();
    if (!firstPass) {
        m_copyJob = nullptr;
    } else {
        // starting archiving
        m_abortArchive = false;
        m_duplicateFiles.clear();
        m_replacementList.clear();
        m_foldersList.clear();
        m_filesList.clear();
        slotDisplayMessage(QStringLiteral("system-run"), i18n("Archiving..."));
        repaint();
        archive_url->setEnabled(false);
        proxy_only->setEnabled(false);
        compressed_archive->setEnabled(false);
    }
    QList<QUrl> files;
    QUrl destUrl;
    QString destPath;
    QTreeWidgetItem *parentItem;
    bool isSlideshow = false;
    int items = 0;

    // We parse all files going into one folder, then start the copy job
    for (int i = 0; i < files_list->topLevelItemCount(); ++i) {
        parentItem = files_list->topLevelItem(i);
        if (parentItem->isDisabled()) {
            parentItem->setExpanded(false);
            continue;
        }
        if (parentItem->childCount() > 0) {
            if (parentItem->data(0, Qt::UserRole).toString() == QLatin1String("slideshows")) {
                QUrl slideFolder = QUrl::fromLocalFile(archive_url->url().toLocalFile() + QStringLiteral("/slideshows"));
                if (isArchive) {
                    m_foldersList.append(QStringLiteral("slideshows"));
                } else {
                    QDir dir(slideFolder.toLocalFile());
                    if (!dir.mkpath(QStringLiteral("."))) {
                        KMessageBox::sorry(this, i18n("Cannot create directory %1", slideFolder.toLocalFile()));
                    }
                }
                isSlideshow = true;
            } else {
                isSlideshow = false;
            }
            files_list->setCurrentItem(parentItem);
            parentItem->setExpanded(true);
            destPath = parentItem->data(0, Qt::UserRole).toString() + QLatin1Char('/');
            destUrl = QUrl::fromLocalFile(archive_url->url().toLocalFile() + QLatin1Char('/') + destPath);
            QTreeWidgetItem *item;
            for (int j = 0; j < parentItem->childCount(); ++j) {
                item = parentItem->child(j);
                if (item->isDisabled()) {
                    continue;
                }
                // Special case: slideshows
                items++;
                if (isSlideshow) {
                    destPath += item->data(0, Qt::UserRole).toString() + QLatin1Char('/');
                    destUrl = QUrl::fromLocalFile(archive_url->url().toLocalFile() + QDir::separator() + destPath);
                    QStringList srcFiles = item->data(0, Qt::UserRole + 1).toStringList();
                    for (int k = 0; k < srcFiles.count(); ++k) {
                        files << QUrl::fromLocalFile(srcFiles.at(k));
                    }
                    item->setDisabled(true);
                    if (parentItem->indexOfChild(item) == parentItem->childCount() - 1) {
                        // We have processed all slideshows
                        parentItem->setDisabled(true);
                    }
                    break;
                } else if (item->data(0, Qt::UserRole).isNull()) {
                    files << QUrl::fromLocalFile(item->text(0));
                } else {
                    // We must rename the destination file, since another file with same name exists
                    // TODO: monitor progress
                    if (isArchive) {
                        m_filesList.insert(item->text(0), destPath + item->data(0, Qt::UserRole).toString());
                    } else {
                        m_duplicateFiles.insert(QUrl::fromLocalFile(item->text(0)),
                                                QUrl::fromLocalFile(destUrl.toLocalFile() + QLatin1Char('/') + item->data(0, Qt::UserRole).toString()));
                    }
                }
            }
            if (!isSlideshow) {
                parentItem->setDisabled(true);
            }
            break;
        }
    }

    if (items == 0) {
        // No clips to archive
        slotArchivingFinished(nullptr, true);
        return true;
    }

    if (destPath.isEmpty()) {
        if (m_duplicateFiles.isEmpty()) {
            return false;
        }
        QMapIterator<QUrl, QUrl> i(m_duplicateFiles);
        if (i.hasNext()) {
            i.next();
            QUrl startJobSrc = i.key();
            QUrl startJobDst = i.value();
            m_duplicateFiles.remove(startJobSrc);
            KIO::CopyJob *job = KIO::copyAs(startJobSrc, startJobDst, KIO::HideProgressInfo);
            connect(job, &KJob::result, this, [this] (KJob *jb) {
                slotArchivingFinished(jb, false);
            });
            connect(job, &KJob::processedSize, this, &ArchiveWidget::slotArchivingProgress);
        }
        return true;
    }

    if (isArchive) {
        m_foldersList.append(destPath);
        for (int i = 0; i < files.count(); ++i) {
            m_filesList.insert(files.at(i).toLocalFile(), destPath + files.at(i).fileName());
        }
        slotArchivingFinished();
    } else if (files.isEmpty()) {
        slotStartArchiving(false);
    } else {
        QDir dir(destUrl.toLocalFile());
        if (!dir.mkpath(QStringLiteral("."))) {
            KMessageBox::sorry(this, i18n("Cannot create directory %1", destUrl.toLocalFile()));
        }
        m_copyJob = KIO::copy(files, destUrl, KIO::HideProgressInfo);
        connect(m_copyJob, &KJob::result, this, [this] (KJob *jb) {
            slotArchivingFinished(jb, false);
        });
        connect(m_copyJob, &KJob::processedSize, this, &ArchiveWidget::slotArchivingProgress);
    }
    if (firstPass) {
        progressBar->setValue(0);
        buttonBox->button(QDialogButtonBox::Apply)->setText(i18n("Abort"));
    }
    return true;
}

void ArchiveWidget::slotArchivingFinished(KJob *job, bool finished)
{
    if (job == nullptr || job->error() == 0) {
        if (!finished && slotStartArchiving(false)) {
            // We still have files to archive
            return;
        }
        if (!compressed_archive->isChecked()) {
            // Archiving finished
            progressBar->setValue(100);
            if (processProjectFile()) {
                slotJobResult(true, i18n("Project was successfully archived."));
            } else {
                slotJobResult(false, i18n("There was an error processing project file"));
            }
        } else {
            processProjectFile();
        }
    } else {
        m_copyJob = nullptr;
        slotJobResult(false, i18n("There was an error while copying the files: %1", job->errorString()));
    }
    if (!compressed_archive->isChecked()) {
        buttonBox->button(QDialogButtonBox::Apply)->setText(i18n("Archive"));
        archive_url->setEnabled(true);
        proxy_only->setEnabled(true);
        compressed_archive->setEnabled(true);
        for (int i = 0; i < files_list->topLevelItemCount(); ++i) {
            files_list->topLevelItem(i)->setDisabled(false);
            for (int j = 0; j < files_list->topLevelItem(i)->childCount(); ++j) {
                files_list->topLevelItem(i)->child(j)->setDisabled(false);
            }
        }
    }
}

void ArchiveWidget::slotArchivingProgress(KJob *, qulonglong size)
{
    progressBar->setValue(static_cast<int>(100 * size / m_requestedSize));
}

bool ArchiveWidget::processProjectFile()
{
    QTreeWidgetItem *item;
    bool isArchive = compressed_archive->isChecked();

    for (int i = 0; i < files_list->topLevelItemCount(); ++i) {
        QTreeWidgetItem *parentItem = files_list->topLevelItem(i);
        if (parentItem->childCount() > 0) {
            QDir destFolder(archive_url->url().toLocalFile() + QDir::separator() + parentItem->data(0, Qt::UserRole).toString());
            bool isSlideshow = parentItem->data(0, Qt::UserRole).toString() == QLatin1String("slideshows");
            for (int j = 0; j < parentItem->childCount(); ++j) {
                item = parentItem->child(j);
                QUrl src = QUrl::fromLocalFile(item->text(0));
                QUrl dest = QUrl::fromLocalFile(destFolder.absolutePath());
                if (isSlideshow) {
                    dest = QUrl::fromLocalFile(parentItem->data(0, Qt::UserRole).toString() + QLatin1Char('/') + item->data(0, Qt::UserRole).toString() +
                                               QLatin1Char('/') + src.fileName());
                } else if (item->data(0, Qt::UserRole).isNull()) {
                    dest = QUrl::fromLocalFile(parentItem->data(0, Qt::UserRole).toString() + QLatin1Char('/') + src.fileName());
                } else {
                    dest = QUrl::fromLocalFile(parentItem->data(0, Qt::UserRole).toString() + QLatin1Char('/') + item->data(0, Qt::UserRole).toString());
                }
                m_replacementList.insert(src, dest);
            }
        }
    }

    QDomElement mlt = m_doc.documentElement();
    QString root = mlt.attribute(QStringLiteral("root"));
    if (!root.isEmpty() && !root.endsWith(QLatin1Char('/'))) {
        root.append(QLatin1Char('/'));
    }

    // Adjust global settings
    QString basePath;
    if (isArchive) {
        basePath = QStringLiteral("$CURRENTPATH");
    } else {
        basePath = archive_url->url().adjusted(QUrl::StripTrailingSlash | QUrl::StripTrailingSlash).toLocalFile();
    }
    // Switch to relative path
    mlt.removeAttribute(QStringLiteral("root"));

    // process mlt producers
    QDomNodeList prods = mlt.elementsByTagName(QStringLiteral("producer"));
    for (int i = 0; i < prods.count(); ++i) {
        QDomElement e = prods.item(i).toElement();
        if (e.isNull()) {
            continue;
        }
        bool isTimewarp = Xml::getXmlProperty(e, QStringLiteral("mlt_service")) == QLatin1String("timewarp");
        QString src = Xml::getXmlProperty(e, QStringLiteral("resource"));
        if (!src.isEmpty()) {
            if (isTimewarp) {
                // Timewarp needs to be handled separately.
                src = Xml::getXmlProperty(e, QStringLiteral("warp_resource"));
            }
            if (QFileInfo(src).isRelative()) {
                src.prepend(root);
            }
            QUrl srcUrl = QUrl::fromLocalFile(src);
            QUrl dest = m_replacementList.value(srcUrl);
            if (!dest.isEmpty()) {
                if (isTimewarp) {
                    Xml::setXmlProperty(e, QStringLiteral("warp_resource"), dest.toLocalFile());
                    Xml::setXmlProperty(e, QStringLiteral("resource"), QString("%1:%2").arg(Xml::getXmlProperty(e, QStringLiteral("warp_speed")), dest.toLocalFile()));
                } else {
                    Xml::setXmlProperty(e, QStringLiteral("resource"), dest.toLocalFile());
                }
            }
        }
        src = Xml::getXmlProperty(e, QStringLiteral("kdenlive:proxy"));
        if (src.size() > 2) {
            if (QFileInfo(src).isRelative()) {
                src.prepend(root);
            }
            QUrl srcUrl = QUrl::fromLocalFile(src);
            QUrl dest = m_replacementList.value(srcUrl);
            if (!dest.isEmpty()) {
                Xml::setXmlProperty(e, QStringLiteral("kdenlive:proxy"), dest.toLocalFile());
            }
        }
        src = Xml::getXmlProperty(e, QStringLiteral("kdenlive:originalurl"));
        if (!src.isEmpty()) {
            if (QFileInfo(src).isRelative()) {
                src.prepend(root);
            }
            QUrl srcUrl = QUrl::fromLocalFile(src);
            QUrl dest = m_replacementList.value(srcUrl);
            if (!dest.isEmpty()) {
                Xml::setXmlProperty(e, QStringLiteral("kdenlive:originalurl"), dest.toLocalFile());
            }
        }
        src = Xml::getXmlProperty(e, QStringLiteral("xmldata"));
        bool found = false;
        if (!src.isEmpty() && (src.contains(QLatin1String("QGraphicsPixmapItem")) || src.contains(QLatin1String("QGraphicsSvgItem")))) {
            // Title with images, replace paths
            QDomDocument titleXML;
            titleXML.setContent(src);
            QDomNodeList images = titleXML.documentElement().elementsByTagName(QLatin1String("item"));
            for (int j = 0; j < images.count(); ++j) {
                QDomNode n = images.at(j);
                QDomElement url = n.firstChildElement(QLatin1String("content"));
                if (!url.isNull() && url.hasAttribute(QLatin1String("url"))) {
                    QUrl srcUrl = QUrl::fromLocalFile(url.attribute(QLatin1String("url")));
                    QUrl dest = m_replacementList.value(srcUrl);
                    if (dest.isValid()) {
                        url.setAttribute(QLatin1String("url"), dest.toLocalFile());
                        found = true;
                    }
                }
            }
            if (found) {
                // replace content
                Xml::setXmlProperty(e, QStringLiteral("xmldata"), titleXML.toString());
            }
        }
    }

    // process mlt transitions (for luma files)
    prods = mlt.elementsByTagName(QStringLiteral("transition"));
    QString attribute;
    for (int i = 0; i < prods.count(); ++i) {
        QDomElement e = prods.item(i).toElement();
        if (e.isNull()) {
            continue;
        }
        attribute = QStringLiteral("resource");
        QString src = Xml::getXmlProperty(e, attribute);
        if (src.isEmpty()) {
            attribute = QStringLiteral("luma");
        }
        src = Xml::getXmlProperty(e, attribute);
        if (!src.isEmpty()) {
            if (QFileInfo(src).isRelative()) {
                src.prepend(root);
            }
            QUrl srcUrl = QUrl::fromLocalFile(src);
            QUrl dest = m_replacementList.value(srcUrl);
            if (!dest.isEmpty()) {
                Xml::setXmlProperty(e, attribute, dest.toLocalFile());
            }
        }
    }

    QString playList = m_doc.toString();
    if (isArchive) {
        QString startString(QStringLiteral("\""));
        startString.append(archive_url->url().adjusted(QUrl::StripTrailingSlash).toLocalFile());
        QString endString(QStringLiteral("\""));
        endString.append(basePath);
        playList.replace(startString, endString);
        startString = QLatin1Char('>') + archive_url->url().adjusted(QUrl::StripTrailingSlash).toLocalFile();
        endString = QLatin1Char('>') + basePath;
        playList.replace(startString, endString);
    }

    if (isArchive) {
        m_temp = new QTemporaryFile;
        if (!m_temp->open()) {
            KMessageBox::error(this, i18n("Cannot create temporary file"));
        }
        m_temp->write(playList.toUtf8());
        m_temp->close();
        m_archiveThread = QtConcurrent::run(this, &ArchiveWidget::createArchive);
        return true;
    }

    // Make a copy of original project file for extra safety
    QString backupPath = archive_url->url().toLocalFile() + QDir::separator() + m_name + QStringLiteral("-backup.kdenlive");
    QFile source(pCore->currentDoc()->url().toLocalFile());
    source.copy(backupPath);

    QString path = archive_url->url().toLocalFile() + QDir::separator() + m_name + QStringLiteral(".kdenlive");
    QFile file(path);
    if (file.exists() && KMessageBox::warningYesNo(this, i18n("Output file already exists. Do you want to overwrite it?")) != KMessageBox::Yes) {
        return false;
    }
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qCWarning(KDENLIVE_LOG) << "//////  ERROR writing to file: " << path;
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
    QString archiveName(archive_url->url().toLocalFile() + QDir::separator() + m_name + QStringLiteral(".tar.gz"));
    if (QFile::exists(archiveName) &&
        KMessageBox::questionYesNo(this, i18n("File %1 already exists.\nDo you want to overwrite it?", archiveName)) == KMessageBox::No) {
        return;
    }
    QFileInfo dirInfo(archive_url->url().toLocalFile());
    QString user = dirInfo.owner();
    QString group = dirInfo.group();
    KTar archive(archiveName, QStringLiteral("application/x-gzip"));
    archive.open(QIODevice::WriteOnly);

    // Create folders
    for (const QString &path : qAsConst(m_foldersList)) {
        archive.writeDir(path, user, group);
    }

    // Add files
    int ix = 0;
    QMapIterator<QString, QString> i(m_filesList);
    while (i.hasNext()) {
        i.next();
        archive.addLocalFile(i.key(), i.value());
        emit archiveProgress((int)100 * ix / m_filesList.count());
        ix++;
    }

    // Add project file
    bool result = false;
    if (m_temp) {
        archive.addLocalFile(m_temp->fileName(), m_name + QStringLiteral(".kdenlive"));
        result = archive.close();
        delete m_temp;
        m_temp = nullptr;
    }
    emit archivingFinished(result);
}

void ArchiveWidget::slotArchivingBoolFinished(bool result)
{
    if (result) {
        slotJobResult(true, i18n("Project was successfully archived."));
        buttonBox->button(QDialogButtonBox::Apply)->setEnabled(false);
    } else {
        slotJobResult(false, i18n("There was an error processing project file"));
    }
    progressBar->setValue(100);
    buttonBox->button(QDialogButtonBox::Apply)->setText(i18n("Archive"));
    archive_url->setEnabled(true);
    proxy_only->setEnabled(true);
    compressed_archive->setEnabled(true);
    for (int i = 0; i < files_list->topLevelItemCount(); ++i) {
        files_list->topLevelItem(i)->setDisabled(false);
        for (int j = 0; j < files_list->topLevelItem(i)->childCount(); ++j) {
            files_list->topLevelItem(i)->child(j)->setDisabled(false);
        }
    }
}

void ArchiveWidget::slotArchivingIntProgress(int p)
{
    progressBar->setValue(p);
}

void ArchiveWidget::slotStartExtracting()
{
    if (m_archiveThread.isRunning()) {
        // TODO: abort extracting
        return;
    }
    QFileInfo f(m_extractUrl.toLocalFile());
    m_requestedSize = static_cast<KIO::filesize_t>(f.size());
    QDir dir(archive_url->url().toLocalFile());
    if (!dir.mkpath(QStringLiteral("."))) {
        KMessageBox::sorry(this, i18n("Cannot create directory %1", archive_url->url().toLocalFile()));
    }
    slotDisplayMessage(QStringLiteral("system-run"), i18n("Extracting..."));
    buttonBox->button(QDialogButtonBox::Apply)->setText(i18n("Abort"));
    m_archiveThread = QtConcurrent::run(this, &ArchiveWidget::doExtracting);
    m_progressTimer->start();
}

void ArchiveWidget::slotExtractProgress()
{
    KIO::DirectorySizeJob *job = KIO::directorySize(archive_url->url());
    connect(job, &KJob::result, this, &ArchiveWidget::slotGotProgress);
}

void ArchiveWidget::slotGotProgress(KJob *job)
{
    if (!job->error()) {
        auto *j = static_cast<KIO::DirectorySizeJob *>(job);
        progressBar->setValue(static_cast<int>(100 * j->totalSize() / m_requestedSize));
    }
    job->deleteLater();
}

void ArchiveWidget::doExtracting()
{
    m_extractArchive->directory()->copyTo(archive_url->url().toLocalFile() + QDir::separator());
    m_extractArchive->close();
    emit extractingFinished();
}

QString ArchiveWidget::extractedProjectFile() const
{
    return archive_url->url().toLocalFile() + QDir::separator() + m_projectName;
}

void ArchiveWidget::slotExtractingFinished()
{
    m_progressTimer->stop();
    // Process project file
    QFile file(extractedProjectFile());
    bool error = false;
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        error = true;
    } else {
        QString playList = QString::fromUtf8(file.readAll());
        file.close();
        if (playList.isEmpty()) {
            error = true;
        } else {
            playList.replace(QLatin1String("$CURRENTPATH"), archive_url->url().adjusted(QUrl::StripTrailingSlash).toLocalFile());
            if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
                qCWarning(KDENLIVE_LOG) << "//////  ERROR writing to file: ";
                error = true;
            } else {
                file.write(playList.toUtf8());
                if (file.error() != QFile::NoError) {
                    error = true;
                }
                file.close();
            }
        }
    }
    if (error) {
        KMessageBox::sorry(QApplication::activeWindow(), i18n("Cannot open project file %1", extractedProjectFile()), i18n("Cannot open file"));
        reject();
    } else {
        accept();
    }
}

void ArchiveWidget::slotProxyOnly(int onlyProxy)
{
    m_requestedSize = 0;
    if (onlyProxy == Qt::Checked) {
        // Archive proxy clips
        QStringList proxyIdList;
        QTreeWidgetItem *parentItem = nullptr;

        // Build list of existing proxy ids
        for (int i = 0; i < files_list->topLevelItemCount(); ++i) {
            parentItem = files_list->topLevelItem(i);
            if (parentItem->data(0, Qt::UserRole).toString() == QLatin1String("proxy")) {
                break;
            }
        }
        if (!parentItem) {
            return;
        }
        int items = parentItem->childCount();
        for (int j = 0; j < items; ++j) {
            proxyIdList << parentItem->child(j)->data(0, Qt::UserRole + 2).toString();
        }

        // Parse all items to disable original clips for existing proxies
        for (int i = 0; i < proxyIdList.count(); ++i) {
            const QString &id = proxyIdList.at(i);
            if (id.isEmpty()) {
                continue;
            }
            for (int j = 0; j < files_list->topLevelItemCount(); ++j) {
                parentItem = files_list->topLevelItem(j);
                if (parentItem->data(0, Qt::UserRole).toString() == QLatin1String("proxy")) {
                    continue;
                }
                items = parentItem->childCount();
                for (int k = 0; k < items; ++k) {
                    if (parentItem->child(k)->data(0, Qt::UserRole + 2).toString() == id) {
                        // This item has a proxy, do not archive it
                        parentItem->child(k)->setFlags(Qt::ItemIsSelectable);
                        break;
                    }
                }
            }
        }
    } else {
        // Archive all clips
        for (int i = 0; i < files_list->topLevelItemCount(); ++i) {
            QTreeWidgetItem *parentItem = files_list->topLevelItem(i);
            int items = parentItem->childCount();
            for (int j = 0; j < items; ++j) {
                parentItem->child(j)->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
            }
        }
    }

    // Calculate requested size
    int total = 0;
    for (int i = 0; i < files_list->topLevelItemCount(); ++i) {
        QTreeWidgetItem *parentItem = files_list->topLevelItem(i);
        int items = parentItem->childCount();
        int itemsCount = 0;
        bool isSlideshow = parentItem->data(0, Qt::UserRole).toString() == QLatin1String("slideshows");

        for (int j = 0; j < items; ++j) {
            if (!parentItem->child(j)->isDisabled()) {
                m_requestedSize += static_cast<KIO::filesize_t>(parentItem->child(j)->data(0, Qt::UserRole + 3).toInt());
                if (isSlideshow) {
                    total += parentItem->child(j)->data(0, Qt::UserRole + 1).toStringList().count();
                } else {
                    total++;
                }
                itemsCount++;
            }
        }
        parentItem->setText(0, parentItem->text(0).section(QLatin1Char('('), 0, 0) + i18np("(%1 item)", "(%1 items)", itemsCount));
    }
    project_files->setText(i18np("%1 file to archive, requires %2", "%1 files to archive, requires %2", total, KIO::convertSize(m_requestedSize)));
    slotCheckSpace();
}
