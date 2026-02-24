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
#include <QFileInfo>
#include <QMenu>
#include <QMimeData>
#include <QPainter>
#include <QPainterPath>

TransitionTreeModel::TransitionTreeModel(QObject *parent)
    : AssetTreeModel(parent)
{
    m_assetIconProvider = new AssetIconProvider(false, this);
}

std::shared_ptr<TransitionTreeModel> TransitionTreeModel::construct(bool flat, QObject *parent)
{
    std::shared_ptr<TransitionTreeModel> self(new TransitionTreeModel(parent));
    QList<QVariant> rootData{"Name", "ID", "Type", "isFav", "Includelist", "SupportTenBit", "Icon"};
    self->rootItem = TreeItem::construct(rootData, self, true);

    // We create categories, if requested
    std::shared_ptr<TreeItem> compoCategory, transCategory, lumaCategory;
    if (!flat) {
        compoCategory = self->rootItem->appendChild(QList<QVariant>{i18n("Compositions"), QStringLiteral("root")});
        transCategory = self->rootItem->appendChild(QList<QVariant>{i18n("Transitions"), QStringLiteral("root")});
        lumaCategory = self->rootItem->appendChild(QList<QVariant>{i18n("Lumas"), QStringLiteral("root")});
    }

    // We parse transitions
    auto allTransitions = TransitionsRepository::get()->getNames();
    for (const auto &transition : std::as_const(allTransitions)) {
        std::shared_ptr<TreeItem> targetCategory = compoCategory;
        AssetListType::AssetType type = TransitionsRepository::get()->getType(transition.first);
        if (flat) {
            targetCategory = self->rootItem;
        } else if (type == AssetListType::AssetType::AudioTransition || type == AssetListType::AssetType::VideoTransition) {
            targetCategory = transCategory;
        }

        // we create the data list corresponding to this transition
        bool isFav = KdenliveSettings::favorite_transitions().contains(transition.first);
        bool isPreferred = false;
        bool includeListed = false;
        bool supportsTenBit = false;
        TransitionsRepository::get()->getAttributes(transition.first, &isPreferred, &includeListed, &supportsTenBit);
        QList<QVariant> data{transition.second, transition.first, QVariant::fromValue(type), isFav, 0, true, includeListed, supportsTenBit};

        targetCategory->appendChild(data);
    }

    // Parse Lumas
    self->reparseLumas();

    return self;
}

void TransitionTreeModel::reparseLumas()
{
    const QStringList lumaFiles = pCore->getLumasForProfile();
    // bool isPreferred = false;
    bool includeListed = true;
    bool supportsTenBit = false;
    QPixmap pix(QSize(120, 68));
    QPainterPath path;
    path.addRoundedRect(0.5, 0.5, pix.width() - 1, pix.height() - 1, 4, 4);
    for (auto &f : lumaFiles) {
        QFileInfo fName(f);
        const QString lumaId = QStringLiteral("luma:%1").arg(f);
        bool isFav = KdenliveSettings::favorite_transitions().contains(lumaId);
        QPixmap previewPixmap(f);
        pix.fill(Qt::transparent);
        QPainter p(&pix);
        p.setRenderHint(QPainter::Antialiasing, true);
        p.setClipPath(path);
        p.drawPixmap(0, 0, pix.width(), pix.height(), previewPixmap);
        p.end();
        QList<QVariant> data{fName.baseName(), lumaId,    QVariant::fromValue(AssetListType::AssetType::LumaTransition), isFav, 0, true, includeListed,
                             supportsTenBit,   QIcon(pix)};
        rootItem->appendChild(data);
        TransitionsRepository::get()->addLuma(f);
    }
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
    QString id = item->dataColumn(AssetTreeModel::IdCol).toString();
    if (id.startsWith(QLatin1String("luma:"))) {
        id.remove(0, 5);
    }
    QStringList favs = KdenliveSettings::favorite_transitions();
    if (favorite) {
        favs << id;
    } else {
        favs.removeAll(id);
    }
    KdenliveSettings::setFavorite_transitions(favs);
}

QVariant TransitionTreeModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid()) {
        return QVariant();
    }

    if (role == Qt::DecorationRole) {
        auto type = index.data(AssetTreeModel::TypeRole).value<AssetListType::AssetType>();
        if (type == AssetListType::AssetType::LumaTransition) {
            // Fetch Luma image
            auto item = getItemById(int(index.internalId()));
            return item->dataColumn(AssetTreeModel::IconCol).value<QIcon>();
        }
    }

    return AssetTreeModel::data(index, role);
}

void TransitionTreeModel::deleteEffect(const QModelIndex &) {}

void TransitionTreeModel::editCustomAsset(const QString &, const QString &, const QModelIndex &) {}

QMimeData *TransitionTreeModel::mimeData(const QModelIndexList &indexes) const
{
    QMimeData *mimeData = new QMimeData;
    std::shared_ptr<TreeItem> item = getItemById(int(indexes.first().internalId()));
    if (item) {
        const QString assetId = item->dataColumn(AssetTreeModel::IdCol).toString();
        mimeData->setData(QStringLiteral("kdenlive/composition"), assetId.toUtf8());
        AssetListType::AssetType type = item->dataColumn(TypeCol).value<AssetListType::AssetType>();
        if (type == AssetListType::AssetType::AudioTransition) {
            mimeData->setData(QStringLiteral("type"), "audio");
        }
    }
    return mimeData;
}
