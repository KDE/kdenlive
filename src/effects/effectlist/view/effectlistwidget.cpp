/*
    SPDX-FileCopyrightText: 2017 Nicolas Carion
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

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
#include <QFileDialog>
#include <KIO/FileCopyJob>
#include <KRecentDirs>
#include <KMessageBox>

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

void EffectListWidget::exportCustomEffect(const QModelIndex &index) {
    QString id = getAssetId(index);
    if (id.isEmpty()) {
        return;
    }

    QString filter = QString("%1 (*.xml);;%2 (*)").arg(i18n("Kdenlive Effect definitions"), i18n("All Files"));
    QFileDialog dialog(this, i18n("Export Custom Effect"));
    QString startFolder = KRecentDirs::dir(QStringLiteral(":KdenliveExportCustomEffect"));
    QUrl source = QUrl::fromLocalFile(EffectsRepository::get()->getCustomPath(id));
    startFolder.append(source.fileName());
    QString filename = QFileDialog::getSaveFileName(this, i18n("Export Custom Effect"), startFolder, filter);
    QUrl target = QUrl::fromLocalFile(filename);
    if (source.isValid() && target.isValid()) {
        KRecentDirs::add(QStringLiteral(":KdenliveExportCustomEffect"), target.adjusted(QUrl::RemoveFilename).toLocalFile());
        KIO::FileCopyJob *copyjob = KIO::file_copy(source, target);
        if (!copyjob->exec()) {
            KMessageBox::sorry(this, i18n("Unable to write to file %1", target.toLocalFile()));
        }
    }
}
