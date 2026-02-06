/*
    SPDX-FileCopyrightText: 2017 Nicolas Carion
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "dopesheetmodel.hpp"
#include "assets/keyframes/model/keyframemodellist.hpp"
#include "core.h"
#include "doc/docundostack.hpp"
#include "effects/effectstack/model/effectitemmodel.hpp"
#include "effects/effectstack/model/effectstackmodel.hpp"
#include "macros.hpp"
#include "profiles/profilemodel.hpp"
#include "utils/qcolorutils.h"

#include <QDebug>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLineF>
#include <QSize>
#include <mlt++/Mlt.h>
#include <utility>

DopeSheetModel::DopeSheetModel(QObject *parent)
    : AbstractTreeModel(parent)
    , m_lock(QReadWriteLock::Recursive)
{
}

std::shared_ptr<DopeSheetModel> DopeSheetModel::construct(QObject *parent)
{
    std::shared_ptr<DopeSheetModel> self(new DopeSheetModel(parent));
    QList<QVariant> rootData{"Name"};
    self->rootItem = TreeItem::construct(rootData, self, true);
    return self;
}

QHash<int, QByteArray> DopeSheetModel::roleNames() const
{
    QHash<int, QByteArray> roles;
    roles[NameRole] = "dopeName";
    roles[AssetTypeRole] = "dopeType";
    roles[ModelRole] = "dopeModel";
    roles[IndexRole] = "dopeIndex";
    roles[SelectedRole] = "dopeSelected";
    return roles;
}

QVariant DopeSheetModel::data(const QModelIndex &index, int role) const
{
    READ_LOCK();
    if (!index.isValid()) {
        qDebug() << "Index is not valid" << index;
        return QVariant();
    }
    std::shared_ptr<TreeItem> item = getItemById(int(index.internalId()));
    int itemId = item->getId();
    if (role != NameRole && m_paramsList.find(itemId) == m_paramsList.end()) {
        qDebug() << "Item index is not valid" << itemId << ", ROLE: " << role;
        return QVariant();
    }
    // auto stack = std::static_pointer_cast<EffectItemModel> (parentItem
    qDebug() << "::: QUERYING DATA FOR INDEX: " << index.row() << " = " << role;
    switch (role) {
    case Qt::DisplayRole:
    case Qt::EditRole:
    case NameRole:
        qDebug() << "::: QUERYING DATA FOR INDEX: " << index.row() << " = " << item->dataColumn(0);
        return item->dataColumn(0);
    case ModelRole:
        return QVariant::fromValue(m_paramsList.at(itemId).second.get());
    /*case AssetTypeRole:
        return QVariant::fromValue(it->second.first.second);
    case SelectedRole:
        return m_selectedIndexes.contains(it->first);*/
    default:
        qWarning() << "==== REQUESTING INCORRECT DATA TYPE FOR DOPESHEETMODEL: " << role;
        break;
    }
    return AbstractTreeModel::data(index, role);
}

/*void DopeSheetModel::registerItem(QPersistentModelIndex ix, const QString &name, ParamType type, std::shared_ptr<KeyframeModel> model)
{
    READ_LOCK();
    int insertionRow = static_cast<int>(m_paramsList.size());
    beginInsertRows(QModelIndex(), insertionRow, insertionRow);
    m_paramsList.insert({ix, {{name, type}, model}});
    endInsertRows();
    for (int i = 0; i < rowCount(); i++) {
        QModelIndex index = createIndex(i, 0);
        qDebug()<<"::: PARAM: "<<i<<" = "<<data(index, NameRole);
    }
    qDebug()<<"________________________________________________";
}

void DopeSheetModel::deregisterItem(QPersistentModelIndex ix)
{
    READ_LOCK();
    int row = getRowfromId(ix);
    beginRemoveRows(QModelIndex(), row, row);
    m_paramsList.erase(ix);
    endRemoveRows();
}*/

/*int DopeSheetModel::getRowfromId(QModelIndex mid) const
{
    READ_LOCK();
    Q_ASSERT(m_paramsList.count(mid) > 0);
    return (int)std::distance(m_paramsList.begin(), m_paramsList.find(mid));
}*/

/*void DopeSheetModel::clear()
{
    beginRemoveRows(QModelIndex(), 0, rowCount());
    m_paramsList.clear();
    endRemoveRows();
}*/

void DopeSheetModel::registerStack(std::shared_ptr<EffectStackModel> model)
{
    QWriteLocker locker(&m_lock);
    if (!m_paramsList.empty()) {
        m_paramsList.clear();
        clear();
    }
    if (!model) {
        return;
    }
    int max = model->rowCount();
    for (int i = 0; i < max; i++) {
        std::shared_ptr<AbstractEffectItem> item = model->getEffectStackRow(i);
        if (item->childCount() > 0) {
            // group, create sub stack
            continue;
        }
        std::shared_ptr<EffectItemModel> effectModel = std::static_pointer_cast<EffectItemModel>(item);
        registerAsset(effectModel);
    }
}

