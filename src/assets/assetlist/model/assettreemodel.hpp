/***************************************************************************
 *   Copyright (C) 2017 by Nicolas Carion                                  *
 *   This file is part of Kdenlive. See www.kdenlive.org.                  *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) version 3 or any later version accepted by the       *
 *   membership of KDE e.V. (or its successor approved  by the membership  *
 *   of KDE e.V.), which shall act as a proxy defined in Section 14 of     *
 *   version 3 of the license.                                             *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>. *
 ***************************************************************************/

#ifndef ASSETTREEMODEL_H
#define ASSETTREEMODEL_H

#include "abstractmodel/abstracttreemodel.hpp"

/* @brief This class represents an effect hierarchy to be displayed as a tree
 */
class TreeItem;
class QMenu;
class KActionCategory;

class AssetTreeModel : public AbstractTreeModel
{

public:
    explicit AssetTreeModel(QObject *parent = nullptr);

    enum { IdRole = Qt::UserRole + 1, NameRole, FavoriteRole, TypeRole };

    // Helper function to retrieve name
    QString getName(const QModelIndex &index) const;
    // Helper function to retrieve description
    QString getDescription(bool isEffect, const QModelIndex &index) const;
    // Helper function to retrieve if an effect is categorized as favorite
    bool isFavorite(const QModelIndex &index) const;
    QHash<int, QByteArray> roleNames() const override;
    QVariant data(const QModelIndex &index, int role) const override;
    virtual void reloadAssetMenu(QMenu *effectsMenu, KActionCategory *effectActions) = 0;
    virtual void setFavorite(const QModelIndex &index, bool favorite, bool isEffect) = 0;
    virtual void deleteEffect(const QModelIndex &index) = 0;

    // for convenience, we store the column of each data field
    static int nameCol, idCol, favCol, typeCol, preferredCol;

protected:
};

#endif
