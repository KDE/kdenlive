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


#include "freesound.h"

#include <QPushButton>
#include <QListWidget>
#include <QApplication>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QDebug>

#include "kdenlivesettings.h"
#include <kio/storedtransferjob.h>
#include <KLocalizedString>
#include <KMessageBox>
#include "qt-oauth-lib/oauth2.h"

/**
 * @brief FreeSound::FreeSound
 * @param listWidget
 * @param parent
 */
FreeSound::FreeSound(QListWidget *listWidget, QObject *parent) :
        AbstractService(listWidget, parent),
        m_previewProcess(new QProcess)
{
    serviceType = FREESOUND;
    hasPreview = true;
    hasMetadata = true;

    connect (m_previewProcess ,SIGNAL (error(QProcess::ProcessError )),this,SLOT(slotPreviewErrored(QProcess::ProcessError)));
    connect (m_previewProcess ,SIGNAL (finished(int , QProcess::ExitStatus )),this,SLOT(slotPreviewFinished(int , QProcess::ExitStatus)));
}

FreeSound::~FreeSound()
{
    delete m_previewProcess;
}
/**
 * @brief FreeSound::slotStartSearch
 * @param searchText
 * @param page
 * Called by ResourceWidget::slotStartSearch
 */
void FreeSound::slotStartSearch(const QString &searchText, int page)
{
    // ver  2 of freesound API
    m_listWidget->clear();

    QString uri = "https://www.freesound.org/apiv2/search/text/?format=json&query=";
    uri.append(searchText);
    if (page > 1)
        uri.append("&page=" + QString::number(page));
    uri.append("&token="  + OAuth2::m_strClientSecret);

   //  qDebug()<<uri;
    KIO::StoredTransferJob* resolveJob = KIO::storedGet( QUrl(uri), KIO::NoReload, KIO::HideProgressInfo );
    connect(resolveJob, &KIO::StoredTransferJob::result, this, &FreeSound::slotShowResults);

}

/**
 * @brief FreeSound::slotShowResults
 * @param job
 */
void FreeSound::slotShowResults(KJob* job)
{
    if (job->error() != 0 )
    {
        qDebug()<< job->errorString();

    }
    else
    {
        m_listWidget->blockSignals(true);// stop the listWidget from emiting signals.Ie clicking on the list while we are busy here will do nothing
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

                if (i.key() == "count") emit searchInfo(i18np("Found %1 result", "Found %1 results", i.value().toInt()));
                else if (i.key() == "num_pages") {
                    emit maxPages(i.value().toInt());
                }
                else if (i.key() == "results") {

                    sounds = i.value();
                    if (sounds.canConvert(QVariant::List)) {
                        QList <QVariant> soundsList = sounds.toList();
                        for (int j = 0; j < soundsList.count(); ++j) {
                            if (soundsList.at(j).canConvert(QVariant::Map)) {
                                QMap <QString, QVariant> soundmap = soundsList.at(j).toMap();

                                if (soundmap.contains("name")) {
                                    QListWidgetItem *item = new   QListWidgetItem(soundmap.value("name").toString(), m_listWidget);


                                    QVariant vid = soundmap.value("id");
                                    item->setData(idRole, vid);

                                    QVariant authorInfo = soundmap.value("username");
                                    item->setData(authorRole, authorInfo);

                                    item->setData(authorUrl, "http://freesound.org/people/" + soundmap.value("username").toString());
                                    item->setData(licenseRole, soundmap.value("license"));
                                    item->setData(infoData,"http://www.freesound.org/apiv2/sounds/"+ vid.toString() +"/?format=json&token=" + OAuth2::m_strClientSecret);



                                }
                            }
                        }
                    }
                }
                ++i;
            }
        }
        m_listWidget->blockSignals(false);// enable listWidget to send signals again. It will register clicks now
        m_listWidget->setCurrentRow(0);
        emit searchDone();
    }
}
    

/**
 * @brief FreeSound::displayItemDetails
 * @param item
 * @return
 * This is a slot
 * Called by  ResourceWidget::slotUpdateCurrentSound
 */
OnlineItemInfo FreeSound::displayItemDetails(QListWidgetItem *item)
{
    OnlineItemInfo info;
    m_metaInfo.clear();
    if (!item) {
        return info;
    }
    info.itemPreview = item->data(previewRole).toString();

    info.itemId = item->data(idRole).toString();
    info.itemName = item->text();
    info.infoUrl = item->data(infoUrl).toString();
    info.author = item->data(authorRole).toString();
    info.authorUrl = item->data(authorUrl).toString();
    m_metaInfo.insert(i18n("Duration"), item->data(durationRole).toString());
    info.license = item->data(licenseRole).toString();
    info.description = item->data(descriptionRole).toString();
    
    QString extraInfoUrl = item->data(infoData).toString();

    if (!extraInfoUrl.isEmpty()) {
        KJob* resolveJob = KIO::storedGet( QUrl(extraInfoUrl), KIO::NoReload, KIO::HideProgressInfo );
        // connect (obj,signal,obj, slot)
        // when the KJob resolveJob emits a result signal slotParseResults will be notified
        connect(resolveJob, &KJob::result, this, &FreeSound::slotParseResults);
    }

    return info;
}

/**
 * @brief FreeSound::slotParseResults
 * @param job
 * emits gotMetaInfo which is connected to slotSetMetadata and to slotDisplayMetaInfo
 * Fires when displayItemDetails has finished downloading the extra info from freesound
 */
