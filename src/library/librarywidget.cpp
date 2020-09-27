/***************************************************************************
 *   Copyright (C) 2016 by Jean-Baptiste Mardelle (jb@kdenlive.org)        *
 *   This file is part of Kdenlive. See www.kdenlive.org.                  *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or         *
 *   modify it under the terms of the GNU General Public License as        *
 *   published by the Free Software Foundation; either version 2 of        *
 *   the License or (at your option) version 3 or any later version        *
 *   accepted by the membership of KDE e.V. (or its successor approved     *
 *   by the membership of KDE e.V.), which shall act as a proxy            *
 *   defined in Section 14 of version 3 of the license.                    *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>. *
 ***************************************************************************/

#include "librarywidget.h"
#include "core.h"
#include "doc/kthumb.h"
#include "kdenlivesettings.h"
#include "project/projectmanager.h"

#include <QAction>
#include <QDropEvent>
#include <QInputDialog>
#include <QMimeData>
#include <QProgressBar>
#include <QStandardPaths>
#include <QToolBar>
#include <QTreeWidgetItem>
#include <QVBoxLayout>

#include <KIO/FileCopyJob>
#include <KMessageBox>
#include <klocalizedstring.h>

enum LibraryItem { PlayList, Clip, Folder };

LibraryTree::LibraryTree(QWidget *parent)
    : QTreeWidget(parent)
{
    int size = QFontInfo(font()).pixelSize();
    setIconSize(QSize(size * 4, size * 2));
    setDragDropMode(QAbstractItemView::DragDrop);
    setAcceptDrops(true);
    setDragEnabled(true);
    setDropIndicatorShown(true);
    viewport()->setAcceptDrops(true);
}

// virtual
QMimeData *LibraryTree::mimeData(const QList<QTreeWidgetItem *> list) const
{
    QList<QUrl> urls;
    urls.reserve(list.count());
    for (QTreeWidgetItem *item : list) {
        urls << QUrl::fromLocalFile(item->data(0, Qt::UserRole).toString());
    }
    auto *mime = new QMimeData;
    mime->setUrls(urls);
    return mime;
}

QStringList LibraryTree::mimeTypes() const
{
    return QStringList() << QStringLiteral("text/uri-list") << QStringLiteral("kdenlive/clip") << QStringLiteral("kdenlive/producerslist");
}

void LibraryTree::slotUpdateThumb(const QString &path, const QString &iconPath)
{
    QString name = QUrl::fromLocalFile(path).fileName();
    QList<QTreeWidgetItem *> list = findItems(name, Qt::MatchExactly | Qt::MatchRecursive);
    for (QTreeWidgetItem *item : qAsConst(list)) {
        if (item->data(0, Qt::UserRole).toString() == path) {
            // We found our item
            blockSignals(true);
            item->setData(0, Qt::DecorationRole, QIcon(iconPath));
            blockSignals(false);
            break;
        }
    }
}

void LibraryTree::slotUpdateThumb(const QString &path, const QPixmap &pix)
{
    QString name = QUrl::fromLocalFile(path).fileName();
    QList<QTreeWidgetItem *> list = findItems(name, Qt::MatchExactly | Qt::MatchRecursive);
    for (QTreeWidgetItem *item : qAsConst(list)) {
        if (item->data(0, Qt::UserRole).toString() == path) {
            // We found our item
            blockSignals(true);
            item->setData(0, Qt::DecorationRole, QIcon(pix));
            blockSignals(false);
            break;
        }
    }
}

void LibraryTree::mousePressEvent(QMouseEvent *event)
{
    QTreeWidgetItem *clicked = this->itemAt(event->pos());
    QList<QAction *> act = actions();
    if (clicked) {
        for (QAction *a : qAsConst(act)) {
            a->setEnabled(true);
        }
    } else {
        // Clicked in empty area, disable clip actions
        clearSelection();
        for (QAction *a : qAsConst(act)) {
            if (a->data().toInt() == 1) {
                a->setEnabled(false);
            }
        }
    }
    QTreeWidget::mousePressEvent(event);
}

