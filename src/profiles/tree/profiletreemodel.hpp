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

#include <QAbstractItemModel>

/* @brief This class represents a profile hierarchy to be displayed as a tree
 */
class ProfileItem;
class ProfileModel;
class ProfileTreeModel : public QAbstractItemModel
  {
      Q_OBJECT

  public:
      explicit ProfileTreeModel(QObject *parent = nullptr);
      ~ProfileTreeModel();

      QVariant data(const QModelIndex &index, int role) const override;
      //This is reimplemented to prevent selection of the categories
      Qt::ItemFlags flags(const QModelIndex &index) const override;
      QVariant headerData(int section, Qt::Orientation orientation,
                          int role = Qt::DisplayRole) const override;
      QModelIndex index(int row, int column,
                        const QModelIndex &parent = QModelIndex()) const override;
      QModelIndex parent(const QModelIndex &index) const override;
      int rowCount(const QModelIndex &parent = QModelIndex()) const override;
      int columnCount(const QModelIndex &parent = QModelIndex()) const override;

      /*@brief Given a valid QModelIndex, this function retrieves the corresponding profile's path. Returns the empty string if something went wrong */
      static QString getProfile(const QModelIndex& index);

      /** @brief This function returns the model index corresponding to a given @param profile path */
      QModelIndex findProfile(const QString& profile);
  private:
      ProfileItem *rootItem;
};

#endif
