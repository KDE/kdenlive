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
#include "../model/effectfilter.hpp"
#include "../model/effecttreemodel.hpp"
#include "assets/assetlist/view/qmltypes/asseticonprovider.hpp"

#include <KActionCategory>
#include <QMenu>
#include <QQmlContext>
#include <QStandardPaths>
#include <memory>
#include <QFormLayout>
#include <QDialog>
#include <QDialogButtonBox>
#include <QLineEdit>
#include <QTextEdit>

EffectListWidget::EffectListWidget(QWidget *parent)
    : AssetListWidget(parent)
{

    QString effectCategory = QStandardPaths::locate(QStandardPaths::AppDataLocation, QStringLiteral("kdenliveeffectscategory.rc"));
    m_model = EffectTreeModel::construct(effectCategory, this);

    m_proxyModel = std::make_unique<EffectFilter>(this);
    m_proxyModel->setSourceModel(m_model.get());
    m_proxyModel->setSortRole(EffectTreeModel::NameRole);
    m_proxyModel->sort(0, Qt::AscendingOrder);
    m_proxy = new EffectListWidgetProxy(this);
    rootContext()->setContextProperty("assetlist", m_proxy);
    rootContext()->setContextProperty("assetListModel", m_proxyModel.get());
    rootContext()->setContextProperty("isEffectList", true);
    m_assetIconProvider = new AssetIconProvider(true);

    setup();
    // Activate "Main effects" filter
    setFilterType("");
}

void EffectListWidget::updateFavorite(const QModelIndex &index)
{
    emit m_proxyModel->dataChanged(index, index, QVector<int>());
    m_proxyModel->reloadFilterOnFavorite();
    emit reloadFavorites();
}

EffectListWidget::~EffectListWidget()
{
    qDebug() << " - - -Deleting effect list widget";
}

void EffectListWidget::setFilterType(const QString &type)
{
    if (type == "video") {
        static_cast<EffectFilter *>(m_proxyModel.get())->setFilterType(true, AssetListType::AssetType::Video);
    } else if (type == "audio") {
        static_cast<EffectFilter *>(m_proxyModel.get())->setFilterType(true, AssetListType::AssetType::Audio);
    } else if (type == "custom") {
        static_cast<EffectFilter *>(m_proxyModel.get())->setFilterType(true, AssetListType::AssetType::Custom);
    } else if (type == "favorites") {
        static_cast<EffectFilter *>(m_proxyModel.get())->setFilterType(true, AssetListType::AssetType::Favorites);
    } else {
        static_cast<EffectFilter *>(m_proxyModel.get())->setFilterType(true, AssetListType::AssetType::Preferred);
    }
}

QString EffectListWidget::getMimeType(const QString &assetId) const
{
    Q_UNUSED(assetId);
    return QStringLiteral("kdenlive/effect");
}

void EffectListWidget::reloadCustomEffectIx(const QModelIndex &index)
{
    static_cast<EffectTreeModel *>(m_model.get())->reloadEffectFromIndex(m_proxyModel->mapToSource(index));
    m_proxyModel->sort(0, Qt::AscendingOrder);
}


void EffectListWidget::reloadCustomEffect(const QString &path)
{
    static_cast<EffectTreeModel *>(m_model.get())->reloadEffect(path);
    m_proxyModel->sort(0, Qt::AscendingOrder);
}

void EffectListWidget::reloadEffectMenu(QMenu *effectsMenu, KActionCategory *effectActions)
{
    m_model->reloadAssetMenu(effectsMenu, effectActions);
}

void EffectListWidget::editCustomAsset(const QModelIndex &index)
{
    QDialog dialog(this);
    QFormLayout form(&dialog);
    QString currentName = getName(index);
    QString desc = getDescription(true, index);
    // Strip effect Name
    if (desc.contains(QLatin1Char('('))) {
        desc = desc.section(QLatin1Char('('), 0, -2).simplified();
    }
    auto *effectName = new QLineEdit(currentName, &dialog);
    auto *descriptionBox = new QTextEdit(desc, &dialog);
    form.addRow(i18n("Name : "), effectName);
    form.addRow(i18n("Comments : "), descriptionBox);
    QDialogButtonBox buttonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, Qt::Horizontal, &dialog);
    form.addRow(&buttonBox);
    QObject::connect(&buttonBox, SIGNAL(accepted()), &dialog, SLOT(accept()));
    QObject::connect(&buttonBox, SIGNAL(rejected()), &dialog, SLOT(reject()));
    if(dialog.exec() == QDialog::Accepted) {
        QString name = effectName->text();
        QString enteredDescription = descriptionBox->toPlainText();
        if (name.trimmed().isEmpty() && enteredDescription.trimmed().isEmpty()) {
           return;
        }
        m_model->editCustomAsset(name, enteredDescription, m_proxyModel->mapToSource(index));
    }
}
