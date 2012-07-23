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


#include "backupwidget.h"
#include "kdenlivesettings.h"

#include <KUrl>


BackupWidget::BackupWidget(KUrl projectUrl, KUrl projectFolder, const QString &projectId, QWidget * parent) :
        QDialog(parent)
{
    setupUi(this);
    setWindowTitle(i18n("Restore Backup File"));

    KUrl backupFile;

    if (projectUrl.isEmpty()) {
        // No url, means we opened the backup dialog from an empty project
        info_label->setText(i18n("Showing all backup files in folder"));
        m_projectWildcard = '*';
    }
    else {
        info_label->setText(i18n("Showing backup files for %1", projectUrl.fileName()));
        m_projectWildcard = projectUrl.fileName().section('.', 0, -2);
        if (!projectId.isEmpty()) m_projectWildcard.append('-' + projectId);
        else {
            // No project id, it was lost, add wildcard
            m_projectWildcard.append('*');
        }
    }
    project_url->setUrl(projectFolder);
    m_projectWildcard.append("-??");
    m_projectWildcard.append("??");
    m_projectWildcard.append("-??");
    m_projectWildcard.append("-??");
    m_projectWildcard.append("-??");
    m_projectWildcard.append("-??.kdenlive");

    slotParseBackupFiles();
    connect(backup_list, SIGNAL(currentRowChanged(int)), this, SLOT(slotDisplayBackupPreview()));
    connect(project_url, SIGNAL(textChanged(const QString &)), this, SLOT(slotParseBackupFiles()));
    backup_list->setCurrentRow(0);
    backup_list->setMinimumHeight(QFontMetrics(font()).lineSpacing() * 12);
    
}



BackupWidget::~BackupWidget()
{

}

void BackupWidget::slotParseBackupFiles()
{
    QStringList filter;
    KUrl backupFile = project_url->url();
    backupFile.addPath(".backup/");
    QDir dir(backupFile.path());

    filter << m_projectWildcard;
    dir.setNameFilters(filter);
    QFileInfoList resultList = dir.entryInfoList(QDir::Files, QDir::Time);
    QStringList results;
    backup_list->clear();
    QListWidgetItem *item;
    QString label;
    for (int i = 0; i < resultList.count(); i++) {
        label = resultList.at(i).lastModified().toString(Qt::SystemLocaleLongDate);
        if (m_projectWildcard.startsWith('*')) {
            // Displaying all backup files, so add project name in the entries
            label.prepend(resultList.at(i).fileName().section('-', 0, -7) + ".kdenlive - ");
        }
        item = new QListWidgetItem(label, backup_list);
        item->setData(Qt::UserRole, resultList.at(i).absoluteFilePath());
    }
    buttonBox->button(QDialogButtonBox::Open)->setEnabled(backup_list->count() > 0);
}

void BackupWidget::slotDisplayBackupPreview()
{
    if (!backup_list->currentItem()) {
        backup_preview->setPixmap(QPixmap());
        return;
    }
    QString path = backup_list->currentItem()->data(Qt::UserRole).toString();
    QPixmap pix(path + ".png");
    backup_preview->setPixmap(pix);
}

QString BackupWidget::selectedFile()
{
    if (!backup_list->currentItem()) return QString();
    return backup_list->currentItem()->data(Qt::UserRole).toString();
}

