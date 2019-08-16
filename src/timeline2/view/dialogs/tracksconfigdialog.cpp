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
#include "timeline/timeline.h"

#include <KComboBox>
#include <QIcon>
#include <QTableWidget>

#include "klocalizedstring.h"

TracksDelegate::TracksDelegate(QObject *parent)
    : QItemDelegate(parent)
{
}

QWidget *TracksDelegate::createEditor(QWidget *parent, const QStyleOptionViewItem & /* option */, const QModelIndex & /*index*/) const
{
    KComboBox *comboBox = new KComboBox(parent);
    comboBox->addItem(i18n("Video"));
    comboBox->addItem(i18n("Audio"));
    connect(comboBox, SIGNAL(activated(int)), this, SLOT(emitCommitData()));
    return comboBox;
}

void TracksDelegate::setEditorData(QWidget *editor, const QModelIndex &index) const
{
    KComboBox *comboBox = qobject_cast<KComboBox *>(editor);
    if (!comboBox) {
        return;
    }
    const int pos = comboBox->findText(index.model()->data(index).toString(), Qt::MatchExactly);
    comboBox->setCurrentIndex(pos);
}

void TracksDelegate::setModelData(QWidget *editor, QAbstractItemModel *model, const QModelIndex &index) const
{
    KComboBox *comboBox = qobject_cast<KComboBox *>(editor);
    if (!comboBox) {
        return;
    }
    model->setData(index, comboBox->currentText());
}

void TracksDelegate::emitCommitData()
{
    emit commitData(qobject_cast<QWidget *>(sender()));
}

TracksConfigDialog::TracksConfigDialog(Timeline *timeline, int selected, QWidget *parent)
    : QDialog(parent)
    , m_timeline(timeline)
{
    setupUi(this);

    table->setColumnCount(5);
    table->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    table->setHorizontalHeaderLabels(QStringList() << i18n("Name") << i18n("Type") << i18n("Hidden") << i18n("Muted") << i18n("Locked") << i18n("Composite"));
    table->setSelectionMode(QAbstractItemView::SingleSelection);
    table->setItemDelegateForColumn(1, new TracksDelegate(this));
    table->verticalHeader()->setHidden(true);

    buttonReset->setIcon(QIcon::fromTheme(QStringLiteral("document-revert")));
    buttonReset->setToolTip(i18n("Reset"));
    connect(buttonReset, &QAbstractButton::clicked, this, &TracksConfigDialog::setupOriginal);

    buttonAdd->setIcon(QIcon::fromTheme(QStringLiteral("list-add")));
    buttonAdd->setToolTip(i18n("Add Track"));
    buttonAdd->setEnabled(false);

    buttonDelete->setIcon(QIcon::fromTheme(QStringLiteral("list-remove")));
    buttonDelete->setToolTip(i18n("Delete Track"));
    connect(buttonDelete, &QAbstractButton::clicked, this, &TracksConfigDialog::slotDelete);

    buttonUp->setIcon(QIcon::fromTheme(QStringLiteral("arrow-up")));
    buttonUp->setToolTip(i18n("Move Track upwards"));
    buttonUp->setEnabled(false);

    buttonDown->setIcon(QIcon::fromTheme(QStringLiteral("arrow-down")));
    buttonDown->setToolTip(i18n("Move Track downwards"));
    buttonDown->setEnabled(false);

    setupOriginal(selected);
    // table->resizeColumnToContents(0);
    table->resizeColumnsToContents();
    /*QRect rect = table->geometry();
    rect.setWidth(table->horizontalHeader()->length());
    table->setGeometry(rect);*/
    table->horizontalHeader()->setStretchLastSection(true);
    table->setMinimumSize(table->horizontalHeader()->length(), table->verticalHeader()->length() + table->horizontalHeader()->height() * 2);
    connect(table, &QTableWidget::itemChanged, this, &TracksConfigDialog::slotUpdateRow);
}

const QList<TrackInfo> TracksConfigDialog::tracksList()
{
    QList<TrackInfo> tracks;
    TrackInfo info;
    tracks.reserve(table->rowCount());
    for (int i = 0; i < table->rowCount(); i++) {
        info.trackName = table->item(i, 0)->text();
        QTableWidgetItem *item = table->item(i, 1);
        if (item->text() == i18n("Audio")) {
            info.type = AudioTrack;
            info.isBlind = true;
        } else {
            info.type = VideoTrack;
            info.isBlind = (table->item(i, 2)->checkState() == Qt::Checked);
        }
        info.isMute = (table->item(i, 3)->checkState() == Qt::Checked);
        info.isLocked = (table->item(i, 4)->checkState() == Qt::Checked);
        tracks << info;
    }
    return tracks;
}

QList<int> TracksConfigDialog::deletedTracks() const
{
    return m_deletedRows;
}

void TracksConfigDialog::setupOriginal(int selected)
{
    int max = m_timeline->visibleTracksCount();
    table->setRowCount(max);

    QStringList numbers;
    TrackInfo info;
    for (int i = 0; i < max; i++) {
        numbers << QString::number(i);
        info = m_timeline->getTrackInfo(max - i);
        table->setItem(i, 0, new QTableWidgetItem(info.trackName));

        QTableWidgetItem *item1 = new QTableWidgetItem(i18n("Video"));
        if (info.type == AudioTrack) {
            item1->setText(i18n("Audio"));
        }
        table->setItem(i, 1, item1);
        table->openPersistentEditor(item1);

        QTableWidgetItem *item2 = new QTableWidgetItem(QString());
        item2->setFlags(item2->flags() & ~Qt::ItemIsEditable);
        item2->setCheckState(info.isBlind ? Qt::Checked : Qt::Unchecked);
        if (info.type == AudioTrack) {
            item2->setFlags(item2->flags() & ~Qt::ItemIsEnabled);
        }
        table->setItem(i, 2, item2);

        QTableWidgetItem *item3 = new QTableWidgetItem(QString());
        item3->setFlags(item3->flags() & ~Qt::ItemIsEditable);
        item3->setCheckState(info.isMute ? Qt::Checked : Qt::Unchecked);
        table->setItem(i, 3, item3);

        QTableWidgetItem *item4 = new QTableWidgetItem(QString());
        item4->setFlags(item4->flags() & ~Qt::ItemIsEditable);
        item4->setCheckState(info.isLocked ? Qt::Checked : Qt::Unchecked);
        table->setItem(i, 4, item4);
    }
    table->setVerticalHeaderLabels(numbers);

    table->resizeColumnsToContents();
    if (selected != -1) {
        table->selectRow(max - selected);
    }

    m_deletedRows.clear();
}

void TracksConfigDialog::slotUpdateRow(QTableWidgetItem *item)
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

void TracksConfigDialog::slotDelete()
{
    int row = table->currentRow();
    int trackToDelete = table->rowCount() - row;
    if (row < 0 || m_deletedRows.contains(trackToDelete)) {
        return;
    }
    m_deletedRows.append(trackToDelete);
    std::sort(m_deletedRows.begin(), m_deletedRows.end());
    for (int i = 0; i < table->columnCount(); ++i) {
        QTableWidgetItem *item = table->item(row, i);
        item->setFlags(Qt::NoItemFlags);
        item->setBackground(palette().dark());
    }
}
