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

#ifndef TRACKSCONFIGDIALOG_H
#define TRACKSCONFIGDIALOG_H

#include "ui_tracksconfigdialog_ui.h"

#include <QItemDelegate>

class TracksDelegate : public QItemDelegate
{
    Q_OBJECT
public:
    explicit TracksDelegate(QObject *parent = nullptr);
    QWidget *createEditor(QWidget *parent, const QStyleOptionViewItem &option, const QModelIndex &index) const override;
    void setEditorData(QWidget *editor, const QModelIndex &index) const override;
    void setModelData(QWidget *editor, QAbstractItemModel *model, const QModelIndex &index) const override;
private slots:
    void emitCommitData();
};

class TrackInfo;
class Timeline;
class QTableWidgetItem;

/**
 * @class TracksConfigDialog
 * @brief A dialog to change the name, type, ... of tracks.
 * @author Till Theato
 */

class TracksConfigDialog : public QDialog, public Ui::TracksConfigDialog_UI
{
    Q_OBJECT
public:
    /** @brief Sets up the table.
     * @param doc the kdenlive document whose tracks to use
     * @param selected the track which should be selected by default
     * @param parent the parent widget */
    explicit TracksConfigDialog(Timeline *timeline, int selected = -1, QWidget *parent = nullptr);

    /** @brief Returns the new list of tracks created from the table. */
    const QList<TrackInfo> tracksList();

    /** @brief A list of tracks, which should be deleted. */
    QList<int> deletedTracks() const;

private slots:
    /** @brief Updates the "hidden" checkbox if type was changed. */
    void slotUpdateRow(QTableWidgetItem *item);

private slots:
    /** @brief Recreates the table from the list of tracks in m_doc. */
    void setupOriginal(int selected = -1);

    /** @brief Marks a track to be deleted. */
    void slotDelete();

private:
    Timeline *m_timeline;
    QList<int> m_deletedRows;
};

#endif