void FreeSound::slotParseResults(KJob* job)
{

    KIO::StoredTransferJob* storedQueryJob = static_cast<KIO::StoredTransferJob*>( job );
    QJsonParseError jsonError;
    QJsonDocument doc = QJsonDocument::fromJson(storedQueryJob->data(), &jsonError);
    if (jsonError.error != QJsonParseError::NoError) {
        // There was an error parsing data
        KMessageBox::sorry(m_listWidget, jsonError.errorString(), i18n("Error Loading Data"));
    }
    QVariant data = doc.toVariant();
    QString html = QString("<style type=\"text/css\">tr.cellone {background-color: %1;}</style>").arg(qApp->palette().alternateBase().color().name());

    if (data.canConvert(QVariant::Map)) {
        QMap <QString, QVariant> info = data.toMap();
        
        html += "<table width=\"100%\" cellspacing=\"0\" cellpadding=\"2\">";
    
        if (info.contains("duration")) {
	    html += "<tr>";
            html += "<td>" + i18n("Duration (s)") + "</td><td>" + QString::number( info.value("duration").toDouble()) + "</td></tr>";
            m_metaInfo.remove(i18n("Duration"));
            m_metaInfo.insert(i18n("Duration"),  info.value("duration").toString());
        }
        
        if (info.contains("samplerate")) {
            html += "<tr class=\"cellone\">";
            html += "<td>" + i18n("Samplerate") + "</td><td>" + QString::number(info.value("samplerate").toDouble()) + "</td></tr>";
        }
        if (info.contains("channels")) {
            html += "<tr>";
            html += "<td>" + i18n("Channels") + "</td><td>" + QString::number(info.value("channels").toInt()) + "</td></tr>";
        }
        if (info.contains("filesize")) {
            html += "<tr class=\"cellone\">";
            KIO::filesize_t fSize = info.value("filesize").toDouble();
            html += "<td>" + i18n("File size") + "</td><td>" + KIO::convertSize(fSize) + "</td></tr>";
        }
        if (info.contains("license")) {
            m_metaInfo.insert("license", info.value("license").toString());
        }
        html +="</table>";
        if (info.contains("description")) {
            m_metaInfo.insert("description", info.value("description").toString());
        }

        if (info.contains("previews")) {
            QMap <QString, QVariant> previews = info.value("previews").toMap();


            if (previews.contains("preview-lq-mp3")) {
                m_metaInfo.insert("itemPreview", previews.value("preview-lq-mp3").toString());
            }

            if (previews.contains("preview-hq-mp3")) {// Can use the high qual preview as alternative download if user does not have a free sound account
               m_metaInfo.insert("HQpreview", previews.value("preview-hq-mp3").toString());

            }

        }

        if (info.contains("images")) {
            QMap <QString, QVariant> images = info.value("images").toMap();


            if (images.contains("waveform_m")) {
                m_metaInfo.insert("itemImage", images.value("waveform_m").toString());
            }
        }
        if (info.contains("download")) {// this URL will start a download of the actual sound - if used in a browser while logged on to freesound
             m_metaInfo.insert("itemDownload", info.value("download").toString());
        }
        if (info.contains("type")) {// wav, aif, mp3 etc
             m_metaInfo.insert("fileType", info.value("type").toString());
        }

    }

    emit gotMetaInfo(html);
    emit gotMetaInfo(m_metaInfo);
    emit gotThumb( m_metaInfo.value("itemImage"));

}

/**
 * @brief FreeSound::startItemPreview
 * Starts playing preview version of the sound using ffplay
 * @param item
 * @return
 */
bool FreeSound::startItemPreview(QListWidgetItem *item)
{    
    if (!item)
        return false;
    const QString url = m_metaInfo.value("itemPreview");
    if (url.isEmpty())
        return false;
    if (m_previewProcess) {
	    if (m_previewProcess->state() != QProcess::NotRunning) {
		    m_previewProcess->close();
		}
       // m_previewProcess->start(KdenliveSettings::ffplaypath() + " " +  url  + " -nodisp -autoexit");
       qDebug()<<KdenliveSettings::ffplaypath() + " " +  url  + " -nodisp -autoexit";
        m_previewProcess->start(KdenliveSettings::ffplaypath(), QStringList() << url << QStringLiteral("-nodisp") << QStringLiteral("-autoexit"));

	}
    return true;
}

/**
 * @brief FreeSound::stopItemPreview
 */
void FreeSound::stopItemPreview(QListWidgetItem * /*item*/)
{    
    if (m_previewProcess && m_previewProcess->state() != QProcess::NotRunning) {
        m_previewProcess->close();
    }
}
/**
 * @brief FreeSound::getExtension
 * @param item
 * @return
 */
QString FreeSound::getExtension(QListWidgetItem *item)
{
     QString sExt;
     QString sItem;
    if (!item)
        return QString();
    else
    {
     sItem=item->text();
     if (sItem.contains("."))
     {
         sExt=sItem.section('.', -1);
         return QStringLiteral("*.") + sExt;
     }
     else
         return QString();// return null if file name has no dots - ie no extension
    }
}

/**
 * @brief FreeSound::getDefaultDownloadName
 * @param item
 * @return
 */
QString FreeSound::getDefaultDownloadName(QListWidgetItem *item)
{
    if (!item)
        return QString();
    return item->text();
}

/**
 * @brief FreeSound::slotPreviewFinished
 * @param exitCode
 * @param exitStatus
 * Connected to the QProcess m_previewProcess finished signal
 * emits signal picked up by ResourceWidget that ResouceWidget uses
 * to set the Preview button back to the text Preview (it will say "Stop" before this.)
 */
void FreeSound::slotPreviewFinished(int  exitCode, QProcess::ExitStatus exitStatus)
{

     emit previewFinished();

}

void FreeSound::slotPreviewErrored(QProcess::ProcessError error)
{
  qDebug()<< error;
}
