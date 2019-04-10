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
#include "abstractmodel/treeitem.hpp"
#include "effects/effectsrepository.hpp"
#include "kdenlivesettings.h"
#include <KLocalizedString>
#include <QDomDocument>
#include <QFile>
#include <QVector>
#include <array>
#include <vector>

#include <KActionCategory>
#include <QDebug>
#include <QMenu>

EffectTreeModel::EffectTreeModel(QObject *parent)
    : AssetTreeModel(parent)
    , m_customCategory(nullptr)
{
}

std::shared_ptr<EffectTreeModel> EffectTreeModel::construct(const QString &categoryFile, QObject *parent)
{
    std::shared_ptr<EffectTreeModel> self(new EffectTreeModel(parent));
    QList<QVariant> rootData {"Name", "ID", "Type", "isFav"};
    self->rootItem = TreeItem::construct(rootData, self, true);

    QHash<QString, std::shared_ptr<TreeItem>> effectCategory; // category in which each effect should land.

    std::shared_ptr<TreeItem> miscCategory = nullptr;
    std::shared_ptr<TreeItem> audioCategory = nullptr;
    // We parse category file
    if (!categoryFile.isEmpty()) {
        QDomDocument doc;
        QFile file(categoryFile);
        doc.setContent(&file, false);
        file.close();
        QDomNodeList groups = doc.documentElement().elementsByTagName(QStringLiteral("group"));
        // Create favorite group
        auto groupFav = self->rootItem->appendChild(QList<QVariant>{i18n("Favorites"), QStringLiteral("root"), -1});
        
        effectCategory[QStringLiteral("kdenlive:favorites")] = groupFav;

        auto groupLegacy = self->rootItem->appendChild(QList<QVariant>{i18n("Legacy"), QStringLiteral("root")});

        for (int i = 0; i < groups.count(); i++) {
            QString groupName = i18n(groups.at(i).firstChild().firstChild().nodeValue().toUtf8().constData());
            if (!KdenliveSettings::gpu_accel() && groupName == i18n("GPU effects")) {
                continue;
            }
            QStringList list = groups.at(i).toElement().attribute(QStringLiteral("list")).split(QLatin1Char(','), QString::SkipEmptyParts);
            auto groupItem = self->rootItem->appendChild(QList<QVariant>{groupName, QStringLiteral("root")});
            for (const QString &effect : list) {
                effectCategory[effect] = groupItem;
            }
        }
        // We also create "Misc", "Audio" and "Custom" categories
        miscCategory = self->rootItem->appendChild(QList<QVariant>{i18n("Misc"), QStringLiteral("root")});
        audioCategory = self->rootItem->appendChild(QList<QVariant>{i18n("Audio"), QStringLiteral("root")});
        self->m_customCategory = self->rootItem->appendChild(QList<QVariant>{i18n("Custom"), QStringLiteral("root")});
    } else {
        // Flat view
        miscCategory = self->rootItem;
        audioCategory = self->rootItem;
        self->m_customCategory = self->rootItem;
    }

    // We parse effects
    auto allEffects = EffectsRepository::get()->getNames();
    QString favCategory = QStringLiteral("kdenlive:favorites");
    for (const auto &effect : allEffects) {
        if (!KdenliveSettings::gpu_accel() && effect.first.contains(QLatin1String("movit."))) {
            continue;
        }
        auto targetCategory = miscCategory;
        EffectType type = EffectsRepository::get()->getType(effect.first);
        if (effectCategory.contains(effect.first)) {
            targetCategory = effectCategory[effect.first];
        } else if (type == EffectType::Audio) {
            targetCategory = audioCategory;
        }

        if (type == EffectType::Custom) {
            targetCategory = self->m_customCategory;
        }

        // we create the data list corresponding to this profile
        bool isFav = KdenliveSettings::favorite_effects().contains(effect.first);
        bool isPreferred = EffectsRepository::get()->isPreferred(effect.first);
        //qDebug() << effect.second << effect.first << "in " << targetCategory->dataColumn(0).toString();
        QList<QVariant> data {effect.second, effect.first, QVariant::fromValue(type), isFav, targetCategory->row(), isPreferred};
        if (KdenliveSettings::favorite_effects().contains(effect.first) && effectCategory.contains(favCategory)) {
            targetCategory = effectCategory[favCategory];
        }
        targetCategory->appendChild(data);
    }
    return self;
}

void EffectTreeModel::reloadEffect(const QString &path)
{
    QPair<QString, QString> asset = EffectsRepository::get()->reloadCustom(path);
    if (asset.first.isEmpty() || m_customCategory == nullptr) {
        return;
    }
    bool isFav = KdenliveSettings::favorite_effects().contains(asset.first);
    QList<QVariant> data {asset.second, asset.first, QVariant::fromValue(EffectType::Custom), isFav};
    m_customCategory->appendChild(data);
}

void EffectTreeModel::reloadAssetMenu(QMenu *effectsMenu, KActionCategory *effectActions)
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

void EffectTreeModel::setFavorite(const QModelIndex &index, bool favorite, bool isEffect)
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
    if (!EffectsRepository::get()->exists(id)) {
        qDebug()<<"Trying to reparent unknown asset: "<<id;
        return;
    }
    int max = rootItem->childCount();
    QStringList favs = KdenliveSettings::favorite_effects();
    if (!favorite) {
        int ix = item->dataColumn(4).toInt();
        item->changeParent(rootItem->child(ix));
        favs.removeAll(id);
    } else {
        for (int i = 0; i < max; i++) {
            if (rootItem->child(i)->dataColumn(2).toInt() == -1) {
                item->changeParent(rootItem->child(i));
                break;
            }
        }
        favs << id;
    }
    KdenliveSettings::setFavorite_effects(favs);
}
