/*
    SPDX-FileCopyrightText: 2017 Nicolas Carion
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#ifndef ASSETTREEMODEL_H
#define ASSETTREEMODEL_H

#include "abstractmodel/abstracttreemodel.hpp"

/** @brief This class represents an effect hierarchy to be displayed as a tree
 */
class TreeItem;
class QMenu;
class KActionCategory;

class AssetTreeModel : public AbstractTreeModel
{

public:
    explicit AssetTreeModel(QObject *parent = nullptr);

    enum { IdRole = Qt::UserRole + 1, NameRole, FavoriteRole, TypeRole };
    enum { NameCol = 0, IdCol = 1, TypeCol = 2, FavCol = 3, PreferredCol = 5};

    /** @brief Helper function to retrieve name */
    QString getName(const QModelIndex &index) const;
    /** @brief  Helper function to retrieve description */
    QString getDescription(bool isEffect, const QModelIndex &index) const;
    /** @brief Helper function to retrieve if an effect is categorized as favorite */
    bool isFavorite(const QModelIndex &index) const;
    QHash<int, QByteArray> roleNames() const override;
    QVariant data(const QModelIndex &index, int role) const override;
    virtual void reloadAssetMenu(QMenu *effectsMenu, KActionCategory *effectActions) = 0;
    virtual void setFavorite(const QModelIndex &index, bool favorite, bool isEffect) = 0;
    virtual void deleteEffect(const QModelIndex &index) = 0;
    virtual void editCustomAsset(const QString &newName,const QString &newDescription,const QModelIndex &index) = 0;

protected:
};

#endif
