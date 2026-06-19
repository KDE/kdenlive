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
#include <QJsonObject>
#include <QLineF>
#include <QSize>
#include <mlt++/Mlt.h>
#include <qapplication.h>
#include <qclipboard.h>
#include <utility>

struct
{
    mlt_keyframe_type t;
    const QChar s;
} keyframe_type_map[] = {
    // Map keyframe type to any single character except numeric values.
    {mlt_keyframe_discrete, QChar('|')},
    {mlt_keyframe_discrete, QChar('!')},
    {mlt_keyframe_linear, QChar()},
    {mlt_keyframe_smooth, QChar('~')},
    {mlt_keyframe_smooth_loose, QChar('~')},
    {mlt_keyframe_smooth_natural, QChar('$')},
    {mlt_keyframe_smooth_tight, QChar('-')},
    {mlt_keyframe_sinusoidal_in, QChar('a')},
    {mlt_keyframe_sinusoidal_out, QChar('b')},
    {mlt_keyframe_sinusoidal_in_out, QChar('c')},
    {mlt_keyframe_quadratic_in, QChar('d')},
    {mlt_keyframe_quadratic_out, QChar('e')},
    {mlt_keyframe_quadratic_in_out, QChar('f')},
    {mlt_keyframe_cubic_in, QChar('g')},
    {mlt_keyframe_cubic_out, QChar('h')},
    {mlt_keyframe_cubic_in_out, QChar('i')},
    {mlt_keyframe_quartic_in, QChar('j')},
    {mlt_keyframe_quartic_out, QChar('k')},
    {mlt_keyframe_quartic_in_out, QChar('l')},
    {mlt_keyframe_quintic_in, QChar('m')},
    {mlt_keyframe_quintic_out, QChar('n')},
    {mlt_keyframe_quintic_in_out, QChar('o')},
    {mlt_keyframe_exponential_in, QChar('p')},
    {mlt_keyframe_exponential_out, QChar('q')},
    {mlt_keyframe_exponential_in_out, QChar('r')},
    {mlt_keyframe_circular_in, QChar('s')},
    {mlt_keyframe_circular_out, QChar('t')},
    {mlt_keyframe_circular_in_out, QChar('u')},
    {mlt_keyframe_back_in, QChar('v')},
    {mlt_keyframe_back_out, QChar('w')},
    {mlt_keyframe_back_in_out, QChar('x')},
    {mlt_keyframe_elastic_in, QChar('y')},
    {mlt_keyframe_elastic_out, QChar('z')},
    {mlt_keyframe_elastic_in_out, QChar('A')},
    {mlt_keyframe_bounce_in, QChar('B')},
    {mlt_keyframe_bounce_out, QChar('C')},
    {mlt_keyframe_bounce_in_out, QChar('D')},
};

static mlt_keyframe_type str_to_keyframe_type(const QChar s)
{
    int map_count = sizeof(keyframe_type_map) / sizeof(*keyframe_type_map);
    for (int i = 0; i < map_count; i++) {
        if (s == keyframe_type_map[i].s) {
            return keyframe_type_map[i].t;
        }
    }
    return mlt_keyframe_linear;
}

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

DopeSheetModel::~DopeSheetModel()
{
    clearModel();
}

void DopeSheetModel::clearModel()
{
    m_paramsList.clear();
    m_hasGrabbedKeyframes = false;
    m_indexesOnKeyframe.clear();
    for (auto &c : m_connectionList) {
        QObject::disconnect(c);
    }
    m_connectionList.clear();
    m_model.reset();
    clear();
}

