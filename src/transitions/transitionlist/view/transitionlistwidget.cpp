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

#include "transitionlistwidget.hpp"
#include "../model/transitiontreemodel.hpp"
#include "assets/assetlist/model/assetfilter.hpp"
#include "transitions/transitionsrepository.hpp"

#include <QQmlContext>

TransitionListWidget::TransitionListWidget(QWidget *parent)
    : AssetListWidget(parent)
{

    m_model = TransitionTreeModel::construct(false, this);

    m_proxyModel.reset(new AssetFilter(this));
    m_proxyModel->setSourceModel(m_model.get());
    m_proxyModel->setSortRole(AssetTreeModel::NameRole);
    m_proxyModel->sort(0, Qt::AscendingOrder);

    m_proxy = new TransitionListWidgetProxy(this);
    rootContext()->setContextProperty("assetlist", m_proxy);
    rootContext()->setContextProperty("assetListModel", m_proxyModel.get());
    rootContext()->setContextProperty("isEffectList", false);
    m_assetIconProvider = new AssetIconProvider(false);

    setup();
}

TransitionListWidget::~TransitionListWidget()
{
    delete m_proxy;
    qDebug() << " - - -Deleting transition list widget";
}

QString TransitionListWidget::getMimeType(const QString &assetId) const
{
    if (TransitionsRepository::get()->isComposition(assetId)) {
        return QStringLiteral("kdenlive/composition");
    }
    return QStringLiteral("kdenlive/transition");
}
