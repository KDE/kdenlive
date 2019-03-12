/***************************************************************************
 *   Copyright (C) 2011 by Jean-Baptiste Mardelle (jb@kdenlive.org)        *
 *                                                                         *
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

#ifndef ARCHIVEORG_H
#define ARCHIVEORG_H

#include "abstractservice.h"

#include <QProcess>
#include <kio/jobclasses.h>

/**
  \brief search and download videos from archive.org

  This class is used to search the archive.org libraries and download videos. Is used by ResourceWidget
*/
class ArchiveOrg : public AbstractService
{
    Q_OBJECT

public:
    explicit ArchiveOrg(QListWidget *listWidget, QObject *parent = nullptr);
    ~ArchiveOrg() override;
    QString getExtension(QListWidgetItem *item) override;
    QString getDefaultDownloadName(QListWidgetItem *item) override;

public slots:
    void slotStartSearch(const QString &searchText, int page = 0) override;
    OnlineItemInfo displayItemDetails(QListWidgetItem *item) override;
    //  bool startItemPreview(QListWidgetItem *item);
    //  void stopItemPreview(QListWidgetItem *item);

private slots:
    void slotShowResults(KJob *job);
    void slotParseResults(KJob *job);
    // void slotParseThumbs(KJob* job);

private:
    QMap<QString, QString> m_metaInfo;
    QProcess *m_previewProcess;
    QString m_thumbsPath;

signals:
    void addClip(const QUrl &, const QString &);
    void gotPreview(const QString &url);
};

#endif
