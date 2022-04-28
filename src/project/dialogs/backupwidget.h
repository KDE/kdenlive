/*
    SPDX-FileCopyrightText: 2011 Jean-Baptiste Mardelle <jb@kdenlive.org>

SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include "ui_backupdialog_ui.h"

#include <QUrl>

/** @class BackupWidget
    @brief A widget allowing to parse backup project files
    @author Jean-Baptiste Mardelle
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
