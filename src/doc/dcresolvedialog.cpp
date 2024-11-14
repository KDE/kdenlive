/*
    SPDX-FileCopyrightText: 2023 Julius Künzel <julius.kuenzel@kde.org>
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "dcresolvedialog.h"

#include <KUrlRequester>
#include <KUrlRequesterDialog>
#include <QDialog>
#include <QFontComboBox>
#include <QFontDatabase>
#include <QPointer>

DCResolveDialog::DCResolveDialog(std::vector<DocumentChecker::DocumentResource> items, const QUrl &projectUrl, QWidget *parent)
    : QDialog(parent)
    , m_url(projectUrl)
{
    setupUi(this);
    setFont(QFontDatabase::systemFont(QFontDatabase::SmallestReadableFont));

    m_model = DocumentCheckerTreeModel::construct(items, this);
    removeSelected->setEnabled(false);
    manualSearch->setEnabled(false);
    m_sortModel = std::make_unique<QSortFilterProxyModel>(new QSortFilterProxyModel(this));
    m_sortModel->setSourceModel(m_model.get());
    treeView->setModel(m_sortModel.get());
    treeView->setAlternatingRowColors(true);
    treeView->setSortingEnabled(true);
    // treeView->header()->resizeSections(QHeaderView::ResizeToContents);
    treeView->setSelectionMode(QAbstractItemView::ExtendedSelection);
    bool showTreeView = !m_model.get()->isEmpty();
    treeView->setVisible(showTreeView);
    actionButtonBox->setVisible(showTreeView);

    connect(removeSelected, &QPushButton::clicked, this, [&]() {
        QItemSelectionModel *selectionModel = treeView->selectionModel();
        QModelIndexList selection = selectionModel->selectedRows();
        for (auto &i : selection) {
            m_model->removeItem(m_sortModel->mapToSource(i));
        }
        checkStatus();
    });
    connect(manualSearch, &QPushButton::clicked, this, &DCResolveDialog::slotEditCurrentItem);
    connect(usePlaceholders, &QPushButton::clicked, this, [&]() {
        m_model->usePlaceholdersForMissing();
        checkStatus();
    });
    connect(recursiveSearch, &QPushButton::clicked, this, [&]() {
        m_searchTimer.start();
        slotRecursiveSearch();
    });

    connect(m_model.get(), &DocumentCheckerTreeModel::searchProgress, this, [&](int current, int total) {
        setEnableChangeItems(false);
        progressBox->setVisible(true);
        progressLabel->setText(i18n("Recursive search: processing clips"));
        progressBar->setMinimum(0);
        progressBar->setMaximum(total);
        progressBar->setValue(current);
    });

    connect(m_model.get(), &DocumentCheckerTreeModel::searchDone, this, [&]() {
        setEnableChangeItems(true);
        progressBox->hide();
        infoLabel->setText(i18n("Recursive search: done in %1 s", QString::number(m_searchTimer.elapsed() / 1000., 'f', 2)));
        infoLabel->setMessageType(KMessageWidget::MessageType::Positive);
        infoLabel->animatedShow();
        infoLabel->setCloseButtonVisible(true);
    });

    QItemSelectionModel *selectionModel = treeView->selectionModel();
    connect(selectionModel, &QItemSelectionModel::selectionChanged, this, &DCResolveDialog::newSelection);

    progressBox->setVisible(false);
    infoLabel->setVisible(false);
    recreateProxies->setEnabled(false);
    initProxyPanel(items);

    checkStatus();
    adjustSize();
}

void DCResolveDialog::newSelection(const QItemSelection &, const QItemSelection &)
{
    QItemSelectionModel *selectionModel = treeView->selectionModel();
    QModelIndexList selection = selectionModel->selectedRows();
    bool notRemovable = false;
    for (auto &i : selection) {
        DocumentChecker::DocumentResource resource = m_model->getDocumentResource(m_sortModel->mapToSource(i));
        if (notRemovable == false && (resource.type == DocumentChecker::MissingType::TitleFont || resource.type == DocumentChecker::MissingType::TitleImage ||
                                      resource.type == DocumentChecker::MissingType::Effect || resource.type == DocumentChecker::MissingType::Transition)) {
            notRemovable = true;
        }
    }
    if (notRemovable) {
        removeSelected->setEnabled(false);
    } else {
        removeSelected->setEnabled(!selection.isEmpty());
    }
    usePlaceholders->setEnabled(!selection.isEmpty());
    manualSearch->setEnabled(!selection.isEmpty());
}

QList<DocumentChecker::DocumentResource> DCResolveDialog::getItems()
{
    QList<DocumentChecker::DocumentResource> items = m_model.get()->getDocumentResources();
    for (auto &proxy : m_proxies) {
        if (recreateProxies->isChecked()) {
            proxy.status = DocumentChecker::MissingStatus::Reload;
        } else {
            proxy.status = DocumentChecker::MissingStatus::Remove;
        }
        items.append(proxy);
    }
    return items;
}

void DCResolveDialog::slotEditCurrentItem()
{
    QItemSelectionModel *selectionModel = treeView->selectionModel();
    QModelIndex index = m_sortModel->mapToSource(selectionModel->currentIndex());
    DocumentChecker::DocumentResource resource = m_model->getDocumentResource(index);

    if (resource.type == DocumentChecker::MissingType::TitleFont) {
        QScopedPointer<QDialog> d(new QDialog(this));
        auto *l = new QVBoxLayout;
        auto *fontcombo = new QFontComboBox;
        QString selectedFont = resource.newFilePath.isEmpty() ? resource.originalFilePath : resource.newFilePath;
        fontcombo->setCurrentFont(QFontInfo(selectedFont).family());
        l->addWidget(fontcombo);
        QDialogButtonBox *box = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
        l->addWidget(box);
        connect(box, &QDialogButtonBox::accepted, d.data(), &QDialog::accept);
        connect(box, &QDialogButtonBox::rejected, d.data(), &QDialog::reject);
        d->setLayout(l);
        if (d->exec() == QDialog::Accepted) {
            m_model->setItemsNewFilePath(index, fontcombo->currentFont().family(), DocumentChecker::MissingStatus::Fixed);
        }
        checkStatus();
        return;
    }

    ClipType::ProducerType type = resource.clipType;
    QUrl url;
    QString path = resource.newFilePath.isEmpty() ? resource.originalFilePath : resource.newFilePath;
    if (type == ClipType::SlideShow) {
        path = QFileInfo(path).dir().absolutePath();
        QPointer<KUrlRequesterDialog> dlg(new KUrlRequesterDialog(QUrl::fromLocalFile(path), i18n("Enter new location for folder"), this));
        dlg->urlRequester()->setMode(KFile::Directory | KFile::ExistingOnly);
        if (dlg->exec() != QDialog::Accepted) {
            delete dlg;
            return;
        }
        url = QUrl::fromLocalFile(QDir(dlg->selectedUrl().path()).absoluteFilePath(QFileInfo(path).fileName()));
        // Reset hash to ensure we find it next time
        m_model->setItemsFileHash(index, QString());
        delete dlg;
    } else {
        url = KUrlRequesterDialog::getUrl(QUrl::fromLocalFile(path), this, i18n("Enter new location for file"));
    }
    if (!url.isValid()) {
        return;
    }
    bool fixed = false;
    if (type == ClipType::SlideShow && QFile::exists(url.adjusted(QUrl::RemoveFilename).toLocalFile())) {
        fixed = true;
    }
    if (fixed || QFile::exists(url.toLocalFile())) {
        m_model->setItemsNewFilePath(index, url.toLocalFile(), DocumentChecker::MissingStatus::Fixed);
    } else {
        m_model->setItemsNewFilePath(index, url.toLocalFile(), DocumentChecker::MissingStatus::Missing);
    }
    checkStatus();
}

void DCResolveDialog::initProxyPanel(const std::vector<DocumentChecker::DocumentResource> &items)
{
    m_proxies.clear();
    for (const auto &item : items) {
        if (item.type == DocumentChecker::MissingType::Proxy) {
            m_proxies.push_back(item);
        }
    }
}

void DCResolveDialog::updateStatusLabel(int missingClips, int missingClipsWithProxy, int removedClips, int placeholderClips, int missingProxies,
                                        int recoverableProxies)
{
    if (missingClips + removedClips + missingClipsWithProxy + missingProxies == 0) {
        statusLabel->hide();
        return;
    }
    QString statusMessage = i18n("The project contains:");
    statusMessage.append(QStringLiteral("<ul>"));
    QStringList missingMessage;
    if (missingClips + placeholderClips + removedClips + missingClipsWithProxy > 0) {
        statusMessage.append(QStringLiteral("<li>"));
        if (missingClips > 0) {
            missingMessage << i18np("One missing clip", "%1 missing clips", missingClips);
        }
        if (missingClipsWithProxy > 0) {
            missingMessage << i18np("One missing source with available proxy", "%1 missing sources with available proxy", missingClipsWithProxy);
        }
        if (placeholderClips > 0) {
            missingMessage << i18np("One placeholder clip", "%1 placeholder clips", placeholderClips);
        }
        if (removedClips > 0) {
            missingMessage << i18np("One removed clip", "%1 removed clips", removedClips);
        }
        statusMessage.append(missingMessage.join(QLatin1String(", ")));
        statusMessage.append(QStringLiteral("</li>"));
    }
    if (missingProxies > 0 || recoverableProxies > 0) {
        statusMessage.append(QStringLiteral("<li>"));
        QStringList proxyMessage;
        if (missingProxies > 0) {
            proxyMessage << i18np("One missing proxy", "%1 missing proxies", missingProxies);
        }
        if (recoverableProxies > 0) {
            proxyMessage << i18np("One proxy can be recovered", "%1 proxies can be recovered", recoverableProxies);
        }
        statusMessage.append(proxyMessage.join(QLatin1String(", ")));
        statusMessage.append(QStringLiteral("</li>"));
    }
    statusMessage.append(QStringLiteral("</ul>"));
    statusLabel->setText(statusMessage);
    if (recoverableProxies > 0) {
        recreateProxies->setEnabled(true);
    } else {
        recreateProxies->setEnabled(false);
    }
}

void DCResolveDialog::slotRecursiveSearch()
{
    QString clipFolder = m_url.adjusted(QUrl::RemoveFilename).toLocalFile();
    const QString newpath = QFileDialog::getExistingDirectory(qApp->activeWindow(), i18nc("@title:window", "Clips Folder"), clipFolder);
    if (newpath.isEmpty()) {
        return;
    }
    m_model->slotSearchRecursively(newpath);
    checkStatus();
}

void DCResolveDialog::checkStatus()
{
    bool status = true;
    int missingClips = 0;
    int removedClips = 0;
    int placeholderClips = 0;
    int missingProxies = 0;
    int missingWithProxies = 0;
    // Proxy clips that cannot be recovered (missing, removed or placeholder clip)
    int lostProxies = 0;
    QStringList idsToRemove;
    QStringList idsNotRecovered;
    for (const auto &item : m_model->getDocumentResources()) {
        if (item.status == DocumentChecker::MissingStatus::Missing && item.type != DocumentChecker::MissingType::Proxy) {
            missingClips++;
            idsNotRecovered << item.clipId;
            status = false;
        } else if (item.status == DocumentChecker::MissingStatus::MissingButProxy) {
            missingWithProxies++;
        } else if (item.status == DocumentChecker::MissingStatus::Remove) {
            idsToRemove << item.clipId;
            removedClips++;
        } else if (item.status == DocumentChecker::MissingStatus::Placeholder) {
            placeholderClips++;
            idsNotRecovered << item.clipId;
        }
    }
    for (auto &proxy : m_proxies) {
        if (idsToRemove.contains(proxy.clipId)) {
            lostProxies++;
        } else {
            missingProxies++;
            if (idsNotRecovered.contains(proxy.clipId)) {
                lostProxies++;
            }
        }
    }
    updateStatusLabel(missingClips, missingWithProxies, removedClips, placeholderClips, missingProxies, m_proxies.size() - lostProxies);
    recursiveSearch->setEnabled(!status || missingWithProxies > 0);
    buttonBox->button(QDialogButtonBox::Ok)->setEnabled(status);
}

void DCResolveDialog::setEnableChangeItems(bool enabled)
{
    recursiveSearch->setEnabled(enabled);
    manualSearch->setEnabled(enabled);
    removeSelected->setEnabled(enabled);
    usePlaceholders->setEnabled(enabled);
}
