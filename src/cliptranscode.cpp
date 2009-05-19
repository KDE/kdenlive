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


ClipTranscode::ClipTranscode(const KUrl &src, const QString &params, QWidget * parent) :
        QDialog(parent)
{
    setFont(KGlobalSettings::toolBarFont());
    m_view.setupUi(this);
    setAttribute(Qt::WA_DeleteOnClose);
    setWindowTitle(i18n("Transcode Clip"));

    QString fileName = src.path(); //.section('.', 0, -1);
    QString newFile = params.section(' ', -1).replace("%1", fileName);
    KUrl dest(newFile);
    m_view.source_url->setUrl(src);
    m_view.dest_url->setUrl(dest);
    connect(m_view.source_url, SIGNAL(textChanged(const QString &)), this, SLOT(slotUpdateParams()));
    if (!params.isEmpty()) {
        m_view.label_profile->setHidden(true);
        m_view.profile_list->setHidden(true);
        m_view.params->setPlainText(params.simplified());
    } else {
        // load Profiles
        KSharedConfigPtr config = KGlobal::config();
        KConfigGroup transConfig(config, "Transcoding");
        // read the entries
        QMap< QString, QString > profiles = transConfig.entryMap();
        QMapIterator<QString, QString> i(profiles);
        connect(m_view.profile_list, SIGNAL(currentIndexChanged(int)), this, SLOT(slotUpdateParams(int)));
        while (i.hasNext()) {
            i.next();
            m_view.profile_list->addItem(i.key(), i.value());
        }
    }

    kDebug() << "//PARAMS: " << params << "\n\nNAME: " << newFile;

    connect(m_view.button_start, SIGNAL(clicked()), this, SLOT(slotStartTransCode()));

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
    parameters << "-i" << m_view.source_url->url().path();

    if (QFile::exists(m_view.dest_url->url().path())) {
        if (KMessageBox::questionYesNo(this, i18n("File %1 already exists.\nDo you want to overwrite it?", m_view.dest_url->url().path())) == KMessageBox::No) return;
        parameters << "-y";
    }
    m_view.buttonBox->button(QDialogButtonBox::Abort)->setText(i18n("Abort"));
    QString params = m_view.params->toPlainText().simplified();
    params = params.section(' ', 0, -2);
    parameters << params.split(' ') << m_view.dest_url->url().path();

    kDebug() << "/// FFMPEG ARGS: " << parameters;

    m_transcodeProcess.start("ffmpeg", parameters);
    m_view.button_start->setEnabled(false);

}

void ClipTranscode::slotShowTranscodeInfo()
{
    QString log = QString(m_transcodeProcess.readAll());
    //kDebug() << "//LOG: " << log;
    m_view.log->setPlainText(log);
}

void ClipTranscode::slotTranscodeFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    m_view.buttonBox->button(QDialogButtonBox::Abort)->setText(i18n("Close"));
    m_view.button_start->setEnabled(true);

    if (exitCode == 0 && exitStatus == QProcess::NormalExit) {
        m_view.log->setHtml(m_view.log->toPlainText() + "<br><b>" + i18n("Transcoding finished."));
        if (m_view.auto_add->isChecked()) emit addClip(m_view.dest_url->url());
        if (m_view.auto_close->isChecked()) accept();
    } else {
        m_view.log->setHtml(m_view.log->toPlainText() + "<br><b>" + i18n("Transcoding FAILED!"));
    }

    m_transcodeProcess.close();
}

void ClipTranscode::slotUpdateParams(int ix)
{
    if (ix == -1) ix = m_view.profile_list->currentIndex();
    QString fileName = m_view.source_url->url().path();
    QString params = m_view.profile_list->itemData(ix).toString();
    QString newFile = params.section(' ', -1).replace("%1", fileName);
    KUrl dest(newFile);
    m_view.dest_url->setUrl(dest);
    m_view.params->setPlainText(params.simplified());
}

#include "cliptranscode.moc"


