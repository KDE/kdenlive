/*
    SPDX-FileCopyrightText: 2026 Jean-Baptiste Mardelle
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include "abstractmodel/abstracttreemodel.hpp"
#include "assets/keyframes/model/keyframemodel.hpp"
#include "assets/model/assetparametermodel.hpp"
#include "definitions.h"
#include "undohelper.hpp"

#include <QAbstractListModel>
#include <QReadWriteLock>
#include <QtGlobal>

#include <framework/mlt_version.h>
#include <memory>

class AssetParameterModel;
class DocUndoStack;
class EffectItemModel;

/** @class KeyframeModel
    @brief This class is the model for a list of keyframes.
   A keyframe is defined by a time, a type and a value
   We store them in a sorted fashion using a std::map
 */
class DopeSheetModel : public AbstractTreeModel
{
    Q_OBJECT
protected:
    explicit DopeSheetModel(QObject *parent = nullptr);

public:
    friend class KdenliveTests;
    static std::shared_ptr<DopeSheetModel> construct(QObject *parent = nullptr);
    enum { NameRole = Qt::UserRole + 1, AssetTypeRole, ModelRole, IndexRole, SelectedRole };
    friend class KeyframeModel;
    friend class KeyframeContainer;
    friend class KeyframeImport;

public:
    // Mandatory overloads
    Q_INVOKABLE QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;
    void registerItem(QPersistentModelIndex ix, const QString &name, ParamType type, std::shared_ptr<KeyframeModel> model);
    void deregisterItem(QPersistentModelIndex ix);
    // void clear();
    /** @brief Register all keyframable params for an effect */
    void registerAsset(std::shared_ptr<EffectItemModel> effectModel);

protected:
    std::map<int, std::pair<std::pair<QString, ParamType>, std::shared_ptr<KeyframeModel>>> m_paramsList;
    // int getRowfromId(QModelIndex mid) const;
    /** @brief Register the existence of a new element
     */
    void registerItem(const std::shared_ptr<TreeItem> &item) override;
    /** @brief Deregister the existence of a new element*/
    void deregisterItem(int id, TreeItem *item) override;

private:
    /** @brief This is a lock that ensures safety in case of concurrent access */
    mutable QReadWriteLock m_lock;
    QList<QModelIndex> m_selectedIndexes;

Q_SIGNALS:
    void modelChanged();
    void requestModelUpdate(const QModelIndex &, const QModelIndex &, const QVector<int> &);
};
