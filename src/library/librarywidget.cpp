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
#include "project/projectmanager.h"
#include "utils/KoIconUtils.h"
#include "doc/kthumb.h"

#include <QVBoxLayout>
#include <QDateTime>
#include <QStandardPaths>
#include <QTreeWidgetItem>
#include <QInputDialog>
#include <QAction>
#include <QMimeData>
#include <QDropEvent>
#include <QtConcurrent>
#include <QToolBar>
#include <QProgressBar>

#include <klocalizedstring.h>
#include <KMessageBox>
#include <KIO/FileCopyJob>

enum LibraryItem {
    PlayList,
    Clip,
    Folder
};

LibraryTree::LibraryTree(QWidget *parent) : QTreeWidget(parent)
{
    int size = QFontInfo(font()).pixelSize();
    setIconSize(QSize(size * 4, size * 2));
    setDragDropMode(QAbstractItemView::DragDrop);
    setAcceptDrops(true);
    setDragEnabled(true);
    setDropIndicatorShown(true);
    viewport()->setAcceptDrops(true);
}

//virtual
QMimeData * LibraryTree::mimeData(const QList<QTreeWidgetItem *> list) const
{
    QList <QUrl> urls;
    foreach(QTreeWidgetItem *item, list) {
        urls << QUrl::fromLocalFile(item->data(0, Qt::UserRole).toString());
    }
    QMimeData *mime = new QMimeData;
    mime->setUrls(urls);
    return mime;
}

QStringList LibraryTree::mimeTypes() const
{
    return QStringList() << QString("text/uri-list");;
}

void LibraryTree::slotUpdateThumb(const QString &path, const QString &iconPath)
{
    QString name = QUrl::fromLocalFile(path).fileName().section(QLatin1Char('.'), 0, -2);
    QList <QTreeWidgetItem*> list = findItems(name, Qt::MatchExactly | Qt::MatchRecursive);
    foreach(QTreeWidgetItem *item, list) {
        if (item->data(0, Qt::UserRole).toString() == path) {
            // We found our item
            blockSignals(true);
            item->setData(0, Qt::DecorationRole, QIcon(iconPath));
            blockSignals(false);
            break;
        }
    }
}

void LibraryTree::mousePressEvent(QMouseEvent * event)
{
    QTreeWidgetItem *clicked = this->itemAt(event->pos());
    QList <QAction *> act = actions();
    if (clicked) {
        foreach(QAction *a, act) {
            a->setEnabled(true);
        }
    } else {
        // Clicked in empty area, disable clip actions
        clearSelection();
        foreach(QAction *a, act) {
            if (a->data().toInt() == 1) {
                a->setEnabled(false);
            }
        }
    }
    QTreeWidget::mousePressEvent(event);
}

void LibraryTree::dropEvent(QDropEvent *event)
{
    const QMimeData* qMimeData = event->mimeData();
    if (!qMimeData->hasUrls()) return;
    //QTreeWidget::dropEvent(event);
    QList <QUrl> urls = qMimeData->urls();
    QTreeWidgetItem *dropped = this->itemAt(event->pos());
    QString dest;
    if (dropped) {
        dest = dropped->data(0, Qt::UserRole).toString();
        if (dropped->data(0, Qt::UserRole + 2).toInt() != LibraryItem::Folder) {
            dest = QUrl::fromLocalFile(dest).adjusted(QUrl::RemoveFilename).path();
        }
    }
    event->accept();
    emit moveData(urls, dest);
}


LibraryWidget::LibraryWidget(ProjectManager *manager, QWidget *parent) : QWidget(parent)
  , m_manager(manager)
{
    QVBoxLayout *lay = new QVBoxLayout(this);
    m_libraryTree = new LibraryTree(this);
    m_libraryTree->setColumnCount(1);
    m_libraryTree->setHeaderHidden(true);
    m_libraryTree->setDragEnabled(true);
    m_libraryTree->setItemDelegate(new LibraryItemDelegate);
    m_libraryTree->setAlternatingRowColors(true);
    lay->addWidget(m_libraryTree);
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
    QString path = QStandardPaths::writableLocation(QStandardPaths::DataLocation) + "/library";
    m_directory = QDir(path);
    if (!m_directory.exists()) {
        m_directory.mkpath(QStringLiteral("."));
    }

    m_libraryTree->setContextMenuPolicy(Qt::ActionsContextMenu);
    m_timer.setSingleShot(true);
    m_timer.setInterval(4000);
    connect(&m_timer, &QTimer::timeout, m_infoWidget, &KMessageWidget::animatedHide);
    connect(this, &LibraryWidget::thumbReady, m_libraryTree, &LibraryTree::slotUpdateThumb);
    connect(m_libraryTree, &LibraryTree::moveData, this, &LibraryWidget::slotMoveData);
    parseLibrary();
}