void LibraryTree::dropEvent(QDropEvent *event)
{
    // QTreeWidget::dropEvent(event);
    const QMimeData *qMimeData = event->mimeData();
    QTreeWidgetItem *dropped = this->itemAt(event->pos());
    QString dest;
    if (dropped) {
        dest = dropped->data(0, Qt::UserRole).toString();
        if (dropped->data(0, Qt::UserRole + 2).toInt() != LibraryItem::Folder) {
            dest = QUrl::fromLocalFile(dest).adjusted(QUrl::RemoveFilename).toLocalFile();
        }
    }
    if (qMimeData->hasUrls()) {
        QList<QUrl> urls = qMimeData->urls();
        emit moveData(urls, dest);
    } else if (qMimeData->hasFormat(QStringLiteral("kdenlive/clip"))) {
        emit importSequence(QString(qMimeData->data(QStringLiteral("kdenlive/clip"))).split(QLatin1Char(';')), dest);
    } else if (qMimeData->hasFormat(QStringLiteral("kdenlive/producerslist"))) {
        QStringList list = QString(qMimeData->data(QStringLiteral("kdenlive/producerslist"))).split(QLatin1Char(';'));
        for (const QString &prodslist : qAsConst(list)) {
            if (prodslist.startsWith(QLatin1Char('#'))) {
                // Bin folder, not supported yet
                continue;
            }
            if (prodslist.contains(QLatin1Char('/'))) {
                // Clip zone
                emit importSequence(prodslist.split(QLatin1Char('/')), dest);
            } else {
                // Full clip
                emit importSequence(QStringList() << prodslist << QStringLiteral("-1") << QStringLiteral("-1"), dest);
            }
        }
    }
    event->accept();
}

LibraryWidget::LibraryWidget(ProjectManager *manager, QWidget *parent)
    : QWidget(parent)
    , m_manager(manager)
    , m_previewJob(nullptr)
{
    auto *lay = new QVBoxLayout(this);
    m_libraryTree = new LibraryTree(this);
    m_libraryTree->setColumnCount(1);
    m_libraryTree->setHeaderHidden(true);
    m_libraryTree->setDragEnabled(true);
    m_libraryTree->setItemDelegate(new LibraryItemDelegate(this));
    m_libraryTree->setAlternatingRowColors(true);
    lay->addWidget(m_libraryTree);

    // Library path
    QString path;
    if (KdenliveSettings::librarytodefaultfolder() || KdenliveSettings::libraryfolder().isEmpty()) {
        path = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + QStringLiteral("/library");
    } else {
        path = KdenliveSettings::libraryfolder();
    }

    // Info message
    m_infoWidget = new KMessageWidget;
    lay->addWidget(m_infoWidget);
    m_infoWidget->hide();
    // Download progress bar
    m_progressBar = new QProgressBar(this);
    lay->addWidget(m_progressBar);
    m_toolBar = new QToolBar(this);
    m_progressBar->setRange(0, 100);
    m_progressBar->setOrientation(Qt::Horizontal);
    m_progressBar->setVisible(false);
    lay->addWidget(m_toolBar);
    setLayout(lay);

    m_directory = QDir(path);
    if (!m_directory.exists()) {
        m_directory.mkpath(QStringLiteral("."));
    }
    QFileInfo fi(m_directory.absolutePath());
    if (!m_directory.exists() || !fi.isWritable()) {
        // Something went wrong
        showMessage(i18n("Check your settings, Library path is invalid: %1", m_directory.absolutePath()), KMessageWidget::Warning);
        setEnabled(false);
    }

    m_libraryTree->setContextMenuPolicy(Qt::ActionsContextMenu);
    m_timer.setSingleShot(true);
    m_timer.setInterval(4000);
    connect(&m_timer, &QTimer::timeout, m_infoWidget, &KMessageWidget::animatedHide);
    connect(m_libraryTree, &LibraryTree::moveData, this, &LibraryWidget::slotMoveData);
    connect(m_libraryTree, &LibraryTree::importSequence, this, &LibraryWidget::slotSaveSequence);

    m_coreLister = new KCoreDirLister(this);
    m_coreLister->setDelayedMimeTypes(false);
    connect(m_coreLister, &KCoreDirLister::itemsAdded, this, &LibraryWidget::slotItemsAdded);
    connect(m_coreLister, &KCoreDirLister::itemsDeleted, this, &LibraryWidget::slotItemsDeleted);
    connect(m_coreLister, SIGNAL(clear()), this, SLOT(slotClearAll()));
    m_coreLister->openUrl(QUrl::fromLocalFile(m_directory.absolutePath()));
    m_libraryTree->setSortingEnabled(true);
    m_libraryTree->sortByColumn(0, Qt::AscendingOrder);
    connect(m_libraryTree, &LibraryTree::itemChanged, this, &LibraryWidget::slotItemEdited, Qt::UniqueConnection);
}

