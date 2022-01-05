/*
    SPDX-FileCopyrightText: 2017 Nicolas Carion
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "transitiontreemodel.hpp"
#include "abstractmodel/treeitem.hpp"
#include "kdenlivesettings.h"
#include "transitions/transitionsrepository.hpp"
#include <KLocalizedString>

#include <KActionCategory>
#include <QDebug>
#include <QMenu>

TransitionTreeModel::TransitionTreeModel(QObject *parent)
    : AssetTreeModel(parent)
{
}
std::shared_ptr<TransitionTreeModel> TransitionTreeModel::construct(bool flat, QObject *parent)
{
    std::shared_ptr<TransitionTreeModel> self(new TransitionTreeModel(parent));
    QList<QVariant> rootData {"Name", "ID", "Type", "isFav"};
    self->rootItem = TreeItem::construct(rootData, self, true);

    // We create categories, if requested
    std::shared_ptr<TreeItem> compoCategory, transCategory;
    if (!flat) {
        compoCategory = self->rootItem->appendChild(QList<QVariant>{i18n("Compositions"), QStringLiteral("root")});
        transCategory = self->rootItem->appendChild(QList<QVariant>{i18n("Transitions"), QStringLiteral("root")});
    }

    // We parse transitions
    auto allTransitions = TransitionsRepository::get()->getNames();
    for (const auto &transition : qAsConst(allTransitions)) {
        std::shared_ptr<TreeItem> targetCategory = compoCategory;
        AssetListType::AssetType type = TransitionsRepository::get()->getType(transition.first);
        if (flat) {
            targetCategory = self->rootItem;
        } else if (type == AssetListType::AssetType::AudioTransition || type == AssetListType::AssetType::VideoTransition) {
            targetCategory = transCategory;
        }

        // we create the data list corresponding to this transition
        bool isFav = KdenliveSettings::favorite_transitions().contains(transition.first);
        //qDebug() << transition.second << transition.first << "in " << targetCategory->dataColumn(0).toString();
        QList<QVariant> data {transition.second, transition.first, QVariant::fromValue(type), isFav};

        targetCategory->appendChild(data);
    }
    return self;
}

void TransitionTreeModel::reloadAssetMenu(QMenu *effectsMenu, KActionCategory *effectActions)
{
    for (int i = 0; i < rowCount(); i++) {
        std::shared_ptr<TreeItem> item = rootItem->child(i);
        if (item->childCount() > 0) {
            QMenu *catMenu = new QMenu(item->dataColumn(NameCol).toString(), effectsMenu);
            effectsMenu->addMenu(catMenu);
            for (int j = 0; j < item->childCount(); j++) {
                std::shared_ptr<TreeItem> child = item->child(j);
                QAction *a = new QAction(child->dataColumn(NameCol).toString(), catMenu);
                const QString id = child->dataColumn(IdCol).toString();
                a->setData(id);
                catMenu->addAction(a);
                effectActions->addAction("transition_" + id, a);
            }
        }
    }
}

void TransitionTreeModel::setFavorite(const QModelIndex &index, bool favorite, bool isEffect)
{
    if (!index.isValid()) {
        return;
    }
    std::shared_ptr<TreeItem> item = getItemById(int(index.internalId()));
    if (isEffect && item->depth() == 1) {
        return;
    }
    item->setData(AssetTreeModel::FavCol, favorite);
    auto id = item->dataColumn(AssetTreeModel::IdCol).toString();
    QStringList favs = KdenliveSettings::favorite_transitions();
    if (favorite) {
        favs << id;
    } else {
        favs.removeAll(id);
    }
    KdenliveSettings::setFavorite_transitions(favs);
}

void TransitionTreeModel::deleteEffect(const QModelIndex &)
{
}

void TransitionTreeModel::editCustomAsset(const QString&, const QString&, const QModelIndex &)
{
}