QHash<int, QByteArray> DopeSheetModel::roleNames() const
{
    QHash<int, QByteArray> roles;
    roles[NameRole] = "dopeName";
    roles[AssetTypeRole] = "dopeType";
    roles[ModelRole] = "dopeModel";
    roles[IndexRole] = "dopeIndex";
    roles[SelectedRole] = "dopeSelected";
    roles[RecapRole] = "dopeRecap";
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
    case RecapRole:
        return m_paramsList.at(itemId).first.row == -1;
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

bool DopeSheetModel::registerStack(std::shared_ptr<EffectStackModel> model)
{
    if (model == m_model) {
        return false;
    }
    m_model.reset();
    m_hasGrabbedKeyframes = false;
    m_indexesOnKeyframe.clear();
    m_model = std::move(model);
    Q_EMIT dopeDurationChanged();
    Q_EMIT dopePositionChanged();
    Q_EMIT dopeInPointChanged();
    if (m_model) {
        connect(m_model.get(), &QAbstractItemModel::rowsInserted, this, &DopeSheetModel::loadEffects, Qt::QueuedConnection);
        connect(m_model.get(), &QAbstractItemModel::rowsRemoved, this, &DopeSheetModel::loadEffects, Qt::QueuedConnection);
        connect(m_model.get(), &QAbstractItemModel::rowsMoved, this, &DopeSheetModel::loadEffects, Qt::QueuedConnection);
    }
    loadEffects();
    return true;
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
    int activeEffect = m_model->getActiveEffect();
    for (int i = 0; i < max; i++) {
        std::shared_ptr<AbstractEffectItem> item = m_model->getEffectStackRow(i);
        if (item->childCount() > 0) {
            // group, create sub stack
            continue;
        }
        std::shared_ptr<EffectItemModel> effectModel = std::static_pointer_cast<EffectItemModel>(item);
        if (registerAsset(i, effectModel) && i == activeEffect) {
            Q_EMIT activateEffect(m_model->index(i, 0));
        }
    }
}

bool DopeSheetModel::registerAsset(int row, std::shared_ptr<EffectItemModel> effectModel)
{
    qDebug() << ":::: REGISTERING DOPE EFFECT: " << effectModel->dataColumn(0);
    auto effectItem = TreeItem::construct({effectModel->dataColumn(0).toString(), row}, shared_from_this(), false);
    std::shared_ptr<KeyframeModelList> keyframes = effectModel->getKeyframeModel();
    if (!keyframes) {
        // EFfect has no keyframes, abort
        return false;
    }
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
        if (blockedParams.contains(effectModel->data(ix, AssetParameterModel::NameRole).toString())) {
            continue;
        }
        auto km = keyframes->getKeyModel(ix);
        auto conn = connect(km.get(), &KeyframeModel::dataChanged, this, &DopeSheetModel::modelChanged, Qt::QueuedConnection);
        m_connectionList << conn;
        auto paramItem = TreeItem::construct({effectModel->data(ix, Qt::DisplayRole).toString()}, shared_from_this(), false);
        effectItem->appendChild(paramItem);
        qDebug() << "::: REGISTERING PARAMETER: " << effectModel->data(ix, Qt::DisplayRole).toString();
        pInfo.id = effectModel->data(ix, Qt::DisplayRole).toString();
        pInfo.mltId = effectModel->data(ix, AssetParameterModel::NameRole).toString();
        pInfo.type = effectModel->data(ix, AssetParameterModel::TypeRole).value<ParamType>();
        pInfo.row = ix.row();
        pInfo.index = ix;
        m_paramsList.insert({paramItem->getId(), {pInfo, km}});
    }
    auto connection = connect(effectModel.get(), &AssetParameterModel::dataChanged, this,
                              [this, effectItem, effectModel](const QModelIndex &ix1, const QModelIndex & /*ix2*/, const QList<int> &roles) {
                                  if (roles.contains(AssetParameterModel::ParentDurationRole)) {
                                      Q_EMIT dopeDurationChanged();
                                  }
                                  if (roles.contains(AssetParameterModel::InRole)) {
                                      Q_EMIT dopeInPointChanged();
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
                                          qDebug() << "::: REGISTERING PARAMETER: " << effectModel->data(ix, Qt::DisplayRole).toString() << " / "
                                                   << effectModel->data(ix, AssetParameterModel::NameRole).toString();
                                          EffectParamInfo pInfo;
                                          pInfo.id = effectModel->data(ix, Qt::DisplayRole).toString();
                                          pInfo.mltId = effectModel->data(ix, AssetParameterModel::NameRole).toString();
                                          pInfo.type = effectModel->data(ix, AssetParameterModel::TypeRole).value<ParamType>();
                                          pInfo.row = ix.row();
                                          m_paramsList.insert({paramItem->getId(), {pInfo, km}});
                                      }
                                  }
                              });
    m_connectionList << connection;
    return true;
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

