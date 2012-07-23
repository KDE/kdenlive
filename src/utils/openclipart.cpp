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


#include "openclipart.h"

#include <QListWidget>
#include <QDomDocument>

#include <KDebug>
#include <kio/job.h>
#include <KIO/NetAccess>


OpenClipArt::OpenClipArt(QListWidget *listWidget, QObject *parent) :
        AbstractService(listWidget, parent)
{
    serviceType = OPENCLIPART;
}

OpenClipArt::~OpenClipArt()
{
}

void OpenClipArt::slotStartSearch(const QString searchText, int page)
{
    m_listWidget->clear();
    QString uri = "http://openclipart.org/api/search/?query=";
    uri.append(searchText);
    if (page > 1) uri.append("&page=" + QString::number(page));
        
    KJob* resolveJob = KIO::storedGet( KUrl(uri), KIO::NoReload, KIO::HideProgressInfo );
    connect( resolveJob, SIGNAL( result( KJob* ) ), this, SLOT( slotShowResults( KJob* ) ) );
}


void OpenClipArt::slotShowResults(KJob* job)
{
    if (job->error() != 0 ) return;
    m_listWidget->blockSignals(true);    
    KIO::StoredTransferJob* storedQueryJob = static_cast<KIO::StoredTransferJob*>( job );
    
    QDomDocument doc;
    doc.setContent(QString::fromAscii(storedQueryJob->data()));
    QDomNodeList items = doc.documentElement().elementsByTagName("item");
    for (int i = 0; i < items.count(); i++) {
        QDomElement currentClip = items.at(i).toElement();
        QDomElement title = currentClip.firstChildElement("title");
        QListWidgetItem *item = new QListWidgetItem(title.firstChild().nodeValue(), m_listWidget);
        QDomElement thumb = currentClip.firstChildElement("media:thumbnail");
        item->setData(imageRole, thumb.attribute("url"));
        QDomElement enclosure = currentClip.firstChildElement("enclosure");
        item->setData(downloadRole, enclosure.attribute("url"));
        QDomElement link = currentClip.firstChildElement("link");
        item->setData(infoUrl, link.firstChild().nodeValue());
        QDomElement license = currentClip.firstChildElement("cc:license");
        item->setData(licenseRole, license.firstChild().nodeValue());
        QDomElement desc = currentClip.firstChildElement("description");
        item->setData(descriptionRole, desc.firstChild().nodeValue());
        QDomElement author = currentClip.firstChildElement("dc:creator");
        item->setData(authorRole, author.firstChild().nodeValue());
        item->setData(authorUrl, QString("http://openclipart.org/user-detail/") + author.firstChild().nodeValue());
    }        
    m_listWidget->blockSignals(false);
    m_listWidget->setCurrentRow(0);
    emit searchDone();
}
    

OnlineItemInfo OpenClipArt::displayItemDetails(QListWidgetItem *item)
{
    OnlineItemInfo info;
    if (!item) {
        return info;
    }
    info.itemPreview = item->data(previewRole).toString();
    info.itemDownload = item->data(downloadRole).toString();
    info.itemId = item->data(idRole).toInt();
    info.itemName = item->text();
    info.infoUrl = item->data(infoUrl).toString();
    info.author = item->data(authorRole).toString();
    info.authorUrl = item->data(authorUrl).toString();
    info.license = item->data(licenseRole).toString();
    info.description = item->data(descriptionRole).toString();
    emit gotThumb(item->data(imageRole).toString());
    return info;
}

QString OpenClipArt::getExtension(QListWidgetItem *item)
{
    if (!item) return QString();
    QString url = item->data(downloadRole).toString();
    return QString("*.") + url.section('.', -1);
}

QString OpenClipArt::getDefaultDownloadName(QListWidgetItem *item)
{
    if (!item) return QString();
    QString url = item->data(downloadRole).toString();
    QString path = item->text();
    path.append('.' + url.section('.', -1));
    return path;
}

