/*
    SPDX-FileCopyrightText: 2024 Jean-Baptiste Mardelle
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "effectstackfilter.hpp"
#include "effectitemmodel.hpp"
#include "effectstackmodel.hpp"

EffectStackFilter::EffectStackFilter(QObject *parent)
    : QSortFilterProxyModel(parent)
{
}

bool EffectStackFilter::filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const
{
    auto *model = static_cast<AbstractTreeModel *>(sourceModel());
    QModelIndex row = model->index(sourceRow, 0, sourceParent);
    std::shared_ptr<TreeItem> item = model->getItemById(int(row.internalId()));
    std::shared_ptr<EffectItemModel> effectModel = std::static_pointer_cast<EffectItemModel>(item);
    qDebug() << "=== FILTERING EFFECT: " << effectModel->getAssetId();
    if (effectModel->isHiddenBuiltIn()) {
        qDebug() << "=== FILTERING EFFECT: " << effectModel->getAssetId() << " WILL BE HIDDEN!!!!!!!!!!!!";
        return false;
    }
    qDebug() << "=== FILTERING EFFECT: " << effectModel->getAssetId() << " WILL BE VISIBLE+++++++++";
    return true;
}

bool EffectStackFilter::isVisible(const QModelIndex &sourceIndex)
{
    auto parent = sourceModel()->parent(sourceIndex);
    return filterAcceptsRow(sourceIndex.row(), parent);
}