void DopeSheetModel::slotPasteKeyframeFromClipBoard(int position)
{
    QClipboard *clipboard = QApplication::clipboard();
    QString values = clipboard->text();
    auto json = QJsonDocument::fromJson(values.toUtf8());
    Fun undo = []() { return true; };
    Fun redo = []() { return true; };
    if (!json.isArray()) {
        pCore->displayMessage(i18n("No valid keyframe data in clipboard"), InformationMessage);
        return;
    }
    auto list = json.array();
    QMap<QString, QMap<std::pair<int, QChar>, QVariant>> storedValues;
    for (const auto &entry : std::as_const(list)) {
        if (!entry.isObject()) {
            qDebug() << "Warning : Skipping invalid marker data";
            continue;
        }
        auto entryObj = entry.toObject();
        if (!entryObj.contains(QLatin1String("name"))) {
            qDebug() << "Warning : Skipping invalid marker data (does not contain name)";
            continue;
        }

        ParamType kfrType = entryObj[QLatin1String("type")].toVariant().value<ParamType>();
        auto assetModel = m_model->getActiveAsset();
        if (!assetModel) {
            qWarning() << ":::: COULD NOT FIND AN ASSET MODEL...";
            return;
        }
        if (assetModel->isAnimated(kfrType)) {
            QMap<std::pair<int, QChar>, QVariant> values;
            if (kfrType == ParamType::Roto_spline) {
                auto value = entryObj.value(QLatin1String("value"));
                if (value.isObject()) {
                    QJsonObject obj = value.toObject();
                    QStringList keys = obj.keys();
                    for (auto &k : keys) {
                        values.insert({k.toInt(), QChar()}, obj.value(k));
                    }
                } else if (value.isArray()) {
                    auto list = value.toArray();
                    for (const auto &entry : std::as_const(list)) {
                        if (!entry.isObject()) {
                            qDebug() << "Warning : Skipping invalid category data";
                            continue;
                        }
                        QJsonObject obj = entry.toObject();
                        QStringList keys = obj.keys();
                        for (auto &k : keys) {
                            values.insert({k.toInt(), QChar()}, obj.value(k));
                        }
                    }
                } else {
                    pCore->displayMessage(i18n("No valid keyframe data in clipboard"), InformationMessage);
                    qDebug() << "::: Invalid ROTO VALUE, ABORTING PASTE\n" << value;
                    return;
                }
            } else {
                const QString value = entryObj.value(QLatin1String("value")).toString();
                if (value.isEmpty()) {
                    pCore->displayMessage(i18n("No valid keyframe data in clipboard"), InformationMessage);
                    qDebug() << "::: Invalid KFR VALUE, ABORTING PASTE\n" << value;
                    return;
                }
                const QStringList stringVals = value.split(QLatin1Char(';'), Qt::SkipEmptyParts);
                for (auto &val : stringVals) {
                    QChar separator;
                    QString timeVal = val.section(QLatin1Char('='), 0, 0);
                    if (!timeVal.isEmpty() && !timeVal.back().isDigit()) {
                        separator = timeVal.back();
                        timeVal.chop(1);
                    }
                    int position = assetModel->time_to_frames(timeVal);
                    values.insert({position, separator}, val.section(QLatin1Char('='), 1));
                }
            }
            storedValues.insert(entryObj[QLatin1String("name")].toString(), values);
        } else {
            const QString value = entryObj.value(QLatin1String("value")).toString();
            QMap<std::pair<int, QChar>, QVariant> values;
            values.insert({0, QChar()}, value);
            storedValues.insert(entryObj[QLatin1String("name")].toString(), values);
        }
    }
    for (auto &p : m_paramsList) {
        qDebug() << "::: CHECKING PARAM: " << p.second.first.id << " / " << p.second.first.mltId;
        QPersistentModelIndex ix = p.second.first.index;
        auto paramName = p.second.first.mltId; // m_model->data(ix, AssetParameterModel::NameRole).toString();
        if (paramName.isEmpty()) {
            // Recap, ignore
            continue;
        }
        if (storedValues.contains(paramName)) {
            auto km = p.second.second;
            const QMap<std::pair<int, QChar>, QVariant> values = storedValues.value(paramName);
            int offset = values.firstKey().first;
            QMapIterator<std::pair<int, QChar>, QVariant> i(values);
            while (i.hasNext()) {
                i.next();
                mlt_keyframe_type type = str_to_keyframe_type(i.key().second);
                qDebug() << "::: ADDING KF AT: " << position + i.key().first << "\nXXXXXXXXXXXXXxx";
                km->addKeyframe(GenTime(position + i.key().first - offset, pCore->getCurrentFps()), KeyframeModel::convertFromMltType(type), i.value(), true,
                                undo, redo);
            }
        } else {
            qDebug() << "::: NOT FOUND PARAM: " << paramName << " in list: " << storedValues.keys();
        }
    }
    pCore->pushUndo(undo, redo, i18n("Paste keyframe"));
}

