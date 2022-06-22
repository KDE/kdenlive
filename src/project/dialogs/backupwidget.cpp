/*
    SPDX-FileCopyrightText: 2011 Jean-Baptiste Mardelle <jb@kdenlive.org>

SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "backupwidget.h"
#include "kdenlivesettings.h"

#include <QDir>
#include <QPushButton>

BackupWidget::BackupWidget(const QUrl &projectUrl, QUrl projectFolder, const QString &projectId, QWidget *parent)
    : QDialog(parent)
    , m_projectFolder(std::move(projectFolder))
{
    setupUi(this);
    setWindowTitle(i18nc("@title:window", "Restore Backup File"));

    if (!projectUrl.isValid()) {
        // No url, means we opened the backup dialog from an empty project
        info_label->setText(i18n("Showing all backup files in folder"));
        m_projectWildcard = QLatin1Char('*');
    } else {
        info_label->setText(i18n("Showing backup files for %1", projectUrl.fileName()));
        m_projectWildcard = projectUrl.fileName().section(QLatin1Char('.'), 0, -2);
        if (!projectId.isEmpty()) {
            m_projectWildcard.append(QLatin1Char('-') + projectId);
        } else {
            // No project id, it was lost, add wildcard
            m_projectWildcard.append(QLatin1Char('*'));
        }
    }
    m_projectWildcard.append(QStringLiteral("-??"));
    m_projectWildcard.append(QStringLiteral("??"));
    m_projectWildcard.append(QStringLiteral("-??"));
    m_projectWildcard.append(QStringLiteral("-??"));
    m_projectWildcard.append(QStringLiteral("-??"));
    m_projectWildcard.append(QStringLiteral("-??.kdenlive"));

    slotParseBackupFiles();
    connect(backup_list, &QListWidget::currentRowChanged, this, &BackupWidget::slotDisplayBackupPreview);
    backup_list->setCurrentRow(0);
    backup_list->setMinimumHeight(QFontMetrics(font()).lineSpacing() * 12);
    slotParseBackupFiles();
}

BackupWidget::~BackupWidget() = default;

void BackupWidget::slotParseBackupFiles()
{
    QLocale locale;
    QStringList filter;
    filter << m_projectWildcard;
    backup_list->clear();

    // Parse new XDG backup folder $HOME/.local/share/kdenlive/.backup
    QDir backupFolder(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + QStringLiteral("/.backup"));
    backupFolder.setNameFilters(filter);
    QFileInfoList resultList = backupFolder.entryInfoList(QDir::Files, QDir::Time);
    for (int i = 0; i < resultList.count(); ++i) {
        QString label = locale.toString(resultList.at(i).lastModified(), QLocale::ShortFormat);
        if (m_projectWildcard.startsWith(QLatin1Char('*'))) {
            // Displaying all backup files, so add project name in the entries
            label.prepend(resultList.at(i).fileName().section(QLatin1Char('-'), 0, -7) + QStringLiteral(".kdenlive - "));
        }
        auto *item = new QListWidgetItem(label, backup_list);
        item->setData(Qt::UserRole, resultList.at(i).absoluteFilePath());
        item->setToolTip(resultList.at(i).absoluteFilePath());
    }

    // Parse old $HOME/kdenlive/.backup folder
    QDir dir(m_projectFolder.toLocalFile() + QStringLiteral("/.backup"));
    if (dir.exists()) {
        dir.setNameFilters(filter);
        QFileInfoList resultList2 = dir.entryInfoList(QDir::Files, QDir::Time);
        for (int i = 0; i < resultList2.count(); ++i) {
            QString label = locale.toString(resultList2.at(i).lastModified(), QLocale::ShortFormat);
            if (m_projectWildcard.startsWith(QLatin1Char('*'))) {
                // Displaying all backup files, so add project name in the entries
                label.prepend(resultList2.at(i).fileName().section(QLatin1Char('-'), 0, -7) + QStringLiteral(".kdenlive - "));
            }
            auto *item = new QListWidgetItem(label, backup_list);
            item->setData(Qt::UserRole, resultList2.at(i).absoluteFilePath());
            item->setToolTip(resultList2.at(i).absoluteFilePath());
        }
    }

    buttonBox->button(QDialogButtonBox::Open)->setEnabled(backup_list->count() > 0);
}

void BackupWidget::slotDisplayBackupPreview()
{
    if (!backup_list->currentItem()) {
        backup_preview->setPixmap(QPixmap());
        return;
    }
    const QString path = backup_list->currentItem()->data(Qt::UserRole).toString();
    QPixmap pix(path + QStringLiteral(".png"));
    backup_preview->setPixmap(pix);
}

QString BackupWidget::selectedFile() const
{
    if (!backup_list->currentItem()) {
        return QString();
    }
    return backup_list->currentItem()->data(Qt::UserRole).toString();
}
