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
#include "kdenlivesettings.h"
#include "kxmlgui_version.h"

#include <QFontDatabase>
#include <QStandardPaths>

#include <KMessageBox>
#include <klocalizedstring.h>

ClipTranscode::ClipTranscode(QStringList urls, const QString &params, QStringList postParams, const QString &description, QString folderInfo,
                             bool automaticMode, QWidget *parent)
    : QDialog(parent)
    , m_urls(std::move(urls))
    , m_folderInfo(std::move(folderInfo))
    , m_duration(0)
    , m_automaticMode(automaticMode)
    , m_postParams(std::move(postParams))
{
    setFont(QFontDatabase::systemFont(QFontDatabase::SmallestReadableFont));
    setupUi(this);
    setAttribute(Qt::WA_DeleteOnClose);
    m_infoMessage = new KMessageWidget;
    auto *s = static_cast<QGridLayout *>(layout());
    s->addWidget(m_infoMessage, 10, 0, 1, -1);
    m_infoMessage->setCloseButtonVisible(false);
    m_infoMessage->hide();
    log_text->setHidden(true);
    setWindowTitle(i18n("Transcode Clip"));
    if (m_automaticMode) {
        auto_add->setHidden(true);
    }
    auto_add->setText(i18np("Add clip to project", "Add clips to project", m_urls.count()));
    auto_add->setChecked(KdenliveSettings::add_new_clip());

    if (m_urls.count() == 1) {
        QString fileName = m_urls.constFirst();
        source_url->setUrl(QUrl::fromLocalFile(fileName));
        dest_url->setMode(KFile::File);
        dest_url->setAcceptMode(QFileDialog::AcceptSave);
        if (!params.isEmpty()) {
            QString newFile = params.section(QLatin1Char(' '), -1).replace(QLatin1String("%1"), fileName);
            QUrl dest = QUrl::fromLocalFile(newFile);
            dest_url->setUrl(dest);
        }
        urls_list->setHidden(true);
        connect(source_url, SIGNAL(textChanged(QString)), this, SLOT(slotUpdateParams()));
        ffmpeg_params->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::MinimumExpanding);
    } else {
        label_source->setHidden(true);
        source_url->setHidden(true);
        label_dest->setText(i18n("Destination folder"));
        dest_url->setMode(KFile::Directory);
        dest_url->setUrl(QUrl::fromLocalFile(m_urls.constFirst()).adjusted(QUrl::RemoveFilename));
        dest_url->setMode(KFile::Directory | KFile::ExistingOnly);
        for (int i = 0; i < m_urls.count(); ++i) {
            urls_list->addItem(m_urls.at(i));
        }
        urls_list->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::MinimumExpanding);
    }
    if (!params.isEmpty()) {
        label_profile->setHidden(true);
        profile_list->setHidden(true);
        ffmpeg_params->setPlainText(params.simplified());
        if (!description.isEmpty()) {
            transcode_info->setText(description);
        } else {
            transcode_info->setHidden(true);
        }
    } else {
        // load Profiles
        KSharedConfigPtr config =
            KSharedConfig::openConfig(QStandardPaths::locate(QStandardPaths::AppDataLocation, QStringLiteral("kdenlivetranscodingrc")), KConfig::CascadeConfig);
        KConfigGroup transConfig(config, "Transcoding");
        // read the entries
        QMap<QString, QString> profiles = transConfig.entryMap();
        QMapIterator<QString, QString> i(profiles);
        while (i.hasNext()) {
            i.next();
            QStringList list = i.value().split(QLatin1Char(';'));
            profile_list->addItem(i.key(), list.at(0));
            if (list.count() > 1) {
                profile_list->setItemData(profile_list->count() - 1, list.at(1), Qt::UserRole + 1);
            }
        }
        connect(profile_list, SIGNAL(currentIndexChanged(int)), this, SLOT(slotUpdateParams(int)));
        slotUpdateParams(0);
    }

    connect(button_start, &QAbstractButton::clicked, this, &ClipTranscode::slotStartTransCode);

    m_transcodeProcess.setProcessChannelMode(QProcess::MergedChannels);
    connect(&m_transcodeProcess, &QProcess::readyReadStandardOutput, this, &ClipTranscode::slotShowTranscodeInfo);
    connect(&m_transcodeProcess, static_cast<void (QProcess::*)(int, QProcess::ExitStatus)>(&QProcess::finished), this, &ClipTranscode::slotTranscodeFinished);

    //ffmpeg_params->setMaximumHeight(QFontMetrics(font()).lineSpacing() * 5);

    adjustSize();
}

ClipTranscode::~ClipTranscode()
{
    KdenliveSettings::setAdd_new_clip(auto_add->isChecked());
    if (m_transcodeProcess.state() != QProcess::NotRunning) {
        m_transcodeProcess.close();
    }
    delete m_infoMessage;
}

