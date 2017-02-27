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
#include "utils/KoIconUtils.h"

#include <KLocalizedString>
#include <KIO/DirectorySizeJob>
#include <KMessageBox>

#include <QVBoxLayout>
#include <QGridLayout>
#include <QTabWidget>
#include <QPaintEvent>
#include <QFontMetrics>
#include <QPainter>
#include <QLabel>
#include <QStandardPaths>
#include <QToolButton>
#include <QDesktopServices>
#include <QTreeWidget>
#include <QPushButton>

static QList<QColor> chartColors;

ChartWidget::ChartWidget(QWidget *parent) :
    QWidget(parent)
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
    foreach (int val, m_segments) {
        if (val == 0) {
            ix++;
            continue;
        }
        painter.setBrush(chartColors.at(ix));
        painter.drawPie(pieRect, previous, val * 16);
        previous = val * 16;
        ix ++;
    }
}

TemporaryData::TemporaryData(KdenliveDoc *doc, bool currentProjectOnly, QWidget *parent) :
    QWidget(parent)
    , m_doc(doc)
    , m_globalPage(nullptr)
    , m_globalDelete(nullptr)
{
    chartColors << QColor(Qt::darkRed) << QColor(Qt::darkBlue)  << QColor(Qt::darkGreen) << QColor(Qt::darkMagenta);
    mCurrentSizes << 0 << 0 << 0 << 0;
    QVBoxLayout *lay = new QVBoxLayout;

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
    QToolButton *del = new QToolButton(this);
    del->setIcon(KoIconUtils::themedIcon(QStringLiteral("trash-empty")));
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
    del->setIcon(KoIconUtils::themedIcon(QStringLiteral("trash-empty")));
    connect(del, &QToolButton::clicked, this, &TemporaryData::deleteProxy);
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
    del->setIcon(KoIconUtils::themedIcon(QStringLiteral("trash-empty")));
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
    del->setIcon(KoIconUtils::themedIcon(QStringLiteral("trash-empty")));
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
    del->setIcon(KoIconUtils::themedIcon(QStringLiteral("trash-empty")));
    connect(del, &QToolButton::clicked, this, &TemporaryData::deleteCurrentCacheData);
    del->setEnabled(false);
    m_grid->addWidget(del, 6, 4);

    m_currentPage->setLayout(m_grid);
    m_proxies = m_doc->getProxyHashList();
    for (int i = 0; i < m_proxies.count(); i++) {
        m_proxies[i].append(QLatin1Char('*'));
    }

    if (currentProjectOnly) {
        lay->addWidget(m_currentPage);
        QSpacerItem *spacer = new QSpacerItem(1, 1, QSizePolicy::Maximum, QSizePolicy::MinimumExpanding);
        lay->addSpacerItem(spacer);
    } else {
        QTabWidget *tab = new QTabWidget(this);
        tab->addTab(m_currentPage, i18n("Current Project"));
        m_globalPage = new QWidget(this);
        buildGlobalCacheDialog(minHeight);
        tab->addTab(m_globalPage, i18n("All Projects"));
        lay->addWidget(tab);
    }
    setLayout(lay);
    updateDataInfo();
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
        preview.setNameFilters(m_proxies);
        const QFileInfoList fList = preview.entryInfoList();
        qint64 size = 0;
        for (const QFileInfo &info : fList) {
            size += info.size();
        }
        gotProxySize(size);
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
    KIO::DirectorySizeJob *sourceJob = static_cast<KIO::DirectorySizeJob *>(job);
    qulonglong total = sourceJob->totalSize();
    if (sourceJob->totalFiles() == 0) {
        total = 0;
    }
    QLayoutItem *button = m_grid->itemAtPosition(0, 4);
    if (button && button->widget()) {
        button->widget()->setEnabled(total > 0);
    }
    m_totalCurrent += total;
    mCurrentSizes[0] = total;
    m_previewSize->setText(KIO::convertSize(total));
    updateTotal();
}

void TemporaryData::gotProxySize(qint64 total)
{
    QLayoutItem *button = m_grid->itemAtPosition(1, 4);
    if (button && button->widget()) {
        button->widget()->setEnabled(total > 0);
    }
    m_totalCurrent += total;
    mCurrentSizes[1] = total;
    m_proxySize->setText(KIO::convertSize(total));
    updateTotal();
}

