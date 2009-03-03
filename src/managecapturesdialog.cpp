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

#include <QTreeWidgetItem>
#include <QFile>
#include <QHeaderView>
#include <QIcon>
#include <QPixmap>
#include <QTimer>

#include <KDebug>
#include <KGlobalSettings>
#include <KFileItem>
#include <KIO/NetAccess>

#include "managecapturesdialog.h"
#include "kthumb.h"


ManageCapturesDialog::ManageCapturesDialog(KUrl::List files, QWidget * parent): QDialog(parent) {
    setFont(KGlobalSettings::toolBarFont());
    m_view.setupUi(this);
    m_importButton = m_view.buttonBox->button(QDialogButtonBox::Ok);
    m_importButton->setText(i18n("import"));
    m_view.treeWidget->setIconSize(QSize(70, 50));
    foreach(const KUrl &url, files) {
        QStringList text;
        text << url.fileName();
        KFileItem file(KFileItem::Unknown, KFileItem::Unknown, url, true);
        text << KIO::convertSize(file.size());
        QTreeWidgetItem *item = new QTreeWidgetItem(m_view.treeWidget, text);
        item->setData(0, Qt::UserRole, url.path());
        item->setToolTip(0, url.path());
        item->setFlags(Qt::ItemIsUserCheckable | Qt::ItemIsSelectable | Qt::ItemIsEnabled);
        item->setCheckState(0, Qt::Checked);
    }
    connect(m_view.treeWidget, SIGNAL(itemChanged(QTreeWidgetItem *, int)), this, SLOT(slotRefreshButtons()));
    connect(m_view.deleteButton, SIGNAL(pressed()), this, SLOT(slotDeleteCurrent()));
    connect(m_view.toggleButton, SIGNAL(pressed()), this, SLOT(slotToggle()));
    QTreeWidgetItem *item = m_view.treeWidget->topLevelItem(0);
    if (item) m_view.treeWidget->setCurrentItem(item);
    connect(m_view.treeWidget, SIGNAL(itemSelectionChanged()), this, SLOT(slotCheckItemIcon()));
    QTimer::singleShot(500, this, SLOT(slotCheckItemIcon()));
    m_view.treeWidget->resizeColumnToContents(0);
    m_view.treeWidget->setEnabled(false);
    adjustSize();
}

ManageCapturesDialog::~ManageCapturesDialog() {}

void ManageCapturesDialog::slotCheckItemIcon() {
    int ct = 0;
    int count = m_view.treeWidget->topLevelItemCount();
    while (ct < count) {
        QTreeWidgetItem *item = m_view.treeWidget->topLevelItem(ct);
        //QTreeWidgetItem *item = m_view.treeWidget->currentItem();
        if (item->icon(0).isNull()) {
            QPixmap p = KThumb::getImage(KUrl(item->data(0, Qt::UserRole).toString()), 0, 70, 50);
            item->setIcon(0, QIcon(p));
            m_view.treeWidget->resizeColumnToContents(0);
            repaint();
            //QTimer::singleShot(400, this, SLOT(slotCheckItemIcon()));
        }
        ct++;
    }
    m_view.treeWidget->setEnabled(true);
}

void ManageCapturesDialog::slotRefreshButtons() {
    int count = m_view.treeWidget->topLevelItemCount();
    bool enabled = false;
    for (int i = 0; i < count; i++) {
        QTreeWidgetItem *item = m_view.treeWidget->topLevelItem(i);
        if (item && item->checkState(0) == Qt::Checked) {
            enabled = true;
            break;
        }
    }
    m_importButton->setEnabled(enabled);
}

void ManageCapturesDialog::slotDeleteCurrent() {
    QTreeWidgetItem *item = m_view.treeWidget->currentItem();
    if (!item) return;
    int i = m_view.treeWidget->indexOfTopLevelItem(item);
    m_view.treeWidget->takeTopLevelItem(i);
    kDebug() << "DELETING FILE: " << item->text(0);
    //KIO::NetAccess::del(KUrl(item->text(0)), this);
    QFile f(item->data(0, Qt::UserRole).toString());
    f.remove();
    delete item;
    item = NULL;
}

void ManageCapturesDialog::slotToggle() {
    int count = m_view.treeWidget->topLevelItemCount();
    for (int i = 0; i < count; i++) {
        QTreeWidgetItem *item = m_view.treeWidget->topLevelItem(i);
        if (item) {
            if (item->checkState(0) == Qt::Checked) item->setCheckState(0, Qt::Unchecked);
            else item->setCheckState(0, Qt::Checked);
        }
    }
}

KUrl::List ManageCapturesDialog::importFiles() {
    KUrl::List result;

    int count = m_view.treeWidget->topLevelItemCount();
    for (int i = 0; i < count; i++) {
        QTreeWidgetItem *item = m_view.treeWidget->topLevelItem(i);
        if (item && item->checkState(0) == Qt::Checked)
            result.append(KUrl(item->data(0, Qt::UserRole).toString()));
    }
    return result;
}

#include "managecapturesdialog.moc"


