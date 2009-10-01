/***************************************************************************
 *   Copyright (C) 2008 by Jean-Baptiste Mardelle (jb@kdenlive.org)        *
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


#include "cliptranscode.h"

#include <KDebug>
#include <KGlobalSettings>
#include <KMessageBox>
#include <KFileDialog>


ClipTranscode::ClipTranscode(KUrl::List urls, const QString &params, QWidget * parent) :
        QDialog(parent), m_urls(urls)
{
    setFont(KGlobalSettings::toolBarFont());
    setupUi(this);
    setAttribute(Qt::WA_DeleteOnClose);
    setWindowTitle(i18n("Transcode Clip"));
    auto_add->setText(i18np("Add clip to project", "Add clips to project", m_urls.count()));
    dest_url->fileDialog()->setOperationMode(KFileDialog::Saving);

    if (m_urls.count() == 1) {
        QString fileName = m_urls.at(0).path(); //.section('.', 0, -1);
        QString newFile = params.section(' ', -1).replace("%1", fileName);
        KUrl dest(newFile);
        source_url->setUrl(m_urls.at(0));
        dest_url->setUrl(dest);
        urls_list->setHidden(true);
        connect(source_url, SIGNAL(textChanged(const QString &)), this, SLOT(slotUpdateParams()));
    } else {
        label_source->setHidden(true);
        source_url->setHidden(true);
        label_dest->setText(i18n("Destination folder"));
        dest_url->setMode(KFile::Directory);
        dest_url->setUrl(KUrl(m_urls.at(0).directory()));
        for (int i = 0; i < m_urls.count(); i++)
            urls_list->addItem(m_urls.at(i).path());
    }
    if (!params.isEmpty()) {
        label_profile->setHidden(true);
        profile_list->setHidden(true);
        ffmpeg_params->setPlainText(params.simplified());
    } else {
        // load Profiles
        KSharedConfigPtr config = KSharedConfig::openConfig("kdenlivetranscodingrc");
        KConfigGroup transConfig(config, "Transcoding");
        // read the entries
        QMap< QString, QString > profiles = transConfig.entryMap();
        QMapIterator<QString, QString> i(profiles);
        connect(profile_list, SIGNAL(currentIndexChanged(int)), this, SLOT(slotUpdateParams(int)));
        while (i.hasNext()) {
            i.next();
            profile_list->addItem(i.key(), i.value());
        }
    }

    connect(button_start, SIGNAL(clicked()), this, SLOT(slotStartTransCode()));

    m_transcodeProcess.setProcessChannelMode(QProcess::MergedChannels);
    connect(&m_transcodeProcess, SIGNAL(readyReadStandardOutput()), this, SLOT(slotShowTranscodeInfo()));
    connect(&m_transcodeProcess, SIGNAL(finished(int, QProcess::ExitStatus)), this, SLOT(slotTranscodeFinished(int, QProcess::ExitStatus)));

    //adjustSize();
}

ClipTranscode::~ClipTranscode()
{
    if (m_transcodeProcess.state() != QProcess::NotRunning) {
        m_transcodeProcess.close();
    }
}

void ClipTranscode::slotStartTransCode()
{
    if (m_transcodeProcess.state() != QProcess::NotRunning) {
        return;
    }
    QStringList parameters;
    QString destination;
    QString params = ffmpeg_params->toPlainText().simplified();
    if (urls_list->count() > 0) {
        source_url->setUrl(m_urls.takeFirst());
        destination = dest_url->url().path(KUrl::AddTrailingSlash) + source_url->url().fileName();
        QList<QListWidgetItem *> matching = urls_list->findItems(source_url->url().path(), Qt::MatchExactly);
        if (matching.count() > 0) {
            matching.at(0)->setFlags(Qt::ItemIsSelectable);
            urls_list->setCurrentItem(matching.at(0));
        }
    } else {
        destination = dest_url->url().path().section('.', 0, -2);
    }
    QString extension = params.section("%1", 1, 1).section(' ', 0, 0);
    QString s_url = source_url->url().path();

    parameters << "-i" << s_url;
    if (QFile::exists(destination + extension)) {
        if (KMessageBox::questionYesNo(this, i18n("File %1 already exists.\nDo you want to overwrite it?", destination + extension)) == KMessageBox::No) return;
        parameters << "-y";
    }
    foreach(QString s, params.split(' '))
    parameters << s.replace("%1", destination);
    buttonBox->button(QDialogButtonBox::Abort)->setText(i18n("Abort"));

    //kDebug() << "/// FFMPEG ARGS: " << parameters;

    m_transcodeProcess.start("ffmpeg", parameters);
    button_start->setEnabled(false);

}

void ClipTranscode::slotShowTranscodeInfo()
{
    QString log = QString(m_transcodeProcess.readAll());
    //kDebug() << "//LOG: " << log;
    log_text->setPlainText(log);
}

void ClipTranscode::slotTranscodeFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    buttonBox->button(QDialogButtonBox::Abort)->setText(i18n("Close"));
    button_start->setEnabled(true);

    if (exitCode == 0 && exitStatus == QProcess::NormalExit) {
        log_text->setHtml(log_text->toPlainText() + "<br><b>" + i18n("Transcoding finished."));
        if (auto_add->isChecked()) {
            KUrl url;
            if (urls_list->count() > 0) {
                QString params = ffmpeg_params->toPlainText().simplified();
                QString extension = params.section("%1", 1, 1).section(' ', 0, 0);
                url = KUrl(dest_url->url().path(KUrl::AddTrailingSlash) + source_url->url().fileName() + extension);
            } else url = dest_url->url();
            emit addClip(url);
        }
        if (urls_list->count() > 0 && m_urls.count() > 0) {
            m_transcodeProcess.close();
            slotStartTransCode();
            return;
        } else if (auto_close->isChecked()) accept();
    } else {
        log_text->setHtml(log_text->toPlainText() + "<br><b>" + i18n("Transcoding FAILED!"));
    }

    m_transcodeProcess.close();
}

void ClipTranscode::slotUpdateParams(int ix)
{
    QString fileName = source_url->url().path();
    if (ix != -1) {
        QString params = profile_list->itemData(ix).toString();
        ffmpeg_params->setPlainText(params.simplified());
    }
    if (urls_list->count() == 0) {
        QString newFile = ffmpeg_params->toPlainText().simplified().section(' ', -1).replace("%1", fileName);
        dest_url->setUrl(KUrl(newFile));
    }

}

#include "cliptranscode.moc"


