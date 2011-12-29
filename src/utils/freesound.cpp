/***************************************************************************
 *   Copyright (C) 2008 by Jean-Baptiste Mardelle (jb@kdenlive.org)        *
 *   Copyright (C) 2011 by Marco Gittler (marco@gitma.de)                  *
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

#include <KDebug>
#include "kdenlivesettings.h"
#include <KGlobalSettings>
#include <KMessageBox>
#include <KFileDialog>
#include <kio/job.h>
#include <KIO/NetAccess>
#include <KRun>

#include <qjson/parser.h>

const int imageRole = Qt::UserRole;
const int urlRole = Qt::UserRole + 1;
const int soundRole = Qt::UserRole + 2;
const int durationRole = Qt::UserRole + 3;
const int previewRole = Qt::UserRole + 4;
const int authorRole = Qt::UserRole + 5;
const int authorUrl = Qt::UserRole + 6;
const int soundUrl = Qt::UserRole + 7;

FreeSound::FreeSound(const QString & folder, QWidget * parent) :
        QDialog(parent),
        m_folder(folder)
{
    setFont(KGlobalSettings::toolBarFont());
    setupUi(this);
    setAttribute(Qt::WA_DeleteOnClose);
    setWindowTitle(i18n("Search FreeSound Audio Library"));
    connect(button_search, SIGNAL(clicked()), this, SLOT(slotStartSearch()));
    connect(search_results, SIGNAL(currentRowChanged(int)), this, SLOT(slotUpdateCurrentSound()));
    connect(button_preview, SIGNAL(clicked()), this, SLOT(slotPlaySound()));
    connect(button_import, SIGNAL(clicked()), this, SLOT(slotSaveSound()));
    connect(sound_author, SIGNAL(leftClickedUrl(const QString &)), this, SLOT(slotOpenUrl(const QString &)));
    connect(sound_name, SIGNAL(leftClickedUrl(const QString &)), this, SLOT(slotOpenUrl(const QString &)));
    m_previewProcess = new QProcess;
    connect(m_previewProcess, SIGNAL(stateChanged(QProcess::ProcessState)), this, SLOT(slotPreviewStatusChanged(QProcess::ProcessState)));
}

FreeSound::~FreeSound()
{
    if (m_previewProcess) delete m_previewProcess;
}

void FreeSound::slotStartSearch()
{
    m_result.clear();
    m_currentPreview.clear();
    m_currentUrl.clear();
    page_number->blockSignals(true);
    page_number->setValue(0);
    page_number->blockSignals(false);
    QString uri = "http://www.freesound.org/api/sounds/search/?q=";
    uri.append(search_text->text());
    uri.append("&api_key=a1772c8236e945a4bee30a64058dabf8");
    KIO::TransferJob *job = KIO::get(KUrl(uri));
    connect (job, SIGNAL(  data(KIO::Job *, const QByteArray & )), this, SLOT(slotDataIsHere(KIO::Job *,const QByteArray &)));
    connect(job, SIGNAL(result(KJob*)), this, SLOT(slotShowResults()));
}

void FreeSound::slotDataIsHere(KIO::Job *,const QByteArray & data )
{
  //kDebug() << "data is here";
  m_result.append(data);
}

void FreeSound::slotShowResults()
{
    QJson::Parser parser;
    bool ok;
    m_data = parser.parse(m_result, &ok);
    QVariant sounds;
    search_results->blockSignals(true);
    search_results->clear();
    if (m_data.canConvert(QVariant::Map)) {
        QMap <QString, QVariant> map = m_data.toMap();
        QMap<QString, QVariant>::const_iterator i = map.constBegin();
        while (i != map.constEnd()) {
            if (i.key() == "num_results") search_info->setText(i18np("Found %1 result", "Found %1 results", i.value().toInt()));
            else if (i.key() == "num_pages") {
                page_number->setMaximum(i.value().toInt());
            }
            else if (i.key() == "sounds") {
                sounds = i.value();
                if (sounds.canConvert(QVariant::List)) {
                    QList <QVariant> soundsList = sounds.toList();
                    for (int j = 0; j < soundsList.count(); j++) {
                        if (soundsList.at(j).canConvert(QVariant::Map)) {
                            QMap <QString, QVariant> soundmap = soundsList.at(j).toMap();
                            if (soundmap.contains("original_filename")) {
                                QListWidgetItem *item = new QListWidgetItem(soundmap.value("original_filename").toString(), search_results);
                                item->setData(imageRole, soundmap.value("waveform_m").toString());
                                item->setData(soundUrl, soundmap.value("url").toString());
                                item->setData(durationRole, soundmap.value("duration").toDouble());
                                item->setData(previewRole, soundmap.value("preview-hq-mp3").toString());
                                item->setData(soundRole, soundmap.value("serve").toString() + "?api_key=a1772c8236e945a4bee30a64058dabf8");
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
    search_results->blockSignals(false);
    search_results->setCurrentRow(0);
}

void FreeSound::slotUpdateCurrentSound()
{
    m_currentPreview.clear();
    m_currentUrl.clear();
    QListWidgetItem *item = search_results->currentItem();
    if (!item) {
        sound_box->setEnabled(false);
        return;
    }
    m_currentPreview = item->data(previewRole).toString();
    m_currentUrl = item->data(soundRole).toString();
    button_preview->setEnabled(!m_currentPreview.isEmpty());
    sound_box->setEnabled(true);
    sound_name->setText(item->text());
    sound_name->setUrl(item->data(soundUrl).toString());
    sound_author->setText(item->data(authorRole).toString());
    sound_author->setUrl(item->data(authorUrl).toString());
    sound_duration->setText(QString::number(item->data(durationRole).toDouble()));
    KUrl img(item->data(imageRole).toString());
    if (img.isEmpty()) return;
    if (KIO::NetAccess::exists(img, KIO::NetAccess::SourceSide, this)) {
        QString tmpFile;
        if (KIO::NetAccess::download(img, tmpFile, this)) {
            QPixmap pix(tmpFile);
            sound_image->setPixmap(pix);
            KIO::NetAccess::removeTempFile(tmpFile);
        }
    }
}


void FreeSound::slotPlaySound()
{
    if (m_currentPreview.isEmpty()) return;
    if (m_previewProcess && m_previewProcess->state() != QProcess::NotRunning) {
        m_previewProcess->close();
        return;
    }
    m_previewProcess->start("ffplay", QStringList() << m_currentPreview << "-nodisp");
}

void FreeSound::slotPreviewStatusChanged(QProcess::ProcessState state)
{
    if (state == QProcess::NotRunning)
        button_preview->setText(i18n("Preview"));
    else 
        button_preview->setText(i18n("Stop"));
}

void FreeSound::slotSaveSound()
{
    if (m_currentUrl.isEmpty()) return;
    QString path = m_folder;
    if (!path.endsWith('/')) path.append('/');
    path.append(sound_name->text());
    QString ext = "*." + sound_name->text().section('.', -1);
    QString saveUrl = KFileDialog::getSaveFileName(KUrl(path), ext);
    if (saveUrl.isEmpty()) return;
    if (KIO::NetAccess::download(KUrl(m_currentUrl), saveUrl, this)) {
        emit addClip(KUrl(saveUrl), sound_name->url());
    }
}

void FreeSound::slotOpenUrl(const QString &url)
{
    new KRun(KUrl(url), this);
}
