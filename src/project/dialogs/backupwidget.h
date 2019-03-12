/***************************************************************************
 *   Copyright (C) 2011 by Jean-Baptiste Mardelle (jb@kdenlive.org)        *
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

#ifndef BACKUPWIDGET_H
#define BACKUPWIDGET_H

#include "ui_backupdialog_ui.h"

#include <QUrl>

/**
 * @class BackupWidget
 * @brief A widget allowing to parse backup project files
 * @author Jean-Baptiste Mardelle
 */

class BackupWidget : public QDialog, public Ui::BackupDialog_UI
{
    Q_OBJECT

public:
    BackupWidget(const QUrl &projectUrl, QUrl projectFolder, const QString &projectId, QWidget *parent = nullptr);
    // Constructor for extracting widget
    ~BackupWidget() override;
    /** @brief Return the path for selected backup file. */
    QString selectedFile() const;

private slots:
    /** @brief Parse the backup files in project folder. */
    void slotParseBackupFiles();
    /** @brief Display a thumbnail preview of selected backup. */
    void slotDisplayBackupPreview();

private:
    QString m_projectWildcard;
    QUrl m_projectFolder;
};

#endif