void LibraryWidget::setupActions(const QList<QAction *> &list)
{
    QList<QAction *> menuList;
    m_addAction = new QAction(QIcon::fromTheme(QStringLiteral("kdenlive-add-clip")), i18n("Add Clip to Project"), this);
    connect(m_addAction, &QAction::triggered, this, &LibraryWidget::slotAddToProject);
    m_addAction->setData(1);
    m_deleteAction = new QAction(QIcon::fromTheme(QStringLiteral("edit-delete")), i18n("Delete Clip from Library"), this);
    connect(m_deleteAction, &QAction::triggered, this, &LibraryWidget::slotDeleteFromLibrary);
    m_deleteAction->setData(1);
    QAction *addFolder = new QAction(QIcon::fromTheme(QStringLiteral("folder-new")), i18n("Create Library Folder"), this);
    connect(addFolder, &QAction::triggered, this, &LibraryWidget::slotAddFolder);
    QAction *renameFolder = new QAction(QIcon(), i18n("Rename Library Clip"), this);
    renameFolder->setData(1);
    connect(renameFolder, &QAction::triggered, this, &LibraryWidget::slotRenameItem);
    menuList << m_addAction << addFolder << renameFolder << m_deleteAction;
    m_toolBar->addAction(m_addAction);
    m_toolBar->addSeparator();
    m_toolBar->addAction(addFolder);
    for (QAction *action : list) {
        m_toolBar->addAction(action);
        menuList << action;
        connect(this, &LibraryWidget::enableAddSelection, action, &QAction::setEnabled);
    }

    // Create spacer
    QWidget *spacer = new QWidget();
    spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_toolBar->addWidget(spacer);
    m_toolBar->addSeparator();
    m_toolBar->addAction(m_deleteAction);
    m_libraryTree->addActions(menuList);
    connect(m_libraryTree, &QTreeWidget::itemSelectionChanged, this, &LibraryWidget::updateActions);
}

void LibraryWidget::slotAddToLibrary()
{
    if (!isEnabled()) {
        return;
    }
    emit saveTimelineSelection(m_directory);
}

void LibraryWidget::showMessage(const QString &text, KMessageWidget::MessageType type)
{
    m_timer.stop();
    m_infoWidget->setText(text);
    m_infoWidget->setWordWrap(m_infoWidget->text().length() > 35);
    m_infoWidget->setMessageType(type);
    m_infoWidget->animatedShow();
    m_timer.start();
}

void LibraryWidget::slotAddToProject()
{
    QTreeWidgetItem *current = m_libraryTree->currentItem();
    if (!current) {
        return;
    }
    const QList<QUrl> list = {QUrl::fromLocalFile(current->data(0, Qt::UserRole).toString())};
    emit addProjectClips(list);
}

void LibraryWidget::updateActions()
{
    QTreeWidgetItem *current = m_libraryTree->currentItem();
    if (!current) {
        m_addAction->setEnabled(false);
        m_deleteAction->setEnabled(false);
        return;
    }
    m_addAction->setEnabled(true);
    m_deleteAction->setEnabled(true);
}

