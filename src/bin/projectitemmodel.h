/*
Copyright (C) 2012  Till Theato <root@ttill.de>
Copyright (C) 2014  Jean-Baptiste Mardelle <jb@kdenlive.org>
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

#ifndef PROJECTITEMMODEL_H
#define PROJECTITEMMODEL_H


#include <QAbstractItemModel>
#include <QSize>

class AbstractProjectItem;
class Bin;

/**
 * @class ProjectItemModel
 * @brief Acts as an adaptor to be able to use BinModel with item views.
 */

class ProjectItemModel : public QAbstractItemModel
{
    Q_OBJECT

public:
    ProjectItemModel(Bin *bin);
    ~ProjectItemModel();

    /** @brief Returns item data depending on role requested */
    QVariant data(const QModelIndex &index, int role) const;
    /** @brief Allow selection and drag & drop */
    Qt::ItemFlags flags(const QModelIndex &index) const;
    /** @brief Returns column names in case we want to use columns in QTreeView */
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const;
    /** @brief Mandatory reimplementation from QAbstractItemModel */
    QModelIndex index(int row, int column, const QModelIndex &parent = QModelIndex()) const;
    /** @brief Mandatory reimplementation from QAbstractItemModel */
    QModelIndex parent(const QModelIndex &index) const;
    /** @brief Mandatory reimplementation from QAbstractItemModel */
    int rowCount(const QModelIndex &parent = QModelIndex()) const;
    /** @brief Mandatory reimplementation from QAbstractItemModel */
    int columnCount(const QModelIndex &parent = QModelIndex()) const;
    /** @brief Returns the mimetype used for Drag actions */
    QStringList mimeTypes() const;
    /** @brief Create data that will be used for Drag events */
    QMimeData *mimeData(const QModelIndexList &indices) const;
    /** @brief Set size for thumbnails */
    void setIconSize(QSize s);
    /** @brief Prepare some stuff before inserting a new item */
    void onAboutToAddItem(AbstractProjectItem *item);
    /** @brief Prepare some stuff after inserting a new item */
    void onItemAdded(AbstractProjectItem *item);
    /** @brief Prepare some stuff before removing a new item */
    void onAboutToRemoveItem(AbstractProjectItem *item);
    /** @brief Prepare some stuff after removing a new item */
    void onItemRemoved(AbstractProjectItem *item);
    bool dropMimeData(const QMimeData *data, Qt::DropAction action, int row, int column, const QModelIndex &parent);
    Qt::DropActions supportedDropActions() const;

public slots:
    /** @brief An item in the list was modified, notify */
    void onItemUpdated(AbstractProjectItem* item);

private:
    /** @brief Reference to the project bin */
    Bin *m_bin;
    /** @brief The default size for thumbnails */
    QSize m_iconSize;

signals:
    //TODO
    void markersNeedUpdate(const QString &id,const QList<int>&);
    void itemDropped(QStringList, const QModelIndex &);
    void itemDropped(const QList <QUrl> &, const QModelIndex &);
};

#endif
