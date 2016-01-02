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

#include <QVBoxLayout>
#include <QDateTime>
#include <QStandardPaths>
#include <QTreeWidgetItem>
#include <QInputDialog>
#include <QAction>
#include <QMimeData>

#include <KMessageWidget>
#include <klocalizedstring.h>

LibraryTree::LibraryTree(QWidget *parent) : QTreeWidget(parent)
{}

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


LibraryWidget::LibraryWidget(ProjectManager *manager, QWidget *parent) : QWidget(parent)
  , m_manager(manager)
{
    QVBoxLayout *lay = new QVBoxLayout(this);
    m_libraryTree = new LibraryTree(this);
    m_libraryTree->setColumnCount(2);
    m_libraryTree->setHeaderLabels(QStringList() << i18n("Name") << i18n("Date"));
    m_libraryTree->setRootIsDecorated(false);
    m_libraryTree->setDragEnabled(true);
    //m_libraryTree->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::MinimumExpanding);
    lay->addWidget(m_libraryTree);
    m_infoWidget = new KMessageWidget;
    lay->addWidget(m_infoWidget);
    m_infoWidget->hide();
    m_toolBar = new QToolBar(this);
    lay->addWidget(m_toolBar);
    setLayout(lay);
    QString path = QStandardPaths::writableLocation(QStandardPaths::DataLocation) + "/library";
    m_directory = QDir(path);
    if (!m_directory.exists()) {
        m_directory.mkpath(QStringLiteral("."));
    }
    m_timer.setSingleShot(true);
    m_timer.setInterval(4000);
    connect(&m_timer, &QTimer::timeout, m_infoWidget, &KMessageWidget::animatedHide);
    parseLibrary();
}

void LibraryWidget::setupActions(QList <QAction *>list)
{
    foreach(QAction *action, list) {
        m_toolBar->addAction(action);
    }
}

void LibraryWidget::parseLibrary()
{
    m_libraryTree->clear();
    QStringList filter;
    filter << QStringLiteral("*.mlt");
    const QStringList fileList = m_directory.entryList(filter, QDir::Files);
    QList <QTreeWidgetItem *> items;
    foreach(const QString &file, fileList) {
        QFileInfo info(m_directory.absoluteFilePath(file));
        QTreeWidgetItem *item = new QTreeWidgetItem(QStringList() << file.section(QLatin1Char('.'), 0, -2) << info.lastModified().toString(Qt::SystemLocaleShortDate));
        item->setData(0, Qt::UserRole, m_directory.absoluteFilePath(file));
        items << item;
    }
    m_libraryTree->insertTopLevelItems(0, items);
    m_libraryTree->setSortingEnabled(true);
    m_libraryTree->sortByColumn(0, Qt::AscendingOrder);
}

void LibraryWidget::slotAddToLibrary()
{
    if (!m_manager->hasSelection()) {
        m_timer.stop();
        m_infoWidget->setText(i18n("Select clips in timeline for the Library"));
        m_infoWidget->setWordWrap(m_infoWidget->text().length() > 35);
        m_infoWidget->setMessageType(KMessageWidget::Warning);
        m_infoWidget->animatedShow();
        m_timer.start();
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
