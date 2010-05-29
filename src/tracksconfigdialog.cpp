/***************************************************************************
 *   Copyright (C) 2010 by Till Theato (root@ttill.de)                     *
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

#include "tracksconfigdialog.h"
#include "kdenlivedoc.h"

#include <QTableWidget>
#include <QComboBox>
#include <KDebug>

TracksDelegate::TracksDelegate(QObject *parent) :
        QItemDelegate(parent)
{
}

QWidget *TracksDelegate::createEditor(QWidget *parent, const QStyleOptionViewItem & /* option */, const QModelIndex & /*index*/) const
{
    QComboBox *comboBox = new QComboBox(parent);
    comboBox->addItem(i18n("Video"));
    comboBox->addItem(i18n("Audio"));
    connect(comboBox, SIGNAL(activated(int)), this, SLOT(emitCommitData()));
    return comboBox;
}

void TracksDelegate::setEditorData(QWidget *editor, const QModelIndex &index) const
{
    QComboBox *comboBox = qobject_cast<QComboBox *>(editor);
    if (!comboBox)
        return;
    int pos = comboBox->findText(index.model()->data(index).toString(), Qt::MatchExactly);
    comboBox->setCurrentIndex(pos);
}

void TracksDelegate::setModelData(QWidget *editor, QAbstractItemModel *model, const QModelIndex &index) const
{
    QComboBox *comboBox = qobject_cast<QComboBox *>(editor);
    if (!comboBox)
        return;
    model->setData(index, comboBox->currentText());
}

void TracksDelegate::emitCommitData()
{
    emit commitData(qobject_cast<QWidget *>(sender()));
}


TracksConfigDialog::TracksConfigDialog(KdenliveDoc * doc, int selected, QWidget* parent) :
        QDialog(parent),
        m_doc(doc)
{
    setupUi(this);

    table->setColumnCount(5);
    table->setHorizontalHeaderLabels(QStringList() << i18n("Name") << i18n("Type") << i18n("Hidden") << i18n("Muted") << i18n("Locked"));
    table->horizontalHeader()->setResizeMode(0, QHeaderView::Stretch);
    table->horizontalHeader()->setResizeMode(1, QHeaderView::Fixed);
    table->horizontalHeader()->setResizeMode(2, QHeaderView::Fixed);
    table->horizontalHeader()->setResizeMode(3, QHeaderView::Fixed);
    table->horizontalHeader()->setResizeMode(4, QHeaderView::Fixed);

    table->setItemDelegateForColumn(1, new TracksDelegate(this));

    setupOriginal(selected);
    connect(table, SIGNAL(itemChanged(QTableWidgetItem *)), this, SLOT(slotUpdateRow(QTableWidgetItem *)));
}

const QList <TrackInfo> TracksConfigDialog::tracksList()
{
    QList <TrackInfo> tracks;
    TrackInfo info;
    for (int i = table->rowCount() - 1; i >= 0; i--) {
        info.trackName = table->item(i, 0)->text();
        QTableWidgetItem *item = table->item(i, 1);
        if (item->text() == i18n("Audio")) {
            info.type = AUDIOTRACK;
            info.isBlind = true;
        } else {
            info.type = VIDEOTRACK;
            info.isBlind = (table->item(i, 2)->checkState() == Qt::Checked);
        }
        info.isMute = (table->item(i, 3)->checkState() == Qt::Checked);
        info.isLocked = (table->item(i, 4)->checkState() == Qt::Checked);
        tracks << info;
    }
    return tracks;
}

void TracksConfigDialog::setupOriginal(int selected)
{
    table->setRowCount(m_doc->tracksCount());

    QStringList numbers;
    TrackInfo info;
    for (int i = m_doc->tracksCount() - 1; i >= 0; i--) {
        numbers << QString::number(i);
        info = m_doc->trackInfoAt(m_doc->tracksCount() - i - 1);
        table->setItem(i, 0, new QTableWidgetItem(info.trackName));

        QTableWidgetItem *item1 = new QTableWidgetItem(i18n("Video"));
        if (info.type == AUDIOTRACK)
            item1->setText(i18n("Audio"));
        table->setItem(i, 1, item1);
        table->openPersistentEditor(item1);

        QTableWidgetItem *item2 = new QTableWidgetItem("");
        item2->setFlags(item2->flags() & ~Qt::ItemIsEditable);
        item2->setCheckState(info.isBlind ? Qt::Checked : Qt::Unchecked);
        if (info.type == AUDIOTRACK)
            item2->setFlags(item2->flags() & ~Qt::ItemIsEnabled);
        table->setItem(i, 2, item2);

        QTableWidgetItem *item3 = new QTableWidgetItem("");
        item3->setFlags(item3->flags() & ~Qt::ItemIsEditable);
        item3->setCheckState(info.isMute ? Qt::Checked : Qt::Unchecked);
        table->setItem(i, 3, item3);

        QTableWidgetItem *item4 = new QTableWidgetItem("");
        item4->setFlags(item4->flags() & ~Qt::ItemIsEditable);
        item4->setCheckState(info.isLocked ? Qt::Checked : Qt::Unchecked);
        table->setItem(i, 4, item4);
    }
    table->setVerticalHeaderLabels(numbers);

    table->resizeColumnsToContents();
    if (selected != -1)
        table->selectRow(selected);
}

void TracksConfigDialog::slotUpdateRow(QTableWidgetItem* item)
{
    if (table->column(item) == 1) {
        QTableWidgetItem *item2 = table->item(table->row(item), 2);
        if (item->text() == i18n("Audio")) {
            item2->setFlags(item2->flags() & ~Qt::ItemIsEnabled);
            item2->setCheckState(Qt::Checked);
        } else {
            item2->setFlags(item2->flags() | Qt::ItemIsEnabled);
            item2->setCheckState(Qt::Unchecked);
        }
    }
}

#include "tracksconfigdialog.moc"
