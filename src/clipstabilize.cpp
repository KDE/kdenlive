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
#include <QtConcurrentRun>
#include <QTimer>
#include <KFileDialog>


ClipStabilize::ClipStabilize(KUrl::List urls, const QString &params, QWidget * parent) :
        QDialog(parent), m_urls(urls), m_duration(0),m_profile(NULL),m_consumer(NULL),m_playlist(NULL)
{
    setFont(KGlobalSettings::toolBarFont());
    setupUi(this);
    setAttribute(Qt::WA_DeleteOnClose);
    log_text->setHidden(true);
    setWindowTitle(i18n("Stabilize Clip"));
    auto_add->setText(i18np("Add clip to project", "Add clips to project", m_urls.count()));
	m_profile = new Mlt::Profile(KdenliveSettings::current_profile().toUtf8().constData());
	filtername=params;

    if (m_urls.count() == 1) {
        QString fileName = m_urls.at(0).path(); //.section('.', 0, -1);
        QString newFile = fileName.append(".mlt");
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
        //ffmpeg_params->setPlainText(params.simplified());
        /*if (!description.isEmpty()) {
            transcode_info->setText(description);
        } else transcode_info->setHidden(true);*/
    } 

    connect(button_start, SIGNAL(clicked()), this, SLOT(slotStartStabilize()));
	connect(buttonBox,SIGNAL(rejected()), this, SLOT(slotAbortStabilize()));

	m_timer=new QTimer(this);
	connect(m_timer, SIGNAL(timeout()), this, SLOT(slotShowStabilizeInfo()));

    adjustSize();
}

ClipStabilize::~ClipStabilize()
{
    /*if (m_stabilizeProcess.state() != QProcess::NotRunning) {
        m_stabilizeProcess.close();
    }*/
	if (m_profile) free (m_profile);
	if (m_consumer) free (m_consumer);
	if (m_playlist) free (m_playlist);
}

void ClipStabilize::slotStartStabilize()
{
    if (m_consumer && !m_consumer->is_stopped()) {
        return;
    }
    m_duration = 0;
    QStringList parameters;
    QString destination;
    QString params = ffmpeg_params->toPlainText().simplified();
    if (urls_list->count() > 0) {
        source_url->setUrl(m_urls.takeFirst());
        destination = dest_url->url().path();
        QList<QListWidgetItem *> matching = urls_list->findItems(source_url->url().path(), Qt::MatchExactly);
        if (matching.count() > 0) {
            matching.at(0)->setFlags(Qt::ItemIsSelectable);
            urls_list->setCurrentItem(matching.at(0));
        }
    } else {
        destination = dest_url->url().path();
    }
    QString s_url = source_url->url().path();

    if (QFile::exists(destination)) {
        if (KMessageBox::questionYesNo(this, i18n("File %1 already exists.\nDo you want to overwrite it?", destination )) == KMessageBox::No) return;
    }
    buttonBox->button(QDialogButtonBox::Abort)->setText(i18n("Abort"));

	if (m_profile){
		qDebug() << m_profile->description();
		m_playlist= new Mlt::Playlist;
		Mlt::Filter filter(*m_profile,filtername.toUtf8().data());
		Mlt::Producer p(*m_profile,s_url.toUtf8().data());
		m_playlist->append(p);
		m_playlist->attach(filter);
		//m_producer->set("out",20);
		m_consumer= new Mlt::Consumer(*m_profile,"xml",destination.toUtf8().data());
		m_consumer->set("all","1");
		m_consumer->set("real_time","-2");
		m_consumer->connect(*m_playlist);
		QtConcurrent::run(this, &ClipStabilize::slotRunStabilize);
		button_start->setEnabled(false);
	}

}

void ClipStabilize::slotRunStabilize()
{
	if (m_consumer)
	{
		m_timer->start(500);
		m_consumer->run();
	}
}

void ClipStabilize::slotAbortStabilize()
{
	if (m_consumer)
	{
		m_timer->stop();
		m_consumer->stop();
		slotStabilizeFinished(false);
	}
}

void ClipStabilize::slotShowStabilizeInfo()
{
	if (m_playlist){
        job_progress->setValue((int) (100.0 * m_consumer->position()/m_playlist->get_out() ));
		if (m_consumer->position()==m_playlist->get_out()){
			m_timer->stop();
			slotStabilizeFinished(true);
		}
	}
}

void ClipStabilize::slotStabilizeFinished(bool success)
{
    buttonBox->button(QDialogButtonBox::Abort)->setText(i18n("Close"));
    button_start->setEnabled(true);
    m_duration = 0;

    if (success) {
        log_text->setHtml(log_text->toPlainText() + "<br /><b>" + i18n("Stabilize finished."));
        if (auto_add->isChecked()) {
            KUrl url;
            if (urls_list->count() > 0) {
                url = KUrl(dest_url->url());
            } else url = dest_url->url();
            emit addClip(url);
        }
        if (urls_list->count() > 0 && m_urls.count() > 0) {
            slotStartStabilize();
            return;
        } else if (auto_close->isChecked()) accept();
    } else {
        log_text->setHtml(log_text->toPlainText() + "<br /><b>" + i18n("Stabilizing FAILED!"));
    }

}


#include "clipstabilize.moc"


