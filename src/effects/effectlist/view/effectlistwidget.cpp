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

#include "effectlistwidget.hpp"
#include "../model/effecttreemodel.hpp"
#include "../model/effectfilter.hpp"
#include "assets/assetlist/view/qmltypes/asseticonprovider.hpp"

#include <KDeclarative/KDeclarative>
#include <QStandardPaths>
#include <QQmlContext>

EffectListWidget::EffectListWidget(QWidget *parent)
    : QQuickWidget(parent)
{

    QString effectCategory = QStandardPaths::locate(QStandardPaths::AppDataLocation, QStringLiteral("kdenliveeffectscategory.rc"));
    m_model.reset(new EffectTreeModel(effectCategory, this));


    m_proxyModel = new EffectFilter(this);
    m_proxyModel->setSourceModel(m_model.get());
    m_proxyModel->setSortRole(EffectTreeModel::NameRole);
    m_proxyModel->sort(0, Qt::AscendingOrder);

    KDeclarative::KDeclarative kdeclarative;
    kdeclarative.setDeclarativeEngine(engine());
    kdeclarative.setupBindings();

    setResizeMode(QQuickWidget::SizeRootObjectToView);
    rootContext()->setContextProperty("assetlist", this);
    rootContext()->setContextProperty("assetListModel", m_proxyModel);
    rootContext()->setContextProperty("isEffectList", true);

    m_assetIconProvider.reset(new AssetIconProvider(true));
    engine()->addImageProvider(QStringLiteral("asseticon"), m_assetIconProvider.get());
    setSource(QUrl(QStringLiteral("qrc:/qml/assetList.qml")));
    setFocusPolicy(Qt::StrongFocus);
}


QString EffectListWidget::getName(const QModelIndex& index) const
{
    return m_model->getName(m_proxyModel->mapToSource(index));
}

QString EffectListWidget::getDescription(const QModelIndex& index) const
{
    return m_model->getDescription(m_proxyModel->mapToSource(index));
}

void EffectListWidget::setFilterName(const QString& pattern)
{
    m_proxyModel->setFilterName(!pattern.isEmpty(), pattern);
}

void EffectListWidget::setFilterType(const QString& type)
{
    if (type == "video") {
        m_proxyModel->setFilterType(true, EffectType::Video);
    } else if (type == "audio") {
        m_proxyModel->setFilterType(true, EffectType::Audio);
    } else if (type == "custom") {
        m_proxyModel->setFilterType(true, EffectType::Custom);
    } else {
        m_proxyModel->setFilterType(false, EffectType::Video);
    }
}