void TemporaryData::gotAudioSize(KJob *job)
{
    KIO::DirectorySizeJob *sourceJob = static_cast<KIO::DirectorySizeJob *>(job);
    qulonglong total = sourceJob->totalSize();
    if (sourceJob->totalFiles() == 0) {
        total = 0;
    }
    QLayoutItem *button = m_grid->itemAtPosition(2, 4);
    if (button && button->widget()) {
        button->widget()->setEnabled(total > 0);
    }
    m_totalCurrent += total;
    mCurrentSizes[2] = total;
    m_audioSize->setText(KIO::convertSize(total));
    updateTotal();
}

void TemporaryData::gotThumbSize(KJob *job)
{
    KIO::DirectorySizeJob *sourceJob = static_cast<KIO::DirectorySizeJob *>(job);
    qulonglong total = sourceJob->totalSize();
    if (sourceJob->totalFiles() == 0) {
        total = 0;
    }
    QLayoutItem *button = m_grid->itemAtPosition(3, 4);
    if (button && button->widget()) {
        button->widget()->setEnabled(total > 0);
    }
    m_totalCurrent += total;
    mCurrentSizes[3] = total;
    m_thumbSize->setText(KIO::convertSize(total));
    updateTotal();
}

void TemporaryData::updateTotal()
{
    m_currentSize->setText(KIO::convertSize(m_totalCurrent));
    QLayoutItem *button = m_grid->itemAtPosition(5, 4);
    if (button && button->widget()) {
        button->widget()->setEnabled(m_totalCurrent > 0);
    }
    QList<int> segments;
    foreach (qulonglong size, mCurrentSizes) {
        if (m_totalCurrent == 0) {
            segments << 0;
        } else {
            segments << size * 360 / m_totalCurrent;
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
    if (KMessageBox::warningContinueCancel(this, i18n("Delete all data in the cache folder:\n%1", dir.absolutePath())) != KMessageBox::Continue) {
        return;
    }
    if (dir.dirName() == QLatin1String("preview")) {
        dir.removeRecursively();
        dir.mkpath(QStringLiteral("."));
        emit disablePreview();
        updateDataInfo();
    }
}

void TemporaryData::deleteProxy()
{
    bool ok = false;
    QDir dir = m_doc->getCacheDir(CacheProxy, &ok);
    if (!ok || dir.dirName() != QLatin1String("proxy")) {
        return;
    }
    dir.setNameFilters(m_proxies);
    QStringList files = dir.entryList(QDir::Files);
    if (KMessageBox::warningContinueCancelList(this, i18n("Delete all project data in the cache proxy folder:\n%1", dir.absolutePath()), files) != KMessageBox::Continue) {
        return;
    }
    foreach (const QString &file, files) {
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
    if (KMessageBox::warningContinueCancel(this, i18n("Delete all data in the cache audio folder:\n%1", dir.absolutePath())) != KMessageBox::Continue) {
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
    if (KMessageBox::warningContinueCancel(this, i18n("Delete all data in the cache thumbnail folder:\n%1", dir.absolutePath())) != KMessageBox::Continue) {
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
    if (KMessageBox::warningContinueCancel(this, i18n("Delete all data in cache folder:\n%1", dir.absolutePath())) != KMessageBox::Continue) {
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

void TemporaryData::buildGlobalCacheDialog(int minHeight)
{
    QGridLayout *lay = new QGridLayout;
    m_globalPie = new ChartWidget(this);
    lay->addWidget(m_globalPie, 0, 0, 1, 1);
    m_listWidget = new QTreeWidget(this);
    m_listWidget->setColumnCount(3);
    m_listWidget->setHeaderLabels(QStringList() << i18n("Folder") << i18n("Size") << i18n("Date"));
    m_listWidget->setRootIsDecorated(false);
    m_listWidget->setAlternatingRowColors(true);
    m_listWidget->setSortingEnabled(true);
    m_listWidget->setSelectionMode(QAbstractItemView::ExtendedSelection);
    lay->addWidget(m_listWidget, 0, 1, 1, 4);
    m_globalPage->setLayout(lay);

    // Total Cache data
    QPalette pal = palette();
    QLabel *color = new QLabel(this);
    color->setFixedSize(minHeight, minHeight);
    pal.setColor(QPalette::Window, chartColors.at(0));
    color->setPalette(pal);
    color->setAutoFillBackground(true);
    lay->addWidget(color, 1, 1, Qt::AlignCenter);
    QLabel *lab = new QLabel(i18n("Total Cached Data"), this);
    lay->addWidget(lab, 1, 2, 1, 1);
    m_globalSize = new QLabel(this);
    lay->addWidget(m_globalSize, 1, 3, 1, 1);

    // Selection
    color = new QLabel(this);
    color->setFixedSize(minHeight, minHeight);
    pal.setColor(QPalette::Window, chartColors.at(1));
    color->setPalette(pal);
    color->setAutoFillBackground(true);
    lay->addWidget(color, 2, 1, Qt::AlignCenter);
    lab = new QLabel(i18n("Selected Cached Data"), this);
    lay->addWidget(lab, 2, 2, 1, 1);
    m_selectedSize = new QLabel(this);
    lay->addWidget(m_selectedSize, 2, 3, 1, 1);
    m_globalDelete = new QPushButton(i18n("Delete selected cache"), this);
    connect(m_globalDelete, &QPushButton::clicked, this, &TemporaryData::deleteSelected);
    lay->addWidget(m_globalDelete, 2, 4, 1, 1);

    lay->setColumnStretch(4, 10);
    lay->setRowStretch(0, 10);
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
    m_globalDelete->setEnabled(!m_globalDirectories.isEmpty());
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
    KIO::DirectorySizeJob *sourceJob = static_cast<KIO::DirectorySizeJob *>(job);
    qulonglong total = sourceJob->totalSize();
    if (sourceJob->totalFiles() == 0) {
        total = 0;
    }
    m_totalGlobal += total;
    TreeWidgetItem *item = new TreeWidgetItem(m_listWidget);
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
            item->setIcon(0, KoIconUtils::themedIcon(QStringLiteral("kdenlive")));
        } else {
            item->setIcon(0, KoIconUtils::themedIcon(QStringLiteral("dialog-close")));
        }
    } else {
        item->setText(0, m_processingDirectory);
        if (m_processingDirectory == QLatin1String("proxy")) {
            item->setIcon(0, KoIconUtils::themedIcon(QStringLiteral("kdenlive-show-video")));
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
    qulonglong currentSize = 0;
    foreach (QTreeWidgetItem *current, list) {
        if (current) {
            currentSize += current->data(1, Qt::UserRole).toULongLong();
        }
    }
    m_selectedSize->setText(KIO::convertSize(currentSize));
    int percent = m_totalGlobal <= 0 ? 0 : (int)(currentSize * 360 / m_totalGlobal);
    m_globalPie->setSegments(QList<int>() << 360 << percent);
    if (list.size() == 1 && list.at(0)->text(0) == m_doc->getDocumentProperty(QStringLiteral("documentid"))) {
        m_globalDelete->setText(i18n("Clear current cache"));
    } else {
        m_globalDelete->setText(i18n("Delete selected cache"));
    }
}

void TemporaryData::deleteSelected()
{
    QList<QTreeWidgetItem *> list = m_listWidget->selectedItems();
    QStringList folders;
    foreach (QTreeWidgetItem *current, list) {
        if (current) {
            folders << current->data(0, Qt::UserRole).toString();
        }
    }
    if (KMessageBox::warningContinueCancelList(this, i18n("Delete the following cache folders from\n%1", m_globalDir.absolutePath()), folders) != KMessageBox::Continue) {
        return;
    }
    const QString currentId = m_doc->getDocumentProperty(QStringLiteral("documentid"));
    foreach (const QString &folder, folders) {
        if (folder == currentId) {
            // Trying to delete current project's tmp folder. Do not delete, but clear it
            deleteCurrentCacheData();
            continue;
        }
        QDir toRemove(m_globalDir.absoluteFilePath(folder));
        toRemove.removeRecursively();
        if (folder == QLatin1String("proxy")) {
            // We deleted proxy folder, recreate it
            toRemove.mkpath(QStringLiteral("."));
        }
    }
    updateGlobalInfo();
}
