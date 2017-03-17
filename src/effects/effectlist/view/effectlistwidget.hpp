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

#ifndef EFFECTLISTWIDGET_H
#define EFFECTLISTWIDGET_H

#include <QQuickWidget>
#include <memory>
#include "qmltypes/effecticonprovider.hpp"
#include "effects/effectsrepository.hpp"

/* @brief This class is a widget that display the list of available effects
 */


class EffectFilter;
class EffectTreeModel;
class EffectListWidget : public QQuickWidget
{
    Q_OBJECT

public:
    EffectListWidget(QWidget *parent = Q_NULLPTR);

    Q_INVOKABLE QString getName(const QModelIndex& index) const;
    Q_INVOKABLE QString getDescription(const QModelIndex& index) const;
    Q_INVOKABLE void setFilterName(const QString& pattern);
    Q_INVOKABLE void setFilterType(const QString& type);
private:
    std::unique_ptr<EffectTreeModel> m_model;
    EffectFilter *m_proxyModel;

    std::unique_ptr<EffectIconProvider> m_effectIconProvider;

signals:
};

#endif


