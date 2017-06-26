/***************************************************************************
 *   Copyright (C) 2017 by by Jean-Baptiste Mardelle                                  *
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

#include "assetcommand.hpp"
#include "effects/effectsrepository.hpp"
#include "transitions/transitionsrepository.hpp"
#include <memory>

AssetCommand::AssetCommand(std::shared_ptr<AssetParameterModel> model, const QModelIndex &index, const QString &value, QUndoCommand *parent)
    : QUndoCommand(parent)
    , m_model(model)
    , m_index(index)
    , m_value(value)
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
    m_model->setParameter(m_name, m_oldValue);
    m_model->dataChanged(m_index, m_index, QVector<int>());
}
// virtual
void AssetCommand::redo()
{
    m_model->setParameter(m_name, m_value);
    if (m_updateView) {
        m_model->dataChanged(m_index, m_index, QVector<int>());
    }
    m_updateView = true;
}

// virtual
int AssetCommand::id() const
{
    return 1;
}
// virtual
bool AssetCommand::mergeWith(const QUndoCommand *other)
{
    if (other->id() != id() || static_cast<const AssetCommand *>(other)->m_index != m_index ||
        m_stamp.msecsTo(static_cast<const AssetCommand *>(other)->m_stamp) > 3000) {
        return false;
    }
    m_value = static_cast<const AssetCommand *>(other)->m_value;
    m_stamp = static_cast<const AssetCommand *>(other)->m_stamp;
    return true;
}
