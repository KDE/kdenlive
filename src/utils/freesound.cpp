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
#include <QSpinBox>
#include <QListWidget>
#include <QDomDocument>

#include <KDebug>
#include "kdenlivesettings.h"
#include <KGlobalSettings>
#include <KMessageBox>
#include <KFileDialog>
#include <kio/job.h>
#include <KIO/NetAccess>
#include <Solid/Networking>
#include <KRun>

#ifdef USE_NEPOMUK
#include <Nepomuk/Variant>
#include <Nepomuk/Resource>
#include <Nepomuk/ResourceManager>
#include <Soprano/Vocabulary/NAO>
#include <Nepomuk/Vocabulary/NIE>
#include <Nepomuk/Vocabulary/NDO>
#endif

#ifdef USE_QJSON
#include <qjson/parser.h>
#endif

FreeSound::FreeSound(QListWidget *listWidget, QObject *parent) :
        AbstractService(listWidget, parent),
        m_previewProcess(new QProcess)
{
    serviceType = FREESOUND;
    hasPreview = true;
    hasMetadata = true;
    //connect(m_previewProcess, SIGNAL(stateChanged(QProcess::ProcessState)), this, SLOT(slotPreviewStatusChanged(QProcess::ProcessState)));
}

FreeSound::~FreeSound()
{
    if (m_previewProcess) delete m_previewProcess;
}

void FreeSound::slotStartSearch(const QString searchText, int page)
{
    m_listWidget->clear();
    QString uri = "http://www.freesound.org/api/sounds/search/?q=";
    uri.append(searchText);
    if (page > 1) uri.append("&p=" + QString::number(page));
    uri.append("&api_key=a1772c8236e945a4bee30a64058dabf8");

    KJob* resolveJob = KIO::storedGet( KUrl(uri), KIO::NoReload, KIO::HideProgressInfo );
    connect( resolveJob, SIGNAL( result( KJob* ) ), this, SLOT( slotShowResults( KJob* ) ) );
}


void FreeSound::slotShowResults(KJob* job)
{
    if (job->error() != 0 ) return;
    m_listWidget->blockSignals(true);
    KIO::StoredTransferJob* storedQueryJob = static_cast<KIO::StoredTransferJob*>( job );
#ifdef USE_QJSON
    QJson::Parser parser;
    bool ok;
    //kDebug()<<"// GOT RESULT: "<<m_result;
    QVariant data = parser.parse(storedQueryJob->data(), &ok);
    QVariant sounds;
    if (data.canConvert(QVariant::Map)) {
        QMap <QString, QVariant> map = data.toMap();
        QMap<QString, QVariant>::const_iterator i = map.constBegin();
        while (i != map.constEnd()) {
            if (i.key() == "num_results") emit searchInfo(i18np("Found %1 result", "Found %1 results", i.value().toInt()));
            else if (i.key() == "num_pages") {
                emit maxPages(i.value().toInt());
            }
            else if (i.key() == "sounds") {
                sounds = i.value();
                if (sounds.canConvert(QVariant::List)) {
                    QList <QVariant> soundsList = sounds.toList();
                    for (int j = 0; j < soundsList.count(); j++) {
                        if (soundsList.at(j).canConvert(QVariant::Map)) {
                            QMap <QString, QVariant> soundmap = soundsList.at(j).toMap();
                            if (soundmap.contains("original_filename")) {
                                QListWidgetItem *item = new   QListWidgetItem(soundmap.value("original_filename").toString(), m_listWidget);
                                item->setData(imageRole, soundmap.value("waveform_m").toString());
                                item->setData(infoUrl, soundmap.value("url").toString());
                                item->setData(infoData, soundmap.value("ref").toString() + "?api_key=a1772c8236e945a4bee30a64058dabf8");
                                item->setData(durationRole, soundmap.value("duration").toDouble());
                                item->setData(idRole, soundmap.value("id").toInt());
                                item->setData(previewRole, soundmap.value("preview-hq-mp3").toString());
                                item->setData(downloadRole, soundmap.value("serve").toString() + "?api_key=a1772c8236e945a4bee30a64058dabf8");
                                QVariant authorInfo = soundmap.value("user");
                                if (authorInfo.canConvert(QVariant::Map)) {
                                    QMap <QString, QVariant> authorMap = authorInfo.toMap();
                                    if (authorMap.contains("username")) {
                                        item->setData(authorRole, authorMap.value("username").toString());
                                        item->setData(authorUrl, authorMap.value("url").toString());
                                    }
                                }
                            }
                        }
                    }
                }
            }
            ++i;
        }
    }
#endif  
    m_listWidget->blockSignals(false);
    m_listWidget->setCurrentRow(0);
}
    

OnlineItemInfo FreeSound::displayItemDetails(QListWidgetItem *item)
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
    m_metaInfo.insert(i18n("Duration"), item->data(durationRole).toString());
    info.license = item->data(licenseRole).toString();
    info.description = item->data(descriptionRole).toString();
    
    QString extraInfoUrl = item->data(infoData).toString();
    if (!extraInfoUrl.isEmpty()) {
        KJob* resolveJob = KIO::storedGet( KUrl(extraInfoUrl), KIO::NoReload, KIO::HideProgressInfo );
        connect( resolveJob, SIGNAL( result( KJob* ) ), this, SLOT( slotParseResults( KJob* ) ) );
    }
    info.imagePreview = item->data(imageRole).toString();
    return info;
}


void FreeSound::slotParseResults(KJob* job)
{
#ifdef USE_QJSON
    KIO::StoredTransferJob* storedQueryJob = static_cast<KIO::StoredTransferJob*>( job );
    QJson::Parser parser;
    bool ok;
    QVariant data = parser.parse(storedQueryJob->data(), &ok);
    if (data.canConvert(QVariant::Map)) {
        QMap <QString, QVariant> infos = data.toMap();
        //if (m_currentId != infos.value("id").toInt()) return;
        if (infos.contains("samplerate"))
            m_metaInfo.insert(i18n("Samplerate"), QString::number(infos.value("samplerate").toDouble()));
        if (infos.contains("channels"))
            m_metaInfo.insert(i18n("Channels"), QString::number(infos.value("channels").toInt()));
        if (infos.contains("filesize")) {
            KIO::filesize_t fSize = infos.value("filesize").toDouble();
            m_metaInfo.insert(i18n("File size"), KIO::convertSize(fSize));
        }
        if (infos.contains("description")) {
            m_metaInfo.insert("description", infos.value("description").toString());
        }
        if (infos.contains("license")) {
            m_metaInfo.insert("license", infos.value("license").toString());
        }
    }
    emit gotMetaInfo(m_metaInfo);
#endif    
}


bool FreeSound::startItemPreview(QListWidgetItem *item)
{    
    if (!item) return false;
    QString url = item->data(previewRole).toString();
    if (url.isEmpty()) return false;
    if (m_previewProcess && m_previewProcess->state() != QProcess::NotRunning) {
        m_previewProcess->close();
    }
    m_previewProcess->start("ffplay", QStringList() << url << "-nodisp");
    return true;
}


void FreeSound::stopItemPreview(QListWidgetItem *item)
{    
    if (m_previewProcess && m_previewProcess->state() != QProcess::NotRunning) {
        m_previewProcess->close();
    }
}

QString FreeSound::getExtension(QListWidgetItem *item)
{
    if (!item) return QString();
    return QString("*.") + item->text().section('.', -1);
}


QString FreeSound::getDefaultDownloadName(QListWidgetItem *item)
{
    if (!item) return QString();
    return item->text();
}
