/*
Copyright (C) 2016  Jean-Baptiste Mardelle <jb@kdenlive.org>
This file is part of Kdenlive. See www.kdenlive.org.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of
the License or (at your option) version 3 or any later version
accepted by the membership of KDE e.V. (or its successor approved
by the membership of KDE e.V.), which shall act as a proxy
defined in Section 14 of version 3 of the license.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "temporarydata.h"
#include "doc/kdenlivedoc.h"
#include "kdenlivesettings.h"
#include "core.h"
#include "bin/bin.h"

#include <KLocalizedString>
#include <KMessageBox>
#include <KDiskFreeSpaceInfo>
#include <QDesktopServices>
#include <QFontMetrics>
#include <QGridLayout>
#include <QLabel>
#include <QPaintEvent>
#include <QPainter>
#include <QPushButton>
#include <QStandardPaths>
#include <QTabWidget>
#include <QToolButton>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <QProgressBar>
#include <QSpinBox>

static QList<QColor> chartColors;

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
    int pieWidth = qMin(width(), height()) - 10;
    const QRectF pieRect(5, 5, pieWidth, pieWidth);
    int ix = 0;
    int previous = 0;
    for (int val : qAsConst(m_segments)) {
        if (val == 0) {
            ix++;
            continue;
        }
        painter.setBrush(chartColors.at(ix));
        painter.drawPie(pieRect, previous, val * 16);
        previous = val * 16;
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
    chartColors << QColor(Qt::darkRed) << QColor(Qt::darkBlue) << QColor(Qt::darkGreen) << QColor(Qt::darkMagenta);
    m_currentSizes << 0 << 0 << 0 << 0;

    // Setup page for current project
    m_currentPie = new ChartWidget(this);
    currentChartBox->addWidget(m_currentPie);

    QPalette pal(palette());
    QFontMetrics ft(font());
    int minHeight = ft.height() / 2;

    // Timeline preview data
    previewColor->setFixedSize(minHeight, minHeight);
    pal.setColor(QPalette::Window, chartColors.at(0));
    previewColor->setPalette(pal);
    connect(delPreview, &QToolButton::clicked, this, &TemporaryData::deletePreview);

    // Proxy clips
    proxyColor->setFixedSize(minHeight, minHeight);
    pal.setColor(QPalette::Window, chartColors.at(1));
    proxyColor->setPalette(pal);
    connect(delProxy, &QToolButton::clicked, this, &TemporaryData::deleteProjectProxy);

    // Audio Thumbs
    audioColor->setFixedSize(minHeight, minHeight);
    pal.setColor(QPalette::Window, chartColors.at(2));
    audioColor->setPalette(pal);
    connect(delAudio, &QToolButton::clicked, this, &TemporaryData::deleteAudio);

    // Video Thumbs
    thumbColor->setFixedSize(minHeight, minHeight);
    pal.setColor(QPalette::Window, chartColors.at(3));
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
    pal.setColor(QPalette::Window, chartColors.at(0));
    gTotalColor->setPalette(pal);
    connect(gClean, &QToolButton::clicked, this, &TemporaryData::cleanCache);

    // Selection
    gSelectedColor->setFixedSize(minHeight, minHeight);
    pal.setColor(QPalette::Window, chartColors.at(1));
    gSelectedColor->setPalette(pal);
    connect(gDelete, &QToolButton::clicked, this, &TemporaryData::deleteSelected);

    // Proxy data
    connect(gProxyClean, &QToolButton::clicked, this, &TemporaryData::cleanProxy);
    connect(gProxyDelete, &QToolButton::clicked, this, &TemporaryData::deleteProxy);
    ok = false;
    QDir global = m_doc->getCacheDir(SystemCacheRoot, &ok);
    QDir proxyFolder(global.absoluteFilePath(QStringLiteral("proxy")));
    gProxyPath->setText(QString("<a href='#'>%1</a>").arg(proxyFolder.absolutePath()));
    connect(gProxyPath, &QLabel::linkActivated, [proxyFolder]() {
        QDesktopServices::openUrl(QUrl::fromLocalFile(proxyFolder.absolutePath()));
    });

    // Backup data
    connect(gBackupClean, &QToolButton::clicked, this, &TemporaryData::cleanBackup);
    connect(gBackupDelete, &QToolButton::clicked, this, &TemporaryData::deleteBackup);
    QDir backupFolder(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + QStringLiteral("/.backup"));
    gBackupPath->setText(QString("<a href='#'>%1</a>").arg(backupFolder.absolutePath()));
    connect(gBackupPath, &QLabel::linkActivated, [backupFolder]() {
        QDesktopServices::openUrl(QUrl::fromLocalFile(backupFolder.absolutePath()));
    });

    // Config cleanup age
    gCleanupSpin->setSuffix(i18np(" month", " months", KdenliveSettings::cleanCacheMonths()));
    gCleanupSpin->setValue(KdenliveSettings::cleanCacheMonths());
    connect(gCleanupSpin, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), this, [&] (int value) {
        KdenliveSettings::setCleanCacheMonths(value);
        gCleanupSpin->setSuffix(i18np(" month", " months", KdenliveSettings::cleanCacheMonths()));
    });

    processBackupDirectories();

    connect(listWidget, &QTreeWidget::itemSelectionChanged, this, &TemporaryData::refreshGlobalPie);

    bool globalOnly = pCore->bin()->isEmpty();
    if (currentProjectOnly) {
        tabWidget->removeTab(1);
        //globalPage->setEnabled(false);
    } else if (globalOnly) {
        tabWidget->removeTab(0);
        //projectPage->setEnabled(false);
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
        KIO::DirectorySizeJob *job = KIO::directorySize(QUrl::fromLocalFile(preview.absolutePath()));
        connect(job, &KIO::DirectorySizeJob::result, this, &TemporaryData::gotPreviewSize);
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
        KIO::DirectorySizeJob *job = KIO::directorySize(QUrl::fromLocalFile(preview.absolutePath()));
        connect(job, &KIO::DirectorySizeJob::result, this, &TemporaryData::gotAudioSize);
    }
    preview = m_doc->getCacheDir(CacheThumbs, &ok);
    if (ok) {
        KIO::DirectorySizeJob *job = KIO::directorySize(QUrl::fromLocalFile(preview.absolutePath()));
        connect(job, &KIO::DirectorySizeJob::result, this, &TemporaryData::gotThumbSize);
    }
    if (!m_currentProjectOnly) {
        updateGlobalInfo();
    }
}

void TemporaryData::gotPreviewSize(KJob *job)
{
    auto *sourceJob = static_cast<KIO::DirectorySizeJob *>(job);
    KIO::filesize_t total = sourceJob->totalSize();
    if (sourceJob->totalFiles() == 0) {
        total = 0;
    }
    delPreview->setEnabled(total > 0);
    m_totalCurrent += total;
    m_currentSizes[0] = total;
    previewSize->setText(KIO::convertSize(total));
    updateTotal();
}

void TemporaryData::gotProxySize(KIO::filesize_t total)
{
    delProxy->setEnabled(total > 0);
    m_totalCurrent += total;
    m_currentSizes[1] = total;
    proxySize->setText(KIO::convertSize(total));
    updateTotal();
}

void TemporaryData::gotAudioSize(KJob *job)
{
    auto *sourceJob = static_cast<KIO::DirectorySizeJob *>(job);
    KIO::filesize_t total = sourceJob->totalSize();
    if (sourceJob->totalFiles() == 0) {
        total = 0;
    }
    delAudio->setEnabled(total > 0);
    m_totalCurrent += total;
    m_currentSizes[2] = total;
    audioSize->setText(KIO::convertSize(total));
    updateTotal();
}

void TemporaryData::gotThumbSize(KJob *job)
{
    auto *sourceJob = static_cast<KIO::DirectorySizeJob *>(job);
    KIO::filesize_t total = sourceJob->totalSize();
    if (sourceJob->totalFiles() == 0) {
        total = 0;
    }
    delThumb->setEnabled(total > 0);
    m_totalCurrent += total;
    m_currentSizes[3] = total;
    thumbSize->setText(KIO::convertSize(total));
    updateTotal();
}

void TemporaryData::updateTotal()
{
    currentSize->setText(KIO::convertSize(m_totalCurrent));
    delCurrent->setEnabled(m_totalCurrent > 0);
    QList<int> segments;
    for (KIO::filesize_t size : qAsConst(m_currentSizes)) {
        if (m_totalCurrent == 0) {
            segments << 0;
        } else {
            segments << static_cast<int>(size * 360 / m_totalCurrent);
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
    if (KMessageBox::warningContinueCancel(this, i18n("Delete all data in the preview folder:\n%1\nPreview folder contains the timeline previews, and can be recreated with the source project.", dir.absolutePath())) != KMessageBox::Continue) {
        return;
    }
    if (dir.dirName() == QLatin1String("preview")) {
        dir.removeRecursively();
        dir.mkpath(QStringLiteral("."));
        emit disablePreview();
        updateDataInfo();
    }
}

void TemporaryData::deleteBackup()
{
    QDir backupFolder(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + QStringLiteral("/.backup"));
    if (KMessageBox::warningContinueCancel(this, i18n("Delete all data in the backup folder:\n%1\nA copy of all your project files is kept in this folder for recovery in case of corruption.", backupFolder.absolutePath())) != KMessageBox::Continue) {
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
    for (const QFileInfo &f : qAsConst(files)) {
        if (f.lastModified().addMonths(KdenliveSettings::cleanCacheMonths()) < current) {
            oldFiles << f.fileName();
        }
    }
    if (oldFiles.isEmpty()) {
        KMessageBox::information(this, i18n("No backup data older than %1 months was found.", KdenliveSettings::cleanCacheMonths()));
        return;
    }
    if (KMessageBox::warningContinueCancelList(this, i18n("This will delete backup data for projects older than %1 months.", KdenliveSettings::cleanCacheMonths()), oldFiles) !=
        KMessageBox::Continue) {
        return;
    }
    if (backupFolder.dirName() == QLatin1String(".backup")) {
        for (const QString &f : qAsConst(oldFiles)) {
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
    // Find old backup data ( older than 6 months )
    size_t total = 0;
    QDateTime current = QDateTime::currentDateTime();
    int max = root->childCount();
    for (int i = 0; i< max; i++) {
        QTreeWidgetItem *child = root->child(i);
        if (child->data(2, Qt::UserRole).toDateTime().addMonths(KdenliveSettings::cleanCacheMonths()) < current) {
            emptyDirs << child;
            total += size_t(child->data(1, Qt::UserRole).toLongLong());
        }
    }
    QStringList folders;
    for (auto *item : qAsConst(emptyDirs)) {
        folders << item->data(0, Qt::UserRole).toString();
    }
    if (folders.isEmpty()) {
        KMessageBox::information(this, i18n("No cache data older than %1 months was found.", KdenliveSettings::cleanCacheMonths()));
        return;
    }

    if (KMessageBox::warningContinueCancelList(this, i18n("This will delete cache data (%1) for missing projects or projects older than %2 months.", KIO::convertSize(total), KdenliveSettings::cleanCacheMonths()), folders) !=
        KMessageBox::Continue) {
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
    if (KMessageBox::warningContinueCancelList(this, i18n("Delete all project data in the proxy folder:\n%1\nProxy folder contains the proxy clips for all your projects. This proxies can be recreated from the source clips.", dir.absolutePath()), files) !=
        KMessageBox::Continue) {
        return;
    }
    for (const QString &file : qAsConst(files)) {
        dir.remove(file);
    }
    emit disableProxies();
    updateDataInfo();
}

void TemporaryData::deleteAudio()
{
    bool ok = false;
    QDir dir = m_doc->getCacheDir(CacheAudio, &ok);
    if (!ok) {
        return;
    }
    if (KMessageBox::warningContinueCancel(this, i18n("Delete all data in the cache audio folder:\n%1\nThis folder contains the data for audio thumbnails in this project.", dir.absolutePath())) != KMessageBox::Continue) {
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
    if (KMessageBox::warningContinueCancel(this, i18n("Delete all data in the cache thumbnail folder:\n%1\nThis folder contains the data for video thumbnails in this project.", dir.absolutePath())) != KMessageBox::Continue) {
        return;
    }
    if (dir.dirName() == QLatin1String("videothumbs")) {
        dir.removeRecursively();
        dir.mkpath(QStringLiteral("."));
        updateDataInfo();
    }
}

void TemporaryData::deleteCurrentCacheData()
{
    bool ok = false;
    QDir dir = m_doc->getCacheDir(CacheBase, &ok);
    if (!ok) {
        return;
    }
    if (KMessageBox::warningContinueCancel(this, i18n("Delete all data in the cache folder:\n%1\nCache folder contains the audio and video thumbnails, as well as timeline previews. All this data will be recreated on project opening.", dir.absolutePath())) != KMessageBox::Continue) {
        return;
    }
    if (dir.dirName() == m_doc->getDocumentProperty(QStringLiteral("documentid"))) {
        emit disablePreview();
        emit disableProxies();
        dir.removeRecursively();
        m_doc->initCacheDirs();
        updateDataInfo();
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
    KIO::DirectorySizeJob *job = KIO::directorySize(QUrl::fromLocalFile(backupFolder.absolutePath()));
    connect(job, &KIO::DirectorySizeJob::result, this, &TemporaryData::gotBackupSize);
}

void TemporaryData::processProxyDirectory()
{
    KIO::DirectorySizeJob *job = KIO::directorySize(QUrl::fromLocalFile(m_globalDir.absoluteFilePath(QStringLiteral("proxy"))));
    connect(job, &KIO::DirectorySizeJob::result, this, &TemporaryData::gotProjectProxySize);
}

void TemporaryData::gotProjectProxySize(KJob *job)
{
    auto *sourceJob = static_cast<KIO::DirectorySizeJob *>(job);
    KIO::filesize_t total = sourceJob->totalSize();
    gProxySize->setText(KIO::convertSize(total));
}

void TemporaryData::gotBackupSize(KJob *job)
{
    auto *sourceJob = static_cast<KIO::DirectorySizeJob *>(job);
    KIO::filesize_t total = sourceJob->totalSize();
    if (sourceJob->totalFiles() == 0) {
        total = 0;
    }
    gBackupSize->setText(KIO::convertSize(total));
}

void TemporaryData::updateGlobalInfo()
{
    listWidget->blockSignals(true);
    m_totalGlobal = 0;
    listWidget->clear();
    bool ok = false;
    QDir preview = m_doc->getCacheDir(SystemCacheRoot, &ok);
    if (!ok) {
        globalPage->setEnabled(false);
        return;
    }
    m_globalDir = preview;
    m_globalDirectories.clear();
    m_processingDirectory.clear();
    m_globalDirectories = m_globalDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    // These are some KDE cache dirs relater to Kdenlice that don't manage ourselves
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
    KIO::DirectorySizeJob *job = KIO::directorySize(QUrl::fromLocalFile(m_globalDir.absoluteFilePath(m_processingDirectory)));
    connect(job, &KIO::DirectorySizeJob::result, this, &TemporaryData::gotFolderSize);
}

void TemporaryData::gotFolderSize(KJob *job)
{
    auto *sourceJob = static_cast<KIO::DirectorySizeJob *>(job);
    KIO::filesize_t total = sourceJob->totalSize();
    if (sourceJob->totalFiles() == 0) {
        total = 0;
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
    item->setText(2, date.toString(Qt::SystemLocaleShortDate));
    item->setData(1, Qt::UserRole, total);
    item->setData(2, Qt::UserRole, date);
    listWidget->addTopLevelItem(item);
    listWidget->resizeColumnToContents(0);
    listWidget->resizeColumnToContents(1);
    if (m_globalDirectories.isEmpty()) {
        gTotalSize->setText(KIO::convertSize(m_totalGlobal));
        listWidget->setCurrentItem(listWidget->topLevelItem(0));
    } else {
        processglobalDirectories();
    }
}

void TemporaryData::refreshGlobalPie()
{
    QList<QTreeWidgetItem *> list = listWidget->selectedItems();
    KIO::filesize_t currentSize = 0;
    for (QTreeWidgetItem *current : qAsConst(list)) {
        if (current) {
            currentSize += current->data(1, Qt::UserRole).toULongLong();
        }
    }
    gSelectedSize->setText(KIO::convertSize(currentSize));
    int percent = m_totalGlobal <= 0 ? 0 : int(currentSize * 360 / m_totalGlobal);
    m_globalPie->setSegments(QList<int>() << 360 << percent);
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
    for (QTreeWidgetItem *current : qAsConst(list)) {
        if (current) {
            folders << current->data(0, Qt::UserRole).toString();
        }
    }
    if (KMessageBox::warningContinueCancelList(this, i18n("Delete the following cache folders from\n%1\nCache folders contains the audio and video thumbnails, as well as timeline previews. All this data will be recreated on project opening.", m_globalDir.absolutePath()), folders) !=
        KMessageBox::Continue) {
        return;
    }
    deleteCache(folders);
}

void TemporaryData::deleteCache(QStringList &folders)
{
    const QString currentId = m_doc->getDocumentProperty(QStringLiteral("documentid"));
    for (const QString &folder : qAsConst(folders)) {
        if (folder == currentId) {
            // Trying to delete current project's tmp folder. Do not delete, but clear it
            deleteCurrentCacheData();
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
    for (const QFileInfo &f : qAsConst(files)) {
        if (f.lastModified().addMonths(KdenliveSettings::cleanCacheMonths()) < current) {
            oldFiles << f.fileName();
            size += size_t(f.size());
        }
    }
    if (oldFiles.isEmpty()) {
        KMessageBox::information(this, i18n("No proxy clips older than %1 months was found.", KdenliveSettings::cleanCacheMonths()));
        return;
    }
    if (KMessageBox::warningContinueCancelList(this, i18n("Delete the following proxy clips (%1)\nProxy clips can be recreated on project opening.", KIO::convertSize(size)), oldFiles) !=
        KMessageBox::Continue) {
        return;
    }
    for (const QString &f : qAsConst(oldFiles)) {
        proxies.remove(f);
    }
    processProxyDirectory();
}