void LibraryWidget::setupActions(QList <QAction *>list)
{
    QList <QAction *> menuList;
    m_addAction = new QAction(KoIconUtils::themedIcon(QStringLiteral("list-add")), i18n("Add Clip to Project"), this);
    connect(m_addAction, SIGNAL(triggered(bool)), this, SLOT(slotAddToProject()));
    m_addAction->setData(1);
    m_deleteAction = new QAction(KoIconUtils::themedIcon(QStringLiteral("edit-delete")), i18n("Delete Clip from Library"), this);
    connect(m_deleteAction, SIGNAL(triggered(bool)), this, SLOT(slotDeleteFromLibrary()));
    m_deleteAction->setData(1);
    QAction *addFolder = new QAction(KoIconUtils::themedIcon(QStringLiteral("folder-new")), i18n("Create Folder"), this);
    connect(addFolder, SIGNAL(triggered(bool)), this, SLOT(slotAddFolder()));
    QAction *renameFolder = new QAction(QIcon(), i18n("Rename Item"), this);
    renameFolder->setData(1);
    connect(renameFolder, SIGNAL(triggered(bool)), this, SLOT(slotRenameItem()));
    menuList << m_addAction << addFolder << renameFolder << m_deleteAction;
    m_toolBar->addAction(m_addAction);
    m_toolBar->addSeparator();
    m_toolBar->addAction(addFolder);
    foreach(QAction *action, list) {
        m_toolBar->addAction(action);
        menuList << action;
    }

    // Create spacer
    QWidget* spacer = new QWidget();
    spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_toolBar->addWidget(spacer);
    m_toolBar->addSeparator();
    m_toolBar->addAction(m_deleteAction);
    m_libraryTree->addActions(menuList);
    connect(m_libraryTree, &QTreeWidget::itemSelectionChanged, this, &LibraryWidget::updateActions);
}

void LibraryWidget::parseLibrary()
{
    QString selectedUrl;
    disconnect(m_libraryTree, &LibraryTree::itemChanged, this, &LibraryWidget::slotItemEdited);
    if (m_libraryTree->topLevelItemCount() > 0) {
        // Remember last selected item
        QTreeWidgetItem *current = m_libraryTree->currentItem();
        if (current) {
            selectedUrl = current->data(0, Qt::UserRole).toString();
        }
    }
    m_lastSelectedItem = NULL;
    m_libraryTree->clear();

    // Build folders list
    parseFolder(NULL, m_directory.path(), selectedUrl);

    if (m_lastSelectedItem) {
        m_libraryTree->setCurrentItem(m_lastSelectedItem);
    } else {
        m_libraryTree->setCurrentItem(m_libraryTree->topLevelItem(0));
    }
    m_libraryTree->resizeColumnToContents(0);
    m_libraryTree->setSortingEnabled(true);
    m_libraryTree->sortByColumn(0, Qt::AscendingOrder);
    connect(m_libraryTree, &LibraryTree::itemChanged, this, &LibraryWidget::slotItemEdited, Qt::UniqueConnection);
}

