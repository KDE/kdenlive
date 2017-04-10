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

#include "effecttreemodel.hpp"
#include "effects/effectsrepository.hpp"
#include "abstractmodel/treeitem.hpp"
#include <vector>
#include <QVector>
#include <array>
#include <KLocalizedString>
#include <QDomDocument>
#include <QFile>


#include <QDebug>

EffectTreeModel::EffectTreeModel(const QString &categoryFile, QObject *parent)
    : AssetTreeModel(parent)
{
    QList<QVariant> rootData;
    rootData << "Name" << "ID" << "Type" << "isFav";
    rootItem = new TreeItem(rootData, static_cast<AbstractTreeModel*>(this));

    QHash<QString, TreeItem*> effectCategory; //category in which each effect should land.

    //We parse category file
    if (!categoryFile.isEmpty()) {
        QDomDocument doc;
        QFile file(categoryFile);
        doc.setContent(&file, false);
        file.close();
        QDomNodeList groups = doc.documentElement().elementsByTagName(QStringLiteral("group"));

        for (int i = 0; i < groups.count(); i++) {
            QString groupName = i18n(groups.at(i).firstChild().firstChild().nodeValue().toUtf8().constData());
            QStringList list = groups.at(i).toElement().attribute(QStringLiteral("list")).split(QLatin1Char(','), QString::SkipEmptyParts);

            TreeItem *groupItem = rootItem->appendChild(QList<QVariant>{groupName, QStringLiteral("root")});
            for (const QString& effect : list){
                effectCategory[effect] = groupItem;
            }
        }
    }

    //We also create "Misc" and "Custom" categories
    TreeItem *miscCategory = rootItem->appendChild(QList<QVariant>{i18n("Misc")});
    TreeItem *customCategory = rootItem->appendChild(QList<QVariant>{i18n("Custom")});

    //We parse effects
    auto allEffects = EffectsRepository::get()->getNames();
    for (const auto& effect : allEffects) {
        TreeItem *targetCategory = miscCategory;
        EffectType type = EffectsRepository::get()->getType(effect.first);
        if (effectCategory.contains(effect.first)) {
            targetCategory = effectCategory[effect.first];
        }

        if (type == EffectType::Custom) {
            targetCategory = customCategory;
        }

        // we create the data list corresponding to this profile
        QList<QVariant> data;
        bool isFav = EffectsRepository::get()->isFavorite(effect.first);
        qDebug()<<effect.second << effect.first << "in "<<targetCategory->dataColumn(0).toString();
        data << effect.second << effect.first << QVariant::fromValue(type) << isFav;

        targetCategory->appendChild(data);
    }
}

