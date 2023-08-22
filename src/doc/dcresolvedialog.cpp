/*
    SPDX-FileCopyrightText: 2023 Julius Künzel <jk.kdedev@smartlab.uber.space>
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "dcresolvedialog.h"

#include <KUrlRequester>
#include <KUrlRequesterDialog>
#include <QDialog>
#include <QFontComboBox>
#include <QFontDatabase>
#include <QPointer>

DCResolveDialog::DCResolveDialog(std::vector<DocumentChecker::DocumentResource> items, QWidget *parent)
    : QDialog(parent)
{
    setupUi(this);
    setFont(QFontDatabase::systemFont(QFontDatabase::SmallestReadableFont));

    m_model = DocumentCheckerTreeModel::construct(items, this);
    removeSelected->setEnabled(false);
    manualSearch->setEnabled(false);
    treeView->setModel(m_model.get());
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
            m_model->removeItem(i);
        }
    });
    connect(manualSearch, &QPushButton::clicked, this, [&]() { slotEditCurrentItem(); });
    connect(usePlaceholders, &QPushButton::clicked, this, [&]() {
        m_model->usePlaceholdersForMissing();
        checkStatus();
    });
    connect(recursiveSearch, &QPushButton::clicked, this, [&]() { slotRecursiveSearch(); });

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
        infoLabel->setText(i18n("Recursive search: done"));
        infoLabel->setMessageType(KMessageWidget::MessageType::Positive);
        infoLabel->animatedShow();
        infoLabel->setCloseButtonVisible(true);
    });

    QItemSelectionModel *selectionModel = treeView->selectionModel();
    connect(selectionModel, &QItemSelectionModel::selectionChanged, this, &DCResolveDialog::newSelection);

    progressBox->setVisible(false);
    infoLabel->setVisible(false);
    proxyBox->setVisible(false);

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
        DocumentChecker::DocumentResource resource = m_model->getDocumentResource(i);
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
    QModelIndex index = selectionModel->currentIndex();
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

    if (m_proxies.size() > 0) {
        proxyLabel->setText(i18np("The project contains one missing proxy", "The project contains %1 missing proxies", m_proxies.size()));
        proxyBox->show();
    } else {
        proxyBox->hide();
    }
}

void DCResolveDialog::slotRecursiveSearch()
{
    QString clipFolder; // = m_url.adjusted(QUrl::RemoveFilename).toLocalFile();
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
    for (const auto &item : m_model->getDocumentResources()) {
        if (item.status == DocumentChecker::MissingStatus::Missing && item.type != DocumentChecker::MissingType::Proxy) {
            status = false;
        }
    }

    recursiveSearch->setEnabled(!status);
    buttonBox->button(QDialogButtonBox::Ok)->setEnabled(status);
}

void DCResolveDialog::setEnableChangeItems(bool enabled)
{
    recursiveSearch->setEnabled(enabled);
    manualSearch->setEnabled(enabled);
    removeSelected->setEnabled(enabled);
    usePlaceholders->setEnabled(enabled);
}
