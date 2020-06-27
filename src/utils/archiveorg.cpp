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

#include "kdenlive_debug.h"
#include <QApplication>
#include <QDomDocument>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QListWidget>
#include <QPushButton>

#include "kdenlivesettings.h"
#include <KLocalizedString>
#include <KMessageBox>
#include <kio/storedtransferjob.h>

ArchiveOrg::ArchiveOrg(QListWidget *listWidget, QObject *parent)
    : AbstractService(listWidget, parent)
    , m_previewProcess(new QProcess)
{
    serviceType = ARCHIVEORG;
    hasPreview = false; // has no downloadable preview as such. We are automatically displaying the animated gif as a preview.
    hasMetadata = true;
    inlineDownload = true;
    // connect(m_previewProcess, SIGNAL(stateChanged(QProcess::ProcessState)), this, SLOT(slotPreviewStatusChanged(QProcess::ProcessState)));
}

ArchiveOrg::~ArchiveOrg()
{
    delete m_previewProcess;
}
/**
 * @brief ArchiveOrg::slotStartSearch
 * @param searchText
 * called by ResourceWidget::slotStartSearch
 * @param page
 */
void ArchiveOrg::slotStartSearch(const QString &searchText, int page)
{
    m_listWidget->clear();
    QString uri = QStringLiteral("https://www.archive.org/advancedsearch.php?q=%28");
    uri.append(searchText);
    uri.append(QStringLiteral("%29+AND+mediatype%3A%28movies%29"));
    uri.append(QStringLiteral("&fl%5B%5D=creator&fl%5B%5D=description&fl%5B%5D=identifier&fl%5B%5D=licenseurl&fl%5B%5D=title&sort%5B%5D=&sort%5B%5D=&sort%5B%"
                              "5D=&rows=30")); //&callback=callback&save=yes"

    uri.append(QStringLiteral("&mediatype=movies")); // MovingImage");

    if (page > 1) {
        uri.append(QStringLiteral("&page=") + QString::number(page));
    }

    uri.append(QStringLiteral("&output=json"));

    //  qCDebug(KDENLIVE_LOG)<<"ArchiveOrg URL: "<< uri;
    KJob *resolveJob = KIO::storedGet(QUrl(uri), KIO::NoReload, KIO::HideProgressInfo);

    connect(resolveJob, &KJob::result, this, &ArchiveOrg::slotShowResults);
}