void LibraryWidget::parseFolder(QTreeWidgetItem *parentItem, const QString &folder, const QString &selectedUrl)
{
    QDir directory(folder);
    const QStringList folders = directory.entryList(QDir::AllDirs | QDir::NoDotAndDotDot);
    foreach(const QString &file, folders) {
        QTreeWidgetItem *item;
        if (parentItem) {
            item = new QTreeWidgetItem(parentItem, QStringList() << file);
        } else {
            item = new QTreeWidgetItem(m_libraryTree, QStringList() << file);
        }
        item->setData(0, Qt::UserRole, directory.absoluteFilePath(file));
        item->setData(0, Qt::DecorationRole, KoIconUtils::themedIcon(QStringLiteral("folder")));
        item->setData(0, Qt::UserRole + 2, (int) LibraryItem::Folder);
        item->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled | Qt::ItemIsDragEnabled | Qt::ItemIsDropEnabled | Qt::ItemIsEditable);
        parseFolder(item, directory.absoluteFilePath(file), selectedUrl);
    }

    const QStringList fileList = directory.entryList(QDir::Files);
    foreach(const QString &file, fileList) {
        QFileInfo info(directory.absoluteFilePath(file));
        QTreeWidgetItem *item;
        if (parentItem) {
            item = new QTreeWidgetItem(parentItem, QStringList() << file.section(QLatin1Char('.'), 0, -2));
        } else {
            item = new QTreeWidgetItem(m_libraryTree, QStringList() << file.section(QLatin1Char('.'), 0, -2));
        }
        item->setData(0, Qt::UserRole, directory.absoluteFilePath(file));
        item->setData(0, Qt::UserRole + 1, info.lastModified().toString(Qt::SystemLocaleShortDate));
        if (file.endsWith(".mlt") || file.endsWith(".kdenlive")) {
            item->setData(0, Qt::UserRole + 2, (int) LibraryItem::PlayList);
        } else {
            item->setData(0, Qt::UserRole + 2, (int) LibraryItem::Clip);
        }
        item->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled | Qt::ItemIsDragEnabled | Qt::ItemIsDropEnabled | Qt::ItemIsEditable);
        QString thumbPath = thumbnailPath(directory.absoluteFilePath(file));
        if (!QFile::exists(thumbPath)) {
            // Request thumbnail
            QtConcurrent::run(this, &LibraryWidget::slotSaveThumbnail, directory.absoluteFilePath(file));
        } else {
            //item->setIcon(0, QIcon(thumbPath));
            item->setData(0, Qt::DecorationRole, QIcon(thumbPath));
        }
        if (item->data(0, Qt::UserRole).toString() == selectedUrl) {
            m_lastSelectedItem = item;
        }
    }
}

void LibraryWidget::slotAddToLibrary()
{
    if (!m_manager->hasSelection()) {
        showMessage(i18n("Select clips in timeline for the Library"));
        return;
    }
    bool ok;
    QString name = QInputDialog::getText(this, i18n("Add Clip to Library"), i18n("Enter a name for the clip in Library"), QLineEdit::Normal, QString(), &ok);
    if (name.isEmpty() || !ok) return;
    if (m_directory.exists(name + ".mlt")) {
        //TODO: warn and ask for overwrite / rename
    }
    QString fullPath = m_directory.absoluteFilePath(name + ".mlt");
    m_manager->slotSaveSelection(fullPath);
    parseLibrary();
}

void LibraryWidget::slotSaveThumbnail(const QString &path)
{
    // Thumb needs to have a hidden name or when we drag a folder to bi, thumbnails will also be imported
    QString dest = thumbnailPath(path);
    KThumb::saveThumbnail(path, dest, 80);
    emit thumbReady(path, dest);
}

QString LibraryWidget::thumbnailPath(const QString &path)
{
    QUrl url = QUrl::fromLocalFile(path);
    QDir dir(url.adjusted(QUrl::RemoveFilename).path());
    dir.mkdir(".thumbs");
    return dir.absoluteFilePath(QStringLiteral(".thumbs/") + url.fileName() + QStringLiteral(".png"));
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
    if (!current) return;
    QList <QUrl> list;
    list << QUrl::fromLocalFile(current->data(0, Qt::UserRole).toString());
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
        qDebug()<<" * * *Deleting no item ";
        return;
    }
    QString path = current->data(0, Qt::UserRole).toString();
    if (current->data(0, Qt::UserRole + 2).toInt() == LibraryItem::Folder) {
        // Deleting a folder
        QDir dir(path);
        // Make sure we are really trying to remove a directory located in the library folder
        if (!path.startsWith(m_directory.path())) {
            showMessage(i18n("You are trying to remove an invalid folder: %1", path));
            return;
        }
        const QStringList fileList = dir.entryList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot);
        if (!fileList.isEmpty()) {
            if (KMessageBox::warningContinueCancel(this, i18n("This will delete all playlists in folder: %1", path)) != KMessageBox::Continue) {
                return;
            }
        }
        dir.removeRecursively();
    } else {
        QString message;
        if (current->data(0, Qt::UserRole + 2).toInt() == LibraryItem::PlayList) {
            message = i18n("This will delete the MLT playlist:\n%1", path);
        } else {
            message = i18n("This will delete the file :\n%1", path);
        }
        if (KMessageBox::warningContinueCancel(this, message) != KMessageBox::Continue) {
            return;
        }
        // Remove thumbnail
        QFile::remove(thumbnailPath(path));
        // Remove playlist
        if (!QFile::remove(path)) {
            showMessage(i18n("Error removing %1", path));
        }
    }
    parseLibrary();
}

