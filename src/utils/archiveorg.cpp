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


#include "archiveorg.h"

#include <QPushButton>
#include <QListWidget>
#include <QDomDocument>
#include <QApplication>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QDebug>

#include "kdenlivesettings.h"
#include <kio/storedtransferjob.h>
#include <KLocalizedString>
#include <KMessageBox>

ArchiveOrg::ArchiveOrg(QListWidget *listWidget, QObject *parent) :
        AbstractService(listWidget, parent),
        m_previewProcess(new QProcess)
{
    serviceType = ARCHIVEORG;
    hasPreview = false;
    hasMetadata = true;
    inlineDownload = true;
    //connect(m_previewProcess, SIGNAL(stateChanged(QProcess::ProcessState)), this, SLOT(slotPreviewStatusChanged(QProcess::ProcessState)));
}

ArchiveOrg::~ArchiveOrg()
{
    delete m_previewProcess;
}

void ArchiveOrg::slotStartSearch(const QString &searchText, int page)
{
    m_listWidget->clear();
    QString uri = QStringLiteral("http://www.archive.org/advancedsearch.php?q=");
    uri.append(searchText);
    uri.append("%20AND%20mediatype:movies");//MovingImage");
    uri.append("&fl%5B%5D=creator&fl%5B%5D=description&fl%5B%5D=identifier&fl%5B%5D=licenseurl&fl%5B%5D=title");
    uri.append("&rows=30");
    if (page > 1) uri.append("&page=" + QString::number(page));
    uri.append("&output=json"); //&callback=callback&save=yes#raw");

    KJob* resolveJob = KIO::storedGet( QUrl(uri), KIO::NoReload, KIO::HideProgressInfo );
    connect(resolveJob, &KJob::result, this, &ArchiveOrg::slotShowResults);
}


void ArchiveOrg::slotShowResults(KJob* job)
{
    if (job->error() != 0 ) return;
    m_listWidget->blockSignals(true);
    KIO::StoredTransferJob* storedQueryJob = static_cast<KIO::StoredTransferJob*>( job );
    QJsonParseError jsonError;
    QJsonDocument doc = QJsonDocument::fromJson(storedQueryJob->data(), &jsonError);
    if (jsonError.error != QJsonParseError::NoError) {
        // There was an error parsing data
        KMessageBox::sorry(m_listWidget, jsonError.errorString(), i18n("Error Loading Data"));
    }
    QVariant data = doc.toVariant();
    
    QVariant sounds;
    if (data.canConvert(QVariant::Map)) {
        QMap <QString, QVariant> map = data.toMap();
        QMap<QString, QVariant>::const_iterator i = map.constBegin();
        while (i != map.constEnd()) {
            if (i.key() == QLatin1String("response")) {
                sounds = i.value();
                if (sounds.canConvert(QVariant::Map)) {
                    QMap <QString, QVariant> soundsList = sounds.toMap();
                    if (soundsList.contains(QStringLiteral("numFound"))) emit searchInfo(i18np("Found %1 result", "Found %1 results", soundsList.value("numFound").toInt()));
                    QList <QVariant> resultsList;
                    if (soundsList.contains(QStringLiteral("docs"))) {
                        resultsList = soundsList.value(QStringLiteral("docs")).toList();
                    }
                    
                    for (int j = 0; j < resultsList.count(); ++j) {
                        if (resultsList.at(j).canConvert(QVariant::Map)) {
                            QMap <QString, QVariant> soundmap = resultsList.at(j).toMap();
                            if (soundmap.contains(QStringLiteral("title"))) {
                                QListWidgetItem *item = new   QListWidgetItem(soundmap.value(QStringLiteral("title")).toString(), m_listWidget);
                                item->setData(descriptionRole, soundmap.value(QStringLiteral("description")).toString());
                                item->setData(idRole, soundmap.value(QStringLiteral("identifier")).toString());
                                QString author = soundmap.value(QStringLiteral("creator")).toString();
                                item->setData(authorRole, author);
                                if (author.startsWith(QLatin1String("http"))) item->setData(authorUrl, author);
                                item->setData(infoUrl, "http://archive.org/details/" + soundmap.value(QStringLiteral("identifier")).toString());
                                item->setData(downloadRole, "http://archive.org/download/" + soundmap.value(QStringLiteral("identifier")).toString());
                                item->setData(licenseRole, soundmap.value(QStringLiteral("licenseurl")).toString());                        
                            }
                        }
                    }
                }
            }
            ++i;
        }
    }
    m_listWidget->blockSignals(false);
    m_listWidget->setCurrentRow(0);
    emit searchDone();
}
    

