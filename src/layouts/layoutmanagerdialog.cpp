/*
SPDX-FileCopyrightText: 2012 Jean-Baptiste Mardelle <jb@kdenlive.org>
SPDX-FileCopyrightText: 2014 Till Theato <root@ttill.de>
SPDX-FileCopyrightText: 2020 Julius Künzel <julius.kuenzel@kde.org>
SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "layouts/layoutmanagerdialog.h"
#include "layouts/layoutcollection.h"
#include "layouts/layoutinfo.h"
#include <KLocalizedString>
#include <KMessageBox>
#include <KStandardGuiItem>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QInputDialog>
#include <QMessageBox>
#include <QUrl>

LayoutManagerDialog::LayoutManagerDialog(QWidget *parent, LayoutCollection layouts, const QString &currentLayoutId)
    : QDialog(parent)
    , m_previewCollection(std::move(layouts))
{
    setupUi(m_previewCollection, currentLayoutId);
}

void LayoutManagerDialog::setupUi(const LayoutCollection &layouts, const QString &currentLayoutId)
{
    setWindowTitle(i18n("Manage Layouts"));
    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->addWidget(new QLabel(i18n("Current Layouts"), this));

    m_listWidget = new QListWidget(this);
    m_listWidget->setAlternatingRowColors(true);
    mainLayout->addWidget(m_listWidget);

    // Add layouts to list
    QList<LayoutInfo> allLayouts = layouts.getAllLayouts();
    for (const LayoutInfo &layout : allLayouts) {
        addLayoutItem(layout);
    }
    // Select current
    int ix = 0;
    for (int i = 0; i < m_listWidget->count(); ++i) {
        if (m_listWidget->item(i)->data(Qt::UserRole).toString() == currentLayoutId) {
            ix = i;
            break;
        }
    }
    m_listWidget->setCurrentRow(ix);

    // Buttons
    auto *buttonLayout = new QHBoxLayout;
    m_deleteButton = new QToolButton(this);
    m_deleteButton->setIcon(QIcon::fromTheme(QStringLiteral("edit-delete")));
    m_deleteButton->setAutoRaise(true);
    m_deleteButton->setToolTip(i18n("Delete the layout."));
    buttonLayout->addWidget(m_deleteButton);

    m_upButton = new QToolButton(this);
    m_upButton->setIcon(QIcon::fromTheme(QStringLiteral("go-up")));
    m_upButton->setAutoRaise(true);
    buttonLayout->addWidget(m_upButton);

    m_downButton = new QToolButton(this);
    m_downButton->setIcon(QIcon::fromTheme(QStringLiteral("go-down")));
    m_downButton->setAutoRaise(true);
    buttonLayout->addWidget(m_downButton);

    m_resetButton = new QToolButton(this);
    m_resetButton->setIcon(QIcon::fromTheme(QStringLiteral("view-refresh")));
    m_resetButton->setAutoRaise(true);
    m_resetButton->setToolTip(i18n("Reset"));
    buttonLayout->addWidget(m_resetButton);

    m_importButton = new QToolButton(this);
    m_importButton->setIcon(QIcon::fromTheme(QStringLiteral("document-import")));
    m_importButton->setAutoRaise(true);
    m_importButton->setToolTip(i18n("Import"));
    buttonLayout->addWidget(m_importButton);

    m_exportButton = new QToolButton(this);
    m_exportButton->setIcon(QIcon::fromTheme(QStringLiteral("document-export")));
    m_exportButton->setAutoRaise(true);
    m_exportButton->setToolTip(i18n("Export selected"));
    buttonLayout->addWidget(m_exportButton);

    buttonLayout->addStretch();
    mainLayout->addLayout(buttonLayout);

    m_buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    mainLayout->addWidget(m_buttonBox);

    // Connections
    connect(m_buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(m_buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(m_deleteButton, &QToolButton::clicked, this, &LayoutManagerDialog::deleteCurrentItem);
    connect(m_upButton, &QToolButton::clicked, this, &LayoutManagerDialog::moveItemUp);
    connect(m_downButton, &QToolButton::clicked, this, &LayoutManagerDialog::moveItemDown);
    connect(m_listWidget, &QListWidget::currentRowChanged, this, &LayoutManagerDialog::updateButtonStates);
    connect(m_resetButton, &QToolButton::clicked, this, &LayoutManagerDialog::resetToDefaults);
    connect(m_importButton, &QToolButton::clicked, this, &LayoutManagerDialog::importLayout);
    connect(m_exportButton, &QToolButton::clicked, this, &LayoutManagerDialog::exportLayout);
    updateButtonStates();
}

void LayoutManagerDialog::deleteCurrentItem()
{
    if (m_listWidget->currentItem()) {
        delete m_listWidget->currentItem();
    }
    updateButtonStates();
}

void LayoutManagerDialog::moveItemUp()
{
    int row = m_listWidget->currentRow();
    if (row > 0) {
        QListWidgetItem *item = m_listWidget->takeItem(row);
        m_listWidget->insertItem(row - 1, item);
        m_listWidget->setCurrentRow(row - 1);
    }
    updateButtonStates();
}

void LayoutManagerDialog::moveItemDown()
{
    int row = m_listWidget->currentRow();
    if (row < m_listWidget->count() - 1) {
        QListWidgetItem *item = m_listWidget->takeItem(row);
        m_listWidget->insertItem(row + 1, item);
        m_listWidget->setCurrentRow(row + 1);
    }
    updateButtonStates();
}

void LayoutManagerDialog::resetToDefaults()
{
    if (QMessageBox::question(this, i18n("Reset Layouts"),
                              i18n("Are you sure you want to reset all layouts to the default set? This will remove all custom layouts.")) ==
        QMessageBox::Yes) {
        m_listWidget->clear();
        LayoutCollection defaults = LayoutCollection::getDefaultLayouts();
        for (const LayoutInfo &layout : defaults.getAllLayouts()) {
            addLayoutItem(layout);
        }
        updateButtonStates();
    }
}

void LayoutManagerDialog::importLayout()
{
    QString fileName = QFileDialog::getOpenFileName(this, i18n("Import Layout"), QString(), i18n("Kdenlive Layout (*.kdenlivelayout)"));
    if (fileName.isEmpty()) return;

    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QMessageBox::critical(this, i18n("Import Layout"), i18n("Cannot open file %1", QUrl::fromLocalFile(fileName).fileName()));
        return;
    }

    QString state = QString::fromUtf8(file.readAll());
    file.close();

    QFileInfo fileInfo(fileName);
    QString suggestedName = fileInfo.baseName();
    QString layoutName = QInputDialog::getText(this, i18n("Import Layout"), i18n("Layout name:"), QLineEdit::Normal, suggestedName);
    if (layoutName.isEmpty()) return;

    // Check for duplicate
    for (int i = 0; i < m_listWidget->count(); ++i) {
        if (m_listWidget->item(i)->text() == layoutName) {
            int res = KMessageBox::questionTwoActions(this, i18n("The layout %1 already exists. Do you want to replace it?", layoutName), {},
                                                      KStandardGuiItem::overwrite(), KStandardGuiItem::cancel());
            if (res != KMessageBox::PrimaryAction) {
                return;
            } else {
                delete m_listWidget->item(i);
                break;
            }
        }
    }

    // Add to list
    auto *item = new QListWidgetItem(layoutName, m_listWidget);
    item->setData(Qt::UserRole, layoutName); // Use name as ID for imported
    item->setFlags(Qt::ItemIsEditable | Qt::ItemIsEnabled | Qt::ItemIsSelectable);

    // Store the layout data in m_previewCollection for export
    LayoutInfo info(layoutName, layoutName, state);
    m_previewCollection.addLayout(info);
    m_listWidget->setCurrentItem(item);
    updateButtonStates();
}

void LayoutManagerDialog::exportLayout()
{
    if (!m_listWidget->currentItem()) {
        QMessageBox::warning(this, i18n("Export Layout"), i18n("No layout selected."));
        return;
    }

    QString layoutId = m_listWidget->currentItem()->data(Qt::UserRole).toString();
    LayoutInfo info = m_previewCollection.getLayout(layoutId);
    if (!info.isValid() || info.data.isEmpty()) {
        QMessageBox::warning(this, i18n("Export Layout"), i18n("Cannot find layout data for %1.", layoutId));
        return;
    }

    QString fileName = QFileDialog::getSaveFileName(this, i18n("Export Layout"), layoutId + ".kdenlivelayout", i18n("Kdenlive Layout (*.kdenlivelayout)"));
    if (fileName.isEmpty()) return;

    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::critical(this, i18n("Export Layout"), i18n("Cannot open file %1", QUrl::fromLocalFile(fileName).fileName()));
        return;
    }

    file.write(info.data.toUtf8());
    file.close();
    QMessageBox::information(this, i18n("Export Layout"), i18n("Layout exported successfully."));
}

void LayoutManagerDialog::addLayoutItem(const LayoutInfo &layout)
{
    auto *item = new QListWidgetItem(layout.displayName, m_listWidget);
    item->setData(Qt::UserRole, layout.internalId);
    if (!layout.isKDDockWidgetsLayout()) {
        item->setIcon(QIcon::fromTheme("dialog-warning"));
        item->setToolTip(i18n("This layout uses an old and unsupported format, should be removed and recreated"));
    }
    item->setFlags(Qt::ItemIsEditable | Qt::ItemIsEnabled | Qt::ItemIsSelectable);
}

void LayoutManagerDialog::updateButtonStates()
{
    int row = m_listWidget->currentRow();
    m_upButton->setEnabled(row > 0);
    m_downButton->setEnabled(row >= 0 && row < m_listWidget->count() - 1);
    m_deleteButton->setEnabled(row >= 0);
    m_exportButton->setEnabled(row >= 0);
}

LayoutCollection LayoutManagerDialog::getUpdatedCollection() const
{
    LayoutCollection updated = m_previewCollection;
    QStringList newOrder;
    for (int i = 0; i < m_listWidget->count(); ++i) {
        QListWidgetItem *item = m_listWidget->item(i);
        QString layoutId = item->data(Qt::UserRole).toString();
        QString displayName = item->text();
        LayoutInfo info = updated.getLayout(layoutId);
        if (info.isValid()) {
            if (info.displayName != displayName) {
                info.displayName = displayName;
                updated.addLayout(info);
            }
        } else {
            updated.addLayout(LayoutInfo(layoutId, displayName));
        }
        newOrder.append(layoutId);
    }
    updated.reorderLayouts(newOrder);
    return updated;
}

QString LayoutManagerDialog::getSelectedLayoutId() const
{
    if (m_listWidget->currentItem()) {
        return m_listWidget->currentItem()->data(Qt::UserRole).toString();
    }
    return QString();
}
