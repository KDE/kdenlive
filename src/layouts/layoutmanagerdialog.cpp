/*
SPDX-FileCopyrightText: 2012 Jean-Baptiste Mardelle <jb@kdenlive.org>
SPDX-FileCopyrightText: 2014 Till Theato <root@ttill.de>
SPDX-FileCopyrightText: 2020 Julius Künzel <julius.kuenzel@kde.org>
SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "layouts/layoutmanagerdialog.h"
#include "kdenlivesettings.h"
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
#include <QPainter>
#include <QUrl>

LayoutManagerDialog::LayoutManagerDialog(QWidget *parent, LayoutCollection &layouts, const QString &currentLayoutId)
    : QDialog(parent)
    , m_previewCollection(layouts)
{
    setupUi(currentLayoutId);
}

void LayoutManagerDialog::setupUi(const QString &currentLayoutId)
{
    setWindowTitle(i18n("Manage Layouts"));
    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->addWidget(new QLabel(i18n("Current Layouts"), this));

    m_listWidget = new QListWidget(this);
    m_listWidget->setAlternatingRowColors(true);
    mainLayout->addWidget(m_listWidget);

    int iconSize = style()->pixelMetric(QStyle::PM_SmallIconSize);
    qDebug() << "::: PAINTING ITEMS WITH ICON SIZE: " << iconSize;
    QIcon horizontalIcon = QIcon::fromTheme(QStringLiteral("video-television"));
    QIcon verticalIcon = QIcon::fromTheme(QStringLiteral("smartphone"));
    QPixmap pix(iconSize * 2, iconSize);
    pix.fill(Qt::transparent);
    QPainter p(&pix);
    horizontalIcon.paint(&p, QRect(0, 0, iconSize, iconSize));
    p.end();
    m_horizontalProfile = QIcon(pix);
    p.begin(&pix);
    verticalIcon.paint(&p, QRect(iconSize, 0, iconSize, iconSize));
    p.end();
    m_horizontalAndVerticalProfile = QIcon(pix);
    pix.fill(Qt::transparent);
    p.begin(&pix);
    verticalIcon.paint(&p, QRect(iconSize, 0, iconSize, iconSize));
    p.end();
    m_verticalProfile = QIcon(pix);

    m_listWidget->setIconSize(QSize(2 * iconSize, iconSize));

    // Add layouts to list
    QList<LayoutInfo> allLayouts = m_previewCollection.getAllLayouts();
    for (const LayoutInfo &layout : std::as_const(allLayouts)) {
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

    m_buttonBox = new QDialogButtonBox(QDialogButtonBox::Close, this);
    mainLayout->addWidget(m_buttonBox);

    // Connections
    connect(m_buttonBox, &QDialogButtonBox::rejected, this, &QDialog::accept);
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
        if (QMessageBox::question(this, i18n("Delete Layout"), i18n("Are you sure you want to delete the layout %1?", m_listWidget->currentItem()->text())) !=
            QMessageBox::Yes) {
            return;
        }
        const QString layoutId = m_listWidget->currentItem()->data(Qt::UserRole).toString();
        LayoutInfo info = m_previewCollection.getLayout(layoutId);
        QDir layoutsFolder(QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation));
        if (layoutsFolder.cd(QStringLiteral("layouts"))) {
            // Process deletion
            QFileInfo path(info.path);
            if (layoutsFolder.exists(path.fileName())) {
                QFile::remove(layoutsFolder.absoluteFilePath(path.fileName()));
            }
            QFileInfo vPath(info.verticalPath);
            if (layoutsFolder.exists(vPath.fileName())) {
                QFile::remove(layoutsFolder.absoluteFilePath(vPath.fileName()));
            }
        }
        if (info.isDefault) {
            // We never delete default layouts, just set empty paths
            info.path.clear();
            info.verticalPath.clear();
            m_previewCollection.addLayout(info);
        } else {
            m_previewCollection.removeLayout(layoutId);
        }
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
        m_previewCollection.resetDefaultLayouts();
        // Reset and reload list
        m_listWidget->clear();
        // Add layouts to list
        QList<LayoutInfo> allLayouts = m_previewCollection.getAllLayouts();
        for (const LayoutInfo &layout : std::as_const(allLayouts)) {
            addLayoutItem(layout);
        }
        updateButtonStates();
    }
}

void LayoutManagerDialog::importLayout()
{
    QString fileName = QFileDialog::getOpenFileName(this, i18n("Import Layout"), QString(), i18n("Kdenlive Layout (*.json)"));
    if (fileName.isEmpty()) return;

    QFile srcFile(fileName);
    if (!srcFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QMessageBox::critical(this, i18n("Import Layout"), i18n("Cannot open file %1", QUrl::fromLocalFile(fileName).fileName()));
        return;
    }

    srcFile.close();

    QFileInfo fileInfo(fileName);
    QString suggestedName = fileInfo.baseName();
    bool isVertical = suggestedName.contains(QStringLiteral("_vertical"));
    if (isVertical) {
        suggestedName.replace(QStringLiteral("_vertical"), QString());
    }
    QString layoutName = QInputDialog::getText(this, i18n("Import Layout"), i18n("Layout name:"), QLineEdit::Normal, suggestedName);
    if (layoutName.isEmpty()) return;

    // Copy file to Layout folder
    QDir layoutsFolder(QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation));
    layoutsFolder.mkdir(QStringLiteral("layouts"));
    if (!layoutsFolder.cd(QStringLiteral("layouts"))) {
        KMessageBox::information(this, i18n("Could not write in the layouts folder %1", layoutsFolder.absolutePath()));
        return;
    }
    if (fileInfo.dir() == layoutsFolder) {
        KMessageBox::information(this, i18n("Selected layout is already imported"));
        return;
    }
    QString newFile = layoutsFolder.absoluteFilePath(layoutName + QStringLiteral(".json"));
    // Sanity check
    QFileInfo destFile(newFile);
    if (destFile.dir() != layoutsFolder) {
        KMessageBox::information(this, i18n("Invalid file selected: %1", newFile));
        return;
    }
    if (destFile.exists()) {
        int res = KMessageBox::questionTwoActions(this, i18n("The layout %1 already exists. Do you want to replace it?", layoutName), {},
                                                  KStandardGuiItem::overwrite(), KStandardGuiItem::cancel());
        if (res != KMessageBox::PrimaryAction) {
            return;
        }
        QFile::remove(newFile);
    }
    srcFile.copy(newFile);

    // Add to list
    auto *item = new QListWidgetItem(layoutName, m_listWidget);
    item->setData(Qt::UserRole, layoutName); // Use name as ID for imported
    item->setFlags(Qt::ItemIsEditable | Qt::ItemIsEnabled | Qt::ItemIsSelectable);

    // Store the layout data in m_previewCollection for export
    LayoutInfo info(layoutName, layoutName);
    if (isVertical) {
        info.verticalPath = newFile;
    } else {
        info.path = newFile;
    }
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

    const QString layoutId = m_listWidget->currentItem()->data(Qt::UserRole).toString();
    LayoutInfo layout = m_previewCollection.getLayout(layoutId);
    if (!layout.isValid()) {
        QMessageBox::warning(this, i18n("Export Layout"), i18n("Cannot find layout file for %1.", layoutId));
        return;
    }
    QString fileName = QFileDialog::getSaveFileName(this, i18n("Export Layout"), layoutId + ".json", i18n("Kdenlive Layout (*.json)"));
    if (fileName.isEmpty()) return;

    bool result = false;
    if (QFile::exists(fileName)) {
        QFile::remove(fileName);
    }
    // First export the horizontal layout
    if (!layout.path.isEmpty()) {
        QFile src(layout.path);
        result = src.copy(fileName);
    }
    // Next export the vertical layout if any
    QFileInfo info(fileName);
    QDir folder = info.dir();
    QString baseName = info.baseName();
    fileName = folder.absoluteFilePath(baseName + QStringLiteral("_vertical.json"));
    if (QFile::exists(fileName)) {
        if (KMessageBox::warningContinueCancel(this, i18n("Overwrite existing file for vertical layout %1?", fileName)) != KMessageBox::Continue) {
            if (result) {
                QMessageBox::information(this, i18n("Export Layout"), i18n("Horizontal layout exported successfully."));
            }
            return;
        }
        QFile::remove(fileName);
    }
    if (!layout.verticalPath.isEmpty()) {
        QFile src(layout.verticalPath);
        result = src.copy(fileName);
    }
    if (!result) {
        QMessageBox::critical(this, i18n("Export Layout"), i18n("Cannot write to file %1", QUrl::fromLocalFile(fileName).fileName()));
        return;
    }
    QMessageBox::information(this, i18n("Export Layout"), i18n("Layout exported successfully."));
}

void LayoutManagerDialog::addLayoutItem(const LayoutInfo &layout)
{
    if (!layout.isValid()) {
        // Don't show items with no path
        return;
    }
    QIcon icon;
    QString toolTip;
    if (layout.hasHorizontalData()) {
        if (layout.hasVerticalData()) {
            icon = m_horizontalAndVerticalProfile;
            toolTip = i18nc("@info:tooltip", "This layout contains horizontal and vertical profiles");
        } else {
            icon = m_horizontalProfile;
            toolTip = i18nc("@info:tooltip", "This layout contains an horizontal profile");
        }
    } else {
        icon = m_verticalProfile;
        toolTip = i18nc("@info:tooltip", "This layout contains an vertical profile");
    }
    auto *item = new QListWidgetItem(icon, layout.displayName, m_listWidget);
    item->setData(Qt::UserRole, layout.internalId);
    item->setToolTip(toolTip);
    /*if (!layout.isKDDockWidgetsLayout()) {
        item->setIcon(QIcon::fromTheme("dialog-warning"));
        item->setToolTip(i18n("This layout uses an old and unsupported format, should be removed and recreated"));
    }*/
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

void LayoutManagerDialog::updateOrder()
{
    QStringList newOrder;
    for (int i = 0; i < m_listWidget->count(); ++i) {
        QListWidgetItem *item = m_listWidget->item(i);
        QString layoutId = item->data(Qt::UserRole).toString();
        QString displayName = item->text();
        LayoutInfo info = m_previewCollection.getLayout(layoutId);
        if (info.isValid()) {
            if (info.displayName != displayName) {
                info.displayName = displayName;
                m_previewCollection.addLayout(info);
            }
        }
        newOrder.append(layoutId);
    }
    qDebug() << "::: UPDATING LAYOUTS ORDER TO: " << newOrder;
    KdenliveSettings::setLayoutsOrder(newOrder);
}

QString LayoutManagerDialog::getSelectedLayoutId() const
{
    if (m_listWidget->currentItem()) {
        return m_listWidget->currentItem()->data(Qt::UserRole).toString();
    }
    return QString();
}