void ArchiveOrg::slotShowResults(KJob *job)
{
    if (job->error() != 0) {
        qCDebug(KDENLIVE_LOG) << "ArchiveOrg job error" << job->errorString();
        return;
    }
    m_listWidget->blockSignals(true);
    auto *storedQueryJob = static_cast<KIO::StoredTransferJob *>(job);
    QJsonParseError jsonError;
    QJsonDocument doc = QJsonDocument::fromJson(storedQueryJob->data(), &jsonError);

    if (jsonError.error != QJsonParseError::NoError) {
        // There was an error parsing data
        KMessageBox::sorry(m_listWidget, jsonError.errorString(), i18n("Error Loading Data"));
    }
    QVariant data = doc.toVariant();

    QVariant sounds;
    if (data.canConvert(QVariant::Map)) {
        QMap<QString, QVariant> map = data.toMap();
        QMap<QString, QVariant>::const_iterator i = map.constBegin();
        while (i != map.constEnd()) {
            if (i.key() == QLatin1String("response")) {
                sounds = i.value();
                if (sounds.canConvert(QVariant::Map)) {
                    QMap<QString, QVariant> soundsList = sounds.toMap();
                    if (soundsList.contains(QStringLiteral("numFound"))) {
                        emit searchInfo(i18np("Found %1 result", "Found %1 results", soundsList.value("numFound").toInt()));
                    }
                    QList<QVariant> resultsList;
                    if (soundsList.contains(QStringLiteral("docs"))) {
                        resultsList = soundsList.value(QStringLiteral("docs")).toList();
                        /* docs element has a sub element for each search result. And each of the sub elements look like:
                        {"creator":"DoubleACS",                < optional
                        "description":"Piano World",            < all have this
                        "identifier":"DoubleACS_PianoWorld0110", < all have this
                        "licenseurl":"http:\/\/creativecommons.org\/licenses\/by-nc-nd\/3.0\/",  < optional
                        "title":"Piano World"},     < all have this
                        */
                    }

                    for (int j = 0; j < resultsList.count(); ++j) {
                        if (resultsList.at(j).canConvert(QVariant::Map)) {
                            QMap<QString, QVariant> soundmap = resultsList.at(j).toMap();
                            if (soundmap.contains(QStringLiteral("title"))) {
                                QListWidgetItem *item = new QListWidgetItem(soundmap.value(QStringLiteral("title")).toString(), m_listWidget);
                                item->setData(descriptionRole, soundmap.value(QStringLiteral("description")).toString());
                                item->setData(idRole, soundmap.value(QStringLiteral("identifier")).toString());
                                QString author = soundmap.value(QStringLiteral("creator")).toString();
                                item->setData(authorRole, author);
                                if (author.startsWith(QLatin1String("http"))) {
                                    item->setData(authorUrl, author);
                                }
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

/**
 * @brief ArchiveOrg::displayItemDetails
 * @param item
 * @return
 * Called by ResourceWidget::slotUpdateCurrentSound()
 */
OnlineItemInfo ArchiveOrg::displayItemDetails(QListWidgetItem *item)
{
    OnlineItemInfo info;
    m_metaInfo.clear();
    if (!item) {
        return info;
    }
    //  info.itemPreview = item->data(previewRole).toString();
    info.itemDownload = item->data(downloadRole).toString();

    info.itemId = item->data(idRole).toString();
    info.itemName = item->text();
    info.infoUrl = item->data(infoUrl).toString();
    info.author = item->data(authorRole).toString();
    info.authorUrl = item->data(authorUrl).toString();
    info.license = item->data(licenseRole).toString();
    info.description = item->data(descriptionRole).toString();

    m_metaInfo.insert(QStringLiteral("url"), info.itemDownload);
    m_metaInfo.insert(QStringLiteral("id"), info.itemId);

    QString extraInfoUrl = item->data(infoUrl).toString() + QStringLiteral("&output=json");
    if (!extraInfoUrl.isEmpty()) {
        KJob *resolveJob = KIO::storedGet(QUrl(extraInfoUrl), KIO::NoReload, KIO::HideProgressInfo);
        resolveJob->setProperty("id", info.itemId);
        connect(resolveJob, &KJob::result, this, &ArchiveOrg::slotParseResults);
    }
    return info;
}

/**
 * @brief ArchiveOrg::slotParseResults
 * @param job
 * This slot is connected to the download job started by ArchiveOrg::displayItemDetails. The download job is downloading
 * the extra info associated with the search item the user has clicked on.
 * This slot parses the extra info from the json document and creates URLs to insert into html that is displayed in the ResourceWidget
 * The a href url has the tag _import added at the end. This tag is removed by ResourceWidget::slotOpenLink before being used to download the file
 */
void ArchiveOrg::slotParseResults(KJob *job)
{
    auto *storedQueryJob = static_cast<KIO::StoredTransferJob *>(job);

    QJsonParseError jsonError;
    QJsonDocument doc = QJsonDocument::fromJson(storedQueryJob->data(), &jsonError);

    if (jsonError.error != QJsonParseError::NoError) {
        // There was an error parsing data
        KMessageBox::sorry(m_listWidget, jsonError.errorString(), i18n("Error Loading Extra Data"));
        return;
    }
    QVariant data = doc.toVariant();
    QVariant files;
    QVariant fileMetaData;
    QString html = QStringLiteral("<style type=\"text/css\">tr.cellone {background-color: %1;}").arg(qApp->palette().alternateBase().color().name());
    html += QLatin1String(R"(</style><table width="100%" cellspacing="0"
      cellpadding="2">)");
    if (data.canConvert(QVariant::Map)) {
        QMap<QString, QVariant> map = data.toMap();
        QMap<QString, QVariant>::const_iterator i = map.constBegin();
        while (i != map.constEnd()) {
            //   qCDebug(KDENLIVE_LOG)<<"ArchiveOrg::slotParseResults - i.key"<<  i.key();
            if (i.key() == QLatin1String("files")) {
                QString format;
                QString fileSize;
                QString sDownloadUrl;
                QString sPreviewUrl;
                QString sThumbUrl;
                QString minsLong;
                files = i.value();
                if (files.canConvert(QVariant::Map)) {
                    QMap<QString, QVariant> filesList = files.toMap();
                    QMap<QString, QVariant>::const_iterator j = filesList.constBegin();
                    bool bThumbNailFound = false;
                    while (j != filesList.constEnd()) {
                        //  qCDebug(KDENLIVE_LOG)<<"ArchiveOrg::slotParseResults - file url "<< "https://archive.org/download/"+
                        //  m_metaInfo.value(QStringLiteral("id")) + j.key();
                        sDownloadUrl = "https://archive.org/download/" + m_metaInfo.value(QStringLiteral("id")) + j.key();
                        fileMetaData = j.value();
                        if (fileMetaData.canConvert(QVariant::Map)) {
                            QMap<QString, QVariant> filesMetaDataList = fileMetaData.toMap();
                            QMap<QString, QVariant>::const_iterator k = filesMetaDataList.constBegin();
                            while (k != filesMetaDataList.constEnd()) {
                                if (k.key() == QLatin1String("format")) {
                                    format = k.value().toString();
                                } else if (k.key() == QLatin1String("size")) {
                                    fileSize = QLocale().toString(k.value().toInt() / 1024);
                                } else if (k.key() == QLatin1String("length")) {
                                    minsLong = QString::number(k.value().toFloat() / 60, 'f', 1);
                                }

                                ++k; // next metadata item on file
                            }
                        }

                        if (format != QLatin1String("Animated GIF") && format != QLatin1String("Metadata") && format != QLatin1String("Archive BitTorrent") &&
                            format != QLatin1String("Thumbnail") && format != QLatin1String("JSON") && format != QLatin1String("JPEG") &&
                            format != QLatin1String("JPEG Thumb") && format != QLatin1String("PNG") && format != QLatin1String("Video Index")) {
                            // the a href url has the tag _import added at the end. This tag is removed by ResourceWidget::slotOpenLink before being used to
                            // download the file
                            html += QStringLiteral("<tr><td>") + format + QStringLiteral(" (") + fileSize + QStringLiteral("kb ") + minsLong +
                                    QStringLiteral("min) ") +
                                    QStringLiteral("</td><td><a href=\"%1\">%2</a></td></tr>").arg(sDownloadUrl + QStringLiteral("_import"), i18n("Import"));
                        }
                        if (format == QLatin1String("Thumbnail") && !bThumbNailFound) {
                            sThumbUrl = QStringLiteral("https://archive.org/download/") + m_metaInfo.value(QStringLiteral("id")) + j.key();
                            bThumbNailFound = true;
                            emit gotThumb(sThumbUrl);
                        }
                        if (format == QLatin1String("Animated GIF")) //
                        {
                            sPreviewUrl = "https://archive.org/download/" + m_metaInfo.value(QStringLiteral("id")) + j.key();
                            m_metaInfo.insert(QStringLiteral("preview"), sPreviewUrl);

                            emit gotPreview(sPreviewUrl);
                        }

                        ++j; // next file on the list of files that are part of this item
                    }
                }
            }
            ++i; // next key. keys are - "creativecommons", "dir", "files", "item","metadata", "misc","server"
        }
    } // end can the json be converted to a map

    html += QLatin1String("</table>");
    if (m_metaInfo.value(QStringLiteral("id")) == job->property("id").toString()) {
        //  qCDebug(KDENLIVE_LOG)<<"ArchiveOrg::slotParseResults html: "<<html;
        emit gotMetaInfo(html); // connected ResourceWidget::slotSetMetadata which updates the display with the html we pass
    }
    /*
    QDomNodeList links = doc.elementsByTagName(QStringLiteral("a"));
    QString html = QStringLiteral("<style type=\"text/css\">tr.cellone {background-color: %1;}").arg(qApp->palette().alternateBase().color().name());
    html += QLatin1String("</style><table width=\"100%\" cellspacing=\"0\" cellpadding=\"2\">");
    QString link;
    int ct = 0;
    m_thumbsPath.clear();
    for (int i = 0; i < links.count(); ++i) {
        QString href = links.at(i).toElement().attribute(QStringLiteral("href"));
        qCDebug(KDENLIVE_LOG)<<"ArchiveOrg::slotParseResults"<< href;
        if (href.endsWith(QLatin1String(".thumbs/"))) {
            // sub folder contains image thumbs, display one.
            m_thumbsPath = m_metaInfo.value(QStringLiteral("url")) + QLatin1Char('/') + href;
            KJob* thumbJob = KIO::storedGet( QUrl(m_thumbsPath), KIO::NoReload, KIO::HideProgressInfo );
            thumbJob->setProperty("id", m_metaInfo.value(QStringLiteral("id")));
            connect(thumbJob, &KJob::result, this, &ArchiveOrg::slotParseThumbs);
        }
        else if (!href.contains(QLatin1Char('/')) && !href.endsWith(QLatin1String(".xml"))) {
            link = m_metaInfo.value(QStringLiteral("url")) + QLatin1Char('/') + href;
            ct++;
            if (ct %2 == 0) {
                html += QLatin1String("<tr class=\"cellone\">");
            }
            else html += QLatin1String("<tr>");
            html += QStringLiteral("<td>") + QUrl(link).fileName() + QStringLiteral("</td><td><a href=\"%1\">%2</a></td><td><a
    href=\"%3\">%4</a></td></tr>").arg(link).arg(i18n("Preview")).arg(link + "_import").arg(i18n("Import"));
        }
    }
    html += QLatin1String("</table>");
    if (m_metaInfo.value(QStringLiteral("id")) == job->property("id").toString())
    {
        emit gotMetaInfo(html);// connected ResourceWidget::slotSetMetadata which updates the display with the html we pass
     //   qCDebug(KDENLIVE_LOG)<<"ArchiveOrg::slotParseResults"<<html;
    }
    */
}
/*

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
*/
/*
void ArchiveOrg::stopItemPreview(QListWidgetItem *item)
{
    if (m_previewProcess && m_previewProcess->state() != QProcess::NotRunning) {
        m_previewProcess->close();
    }
}
*/
QString ArchiveOrg::getExtension(QListWidgetItem *item)
{
    if (!item) {
        return QString();
    }
    return QStringLiteral("*.") + item->text().section(QLatin1Char('.'), -1);
}

QString ArchiveOrg::getDefaultDownloadName(QListWidgetItem *item)
{
    if (!item) {
        return QString();
    }
    return item->text();
}
/*
void ArchiveOrg::slotParseThumbs(KJob* job)
{
    KIO::StoredTransferJob* storedQueryJob = static_cast<KIO::StoredTransferJob*>( job );
    QDomDocument doc;
    doc.setContent(QString::fromUtf8(storedQueryJob->data()));
    QDomNodeList links = doc.elementsByTagName(QStringLiteral("a"));
    if (links.isEmpty()) return;
    for (int i = 0; i < links.count(); ++i) {
        QString href = links.at(i).toElement().attribute(QStringLiteral("href"));
        if (!href.contains(QLatin1Char('/')) && i >= links.count() / 2) {
            QString thumbUrl = m_thumbsPath + href;
            if (m_metaInfo.value(QStringLiteral("id")) == job->property("id").toString())
                emit gotThumb(thumbUrl);
            break;
        }
    }
}

*/
