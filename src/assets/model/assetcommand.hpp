/***************************************************************************
 *   Copyright (C) 2017 by Jean-Baptiste Mardelle                                  *
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

#ifndef ASSETCOMMAND_H
#define ASSETCOMMAND_H

#include "assetparametermodel.hpp"
#include <QPersistentModelIndex>
#include <QTime>
#include <QUndoCommand>

class AssetCommand : public QUndoCommand
{
public:
    AssetCommand(std::shared_ptr<AssetParameterModel> model, const QModelIndex &index, const QString &value, QUndoCommand *parent = nullptr);
    void undo() override;
    void redo() override;
    int id() const override;
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

#endif
