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

#include <QDomDocument>
#include <QListWidget>

#include <kio/job.h>

OpenClipArt::OpenClipArt(QListWidget *listWidget, QObject *parent)
    : AbstractService(listWidget, parent)
{
    serviceType = OPENCLIPART;
}

OpenClipArt::~OpenClipArt() = default;
/**
 * @brief OpenClipArt::slotStartSearch
 * @param searchText
 * @param page
 *called by  ResourceWidget::slotStartSearch
 */
void OpenClipArt::slotStartSearch(const QString &searchText, int page)
{
    m_listWidget->clear();
    QString uri = QStringLiteral("http://openclipart.org/api/search/?query=");
    uri.append(searchText);
    if (page > 1) {
        uri.append(QStringLiteral("&page=") + QString::number(page));
    }

    KJob *resolveJob = KIO::storedGet(QUrl(uri), KIO::NoReload, KIO::HideProgressInfo);
    connect(resolveJob, &KJob::result, this, &OpenClipArt::slotShowResults);
}

void OpenClipArt::slotShowResults(KJob *job)
{
    if (job->error() != 0) {
        return;
    }
    m_listWidget->blockSignals(true);
    auto *storedQueryJob = static_cast<KIO::StoredTransferJob *>(job);

    QDomDocument doc;
    doc.setContent(QString::fromLatin1(storedQueryJob->data()));
    QDomNodeList items = doc.documentElement().elementsByTagName(QStringLiteral("item"));
    for (int i = 0; i < items.count(); ++i) {
        QDomElement currentClip = items.at(i).toElement();
        QDomElement title = currentClip.firstChildElement(QStringLiteral("title"));
        QListWidgetItem *item = new QListWidgetItem(title.firstChild().nodeValue(), m_listWidget);
        QDomElement thumb = currentClip.firstChildElement(QStringLiteral("media:thumbnail"));
        item->setData(imageRole, thumb.attribute(QStringLiteral("url")));
        QDomElement enclosure = currentClip.firstChildElement(QStringLiteral("enclosure"));
        item->setData(downloadRole, enclosure.attribute(QStringLiteral("url")));
        QDomElement link = currentClip.firstChildElement(QStringLiteral("link"));
        item->setData(infoUrl, link.firstChild().nodeValue());

        QDomElement license = currentClip.firstChildElement(QStringLiteral("cc:license"));

        item->setData(licenseRole, license.firstChild().nodeValue());
        QDomElement desc = currentClip.firstChildElement(QStringLiteral("description"));
        item->setData(descriptionRole, desc.firstChild().nodeValue());
        QDomElement author = currentClip.firstChildElement(QStringLiteral("dc:creator"));
        item->setData(authorRole, author.firstChild().nodeValue());
        item->setData(authorUrl, QStringLiteral("http://openclipart.org/user-detail/") + author.firstChild().nodeValue());
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
    info.itemId = item->data(idRole).toString();
    info.itemName = item->text();
    info.infoUrl = item->data(infoUrl).toString();
    info.author = item->data(authorRole).toString();
    info.authorUrl = item->data(authorUrl).toString();

    info.license = QStringLiteral("https://openclipart.org/share"); // all openclipartfiles are public domain
    info.description = item->data(descriptionRole).toString();
    emit gotThumb(item->data(imageRole).toString());
    return info;
}

QString OpenClipArt::getExtension(QListWidgetItem *item)
{
    if (!item) {
        return QString();
    }
    QString url = item->data(downloadRole).toString();
    return QStringLiteral("*.") + url.section(QLatin1Char('.'), -1);
}

QString OpenClipArt::getDefaultDownloadName(QListWidgetItem *item)
{
    if (!item) {
        return QString();
    }
    QString url = item->data(downloadRole).toString();
    QString path = item->text();
    path.append('.' + url.section(QLatin1Char('.'), -1));
    return path;
}
