/*
    SPDX-FileCopyrightText: 2017 Jean-Baptiste Mardelle
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include "assetparametermodel.hpp"
#include <QPersistentModelIndex>
#include <QTime>
#include <QUndoCommand>

/** @class AssetCommand
    @brief \@todo Describe class AssetCommand
    @todo Describe class AssetCommand
 */
class AssetCommand : public QUndoCommand
{
public:
    AssetCommand(const std::shared_ptr<AssetParameterModel> &model, const QModelIndex &index, QString value, QUndoCommand *parent = nullptr);
    void undo() override;
    void redo() override;
    int id() const override;
    ObjectId owner() const;
    bool mergeWith(const QUndoCommand *other) override;

private:
    std::shared_ptr<AssetParameterModel> m_model;
    QPersistentModelIndex m_index;
    QString m_value;
    QString m_name;
    QString m_oldValue;
    bool m_updateView;
    QTime m_stamp;
};

/** @class AssetMultiCommand
    @brief \@todo Describe class AssetMultiCommand
    @todo Describe class AssetMultiCommand
 */
class AssetMultiCommand : public QUndoCommand
{
public:
    AssetMultiCommand(const std::shared_ptr<AssetParameterModel> &model, const QList<QModelIndex> &indexes, const QStringList &values,
                      QUndoCommand *parent = nullptr);
    void undo() override;
    void redo() override;
    int id() const override;
    bool mergeWith(const QUndoCommand *other) override;

private:
    std::shared_ptr<AssetParameterModel> m_model;
    QList<QModelIndex> m_indexes;
    QStringList m_values;
    QString m_name;
    QStringList m_oldValues;
    bool m_updateView;
    QTime m_stamp;
};

/** @class AssetKeyframeCommand
    @brief \@todo Describe class AssetKeyframeCommand
    @todo Describe class AssetKeyframeCommand
 */
class AssetKeyframeCommand : public QUndoCommand
{
public:
    AssetKeyframeCommand(const std::shared_ptr<AssetParameterModel> &model, const QModelIndex &index, QVariant value, GenTime pos,
                         QUndoCommand *parent = nullptr);
    void undo() override;
    void redo() override;
    int id() const override;
    bool mergeWith(const QUndoCommand *other) override;

private:
    std::shared_ptr<AssetParameterModel> m_model;
    QPersistentModelIndex m_index;
    QVariant m_value;
    QVariant m_oldValue;
    GenTime m_pos;
    bool m_updateView;
    QTime m_stamp;
};

/** @class AssetMultiKeyframeCommand
    @brief \@todo Describe class AssetKeyframeCommand
    @todo Describe class AssetKeyframeCommand
 */
class AssetMultiKeyframeCommand : public QUndoCommand
{
public:
    AssetMultiKeyframeCommand(const std::shared_ptr<AssetParameterModel> &model, const QList<QModelIndex> &indexes, const QStringList &sourceValues,
                              const QStringList &values, GenTime pos, QUndoCommand *parent = nullptr);
    void undo() override;
    void redo() override;
    int id() const override;
    bool mergeWith(const QUndoCommand *other) override;

private:
    std::shared_ptr<AssetParameterModel> m_model;
    QList<QModelIndex> m_indexes;
    QStringList m_values;
    QStringList m_oldValues;
    GenTime m_pos;
    QTime m_stamp;
};

/** @class AssetUpdateCommand
    @brief \@todo Describe class AssetUpdateCommand
    @todo Describe class AssetUpdateCommand
 */
class AssetUpdateCommand : public QUndoCommand
{
public:
    AssetUpdateCommand(const std::shared_ptr<AssetParameterModel> &model, QVector<QPair<QString, QVariant>> parameters, QUndoCommand *parent = nullptr);
    void undo() override;
    void redo() override;
    int id() const override;

private:
    std::shared_ptr<AssetParameterModel> m_model;
    QVector<QPair<QString, QVariant>> m_value;
    QVector<QPair<QString, QVariant>> m_oldValue;
};
