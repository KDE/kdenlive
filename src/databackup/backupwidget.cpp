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


BackupWidget::BackupWidget(KUrl projectUrl, KUrl projectFolder, QWidget * parent) :
        QDialog(parent),
        m_url(projectUrl)
{
    setupUi(this);
    setWindowTitle(i18n("Restore Backup File"));

    KUrl backupFile;
    m_projectWildcard = projectUrl.fileName().section('.', 0, -2);
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
    QListWidgetItem *item;
    for (int i = 0; i < resultList.count(); i++) {
        item = new QListWidgetItem(resultList.at(i).lastModified().toString(Qt::DefaultLocaleLongDate), backup_list);
        item->setData(Qt::UserRole, resultList.at(i).absoluteFilePath());
    }
}

void BackupWidget::slotDisplayBackupPreview()
{
    QString path = backup_list->currentItem()->data(Qt::UserRole).toString();
    QPixmap pix(path + ".png");
    backup_preview->setPixmap(pix);
}

QString BackupWidget::selectedFile()
{
    if (!backup_list->currentItem()) return QString();
    return backup_list->currentItem()->data(Qt::UserRole).toString();
}