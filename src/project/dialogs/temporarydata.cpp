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
    : QWidget(parent)
    , m_doc(doc)
    , m_globalPage(nullptr)
    , m_globalDelete(nullptr)
{
    chartColors << QColor(Qt::darkRed) << QColor(Qt::darkBlue) << QColor(Qt::darkGreen) << QColor(Qt::darkMagenta);
    m_currentSizes << 0 << 0 << 0 << 0;
    auto *lay = new QVBoxLayout;

    m_currentPage = new QWidget(this);
    m_currentPage->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Maximum);
    m_grid = new QGridLayout;
    m_currentPie = new ChartWidget(this);
    m_grid->addWidget(m_currentPie, 0, 0, 5, 1);
    QPalette pal(palette());
    QFontMetrics ft(font());
    int minHeight = ft.height() / 2;

    // Timeline preview data
    QLabel *color = new QLabel(this);
    color->setFixedSize(minHeight, minHeight);
    pal.setColor(QPalette::Window, chartColors.at(0));
    color->setPalette(pal);
    color->setAutoFillBackground(true);
    m_grid->addWidget(color, 0, 1, Qt::AlignCenter);
    QLabel *preview = new QLabel(i18n("Timeline Preview"), this);
    m_grid->addWidget(preview, 0, 2);
    m_previewSize = new QLabel(this);
    m_grid->addWidget(m_previewSize, 0, 3);
    auto *del = new QToolButton(this);
    del->setIcon(QIcon::fromTheme(QStringLiteral("trash-empty")));
    connect(del, &QToolButton::clicked, this, &TemporaryData::deletePreview);
    del->setEnabled(false);
    m_grid->addWidget(del, 0, 4);

    // Proxy clips
    color = new QLabel(this);
    color->setFixedSize(minHeight, minHeight);
    pal.setColor(QPalette::Window, chartColors.at(1));
    color->setPalette(pal);
    color->setAutoFillBackground(true);
    m_grid->addWidget(color, 1, 1, Qt::AlignCenter);
    preview = new QLabel(i18n("Proxy Clips"), this);
    m_grid->addWidget(preview, 1, 2);
    m_proxySize = new QLabel(this);
    m_grid->addWidget(m_proxySize, 1, 3);
    del = new QToolButton(this);
    del->setIcon(QIcon::fromTheme(QStringLiteral("trash-empty")));
    connect(del, &QToolButton::clicked, this, &TemporaryData::deleteProjectProxy);
    del->setEnabled(false);
    m_grid->addWidget(del, 1, 4);

    // Audio Thumbs
    color = new QLabel(this);
    color->setFixedSize(minHeight, minHeight);
    pal.setColor(QPalette::Window, chartColors.at(2));
    color->setPalette(pal);
    color->setAutoFillBackground(true);
    m_grid->addWidget(color, 2, 1, Qt::AlignCenter);
    preview = new QLabel(i18n("Audio Thumbnails"), this);
    m_grid->addWidget(preview, 2, 2);
    m_audioSize = new QLabel(this);
    m_grid->addWidget(m_audioSize, 2, 3);
    del = new QToolButton(this);
    del->setIcon(QIcon::fromTheme(QStringLiteral("trash-empty")));
    connect(del, &QToolButton::clicked, this, &TemporaryData::deleteAudio);
    del->setEnabled(false);
    m_grid->addWidget(del, 2, 4);

    // Video Thumbs
    color = new QLabel(this);
    color->setFixedSize(minHeight, minHeight);
    pal.setColor(QPalette::Window, chartColors.at(3));
    color->setPalette(pal);
    color->setAutoFillBackground(true);
    m_grid->addWidget(color, 3, 1, Qt::AlignCenter);
    preview = new QLabel(i18n("Video Thumbnails"), this);
    m_grid->addWidget(preview, 3, 2);
    m_thumbSize = new QLabel(this);
    m_grid->addWidget(m_thumbSize, 3, 3);
    del = new QToolButton(this);
    del->setIcon(QIcon::fromTheme(QStringLiteral("trash-empty")));
    connect(del, &QToolButton::clicked, this, &TemporaryData::deleteThumbs);
    del->setEnabled(false);
    m_grid->addWidget(del, 3, 4);
    m_grid->setRowStretch(4, 10);

    QFrame *sep = new QFrame(this);
    sep->setFrameShape(QFrame::HLine);
    m_grid->addWidget(sep, 5, 0, 1, 5);
    // Current total
    preview = new QLabel(i18n("Project total cache data"), this);
    m_grid->addWidget(preview, 6, 0, 1, 3);
    bool ok;
    QDir dir = m_doc->getCacheDir(CacheBase, &ok);
    preview = new QLabel(QStringLiteral("<a href='#'>") + dir.absolutePath() + QStringLiteral("</a>"), this);
    preview->setToolTip(i18n("Click to open cache folder"));
    connect(preview, &QLabel::linkActivated, this, &TemporaryData::openCacheFolder);
    m_grid->addWidget(preview, 7, 0, 1, 5);
    m_currentSize = new QLabel(this);
    m_grid->addWidget(m_currentSize, 6, 3);
    del = new QToolButton(this);
    del->setIcon(QIcon::fromTheme(QStringLiteral("trash-empty")));
    connect(del, &QToolButton::clicked, this, &TemporaryData::deleteCurrentCacheData);
    del->setEnabled(false);
    m_grid->addWidget(del, 6, 4);

    m_currentPage->setLayout(m_grid);
    m_proxies = m_doc->getProxyHashList();
    for (int i = 0; i < m_proxies.count(); i++) {
        m_proxies[i].append(QLatin1Char('*'));
    }

    bool globalOnly = pCore->bin()->isEmpty();
    if (currentProjectOnly) {
        lay->addWidget(m_currentPage);
        auto *spacer = new QSpacerItem(1, 1, QSizePolicy::Maximum, QSizePolicy::MinimumExpanding);
        lay->addSpacerItem(spacer);
    } else {
        auto *tab = new QTabWidget(this);
        if (!globalOnly) {
            tab->addTab(m_currentPage, i18n("Current Project"));
        } else {
            tab->setTabBarAutoHide(true);
            delete m_currentPage;
            m_currentPage = nullptr;
        }
        m_globalPage = new QWidget(this);
        buildGlobalCacheDialog(minHeight);
        tab->addTab(m_globalPage, i18n("All Projects"));
        lay->addWidget(tab);
    }
    setLayout(lay);
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
        m_currentPage->setEnabled(false);
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
            KIO::filesize_t size = 0;
            for (const QFileInfo &info : fList) {
                size += (uint)info.size();
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
    if (m_globalPage) {
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
    QLayoutItem *button = m_grid->itemAtPosition(0, 4);
    if ((button != nullptr) && (button->widget() != nullptr)) {
        button->widget()->setEnabled(total > 0);
    }
    m_totalCurrent += total;
    m_currentSizes[0] = total;
    m_previewSize->setText(KIO::convertSize(total));
    updateTotal();
}

void TemporaryData::gotProxySize(KIO::filesize_t total)
{
    QLayoutItem *button = m_grid->itemAtPosition(1, 4);
    if ((button != nullptr) && (button->widget() != nullptr)) {
        button->widget()->setEnabled(total > 0);
    }
    m_totalCurrent += total;
    m_currentSizes[1] = total;
    m_proxySize->setText(KIO::convertSize(total));
    updateTotal();
}

void TemporaryData::gotAudioSize(KJob *job)
{
    auto *sourceJob = static_cast<KIO::DirectorySizeJob *>(job);
    KIO::filesize_t total = sourceJob->totalSize();
    if (sourceJob->totalFiles() == 0) {
        total = 0;
    }
    QLayoutItem *button = m_grid->itemAtPosition(2, 4);
    if ((button != nullptr) && (button->widget() != nullptr)) {
        button->widget()->setEnabled(total > 0);
    }
    m_totalCurrent += total;
    m_currentSizes[2] = total;
    m_audioSize->setText(KIO::convertSize(total));
    updateTotal();
}

void TemporaryData::gotThumbSize(KJob *job)
{
    auto *sourceJob = static_cast<KIO::DirectorySizeJob *>(job);
    KIO::filesize_t total = sourceJob->totalSize();
    if (sourceJob->totalFiles() == 0) {
        total = 0;
    }
    QLayoutItem *button = m_grid->itemAtPosition(3, 4);
    if ((button != nullptr) && (button->widget() != nullptr)) {
        button->widget()->setEnabled(total > 0);
    }
    m_totalCurrent += total;
    m_currentSizes[3] = total;
    m_thumbSize->setText(KIO::convertSize(total));
    updateTotal();
}

void TemporaryData::updateTotal()
{
    m_currentSize->setText(KIO::convertSize(m_totalCurrent));
    QLayoutItem *button = m_grid->itemAtPosition(5, 4);
    if ((button != nullptr) && (button->widget() != nullptr)) {
        button->widget()->setEnabled(m_totalCurrent > 0);
    }
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
    QList<QTreeWidgetItem *> emptyDirs = m_listWidget->findItems(KIO::convertSize(0), Qt::MatchExactly, 2);
    // Find old dirs
    QTreeWidgetItem *root = m_listWidget->invisibleRootItem();
    if (!root) {
        return;
    }
    // Find old backup data ( older than 6 months )
    KIO::filesize_t total = 0;
    QDateTime current = QDateTime::currentDateTime();
    int max = root->childCount();
    for (int i = 0; i< max; i++) {
        QTreeWidgetItem *child = root->child(i);
        if (child->data(2, Qt::UserRole).toDateTime().addMonths(KdenliveSettings::cleanCacheMonths()) < current) {
            emptyDirs << child;
            total += child->data(1, Qt::UserRole).toLongLong();
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
    const QString currentId = m_doc->getDocumentProperty(QStringLiteral("documentid"));
    for (const QString &folder : qAsConst(folders)) {
        if (folder == currentId) {
            // Trying to delete current project's tmp folder. Do not delete, but clear it
            deleteCurrentCacheData();
            continue;
        }
        if (!m_globalDir.exists(folder)) {
            continue;
        }
        m_globalDir.remove(folder);
    }
    updateGlobalInfo();
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
    m_totalProxySize->setText(KIO::convertSize(total));
}

void TemporaryData::gotBackupSize(KJob *job)
{
    auto *sourceJob = static_cast<KIO::DirectorySizeJob *>(job);
    KIO::filesize_t total = sourceJob->totalSize();
    if (sourceJob->totalFiles() == 0) {
        total = 0;
    }
    m_backupSize->setText(KIO::convertSize(total));
}

void TemporaryData::buildGlobalCacheDialog(int minHeight)
{
    auto *lay = new QVBoxLayout;
    auto *chartLay = new QHBoxLayout;
    auto *treeLay = new QVBoxLayout;
    m_globalPie = new ChartWidget(this);
    m_listWidget = new QTreeWidget(this);
    m_listWidget->setColumnCount(3);
    m_listWidget->setHeaderLabels(QStringList() << i18n("Folder") << i18n("Size") << i18n("Date"));
    m_listWidget->setRootIsDecorated(false);
    m_listWidget->setAlternatingRowColors(true);
    m_listWidget->setSortingEnabled(true);
    m_listWidget->setSelectionMode(QAbstractItemView::ExtendedSelection);
    connect(m_listWidget, &QTreeWidget::itemDoubleClicked, this, [&](QTreeWidgetItem *item, int) {
        QDesktopServices::openUrl(QUrl::fromLocalFile(m_globalDir.absoluteFilePath(item->data(0, Qt::UserRole).toString())));
    });
    treeLay->addWidget(m_listWidget);
    chartLay->addLayout(treeLay);
    chartLay->addWidget(m_globalPie);
    chartLay->setStretch(0, 15);
    chartLay->setStretch(1, 5);
    m_globalPage->setLayout(lay);
    lay->addLayout(chartLay);

    // Total Cache data
    QPalette pal = palette();
    QLabel *color = new QLabel(this);
    color->setFixedSize(minHeight, minHeight);
    pal.setColor(QPalette::Window, chartColors.at(0));
    color->setPalette(pal);
    color->setAutoFillBackground(true);
    auto *labelLay = new QHBoxLayout;
    labelLay->addWidget(color);
    QLabel *lab = new QLabel(i18n("Total Cached Data"), this);
    labelLay->addWidget(lab);
    m_globalSize = new QLabel(i18n("Calculating..."), this);
    labelLay->addWidget(m_globalSize);
    // Cleanup button
    auto *clean = new QToolButton(this);
    clean->setIcon(QIcon::fromTheme(QStringLiteral("edit-clear-history")));
    clean->setToolTip(i18n("Cleanup unused cache"));
    connect(clean, &QToolButton::clicked, this, &TemporaryData::cleanCache);
    labelLay->addStretch(10);
    labelLay->addWidget(clean);
    lay->addLayout(labelLay);

    // Selection
    auto *labelLay2 = new QHBoxLayout;
    color = new QLabel(this);
    color->setFixedSize(minHeight, minHeight);
    pal.setColor(QPalette::Window, chartColors.at(1));
    color->setPalette(pal);
    color->setAutoFillBackground(true);
    labelLay2->addWidget(color);
    lab = new QLabel(i18n("Selected Cached Data"), this);
    labelLay2->addWidget(lab);
    m_selectedSize = new QLabel(i18n("Calculating..."), this);
    labelLay2->addWidget(m_selectedSize);

    // Delete button
    m_globalDelete = new QToolButton(this);
    m_globalDelete->setIcon(QIcon::fromTheme(QStringLiteral("trash-empty")));
    m_globalDelete->setToolTip(i18n("Delete selected cache"));
    connect(m_globalDelete, &QToolButton::clicked, this, &TemporaryData::deleteSelected);
    labelLay2->addStretch(10);
    labelLay2->addWidget(m_globalDelete);
    lay->addLayout(labelLay2);

    QFrame *sep = new QFrame(this);
    sep->setFrameShape(QFrame::HLine);
    lay->addWidget(sep);

    // Proxy data
    QHBoxLayout *hLay = new QHBoxLayout;
    lab = new QLabel(i18n("Total Proxy Data"), this);
    hLay->addWidget(lab);
    m_totalProxySize = new QLabel(i18n("Calculating..."), this);
    hLay->addWidget(m_totalProxySize);
    hLay->addStretch(10);
    // Cleanup button
    auto *clean3 = new QToolButton(this);
    clean3->setIcon(QIcon::fromTheme(QStringLiteral("edit-clear-history")));
    clean3->setToolTip(i18n("Cleanup old proxy files"));
    connect(clean3, &QToolButton::clicked, this, &TemporaryData::cleanProxy);
    hLay->addWidget(clean3);
    auto *del2 = new QToolButton(this);
    del2->setIcon(QIcon::fromTheme(QStringLiteral("trash-empty")));
    del2->setToolTip(i18n("Delete all proxy clips"));
    connect(del2, &QToolButton::clicked, this, &TemporaryData::deleteProxy);
    hLay->addWidget(del2);
    lay->addLayout(hLay);
    bool ok = false;
    QDir global = m_doc->getCacheDir(SystemCacheRoot, &ok);
    QDir proxyFolder(global.absoluteFilePath(QStringLiteral("proxy")));
    auto *url2 = new QLabel(QString("<a href='#'>%1</a>").arg(proxyFolder.absolutePath()), this);
    url2->setToolTip(i18n("Click to open proxy folder"));
    connect(url2, &QLabel::linkActivated, [proxyFolder]() {
        QDesktopServices::openUrl(QUrl::fromLocalFile(proxyFolder.absolutePath()));
    });
    lay->addWidget(url2);

    sep = new QFrame(this);
    sep->setFrameShape(QFrame::HLine);
    lay->addWidget(sep);

    // Backup data
    hLay = new QHBoxLayout;
    lab = new QLabel(i18n("Total Backup Data"), this);
    hLay->addWidget(lab);
    m_backupSize = new QLabel(i18n("Calculating..."), this);
    hLay->addWidget(m_backupSize);
    hLay->addStretch(10);
    // Cleanup button
    auto *clean2 = new QToolButton(this);
    clean2->setIcon(QIcon::fromTheme(QStringLiteral("edit-clear-history")));
    clean2->setToolTip(i18n("Cleanup old backups"));
    connect(clean2, &QToolButton::clicked, this, &TemporaryData::cleanBackup);
    hLay->addWidget(clean2);
    auto *del = new QToolButton(this);
    del->setIcon(QIcon::fromTheme(QStringLiteral("trash-empty")));
    del->setToolTip(i18n("Delete all backup data"));
    connect(del, &QToolButton::clicked, this, &TemporaryData::deleteBackup);
    hLay->addWidget(del);
    lay->addLayout(hLay);
    QDir backupFolder(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + QStringLiteral("/.backup"));
    auto *url = new QLabel(QString("<a href='#'>%1</a>").arg(backupFolder.absolutePath()), this);
    url->setToolTip(i18n("Click to open backup folder"));
    connect(url, &QLabel::linkActivated, [backupFolder]() {
        QDesktopServices::openUrl(QUrl::fromLocalFile(backupFolder.absolutePath()));
    });
    lay->addWidget(url);

    sep = new QFrame(this);
    sep->setFrameShape(QFrame::HLine);
    lay->addWidget(sep);

    // Config cleanup age
    hLay = new QHBoxLayout;
    hLay->addWidget(new QLabel(i18n("Cleanup will delete data older than"), this));
    QSpinBox *age = new QSpinBox(this);
    age->setRange(1, 48);
    age->setSuffix(i18np(" month", " months", KdenliveSettings::cleanCacheMonths()));
    age->setValue(KdenliveSettings::cleanCacheMonths());
    connect(age, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), [&, age] (int value) {
        KdenliveSettings::setCleanCacheMonths(value);
        age->setSuffix(i18np(" month", " months", KdenliveSettings::cleanCacheMonths()));
    });
    hLay->addWidget(age);
    lay->addLayout(hLay);
    lay->addStretch(10);

    processBackupDirectories();

    connect(m_listWidget, &QTreeWidget::itemSelectionChanged, this, &TemporaryData::refreshGlobalPie);
}

void TemporaryData::updateGlobalInfo()
{
    m_listWidget->blockSignals(true);
    m_totalGlobal = 0;
    m_listWidget->clear();
    bool ok = false;
    QDir preview = m_doc->getCacheDir(SystemCacheRoot, &ok);
    if (!ok) {
        m_globalPage->setEnabled(false);
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
    m_globalDelete->setEnabled(!m_globalDirectories.isEmpty());
    processProxyDirectory();
    processglobalDirectories();
    m_listWidget->blockSignals(false);
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
    auto *item = new TreeWidgetItem(m_listWidget);
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
    m_listWidget->addTopLevelItem(item);
    m_listWidget->resizeColumnToContents(0);
    m_listWidget->resizeColumnToContents(1);
    if (m_globalDirectories.isEmpty()) {
        m_globalSize->setText(KIO::convertSize(m_totalGlobal));
        m_listWidget->setCurrentItem(m_listWidget->topLevelItem(0));
    } else {
        processglobalDirectories();
    }
}

void TemporaryData::refreshGlobalPie()
{
    QList<QTreeWidgetItem *> list = m_listWidget->selectedItems();
    KIO::filesize_t currentSize = 0;
    for (QTreeWidgetItem *current : qAsConst(list)) {
        if (current) {
            currentSize += current->data(1, Qt::UserRole).toULongLong();
        }
    }
    m_selectedSize->setText(KIO::convertSize(currentSize));
    int percent = m_totalGlobal <= 0 ? 0 : (int)(currentSize * 360 / m_totalGlobal);
    m_globalPie->setSegments(QList<int>() << 360 << percent);
    if (list.size() == 1 && list.at(0)->text(0) == m_doc->getDocumentProperty(QStringLiteral("documentid"))) {
        m_globalDelete->setToolTip(i18n("Clear current cache"));
    } else {
        m_globalDelete->setToolTip(i18n("Delete selected cache"));
    }
}

void TemporaryData::deleteSelected()
{
    QList<QTreeWidgetItem *> list = m_listWidget->selectedItems();
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
    const QString currentId = m_doc->getDocumentProperty(QStringLiteral("documentid"));
    for (const QString &folder : qAsConst(folders)) {
        if (folder == currentId) {
            // Trying to delete current project's tmp folder. Do not delete, but clear it
            deleteCurrentCacheData();
            continue;
        }
        m_globalDir.remove(folder);
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
    m_globalDir.remove(QStringLiteral("proxy"));
    // We deleted proxy folder, recreate it
    m_globalDir.mkpath(QStringLiteral("."));
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
    KIO::filesize_t size = 0;
    for (const QFileInfo &f : qAsConst(files)) {
        if (f.lastModified().addMonths(KdenliveSettings::cleanCacheMonths()) < current) {
            oldFiles << f.fileName();
            size += f.size();
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
