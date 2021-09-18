/*
    SPDX-FileCopyrightText: 2017 Nicolas Carion
    SPDX-License-Identifier: LicenseRef-KDE-Accepted-GPL
*/

#ifndef TRANSITIONTREEMODEL_H
#define TRANSITIONTREEMODEL_H

#include "abstractmodel/abstracttreemodel.hpp"
#include "assets/assetlist/model/assettreemodel.hpp"

/** @brief This class represents a transition hierarchy to be displayed as a tree
 */
class TreeItem;
class TransitionTreeModel : public AssetTreeModel
{

protected:
    explicit TransitionTreeModel(QObject *parent);

public:
    /** @param flat if true, then the categories are not created */
    static std::shared_ptr<TransitionTreeModel> construct(bool flat = false, QObject *parent = nullptr);
    void reloadAssetMenu(QMenu *effectsMenu, KActionCategory *effectActions) override;
    void setFavorite(const QModelIndex &index, bool favorite, bool isEffect) override;
    void deleteEffect(const QModelIndex &index) override;
    void editCustomAsset(const QString newName, const QString newDescription, const QModelIndex &index) override;
protected:
};

#endif
