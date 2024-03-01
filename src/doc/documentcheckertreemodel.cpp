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

Qt::ItemFlags DocumentCheckerTreeModel::flags(const QModelIndex &index) const
{
    const auto flags = QAbstractItemModel::flags(index);
    return flags;
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

    // auto createCat = [&](const QString &name) { return self->rootItem->appendChild(QList<QVariant>{name}); };
    // std::vector<std::shared_ptr<TreeItem>> cats{};

    QList<QVariant> data;
    data.reserve(3);
    for (const auto &item : items) {
        if (item.type == DocumentChecker::MissingType::Proxy) {
            // Skip proxy as they are shown in a different widget
            continue;
        }

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

void DocumentCheckerTreeModel::removeItem(const QModelIndex &ix)
{
    int itemId = int(ix.internalId());
    m_resourceItems[itemId].status = DocumentChecker::MissingStatus::Remove;
    Q_EMIT dataChanged(index(ix.row(), 0), index(ix.row(), columnCount() - 1));
}

void DocumentCheckerTreeModel::slotSearchRecursively(const QString &newpath)
{
    QDir searchDir(newpath);
    QMap<QModelIndex, QString> fixedMap;
    QMapIterator<int, DocumentChecker::DocumentResource> i(m_resourceItems);
    int counter = 1;
    while (i.hasNext()) {
        i.next();
        Q_EMIT searchProgress(counter, m_resourceItems.count());
        counter++;
        if (i.value().status != DocumentChecker::MissingStatus::Missing && i.value().status != DocumentChecker::MissingStatus::MissingButProxy) {
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
            fixedMap.insert(getIndexFromId(i.key()), newPath);
        }
    }
    QMapIterator<QModelIndex, QString> j(fixedMap);
    while (j.hasNext()) {
        j.next();
        setItemsNewFilePath(j.key(), j.value(), DocumentChecker::MissingStatus::Fixed, false);
    }
    Q_EMIT dataChanged(QModelIndex(), QModelIndex());
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
        if (i.value().status != DocumentChecker::MissingStatus::Missing && i.value().status != DocumentChecker::MissingStatus::MissingButProxy) {
            continue;
        }
        m_resourceItems[i.key()].status = DocumentChecker::MissingStatus::Placeholder;
    }
    Q_EMIT dataChanged(QModelIndex(), QModelIndex());
}

void DocumentCheckerTreeModel::setItemsNewFilePath(const QModelIndex &ix, const QString &url, DocumentChecker::MissingStatus status, bool refresh)
{
    int itemId = int(ix.internalId());
    m_resourceItems[itemId].status = status;
    m_resourceItems[itemId].newFilePath = url;
    if (refresh) {
        Q_EMIT dataChanged(index(ix.row(), 0), index(ix.row(), columnCount() - 1));
    }
}

void DocumentCheckerTreeModel::setItemsFileHash(const QModelIndex &index, const QString &hash)
{
    m_resourceItems[int(index.internalId())].hash = hash;
}

QVariant DocumentCheckerTreeModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid()) {
        qDebug() << "Index is not valid" << index;
        return QVariant();
    }

    if (m_resourceItems.contains(int(index.internalId()))) {
        DocumentChecker::DocumentResource resource = m_resourceItems.value(int(index.internalId()));

        if (role == Qt::ForegroundRole) {
            if (resource.status == DocumentChecker::MissingStatus::Remove) {
                KColorScheme scheme(qApp->palette().currentColorGroup(), KColorScheme::Window);
                return scheme.foreground(KColorScheme::InactiveText).color();
            }
        }

        if (role == Qt::BackgroundRole && resource.status == DocumentChecker::MissingStatus::Missing && index.column() == 1) {
            KColorScheme scheme(qApp->palette().currentColorGroup(), KColorScheme::Window);
            return scheme.background(KColorScheme::NegativeBackground).color();
        }

        if (role == Qt::FontRole) {
            if (resource.status == DocumentChecker::MissingStatus::Remove && index.column() == 2) {
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

            if (resource.status == DocumentChecker::MissingStatus::Reload) {
                return QIcon::fromTheme(QStringLiteral("dialog-information"));
            }

            if (resource.status == DocumentChecker::MissingStatus::Remove) {
                return QIcon::fromTheme(QStringLiteral("entry-delete"));
            }

            return QVariant();
        }
        if (role != Qt::DisplayRole) {
            return QVariant();
        }
        switch (index.column()) {
        case 0:
            return DocumentChecker::readableNameForMissingType(resource.type);
        case 1:
            return DocumentChecker::readableNameForMissingStatus(resource.status);
        case 2:
            return resource.originalFilePath;
        case 3:
            return resource.newFilePath;
        default:
            return QVariant();
        }
    }
    return QVariant();
}

DocumentChecker::DocumentResource DocumentCheckerTreeModel::getDocumentResource(const QModelIndex &index)
{
    return m_resourceItems.value(int(index.internalId()));
}
