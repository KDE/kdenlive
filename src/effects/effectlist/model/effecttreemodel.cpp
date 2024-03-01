/*
    SPDX-FileCopyrightText: 2017 Nicolas Carion
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "effecttreemodel.hpp"
#include "abstractmodel/treeitem.hpp"
#include "effects/effectsrepository.hpp"
#include "kdenlivesettings.h"

#include <QApplication>
#include <QDomDocument>
#include <QFile>
#include <array>
#include <vector>

#include <KActionCategory>
#include <KLocalizedString>
#include <KMessageBox>
#include <QDebug>
#include <QMenu>
#include <QMessageBox>
#include <QMimeData>

EffectTreeModel::EffectTreeModel(QObject *parent)
    : AssetTreeModel(parent)
    , m_customCategory(nullptr)
    , m_templateCategory(nullptr)
{
    m_assetIconProvider = new AssetIconProvider(true, this);
}

std::shared_ptr<EffectTreeModel> EffectTreeModel::construct(const QString &categoryFile, QObject *parent)
{
    std::shared_ptr<EffectTreeModel> self(new EffectTreeModel(parent));
    QList<QVariant> rootData{"Name", "ID", "Type", "isFav"};
    self->rootItem = TreeItem::construct(rootData, self, true);

    QHash<QString, std::shared_ptr<TreeItem>> effectCategory; // category in which each effect should land.

    std::shared_ptr<TreeItem> miscCategory = nullptr;
    std::shared_ptr<TreeItem> audioCategory = nullptr;
    // We parse category file
    QDomDocument doc;
    if (!categoryFile.isEmpty() && Xml::docContentFromFile(doc, categoryFile, false)) {
        QDomNodeList groups = doc.documentElement().elementsByTagName(QStringLiteral("group"));
        auto groupLegacy = self->rootItem->appendChild(QList<QVariant>{i18n("Legacy"), QStringLiteral("root")});

        for (int i = 0; i < groups.count(); i++) {
            QString groupName = i18n(groups.at(i).firstChild().firstChild().nodeValue().toUtf8().constData());
            if (!KdenliveSettings::gpu_accel() && groupName == i18n("GPU effects")) {
                continue;
            }
            QStringList list = groups.at(i).toElement().attribute(QStringLiteral("list")).split(QLatin1Char(','), Qt::SkipEmptyParts);
            auto groupItem = self->rootItem->appendChild(QList<QVariant>{groupName, QStringLiteral("root")});
            for (const QString &effect : qAsConst(list)) {
                effectCategory[effect] = groupItem;
            }
        }
        // We also create "Misc", "Audio" and "Custom" categories
        miscCategory = self->rootItem->appendChild(QList<QVariant>{i18n("Misc"), QStringLiteral("root")});
        audioCategory = self->rootItem->appendChild(QList<QVariant>{i18n("Audio"), QStringLiteral("root")});
        self->m_customCategory = self->rootItem->appendChild(QList<QVariant>{i18n("Custom"), QStringLiteral("root")});
        self->m_templateCategory = self->rootItem->appendChild(QList<QVariant>{i18n("Templates"), QStringLiteral("root")});
    } else {
        // Flat view
        miscCategory = self->rootItem;
        audioCategory = self->rootItem;
        self->m_customCategory = self->rootItem;
        self->m_templateCategory = self->rootItem;
    }

    // We parse effects
    auto allEffects = EffectsRepository::get()->getNames();
    QString favCategory = QStringLiteral("kdenlive:favorites");
    for (const auto &effect : qAsConst(allEffects)) {
        auto targetCategory = miscCategory;
        AssetListType::AssetType type = EffectsRepository::get()->getType(effect.first);
        if (effectCategory.contains(effect.first)) {
            targetCategory = effectCategory[effect.first];
        } else if (type == AssetListType::AssetType::Audio) {
            targetCategory = audioCategory;
        }

        if (type == AssetListType::AssetType::Custom || type == AssetListType::AssetType::CustomAudio) {
            targetCategory = self->m_customCategory;
        } else if (type == AssetListType::AssetType::Template || type == AssetListType::AssetType::TemplateAudio ||
                   type == AssetListType::AssetType::TemplateCustom || type == AssetListType::AssetType::TemplateCustomAudio) {
            targetCategory = self->m_templateCategory;
        }

        // we create the data list corresponding to this profile
        bool isFav = KdenliveSettings::favorite_effects().contains(effect.first);
        bool isPreferred = EffectsRepository::get()->isPreferred(effect.first);
        QList<QVariant> data;
        if (targetCategory->dataColumn(0).toString() == i18n("Deprecated")) {
            QString updatedName = effect.second + i18n(" - deprecated");
            data = {updatedName, effect.first, QVariant::fromValue(type), isFav, targetCategory->row(), isPreferred};
        } else {
            // qDebug() << effect.second << effect.first << "in " << targetCategory->dataColumn(0).toString();
            data = {effect.second, effect.first, QVariant::fromValue(type), isFav, targetCategory->row(), isPreferred};
        }
        if (KdenliveSettings::favorite_effects().contains(effect.first) && effectCategory.contains(favCategory)) {
            targetCategory = effectCategory[favCategory];
        }
        targetCategory->appendChild(data);
    }
    return self;
}

void EffectTreeModel::reloadEffectFromIndex(const QModelIndex &index)
{
    if (!index.isValid()) {
        return;
    }
    std::shared_ptr<TreeItem> item = getItemById(int(index.internalId()));
    const QString path = EffectsRepository::get()->getCustomPath(item->dataColumn(IdCol).toString());
    reloadEffect(path);
}

void EffectTreeModel::reloadEffect(const QString &path)
{
    QPair<QString, QString> asset = EffectsRepository::get()->reloadCustom(path);
    AssetListType::AssetType type = EffectsRepository::get()->getType(asset.first);
    std::shared_ptr<TreeItem> targetCategory = nullptr;
    if (type == AssetListType::AssetType::Custom || type == AssetListType::AssetType::CustomAudio) {
        targetCategory = m_customCategory;
    } else if (type == AssetListType::AssetType::Template || type == AssetListType::AssetType::TemplateAudio ||
               type == AssetListType::AssetType::TemplateCustom || type == AssetListType::AssetType::TemplateCustomAudio) {
        targetCategory = m_templateCategory;
    }
    if (asset.first.isEmpty() || targetCategory == nullptr) {
        return;
    }
    // Check if item already existed, and remove
    for (int i = 0; i < targetCategory->childCount(); i++) {
        std::shared_ptr<TreeItem> item = targetCategory->child(i);
        if (item->dataColumn(IdCol).toString() == asset.first) {
            targetCategory->removeChild(item);
            break;
        }
    }
    bool isFav = KdenliveSettings::favorite_effects().contains(asset.first);
    QString effectName = EffectsRepository::get()->getName(asset.first);
    QList<QVariant> data{effectName, asset.first, QVariant::fromValue(type), isFav};
    targetCategory->appendChild(data);
}

void EffectTreeModel::deleteEffect(const QModelIndex &index)
{
    if (!index.isValid()) {
        return;
    }
    std::shared_ptr<TreeItem> item = getItemById(int(index.internalId()));
    const QString id = item->dataColumn(IdCol).toString();
    if (item->hasAncestor(m_customCategory->getId())) {
        m_customCategory->removeChild(item);
    } else if (item->hasAncestor(m_templateCategory->getId())) {
        m_templateCategory->removeChild(item);
    } else {
        // ERRROR
        return;
    }
    EffectsRepository::get()->deleteEffect(id);
}

void EffectTreeModel::reloadTemplates()
{
    // First remove all templates effects
    if (!m_templateCategory) {
        return;
    }
    // Check if item already existed, and remove
    while (m_templateCategory->childCount() > 0) {
        std::shared_ptr<TreeItem> item = m_templateCategory->child(0);
        m_templateCategory->removeChild(item);
    }
    QStringList asset_dirs = QStandardPaths::locateAll(QStandardPaths::AppDataLocation, QStringLiteral("effect-templates"), QStandardPaths::LocateDirectory);
    QListIterator<QString> dirs_it(asset_dirs);
    for (dirs_it.toBack(); dirs_it.hasPrevious();) {
        auto dir = dirs_it.previous();
        QDir current_dir(dir);
        QStringList filter{QStringLiteral("*.xml")};
        QStringList fileList = current_dir.entryList(filter, QDir::Files);
        for (const auto &file : qAsConst(fileList)) {
            QString path = current_dir.absoluteFilePath(file);
            reloadEffect(path);
        }
    }
}

void EffectTreeModel::reloadAssetMenu(QMenu *effectsMenu, KActionCategory *effectActions)
{
    for (int i = 0; i < rowCount(); i++) {
        std::shared_ptr<TreeItem> item = rootItem->child(i);
        if (item->childCount() > 0) {
            QMenu *catMenu = new QMenu(item->dataColumn(AssetTreeModel::NameCol).toString(), effectsMenu);
            effectsMenu->addMenu(catMenu);
            for (int j = 0; j < item->childCount(); j++) {
                std::shared_ptr<TreeItem> child = item->child(j);
                QAction *a = new QAction(i18n(child->dataColumn(AssetTreeModel::NameCol).toString().toUtf8().data()), catMenu);
                const QString id = child->dataColumn(AssetTreeModel::IdCol).toString();
                a->setData(id);
                catMenu->addAction(a);
                effectActions->addAction("transition_" + id, a);
            }
        }
    }
}

void EffectTreeModel::setFavorite(const QModelIndex &index, bool favorite, bool isEffect)
{
    if (!index.isValid()) {
        return;
    }
    std::shared_ptr<TreeItem> item = getItemById(int(index.internalId()));
    if (isEffect && item->depth() == 1) {
        return;
    }
    item->setData(AssetTreeModel::FavCol, favorite);
    auto id = item->dataColumn(AssetTreeModel::IdCol).toString();
    if (!EffectsRepository::get()->exists(id)) {
        qDebug() << "Trying to reparent unknown asset: " << id;
        return;
    }
    QStringList favs = KdenliveSettings::favorite_effects();
    if (!favorite) {
        favs.removeAll(id);
    } else {
        favs << id;
    }
    KdenliveSettings::setFavorite_effects(favs);
}

void EffectTreeModel::editCustomAsset(const QString &newName, const QString &newDescription, const QModelIndex &index)
{

    std::shared_ptr<TreeItem> item = getItemById(int(index.internalId()));
    QString currentName = item->dataColumn(AssetTreeModel::NameCol).toString();

    QDomDocument doc;

    QDomElement effect = EffectsRepository::get()->getXml(currentName);
    QDir dir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + QStringLiteral("/effects/"));
    QString oldpath = dir.absoluteFilePath(currentName + QStringLiteral(".xml"));

    doc.appendChild(doc.importNode(effect, true));

    if (!newDescription.trimmed().isEmpty()) {
        QDomElement root = doc.documentElement();
        QDomElement nodelist = root.firstChildElement("description");
        QDomElement newNodeTag = doc.createElement(QString("description"));
        QDomText text = doc.createTextNode(newDescription);
        newNodeTag.appendChild(text);
        if (!nodelist.isNull()) {
            root.replaceChild(newNodeTag, nodelist);
        } else {
            root.appendChild(newNodeTag);
        }
    }

    if (!newName.trimmed().isEmpty() && newName != currentName) {
        if (!dir.exists()) {
            dir.mkpath(QStringLiteral("."));
        }

        if (dir.exists(newName + QStringLiteral(".xml"))) {
            QMessageBox message;
            message.critical(nullptr, i18n("Error"), i18n("Effect name %1 already exists.\n Try another name?", newName));
            message.setFixedSize(400, 200);
            return;
        }
        QFile file(dir.absoluteFilePath(newName + QStringLiteral(".xml")));

        QDomElement root = doc.documentElement();
        QDomElement nodelist = root.firstChildElement("name");
        QDomElement newNodeTag = doc.createElement(QString("name"));
        QDomText text = doc.createTextNode(newName);
        newNodeTag.appendChild(text);
        root.replaceChild(newNodeTag, nodelist);

        QDomElement e = doc.documentElement();
        e.setAttribute("id", newName);

        if (file.open(QFile::WriteOnly | QFile::Truncate)) {
            QTextStream out(&file);
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
            out.setCodec("UTF-8");
#endif
            out << doc.toString();
        } else {
            KMessageBox::error(QApplication::activeWindow(), i18n("Cannot write to file %1", file.fileName()));
        }
        file.close();

        deleteEffect(index);
        reloadEffect(dir.absoluteFilePath(newName + QStringLiteral(".xml")));

    } else {
        QFile file(dir.absoluteFilePath(currentName + QStringLiteral(".xml")));
        if (file.open(QFile::WriteOnly | QFile::Truncate)) {
            QTextStream out(&file);
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
            out.setCodec("UTF-8");
#endif
            out << doc.toString();
        } else {
            KMessageBox::error(QApplication::activeWindow(), i18n("Cannot write to file %1", file.fileName()));
        }
        file.close();
        reloadEffect(oldpath);
    }
}

QMimeData *EffectTreeModel::mimeData(const QModelIndexList &indexes) const
{
    QMimeData *mimeData = new QMimeData;
    std::shared_ptr<TreeItem> item = getItemById(int(indexes.first().internalId()));
    if (item) {
        const QString assetId = item->dataColumn(AssetTreeModel::IdCol).toString();
        mimeData->setData(QStringLiteral("kdenlive/effect"), assetId.toUtf8());
        if (EffectsRepository::get()->isAudioEffect(assetId)) {
            qDebug() << "::::: ASSET IS AUDIO!!!";
            mimeData->setData(QStringLiteral("type"), QByteArray("audio"));
        }
    }
    return mimeData;
}
