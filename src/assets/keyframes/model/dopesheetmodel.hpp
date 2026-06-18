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
#include <QtQmlIntegration/qqmlintegration.h>

#include <framework/mlt_version.h>
#include <memory>

class AssetParameterModel;
class DocUndoStack;
class EffectItemModel;
class EffectStackModel;

/** @class KeyframeModel
    @brief This class is the model for a list of keyframes.
   A keyframe is defined by a time, a type and a value
   We store them in a sorted fashion using a std::map
 */
class DopeSheetModel : public AbstractTreeModel
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("DopeSheetModel is owned by Core; obtained via setInitialProperties()")

protected:
    explicit DopeSheetModel(QObject *parent = nullptr);
    struct EffectParamInfo
    {
        QString id;     // Identifier of the parameter
        ParamType type; // The parameter type (double, animatedrect,...)
        int row{-1};    // The index row in the assetmodel
        QPersistentModelIndex index;
    };

public:
    friend class KdenliveTests;
    ~DopeSheetModel() override;

    Q_PROPERTY(int dopeInPoint READ dopeInPoint NOTIFY dopeInPointChanged)
    Q_PROPERTY(int dopeDuration READ dopeDuration NOTIFY dopeDurationChanged)
    Q_PROPERTY(int dopePosition READ dopePosition NOTIFY dopePositionChanged)
    static std::shared_ptr<DopeSheetModel> construct(QObject *parent = nullptr);
    enum { NameRole = Qt::UserRole + 1, AssetTypeRole, ModelRole, IndexRole, SelectedRole, RecapRole };
    friend class KeyframeModel;
    friend class KeyframeContainer;
    friend class KeyframeImport;

public:
    // Mandatory overloads
    Q_INVOKABLE QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;
    void registerItem(QPersistentModelIndex ix, const QString &name, ParamType type, std::shared_ptr<KeyframeModel> model);
    void deregisterItem(QPersistentModelIndex ix);
    void clearModel();
    /** @brief Register all keyframable params for an effect */
    bool registerAsset(int row, std::shared_ptr<EffectItemModel> effectModel);
    bool registerStack(std::shared_ptr<EffectStackModel> model);
    /** @brief Remove all keyframes at given indexes (parameter indexes / keyframes indexes) */
    Q_INVOKABLE void removeKeyframes(QVariantList indexes, QVariantList keyframes);
    /** @brief Remove all keyframes at given position */
    Q_INVOKABLE void removeKeyframe(const QModelIndex &ix, int framePos);
    /** @brief Add a keyframe to all parameters */
    Q_INVOKABLE void addKeyframe(const QModelIndex &ix, int framePosition);
    /** @brief Returns true if a keyframe exists in this parameter at that position */
    Q_INVOKABLE bool isOnKeyframe(int framePosition, bool force, QPersistentModelIndex activeIndex);
    /** @brief Move keyframes in all parameters at current pos */
    Q_INVOKABLE void moveKeyframe(QVariantMap kfData, int sourcePos, int updatedPos, bool logUndo);
    Q_INVOKABLE void moveScaledKeyframe(int updatedPos, bool logUndo, bool updateView);
    /** @brief Align keyframes to the right or left of the selection */
    Q_INVOKABLE void alignKeyframe(QVariantMap kfData, bool right);
    /** @brief Register all keyframes that will need to move */
    Q_INVOKABLE void buildMasterSelection(const QModelIndex &ix, int index);
    Q_INVOKABLE QVariantMap selectKeyframeRange(const QModelIndex &startIndex, const QModelIndex &endIndex, int startFrame, int endFrame);
    Q_INVOKABLE QVariantMap selectKeyframeAtPos(const QModelIndex &masterIndex, int frame);
    Q_INVOKABLE QVariantList selectedIndexes() const;
    Q_INVOKABLE QVariantList grabbedIndexes() const;
    Q_INVOKABLE void copyKeyframes(QVariantMap kfData);
    Q_INVOKABLE void changeKeyframeType(const QVariantMap kfData, int type);
    Q_INVOKABLE void resetScaledInfo();
    Q_INVOKABLE void setScaledInfo(const QVariantMap kfData, int sourcePos);
    Q_INVOKABLE void setActiveIndex(const QPersistentModelIndex ix);
    Q_INVOKABLE KeyframeModel *getKeyframeModel(QPersistentModelIndex activeIndex);
    int dopeDuration() const;
    int dopeInPoint() const;
    int dopePosition() const;
    void updateItemPosition(ObjectId itemId);
    /** @brief True if we have grabbed keyframes for a move */
    bool stateGrabbed() const;
    /** @brief True if we have grabbed keyframes for a move */
    void setGrabbed(bool grabbed);
    int getPreviousSnap(const QModelIndex ix, int pos);
    int getNextSnap(const QModelIndex ix, int pos);
    void addRemoveKeyframe(const QModelIndex ix, int pos);

protected:
    std::map<int, std::pair<EffectParamInfo, std::shared_ptr<KeyframeModel>>> m_paramsList;
    // int getRowfromId(QModelIndex mid) const;
    /** @brief Register the existence of a new element
     */
    void registerItem(const std::shared_ptr<TreeItem> &item) override;
    /** @brief Deregister the existence of a new element*/
    void deregisterItem(int id, TreeItem *item) override;

private:
    /** @brief This is a lock that ensures safety in case of concurrent access */
    mutable QReadWriteLock m_lock;
    QVariantList m_selectedIndexes;
    QVariantList m_grabbedIndexes;
    QMap<QModelIndex, int> m_relatedMove;
    QList<QMetaObject::Connection> m_connectionList;
    std::shared_ptr<EffectStackModel> m_model;
    QMap<QModelIndex, QList<std::pair<int, int>>> m_scaledKFInfo;
    std::pair<int, int> m_scaledRange;
    /** @brief Remember if the playhead is on a keyframe */
    QList<QPersistentModelIndex> m_indexesOnKeyframe;
    bool m_resizeFromStart{false};
    /** @brief Returns a list on int indexes of keyframes in a
     *  parameter ix that are placed between startFrame and endFrame */
    QVariantList processIndex(const QModelIndex ix, int startFrame, int endFrame);
    QVariantMap selectKeyframeByRange(const QModelIndex &startIndex, int startFrame, int endFrame);
    /** @brief Ensure selected keyframes contain all child parameters */
    const QMap<QModelIndex, QVariant> sanitizeKeyframesIndexes(const QVariantMap kfData);
    bool m_hasGrabbedKeyframes{false};

private Q_SLOTS:
    void updateKeyframeRole(const QModelIndex &ix1, const QModelIndex &ix2, const QList<int> &roles);
    void loadEffects();

Q_SIGNALS:
    void modelChanged();
    void dopeDurationChanged();
    void dopeInPointChanged();
    void dopePositionChanged();
    void requestModelUpdate(const QModelIndex &, const QModelIndex &, const QVector<int> &);
    void activateEffect(QPersistentModelIndex ix);
    void matchingKeyframes(QList<QPersistentModelIndex>);
};
