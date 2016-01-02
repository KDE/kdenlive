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

/*!
* @class LibraryWidget
* @brief A "library" that contains a list of clips to be used across projects
* @author Jean-Baptiste Mardelle
*/

#ifndef LIBRARYWIDGET_H
#define LIBRARYWIDGET_H

#include "definitions.h"

#include <QTreeWidget>
#include <QToolBar>
#include <QDir>
#include <QTimer>

#include <KMessageWidget>

class ProjectManager;

class LibraryTree : public QTreeWidget
{
    Q_OBJECT
public:
    explicit LibraryTree(QWidget *parent = 0);

protected:
    virtual QMimeData *mimeData(const QList<QTreeWidgetItem *> list) const;
};


class LibraryWidget : public QWidget
{
    Q_OBJECT

public:
    explicit LibraryWidget(ProjectManager *m_manager, QWidget *parent = 0);
    void setupActions(QList <QAction *>list);

public slots:
    void slotAddToLibrary();

private slots:
    void slotAddToProject();
    void slotDeleteFromLibrary();
    void updateActions();

private:
    LibraryTree *m_libraryTree;
    QToolBar *m_toolBar;
    QAction *m_addAction;
    QAction *m_deleteAction;
    QTimer m_timer;
    KMessageWidget *m_infoWidget;
    ProjectManager *m_manager;
    QDir m_directory;
    void parseLibrary();
    void showMessage(const QString &text, KMessageWidget::MessageType type = KMessageWidget::Warning);

signals:
    void addProjectClips(QList <QUrl>);
};

#endif
