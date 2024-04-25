/*
    SPDX-FileCopyrightText: 2017 Jean-Baptiste Mardelle
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "assetcommand.hpp"
#include "assets/keyframes/model/keyframemodellist.hpp"
#include "effects/effectsrepository.hpp"
#include "transitions/transitionsrepository.hpp"
#include <memory>
#include <utility>
AssetCommand::AssetCommand(const std::shared_ptr<AssetParameterModel> &model, const QModelIndex &index, QString value, QUndoCommand *parent)
    : QUndoCommand(parent)
    , m_model(model)
    , m_index(index)
    , m_value(std::move(value))
    , m_updateView(false)
    , m_stamp(QTime::currentTime())
{
    m_name = m_model->data(index, AssetParameterModel::NameRole).toString();
    const QString id = model->getAssetId();
    if (EffectsRepository::get()->exists(id)) {
        setText(i18n("Edit %1", EffectsRepository::get()->getName(id)));
    } else if (TransitionsRepository::get()->exists(id)) {
        setText(i18n("Edit %1", TransitionsRepository::get()->getName(id)));
    }
    m_oldValue = m_model->data(index, AssetParameterModel::ValueRole).toString();
}

void AssetCommand::undo()
{
    if (m_name.contains(QLatin1Char('\n'))) {
        // Check if it is a multi param
        auto type = m_model->data(m_index, AssetParameterModel::TypeRole).value<ParamType>();
        if (type == ParamType::MultiSwitch) {
            QStringList names = m_name.split(QLatin1Char('\n'));
            QStringList oldValues = m_oldValue.split(QLatin1Char('\n'));
            if (names.count() == oldValues.count()) {
                for (int i = 0; i < names.count(); i++) {
                    m_model->setParameter(names.at(i), oldValues.at(i), true, m_index);
                }
                return;
            }
        }
    }
    m_model->setParameter(m_name, m_oldValue, true, m_index);
    QUndoCommand::undo();
}

void AssetCommand::redo()
{
    if (m_name.contains(QLatin1Char('\n'))) {
        // Check if it is a multi param
        auto type = m_model->data(m_index, AssetParameterModel::TypeRole).value<ParamType>();
        if (type == ParamType::MultiSwitch) {
            QStringList names = m_name.split(QLatin1Char('\n'));
            QStringList values = m_value.split(QLatin1Char('\n'));
            if (names.count() == values.count()) {
                for (int i = 0; i < names.count(); i++) {
                    m_model->setParameter(names.at(i), values.at(i), m_updateView, m_index);
                }
                m_updateView = true;
                return;
            }
        }
    }
    m_model->setParameter(m_name, m_value, m_updateView, m_index);
    m_updateView = true;
    QUndoCommand::redo();
}

int AssetCommand::id() const
{
    return 1;
}

ObjectId AssetCommand::owner() const
{
    return m_model->getOwnerId();
}

bool AssetCommand::mergeWith(const QUndoCommand *other)
{
    if (other->id() != id() || static_cast<const AssetCommand *>(other)->owner() != owner() || static_cast<const AssetCommand *>(other)->m_index != m_index ||
        m_stamp.msecsTo(static_cast<const AssetCommand *>(other)->m_stamp) > 3000) {
        return false;
    }
    m_value = static_cast<const AssetCommand *>(other)->m_value;
    m_stamp = static_cast<const AssetCommand *>(other)->m_stamp;
    return true;
}

AssetMultiCommand::AssetMultiCommand(const std::shared_ptr<AssetParameterModel> &model, const QList<QModelIndex> &indexes, const QStringList &values,
                                     QUndoCommand *parent)
    : QUndoCommand(parent)
    , m_model(model)
    , m_indexes(indexes)
    , m_values(values)
    , m_updateView(false)
    , m_stamp(QTime::currentTime())
{
    qDebug() << "CREATING MULTIPLE COMMAND!!!\nVALUES: " << m_values;
    m_name = m_model->data(m_indexes.first(), AssetParameterModel::NameRole).toString();
    const QString id = model->getAssetId();
    if (EffectsRepository::get()->exists(id)) {
        setText(i18n("Edit %1", EffectsRepository::get()->getName(id)));
    } else if (TransitionsRepository::get()->exists(id)) {
        setText(i18n("Edit %1", TransitionsRepository::get()->getName(id)));
    }
    for (QModelIndex ix : qAsConst(m_indexes)) {
        QVariant previousVal = m_model->data(ix, AssetParameterModel::ValueRole);
        m_oldValues << previousVal.toString();
    }
}

void AssetMultiCommand::undo()
{
    int indx = 0;
    int max = m_indexes.size() - 1;
    for (const QModelIndex &ix : qAsConst(m_indexes)) {
        m_model->setParameter(m_model->data(ix, AssetParameterModel::NameRole).toString(), m_oldValues.at(indx), indx == max, ix);
        indx++;
    }
}
// virtual
void AssetMultiCommand::redo()
{
    int indx = 0;
    int max = m_indexes.size() - 1;
    for (const QModelIndex &ix : qAsConst(m_indexes)) {
        m_model->setParameter(m_model->data(ix, AssetParameterModel::NameRole).toString(), m_values.at(indx), m_updateView && indx == max, ix);
        indx++;
    }
    m_updateView = true;
}

// virtual
int AssetMultiCommand::id() const
{
    return 1;
}
// virtual
bool AssetMultiCommand::mergeWith(const QUndoCommand *other)
{
    if (other->id() != id() || static_cast<const AssetMultiCommand *>(other)->m_indexes != m_indexes ||
        m_stamp.msecsTo(static_cast<const AssetMultiCommand *>(other)->m_stamp) > 3000) {
        return false;
    }
    m_values = static_cast<const AssetMultiCommand *>(other)->m_values;
    m_stamp = static_cast<const AssetMultiCommand *>(other)->m_stamp;
    return true;
}

AssetKeyframeCommand::AssetKeyframeCommand(const std::shared_ptr<AssetParameterModel> &model, const QModelIndex &index, QVariant value, GenTime pos,
                                           QUndoCommand *parent)
    : QUndoCommand(parent)
    , m_model(model)
    , m_index(index)
    , m_value(std::move(value))
    , m_pos(pos)
    , m_updateView(false)
    , m_stamp(QTime::currentTime())
{
    const QString id = model->getAssetId();
    if (EffectsRepository::get()->exists(id)) {
        setText(i18n("Edit %1 keyframe", EffectsRepository::get()->getName(id)));
    } else if (TransitionsRepository::get()->exists(id)) {
        setText(i18n("Edit %1 keyframe", TransitionsRepository::get()->getName(id)));
    }
    m_oldValue = m_model->getKeyframeModel()->getKeyModel(m_index)->getInterpolatedValue(m_pos);
}

void AssetKeyframeCommand::undo()
{
    m_model->getKeyframeModel()->getKeyModel(m_index)->directUpdateKeyframe(m_pos, m_oldValue);
    QUndoCommand::undo();
}
// virtual
void AssetKeyframeCommand::redo()
{
    m_model->getKeyframeModel()->getKeyModel(m_index)->directUpdateKeyframe(m_pos, m_value);
    m_updateView = true;
    QUndoCommand::redo();
}

// virtual
int AssetKeyframeCommand::id() const
{
    return 2;
}
// virtual
bool AssetKeyframeCommand::mergeWith(const QUndoCommand *other)
{
    if (other->id() != id() || static_cast<const AssetKeyframeCommand *>(other)->m_index != m_index ||
        m_stamp.msecsTo(static_cast<const AssetKeyframeCommand *>(other)->m_stamp) > 1000) {
        return false;
    }
    m_value = static_cast<const AssetKeyframeCommand *>(other)->m_value;
    m_stamp = static_cast<const AssetKeyframeCommand *>(other)->m_stamp;
    return true;
}

AssetMultiKeyframeCommand::AssetMultiKeyframeCommand(const std::shared_ptr<AssetParameterModel> &model, const QList<QModelIndex> &indexes,
                                                     const QStringList &sourceValues, const QStringList &values, GenTime pos, QUndoCommand *parent)
    : QUndoCommand(parent)
    , m_model(model)
    , m_indexes(indexes)
    , m_values(values)
    , m_oldValues(sourceValues)
    , m_pos(pos)
    , m_stamp(QTime::currentTime())
{
    const QString id = model->getAssetId();
    if (EffectsRepository::get()->exists(id)) {
        setText(i18n("Edit %1 keyframe", EffectsRepository::get()->getName(id)));
    } else if (TransitionsRepository::get()->exists(id)) {
        setText(i18n("Edit %1 keyframe", TransitionsRepository::get()->getName(id)));
    }
}

void AssetMultiKeyframeCommand::undo()
{
    int indx = 0;
    for (const QModelIndex &ix : qAsConst(m_indexes)) {
        m_model->getKeyframeModel()->getKeyModel(ix)->directUpdateKeyframe(m_pos, m_oldValues.at(indx), false);
        m_model->getKeyframeModel()->getKeyModel(ix)->sendModification();
        indx++;
    }
    Q_EMIT m_model->getKeyframeModel()->modelChanged();
    QUndoCommand::undo();
}

// virtual
void AssetMultiKeyframeCommand::redo()
{
    int indx = 0;
    for (const QModelIndex &ix : qAsConst(m_indexes)) {
        m_model->getKeyframeModel()->getKeyModel(ix)->directUpdateKeyframe(m_pos, m_values.at(indx), false);
        m_model->getKeyframeModel()->getKeyModel(ix)->sendModification();
        indx++;
    }
    Q_EMIT m_model->getKeyframeModel()->modelChanged();
    QUndoCommand::redo();
}

// virtual
int AssetMultiKeyframeCommand::id() const
{
    return 4;
}
// virtual
bool AssetMultiKeyframeCommand::mergeWith(const QUndoCommand *other)
{
    if (other->id() != id()) {
        return false;
    }
    const AssetMultiKeyframeCommand *otherCommand = static_cast<const AssetMultiKeyframeCommand *>(other);
    if (!otherCommand || otherCommand->m_model != m_model || m_stamp.msecsTo(otherCommand->m_stamp) > 3000) {
        return false;
    }
    m_values = static_cast<const AssetMultiKeyframeCommand *>(other)->m_values;
    m_stamp = static_cast<const AssetMultiKeyframeCommand *>(other)->m_stamp;
    return true;
}

AssetUpdateCommand::AssetUpdateCommand(const std::shared_ptr<AssetParameterModel> &model, QVector<QPair<QString, QVariant>> parameters, QUndoCommand *parent)
    : QUndoCommand(parent)
    , m_model(model)
    , m_value(std::move(parameters))
{
    const QString id = model->getAssetId();
    if (EffectsRepository::get()->exists(id)) {
        setText(i18n("Update %1", EffectsRepository::get()->getName(id)));
    } else if (TransitionsRepository::get()->exists(id)) {
        setText(i18n("Update %1", TransitionsRepository::get()->getName(id)));
    }
    m_oldValue = m_model->getAllParameters();
}

void AssetUpdateCommand::undo()
{
    m_model->setParameters(m_oldValue);
}
// virtual
void AssetUpdateCommand::redo()
{
    m_model->setParameters(m_value);
}

// virtual
int AssetUpdateCommand::id() const
{
    return 3;
}
