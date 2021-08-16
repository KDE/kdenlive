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
#include "assets/assetlist/view/qmltypes/asseticonprovider.hpp"
#include "dialogs/profilesdialog.h"
#include "core.h"
#include "mainwindow.h"
#include "mltconnection.h"
#include "transitions/transitionlist/model/transitionfilter.hpp"
#include "transitions/transitionsrepository.hpp"

#include <QQmlContext>
#include <knewstuff_version.h>

TransitionListWidget::TransitionListWidget(QWidget *parent)
    : AssetListWidget(parent)
{

    m_model = TransitionTreeModel::construct(true, this);

    m_proxyModel = std::make_unique<TransitionFilter>(this);
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
    qDebug() << " - - -Deleting transition list widget";
}

QString TransitionListWidget::getMimeType(const QString &assetId) const
{
    if (TransitionsRepository::get()->isComposition(assetId)) {
        return QStringLiteral("kdenlive/composition");
    }
    return QStringLiteral("kdenlive/transition");
}

void TransitionListWidget::updateFavorite(const QModelIndex &index)
{
    emit m_proxyModel->dataChanged(index, index, QVector<int>());
    m_proxyModel->reloadFilterOnFavorite();
    emit reloadFavorites();
}

void TransitionListWidget::setFilterType(const QString &type)
{
    if (type == "favorites") {
        static_cast<TransitionFilter *>(m_proxyModel.get())->setFilterType(true, AssetListType::AssetType::Favorites);
    } else {
        static_cast<TransitionFilter *>(m_proxyModel.get())->setFilterType(false, AssetListType::AssetType::Favorites);
    }
}

void TransitionListWidget::downloadNewLumas()
{
    if (pCore->getNewStuff(QStringLiteral(":data/kdenlive_wipes.knsrc")) > 0) {
        MltConnection::refreshLumas();
        // TODO: refresh currently displayed trans ?
    }
}

void TransitionListWidget::reloadCustomEffectIx(const QModelIndex &)
{
}

void TransitionListWidget::editCustomAsset(const QModelIndex &)
{

}
