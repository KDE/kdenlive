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

#ifndef ASSETLISTWIDGET_H
#define ASSETLISTWIDGET_H

#include <QQuickWidget>
#include <memory>
#include "effects/effectsrepository.hpp"
#include "../model/assettreemodel.hpp"
#include "../model/assetfilter.hpp"
#include "assets/assetlist/view/qmltypes/asseticonprovider.hpp"

/* @brief This class is a generic widget that display the list of available assets
 */


class AssetListWidget : public QQuickWidget
{
    Q_OBJECT

public:
    AssetListWidget(QWidget *parent = Q_NULLPTR);

    Q_INVOKABLE QString getName(const QModelIndex& index) const;
    Q_INVOKABLE QString getDescription(const QModelIndex& index) const;
    Q_INVOKABLE void setFilterName(const QString& pattern);
protected:
    void setup();
    std::unique_ptr<AssetTreeModel> m_model;
    std::unique_ptr<AssetFilter> m_proxyModel;

    std::unique_ptr<AssetIconProvider> m_assetIconProvider;

signals:
};

#endif


