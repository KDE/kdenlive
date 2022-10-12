/*
    SPDX-FileCopyrightText: 2017 Nicolas Carion
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include <KLocalizedContext>

#include "assetlistwidget.hpp"
#include "assets/assetlist/model/assetfilter.hpp"
#include "assets/assetlist/model/assettreemodel.hpp"
#include "assets/assetlist/view/qmltypes/asseticonprovider.hpp"

#include <KDeclarative/KDeclarative>
#include <QQmlContext>
#include <QQuickItem>
#include <QStandardPaths>
#include <kdeclarative_version.h>
#include <kquickiconprovider.h>

AssetListWidget::AssetListWidget(QWidget *parent)
    : QQuickWidget(parent)

{
    KDeclarative::KDeclarative kdeclarative;
    kdeclarative.setDeclarativeEngine(engine());
#if KDECLARATIVE_VERSION < QT_VERSION_CHECK(5, 98, 0)
    kdeclarative.setupEngine(engine());
#else
    engine()->addImageProvider(QStringLiteral("icon"), new KQuickIconProvider);
#endif
    engine()->rootContext()->setContextObject(new KLocalizedContext(this));
}

AssetListWidget::~AssetListWidget()
{
    // clear source
    setSource(QUrl());
}

void AssetListWidget::setup()
{
    setResizeMode(QQuickWidget::SizeRootObjectToView);
    engine()->addImageProvider(QStringLiteral("asseticon"), m_assetIconProvider);
    setSource(QUrl(QStringLiteral("qrc:/qml/assetList.qml")));
    setFocusPolicy(Qt::StrongFocus);
}

void AssetListWidget::reset()
{
    setSource(QUrl(QStringLiteral("qrc:/qml/assetList.qml")));
}

QString AssetListWidget::getName(const QModelIndex &index) const
{
    return m_model->getName(m_proxyModel->mapToSource(index));
}

bool AssetListWidget::isFavorite(const QModelIndex &index) const
{
    return m_model->isFavorite(m_proxyModel->mapToSource(index));
}

void AssetListWidget::setFavorite(const QModelIndex &index, bool favorite, bool isEffect)
{
    m_model->setFavorite(m_proxyModel->mapToSource(index), favorite, isEffect);
}

void AssetListWidget::deleteCustomEffect(const QModelIndex &index)
{
    m_model->deleteEffect(m_proxyModel->mapToSource(index));
}

QString AssetListWidget::getDescription(bool isEffect, const QModelIndex &index) const
{
    return m_model->getDescription(isEffect, m_proxyModel->mapToSource(index));
}

void AssetListWidget::setFilterName(const QString &pattern)
{
    m_proxyModel->setFilterName(!pattern.isEmpty(), pattern);
    if (!pattern.isEmpty()) {
        QVariantList mapped = m_proxyModel->getCategories();
        QMetaObject::invokeMethod(rootObject(), "expandNodes", Qt::DirectConnection, Q_ARG(QVariant, mapped));
    }
}

QVariantMap AssetListWidget::getMimeData(const QString &assetId) const
{
    QVariantMap mimeData;
    mimeData.insert(getMimeType(assetId), assetId);
    if (isAudio(assetId)) {
        mimeData.insert(QStringLiteral("type"), QStringLiteral("audio"));
    }
    return mimeData;
}

void AssetListWidget::activate(const QModelIndex &ix)
{
    if (!ix.isValid()) {
        return;
    }
    const QString assetId = m_model->data(m_proxyModel->mapToSource(ix), AssetTreeModel::IdRole).toString();
    emit activateAsset(getMimeData(assetId));
}
