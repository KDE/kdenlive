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

#include <QSize>
#include "abstractmodel/abstracttreemodel.hpp"

class AbstractProjectItem;
class ProjectClip;
class ProjectFolder;
class Bin;

/**
 * @class ProjectItemModel
 * @brief Acts as an adaptor to be able to use BinModel with item views.
 */

class ProjectItemModel : public AbstractTreeModel
{
    Q_OBJECT

public:
    explicit ProjectItemModel(Bin* bin, QObject *parent);
    ~ProjectItemModel();

    /** @brief Returns a clip from the hierarchy, given its id
     */
    ProjectClip *getClipByBinID(const QString& binId);

    /** @brief Gets a folder by its id. If none is found, the root is returned
     */
    ProjectFolder *getFolderByBinId(const QString &binId);

    /** @brief Returns some info about the folder containing the given index */
    QStringList getEnclosingFolderInfo(const QModelIndex& index) const;

    /** @brief Deletes all element and start a fresh model */
    void clean();

    /** @brief Convenience method to access root folder */
    ProjectFolder *getRootFolder() const;

    /** @brief Convenience method to acces the bin associated with this model*/
    Bin *bin() const;

    /** @brief Returns item data depending on role requested */
    QVariant data(const QModelIndex &index, int role) const override;
    /** @brief Called when user edits an item */
    bool setData(const QModelIndex &index, const QVariant &value, int role = Qt::EditRole) override;
    /** @brief Allow selection and drag & drop */
    Qt::ItemFlags flags(const QModelIndex &index) const override;
    /** @brief Returns column names in case we want to use columns in QTreeView */
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
    /** @brief Mandatory reimplementation from QAbstractItemModel */
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    /** @brief Returns the mimetype used for Drag actions */
    QStringList mimeTypes() const override;
    /** @brief Create data that will be used for Drag events */
    QMimeData *mimeData(const QModelIndexList &indices) const override;
    /** @brief Set size for thumbnails */
    void setIconSize(QSize s);
    bool dropMimeData(const QMimeData *data, Qt::DropAction action, int row, int column, const QModelIndex &parent) override;
    Qt::DropActions supportedDropActions() const override;

public slots:
    /** @brief An item in the list was modified, notify */
    void onItemUpdated(AbstractProjectItem *item);

private:
    /** @brief Return reference to column specific data */
    int mapToColumn(int column) const;

    Bin *m_bin;

signals:
    //TODO
    void markersNeedUpdate(const QString &id, const QList<int> &);
    void itemDropped(const QStringList &, const QModelIndex &);
    void itemDropped(const QList<QUrl> &, const QModelIndex &);
    void effectDropped(const QString &, const QModelIndex &);
    void addClipCut(const QString &, int, int);
};

#endif
