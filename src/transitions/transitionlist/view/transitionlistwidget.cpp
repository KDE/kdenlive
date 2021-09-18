/*
    SPDX-FileCopyrightText: 2017 Nicolas Carion
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

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