void LibraryWidget::slotDeleteFromLibrary()
{
    QTreeWidgetItem *current = m_libraryTree->currentItem();
    if (!current) {
        qCDebug(KDENLIVE_LOG) << " * * *Deleting no item ";
        return;
    }
    QString path = current->data(0, Qt::UserRole).toString();
    if (path.isEmpty()) {
        return;
    }
    if (current->data(0, Qt::UserRole + 2).toInt() == LibraryItem::Folder) {
        // Deleting a folder
        QDir dir(path);
        // Make sure we are really trying to remove a directory located in the library folder
        if (!path.startsWith(m_directory.absolutePath())) {
            showMessage(i18n("You are trying to remove an invalid folder: %1", path));
            return;
        }
        const QStringList fileList = dir.entryList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot);
        if (!fileList.isEmpty()) {
            if (KMessageBox::warningContinueCancel(this, i18n("This will delete the folder %1, including all playlists in it.\nThis cannot be undone", path)) !=
                KMessageBox::Continue) {
                return;
            }
        }
        dir.removeRecursively();
    } else {
        QString message;
        if (current->data(0, Qt::UserRole + 2).toInt() == LibraryItem::PlayList) {
            message = i18n("This will delete the MLT playlist:\n%1", path);
        } else {
            message = i18n("This will delete the file:\n%1", path);
        }
        if (KMessageBox::warningContinueCancel(this, message) != KMessageBox::Continue) {
            return;
        }
        // Remove playlist
        if (!QFile::remove(path)) {
            showMessage(i18n("Error removing %1", path));
        }
    }
}

void LibraryWidget::slotAddFolder()
{
    bool ok;
    QString name = QInputDialog::getText(this, i18n("Add Folder to Library"), i18n("Enter a folder name"), QLineEdit::Normal, QString(), &ok);
    if (name.isEmpty() || !ok) {
        return;
    }
    QTreeWidgetItem *current = m_libraryTree->currentItem();
    QString parentFolder;
    if (current) {
        if (current->data(0, Qt::UserRole + 2).toInt() == LibraryItem::Folder) {
            // Creating a subfolder
            parentFolder = current->data(0, Qt::UserRole).toString();
        } else {
            QTreeWidgetItem *parentItem = current->parent();
            if (parentItem) {
                parentFolder = parentItem->data(0, Qt::UserRole).toString();
            }
        }
    }
    if (parentFolder.isEmpty()) {
        parentFolder = m_directory.absolutePath();
    }
    QDir dir(parentFolder);
    if (dir.exists(name)) {
        showMessage(i18n("Folder %1 already exists", name));
        return;
    }
    if (!dir.mkdir(name)) {
        showMessage(i18n("Error creating folder %1", name));
        return;
    }
}

void LibraryWidget::slotRenameItem()
{
    QTreeWidgetItem *current = m_libraryTree->currentItem();
    if (!current) {
        // This is not a folder, abort
        return;
    }
    m_libraryTree->editItem(current);
}

void LibraryWidget::slotMoveData(const QList<QUrl> &urls, QString dest)
{
    if (urls.isEmpty()) {
        return;
    }
    if (dest.isEmpty()) {
        // moving to library's root
        dest = m_directory.absolutePath();
    }
    QDir dir(dest);
    if (!dir.exists()) {
        return;
    }
    for (const QUrl &url : urls) {
        if (!url.toLocalFile().startsWith(m_directory.absolutePath())) {
            // Dropped an external file, attempt to copy it to library
            KIO::FileCopyJob *copyJob = KIO::file_copy(url, QUrl::fromLocalFile(dir.absoluteFilePath(url.fileName())));
            connect(copyJob, &KJob::result, this, &LibraryWidget::slotDownloadFinished);
            connect(copyJob, SIGNAL(percent(KJob*,ulong)), this, SLOT(slotDownloadProgress(KJob*,ulong)));
        } else {
            // Internal drag/drop
            dir.rename(url.toLocalFile(), url.fileName());
        }
    }
}

void LibraryWidget::slotSaveSequence(const QStringList &info, QString dest)
{
    if (info.isEmpty()) {
        return;
    }
    if (dest.isEmpty()) {
        // moving to library's root
        dest = m_directory.absolutePath();
    }
    QDir dir(dest);
    if (!dir.exists()) {
        return;
    }
    m_manager->saveZone(info, dir);
}

