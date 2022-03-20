/*
    SPDX-FileCopyrightText: 2017 Nicolas Carion
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include "abstractmodel/abstracttreemodel.hpp"
#include "assets/assetlist/model/assettreemodel.hpp"

/** @brief This class represents an effect hierarchy to be displayed as a tree
 */
class TreeItem;

class EffectTreeModel : public AssetTreeModel
{

protected:
    explicit EffectTreeModel(QObject *parent = nullptr);

public:
    static std::shared_ptr<EffectTreeModel> construct(const QString &categoryFile, QObject *parent);
    void reloadEffect(const QString &path);
    void reloadEffectFromIndex(const QModelIndex &index);
    void reloadAssetMenu(QMenu *effectsMenu, KActionCategory *effectActions) override;
    void setFavorite(const QModelIndex &index, bool favorite, bool isEffect) override;
    void deleteEffect(const QModelIndex &index) override;
    void editCustomAsset(const QString &newName, const QString &newDescription, const QModelIndex &index) override;

protected:
    std::shared_ptr<TreeItem> m_customCategory;
    std::shared_ptr<TreeItem> m_templateCategory;
};
