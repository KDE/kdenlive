/*
    SPDX-FileCopyrightText: 2017 Nicolas Carion
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "transitionlistwidget.hpp"
#include "../model/transitiontreemodel.hpp"
#include "assets/assetlist/view/asseticonprovider.hpp"
#include "core.h"
#include "dialogs/profilesdialog.h"
#include "mainwindow.h"
#include "mltconnection.h"
#include "transitions/transitionlist/model/transitionfilter.hpp"
#include "transitions/transitionsrepository.hpp"

#include <QHeaderView>
#include <knewstuff_version.h>

TransitionListWidget::TransitionListWidget(QWidget *parent)
    : AssetListWidget(false, parent)
{
    m_model = TransitionTreeModel::construct(true, this);
    m_proxyModel = std::make_unique<TransitionFilter>(this);
    m_proxyModel->setSourceModel(m_model.get());
    m_proxyModel->setSortRole(AssetTreeModel::NameRole);
    m_proxyModel->sort(0, Qt::AscendingOrder);
    m_effectsTree->setModel(m_proxyModel.get());
    m_effectsTree->setColumnHidden(1, true);
    m_effectsTree->setColumnHidden(2, true);
    m_effectsTree->setColumnHidden(3, true);
    m_effectsTree->setColumnHidden(4, true);
    m_effectsTree->header()->setStretchLastSection(true);
    QItemSelectionModel *sel = m_effectsTree->selectionModel();
    connect(sel, &QItemSelectionModel::currentChanged, this, &AssetListWidget::updateAssetInfo);
}

TransitionListWidget::~TransitionListWidget() {}

bool TransitionListWidget::isAudio(const QString &assetId) const
{
    return TransitionsRepository::get()->isAudio(assetId);
}

QString TransitionListWidget::getMimeType(const QString &assetId) const
{
    Q_UNUSED(assetId);
    return QStringLiteral("kdenlive/composition");
}

void TransitionListWidget::setFilterType(const QString &type)
{
    if (type == "favorites") {
        static_cast<TransitionFilter *>(m_proxyModel.get())->setFilterType(true, AssetListType::AssetType::Favorites);
    } else if (type == "transition") {
        static_cast<TransitionFilter *>(m_proxyModel.get())->setFilterType(true, AssetListType::AssetType::VideoTransition);
    } else {
        static_cast<TransitionFilter *>(m_proxyModel.get())->setFilterType(false, AssetListType::AssetType::Favorites);
    }
}

void TransitionListWidget::refreshLumas()
{
    MltConnection::refreshLumas();
    // TODO: refresh currently displayed trans ?
}

void TransitionListWidget::reloadCustomEffectIx(const QModelIndex &) {}

void TransitionListWidget::reloadCustomEffect(const QString &) {}

void TransitionListWidget::reloadTemplates() {}

void TransitionListWidget::editCustomAsset(const QModelIndex &) {}

void TransitionListWidget::exportCustomEffect(const QModelIndex &){};
