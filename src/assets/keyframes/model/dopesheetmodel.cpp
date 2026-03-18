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
#include <qapplication.h>
#include <qclipboard.h>
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
    m_model.reset();
    m_model = std::move(model);
    Q_EMIT dopeDurationChanged();
    Q_EMIT dopePositionChanged();
    if (m_model) {
        connect(m_model.get(), &QAbstractItemModel::rowsInserted, this, &DopeSheetModel::loadEffects, Qt::QueuedConnection);
        connect(m_model.get(), &QAbstractItemModel::rowsRemoved, this, &DopeSheetModel::loadEffects, Qt::QueuedConnection);
        connect(m_model.get(), &QAbstractItemModel::rowsMoved, this, &DopeSheetModel::loadEffects, Qt::QueuedConnection);
    }
    loadEffects();
}

void DopeSheetModel::loadEffects()
{
    QWriteLocker locker(&m_lock);
    if (!m_paramsList.empty()) {
        m_paramsList.clear();
        clear();
    }
    for (auto &c : m_connectionList) {
        QObject::disconnect(c);
    }
    m_connectionList.clear();
    if (!m_model) {
        return;
    }
    int max = m_model->rowCount();
    for (int i = 0; i < max; i++) {
        std::shared_ptr<AbstractEffectItem> item = m_model->getEffectStackRow(i);
        if (item->childCount() > 0) {
            // group, create sub stack
            continue;
        }
        std::shared_ptr<EffectItemModel> effectModel = std::static_pointer_cast<EffectItemModel>(item);
        registerAsset(i, effectModel);
    }
}