void LibraryWidget::slotAddFolder()
{
    bool ok;
    QString name = QInputDialog::getText(this, i18n("Add Folder to Library"), i18n("Enter a folder name"), QLineEdit::Normal, QString(), &ok);
    if (name.isEmpty() || !ok) return;
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
        parentFolder = m_directory.path();
    }
    QDir dir(parentFolder);
    if (dir.exists(name)) {
        // TODO: warn user
        return;
    }
    if (!dir.mkdir(name)) {
        showMessage(i18n("Error creating folder %1", name));
        return;
    }
    parseLibrary();
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

void LibraryWidget::slotMoveData(QList <QUrl> urls, QString dest)
{
    if (urls.isEmpty()) return;
    if (dest .isEmpty()) {
        // moving to library's root
        dest = m_directory.path();
    }
    QDir dir(dest);
    if (!dir.exists()) return;
    bool internal = true;
    foreach(const QUrl &url, urls) {
        if (!url.path().startsWith(m_directory.path())) {
            internal = false;
            // Dropped an external file, attempt to copy it to library
            KIO::FileCopyJob *copyJob = KIO::file_copy(url, QUrl::fromLocalFile(dir.absoluteFilePath(url.fileName())));
            connect(copyJob, SIGNAL(finished(KJob *)), this, SLOT(slotDownloadFinished(KJob *)));
            connect(copyJob, SIGNAL(percent(KJob *, unsigned long)), this, SLOT(slotDownloadProgress(KJob *, unsigned long)));
        } else {
            // Internal drag/drop
            dir.rename(url.path(), url.fileName());
            // Move thumbnail
            dir.rename(thumbnailPath(url.path()), thumbnailPath(dir.absoluteFilePath(url.fileName())));
        }
    }
    if (internal) parseLibrary();
}

void LibraryWidget::slotItemEdited(QTreeWidgetItem *item, int column)
{
    if (!item || column != 0) return;
    m_libraryTree->blockSignals(true);
    if (item->data(0, Qt::UserRole + 2).toInt() == LibraryItem::Folder) {
        QDir dir(item->data(0, Qt::UserRole).toString());
        dir.cdUp();
        dir.rename(item->data(0, Qt::UserRole).toString(), item->text(0));
        item->setData(0, Qt::UserRole, dir.absoluteFilePath(item->text(0)));
    } else {
        QString oldPath = item->data(0, Qt::UserRole).toString();
        QDir dir(QUrl::fromLocalFile(oldPath).adjusted(QUrl::RemoveFilename).path());
        dir.rename(oldPath, item->text(0) + oldPath.section(QLatin1Char('.'), -2, -1));
        item->setData(0, Qt::UserRole, dir.absoluteFilePath(item->text(0) + oldPath.section(QLatin1Char('.'), -2, -1)));
        dir.rename(thumbnailPath(oldPath), thumbnailPath(item->data(0, Qt::UserRole).toString()));
    }
    m_libraryTree->blockSignals(false);
}

void LibraryWidget::slotDownloadFinished(KJob *)
{
    m_progressBar->setValue(100);
    m_progressBar->setVisible(false);
    parseLibrary();
}

void LibraryWidget::slotDownloadProgress(KJob *, unsigned long progress)
{
    m_progressBar->setVisible(true);
    m_progressBar->setValue(progress);
}
