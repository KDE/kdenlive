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
    treeView->setModel(m_model.get());
    connect(removeSelected, &QPushButton::clicked, this, [&]() {
        QItemSelectionModel *selectionModel = treeView->selectionModel();
        m_model->removeItem(selectionModel->currentIndex());
    });
    connect(manualSearch, &QPushButton::clicked, this, [&]() { slotEditCurrentItem(); });
    connect(usePlaceholders, &QPushButton::clicked, this, [&]() { m_model->usePlaceholdersForMissing(); });
    connect(recursiveSearch, &QPushButton::clicked, this, [&]() { slotRecursiveSearch(); });

    connect(m_model.get(), &DocumentCheckerTreeModel::searchProgress, this, [&](int current, int total) {
        setEnableChangeItems(false);

        infoLabel->setVisible(true);
        infoLabel->setText(i18n("Recursive search: processing clip %1 of %2", current, total));
    });

    connect(m_model.get(), &DocumentCheckerTreeModel::searchDone, this, [&]() {
        setEnableChangeItems(true);
        infoLabel->setText(i18n("Recursive search: done"));
    });

    QItemSelectionModel *selectionModel = treeView->selectionModel();

    connect(selectionModel, &QItemSelectionModel::currentChanged, this, [&](const QModelIndex &current, const QModelIndex &) {
        DocumentChecker::DocumentResource resource = m_model->getDocumentResource(current);
        if (resource.type == DocumentChecker::MissingType::TitleFont || resource.type == DocumentChecker::MissingType::TitleImage ||
            resource.type == DocumentChecker::MissingType::Proxy) {
            removeSelected->setEnabled(false);
        } else {
            removeSelected->setEnabled(true);
        }
        if (resource.type == DocumentChecker::MissingType::Proxy) {
            manualSearch->setEnabled(false);
            usePlaceholders->setEnabled(false);
        } else {
            manualSearch->setEnabled(true);
            usePlaceholders->setEnabled(true);
        }
    });

    infoLabel->setVisible(false);
    checkStatus();
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

    //|| t == TITLE_IMAGE_ELEMENT) {
    ClipType::ProducerType type = resource.clipType; // ClipType::ProducerType(item->data(0, clipTypeRole).toInt());
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
        /*QDomNodeList producers = m_doc.elementsByTagName(QStringLiteral("producer"));
        QDomElement e;
        for (int i = 0; i < producers.count(); ++i) {
            e = producers.item(i).toElement();
            QString parentId = Xml::getXmlProperty(e, QStringLiteral("kdenlive:id"));
            if (parentId.isEmpty()) {
                // This is probably an old project file
                QString sourceId = e.attribute(QStringLiteral("id"));
                parentId = sourceId.section(QLatin1Char('_'), 0, 0);
            }
            if (parentId == resource.clipId) {
                // Fix clip
                Xml::removeXmlProperty(e, QStringLiteral("kdenlive:file_hash"));
            }
        }*/
        delete dlg;
    } else {
        url = KUrlRequesterDialog::getUrl(QUrl::fromLocalFile(path), this, i18n("Enter new location for file"));
    }
    if (!url.isValid()) {
        return;
    }
    // item->setText(1, url.toLocalFile());
    bool fixed = false;
    if (type == ClipType::SlideShow && QFile::exists(url.adjusted(QUrl::RemoveFilename).toLocalFile())) {
        fixed = true;
    }
    if (fixed || QFile::exists(url.toLocalFile())) {
        m_model->setItemsNewFilePath(index, url.toLocalFile(), DocumentChecker::MissingStatus::Fixed);
        // TODO
        /*if (id == SOURCEMISSING) {
            QDomNodeList producers = m_doc.elementsByTagName(QStringLiteral("producer"));
            QDomNodeList chains = m_doc.elementsByTagName(QStringLiteral("chain"));
            fixMissingSource(item->data(0, idRole).toString(), producers, chains);
        }*/
    } else {
        m_model->setItemsNewFilePath(index, url.toLocalFile(), DocumentChecker::MissingStatus::Missing);
    }
    checkStatus();
}

void DCResolveDialog::slotRecursiveSearch()
{
    /*if (m_checkRunning) {
        m_abortSearch = true;
    } else {
        m_abortSearch = false;
        m_checkRunning = true;*/
    QString clipFolder; // = m_url.adjusted(QUrl::RemoveFilename).toLocalFile();
    const QString newpath = QFileDialog::getExistingDirectory(qApp->activeWindow(), i18nc("@title:window", "Clips Folder"), clipFolder);
    if (newpath.isEmpty()) {
        // m_checkRunning = false;
        return;
    }
    m_model->slotSearchRecursively(newpath);
    //}
    checkStatus();
}

void DCResolveDialog::checkStatus()
{
    bool status = true;
    for (auto item : m_model->getDocumentResources()) {
        if (item.status == DocumentChecker::MissingStatus::Missing) {
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

/*void DCResolveDialog::slotCheckButtons()
{
    if (m_ui.treeWidget->currentItem()) {
        QTreeWidgetItem *item = m_ui.treeWidget->currentItem();
        int t = item->data(0, typeRole).toInt();
        int s = item->data(0, statusRole).toInt();
        if (t == TITLE_FONT_ELEMENT || t == TITLE_IMAGE_ELEMENT || s == PROXYMISSING) {
            m_ui.removeSelected->setEnabled(false);
        } else {
            m_ui.removeSelected->setEnabled(true);
        }
        bool allowEdit = s == CLIPMISSING || s == LUMAMISSING;
        m_ui.manualSearch->setEnabled(allowEdit);
    }
}*/