void DopeSheetModel::alignKeyframe(QVariantMap kfData, bool right)
{
    Fun undo = []() { return true; };
    Fun redo = []() { return true; };
    const QMap<QModelIndex, QVariant> selection = sanitizeKeyframesIndexes(kfData);
    GenTime alignPos;
    for (auto i = selection.cbegin(), end = selection.cend(); i != end; ++i) {
        int itemId = int(i.key().internalId());
        auto tItem = getItemById(itemId);
        KeyframeModel *km = data(i.key(), ModelRole).value<KeyframeModel *>();
        if (km) {
            QVariantList ixes = i.value().toList();
            int index = right ? ixes.last().toInt() : ixes.first().toInt();
            if (right) {
                alignPos = qMax(alignPos, km->getPosAtIndex(index));
            } else {
                if (alignPos == GenTime()) {
                    alignPos = km->getPosAtIndex(index);
                } else {
                    alignPos = qMin(alignPos, km->getPosAtIndex(index));
                }
            }
        }
    }
    // Now, align all keyframes
    qDebug() << "///// READY TO ALIGN KEYFRAMES TO: " << alignPos.frames(25);
    bool success = true;
    for (auto i = selection.cbegin(), end = selection.cend(); i != end; ++i) {
        int itemId = int(i.key().internalId());
        auto tItem = getItemById(itemId);
        if (tItem->depth() == 1) {
            // Summary item, ignore
            continue;
        }
        KeyframeModel *km = data(i.key(), ModelRole).value<KeyframeModel *>();
        if (km && success) {
            km->setSelectedKeyframes({});
            const QVariantList indexes = i.value().toList();
            if (!indexes.isEmpty()) {
                for (auto &k : indexes) {
                    GenTime pos = km->getPosAtIndex(k.toInt());
                    if (pos != alignPos) {
                        qDebug() << "READY TO MOVE KEYFRAME: " << pos.frames(25) << " / " << alignPos.frames(25);
                        success = success && km->moveKeyframe(pos, alignPos, QVariant(), undo, redo);
                    }
                }
            }
        }
    }
    if (success) {
        pCore->pushUndo(undo, redo, i18n("Align keyframes"));
    } else {
        undo();
        pCore->displayMessage(i18n("Failed to align keyframe"), InformationMessage);
    }
}

void DopeSheetModel::resetScaledInfo()
{
    m_scaledKFInfo.clear();
}

void DopeSheetModel::setScaledInfo(const QVariantMap kfData, int sourcePos)
{
    QMap<QModelIndex, QVariant> selection = sanitizeKeyframesIndexes(kfData);
    m_scaledKFInfo.clear();
    // m_scaledRange.second = sourcePos;
    m_scaledRange = {-1, -1};
    for (auto i = selection.cbegin(), end = selection.cend(); i != end; ++i) {
        int itemId = int(i.key().internalId());
        auto tItem = getItemById(itemId);
        if (tItem->depth() == 1) {
            // Summary item, ignore
            continue;
        }
        KeyframeModel *km = data(i.key(), ModelRole).value<KeyframeModel *>();
        if (km) {
            QVariantList indexes = i.value().toList();
            std::sort(indexes.begin(), indexes.end(), [](const QVariant &a, const QVariant &b) { return a.toInt() < b.toInt(); });
            QList<std::pair<int, int>> kfMap;
            for (auto &k : indexes) {
                int frame = km->getPosAtIndex(k.toInt()).frames(pCore->getCurrentFps());
                if (frame < m_scaledRange.first || m_scaledRange.first < 0) {
                    m_scaledRange.first = frame;
                }
                m_scaledRange.second = qMax(frame, m_scaledRange.second);
                std::pair<int, int> kfValue(k.toInt(), frame);
                kfMap << kfValue;
            }
            m_scaledKFInfo.insert(i.key(), kfMap);
        }
    }
    m_resizeFromStart = m_scaledRange.first == sourcePos;
}

