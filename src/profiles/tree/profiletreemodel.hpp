/***************************************************************************
 *   Copyright (C) 2017 by Nicolas Carion                                  *
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

#ifndef PROFILETREEMODEL_H
#define PROFILETREEMODEL_H

#include "abstractmodel/abstracttreemodel.hpp"

/* @brief This class represents a profile hierarchy to be displayed as a tree
 */
class TreeItem;
class ProfileModel;
class ProfileTreeModel : public AbstractTreeModel
{
    Q_OBJECT

protected:
    explicit ProfileTreeModel(QObject *parent = nullptr);

public:
    static std::shared_ptr<ProfileTreeModel> construct(QObject *parent);

    void init();

    QVariant data(const QModelIndex &index, int role) const override;

    /*@brief Given a valid QModelIndex, this function retrieves the corresponding profile's path. Returns the empty string if something went wrong */
    QString getProfile(const QModelIndex &index);

    /** @brief This function returns the model index corresponding to a given @param profile path */
    QModelIndex findProfile(const QString &profile);
};

#endif