void DopeSheetModel::registerAsset(int i, std::shared_ptr<EffectItemModel> effectModel)
{
    qDebug() << ":::: REGISTERING DOPE EFFECT: " << effectModel->dataColumn(0);
    auto effectItem = TreeItem::construct({effectModel->dataColumn(0).toString(), i}, shared_from_this(), false);
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
    auto connection = connect(effectModel.get(), &AssetParameterModel::dataChanged, this,
                              [this, effectItem, effectModel](const QModelIndex &ix1, const QModelIndex & /*ix2*/, const QList<int> &roles) {
                                  if (roles.contains(AssetParameterModel::ParentDurationRole)) {
                                      Q_EMIT dopeDurationChanged();
                                  }
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
    m_connectionList << connection;
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

void DopeSheetModel::buildMasterSelection(const QModelIndex &ix, int index)
{
    m_relatedMove.clear();
    if (index == -1) {
        return;
    }
    KeyframeModel *master = data(ix, ModelRole).value<KeyframeModel *>();
    GenTime position = master->getPosAtIndex(index);
    int itemId = int(ix.internalId());
    auto tItem = getItemById(itemId);
    for (int j = 0; j < tItem->childCount(); ++j) {
        auto current = tItem->child(j);
        auto ix2 = getIndexFromItem(current);
        KeyframeModel *km = data(ix2, ModelRole).value<KeyframeModel *>();
        if (km->hasKeyframe(position)) {
            m_relatedMove.insert(ix2, km->getIndexForPos(position));
        }
    }
}

void DopeSheetModel::moveKeyframe(QVariantMap kfData, int sourcePos, int updatedPos, bool logUndo)
{
    Fun undo = []() { return true; };
    Fun redo = []() { return true; };
    const QMap<QModelIndex, QVariant> selection = sanitizeKeyframesIndexes(kfData);
    bool success = true;
    const GenTime offset(updatedPos - sourcePos, pCore->getCurrentFps());
    for (auto i = selection.cbegin(), end = selection.cend(); i != end; ++i) {
        int itemId = int(i.key().internalId());
        auto tItem = getItemById(itemId);
        if (tItem->depth() == 1) {
            // Summary item, ignore
            continue;
        }
        KeyframeModel *km = data(i.key(), ModelRole).value<KeyframeModel *>();
        if (km && success) {
            const QVariantList indexes = i.value().toList();
            if (indexes.size() > 1) {
                QVector<int> selection;
                for (auto &k : indexes) {
                    selection << k.toInt();
                }
                km->setSelectedKeyframes(selection);
            } else {
                km->setSelectedKeyframes({});
            }
            GenTime pos = km->getPosAtIndex(indexes.first().toInt());
            GenTime updatedPos = pos + offset;
            if (updatedPos < GenTime()) {
                success = false;
                qDebug() << "------\n\nABORTING MOVE FORM: " << pos.frames(25) << " TO " << updatedPos.frames(25) << "";
                break;
            }
            qDebug() << "::: MOVING KEYFRAME TO: " << updatedPos.frames(25) << "; OFFSET: " << offset.frames(25) << ", PREV: " << pos.frames(25)
                     << "\n**************************";
            success = success && km->moveKeyframe(pos, updatedPos, QVariant(), undo, redo);
            if (success && logUndo) {
                km->setSelectedKeyframes({});
            }
        }
    }
    if (success) {
        if (logUndo) {
            pCore->pushUndo(undo, redo, i18n("Move keyframes"));
        }
    } else {
        undo();
        if (logUndo) {
            pCore->displayMessage(i18n("Failed to move keyframe"), InformationMessage);
        }
    }
}

void DopeSheetModel::addKeyframe(const QModelIndex &ix, int framePosition)
{
    int itemId = int(ix.internalId());
    auto tItem = getItemById(itemId);
    Fun undo = []() { return true; };
    Fun redo = []() { return true; };
    bool success = false;
    if (tItem->childCount() == 0) {
        // Single parameter
        KeyframeModel *km = data(ix, ModelRole).value<KeyframeModel *>();
        success = km->addKeyframe(framePosition, undo, redo);
    } else {
        for (int j = 0; j < tItem->childCount(); ++j) {
            auto current = tItem->child(j);
            auto ix2 = getIndexFromItem(current);
            KeyframeModel *km = data(ix2, ModelRole).value<KeyframeModel *>();
            success = km->addKeyframe(framePosition, undo, redo);
        }
    }
    if (success) {
        pCore->pushUndo(undo, redo, i18n("Add keyframes"));
    } else {
        undo();
        pCore->displayMessage(i18n("Failed to add keyframe"), InformationMessage);
    }
}

void DopeSheetModel::removeKeyframe(const QModelIndex &ix, int framePos)
{
    int itemId = int(ix.internalId());
    auto tItem = getItemById(itemId);
    Fun undo = []() { return true; };
    Fun redo = []() { return true; };
    GenTime position = GenTime(framePos, pCore->getCurrentFps());
    bool success = false;
    if (tItem->childCount() == 0) {
        // Single parameter
        KeyframeModel *km = data(ix, ModelRole).value<KeyframeModel *>();
        if (km->hasKeyframe(position)) {
            success = km->removeKeyframe(position, undo, redo);
        }
    } else {
        for (int j = 0; j < tItem->childCount(); ++j) {
            auto current = tItem->child(j);
            auto ix2 = getIndexFromItem(current);
            KeyframeModel *km = data(ix2, ModelRole).value<KeyframeModel *>();
            if (km->hasKeyframe(position)) {
                success = km->removeKeyframe(position, undo, redo);
            }
        }
    }
    if (success) {
        pCore->pushUndo(undo, redo, i18n("Remove keyframe"));
    } else {
        undo();
        pCore->displayMessage(i18n("Failed to remove keyframe"), InformationMessage);
    }
}

void DopeSheetModel::removeKeyframes(QVariantList indexes, QVariantList keyframes)
{
    Fun undo = []() { return true; };
    Fun redo = []() { return true; };
    for (int i = 0; i < indexes.size(); i++) {
        QModelIndex ix = indexes.at(i).toModelIndex();
        QVariantList kfrs = keyframes.at(i).toList();
        int itemId = int(ix.internalId());
        auto tItem = getItemById(itemId);
        qDebug() << "REMOVING ON DEPTH: " << tItem->depth();
        if (tItem->depth() == 1) {
            // deleting keyframe in all parameters
            QList<GenTime> positions;
            KeyframeModel *master = data(ix, ModelRole).value<KeyframeModel *>();
            for (auto &id : kfrs) {
                positions << master->getPosAtIndex(id.toInt());
            }
            for (int j = 0; j < tItem->childCount(); ++j) {
                auto current = tItem->child(j);
                auto ix2 = getIndexFromItem(current);
                KeyframeModel *km = data(ix2, ModelRole).value<KeyframeModel *>();
                for (auto &p : positions) {
                    if (km->hasKeyframe(p)) {
                        km->removeKeyframe(p, undo, redo, true);
                    }
                }
            }
        } else {
            KeyframeModel *km = data(ix, ModelRole).value<KeyframeModel *>();
            for (auto &id : kfrs) {
                km->removeKeyframe(km->getPosAtIndex(id.toInt()), undo, redo, true);
            }
        }
    }
    pCore->pushUndo(undo, redo, i18n("Delete keyframes"));
}

QVariantMap DopeSheetModel::selectKeyframeAtPos(const QModelIndex &masterIndex, int frame)
{
    QVariantMap keyframeIndexes;
    m_selectedIndexes.clear();
    QVariantMap currentMap = selectKeyframeByRange(masterIndex, frame, -1);
    for (auto i = currentMap.cbegin(), end = currentMap.cend(); i != end; ++i) {
        keyframeIndexes.insert(i.key(), i.value());
    }
    return keyframeIndexes;
}

QVariantMap DopeSheetModel::selectKeyframeRange(const QModelIndex &startIndex, const QModelIndex &endIndex, int startFrame, int endFrame)
{
    QVariantList paramIndexes;
    QVariantMap keyframeIndexes;
    QList<QModelIndex> processed;
    m_selectedIndexes.clear();
    bool stopParsing = false;
    QModelIndex currentIndex = startIndex;
    while (!stopParsing && currentIndex.isValid()) {
        QVariantMap currentMap = selectKeyframeByRange(currentIndex, startFrame, endFrame);
        for (auto i = currentMap.cbegin(), end = currentMap.cend(); i != end; ++i) {
            keyframeIndexes.insert(i.key(), i.value());
        }
        if (currentIndex == endIndex) {
            stopParsing = true;
            currentIndex = QModelIndex();
        } else {
            int itemId = int(currentIndex.internalId());
            currentIndex = QModelIndex();
            auto tItem = getItemById(itemId);
            if (tItem->depth() == 1) {
                // Top level item
                int currentRow = tItem->row();
                if (currentRow < getRoot()->childCount() - 1) {
                    auto nextItem = getRoot()->child(++currentRow);
                    if (!nextItem) {
                        stopParsing = true;
                    } else {
                        currentIndex = getIndexFromItem(nextItem);
                    }
                } else {
                    // No more item, abort
                    qDebug() << "// Reached last item...";
                }
            } else {
                // Loop children
                auto parentItem = tItem->parentItem();
                int currentRow = tItem->row();
                if (auto ptr = parentItem.lock()) {
                    int siblings = ptr->childCount();
                    qDebug() << ":::: SELECTING ITEM in ROW: " << currentRow << ", MAX: " << ptr->childCount();
                    while (currentRow < siblings) {
                        qDebug() << "_____ CHECKING EFFECT ROW: " << currentRow;
                        auto nextItem = getRoot()->child(currentRow++);
                        qDebug() << "_____ CHECKING EFFECT ROW DONE; NXT: " << currentRow;
                        if (nextItem) {
                            currentIndex = getIndexFromItem(nextItem);
                            break;
                        }
                    }
                    if (currentRow == siblings) {
                        stopParsing = true;
                    }
                }
            }
        }
    }
    return keyframeIndexes;
}

QVariantMap DopeSheetModel::selectKeyframeByRange(const QModelIndex &startIndex, int startFrame, int endFrame)
{
    int itemId = int(startIndex.internalId());
    QVariantList paramIndexes;
    QVariantMap keyframeIndexes;
    auto tItem = getItemById(itemId);
    QList<QModelIndex> processed;
    if (tItem->depth() == 1) {
        // Top level, select all params
        processed << startIndex;
        QVariantList currentKeyframeIndexes = processIndex(startIndex, startFrame, endFrame);
        if (currentKeyframeIndexes.isEmpty()) {
            // No keyframes found, abort
            return keyframeIndexes;
        }
        m_selectedIndexes << startIndex;
        keyframeIndexes.insert(QString::number(startIndex.internalId()), currentKeyframeIndexes);
        for (int j = 0; j < tItem->childCount(); ++j) {
            auto current = tItem->child(j);
            auto ix2 = getIndexFromItem(current);
            processed << ix2;
            currentKeyframeIndexes = processIndex(ix2, startFrame, endFrame);
            if (!currentKeyframeIndexes.isEmpty()) {
                m_selectedIndexes << ix2;
                keyframeIndexes.insert(QString::number(ix2.internalId()), currentKeyframeIndexes);
            }
        }
    } else {
        processed << startIndex;
        QVariantList currentKeyframeIndexes = processIndex(startIndex, startFrame, endFrame);
        if (!currentKeyframeIndexes.isEmpty()) {
            m_selectedIndexes << startIndex;
            keyframeIndexes.insert(QString::number(startIndex.internalId()), currentKeyframeIndexes);
        }
    }
    return keyframeIndexes;
}

QVariantList DopeSheetModel::selectedIndexes() const
{
    qDebug() << "::::: REQUESTING INDEXES: " << m_selectedIndexes << "\n**************************";
    return m_selectedIndexes;
}

QVariantList DopeSheetModel::processIndex(const QModelIndex ix, int startFrame, int endFrame)
{
    QVariantList currentKeyframeIndexes;
    KeyframeModel *km = data(ix, ModelRole).value<KeyframeModel *>();
    if (!km) {
        return currentKeyframeIndexes;
    }
    if (endFrame == -1) {
        if (km->hasKeyframe(startFrame)) {
            return {km->getIndexForPos(GenTime(startFrame, pCore->getCurrentFps()))};
        }
        return {};
    }
    for (int k = 0; k < km->keyframesCount(); k++) {
        int pos = km->getPosAtIndex(k).frames(pCore->getCurrentFps());
        if (pos >= startFrame && pos <= endFrame) {
            currentKeyframeIndexes << k;
        }
    }
    return currentKeyframeIndexes;
}

const QMap<QModelIndex, QVariant> DopeSheetModel::sanitizeKeyframesIndexes(const QVariantMap kfData)
{
    QMap<QModelIndex, QVariant> results;
    // First list all existing indexes
    QList<QModelIndex> includedIndexes;
    QList<QModelIndex> childrenIndexes;
    for (auto i = kfData.cbegin(), end = kfData.cend(); i != end; ++i) {
        auto index = i.value().toMap();
        const QModelIndex ix = index.value(QStringLiteral("index")).toModelIndex();
        includedIndexes << ix;
        results.insert(ix, index.value(QStringLiteral("kfrs")));
    }
    // Next, check if we have summary items
    for (auto i = kfData.cbegin(), end = kfData.cend(); i != end; ++i) {
        auto index = i.value().toMap();
        const QModelIndex ix = index.value(QStringLiteral("index")).toModelIndex();
        // Check if we have top level items
        int itemId = int(ix.internalId());
        auto tItem = getItemById(itemId);
        if (tItem && tItem->depth() == 1) {
            // match, list children
            for (int j = 0; j < tItem->childCount(); j++) {
                auto current = tItem->child(j);
                auto ix2 = getIndexFromItem(current);
                if (!includedIndexes.contains(ix2)) {
                    results.insert(ix2, index.value(QStringLiteral("kfrs")));
                }
            }
        }
    }
    return results;
}

void DopeSheetModel::changeKeyframeType(const QVariantMap kfData, int type)
{
    bool success = true;
    Fun undo = []() { return true; };
    Fun redo = []() { return true; };
    const QMap<QModelIndex, QVariant> selection = sanitizeKeyframesIndexes(kfData);
    for (auto i = selection.cbegin(), end = selection.cend(); i != end; ++i) {
        KeyframeModel *km = data(i.key(), ModelRole).value<KeyframeModel *>();
        if (km) {
            for (auto &k : i.value().toList()) {
                GenTime pos = km->getPosAtIndex(k.toInt());
                success = success && km->updateKeyframeType(pos, type, undo, redo);
            }
            if (!success) {
                pCore->displayMessage(i18n("Failed to change keyframe type"), InformationMessage);
                break;
            }
        }
    }
    if (success) {
        pCore->pushUndo(undo, redo, i18n("Change keyframe type"));
    }
}

void DopeSheetModel::copyKeyframes(QVariantMap kfData)
{
    qDebug() << ":::: " << kfData;
    QJsonArray jsonArray;
    for (auto i = kfData.cbegin(), end = kfData.cend(); i != end; ++i) {
        auto index = i.value().toMap();
        const QModelIndex ix = index.value(QStringLiteral("index")).toModelIndex();
        int itemId = int(ix.internalId());
        auto tItem = getItemById(itemId);
        if (tItem) {
            qDebug() << "::: CHECKING ITEM FOR ID: " << itemId << ", " << tItem->dataColumn(0);
            if (tItem->depth() == 1) {
                continue;
            }
            auto parentItem = tItem->parentItem();
            if (auto ptr = parentItem.lock()) {
                qDebug() << "::: FETCHIBNG ITEM PARENT: " << ptr->dataColumn(0) << " = " << ptr->dataColumn(1);
                std::shared_ptr<AbstractEffectItem> item = m_model->getEffectStackRow(ptr->dataColumn(1).toInt());
                std::shared_ptr<EffectItemModel> effectModel = std::static_pointer_cast<EffectItemModel>(item);
                if (!effectModel) {
                    continue;
                }
                const QVariantList keys = index.value(QStringLiteral("kfrs")).toList();
                QVector<int> selection;
                for (auto &k : keys) {
                    selection << k.toInt();
                }
                int paramRow = m_paramsList.at(tItem->getId()).first.row;
                QJsonDocument doc1 = effectModel->toJson(selection, false, false, {paramRow});
                jsonArray.append(doc1.array());
            }
        }
    }
    QJsonDocument effectDoc(jsonArray);
    if (effectDoc.isEmpty()) {
        pCore->displayMessage(i18n("Cannot copy current parameter values"), InformationMessage);
        return;
    }
    QClipboard *clipboard = QApplication::clipboard();
    clipboard->setText(QString(effectDoc.toJson()));
    pCore->displayMessage(i18n("Current values copied"), InformationMessage);
}

int DopeSheetModel::dopeDuration() const
{
    if (!m_model) {
        return 0;
    }
    return pCore->getItemDuration(m_model->getOwnerId());
}

int DopeSheetModel::dopePosition() const
{
    if (!m_model) {
        return 0;
    }
    return pCore->getItemPosition(m_model->getOwnerId());
}

void DopeSheetModel::updateItemPosition(ObjectId itemId)
{
    if (m_model) {
        if (m_model->getOwnerId() == itemId) {
            Q_EMIT dopePositionChanged();
        }
    }
}
