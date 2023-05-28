/*
    SPDX-FileCopyrightText: 2017 Nicolas Carion
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include <KLocalizedContext>

#include "assetlistwidget.hpp"
#include "assets/assetlist/model/assetfilter.hpp"
#include "assets/assetlist/model/assettreemodel.hpp"
#include "assets/assetlist/view/qmltypes/asseticonprovider.hpp"

#include <QQmlContext>
#include <QQuickItem>
#include <QStandardPaths>
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
#include <KDeclarative/KDeclarative>
#include "kdeclarative_version.h"
#endif
#if QT_VERSION > QT_VERSION_CHECK(6, 0, 0) || KDECLARATIVE_VERSION >= QT_VERSION_CHECK(5, 98, 0)
#include <KQuickIconProvider>
#endif

AssetListWidget::AssetListWidget(QWidget *parent)
    : QQuickWidget(parent)

{
#if QT_VERSION > QT_VERSION_CHECK(6, 0, 0) || KDECLARATIVE_VERSION > QT_VERSION_CHECK(5, 98, 0)
    engine()->addImageProvider(QStringLiteral("icon"), new KQuickIconProvider);
#else
    KDeclarative::KDeclarative kdeclarative;
    kdeclarative.setDeclarativeEngine(engine());
    kdeclarative.setupEngine(engine());
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
    reset();
    setFocusPolicy(Qt::StrongFocus);
}

void AssetListWidget::reset()
{
#if QT_VERSION > QT_VERSION_CHECK(6,0,0)
    setSource(QUrl(QStringLiteral("qrc:/qml/assetList-qt6.qml")));
#else
    setSource(QUrl(QStringLiteral("qrc:/qml/assetList.qml")));
#endif
}

QString AssetListWidget::getName(const QModelIndex &index) const
{
    return m_model->getName(m_proxyModel->mapToSource(index));
}

bool AssetListWidget::isFavorite(const QModelIndex &index) const
{
    return m_model->isFavorite(m_proxyModel->mapToSource(index));
}

void AssetListWidget::setFavorite(const QModelIndex &index, bool favorite)
{
    m_model->setFavorite(m_proxyModel->mapToSource(index), favorite, isEffect());
    Q_EMIT m_proxyModel->dataChanged(index, index, QVector<int>());
    m_proxyModel->reloadFilterOnFavorite();
    Q_EMIT reloadFavorites();
}

void AssetListWidget::deleteCustomEffect(const QModelIndex &index)
{
    m_model->deleteEffect(m_proxyModel->mapToSource(index));
}

QString AssetListWidget::getDescription(const QModelIndex &index) const
{
    return m_model->getDescription(isEffect(), m_proxyModel->mapToSource(index));
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
    Q_EMIT activateAsset(getMimeData(assetId));
}

bool AssetListWidget::showDescription() const
{
    return KdenliveSettings::showeffectinfo();
}

void AssetListWidget::setShowDescription(bool show)
{
    KdenliveSettings::setShoweffectinfo(show);
    Q_EMIT showDescriptionChanged();
}
