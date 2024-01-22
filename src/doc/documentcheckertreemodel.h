/*
    SPDX-FileCopyrightText: 2023 Julius Künzel <jk.kdedev@smartlab.uber.space>
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include "abstractmodel/abstracttreemodel.hpp"

#include "doc/documentchecker.h"

#include <vector>

class DocumentCheckerTreeModel : public AbstractTreeModel
{
    Q_OBJECT

protected:
    explicit DocumentCheckerTreeModel(QObject *parent = nullptr);

public:
    static std::shared_ptr<DocumentCheckerTreeModel> construct(const std::vector<DocumentChecker::DocumentResource> &items, QObject *parent = nullptr);

    void removeItem(const QModelIndex &ix);
    void slotSearchRecursively(const QString &newpath);
    void usePlaceholdersForMissing();
    void setItemsNewFilePath(const QModelIndex &ix, const QString &url, DocumentChecker::MissingStatus status, bool refresh = true);
    void setItemsFileHash(const QModelIndex &index, const QString &hash);

    QVariant data(const QModelIndex &index, int role) const override;

    QList<DocumentChecker::DocumentResource> getDocumentResources() { return m_resourceItems.values(); }
    DocumentChecker::DocumentResource getDocumentResource(const QModelIndex &index);
    // This is reimplemented to allow selection of the categories
    Qt::ItemFlags flags(const QModelIndex &index) const override;

    bool isEmpty() { return m_resourceItems.isEmpty(); }

private:
    std::shared_ptr<TreeItem> getItemByIndex(const QModelIndex &index);

    QMap<int, DocumentChecker::DocumentResource> m_resourceItems;

Q_SIGNALS:
    void searchProgress(int current, int total);
    void searchDone();
};