OnlineItemInfo ArchiveOrg::displayItemDetails(QListWidgetItem *item)
{
    OnlineItemInfo info;
    m_metaInfo.clear();
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
    
    m_metaInfo.insert(QStringLiteral("url"), info.itemDownload);
    m_metaInfo.insert(QStringLiteral("id"), info.itemId);
    
    QString extraInfoUrl = item->data(downloadRole).toString();
    if (!extraInfoUrl.isEmpty()) {
        KJob* resolveJob = KIO::storedGet( QUrl(extraInfoUrl), KIO::NoReload, KIO::HideProgressInfo );
        resolveJob->setProperty("id", info.itemId);
        connect(resolveJob, &KJob::result, this, &ArchiveOrg::slotParseResults);
    }
    return info;
}


void ArchiveOrg::slotParseResults(KJob* job)
{
    KIO::StoredTransferJob* storedQueryJob = static_cast<KIO::StoredTransferJob*>( job );
    QDomDocument doc;
    doc.setContent(QString::fromUtf8(storedQueryJob->data()));
    QDomNodeList links = doc.elementsByTagName(QStringLiteral("a"));
    QString html = QStringLiteral("<style type=\"text/css\">tr.cellone {background-color: %1;}").arg(qApp->palette().alternateBase().color().name());
    html += QLatin1String("</style><table width=\"100%\" cellspacing=\"0\" cellpadding=\"2\">");
    QString link;
    int ct = 0;
    m_thumbsPath.clear();
    for (int i = 0; i < links.count(); ++i) {
        QString href = links.at(i).toElement().attribute(QStringLiteral("href"));
        if (href.endsWith(QLatin1String(".thumbs/"))) {
            // sub folder contains image thumbs, display one.
            m_thumbsPath = m_metaInfo.value(QStringLiteral("url")) + '/' + href;
            KJob* thumbJob = KIO::storedGet( QUrl(m_thumbsPath), KIO::NoReload, KIO::HideProgressInfo );
            thumbJob->setProperty("id", m_metaInfo.value(QStringLiteral("id")));
            connect(thumbJob, &KJob::result, this, &ArchiveOrg::slotParseThumbs);
        }
        else if (!href.contains('/') && !href.endsWith(QLatin1String(".xml"))) {
            link = m_metaInfo.value(QStringLiteral("url")) + '/' + href;
            ct++;
            if (ct %2 == 0) {
                html += QLatin1String("<tr class=\"cellone\">");
            }
            else html += QLatin1String("<tr>");
            html += "<td>" + QUrl(link).fileName() + QStringLiteral("</td><td><a href=\"%1\">%2</a></td><td><a href=\"%3\">%4</a></td></tr>").arg(link).arg(i18n("Preview")).arg(link + "_import").arg(i18n("Import"));
        }
    }
    html += QLatin1String("</table>");
    if (m_metaInfo.value(QStringLiteral("id")) == job->property("id").toString()) emit gotMetaInfo(html);
}


bool ArchiveOrg::startItemPreview(QListWidgetItem *item)
{    
    if (!item) return false;
    QString url = item->data(previewRole).toString();
    if (url.isEmpty()) return false;
    if (m_previewProcess) {
	    if (m_previewProcess->state() != QProcess::NotRunning) {
		    m_previewProcess->close();
		}
		m_previewProcess->start(KdenliveSettings::ffplaypath(), QStringList() << url << QStringLiteral("-nodisp"));
	}
    return true;
}


void ArchiveOrg::stopItemPreview(QListWidgetItem */*item*/)
{    
    if (m_previewProcess && m_previewProcess->state() != QProcess::NotRunning) {
        m_previewProcess->close();
    }
}

QString ArchiveOrg::getExtension(QListWidgetItem *item)
{
    if (!item) return QString();
    return QStringLiteral("*.") + item->text().section('.', -1);
}


QString ArchiveOrg::getDefaultDownloadName(QListWidgetItem *item)
{
    if (!item) return QString();
    return item->text();
}

void ArchiveOrg::slotParseThumbs(KJob* job)
{
    KIO::StoredTransferJob* storedQueryJob = static_cast<KIO::StoredTransferJob*>( job );
    QDomDocument doc;
    doc.setContent(QString::fromUtf8(storedQueryJob->data()));
    QDomNodeList links = doc.elementsByTagName(QStringLiteral("a"));
    if (links.isEmpty()) return;
    for (int i = 0; i < links.count(); ++i) {
        QString href = links.at(i).toElement().attribute(QStringLiteral("href"));
        if (!href.contains('/') && i >= links.count() / 2) {
            QString thumbUrl = m_thumbsPath + href;
            if (m_metaInfo.value(QStringLiteral("id")) == job->property("id").toString())
                emit gotThumb(thumbUrl);
            break;
        }
    }
}


