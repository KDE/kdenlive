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
        if (type == AssetListType::AssetType::AudioTransition || type == AssetListType::AssetType::VideoTransition) {
            targetCategory = transCategory;
        }
        if (flat) {
            targetCategory = self->rootItem;
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
            QMenu *catMenu = new QMenu(item->dataColumn(nameCol).toString(), effectsMenu);
            effectsMenu->addMenu(catMenu);
            for (int j = 0; j < item->childCount(); j++) {
                std::shared_ptr<TreeItem> child = item->child(j);
                QAction *a = new QAction(child->dataColumn(nameCol).toString(), catMenu);
                const QString id = child->dataColumn(idCol).toString();
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
    std::shared_ptr<TreeItem> item = getItemById((int)index.internalId());
    if (isEffect && item->depth() == 1) {
        return;
    }
    item->setData(AssetTreeModel::favCol, favorite);
    auto id = item->dataColumn(AssetTreeModel::idCol).toString();
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