void DopeSheetModel::moveScaledKeyframe(int updatedPos, bool logUndo, bool updateView)
{
    Fun undo = []() { return true; };
    Fun redo = []() { return true; };
    bool success = true;
    double percentage;
    if (m_resizeFromStart) {
        percentage = double(m_scaledRange.second - updatedPos) / (m_scaledRange.second - m_scaledRange.first);
    } else {
        percentage = double(updatedPos - m_scaledRange.first) / (m_scaledRange.second - m_scaledRange.first);
    }
    GenTime firstKeyframe;
    // Check first / last keyframes for scaling
    for (auto i = m_scaledKFInfo.cbegin(), end = m_scaledKFInfo.cend(); i != end; ++i) {
        int itemId = int(i.key().internalId());
        auto tItem = getItemById(itemId);
        if (tItem->depth() == 1) {
            // Summary item, ignore
            continue;
        }
        KeyframeModel *km = data(i.key(), ModelRole).value<KeyframeModel *>();
        km->setSelectedKeyframes({});
        if (km && success) {
            const QList<std::pair<int, int>> indexes = i.value();
            for (auto &j : indexes) {
                int updatedFrame = m_resizeFromStart ? m_scaledRange.second - (m_scaledRange.second - j.second) * percentage
                                                     : m_scaledRange.first + (j.second - m_scaledRange.first) * percentage;
                qDebug() << "::: MOVING KEYFRAME AT: " << j.first << " = " << j.second << " TO " << updatedFrame;
                success = success && km->moveKeyframeByIndex(j.first, updatedFrame, undo, redo, updateView);
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

void DopeSheetModel::moveKeyframe(const QVariantMap kfData, int sourcePos, int updatedPos, bool logUndo)
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

KeyframeModel *DopeSheetModel::getKeyframeModel(QPersistentModelIndex activeIndex)
{
    if (data(activeIndex, RecapRole).toBool() == true) {
        // Recap, return null
        return nullptr;
    }
    return data(activeIndex, ModelRole).value<KeyframeModel *>();
}

bool DopeSheetModel::isOnKeyframe(int framePosition, bool force, QPersistentModelIndex activeIndex)
{
    if (!m_model) {
        return false;
    }
    int max = m_model->rowCount();
    QList<QPersistentModelIndex> matchingIndexes;
    bool matching = false;
    bool foundActive = false;
    for (int i = 0; i < max; i++) {
        QModelIndex ix = index(i, 0);
        KeyframeModel *master = data(ix, ModelRole).value<KeyframeModel *>();
        if (ix == activeIndex) {
            KeyframeModel *km = data(ix, ModelRole).value<KeyframeModel *>();
            if (km && km->hasKeyframe(framePosition)) {
                qDebug() << ":::: FOUND MASTER ITEM AT IX: " << ix;
                foundActive = true;
            }
        }
        if (master && master->hasKeyframe(framePosition)) {
            int itemId = int(ix.internalId());
            auto tItem = getItemById(itemId);
            for (int j = 0; j < tItem->childCount(); ++j) {
                auto current = tItem->child(j);
                auto ix2 = getIndexFromItem(current);
                KeyframeModel *km = data(ix2, ModelRole).value<KeyframeModel *>();
                if (km && km->hasKeyframe(framePosition)) {
                    if (ix2 == activeIndex) {
                        foundActive = true;
                    }
                    matchingIndexes << m_paramsList.at(current->getId()).first.index;
                }
            }
            matching = true;
        }
    }
    if (force || m_indexesOnKeyframe != matchingIndexes) {
        Q_EMIT matchingKeyframes(matchingIndexes);
    }
    m_indexesOnKeyframe = matchingIndexes;
    if (matching) {
        if (activeIndex.isValid()) {
            return foundActive;
        }
    }
    return matching;
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

QVariantList DopeSheetModel::grabbedIndexes() const
{
    return m_grabbedIndexes;
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
            KeyframeModel *masterKm = data(ix, ModelRole).value<KeyframeModel *>();
            if (!masterKm) {
                continue;
            }
            const QVariantList keyframeIndexes = index.value(QStringLiteral("kfrs")).toList();
            QList<GenTime> masterPositions;
            for (auto &k : keyframeIndexes) {
                masterPositions << masterKm->getPosAtIndex(k.toInt());
            }
            for (int j = 0; j < tItem->childCount(); j++) {
                auto current = tItem->child(j);
                auto ix2 = getIndexFromItem(current);
                if (!includedIndexes.contains(ix2)) {
                    KeyframeModel *km = data(ix2, ModelRole).value<KeyframeModel *>();
                    QVariantList matches;
                    if (km) {
                        for (auto &g : masterPositions) {
                            int foundIndex = km->getIndexForPos(g);
                            if (foundIndex > -1) {
                                matches << foundIndex;
                            }
                        }
                        if (!matches.isEmpty()) {
                            results.insert(ix2, matches);
                        }
                    }
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

int DopeSheetModel::dopeDuration() const
{
    if (!m_model) {
        return 0;
    }
    return pCore->getItemDuration(m_model->getOwnerId());
}

int DopeSheetModel::dopeInPoint() const
{
    if (!m_model) {
        return 0;
    }
    return pCore->getItemIn(m_model->getOwnerId());
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

void DopeSheetModel::setGrabbed(bool grabbed)
{
    m_hasGrabbedKeyframes = grabbed;
    if (grabbed) {
        m_grabbedIndexes = m_selectedIndexes;
    } else {
        m_grabbedIndexes.clear();
    }
}

bool DopeSheetModel::stateGrabbed() const
{
    return m_hasGrabbedKeyframes;
}

int DopeSheetModel::getPreviousSnap(const QModelIndex ix, int pos)
{
    // Find active model
    KeyframeModel *km = data(ix, ModelRole).value<KeyframeModel *>();
    pos -= dopePosition();
    if (!km) {
        qDebug() << "// INVALID INDEX PASSED FOR SEEK: " << ix;
        return pos;
    }
    bool ok;
    GenTime previousTime = km->getPrevKeyframe(GenTime(pos, pCore->getCurrentFps()), &ok).first;
    if (ok) {
        pos = previousTime.frames(pCore->getCurrentFps());
    } else {
        pos = 0;
    }
    pos += dopePosition();
    return pos;
}

int DopeSheetModel::getNextSnap(const QModelIndex ix, int pos)
{
    // Find active model
    KeyframeModel *km = data(ix, ModelRole).value<KeyframeModel *>();
    pos -= dopePosition();
    if (!km) {
        qDebug() << "// INVALID INDEX PASSED FOR SEEK: " << ix;
        return pos;
    }
    bool ok;
    GenTime previousTime = km->getNextKeyframe(GenTime(pos, pCore->getCurrentFps()), &ok).first;
    if (ok) {
        pos = previousTime.frames(pCore->getCurrentFps());
    } else {
        pos = dopeDuration() - 1;
    }
    pos += dopePosition();
    return pos;
}

void DopeSheetModel::addRemoveKeyframe(const QModelIndex ix, int pos)
{
    // Get model
    KeyframeModel *km = data(ix, ModelRole).value<KeyframeModel *>();
    if (!km) {
        qDebug() << "// INVALID INDEX PASSED FOR ADD/REMOVE KF: " << ix;
        return;
    }
    if (km->hasKeyframe(pos)) {
        removeKeyframe(ix, pos);
    } else {
        addKeyframe(ix, pos);
    }
}

void DopeSheetModel::setActiveIndex(const QPersistentModelIndex ix)
{
    if (m_model) {
        m_model->setActiveEffect(ix.row());
    }
}

void DopeSheetModel::copySelectedKeyframes(const QModelIndex ix, const QVariantMap kfData)
{
    if (!m_model) {
        return;
    }

    const QMap<QModelIndex, QVariant> selection = sanitizeKeyframesIndexes(kfData);
    if (!selection.contains(ix)) {
        pCore->displayMessage(i18n("No keyframe selected in current parameter"), InformationMessage);
        return;
    }
    auto assetModel = m_model->getActiveAsset();
    QMap<int, QList<int>> finalSelection;
    for (auto i = selection.cbegin(), end = selection.cend(); i != end; ++i) {
        if (data(i.key(), RecapRole).toBool()) {
            // Recap, ignore
            qDebug() << "// RECAP: " << i.key() << ", IGNORING";
            continue;
        }
        const QVariantList indexes = i.value().toList();
        QList<int> kfIndexes;
        for (auto &i : indexes) {
            kfIndexes << i.toInt();
        }
        int itemId = int(i.key().internalId());
        auto tItem = getItemById(itemId);
        int paramRow = m_paramsList.at(tItem->getId()).first.row;
        finalSelection.insert(paramRow, kfIndexes);
    }
    QJsonDocument effectDoc = assetModel->toJson(finalSelection, false);
    if (effectDoc.isEmpty()) {
        pCore->displayMessage(i18n("Cannot copy current parameter values"), InformationMessage);
        return;
    }
    QClipboard *clipboard = QApplication::clipboard();
    clipboard->setText(QString(effectDoc.toJson()));
    qDebug() << ":::: COPIED: " << QString(effectDoc.toJson());
    pCore->displayMessage(i18n("Current values copied"), InformationMessage);
}