void DopeSheetModel::registerAsset(std::shared_ptr<EffectItemModel> effectModel)
{
    READ_LOCK();
    qDebug() << ":::: REGISTERING DOPE EFFECT: " << effectModel->dataColumn(0);
    auto effectItem = TreeItem::construct({effectModel->dataColumn(0).toString()}, shared_from_this(), false);
    std::shared_ptr<KeyframeModelList> keyframes = effectModel->getKeyframeModel();
    QStringList blockedParams = effectModel->data(QModelIndex(), AssetParameterModel::BlockedKeyframesRole).toStringList();
    getRoot()->appendChild(effectItem);
    // Recap line for the effect
    EffectParamInfo pInfo;
    pInfo.id = effectModel->dataColumn(0).toString();
    pInfo.type = ParamType::Double;
    pInfo.row = -1;
    m_paramsList.insert({effectItem->getId(), {pInfo, keyframes->getRecap()}});
    // Loop keyframable parameters
    std::vector<QPersistentModelIndex> indexes = keyframes->getIndexes();
    for (unsigned i = indexes.size(); i-- > 0;) {
        const QPersistentModelIndex ix = indexes.at(i);
        auto km = keyframes->getKeyModel(ix);
        if (blockedParams.contains(effectModel->data(ix, AssetParameterModel::NameRole).toString())) {
            continue;
        }
        auto paramItem = TreeItem::construct({effectModel->data(ix, Qt::DisplayRole).toString()}, shared_from_this(), false);
        effectItem->appendChild(paramItem);
        qDebug() << "::: REGISTERING PARAMETER: " << effectModel->data(ix, Qt::DisplayRole).toString();
        pInfo.id = effectModel->data(ix, Qt::DisplayRole).toString();
        pInfo.type = effectModel->data(ix, AssetParameterModel::TypeRole).value<ParamType>();
        pInfo.row = ix.row();
        m_paramsList.insert({paramItem->getId(), {pInfo, km}});
    }
    connect(effectModel.get(), &AssetParameterModel::dataChanged, this,
            [this, effectItem, effectModel](const QModelIndex &ix1, const QModelIndex & /*ix2*/, const QList<int> &roles) {
                if (roles.contains(AssetParameterModel::BlockedKeyframesRole)) {
                    qDebug() << "KEYFRAMABLE EFFECT CHANGED FOR ROW: " << ix1.row();
                    // Remove and reload keyframable params
                    QStringList blockedParams = effectModel->data(ix1, AssetParameterModel::BlockedKeyframesRole).toStringList();
                    qDebug() << "::::: BLOCKED KEYFRAMES: " << blockedParams;
                    while (effectItem->childCount() > 0) {
                        std::shared_ptr<TreeItem> item = effectItem->child(0);
                        m_paramsList.erase(item->getId());
                        effectItem->removeChild(item);
                    }
                    std::shared_ptr<KeyframeModelList> keyframes = effectModel->getKeyframeModel();
                    std::vector<QPersistentModelIndex> indexes = keyframes->getIndexes();
                    for (unsigned i = indexes.size(); i-- > 0;) {
                        const QPersistentModelIndex ix = indexes.at(i);
                        auto km = keyframes->getKeyModel(ix);
                        if (blockedParams.contains(effectModel->data(ix, AssetParameterModel::NameRole).toString())) {
                            qDebug() << "::::::: HIDDEN PARAM: " << effectModel->data(ix, AssetParameterModel::NameRole).toString();
                            continue;
                        }
                        auto paramItem = TreeItem::construct({effectModel->data(ix, Qt::DisplayRole).toString()}, shared_from_this(), false);
                        effectItem->appendChild(paramItem);
                        qDebug() << "::: REGISTERING PARAMETER: " << effectModel->data(ix, Qt::DisplayRole).toString();
                        EffectParamInfo pInfo;
                        pInfo.id = effectModel->data(ix, Qt::DisplayRole).toString();
                        pInfo.type = effectModel->data(ix, AssetParameterModel::TypeRole).value<ParamType>();
                        pInfo.row = ix.row();
                        m_paramsList.insert({paramItem->getId(), {pInfo, km}});
                    }
                }
            });
}

void DopeSheetModel::updateKeyframeRole(const QModelIndex &ix1, const QModelIndex &ix2, const QList<int> &roles)
{
    if (roles.contains(AssetParameterModel::BlockedKeyframesRole)) {
        qDebug() << "KEYFRAMABLE EFFECT CHANGED FOR ROW: " << ix1.row();
    }
}

void DopeSheetModel::registerItem(const std::shared_ptr<TreeItem> &item)
{
    AbstractTreeModel::registerItem(item);
}
void DopeSheetModel::deregisterItem(int id, TreeItem *item)
{
    AbstractTreeModel::deregisterItem(id, item);
}
