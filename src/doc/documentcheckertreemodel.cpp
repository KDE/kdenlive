/*
    SPDX-FileCopyrightText: 2023 Julius Künzel <jk.kdedev@smartlab.uber.space>
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "documentcheckertreemodel.h"

#include "abstractmodel/treeitem.hpp"

#include <KColorScheme>

DocumentCheckerTreeModel::DocumentCheckerTreeModel(QObject *parent)
    : AbstractTreeModel{parent}
    , m_resourceItems()
{
}

std::shared_ptr<DocumentCheckerTreeModel> DocumentCheckerTreeModel::construct(const std::vector<DocumentChecker::DocumentResource> &items, QObject *parent)
{
    std::shared_ptr<DocumentCheckerTreeModel> self(new DocumentCheckerTreeModel(parent));
    QList<QVariant> rootData;
    rootData << "Type"
             << "Status"
             << "Original Path"
             << "New Path";
    self->rootItem = TreeItem::construct(rootData, self, true);

    auto createCat = [&](const QString &name) { return self->rootItem->appendChild(QList<QVariant>{name}); };

    std::vector<std::shared_ptr<TreeItem>> cats{};

    QList<QVariant> data;
    data.reserve(3);
    for (const auto &item : items) {
        // we create the data list corresponding to this resource
        data.clear();
        data << DocumentChecker::readableNameForMissingType(item.type);
        data << DocumentChecker::readableNameForMissingStatus(item.status);
        data << item.originalFilePath;
        data << item.newFilePath;

        std::shared_ptr<TreeItem> newItem = self->rootItem->appendChild(data);
        self->m_resourceItems.insert(newItem->getId(), item);
    }

    return self;
}

void DocumentCheckerTreeModel::removeItem(const QModelIndex &index)
{
    auto item = getItemByIndex(index);
    m_resourceItems[item->getId()].status = DocumentChecker::MissingStatus::Remove;
    item->setData(1, DocumentChecker::readableNameForMissingStatus(m_resourceItems.value(item->getId()).status));
}

void DocumentCheckerTreeModel::slotSearchRecursively(const QString &newpath)
{
    QDir searchDir(newpath);

    QMapIterator<int, DocumentChecker::DocumentResource> i(m_resourceItems);
    int counter = 1;
    while (i.hasNext()) {
        i.next();
        Q_EMIT searchProgress(counter, m_resourceItems.count());
        counter++;
        if (i.value().status != DocumentChecker::MissingStatus::Missing) {
            continue;
        }
        QString newPath;
        if (i.value().type == DocumentChecker::MissingType::Clip) {
            ClipType::ProducerType type = i.value().clipType;
            if (type == ClipType::SlideShow) {
                // Slideshows cannot be found with hash / size
                newPath = DocumentChecker::searchDirRecursively(searchDir, i.value().hash, i.value().originalFilePath);
            } else {
                newPath = DocumentChecker::searchFileRecursively(searchDir, i.value().fileSize, i.value().hash, i.value().originalFilePath);
            }
            if (newPath.isEmpty()) {
                newPath = DocumentChecker::searchPathRecursively(searchDir, QUrl::fromLocalFile(i.value().originalFilePath).fileName(), type);
            }
        } else if (i.value().type == DocumentChecker::MissingType::Luma) {
            newPath = DocumentChecker::searchLuma(searchDir, i.value().originalFilePath);

        } else if (i.value().type == DocumentChecker::MissingType::AssetFile) {
            newPath = DocumentChecker::searchPathRecursively(searchDir, QFileInfo(i.value().originalFilePath).fileName());

        } else if (i.value().type == DocumentChecker::MissingType::TitleImage) {
            newPath = DocumentChecker::searchPathRecursively(searchDir, QFileInfo(i.value().originalFilePath).fileName());
        }
        if (!newPath.isEmpty()) {
            setItemsNewFilePath(getIndexFromId(i.key()), newPath, DocumentChecker::MissingStatus::Fixed);
        }
    }
    Q_EMIT searchDone();
}

void DocumentCheckerTreeModel::usePlaceholdersForMissing()
{
    QMapIterator<int, DocumentChecker::DocumentResource> i(m_resourceItems);
    while (i.hasNext()) {
        i.next();
        if (i.value().type == DocumentChecker::MissingType::TitleFont) {
            continue;
        }
        if (i.value().status != DocumentChecker::MissingStatus::Missing) {
            continue;
        }
        m_resourceItems[i.key()].status = DocumentChecker::MissingStatus::Placeholder;
        auto item = getItemById(i.key());
        item->setData(1, DocumentChecker::readableNameForMissingStatus(m_resourceItems.value(item->getId()).status));
    }
}

void DocumentCheckerTreeModel::setItemsNewFilePath(const QModelIndex &index, const QString &url, DocumentChecker::MissingStatus status)
{
    auto item = getItemByIndex(index);
    m_resourceItems[item->getId()].status = status;
    m_resourceItems[item->getId()].newFilePath = url;
    item->setData(1, DocumentChecker::readableNameForMissingStatus(m_resourceItems.value(item->getId()).status));
    item->setData(3, m_resourceItems.value(item->getId()).newFilePath);
}

void DocumentCheckerTreeModel::setItemsFileHash(const QModelIndex &index, const QString &hash)
{
    auto item = getItemByIndex(index);
    m_resourceItems[item->getId()].hash = hash;
}

QVariant DocumentCheckerTreeModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid()) {
        qDebug() << "Index is not valid" << index;
        return QVariant();
    }

    if (m_resourceItems.contains(int(index.internalId()))) {
        DocumentChecker::DocumentResource resource = m_resourceItems.value(int(index.internalId()));

        KColorScheme scheme(qApp->palette().currentColorGroup(), KColorScheme::Window);
        if (role == Qt::ForegroundRole) {
            if (resource.status == DocumentChecker::MissingStatus::Remove) {
                return scheme.foreground(KColorScheme::InactiveText).color();
            }
        }

        if (role == Qt::BackgroundRole) {
            if (resource.status == DocumentChecker::MissingStatus::Missing) {
                return scheme.background(KColorScheme::NegativeBackground).color();
            }
        }

        if (role == Qt::FontRole) {
            if (resource.status == DocumentChecker::MissingStatus::Remove) {
                QFont f = qApp->font();
                f.setStrikeOut(true);
                return f;
            }
        }

        if (role == Qt::ToolTipRole) {
            if (!resource.originalFilePath.isEmpty()) {
                return resource.originalFilePath;
            }
        }

        if (role == Qt::DecorationRole && index.column() == 1) {
            if (resource.status == DocumentChecker::MissingStatus::Missing) {
                return QIcon::fromTheme(QStringLiteral("dialog-close"));
            }

            if (resource.status == DocumentChecker::MissingStatus::Fixed) {
                return QIcon::fromTheme(QStringLiteral("dialog-ok"));
            }

            if (resource.status == DocumentChecker::MissingStatus::Placeholder) {
                return QIcon::fromTheme(QStringLiteral("view-preview"));
            }

            if (resource.status == DocumentChecker::MissingStatus::Reloaded) {
                return QIcon::fromTheme(QStringLiteral("dialog-information"));
            }

            if (resource.status == DocumentChecker::MissingStatus::Remove) {
                return QIcon::fromTheme(QStringLiteral("entry-delete"));
            }

            return QVariant();
        }
    }

    auto item = getItemById(int(index.internalId()));
    /*if (role == Qt::DecorationRole) {
        if (item->depth() == 1) {
            return QIcon::fromTheme(QStringLiteral("folder"));
        }
    }*/

    if (role != Qt::DisplayRole) {
        return QVariant();
    }
    return item->dataColumn(index.column());
}

std::shared_ptr<TreeItem> DocumentCheckerTreeModel::getItemByIndex(const QModelIndex &index)
{
    return getItemById(int(index.internalId()));
}

DocumentChecker::DocumentResource DocumentCheckerTreeModel::getDocumentResource(const QModelIndex &index)
{
    auto item = getItemByIndex(index);
    return m_resourceItems.value(item->getId());
}
