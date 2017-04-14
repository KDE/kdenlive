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
#include "transitions/transitionsrepository.hpp"
#include <KLocalizedString>

#include <QDebug>

TransitionTreeModel::TransitionTreeModel(bool flat, QObject *parent)
    : AssetTreeModel(parent)
{
    QList<QVariant> rootData;
    rootData << "Name"
             << "ID"
             << "Type"
             << "isFav";
    rootItem = new TreeItem(rootData, static_cast<AbstractTreeModel *>(this));

    // We create categories, if requested
    TreeItem *compoCategory, *transCategory;
    if (!flat) {
        compoCategory = rootItem->appendChild(QList<QVariant>{i18n("Compositions"), QStringLiteral("root")});
        transCategory = rootItem->appendChild(QList<QVariant>{i18n("Transitions"), QStringLiteral("root")});
    }

    // We parse transitions
    auto allTransitions = TransitionsRepository::get()->getNames();
    for (const auto &transition : allTransitions) {
        TreeItem *targetCategory = compoCategory;
        TransitionType type = TransitionsRepository::get()->getType(transition.first);
        if (type == TransitionType::AudioTransition || type == TransitionType::VideoTransition) {
            targetCategory = transCategory;
        }
        if (flat) {
            targetCategory = rootItem;
        }

        // we create the data list corresponding to this transition
        QList<QVariant> data;
        bool isFav = TransitionsRepository::get()->isFavorite(transition.first);
        qDebug() << transition.second << transition.first << "in " << targetCategory->dataColumn(0).toString();
        data << transition.second << transition.first << QVariant::fromValue(type) << isFav;

        targetCategory->appendChild(data);
    }
}
