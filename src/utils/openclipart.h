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

#ifndef OPENCLIPART_H
#define OPENCLIPART_H

#include "abstractservice.h"

#include <kio/jobclasses.h>

/**
  \brief search and download clip art from openclipart.org

  This class is used to search the openclipart.org libraries and download clipart. Is used by ResourceWidget
*/
class OpenClipArt : public AbstractService
{
    Q_OBJECT

public:
    explicit OpenClipArt(QListWidget *listWidget, QObject *parent = nullptr);
    ~OpenClipArt() override;
    QString getExtension(QListWidgetItem *item) override;
    QString getDefaultDownloadName(QListWidgetItem *item) override;

public slots:
    void slotStartSearch(const QString &searchText, int page = 0) override;
    OnlineItemInfo displayItemDetails(QListWidgetItem *item) override;

private slots:
    void slotShowResults(KJob *job);
};

#endif
