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


#include "clipstabilize.h"

#include <KDebug>
#include <mlt++/Mlt.h>
#include "kdenlivesettings.h"
#include <KGlobalSettings>
#include <KMessageBox>
#include <KFileDialog>


ClipStabilize::ClipStabilize(KUrl::List urls, const QString &params, QWidget * parent) :
        QDialog(parent), m_urls(urls), m_duration(0)
{
    setFont(KGlobalSettings::toolBarFont());
    setupUi(this);
    setAttribute(Qt::WA_DeleteOnClose);
    log_text->setHidden(true);
    setWindowTitle(i18n("Stabilize Clip"));
    auto_add->setText(i18np("Add clip to project", "Add clips to project", m_urls.count()));
	profile=Mlt::Profile(KdenliveSettings::current_profile().toUtf8().constData());
	filtername=params;

    if (m_urls.count() == 1) {
        QString fileName = m_urls.at(0).path(); //.section('.', 0, -1);
        QString newFile = params.section(' ', -1).replace("%1", fileName);
        KUrl dest(newFile);
        source_url->setUrl(m_urls.at(0));
        dest_url->setMode(KFile::File);
        dest_url->setUrl(dest);
        dest_url->fileDialog()->setOperationMode(KFileDialog::Saving);
        urls_list->setHidden(true);
        connect(source_url, SIGNAL(textChanged(const QString &)), this, SLOT(slotUpdateParams()));
    } else {
        label_source->setHidden(true);
        source_url->setHidden(true);
        label_dest->setText(i18n("Destination folder"));
        dest_url->setMode(KFile::Directory);
        dest_url->setUrl(KUrl(m_urls.at(0).directory()));
        dest_url->fileDialog()->setOperationMode(KFileDialog::Saving);
        for (int i = 0; i < m_urls.count(); i++)
            urls_list->addItem(m_urls.at(i).path());
    }
    if (!params.isEmpty()) {
        label_profile->setHidden(true);
        profile_list->setHidden(true);
        ffmpeg_params->setPlainText(params.simplified());
        /*if (!description.isEmpty()) {
            transcode_info->setText(description);
        } else transcode_info->setHidden(true);*/
    } 

    connect(button_start, SIGNAL(clicked()), this, SLOT(slotStartStabilize()));

    m_stabilizeProcess.setProcessChannelMode(QProcess::MergedChannels);
    connect(&m_stabilizeProcess, SIGNAL(readyReadStandardOutput()), this, SLOT(slotShowStabilizeInfo()));
    connect(&m_stabilizeProcess, SIGNAL(finished(int, QProcess::ExitStatus)), this, SLOT(slotStabilizeFinished(int, QProcess::ExitStatus)));

    adjustSize();
}

ClipStabilize::~ClipStabilize()
{
    if (m_stabilizeProcess.state() != QProcess::NotRunning) {
        m_stabilizeProcess.close();
    }
}

void ClipStabilize::slotStartStabilize()
{
    if (m_stabilizeProcess.state() != QProcess::NotRunning) {
        return;
    }
    m_duration = 0;
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

	Mlt::Producer producer(profile,s_url.toUtf8().data());
	Mlt::Filter filter(profile,filtername.toUtf8().data());
	producer.attach(filter);
	Mlt::Consumer xmlout(profile,"xml");
	xmlout.connect(producer);
	xmlout.run();
    //m_stabilizeProcess.start("ffmpeg", parameters);
    button_start->setEnabled(false);

}

void ClipStabilize::slotShowStabilizeInfo()
{
    QString log = QString(m_stabilizeProcess.readAll());
    int progress;
    if (m_duration == 0) {
        if (log.contains("Duration:")) {
            QString data = log.section("Duration:", 1, 1).section(',', 0, 0).simplified();
            QStringList numbers = data.split(':');
            m_duration = numbers.at(0).toInt() * 3600 + numbers.at(1).toInt() * 60 + numbers.at(2).toDouble();
            log_text->setHidden(true);
            job_progress->setHidden(false);
        }
        else {
            log_text->setHidden(false);
            job_progress->setHidden(true);
        }
    }
    else if (log.contains("time=")) {
        QString time = log.section("time=", 1, 1).simplified().section(' ', 0, 0);
        if (time.contains(':')) {
            QStringList numbers = time.split(':');
            progress = numbers.at(0).toInt() * 3600 + numbers.at(1).toInt() * 60 + numbers.at(2).toDouble();
        }
        else progress = (int) time.toDouble();
        kDebug()<<"// PROGRESS: "<<progress<<", "<<m_duration;
        job_progress->setValue((int) (100.0 * progress / m_duration));
    }
    //kDebug() << "//LOG: " << log;
    log_text->setPlainText(log);
}

void ClipStabilize::slotStabilizeFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    buttonBox->button(QDialogButtonBox::Abort)->setText(i18n("Close"));
    button_start->setEnabled(true);
    m_duration = 0;

    if (exitCode == 0 && exitStatus == QProcess::NormalExit) {
        log_text->setHtml(log_text->toPlainText() + "<br /><b>" + i18n("Transcoding finished."));
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
            m_stabilizeProcess.close();
            slotStartStabilize();
            return;
        } else if (auto_close->isChecked()) accept();
    } else {
        log_text->setHtml(log_text->toPlainText() + "<br /><b>" + i18n("Transcoding FAILED!"));
    }

    m_stabilizeProcess.close();
}

void ClipStabilize::slotUpdateParams(int ix)
{
    QString fileName = source_url->url().path();
    if (ix != -1) {
        QString params = profile_list->itemData(ix).toString();
        ffmpeg_params->setPlainText(params.simplified());
        QString desc = profile_list->itemData(ix, Qt::UserRole + 1).toString();
        if (!desc.isEmpty()) {
            transcode_info->setText(desc);
            transcode_info->setHidden(false);
        } else transcode_info->setHidden(true);
    }
    if (urls_list->count() == 0) {
        QString newFile = ffmpeg_params->toPlainText().simplified().section(' ', -1).replace("%1", fileName);
        dest_url->setUrl(KUrl(newFile));
    }

}

#include "clipstabilize.moc"


