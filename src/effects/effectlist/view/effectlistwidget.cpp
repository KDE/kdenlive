/*
    SPDX-FileCopyrightText: 2017 Nicolas Carion
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "effectlistwidget.hpp"
#include "../model/effectfilter.hpp"
#include "../model/effecttreemodel.hpp"
#include "assets/assetlist/view/asseticonprovider.hpp"

#include <KActionCategory>
#include <KIO/FileCopyJob>
#include <KMessageBox>
#include <KRecentDirs>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QHeaderView>
#include <QLineEdit>
#include <QMenu>
#include <QStandardPaths>
#include <QTextEdit>
#include <knewstuff_version.h>

#include <memory>

EffectListWidget::EffectListWidget(QWidget *parent)
    : AssetListWidget(true, parent)
{

    QString effectCategory = QStandardPaths::locate(QStandardPaths::AppDataLocation, QStringLiteral("kdenliveeffectscategory.rc"));
    m_model = EffectTreeModel::construct(effectCategory, this);
    m_proxyModel = std::make_unique<EffectFilter>(this);
    m_proxyModel->setSourceModel(m_model.get());
    m_proxyModel->setSortRole(EffectTreeModel::NameRole);
    m_proxyModel->sort(0, Qt::AscendingOrder);
    m_effectsTree->setModel(m_proxyModel.get());
    m_effectsTree->setColumnHidden(1, true);
    m_effectsTree->setColumnHidden(2, true);
    m_effectsTree->setColumnHidden(3, true);
    m_effectsTree->header()->setStretchLastSection(true);
    setFilterType("");
    QItemSelectionModel *sel = m_effectsTree->selectionModel();
    connect(sel, &QItemSelectionModel::currentChanged, this, &AssetListWidget::updateAssetInfo);
}

EffectListWidget::~EffectListWidget() {}

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
        static_cast<EffectFilter *>(m_proxyModel.get())->setFilterType(false, AssetListType::AssetType::Preferred);
    }
}

bool EffectListWidget::isAudio(const QString &assetId) const
{
    return EffectsRepository::get()->isAudioEffect(assetId);
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
    QString desc = getDescription(index);
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
    QObject::connect(&buttonBox, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    QObject::connect(&buttonBox, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    if (dialog.exec() == QDialog::Accepted) {
        QString name = effectName->text();
        QString enteredDescription = descriptionBox->toPlainText();
        if (name.trimmed().isEmpty() && enteredDescription.trimmed().isEmpty()) {
            return;
        }
        m_model->editCustomAsset(name, enteredDescription, m_proxyModel->mapToSource(index));
    }
}

void EffectListWidget::exportCustomEffect(const QModelIndex &index)
{
    QString name = getName(index);
    if (name.isEmpty()) {
        return;
    }

    QString filter = QString("%1 (*.xml);;%2 (*)").arg(i18n("Kdenlive Effect definitions"), i18n("All Files"));
    QString startFolder = KRecentDirs::dir(QStringLiteral(":KdenliveExportCustomEffect"));
    QUrl source = QUrl::fromLocalFile(EffectsRepository::get()->getCustomPath(name));
    startFolder.append(source.fileName());

    QString filename = QFileDialog::getSaveFileName(this, i18nc("@title:window", "Export Custom Effect"), startFolder, filter);
    QUrl target = QUrl::fromLocalFile(filename);

    if (source.isValid() && target.isValid()) {
        KRecentDirs::add(QStringLiteral(":KdenliveExportCustomEffect"), target.adjusted(QUrl::RemoveFilename).toLocalFile());
        KIO::FileCopyJob *copyjob = KIO::file_copy(source, target);
        if (!copyjob->exec()) {
            KMessageBox::error(this, i18n("Unable to write to file %1", target.toLocalFile()));
        }
    }
}
