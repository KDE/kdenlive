/*
    SPDX-FileCopyrightText: 2011 Jean-Baptiste Mardelle <jb@kdenlive.org>
    SPDX-FileCopyrightText: 2021 Julius Künzel <jk.kdedev@smartalb.uber.space>

SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "archivewidget.h"
#include "bin/bin.h"
#include "bin/projectclip.h"
#include "bin/projectfolder.h"
#include "bin/projectitemmodel.h"
#include "core.h"
#include "projectsettings.h"
#include "titler/titlewidget.h"
#include "utils/qstringutils.h"
#include "xml/xml.hpp"

#include "doc/kdenlivedoc.h"
#include "kdenlive_debug.h"
#include "utils/KMessageBox_KdenliveCompat.h"
#include <KGuiItem>
#include <KLocalizedString>
#include <KMessageBox>
#include <KMessageWidget>
#include <KTar>
#include <KZip>
#include <kio/directorysizejob.h>

#include <QTreeWidget>
#include <QtConcurrent>
#include <utility>
ArchiveWidget::ArchiveWidget(const QString &projectName, const QString &xmlData, const QStringList &luma_list, const QStringList &other_list, QWidget *parent)
    : QDialog(parent)
    , m_requestedSize(0)
    , m_copyJob(nullptr)
    , m_name(projectName.section(QLatin1Char('.'), 0, -2))
    , m_temp(nullptr)
    , m_abortArchive(false)
    , m_extractMode(false)
    , m_progressTimer(nullptr)
    , m_archive(nullptr)
    , m_missingClips(0)
{
    setAttribute(Qt::WA_DeleteOnClose);
    setupUi(this);
    setWindowTitle(i18nc("@title:window", "Archive Project"));
    archive_url->setUrl(QUrl::fromLocalFile(QDir::homePath()));
    connect(archive_url, &KUrlRequester::textChanged, this, &ArchiveWidget::slotCheckSpace);
    connect(this, &ArchiveWidget::archivingFinished, this, &ArchiveWidget::slotArchivingBoolFinished);
    connect(this, &ArchiveWidget::archiveProgress, this, &ArchiveWidget::slotArchivingIntProgress);
    connect(proxy_only, &QCheckBox::stateChanged, this, &ArchiveWidget::slotProxyOnly);
    connect(timeline_archive, &QCheckBox::stateChanged, this, &ArchiveWidget::onlyTimelineItems);

    // Prepare xml
    m_doc.setContent(xmlData);

    // Setup categories
    QTreeWidgetItem *videos = new QTreeWidgetItem(files_list, QStringList() << i18n("Video Clips"));
    videos->setIcon(0, QIcon::fromTheme(QStringLiteral("video-x-generic")));
    videos->setData(0, Qt::UserRole, QStringLiteral("videos"));
    videos->setExpanded(false);
    QTreeWidgetItem *sounds = new QTreeWidgetItem(files_list, QStringList() << i18n("Audio Clips"));
    sounds->setIcon(0, QIcon::fromTheme(QStringLiteral("audio-x-generic")));
    sounds->setData(0, Qt::UserRole, QStringLiteral("sounds"));
    sounds->setExpanded(false);
    QTreeWidgetItem *images = new QTreeWidgetItem(files_list, QStringList() << i18n("Image Clips"));
    images->setIcon(0, QIcon::fromTheme(QStringLiteral("image-x-generic")));
    images->setData(0, Qt::UserRole, QStringLiteral("images"));
    images->setExpanded(false);
    QTreeWidgetItem *slideshows = new QTreeWidgetItem(files_list, QStringList() << i18n("Slideshow Clips"));
    slideshows->setIcon(0, QIcon::fromTheme(QStringLiteral("image-x-generic")));
    slideshows->setData(0, Qt::UserRole, QStringLiteral("slideshows"));
    slideshows->setExpanded(false);
    QTreeWidgetItem *texts = new QTreeWidgetItem(files_list, QStringList() << i18n("Text Clips"));
    texts->setIcon(0, QIcon::fromTheme(QStringLiteral("text-plain")));
    texts->setData(0, Qt::UserRole, QStringLiteral("texts"));
    texts->setExpanded(false);
    QTreeWidgetItem *playlists = new QTreeWidgetItem(files_list, QStringList() << i18n("Playlist Clips"));
    playlists->setIcon(0, QIcon::fromTheme(QStringLiteral("video-mlt-playlist")));
    playlists->setData(0, Qt::UserRole, QStringLiteral("playlist"));
    playlists->setExpanded(false);
    QTreeWidgetItem *others = new QTreeWidgetItem(files_list, QStringList() << i18n("Other Clips"));
    others->setIcon(0, QIcon::fromTheme(QStringLiteral("unknown")));
    others->setData(0, Qt::UserRole, QStringLiteral("others"));
    others->setExpanded(false);
    QTreeWidgetItem *lumas = new QTreeWidgetItem(files_list, QStringList() << i18n("Luma Files"));
    lumas->setIcon(0, QIcon::fromTheme(QStringLiteral("image-x-generic")));
    lumas->setData(0, Qt::UserRole, QStringLiteral("lumas"));
    lumas->setExpanded(false);

    QTreeWidgetItem *proxies = new QTreeWidgetItem(files_list, QStringList() << i18n("Proxy Clips"));
    proxies->setIcon(0, QIcon::fromTheme(QStringLiteral("video-x-generic")));
    proxies->setData(0, Qt::UserRole, QStringLiteral("proxy"));
    proxies->setExpanded(false);

    QTreeWidgetItem *subtitles = new QTreeWidgetItem(files_list, QStringList() << i18n("Subtitles"));
    subtitles->setIcon(0, QIcon::fromTheme(QStringLiteral("text-plain")));
    // subtitles->setData(0, Qt::UserRole, QStringLiteral("subtitles"));
    subtitles->setExpanded(false);

    QStringList subtitlePath = pCore->currentDoc()->getAllSubtitlesPath(true);
    for (auto &path : subtitlePath) {
        QFileInfo info(path);
        m_requestedSize += static_cast<KIO::filesize_t>(info.size());
        new QTreeWidgetItem(subtitles, QStringList() << path);
    }

    // process all files
    QStringList allFonts;
    QStringList extraImageUrls;
    QStringList otherUrls;
    Qt::CaseSensitivity sensitivity = Qt::CaseSensitive;
#ifdef Q_OS_WIN
    // File names in Windows are not case sensitive. So "C:\my_file.mp4" and "c:\my_file.mp4" point to the same file, ensure we handle this
    sensitivity = Qt::CaseInsensitive;
    for (auto &u : other_list) {
        if (!otherUrls.contains(u, sensitivity)) {
            otherUrls << u;
        }
    }
#else
    otherUrls << other_list;
#endif
    generateItems(lumas, luma_list);

    QMap<QString, QString> slideUrls;
    QMap<QString, QString> audioUrls;
    QMap<QString, QString> videoUrls;
    QMap<QString, QString> imageUrls;
    QMap<QString, QString> playlistUrls;
    QMap<QString, QString> proxyUrls;
    QList<std::shared_ptr<ProjectClip>> clipList = pCore->projectItemModel()->getRootFolder()->childClips();
    QStringList handledUrls;
    for (const std::shared_ptr<ProjectClip> &clip : qAsConst(clipList)) {
        ClipType::ProducerType t = clip->clipType();
        if (t == ClipType::Color || t == ClipType::Timeline) {
            continue;
        }
        const QString id = clip->binId();
        const QString url = clip->clipUrl();
        if (t == ClipType::SlideShow) {
            // TODO: Slideshow files
            slideUrls.insert(id, url);
            handledUrls << url;
        } else if (t == ClipType::Image) {
            imageUrls.insert(id, url);
            handledUrls << url;
        } else if (t == ClipType::QText) {
            allFonts << clip->getProducerProperty(QStringLiteral("family"));
        } else if (t == ClipType::Text) {
            QString titleData = clip->getProducerProperty(QStringLiteral("xmldata"));
            QStringList imagefiles = TitleWidget::extractImageList(titleData, pCore->currentDoc()->documentRoot());
            QStringList fonts = TitleWidget::extractFontList(clip->getProducerProperty(QStringLiteral("xmldata")));
            extraImageUrls << imagefiles;
            allFonts << fonts;
        } else if (t == ClipType::Playlist) {
            playlistUrls.insert(id, url);
            handledUrls << url;
            const QStringList files = ProjectSettings::extractPlaylistUrls(clip->clipUrl());
            for (auto &f : files) {
                if (handledUrls.contains(f, sensitivity)) {
                    continue;
                }
                otherUrls << f;
            }
        } else if (!clip->clipUrl().isEmpty()) {
            handledUrls << url;
            if (t == ClipType::Audio) {
                audioUrls.insert(id, url);
            } else {
                videoUrls.insert(id, url);
                // Check if we have a proxy
                QString proxy = clip->getProducerProperty(QStringLiteral("kdenlive:proxy"));
                if (!proxy.isEmpty() && proxy != QLatin1String("-") && QFile::exists(proxy)) {
                    proxyUrls.insert(id, proxy);
                }
            }
        }
        const QStringList files = clip->filesUsedByEffects();
        for (auto &f : files) {
            if (handledUrls.contains(f, sensitivity)) {
                continue;
            }
            otherUrls << f;
            handledUrls << f;
        }
    }

    generateItems(images, extraImageUrls);
    generateItems(sounds, audioUrls);
    generateItems(videos, videoUrls);
    generateItems(images, imageUrls);
    generateItems(slideshows, slideUrls);
    generateItems(playlists, playlistUrls);
    otherUrls.removeDuplicates();
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
                    total += parentItem->child(j)->data(0, SlideshowImagesRole).toStringList().count();
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
    } else {
        // Don't allow archiving a modified or unsaved project
        if (pCore->currentDoc()->isModified()) {
            m_infoMessage->setMessageType(KMessageWidget::Warning);
            m_infoMessage->setText(i18n("Please save your project before archiving"));
            m_infoMessage->animatedShow();
            buttonBox->button(QDialogButtonBox::Apply)->setEnabled(false);
        }
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
    , m_archive(nullptr)
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
    timeline_archive->setHidden(true);
    compression_type->setHidden(true);
    label->setText(i18n("Extract to"));
    setWindowTitle(i18nc("@title:window", "Open Archived Project"));
    archive_url->setUrl(QUrl::fromLocalFile(QDir::homePath()));
    buttonBox->button(QDialogButtonBox::Apply)->setText(i18n("Extract"));
    connect(buttonBox->button(QDialogButtonBox::Apply), &QAbstractButton::clicked, this, &ArchiveWidget::slotStartExtracting);
    buttonBox->button(QDialogButtonBox::Apply)->setEnabled(true);
    adjustSize();
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    m_archiveThread = QtConcurrent::run(this, &ArchiveWidget::openArchiveForExtraction);
#else
    m_archiveThread = QtConcurrent::run(&ArchiveWidget::openArchiveForExtraction, this);
#endif
}

ArchiveWidget::~ArchiveWidget()
{
    delete m_archive;
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
    archive_url->setEnabled(true);
    compressed_archive->setEnabled(true);
    compression_type->setEnabled(true);
    proxy_only->setEnabled(true);
    timeline_archive->setEnabled(true);
    buttonBox->button(QDialogButtonBox::Apply)->setEnabled(true);
    buttonBox->button(QDialogButtonBox::Apply)->setText(i18n("Archive"));
}

void ArchiveWidget::openArchiveForExtraction()
{
    Q_EMIT showMessage(QStringLiteral("system-run"), i18n("Opening archive…"));
    QMimeDatabase db;
    QMimeType mime = db.mimeTypeForUrl(m_extractUrl);
    if (mime.inherits(QStringLiteral("application/x-compressed-tar"))) {
        m_archive = new KTar(m_extractUrl.toLocalFile());
    } else {
        m_archive = new KZip(m_extractUrl.toLocalFile());
    }

    if (!m_archive->isOpen() && !m_archive->open(QIODevice::ReadOnly)) {
        Q_EMIT showMessage(QStringLiteral("dialog-close"), i18n("Cannot open archive file:\n %1", m_extractUrl.toLocalFile()));
        groupBox->setEnabled(false);
        return;
    }

    // Check that it is a kdenlive project archive
    bool isProjectArchive = false;
    QStringList files = m_archive->directory()->entries();
    for (int i = 0; i < files.count(); ++i) {
        if (files.at(i).endsWith(QLatin1String(".kdenlive"))) {
            m_projectName = files.at(i);
            isProjectArchive = true;
            break;
        }
    }

    if (!isProjectArchive) {
        Q_EMIT showMessage(QStringLiteral("dialog-close"), i18n("File %1\n is not an archived Kdenlive project", m_extractUrl.toLocalFile()));
        groupBox->setEnabled(false);
        buttonBox->button(QDialogButtonBox::Apply)->setEnabled(false);
        return;
    }
    buttonBox->button(QDialogButtonBox::Apply)->setEnabled(true);
    Q_EMIT showMessage(QStringLiteral("dialog-ok"), i18n("Ready"));
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
        m_infoMessage->setMessageType(KMessageWidget::Information);
        m_infoMessage->setText(i18n("Abort processing"));
        m_infoMessage->animatedShow();
        m_abortArchive = true;
        if (m_copyJob) {
            m_copyJob->kill();
        }
        m_archiveThread.waitForFinished();
    }
    return true;
}

void ArchiveWidget::generateItems(QTreeWidgetItem *parentItem, const QStringList &items)
{
    QStringList filesList;
    QStringList filesPath;
    QString fileName;
    int ix = 0;
    bool isSlideshow = parentItem->data(0, Qt::UserRole).toString() == QLatin1String("slideshows");
    for (const QString &file : items) {
        fileName = QUrl::fromLocalFile(file).fileName();
        if (file.isEmpty() || fileName.isEmpty()) {
            continue;
        }
        auto *item = new QTreeWidgetItem(parentItem, QStringList() << file);
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
                item->setData(0, SlideshowImagesRole, slideImages);
                item->setData(0, SlideshowSizeRole, totalSize);
                m_requestedSize += static_cast<KIO::filesize_t>(totalSize);
            } else {
                // pattern url (like clip%.3d.png)
                QStringList result = dir.entryList(QDir::Files);
                QString filter = slideUrl.fileName();
                QString ext = filter.section(QLatin1Char('.'), -1);
                filter = filter.section(QLatin1Char('%'), 0, -2);
                QString regexp = QLatin1Char('^') + filter + QStringLiteral("\\d+\\.") + ext + QLatin1Char('$');
                static const QRegularExpression rx(QRegularExpression::anchoredPattern(regexp));
                QStringList slideImages;
                QString directory = dir.absolutePath();
                if (!directory.endsWith(QLatin1Char('/'))) {
                    directory.append(QLatin1Char('/'));
                }
                qint64 totalSize = 0;
                for (const QString &path : qAsConst(result)) {
                    if (rx.match(path).hasMatch()) {
                        totalSize += QFileInfo(directory + path).size();
                        slideImages << directory + path;
                    }
                }
                item->setData(0, SlideshowImagesRole, slideImages);
                item->setData(0, SlideshowSizeRole, totalSize);
                m_requestedSize += static_cast<KIO::filesize_t>(totalSize);
            }
        } else if (filesList.contains(fileName) && !filesPath.contains(file)) {
            // we have 2 different files with same name
            const QString previousName = fileName;
            fileName = QStringUtils::getUniqueFileName(filesList, previousName);
            item->setData(0, Qt::UserRole, fileName);
        }
        if (!isSlideshow) {
            item->setData(0, IsInTimelineRole, 1);
            qint64 fileSize = QFileInfo(file).size();
            if (fileSize <= 0) {
                item->setIcon(0, QIcon::fromTheme(QStringLiteral("edit-delete")));
                m_missingClips++;
            } else {
                m_requestedSize += static_cast<KIO::filesize_t>(fileSize);
                item->setData(0, SlideshowSizeRole, fileSize);
            }
            filesList << fileName;
        }
        filesPath << file;
    }
}

void ArchiveWidget::generateItems(QTreeWidgetItem *parentItem, const QMap<QString, QString> &items)
{
    QStringList filesList;
    QStringList filesPath;
    QString fileName;
    int ix = 0;
    bool isSlideshow = parentItem->data(0, Qt::UserRole).toString() == QLatin1String("slideshows");
    QMap<QString, QString>::const_iterator it = items.constBegin();
    const auto timelineBinId = pCore->bin()->getUsedClipIds();
    while (it != items.constEnd()) {
        QString file = it.value();
        QTreeWidgetItem *item = new QTreeWidgetItem(parentItem, QStringList() << file);
        item->setData(0, IsInTimelineRole, 0);
        for (int id : timelineBinId) {
            if (id == it.key().toInt()) {
                m_timelineSize = static_cast<KIO::filesize_t>(QFileInfo(it.value()).size());
                item->setData(0, IsInTimelineRole, 1);
            }
        }
        // Store the clip's id
        item->setData(0, ClipIdRole, it.key());
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
                item->setData(0, SlideshowImagesRole, slideImages);
                item->setData(0, SlideshowSizeRole, totalSize);
                m_requestedSize += static_cast<KIO::filesize_t>(totalSize);
            } else {
                // pattern url (like clip%.3d.png)
                QStringList result = dir.entryList(QDir::Files);
                QString filter = slideUrl.fileName();
                QString ext = filter.section(QLatin1Char('.'), -1).section(QLatin1Char('?'), 0, 0);
                filter = filter.section(QLatin1Char('%'), 0, -2);
                QString regexp = QLatin1Char('^') + filter + QStringLiteral("\\d+\\.") + ext + QLatin1Char('$');
                static const QRegularExpression rx(QRegularExpression::anchoredPattern(regexp));
                QStringList slideImages;
                qint64 totalSize = 0;
                for (const QString &path : qAsConst(result)) {
                    if (rx.match(path).hasMatch()) {
                        totalSize += QFileInfo(dir.absoluteFilePath(path)).size();
                        slideImages << dir.absoluteFilePath(path);
                    }
                }
                item->setData(0, SlideshowImagesRole, slideImages);
                item->setData(0, SlideshowSizeRole, totalSize);
                m_requestedSize += static_cast<KIO::filesize_t>(totalSize);
            }
        } else if (filesList.contains(fileName) && !filesPath.contains(file)) {
            // we have 2 different files with same name
            const QString previousName = fileName;
            fileName = QStringUtils::getUniqueFileName(filesList, previousName);
            item->setData(0, Qt::UserRole, fileName);
        }
        if (!isSlideshow) {
            qint64 fileSize = QFileInfo(file).size();
            if (fileSize <= 0) {
                item->setIcon(0, QIcon::fromTheme(QStringLiteral("edit-delete")));
                m_missingClips++;
            } else {
                m_requestedSize += static_cast<KIO::filesize_t>(fileSize);
                item->setData(0, SlideshowSizeRole, fileSize);
            }
            filesList << fileName;
        }
        filesPath << file;
        ++it;
    }
}

void ArchiveWidget::slotCheckSpace()
{
    QStorageInfo info(archive_url->url().toLocalFile());
    auto freeSize = static_cast<KIO::filesize_t>(info.bytesAvailable());
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
    m_infoMessage->setMessageType(KMessageWidget::Information);
    m_infoMessage->setText(i18n("Starting archive job"));
    m_infoMessage->animatedShow();
    archive_url->setEnabled(false);
    compressed_archive->setEnabled(false);
    compression_type->setEnabled(false);
    proxy_only->setEnabled(false);
    timeline_archive->setEnabled(false);
    buttonBox->button(QDialogButtonBox::Apply)->setEnabled(false);
    buttonBox->button(QDialogButtonBox::Close)->setText(i18n("Abort"));

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
        m_processedFiles.clear();
        slotDisplayMessage(QStringLiteral("system-run"), i18n("Archiving…"));
        repaint();
    }
    QList<QUrl> files;
    QDir destUrl;
    QString destPath;
    QTreeWidgetItem *parentItem;
    bool isSlideshow = false;
    int items = 0;
    bool isLastCategory = false;

    // We parse all files going into one folder, then start the copy job
    for (int i = 0; i < files_list->topLevelItemCount(); ++i) {
        parentItem = files_list->topLevelItem(i);
        if (parentItem->isDisabled() || parentItem->childCount() == 0) {
            parentItem->setDisabled(true);
            parentItem->setExpanded(false);
            if (i == files_list->topLevelItemCount() - 1) {
                isLastCategory = true;
                break;
            }
            continue;
        }

        if (parentItem->data(0, Qt::UserRole).toString() == QLatin1String("slideshows")) {
            QUrl slideFolder = QUrl::fromLocalFile(archive_url->url().toLocalFile() + QStringLiteral("/slideshows"));
            if (isArchive) {
                m_foldersList.append(QStringLiteral("slideshows"));
            } else {
                QDir dir(slideFolder.toLocalFile());
                if (!dir.mkpath(QStringLiteral("."))) {
                    KMessageBox::error(this, i18n("Cannot create directory %1", slideFolder.toLocalFile()));
                }
            }
            isSlideshow = true;
        } else {
            isSlideshow = false;
        }
        files_list->setCurrentItem(parentItem);
        parentItem->setExpanded(true);
        destPath = parentItem->data(0, Qt::UserRole).toString() + QLatin1Char('/');
        destUrl = QDir(archive_url->url().toLocalFile() + QLatin1Char('/') + destPath);
        QTreeWidgetItem *item;
        for (int j = 0; j < parentItem->childCount(); ++j) {
            item = parentItem->child(j);
            if (item->isDisabled() || item->isHidden()) {
                continue;
            }
            if (m_processedFiles.contains(item->text(0))) {
                // File was already processed, rename
                continue;
            }
            m_processedFiles << item->text(0);
            items++;
            if (parentItem->data(0, Qt::UserRole).toString() == QLatin1String("playlist")) {
                // Special case: playlists (mlt files) may contain urls that need to be replaced too
                QString filename(QUrl::fromLocalFile(item->text(0)).fileName());
                m_infoMessage->setText(i18n("Copying %1", filename));
                const QString playList = processPlaylistFile(item->text(0));
                if (isArchive) {
                    m_temp = new QTemporaryFile();
                    if (!m_temp->open()) {
                        KMessageBox::error(this, i18n("Cannot create temporary file"));
                    }
                    m_temp->write(playList.toUtf8());
                    m_temp->close();
                    m_filesList.insert(m_temp->fileName(), destPath + filename);
                } else {
                    if (!destUrl.mkpath(QStringLiteral("."))) {
                        KMessageBox::error(this, i18n("Cannot create directory %1", destUrl.absolutePath()));
                    }
                    QFile file(destUrl.absoluteFilePath(filename));
                    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
                        qCWarning(KDENLIVE_LOG) << "//////  ERROR writing to file: " << file.fileName();
                        KMessageBox::error(this, i18n("Cannot write to file %1", file.fileName()));
                    }
                    file.write(playList.toUtf8());
                    if (file.error() != QFile::NoError) {
                        KMessageBox::error(this, i18n("Cannot write to file %1", file.fileName()));
                        file.close();
                        return false;
                    }
                    file.close();
                }
            } else if (isSlideshow) {
                // Special case: slideshows
                destPath += item->data(0, Qt::UserRole).toString() + QLatin1Char('/');
                destUrl = QDir(archive_url->url().toLocalFile() + QDir::separator() + destPath);
                QStringList srcFiles = item->data(0, SlideshowImagesRole).toStringList();
                for (int k = 0; k < srcFiles.count(); ++k) {
                    files << QUrl::fromLocalFile(srcFiles.at(k));
                }
                item->setDisabled(true);
                if (parentItem->indexOfChild(item) == parentItem->childCount() - 1) {
                    // We have processed all slideshows
                    parentItem->setDisabled(true);
                }
                // Slideshows are processed one by one, we call slotStartArchiving after each item
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
                                            QUrl::fromLocalFile(destUrl.absoluteFilePath(item->data(0, Qt::UserRole).toString())));
                }
            }
        }
        if (!isSlideshow) {
            // Slideshow is processed one by one and parent is disabled only once all items are done
            parentItem->setDisabled(true);
        }
        // We process each clip category one by one and call slotStartArchiving recursively
        break;
    }

    if (items == 0 && isLastCategory && m_duplicateFiles.isEmpty()) {
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
            const QUrl startJobSrc = i.key();
            const QUrl startJobDst = i.value();
            m_duplicateFiles.remove(startJobSrc);
            m_infoMessage->setText(i18n("Copying %1", startJobSrc.fileName()));
            KIO::CopyJob *job = KIO::copyAs(startJobSrc, startJobDst, KIO::HideProgressInfo);
            connect(job, &KJob::result, this, [this](KJob *jb) { slotArchivingFinished(jb, false); });
            connect(job, &KJob::processedSize, this, &ArchiveWidget::slotArchivingProgress);
        }
        return true;
    }

    if (isArchive) {
        m_foldersList.append(destPath);
        for (int i = 0; i < files.count(); ++i) {
            if (!m_filesList.contains(files.at(i).toLocalFile())) {
                m_filesList.insert(files.at(i).toLocalFile(), destPath + files.at(i).fileName());
            }
        }
        slotArchivingFinished();
    } else if (files.isEmpty()) {
        slotStartArchiving(false);
    } else {
        if (!destUrl.mkpath(QStringLiteral("."))) {
            KMessageBox::error(this, i18n("Cannot create directory %1", destUrl.absolutePath()));
        }
        m_copyJob = KIO::copy(files, QUrl::fromLocalFile(destUrl.absolutePath()), KIO::HideProgressInfo);
        connect(m_copyJob, &KJob::result, this, [this](KJob *jb) { slotArchivingFinished(jb, false); });
        connect(m_copyJob, &KJob::processedSize, this, &ArchiveWidget::slotArchivingProgress);
    }
    if (firstPass) {
        progressBar->setValue(0);
        buttonBox->button(QDialogButtonBox::Apply)->setText(i18n("Abort"));
        buttonBox->button(QDialogButtonBox::Apply)->setEnabled(true);
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
            buttonBox->button(QDialogButtonBox::Close)->setText(i18n("Close"));
        } else {
            processProjectFile();
        }
    } else {
        m_copyJob = nullptr;
        slotJobResult(false, i18n("There was an error while copying the files: %1", job->errorString()));
    }
    if (!compressed_archive->isChecked()) {
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
    if (m_requestedSize == 0) {
        progressBar->setValue(100);
    } else {
        progressBar->setValue(static_cast<int>(100 * size / m_requestedSize));
    }
}

QString ArchiveWidget::processPlaylistFile(const QString &filename)
{
    QDomDocument doc;
    if (!Xml::docContentFromFile(doc, filename, false)) {
        return QString();
    }
    return processMltFile(doc, QStringLiteral("../"));
}

bool ArchiveWidget::processProjectFile()
{
    bool isArchive = compressed_archive->isChecked();

    QString playList = processMltFile(m_doc);

    m_archiveName.clear();
    if (isArchive) {
        m_temp = new QTemporaryFile;
        if (!m_temp->open()) {
            KMessageBox::error(this, i18n("Cannot create temporary file"));
        }
        m_temp->write(playList.toUtf8());
        m_temp->close();
        m_archiveName = QString(archive_url->url().toLocalFile() + QDir::separator() + m_name);
        if (compression_type->currentIndex() == 1) {
            m_archiveName.append(QStringLiteral(".zip"));
        } else {
            m_archiveName.append(QStringLiteral(".tar.gz"));
        };
        if (QFile::exists(m_archiveName) &&
            KMessageBox::questionTwoActions(nullptr, i18n("File %1 already exists.\nDo you want to overwrite it?", m_archiveName), {},
                                            KStandardGuiItem::overwrite(), KStandardGuiItem::cancel()) != KMessageBox::PrimaryAction) {
            return false;
        }
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
        m_archiveThread = QtConcurrent::run(this, &ArchiveWidget::createArchive);
#else
        m_archiveThread = QtConcurrent::run(&ArchiveWidget::createArchive, this);
#endif
        return true;
    }

    // Make a copy of original project file for extra safety
    QString path = archive_url->url().toLocalFile() + QDir::separator() + m_name + QStringLiteral("-backup.kdenlive");
    if (QFile::exists(path) && KMessageBox::warningTwoActions(this, i18n("File %1 already exists.\nDo you want to overwrite it?", path), {},
                                                              KStandardGuiItem::overwrite(), KStandardGuiItem::cancel()) != KMessageBox::PrimaryAction) {
        return false;
    }
    QFile::remove(path);
    QFile source(pCore->currentDoc()->url().toLocalFile());
    if (!source.copy(path)) {
        // Error
        KMessageBox::error(this, i18n("Cannot write to file %1", path));
        return false;
    }

    path = archive_url->url().toLocalFile() + QDir::separator() + m_name + QStringLiteral(".kdenlive");
    QFile file(path);
    if (file.exists() && KMessageBox::warningTwoActions(this, i18n("Output file already exists. Do you want to overwrite it?"), {},
                                                        KStandardGuiItem::overwrite(), KStandardGuiItem::cancel()) != KMessageBox::PrimaryAction) {
        return false;
    }
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qCWarning(KDENLIVE_LOG) << "//////  ERROR writing to file: " << path;
        KMessageBox::error(this, i18n("Cannot write to file %1", path));
        return false;
    }

    file.write(playList.toUtf8());
    if (file.error() != QFile::NoError) {
        KMessageBox::error(this, i18n("Cannot write to file %1", path));
        file.close();
        return false;
    }
    file.close();
    return true;
}

void ArchiveWidget::processElement(QDomElement e, const QString root)
{
    if (e.isNull()) {
        return;
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
                Xml::setXmlProperty(e, QStringLiteral("resource"),
                                    QString("%1:%2").arg(Xml::getXmlProperty(e, QStringLiteral("warp_speed")), dest.toLocalFile()));
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
    propertyProcessUrl(e, QStringLiteral("kdenlive:originalurl"), root);
    src = Xml::getXmlProperty(e, QStringLiteral("xmldata"));
    if (!src.isEmpty() && (src.contains(QLatin1String("QGraphicsPixmapItem")) || src.contains(QLatin1String("QGraphicsSvgItem")))) {
        bool found = false;
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
    propertyProcessUrl(e, QStringLiteral("luma_file"), root);
}

QString ArchiveWidget::processMltFile(const QDomDocument &doc, const QString &destPrefix)
{
    QTreeWidgetItem *item;
    bool isArchive = compressed_archive->isChecked();

    m_replacementList.clear();
    for (int i = 0; i < files_list->topLevelItemCount(); ++i) {
        QTreeWidgetItem *parentItem = files_list->topLevelItem(i);
        if (parentItem->childCount() > 0) {
            // QDir destFolder(archive_url->url().toLocalFile() + QDir::separator() + parentItem->data(0, Qt::UserRole).toString());
            bool isSlideshow = parentItem->data(0, Qt::UserRole).toString() == QLatin1String("slideshows");
            for (int j = 0; j < parentItem->childCount(); ++j) {
                item = parentItem->child(j);
                QUrl src = QUrl::fromLocalFile(item->text(0));
                QUrl dest =
                    QUrl::fromLocalFile(destPrefix + parentItem->data(0, Qt::UserRole).toString() + QLatin1Char('/') + item->data(0, Qt::UserRole).toString());
                if (isSlideshow) {
                    dest = QUrl::fromLocalFile(destPrefix + parentItem->data(0, Qt::UserRole).toString() + QLatin1Char('/') +
                                               item->data(0, Qt::UserRole).toString() + QLatin1Char('/') + src.fileName());
                } else if (item->data(0, Qt::UserRole).isNull()) {
                    dest = QUrl::fromLocalFile(destPrefix + parentItem->data(0, Qt::UserRole).toString() + QLatin1Char('/') + src.fileName());
                }
                m_replacementList.insert(src, dest);
            }
        }
    }

    QDomElement mlt = doc.documentElement();
    QString root = mlt.attribute(QStringLiteral("root"));
    if (!root.isEmpty() && !root.endsWith(QLatin1Char('/'))) {
        root.append(QLatin1Char('/'));
    }

    // Adjust global settings
    QString basePath;
    if (isArchive) {
        basePath = QStringLiteral("$CURRENTPATH");
    } else {
        basePath = archive_url->url().adjusted(QUrl::StripTrailingSlash).toLocalFile();
    }
    // Switch to relative path
    mlt.removeAttribute(QStringLiteral("root"));

    // process mlt producers
    QDomNodeList prods = mlt.elementsByTagName(QStringLiteral("producer"));
    for (int i = 0; i < prods.count(); ++i) {
        processElement(prods.item(i).toElement(), root);
    }
    QDomNodeList chains = mlt.elementsByTagName(QStringLiteral("chain"));
    for (int i = 0; i < chains.count(); ++i) {
        processElement(chains.item(i).toElement(), root);
    }

    // process mlt transitions (for luma files)
    prods = mlt.elementsByTagName(QStringLiteral("transition"));
    for (int i = 0; i < prods.count(); ++i) {
        QDomElement e = prods.item(i).toElement();
        if (e.isNull()) {
            continue;
        }
        propertyProcessUrl(e, QStringLiteral("resource"), root);
        propertyProcessUrl(e, QStringLiteral("luma"), root);
        propertyProcessUrl(e, QStringLiteral("luma.resource"), root);
    }

    // process mlt filters
    prods = mlt.elementsByTagName(QStringLiteral("filter"));
    for (int i = 0; i < prods.count(); ++i) {
        QDomElement e = prods.item(i).toElement();
        if (e.isNull()) {
            continue;
        }
        // properties for vidstab files
        propertyProcessUrl(e, QStringLiteral("filename"), root);
        propertyProcessUrl(e, QStringLiteral("results"), root);
        // properties for LUT files
        propertyProcessUrl(e, QStringLiteral("av.file"), root);
    }

    QString playList = doc.toString();
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
    return playList;
}

void ArchiveWidget::propertyProcessUrl(const QDomElement &e, const QString &propertyName, const QString &root)
{
    QString src = Xml::getXmlProperty(e, propertyName);
    if (!src.isEmpty()) {
        qDebug() << "Found property " << propertyName << " with content: " << src;
        if (QFileInfo(src).isRelative()) {
            src.prepend(root);
        }
        QUrl srcUrl = QUrl::fromLocalFile(src);
        QUrl dest = m_replacementList.value(srcUrl);
        if (!dest.isEmpty()) {
            qDebug() << "-> hast replacement entry " << dest;
            Xml::setXmlProperty(e, propertyName, dest.toLocalFile());
        }
    }
}

void ArchiveWidget::createArchive()
{
    QFileInfo dirInfo(archive_url->url().toLocalFile());
    QString user = dirInfo.owner();
    QString group = dirInfo.group();
    if (compression_type->currentIndex() == 1) {
        m_archive = new KZip(m_archiveName);
    } else {
        m_archive = new KTar(m_archiveName, QStringLiteral("application/x-gzip"));
    }

    QString errorString;
    bool success = true;

    if (!m_archive->isOpen() && !m_archive->open(QIODevice::WriteOnly)) {
        success = false;
        errorString = i18n("Cannot open archive file %1", m_archiveName);
    }

    // Create folders
    if (success) {
        for (const QString &path : qAsConst(m_foldersList)) {
            success = success && m_archive->writeDir(path, user, group);
        }
    }

    // Add files
    if (success) {
        int ix = 0;
        QMapIterator<QString, QString> i(m_filesList);
        int max = m_filesList.count();
        while (i.hasNext()) {
            i.next();
            m_infoMessage->setText(i18n("Archiving %1", i.key()));
            success = m_archive->addLocalFile(i.key(), i.value());
            Q_EMIT archiveProgress(100 * ix / max);
            ix++;
            if (!success || m_abortArchive) {
                if (!success) {
                    errorString.append(i18n("Cannot copy file %1 to %2.", i.key(), i.value()));
                }
                break;
            }
        }
    }

    if (m_abortArchive) {
        return;
    }

    // Add project file
    if (!m_temp) {
        success = false;
    }
    if (success) {
        success = m_archive->addLocalFile(m_temp->fileName(), m_name + QStringLiteral(".kdenlive"));
        delete m_temp;
        m_temp = nullptr;
    }

    errorString.append(m_archive->errorString());
    success = success && m_archive->close();

    Q_EMIT archivingFinished(success, errorString);
}

void ArchiveWidget::slotArchivingBoolFinished(bool result, const QString &errorString)
{
    if (result) {
        slotJobResult(true, i18n("Project was successfully archived.\n%1", m_archiveName));
        // buttonBox->button(QDialogButtonBox::Apply)->setEnabled(false);
    } else {
        slotJobResult(false, i18n("There was an error while archiving the project: %1", errorString.isEmpty() ? i18n("Unknown Error") : errorString));
    }
    progressBar->setValue(100);
    for (int i = 0; i < files_list->topLevelItemCount(); ++i) {
        files_list->topLevelItem(i)->setDisabled(false);
        for (int j = 0; j < files_list->topLevelItem(i)->childCount(); ++j) {
            files_list->topLevelItem(i)->child(j)->setDisabled(false);
        }
    }
    buttonBox->button(QDialogButtonBox::Close)->setText(i18n("Close"));
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
        KMessageBox::error(this, i18n("Cannot create directory %1", archive_url->url().toLocalFile()));
    }
    slotDisplayMessage(QStringLiteral("system-run"), i18n("Extracting…"));
    buttonBox->button(QDialogButtonBox::Apply)->setText(i18n("Abort"));
    buttonBox->button(QDialogButtonBox::Apply)->setEnabled(true);
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    m_archiveThread = QtConcurrent::run(this, &ArchiveWidget::doExtracting);
#else
    m_archiveThread = QtConcurrent::run(&ArchiveWidget::doExtracting, this);
#endif
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
    m_archive->directory()->copyTo(archive_url->url().toLocalFile() + QDir::separator());
    m_archive->close();
    Q_EMIT extractingFinished();
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
        KMessageBox::error(QApplication::activeWindow(), i18n("Cannot open project file %1", extractedProjectFile()), i18n("Cannot open file"));
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
            proxyIdList << parentItem->child(j)->data(0, ClipIdRole).toString();
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
                    if (parentItem->child(k)->data(0, ClipIdRole).toString() == id) {
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
                m_requestedSize += static_cast<KIO::filesize_t>(parentItem->child(j)->data(0, SlideshowSizeRole).toInt());
                if (isSlideshow) {
                    total += parentItem->child(j)->data(0, SlideshowImagesRole).toStringList().count();
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

void ArchiveWidget::onlyTimelineItems(int onlyTimeline)
{
    int count = files_list->topLevelItemCount();
    for (int idx = 0; idx < count; ++idx) {
        QTreeWidgetItem *parent = files_list->topLevelItem(idx);
        int childCount = parent->childCount();
        for (int cidx = 0; cidx < childCount; ++cidx) {
            parent->child(cidx)->setHidden(true);
            if (onlyTimeline == Qt::Checked) {
                if (parent->child(cidx)->data(0, IsInTimelineRole).toInt() > 0) {
                    parent->child(cidx)->setHidden(false);
                }
            } else {
                parent->child(cidx)->setHidden(false);
            }
        }
    }

    // calculating total number of files
    int total = 0;
    for (int i = 0; i < files_list->topLevelItemCount(); ++i) {
        QTreeWidgetItem *parentItem = files_list->topLevelItem(i);
        int items = parentItem->childCount();
        int itemsCount = 0;
        bool isSlideshow = parentItem->data(0, Qt::UserRole).toString() == QLatin1String("slideshows");

        for (int j = 0; j < items; ++j) {
            if (!parentItem->child(j)->isHidden() && !parentItem->child(j)->isDisabled()) {
                if (isSlideshow) {
                    total += parentItem->child(j)->data(0, IsInTimelineRole).toStringList().count();
                } else {
                    total++;
                }
                itemsCount++;
            }
        }
        parentItem->setText(0, parentItem->text(0).section(QLatin1Char('('), 0, 0) + i18np("(%1 item)", "(%1 items)", itemsCount));
    }
    project_files->setText(i18np("%1 file to archive, requires %2", "%1 files to archive, requires %2", total,
                                 KIO::convertSize((onlyTimeline == Qt::Checked) ? m_timelineSize : m_requestedSize)));
    slotCheckSpace();
}