void ClipTranscode::slotStartTransCode()
{
    if (m_transcodeProcess.state() != QProcess::NotRunning) {
        return;
    }
    if (KdenliveSettings::ffmpegpath().isEmpty()) {
        // FFmpeg not detected, cannot process the Job
        log_text->setPlainText(i18n("FFmpeg not found, please set path in Kdenlive's settings Environment"));
        slotTranscodeFinished(1, QProcess::CrashExit);
        return;
    }
    m_duration = 0;
    m_destination.clear();
    m_infoMessage->animatedHide();
    QStringList parameters;
    QString destination;
    QString params = ffmpeg_params->toPlainText().simplified();
    if (!m_urls.isEmpty() && urls_list->count() > 0) {
        // We are processing multiple clips
        source_url->setUrl(QUrl::fromLocalFile(m_urls.takeFirst()));
        destination = QDir(dest_url->url().toLocalFile()).absoluteFilePath(source_url->url().fileName());
        QList<QListWidgetItem *> matching = urls_list->findItems(source_url->url().toLocalFile(), Qt::MatchExactly);
        if (!matching.isEmpty()) {
            matching.at(0)->setFlags(Qt::ItemIsSelectable);
            urls_list->setCurrentItem(matching.at(0));
        }
    } else {
        destination = dest_url->url().toLocalFile().section(QLatin1Char('.'), 0, -2);
    }
    QString extension = params.section(QStringLiteral("%1"), 1, 1).section(QLatin1Char(' '), 0, 0);
    QString s_url = source_url->url().toLocalFile();
    bool mltEncoding = s_url.endsWith(QLatin1String(".mlt")) || s_url.endsWith(QLatin1String(".kdenlive"));
    
    if (QFile::exists(destination + extension)) {
        if (KMessageBox::questionYesNo(this, i18n("File %1 already exists.\nDo you want to overwrite it?", destination + extension)) == KMessageBox::No) {
            // Abort operation
            if (m_automaticMode) {
                // inform caller that we aborted
                emit transcodedClip(source_url->url(), QUrl());
                close();
            }
            return;
        }
        if (!mltEncoding) {
            parameters << QStringLiteral("-y");
        }
    }
    
    if (mltEncoding) {
        params.replace(QStringLiteral("%1"), QString("-consumer %1"));
#if QT_VERSION < QT_VERSION_CHECK(5, 15, 0)
        const QStringList splitted = params.split(QLatin1Char('-'), QString::SkipEmptyParts);
#else
        const QStringList splitted = params.split(QLatin1Char('-'), Qt::SkipEmptyParts);
#endif
        for (const QString &s : splitted) {
            QString t = s.simplified();
            if (t.count(QLatin1Char(' ')) == 0) {
                t.append(QLatin1String("=1"));
            } else {
                if (t.contains(QLatin1String("%1"))) {
                    // file name
                    parameters.prepend(t.section(QLatin1Char(' '), 1).replace(QLatin1String("%1"), QString("avformat:%1").arg(destination)));
                    parameters.prepend(QStringLiteral("-consumer"));
                    continue;
                }
                if (t.startsWith(QLatin1String("aspect "))) {
                    // Fix aspect ratio calculation
                    t.replace(QLatin1Char(' '), QLatin1String("=@"));
                    t.replace(QLatin1Char(':'), QLatin1String("/"));
                } else {
                    t.replace(QLatin1Char(' '), QLatin1String("="));
                }
            }
            parameters << t;
        }
        parameters.prepend(s_url);
        buttonBox->button(QDialogButtonBox::Abort)->setText(i18n("Abort"));
        m_destination = destination + extension;
        m_transcodeProcess.start(KdenliveSettings::rendererpath(), parameters);
        source_url->setEnabled(false);
        dest_url->setEnabled(false);
        button_start->setEnabled(false);
        return;
    }
    if (params.contains(QLatin1String("-i "))) {
        // Filename must be inserted later
    } else {
        parameters << QStringLiteral("-i") << s_url;
    }

    bool replaceVfParams = false;
    const QStringList splitted = params.split(QLatin1Char(' '));
    for (QString s : splitted) {
        if (replaceVfParams) {
            parameters << m_postParams.at(1);
            replaceVfParams = false;
        } else if (s.startsWith(QLatin1String("%1"))) {
            parameters << s.replace(0, 2, destination);
        } else if (!m_postParams.isEmpty() && s == QLatin1String("-vf")) {
            replaceVfParams = true;
            parameters << s;
        } else if (s == QLatin1String("-i")) {
            parameters << s;
            parameters << s_url;
        } else {
            parameters << s;
        }
    }
    buttonBox->button(QDialogButtonBox::Abort)->setText(i18n("Abort"));
    m_destination = destination + extension;
    m_transcodeProcess.start(KdenliveSettings::ffmpegpath(), parameters);
    source_url->setEnabled(false);
    dest_url->setEnabled(false);
    button_start->setEnabled(false);
}

