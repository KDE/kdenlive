/*
    SPDX-FileCopyrightText: 2017 Nicolas Carion
    SPDX-FileCopyrightText: 2022 Julius Künzel <jk.kdedev@smartlab.uber.space>
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "renderpresettreemodel.hpp"
#include "../renderpresetmodel.hpp"
#include "../renderpresetrepository.hpp"
#include "abstractmodel/treeitem.hpp"

#include <KColorScheme>
#include <KLocalizedString>
#include <QApplication>
#include <QIcon>
#include <array>
#include <functional>
#include <vector>

RenderPresetTreeModel::RenderPresetTreeModel(QObject *parent)
    : AbstractTreeModel(parent)
{
}

std::shared_ptr<RenderPresetTreeModel> RenderPresetTreeModel::construct(QObject *parent)
{
    std::shared_ptr<RenderPresetTreeModel> self(new RenderPresetTreeModel(parent));
    QList<QVariant> rootData;
    rootData << "Name";
    self->rootItem = TreeItem::construct(rootData, self, true);
    RenderPresetRepository::get()->refresh();
    QVector<QString> presets = RenderPresetRepository::get()->getAllPresets();

    // helper lambda that creates a preset category with the given name
    auto createCat = [&](const QString &name) { return self->rootItem->appendChild(QList<QVariant>{name}); };

    std::vector<std::shared_ptr<TreeItem>> cats{};

    for (const auto &preset : qAsConst(presets)) {
        bool foundMatch = false;
        // we get a pointer to the presetemodel
        std::unique_ptr<RenderPresetModel> &ptr = RenderPresetRepository::get()->getPreset(preset);
        // we create the data list corresponding to this preset
        QList<QVariant> data;
        data << preset;
        for (const auto &cat : cats) {
            if (cat->dataColumn(0) == ptr->groupName()) {
                cat->appendChild(data);
                foundMatch = true;
                break;
            }
        }
        if (!foundMatch) {
            // category not found, create it
            const auto &cat = createCat(ptr->groupName());
            cats.push_back(cat);
            cat->appendChild(data);
        }
    }
    return self;
}

QVariant RenderPresetTreeModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid()) {
        qDebug() << "Index is not valid" << index;
        return QVariant();
    }

    if (RenderPresetRepository::get()->presetExists(getPreset(index))) {
        std::unique_ptr<RenderPresetModel> &preset = RenderPresetRepository::get()->getPreset(getPreset(index));

        KColorScheme scheme(qApp->palette().currentColorGroup(), KColorScheme::Window);
        if (role == Qt::ForegroundRole) {
            if (!preset->error().isEmpty()) {
                return scheme.foreground(KColorScheme::InactiveText).color();
            }
        }

        if (role == Qt::BackgroundColorRole) {
            if (!preset->error().isEmpty()) {
                return scheme.background(KColorScheme::NegativeBackground).color();
            }
        }

        if (role == Qt::ToolTipRole) {
            if (!preset->note().isEmpty()) {
                return preset->note();
            }
        }

        if (role == Qt::DecorationRole) {
            if (!preset->error().isEmpty()) {
                return QIcon::fromTheme(QStringLiteral("dialog-close"));
            }

            if (!preset->warning().isEmpty()) {
                return QIcon::fromTheme(QStringLiteral("dialog-warning"));
            }

            switch (preset->installType()) {
                case RenderPresetModel::Custom:
                    return QIcon::fromTheme(QStringLiteral("favorite"));
                case RenderPresetModel::Download:
                    return QIcon::fromTheme(QStringLiteral("internet-services"));
                default:
                    return QVariant();
            }
        }


    }

    auto item = getItemById(int(index.internalId()));
    if (role == Qt::DecorationRole) {
        if (item->depth() == 1) {
            return QIcon::fromTheme(QStringLiteral("folder"));
        }
    }

    if (role != Qt::DisplayRole) {
        return QVariant();
    }
    return item->dataColumn(index.column());
}

QString RenderPresetTreeModel::getPreset(const QModelIndex &index) const
{
    if (index.isValid()) {
        auto item = getItemById(int(index.internalId()));
        if (item->depth() == 2) {
            return item->dataColumn(0).toString();
        }
    }
    return QString();
}

QModelIndex RenderPresetTreeModel::findPreset(const QString &presetName)
{
    if (presetName.isEmpty()) {
        return {};
    }
    // we iterate over categories
    for (int i = 0; i < rootItem->childCount(); ++i) {
        // we iterate over preset's of the category
        std::shared_ptr<TreeItem> category = rootItem->child(i);
        for (int j = 0; j < category->childCount(); ++j) {
            // we retrieve preset name
            std::shared_ptr<TreeItem> child = category->child(j);
            QString name = child->dataColumn(0).toString();
            if (name == presetName) {
                return createIndex(j, 0, quintptr(child->getId()));
            }
        }
    }
    return {};
}
