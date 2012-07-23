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


#ifndef FREESOUND_H
#define FREESOUND_H


#include "abstractservice.h"

#include <QProcess>
#include <kio/jobclasses.h>


class FreeSound : public AbstractService
{
    Q_OBJECT

public:
    explicit FreeSound(QListWidget *listWidget, QObject * parent = 0);
    ~FreeSound();
    virtual QString getExtension(QListWidgetItem *item);
    virtual QString getDefaultDownloadName(QListWidgetItem *item);


public slots:
    virtual void slotStartSearch(const QString searchText, int page = 0);
    virtual OnlineItemInfo displayItemDetails(QListWidgetItem *item);
    virtual bool startItemPreview(QListWidgetItem *item);
    virtual void stopItemPreview(QListWidgetItem *item);    

private slots:
    void slotShowResults(KJob* job);
    void slotParseResults(KJob* job);
    
private:
    QMap <QString, QString> m_metaInfo;
    QProcess *m_previewProcess;

signals:
    void addClip(KUrl, const QString &);
};


#endif