void ClipTranscode::slotShowTranscodeInfo()
{
    QString log = QString::fromLatin1(m_transcodeProcess.readAll());
    if (m_duration == 0) {
        if (log.contains(QStringLiteral("Duration:"))) {
            QString duration = log.section(QStringLiteral("Duration:"), 1, 1).section(QLatin1Char(','), 0, 0).simplified();
            QStringList numbers = duration.split(QLatin1Char(':'));
            if (numbers.size() < 3) {
                return;
            }
            m_duration = numbers.at(0).toInt() * 3600 + numbers.at(1).toInt() * 60 + numbers.at(2).toDouble();
            log_text->setHidden(true);
            job_progress->setHidden(false);
        } else {
            log_text->setHidden(false);
            job_progress->setHidden(true);
        }
    } else if (log.contains(QStringLiteral("time="))) {
        int progress;
        QString time = log.section(QStringLiteral("time="), 1, 1).simplified().section(QLatin1Char(' '), 0, 0);
        if (time.contains(QLatin1Char(':'))) {
            QStringList numbers = time.split(QLatin1Char(':'));
            if (numbers.size() < 3) {
                return;
            }
            progress = numbers.at(0).toInt() * 3600 + numbers.at(1).toInt() * 60 + numbers.at(2).toDouble();
        } else {
            progress = (int)time.toDouble();
        }
        job_progress->setValue((int)(100.0 * progress / m_duration));
    }
    log_text->setPlainText(log);
}

void ClipTranscode::slotTranscodeFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    buttonBox->button(QDialogButtonBox::Abort)->setText(i18n("Close"));
    button_start->setEnabled(true);
    source_url->setEnabled(true);
    dest_url->setEnabled(true);
    m_duration = 0;

    if (QFileInfo(m_destination).size() <= 0) {
        // Destination file does not exist, transcoding failed
        exitCode = 1;
    }
    if (exitCode == 0 && exitStatus == QProcess::NormalExit) {
        log_text->setHtml(log_text->toPlainText() + QStringLiteral("<br /><b>") + i18n("Transcoding finished."));
        if (auto_add->isChecked() || m_automaticMode) {
            QUrl url;
            if (urls_list->count() > 0) {
                QString params = ffmpeg_params->toPlainText().simplified();
                QString extension = params.section(QStringLiteral("%1"), 1, 1).section(QLatin1Char(' '), 0, 0);
                url = QUrl::fromLocalFile(dest_url->url().toLocalFile() + QDir::separator() + source_url->url().fileName() + extension);
            } else {
                url = dest_url->url();
            }
            if (m_automaticMode) {
                emit transcodedClip(source_url->url(), url);
            } else {
                emit addClip(url, m_folderInfo);
            }
        }
        if (urls_list->count() > 0 && m_urls.count() > 0) {
            m_transcodeProcess.close();
            slotStartTransCode();
            return;
        }
        if (auto_close->isChecked()) {
            accept();
        } else {
            m_infoMessage->setMessageType(KMessageWidget::Positive);
            m_infoMessage->setText(i18n("Transcoding finished."));
            m_infoMessage->animatedShow();
        }
    } else {
        m_infoMessage->setMessageType(KMessageWidget::Warning);
        m_infoMessage->setText(i18n("Transcoding failed"));
        m_infoMessage->animatedShow();
        log_text->setVisible(true);
    }
    m_transcodeProcess.close();

    // Refill url list in case user wants to transcode to another format
    if (urls_list->count() > 0) {
        m_urls.clear();
        for (int i = 0; i < urls_list->count(); ++i) {
            m_urls << urls_list->item(i)->text();
        }
    }
}

void ClipTranscode::slotUpdateParams(int ix)
{
    QString fileName = source_url->url().toLocalFile();
    if (ix != -1) {
        QString params = profile_list->itemData(ix).toString();
        ffmpeg_params->setPlainText(params.simplified());
        QString desc = profile_list->itemData(ix, Qt::UserRole + 1).toString();
        if (!desc.isEmpty()) {
            transcode_info->setText(desc);
            transcode_info->setHidden(false);
        } else {
            transcode_info->setHidden(true);
        }
    }
    if (urls_list->count() == 0) {
        QString newFile = ffmpeg_params->toPlainText().simplified().section(QLatin1Char(' '), -1).replace(QLatin1String("%1"), fileName);
        dest_url->setUrl(QUrl::fromLocalFile(newFile));
    }
}
