/***************************************************************************
 *   Copyright (C) 2008 by Jean-Baptiste Mardelle (jb@kdenlive.org)        *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA          *
 ***************************************************************************/

#include "managecapturesdialog.h"
#include "doc/kthumb.h"

#include "kdenlive_debug.h"
#include "klocalizedstring.h"
#include <KFileItem>
#include <QFontDatabase>

#include <QFile>
#include <QIcon>
#include <QPixmap>
#include <QTimer>
#include <QTreeWidgetItem>

ManageCapturesDialog::ManageCapturesDialog(const QList<QUrl> &files, QWidget *parent)
    : QDialog(parent)
{
    setFont(QFontDatabase::systemFont(QFontDatabase::SmallestReadableFont));
    m_view.setupUi(this);
    m_importButton = m_view.buttonBox->button(QDialogButtonBox::Ok);
    m_importButton->setText(i18n("Import"));
    m_view.treeWidget->setIconSize(QSize(70, 50));
    for (const QUrl &url : files) {
        QStringList text;
        text << url.fileName();
        KFileItem file(url);
        file.setDelayedMimeTypes(true);
        text << KIO::convertSize(file.size());
        auto *item = new QTreeWidgetItem(m_view.treeWidget, text);
        item->setData(0, Qt::UserRole, url.toLocalFile());
        item->setToolTip(0, url.toLocalFile());
        item->setFlags(Qt::ItemIsUserCheckable | Qt::ItemIsSelectable | Qt::ItemIsEnabled);
        item->setCheckState(0, Qt::Checked);
    }
    connect(m_view.treeWidget, &QTreeWidget::itemChanged, this, &ManageCapturesDialog::slotRefreshButtons);
    connect(m_view.deleteButton, &QAbstractButton::pressed, this, &ManageCapturesDialog::slotDeleteCurrent);
    connect(m_view.toggleButton, &QAbstractButton::pressed, this, &ManageCapturesDialog::slotToggle);
    QTreeWidgetItem *item = m_view.treeWidget->topLevelItem(0);
    if (item) {
        m_view.treeWidget->setCurrentItem(item);
    }
    connect(m_view.treeWidget, &QTreeWidget::itemSelectionChanged, this, &ManageCapturesDialog::slotCheckItemIcon);
    QTimer::singleShot(500, this, &ManageCapturesDialog::slotCheckItemIcon);
    m_view.treeWidget->resizeColumnToContents(0);
    m_view.treeWidget->setEnabled(false);
    adjustSize();
}

ManageCapturesDialog::~ManageCapturesDialog() = default;

void ManageCapturesDialog::slotCheckItemIcon()
{
    int ct = 0;
    const int count = m_view.treeWidget->topLevelItemCount();
    while (ct < count) {
        QTreeWidgetItem *item = m_view.treeWidget->topLevelItem(ct);
        // QTreeWidgetItem *item = m_view.treeWidget->currentItem();
        if (item->icon(0).isNull()) {
            QPixmap p = KThumb::getImage(QUrl(item->data(0, Qt::UserRole).toString()), 0, 70, 50);
            item->setIcon(0, QIcon(p));
            m_view.treeWidget->resizeColumnToContents(0);
            repaint();
            // QTimer::singleShot(400, this, SLOT(slotCheckItemIcon()));
        }
        ct++;
    }
    m_view.treeWidget->setEnabled(true);
}

void ManageCapturesDialog::slotRefreshButtons()
{
    const int count = m_view.treeWidget->topLevelItemCount();
    bool enabled = false;
    for (int i = 0; i < count; ++i) {
        QTreeWidgetItem *item = m_view.treeWidget->topLevelItem(i);
        if ((item != nullptr) && item->checkState(0) == Qt::Checked) {
            enabled = true;
            break;
        }
    }
    m_importButton->setEnabled(enabled);
}

void ManageCapturesDialog::slotDeleteCurrent()
{
    QTreeWidgetItem *item = m_view.treeWidget->currentItem();
    if (!item) {
        return;
    }
    const int i = m_view.treeWidget->indexOfTopLevelItem(item);
    m_view.treeWidget->takeTopLevelItem(i);
    // qCDebug(KDENLIVE_LOG) << "DELETING FILE: " << item->text(0);
    // KIO::NetAccess::del(QUrl(item->text(0)), this);
    if (!QFile::remove(item->data(0, Qt::UserRole).toString())) {
        qCDebug(KDENLIVE_LOG) << "// ERRor removing file " << item->data(0, Qt::UserRole).toString();
    }
    delete item;
    item = nullptr;
}

void ManageCapturesDialog::slotToggle()
{
    const int count = m_view.treeWidget->topLevelItemCount();
    for (int i = 0; i < count; ++i) {
        QTreeWidgetItem *item = m_view.treeWidget->topLevelItem(i);
        if (item) {
            if (item->checkState(0) == Qt::Checked) {
                item->setCheckState(0, Qt::Unchecked);
            } else {
                item->setCheckState(0, Qt::Checked);
            }
        }
    }
}

QList<QUrl> ManageCapturesDialog::importFiles() const
{
    QList<QUrl> result;

    const int count = m_view.treeWidget->topLevelItemCount();
    for (int i = 0; i < count; ++i) {
        QTreeWidgetItem *item = m_view.treeWidget->topLevelItem(i);
        if ((item != nullptr) && item->checkState(0) == Qt::Checked) {
            result.append(QUrl(item->data(0, Qt::UserRole).toString()));
        }
    }
    return result;
}