void LibraryWidget::slotItemEdited(QTreeWidgetItem *item, int column)
{
    if ((item == nullptr) || column != 0) {
        return;
    }
    if (item->data(0, Qt::UserRole + 2).toInt() == LibraryItem::Folder) {
        QDir dir(item->data(0, Qt::UserRole).toString());
        dir.cdUp();
        dir.rename(item->data(0, Qt::UserRole).toString(), item->text(0));
        // item->setData(0, Qt::UserRole, dir.absoluteFilePath(item->text(0)));
    } else {
        QString oldPath = item->data(0, Qt::UserRole).toString();
        QDir dir(QUrl::fromLocalFile(oldPath).adjusted(QUrl::RemoveFilename).toLocalFile());
        dir.rename(oldPath, item->text(0));
        // item->setData(0, Qt::UserRole, dir.absoluteFilePath(item->text(0)));
    }
}

void LibraryWidget::slotDownloadFinished(KJob *)
{
    m_progressBar->setValue(100);
    m_progressBar->setVisible(false);
}

void LibraryWidget::slotDownloadProgress(KJob *, int progress)
{
    m_progressBar->setVisible(true);
    m_progressBar->setValue(progress);
}

void LibraryWidget::slotUpdateLibraryPath()
{
    // Library path changed, reload library with updated path
    m_libraryTree->blockSignals(true);
    m_folders.clear();
    m_libraryTree->clear();
    QString defaultPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + QStringLiteral("/library");
    if (KdenliveSettings::librarytodefaultfolder() || KdenliveSettings::libraryfolder().isEmpty()) {
        m_directory.setPath(defaultPath);
        if (!m_directory.exists()) {
            m_directory.mkpath(QStringLiteral("."));
        }
        showMessage(i18n("Library path set to default: %1", defaultPath), KMessageWidget::Information);
    } else {
        m_directory.setPath(KdenliveSettings::libraryfolder());
        if (!m_directory.exists()) {
            m_directory.mkpath(QStringLiteral("."));
        }
        showMessage(i18n("Library path set to custom: %1", KdenliveSettings::libraryfolder()), KMessageWidget::Information);
    }
    QFileInfo fi(m_directory.absolutePath());
    if (!m_directory.exists() || !fi.isWritable()) {
        // Cannot write to new Library, try default one
        if (m_directory.absolutePath() != defaultPath) {
            showMessage(i18n("Cannot write to Library path: %1, using default", KdenliveSettings::libraryfolder()), KMessageWidget::Warning);
            m_directory.setPath(defaultPath);
            if (!m_directory.exists()) {
                m_directory.mkpath(QStringLiteral("."));
            }
        }
    }
    fi.setFile(m_directory.absolutePath());
    if (!m_directory.exists() || !fi.isWritable()) {
        // Something is really broken, disable library
        showMessage(i18n("Check your settings, Library path is invalid: %1", m_directory.absolutePath()), KMessageWidget::Warning);
        setEnabled(false);
    } else {
        m_coreLister->openUrl(QUrl::fromLocalFile(m_directory.absolutePath()));
        setEnabled(true);
    }
    m_libraryTree->blockSignals(false);
}

void LibraryWidget::slotGotPreview(const KFileItem &item, const QPixmap &pix)
{
    const QString path = item.url().toLocalFile();
    m_libraryTree->blockSignals(true);
    m_libraryTree->slotUpdateThumb(path, pix);
    m_libraryTree->blockSignals(false);
}

