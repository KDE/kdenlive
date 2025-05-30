/*
SPDX-FileCopyrightText: 2016 Jean-Baptiste Mardelle <jb@kdenlive.org>
This file is part of Kdenlive. See www.kdenlive.org.

SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "temporarydata.h"
#include "bin/bin.h"
#include "core.h"
#include "doc/kdenlivedoc.h"
#include "kdenlivesettings.h"
#include "mainwindow.h"

#include <KLocalizedString>
#include <KMessageBox>
#include <QDesktopServices>
#include <QFontMetrics>
#include <QGridLayout>
#include <QLabel>
#include <QPaintEvent>
#include <QPainter>
#include <QProgressBar>
#include <QPushButton>
#include <QSpinBox>
#include <QStandardPaths>
#include <QTabWidget>
#include <QToolButton>
#include <QToolTip>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <QtConcurrent/QtConcurrentRun>

ChartWidget::ChartWidget(QWidget *parent)
    : QWidget(parent)
{
    QFontMetrics ft(font());
    int minHeight = ft.height() * 6;
    setMinimumSize(minHeight, minHeight);
    setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding);
    m_segments = QList<int>();
}

void ChartWidget::setSegments(const QList<int> &segments)
{
    m_segments = segments;
    update();
}

void ChartWidget::paintEvent(QPaintEvent *event)
{
    QPainter painter(this);
    painter.setRenderHints(QPainter::Antialiasing);
    const QRectF clipRect = event->rect();
    painter.setClipRect(clipRect);
    painter.setPen(palette().midlight().color());
    int pieWidth = qMin(width(), height()) - 10;
    const QRectF pieRect(5, 5, pieWidth, pieWidth);
    int ix = 0;
    int previous = 0;
    for (int val : std::as_const(m_segments)) {
        if (val == 0) {
            ix++;
            continue;
        }
        painter.setBrush(colorAt(ix));
        painter.drawPie(pieRect, previous, val);
        previous += val;
        ix++;
    }
}

TemporaryData::TemporaryData(KdenliveDoc *doc, bool currentProjectOnly, QWidget *parent)
    : QDialog(parent)
    , m_doc(doc)
    , m_currentProjectOnly(currentProjectOnly)
{
    setAttribute(Qt::WA_DeleteOnClose);
    setupUi(this);
    m_currentSizes = {0, 0, 0, 0, 0};

    // Setup page for current project
    m_currentPie = new ChartWidget(this);
    currentChartBox->addWidget(m_currentPie);

    QPalette pal(palette());
    QFontMetrics ft(font());
    int minHeight = ft.height() / 2;

    // Timeline preview data
    previewColor->setFixedSize(minHeight, minHeight);
    pal.setColor(QPalette::Window, m_currentPie->colorAt(0));
    previewColor->setPalette(pal);
    connect(delPreview, &QToolButton::clicked, this, &TemporaryData::deletePreview);

    // Proxy clips
    proxyColor->setFixedSize(minHeight, minHeight);
    pal.setColor(QPalette::Window, m_currentPie->colorAt(1));
    proxyColor->setPalette(pal);
    connect(delProxy, &QToolButton::clicked, this, &TemporaryData::deleteProjectProxy);

    // Proxy clips
    sequenceColor->setFixedSize(minHeight, minHeight);
    pal.setColor(QPalette::Window, m_currentPie->colorAt(4));
    sequenceColor->setPalette(pal);

    // Audio Thumbs
    audioColor->setFixedSize(minHeight, minHeight);
    pal.setColor(QPalette::Window, m_currentPie->colorAt(2));
    audioColor->setPalette(pal);
    connect(delAudio, &QToolButton::clicked, this, &TemporaryData::deleteAudio);

    // Video Thumbs
    thumbColor->setFixedSize(minHeight, minHeight);
    pal.setColor(QPalette::Window, m_currentPie->colorAt(3));
    thumbColor->setPalette(pal);
    connect(delThumb, &QToolButton::clicked, this, &TemporaryData::deleteThumbs);

    // Current total
    bool ok;
    QDir dir = m_doc->getCacheDir(CacheBase, &ok);
    currentPath->setText(QStringLiteral("<a href='#'>") + dir.absolutePath() + QStringLiteral("</a>"));
    connect(currentPath, &QLabel::linkActivated, this, &TemporaryData::openCacheFolder);
    connect(delCurrent, &QToolButton::clicked, this, &TemporaryData::deleteCurrentCacheData);

    m_proxies = m_doc->getProxyHashList();
    for (int i = 0; i < m_proxies.count(); i++) {
        m_proxies[i].append(QLatin1Char('*'));
    }

    // Setup global page
    m_globalPie = new ChartWidget(this);
    gChartLayout->addWidget(m_globalPie);
    gChartLayout->setStretch(0, 15);
    gChartLayout->setStretch(1, 5);

    connect(listWidget, &QTreeWidget::itemDoubleClicked, this, [&](QTreeWidgetItem *item, int) {
        QDesktopServices::openUrl(QUrl::fromLocalFile(m_globalDir.absoluteFilePath(item->data(0, Qt::UserRole).toString())));
    });

    // Total Cache data
    pal = palette();
    gTotalColor->setFixedSize(minHeight, minHeight);
    pal.setColor(QPalette::Window, m_currentPie->colorAt(0));
    gTotalColor->setPalette(pal);
    connect(gClean, &QToolButton::clicked, this, &TemporaryData::cleanCache);

    // Selection
    gSelectedColor->setFixedSize(minHeight, minHeight);
    pal.setColor(QPalette::Window, m_currentPie->colorAt(1));
    gSelectedColor->setPalette(pal);
    connect(gDelete, &QToolButton::clicked, this, &TemporaryData::deleteSelected);

    // Proxy data
    connect(gProxyClean, &QToolButton::clicked, this, &TemporaryData::cleanProxy);
    connect(gProxyDelete, &QToolButton::clicked, this, &TemporaryData::deleteProxy);
    ok = false;
    QDir global = m_doc->getCacheDir(SystemCacheRoot, &ok);
    QDir proxyFolder(global.absoluteFilePath(QStringLiteral("proxy")));
    gProxyPath->setText(QStringLiteral("<a href='#'>%1</a>").arg(proxyFolder.absolutePath()));
    connect(gProxyPath, &QLabel::linkActivated, [proxyFolder]() { QDesktopServices::openUrl(QUrl::fromLocalFile(proxyFolder.absolutePath())); });

    // Backup data
    connect(gBackupClean, &QToolButton::clicked, this, &TemporaryData::cleanBackup);
    connect(gBackupDelete, &QToolButton::clicked, this, &TemporaryData::deleteBackup);
    QDir backupFolder(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + QStringLiteral("/.backup"));
    gBackupPath->setText(QStringLiteral("<a href='#'>%1</a>").arg(backupFolder.absolutePath()));
    connect(gBackupPath, &QLabel::linkActivated, [backupFolder]() { QDesktopServices::openUrl(QUrl::fromLocalFile(backupFolder.absolutePath())); });

    // Config cleanup age
    gCleanupSpin->setSuffix(i18np(" month", " months", KdenliveSettings::cleanCacheMonths()));
    gCleanupSpin->setValue(KdenliveSettings::cleanCacheMonths());
    connect(gCleanupSpin, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), this, [&](int value) {
        KdenliveSettings::setCleanCacheMonths(value);
        gCleanupSpin->setSuffix(i18np(" month", " months", KdenliveSettings::cleanCacheMonths()));
    });

    // Setup help text
    help_cached->setToolTip(i18n("<b>Cached data</b> is composed of clip thumbnails and timeline preview videos. Deleting is safe, all data can be recreated "
                                 "on project opening.<br/><b>Backup data</b> is an archive of previous versions of your project files. Useful if you need to "
                                 "recover a previous version of a project. This data cannot be recovered.<br/><b>Proxy clips</b> are lower resolution video "
                                 "files used for faster editing. Deleting is safe, proxy clips can be recreated if you have the original source clips."));
    connect(help_cached, &QToolButton::clicked, this, [this]() { QToolTip::showText(QCursor::pos(), help_cached->toolTip()); });

    // Cache info message
    cache_info->hide();
    QAction *a = new QAction(QIcon::fromTheme(QStringLiteral("edit-clear-history")), i18n("Cleanup"), this);
    cache_info->setText(i18n("Your cache and backup data exceeds %1, cleanup is recommended.", KIO::convertSize(1048576 * KdenliveSettings::maxcachesize())));
    cache_info->addAction(a);
    connect(a, &QAction::triggered, this, &TemporaryData::slotCleanUp);

    processBackupDirectories();

    connect(listWidget, &QTreeWidget::itemSelectionChanged, this, &TemporaryData::refreshGlobalPie);

    bool globalOnly = !pCore->bin()->hasUserClip();
    if (currentProjectOnly) {
        tabWidget->removeTab(1);
    } else if (globalOnly) {
        tabWidget->removeTab(0);
    }

    if (globalOnly && !currentProjectOnly) {
        updateGlobalInfo();
    } else {
        updateDataInfo();
    }
}

void TemporaryData::updateDataInfo()
{
    m_totalCurrent = 0;
    bool ok = false;
    QDir preview = m_doc->getCacheDir(CacheBase, &ok);
    if (!ok) {
        projectPage->setEnabled(false);
        return;
    }
    preview = m_doc->getCacheDir(CachePreview, &ok);
    if (ok) {
        QFuture<KIO::filesize_t> future = QtConcurrent::run(&MainWindow::fetchFolderSize, pCore->window(), preview.absolutePath());
        QFutureWatcher<KIO::filesize_t> *watcher = new QFutureWatcher<KIO::filesize_t>(this);
        watcher->setFuture(future);
        connect(watcher, &QFutureWatcherBase::finished, this, [this, watcher] {
            KIO::filesize_t total = watcher->result();
            delPreview->setEnabled(total > 0);
            m_totalCurrent += total;
            m_currentSizes[0] = total;
            previewSize->setText(KIO::convertSize(total));
            updateTotal();
            watcher->deleteLater();
        });
    }

    preview = m_doc->getCacheDir(CacheProxy, &ok);
    if (ok) {
        if (m_proxies.isEmpty()) {
            // No proxies for this project
            gotProxySize(0);
        } else {
            preview.setNameFilters(m_proxies);
            const QFileInfoList fList = preview.entryInfoList();
            size_t size = 0;
            for (const QFileInfo &info : fList) {
                size += size_t(info.size());
            }
            gotProxySize(size);
        }
    }

    preview = m_doc->getCacheDir(CacheAudio, &ok);
    if (ok) {
        QFuture<KIO::filesize_t> future = QtConcurrent::run(&MainWindow::fetchFolderSize, pCore->window(), preview.absolutePath());
        QFutureWatcher<KIO::filesize_t> *watcher = new QFutureWatcher<KIO::filesize_t>(this);
        watcher->setFuture(future);
        connect(watcher, &QFutureWatcherBase::finished, this, [this, watcher] {
            KIO::filesize_t total = watcher->result();
            delAudio->setEnabled(total > 0);
            m_totalCurrent += total;
            m_currentSizes[2] = total;
            audioSize->setText(KIO::convertSize(total));
            updateTotal();
            watcher->deleteLater();
        });
    }
    preview = m_doc->getCacheDir(CacheSequence, &ok);
    if (ok) {
        QFuture<KIO::filesize_t> future = QtConcurrent::run(&MainWindow::fetchFolderSize, pCore->window(), preview.absolutePath());
        QFutureWatcher<KIO::filesize_t> *watcher = new QFutureWatcher<KIO::filesize_t>(this);
        watcher->setFuture(future);
        connect(watcher, &QFutureWatcherBase::finished, this, [this, watcher] {
            KIO::filesize_t total = watcher->result();
            m_totalCurrent += total;
            m_currentSizes[4] += total;
            sequenceSize->setText(KIO::convertSize(m_currentSizes.at(4)));
            updateTotal();
            watcher->deleteLater();
        });
    }
    preview = m_doc->getCacheDir(CacheTmpWorkFiles, &ok);
    if (ok) {
        QFuture<KIO::filesize_t> future = QtConcurrent::run(&MainWindow::fetchFolderSize, pCore->window(), preview.absolutePath());
        QFutureWatcher<KIO::filesize_t> *watcher = new QFutureWatcher<KIO::filesize_t>(this);
        watcher->setFuture(future);
        connect(watcher, &QFutureWatcherBase::finished, this, [this, watcher] {
            KIO::filesize_t total = watcher->result();
            m_totalCurrent += total;
            m_currentSizes[4] += total;
            sequenceSize->setText(KIO::convertSize(m_currentSizes.at(4)));
            updateTotal();
            watcher->deleteLater();
        });
    }
    preview = m_doc->getCacheDir(CacheThumbs, &ok);
    if (ok) {
        QFuture<KIO::filesize_t> future = QtConcurrent::run(&MainWindow::fetchFolderSize, pCore->window(), preview.absolutePath());
        QFutureWatcher<KIO::filesize_t> *watcher = new QFutureWatcher<KIO::filesize_t>(this);
        watcher->setFuture(future);
        connect(watcher, &QFutureWatcherBase::finished, this, [this, watcher] {
            KIO::filesize_t total = watcher->result();
            delThumb->setEnabled(total > 0);
            m_totalCurrent += total;
            m_currentSizes[3] = total;
            thumbSize->setText(KIO::convertSize(total));
            updateTotal();
            watcher->deleteLater();
        });
    }
    if (!m_currentProjectOnly) {
        updateGlobalInfo();
    }
}

void TemporaryData::gotProxySize(KIO::filesize_t total)
{
    delProxy->setEnabled(total > 0);
    m_totalCurrent += total;
    m_currentSizes[1] = total;
    proxySize->setText(KIO::convertSize(total));
    updateTotal();
}

void TemporaryData::updateTotal()
{
    currentSize->setText(KIO::convertSize(m_totalCurrent));
    delCurrent->setEnabled(m_totalCurrent > 0);
    QList<int> segments;
    for (KIO::filesize_t size : std::as_const(m_currentSizes)) {
        if (m_totalCurrent == 0) {
            segments << 0;
        } else {
            segments << static_cast<int>(16 * size * 360 / m_totalCurrent);
        }
    }
    m_currentPie->setSegments(segments);
}

void TemporaryData::deletePreview()
{
    bool ok = false;
    QDir dir = m_doc->getCacheDir(CachePreview, &ok);
    if (!ok) {
        return;
    }
    if (KMessageBox::warningContinueCancel(
            this,
            i18n("Delete all data in the preview folder:\n%1\nPreview folder contains the timeline previews, and can be recreated with the source project.",
                 dir.absolutePath())) != KMessageBox::Continue) {
        return;
    }
    if (dir.dirName() == QLatin1String("preview")) {
        dir.removeRecursively();
        dir.mkpath(QStringLiteral("."));
        Q_EMIT disablePreview();
        updateDataInfo();
    }
}

void TemporaryData::deleteBackup()
{
    QDir backupFolder(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + QStringLiteral("/.backup"));
    if (KMessageBox::warningContinueCancel(
            this, i18n("Delete all data in the backup folder:\n%1\nA copy of all your project files is kept in this folder for recovery in case of corruption.",
                       backupFolder.absolutePath())) != KMessageBox::Continue) {
        return;
    }
    if (backupFolder.dirName() == QLatin1String(".backup")) {
        backupFolder.removeRecursively();
        backupFolder.mkpath(QStringLiteral("."));
        processBackupDirectories();
    }
}

void TemporaryData::cleanBackup()
{
    QDir backupFolder(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + QStringLiteral("/.backup"));
    QFileInfoList files = backupFolder.entryInfoList(QDir::Files, QDir::Time);
    QStringList oldFiles;
    QDateTime current = QDateTime::currentDateTime();
    KIO::filesize_t totalSize = 0;
    for (const QFileInfo &f : std::as_const(files)) {
        if (f.lastModified().addMonths(KdenliveSettings::cleanCacheMonths()) < current) {
            oldFiles << f.fileName();
            totalSize += f.size();
        }
    }
    if (oldFiles.isEmpty()) {
        KMessageBox::information(this, i18n("No backup data older than %1 months was found.", KdenliveSettings::cleanCacheMonths()));
        return;
    }
    if (KMessageBox::warningContinueCancelList(
            this,
            i18n("This will delete backup data (%1) for projects older than %2 months.", KIO::convertSize(totalSize), KdenliveSettings::cleanCacheMonths()),
            oldFiles) != KMessageBox::Continue) {
        return;
    }
    if (backupFolder.dirName() == QLatin1String(".backup")) {
        for (const QString &f : std::as_const(oldFiles)) {
            backupFolder.remove(f);
        }
        processBackupDirectories();
    }
}

void TemporaryData::cleanCache()
{
    // Find empty dirs
    QList<QTreeWidgetItem *> emptyDirs = listWidget->findItems(KIO::convertSize(0), Qt::MatchExactly, 2);
    // Find old dirs
    QTreeWidgetItem *root = listWidget->invisibleRootItem();
    if (!root) {
        return;
    }
    // Find old backup data ( older than x months ), or very small (that can be quickly recreated)
    size_t total = 0;
    QDateTime current = QDateTime::currentDateTime();
    int max = root->childCount();
    for (int i = 0; i < max; i++) {
        QTreeWidgetItem *child = root->child(i);
        if (emptyDirs.contains(child)) {
            continue;
        }
        size_t childSize = size_t(child->data(1, Qt::UserRole).toLongLong());
        // Check temporary folders with content less than 200kB or old data
        if (childSize < 200000 || child->data(2, Qt::UserRole).toDateTime().addMonths(KdenliveSettings::cleanCacheMonths()) < current) {
            emptyDirs << child;
            total += childSize;
        }
    }
    QStringList folders;
    for (auto *item : std::as_const(emptyDirs)) {
        folders << item->data(0, Qt::UserRole).toString();
    }
    if (folders.isEmpty()) {
        KMessageBox::information(this, i18n("No cache data older than %1 months was found.", KdenliveSettings::cleanCacheMonths()));
        return;
    }

    if (KMessageBox::warningContinueCancelList(this,
                                               i18n("This will delete cache data (%1) for projects that were deleted, have very few cached data or older than "
                                                    "%2 months. All cached data can be recreated from the source files on project opening.",
                                                    KIO::convertSize(total), KdenliveSettings::cleanCacheMonths()),
                                               folders) != KMessageBox::Continue) {
        return;
    }
    deleteCache(folders);
}

void TemporaryData::deleteProjectProxy()
{
    if (m_proxies.isEmpty()) {
        KMessageBox::information(this, i18n("No proxies found in the current project."));
        return;
    }
    bool ok = false;
    QDir dir = m_doc->getCacheDir(CacheProxy, &ok);
    if (!ok || dir.dirName() != QLatin1String("proxy")) {
        return;
    }
    dir.setNameFilters(m_proxies);
    QStringList files = dir.entryList(QDir::Files);
    if (KMessageBox::warningContinueCancelList(this,
                                               i18n("Delete all project data in the proxy folder:\n%1\nProxy folder contains the proxy clips for all your "
                                                    "projects. This proxies can be recreated from the source clips.",
                                                    dir.absolutePath()),
                                               files) != KMessageBox::Continue) {
        return;
    }
    for (const QString &file : std::as_const(files)) {
        dir.remove(file);
    }
    Q_EMIT disableProxies();
    updateDataInfo();
}

void TemporaryData::deleteAudio()
{
    bool ok = false;
    QDir dir = m_doc->getCacheDir(CacheAudio, &ok);
    if (!ok) {
        return;
    }
    if (KMessageBox::warningContinueCancel(
            this, i18n("Delete all data in the cache audio folder:\n%1\nThis folder contains the data for audio thumbnails in this project.",
                       dir.absolutePath())) != KMessageBox::Continue) {
        return;
    }
    if (dir.dirName() == QLatin1String("audiothumbs")) {
        dir.removeRecursively();
        dir.mkpath(QStringLiteral("."));
        updateDataInfo();
    }
}

void TemporaryData::deleteThumbs()
{
    bool ok = false;
    QDir dir = m_doc->getCacheDir(CacheThumbs, &ok);
    if (!ok) {
        return;
    }
    if (KMessageBox::warningContinueCancel(
            this, i18n("Delete all data in the cache thumbnail folder:\n%1\nThis folder contains the data for video thumbnails in this project.",
                       dir.absolutePath())) != KMessageBox::Continue) {
        return;
    }
    if (dir.dirName() == QLatin1String("videothumbs")) {
        dir.removeRecursively();
        dir.mkpath(QStringLiteral("."));
        updateDataInfo();
    }
}

void TemporaryData::deleteCurrentCacheData(bool warn)
{
    bool ok = false;
    QDir dir = m_doc->getCacheDir(CacheBase, &ok);
    if (!ok) {
        return;
    }
    if (warn && KMessageBox::warningContinueCancel(this, i18n("Delete all data in the cache folder:\n%1\nCache folder contains the audio and video thumbnails, "
                                                              "as well as timeline previews. All this data will be recreated on project opening.",
                                                              dir.absolutePath())) != KMessageBox::Continue) {
        return;
    }
    if (dir.dirName() == m_doc->getDocumentProperty(QStringLiteral("documentid"))) {
        Q_EMIT disablePreview();
        Q_EMIT disableProxies();
        dir.removeRecursively();
        m_doc->initCacheDirs();
        if (warn) {
            updateDataInfo();
        }
    }
}

void TemporaryData::openCacheFolder()
{
    bool ok = false;
    QDir dir = m_doc->getCacheDir(CacheBase, &ok);
    if (!ok) {
        return;
    }
    QDesktopServices::openUrl(QUrl::fromLocalFile(dir.absolutePath()));
}

void TemporaryData::processBackupDirectories()
{
    QDir backupFolder(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + QStringLiteral("/.backup"));
    QFuture<KIO::filesize_t> future = QtConcurrent::run(&MainWindow::fetchFolderSize, pCore->window(), backupFolder.absolutePath());
    QFutureWatcher<KIO::filesize_t> *watcher = new QFutureWatcher<KIO::filesize_t>(this);
    watcher->setFuture(future);
    connect(watcher, &QFutureWatcherBase::finished, this, [this, watcher] {
        KIO::filesize_t total = watcher->result();
        m_totalBackup = total;
        refreshWarningMessage();
        gBackupSize->setText(KIO::convertSize(total));
        watcher->deleteLater();
    });
}

void TemporaryData::processProxyDirectory()
{
    QFuture<KIO::filesize_t> future = QtConcurrent::run(&MainWindow::fetchFolderSize, pCore->window(), m_globalDir.absoluteFilePath(QStringLiteral("proxy")));
    QFutureWatcher<KIO::filesize_t> *watcher = new QFutureWatcher<KIO::filesize_t>(this);
    watcher->setFuture(future);
    connect(watcher, &QFutureWatcherBase::finished, this, [this, watcher] {
        KIO::filesize_t total = watcher->result();
        m_totalProxy = total;
        refreshWarningMessage();
        gProxySize->setText(KIO::convertSize(total));
        watcher->deleteLater();
    });
}

void TemporaryData::updateGlobalInfo()
{
    listWidget->blockSignals(true);
    bool ok = false;
    QDir preview = m_doc->getCacheDir(SystemCacheRoot, &ok);
    if (!ok) {
        globalPage->setEnabled(false);
        return;
    }
    m_globalDir = preview;
    m_globalDirectories.clear();
    m_processingDirectory.clear();
    m_totalGlobal = 0;
    m_totalProxy = 0;
    m_totalBackup = 0;
    listWidget->clear();
    m_globalDirectories = m_globalDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    // These are some KDE cache dirs related to Kdenlive that don't manage ourselves
    m_globalDirectories.removeAll(QStringLiteral("knewstuff"));
    m_globalDirectories.removeAll(QStringLiteral("attica"));
    m_globalDirectories.removeAll(QStringLiteral("proxy"));
    gDelete->setEnabled(!m_globalDirectories.isEmpty());
    processProxyDirectory();
    processglobalDirectories();
    listWidget->blockSignals(false);
}

void TemporaryData::processglobalDirectories()
{
    if (m_globalDirectories.isEmpty()) {
        return;
    }
    m_processingDirectory = m_globalDirectories.takeFirst();
    QFuture<KIO::filesize_t> future = QtConcurrent::run(&MainWindow::fetchFolderSize, pCore->window(), m_globalDir.absoluteFilePath(m_processingDirectory));
    QFutureWatcher<KIO::filesize_t> *watcher = new QFutureWatcher<KIO::filesize_t>(this);
    watcher->setFuture(future);
    connect(watcher, &QFutureWatcherBase::finished, this, [this, watcher] {
        KIO::filesize_t total = watcher->result();
        gotFolderSize(total);
        watcher->deleteLater();
    });
}

void TemporaryData::gotFolderSize(KIO::filesize_t total)
{
    if (m_processingDirectory.isEmpty()) {
        return;
    }
    m_totalGlobal += total;
    auto *item = new TreeWidgetItem(listWidget);
    // Check last save path for this cache folder
    QDir dir(m_globalDir.absoluteFilePath(m_processingDirectory));
    QStringList filters;
    filters << QStringLiteral("*.kdenlive");
    QStringList str = dir.entryList(filters, QDir::Files | QDir::Hidden, QDir::Time);
    if (!str.isEmpty()) {
        QString path = QUrl::fromPercentEncoding(str.at(0).toUtf8());
        // Remove leading dot
        path.remove(0, 1);
        item->setText(0, m_processingDirectory + QStringLiteral(" (%1)").arg(QUrl::fromLocalFile(path).fileName()));
        if (QFile::exists(path)) {
            item->setIcon(0, QIcon::fromTheme(QStringLiteral("kdenlive")));
        } else {
            item->setIcon(0, QIcon::fromTheme(QStringLiteral("dialog-close")));
        }
    } else {
        item->setText(0, m_processingDirectory);
        if (m_processingDirectory == QLatin1String("proxy")) {
            item->setIcon(0, QIcon::fromTheme(QStringLiteral("kdenlive-show-video")));
        }
    }
    item->setData(0, Qt::UserRole, m_processingDirectory);
    item->setText(1, KIO::convertSize(total));
    QDateTime date = QFileInfo(dir.absolutePath()).lastModified();
    QLocale locale;
    item->setText(2, locale.toString(date, QLocale::ShortFormat));
    item->setData(1, Qt::UserRole, total);
    item->setData(2, Qt::UserRole, date);
    listWidget->addTopLevelItem(item);
    listWidget->resizeColumnToContents(0);
    listWidget->resizeColumnToContents(1);
    if (m_globalDirectories.isEmpty()) {
        // Processing done, check total size
        refreshWarningMessage();
        gTotalSize->setText(KIO::convertSize(m_totalGlobal));
        listWidget->setCurrentItem(listWidget->topLevelItem(0));
    } else {
        processglobalDirectories();
    }
}

void TemporaryData::refreshWarningMessage()
{
    if (KdenliveSettings::maxcachesize() > 0 &&
        ((m_totalGlobal + m_totalBackup + m_totalProxy) > KIO::filesize_t(1048576) * KdenliveSettings::maxcachesize())) {
        // Cache larger than x MB, warn
        cache_info->animatedShow();
    } else {
        cache_info->animatedHide();
    }
}

void TemporaryData::refreshGlobalPie()
{
    QList<QTreeWidgetItem *> list = listWidget->selectedItems();
    KIO::filesize_t currentSize = 0;
    for (QTreeWidgetItem *current : std::as_const(list)) {
        if (current) {
            currentSize += current->data(1, Qt::UserRole).toULongLong();
        }
    }
    gSelectedSize->setText(KIO::convertSize(currentSize));
    int percent = m_totalGlobal <= 0 ? 0 : int(16 * currentSize * 360 / m_totalGlobal);
    m_globalPie->setSegments(QList<int>() << 16 * 360 << percent);
    if (list.size() == 1 && list.at(0)->text(0) == m_doc->getDocumentProperty(QStringLiteral("documentid"))) {
        gDelete->setToolTip(i18n("Clear current cache"));
    } else {
        gDelete->setToolTip(i18n("Delete selected cache"));
    }
}

void TemporaryData::deleteSelected()
{
    QList<QTreeWidgetItem *> list = listWidget->selectedItems();
    QStringList folders;
    for (QTreeWidgetItem *current : std::as_const(list)) {
        if (current) {
            folders << current->data(0, Qt::UserRole).toString();
        }
    }
    if (KMessageBox::warningContinueCancelList(this,
                                               i18n("Delete the following cache folders from\n%1\nCache folders contains the audio and video thumbnails, as "
                                                    "well as timeline previews. All this data will be recreated on project opening.",
                                                    m_globalDir.absolutePath()),
                                               folders) != KMessageBox::Continue) {
        return;
    }
    deleteCache(folders);
}

void TemporaryData::deleteCache(QStringList &folders)
{
    const QString currentId = m_doc->getDocumentProperty(QStringLiteral("documentid"));
    for (const QString &folder : std::as_const(folders)) {
        if (folder == currentId) {
            // Trying to delete current project's tmp folder. Do not delete, but clear it
            deleteCurrentCacheData(false);
            continue;
        }
        QDir toRemove(m_globalDir.filePath(folder));
        toRemove.removeRecursively();
    }
    updateGlobalInfo();
}

void TemporaryData::deleteProxy()
{
    QDir proxies(m_globalDir.absoluteFilePath(QStringLiteral("proxy")));
    if (proxies.dirName() != QLatin1String("proxy")) {
        return;
    }
    if (KMessageBox::warningContinueCancel(this, i18n("Delete the proxy folder\n%1\nContains proxy clips for all your projects.", proxies.absolutePath())) !=
        KMessageBox::Continue) {
        return;
    }
    QDir toRemove(m_globalDir.filePath(QStringLiteral("proxy")));
    toRemove.removeRecursively();
    // We deleted proxy folder, recreate it
    toRemove.mkpath(QStringLiteral("."));
    processProxyDirectory();
}

void TemporaryData::cleanProxy()
{
    QDir proxies(m_globalDir.absoluteFilePath(QStringLiteral("proxy")));
    if (proxies.dirName() != QLatin1String("proxy")) {
        return;
    }
    QFileInfoList files = proxies.entryInfoList(QDir::Files, QDir::Time);
    QStringList oldFiles;
    QDateTime current = QDateTime::currentDateTime();
    size_t size = 0;
    for (const QFileInfo &f : std::as_const(files)) {
        if (f.lastModified().addMonths(KdenliveSettings::cleanCacheMonths()) < current) {
            oldFiles << f.fileName();
            size += size_t(f.size());
        }
    }
    if (oldFiles.isEmpty()) {
        KMessageBox::information(this, i18n("No proxy clip older than %1 months found.", KdenliveSettings::cleanCacheMonths()));
        return;
    }
    if (KMessageBox::warningContinueCancelList(
            this, i18n("Delete the following proxy clips (%1)\nProxy clips can be recreated on project opening.", KIO::convertSize(size)), oldFiles) !=
        KMessageBox::Continue) {
        return;
    }
    for (const QString &f : std::as_const(oldFiles)) {
        proxies.remove(f);
    }
    processProxyDirectory();
}

void TemporaryData::slotCleanUp()
{
    cleanCache();
    cleanBackup();
}
