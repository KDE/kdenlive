/***************************************************************************
 *                                                                         *
 *   Copyright (C) 2011 by Jean-Baptiste Mardelle (jb@kdenlive.org)        *
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

#include "cutclipjob.h"
#include "bin/bin.h"
#include "bin/clipcreator.hpp"
#include "jobmanager.h"
#include "bin/projectclip.h"
#include "bin/projectitemmodel.h"
#include "core.h"
#include "doc/kdenlivedoc.h"
#include "kdenlive_debug.h"
#include "kdenlivesettings.h"
#include "macros.hpp"

#include "ui_cutjobdialog_ui.h"

#include <klocalizedstring.h>
#include <KIO/RenameDialog>
#include <KLineEdit>

#include <QApplication>
#include <QDialog>
#include <QPointer>

CutClipJob::CutClipJob(const QString &binId, const QString sourcePath, GenTime inTime, GenTime outTime, const QString destPath, QStringList encodingParams)
    : AbstractClipJob(CUTJOB, binId)
    , m_sourceUrl(sourcePath)
    , m_destUrl(destPath)
    , m_done(false)
    , m_jobProcess(nullptr)
    , m_in(inTime)
    , m_out(outTime)
    , m_encodingParams(encodingParams)
    , m_jobDuration((int)(outTime - inTime).seconds())
{
}

const QString CutClipJob::getDescription() const
{
    return i18n("Extract Clip Zone");
}

// static
int CutClipJob::prepareJob(const std::shared_ptr<JobManager> &ptr, const std::vector<QString> &binIds, int parentId, QString undoString, GenTime inTime, GenTime outTime)
{
    if (binIds.empty()) {
        return -1;
    }
    const QString mainId = *binIds.begin();
    auto binClip = pCore->projectItemModel()->getClipByBinID(mainId);
    ClipType::ProducerType type = binClip->clipType();
    if (type != ClipType::AV && type != ClipType::Audio && type != ClipType::Video) {
        //m_errorMessage.prepend(i18n("Cannot extract zone for this clip type."));
        return -1;
    }
    if (KdenliveSettings::ffmpegpath().isEmpty()) {
        // FFmpeg not detected, cannot process the Job
        //m_errorMessage.prepend(i18n("Failed to create cut. FFmpeg not found, please set path in Kdenlive's settings Environment"));
        return -1;
    }
    const QString source = binClip->url();
    QString transcoderExt = source.section(QLatin1Char('.'), -1);
    QFileInfo finfo(source);
    QString fileName = finfo.fileName().section(QLatin1Char('.'), 0, -2);
    QDir dir = finfo.absoluteDir();
    QString inString = QString::number((int)inTime.seconds());
    QString outString = QString::number((int)outTime.seconds());
    QString path = dir.absoluteFilePath(fileName + QString("-%1-%2.").arg(inString, outString) + transcoderExt);

    QPointer<QDialog> d = new QDialog(QApplication::activeWindow());
    Ui::CutJobDialog_UI ui;
    ui.setupUi(d);
    ui.extra_params->setVisible(false);
    ui.add_clip->setChecked(KdenliveSettings::add_new_clip());
    ui.file_url->setMode(KFile::File);
    ui.extra_params->setMaximumHeight(QFontMetrics(QApplication::font()).lineSpacing() * 5);
    ui.file_url->setUrl(QUrl::fromLocalFile(path));
    QFontMetrics fm = ui.file_url->lineEdit()->fontMetrics();
    ui.file_url->setMinimumWidth(fm.boundingRect(ui.file_url->text().left(50)).width() * 1.4);
    ui.button_more->setIcon(QIcon::fromTheme(QStringLiteral("configure")));
    ui.extra_params->setPlainText(QStringLiteral("-acodec copy -vcodec copy"));
    QString mess = i18n("Extracting %1 out of %2", Timecode::getStringTimecode((outTime -inTime).frames(pCore->getCurrentFps()), pCore->getCurrentFps(), true), binClip->getStringDuration());
    ui.info_label->setText(mess);
    if (d->exec() != QDialog::Accepted) {
        delete d;
        return -1;
    }
    path = ui.file_url->url().toLocalFile();
#if QT_VERSION < QT_VERSION_CHECK(5, 15, 0)
    QStringList encodingParams = ui.extra_params->toPlainText().split(QLatin1Char(' '), QString::SkipEmptyParts);
#else
    QStringList encodingParams = ui.extra_params->toPlainText().split(QLatin1Char(' '), Qt::SkipEmptyParts);
#endif
    KdenliveSettings::setAdd_new_clip(ui.add_clip->isChecked());
    delete d;

    if (QFile::exists(path)) {
        KIO::RenameDialog renameDialog(qApp->activeWindow(), i18n("File already exists"), QUrl::fromLocalFile(path), QUrl::fromLocalFile(path), KIO::RenameDialog_Option::RenameDialog_Overwrite );
        if (renameDialog.exec() != QDialog::Rejected) {
            QUrl final = renameDialog.newDestUrl();
            if (final.isValid()) {
                path = final.toLocalFile();
            }
        } else {
            return -1;
        }
    }

    return ptr->startJob_noprepare<CutClipJob>(binIds, parentId, std::move(undoString), source, inTime, outTime, dir.absoluteFilePath(path), encodingParams);

    //return ptr->startJob_noprepare<CutClipJob>(binIds, parentId, std::move(undoString), mainId, source, inTime, outTime, dir.absoluteFilePath(path), encodingParams);
    //return ptr->startJob<CutClipJob>(binIds, parentId, std::move(undoString), std::make_shared<CutClipJob>(mainId, source, inTime, outTime, dir.absoluteFilePath(path)));
}

bool CutClipJob::startJob()
{
    bool result;
    if (m_destUrl == m_sourceUrl) {
        m_errorMessage.append(i18n("You cannot overwrite original clip."));
        m_done = true;
        return false;
    }
    QStringList params = {QStringLiteral("-y"),QStringLiteral("-stats"),QStringLiteral("-v"),QStringLiteral("error"),QStringLiteral("-noaccurate_seek"),QStringLiteral("-ss"),QString::number(m_in.seconds()),QStringLiteral("-i"),m_sourceUrl, QStringLiteral("-t"), QString::number((m_out-m_in).seconds()),QStringLiteral("-avoid_negative_ts"),QStringLiteral("make_zero")};
    params << m_encodingParams << m_destUrl;
    m_jobProcess = std::make_unique<QProcess>(new QProcess);
    connect(m_jobProcess.get(), &QProcess::readyReadStandardError, this, &CutClipJob::processLogInfo);
    connect(this, &CutClipJob::jobCanceled, m_jobProcess.get(), &QProcess::kill, Qt::DirectConnection);
    m_jobProcess->start(KdenliveSettings::ffmpegpath(), params, QIODevice::ReadOnly);
    m_jobProcess->waitForFinished(-1);
    result = m_jobProcess->exitStatus() == QProcess::NormalExit;
    // remove temporary playlist if it exists
    if (result) {
        if (QFileInfo(m_destUrl).size() == 0) {
            QFile::remove(m_destUrl);
            // File was not created
            m_done = false;
            m_errorMessage.append(i18n("Failed to create file."));
        } else {
            m_done = true;
        }
    } else {
        // Proxy process crashed
        QFile::remove(m_destUrl);
        m_done = false;
        m_errorMessage.append(QString::fromUtf8(m_jobProcess->readAll()));
    }
    return result;
}

void CutClipJob::processLogInfo()
{
    const QString buffer = QString::fromUtf8(m_jobProcess->readAllStandardError());
    m_logDetails.append(buffer);
    int progress = 0;
    // Parse FFmpeg output
    if (m_jobDuration == 0) {
        if (buffer.contains(QLatin1String("Duration:"))) {
            QString data = buffer.section(QStringLiteral("Duration:"), 1, 1).section(QLatin1Char(','), 0, 0).simplified();
            if (!data.isEmpty()) {
                QStringList numbers = data.split(QLatin1Char(':'));
                if (numbers.size() < 3) {
                    return;
                }
                m_jobDuration = (int)(numbers.at(0).toInt() * 3600 + numbers.at(1).toInt() * 60 + numbers.at(2).toDouble());
            }
        }
    } else if (buffer.contains(QLatin1String("time="))) {
        QString time = buffer.section(QStringLiteral("time="), 1, 1).simplified().section(QLatin1Char(' '), 0, 0);
        if (!time.isEmpty()) {
            QStringList numbers = time.split(QLatin1Char(':'));
            if (numbers.size() < 3) {
                progress = (int)time.toDouble();
                if (progress == 0) {
                    return;
                }
            } else {
                progress = numbers.at(0).toInt() * 3600 + numbers.at(1).toInt() * 60 + numbers.at(2).toDouble();
            }
        }
        emit jobProgress((int)(100.0 * progress / m_jobDuration));
    }
}

bool CutClipJob::commitResult(Fun &undo, Fun &redo)
{
    Q_ASSERT(!m_resultConsumed);
    if (!m_done) {
        qDebug() << "ERROR: Trying to consume invalid results";
        return false;
    }
    m_resultConsumed = true;
    if (!KdenliveSettings::add_new_clip()) {
        return true;
    }
    QStringList ids = pCore->projectItemModel()->getClipByUrl(QFileInfo(m_destUrl));
    if (!ids.isEmpty()) {
        // Clip was already inserted in bin, will be reloaded automatically, don't add twice
        return true;
    }
    QString folderId = QStringLiteral("-1");
    auto id = ClipCreator::createClipFromFile(m_destUrl, folderId, pCore->projectItemModel(), undo, redo);
    return id != QStringLiteral("-1");
}