void LibraryWidget::slotItemsDeleted(const KFileItemList &list)
{
    m_libraryTree->blockSignals(true);
    QMutexLocker lock(&m_treeMutex);
    for (const KFileItem &fitem : list) {
        QUrl fileUrl = fitem.url();
        QString path;
        if (fitem.isDir()) {
            path = fileUrl.toLocalFile();
        } else {
            path = fileUrl.adjusted(QUrl::RemoveFilename | QUrl::StripTrailingSlash).toLocalFile();
        }
        QTreeWidgetItem *matchingFolder = nullptr;
        if (path != m_directory.absolutePath()) {
            for (QTreeWidgetItem *folder : qAsConst(m_folders)) {
                if (folder->data(0, Qt::UserRole).toString() == path) {
                    // Found parent folder
                    matchingFolder = folder;
                    break;
                }
            }
        }
        if (fitem.isDir()) {
            if (matchingFolder) {
                m_folders.removeAll(matchingFolder);
                // warning, we also need to remove all subfolders since they will be recreated
                QList<QTreeWidgetItem *> subList;
                for (QTreeWidgetItem *folder : qAsConst(m_folders)) {
                    if (folder->data(0, Qt::UserRole).toString().startsWith(path)) {
                        subList << folder;
                    }
                }
                for (QTreeWidgetItem *sub : qAsConst(subList)) {
                    m_folders.removeAll(sub);
                }
                delete matchingFolder;
            }
        } else {
            if (matchingFolder == nullptr) {
                matchingFolder = m_libraryTree->invisibleRootItem();
            }
            for (int i = 0; i < matchingFolder->childCount(); i++) {
                QTreeWidgetItem *item = matchingFolder->child(i);
                if (item->data(0, Qt::UserRole).toString() == fileUrl.toLocalFile()) {
                    // Found deleted item
                    delete item;
                    break;
                }
            }
        }
    }
    m_libraryTree->blockSignals(false);
}

void LibraryWidget::slotItemsAdded(const QUrl &url, const KFileItemList &list)
{
    m_libraryTree->blockSignals(true);
    QMutexLocker lock(&m_treeMutex);
    for (const KFileItem &fitem : list) {
        QUrl fileUrl = fitem.url();
        QString name = fileUrl.fileName();
        QTreeWidgetItem *treeItem;
        QTreeWidgetItem *parent = nullptr;
        if (url != QUrl::fromLocalFile(m_directory.absolutePath())) {
            // not a top level item
            QString directory = fileUrl.adjusted(QUrl::RemoveFilename | QUrl::StripTrailingSlash).toLocalFile();
            for (QTreeWidgetItem *folder : qAsConst(m_folders)) {
                if (folder->data(0, Qt::UserRole).toString() == directory) {
                    // Found parent folder
                    parent = folder;
                    break;
                }
            }
        }
        if (parent) {
            treeItem = new QTreeWidgetItem(parent, QStringList() << name);
        } else {
            treeItem = new QTreeWidgetItem(m_libraryTree, QStringList() << name);
        }
        treeItem->setData(0, Qt::UserRole, fileUrl.toLocalFile());
        treeItem->setData(0, Qt::UserRole + 1, fitem.timeString());
        if (fitem.isDir()) {
            treeItem->setData(0, Qt::UserRole + 2, (int)LibraryItem::Folder);
            m_folders << treeItem;
            m_coreLister->openUrl(fileUrl, KCoreDirLister::Keep);
        } else if (name.endsWith(QLatin1String(".mlt")) || name.endsWith(QLatin1String(".kdenlive"))) {
            treeItem->setData(0, Qt::UserRole + 2, (int)LibraryItem::PlayList);
        } else {
            treeItem->setData(0, Qt::UserRole + 2, (int)LibraryItem::Clip);
        }
        treeItem->setData(0, Qt::DecorationRole, QIcon::fromTheme(fitem.iconName()));
        treeItem->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled | Qt::ItemIsDragEnabled | Qt::ItemIsDropEnabled | Qt::ItemIsEditable);
    }
    QStringList plugins = KIO::PreviewJob::availablePlugins();
    m_previewJob = KIO::filePreview(list, QSize(80, 80), &plugins);
    m_previewJob->setIgnoreMaximumSize();
    connect(m_previewJob, &KIO::PreviewJob::gotPreview, this, &LibraryWidget::slotGotPreview);
    m_libraryTree->blockSignals(false);
}

void LibraryWidget::slotClearAll()
{
    m_libraryTree->blockSignals(true);
    m_folders.clear();
    m_libraryTree->clear();
    m_libraryTree->blockSignals(false);
}
