/***************************************************************************
 *                                                                         *
 *   Copyright (C) 2021 by Jean-Baptiste Mardelle (jb@kdenlive.org)        *
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

#include "speedtask.h"
#include "bin/bin.h"
#include "mainwindow.h"
#include "bin/projectclip.h"
#include "bin/projectitemmodel.h"
#include "core.h"
#include "kdenlive_debug.h"
#include "kdenlivesettings.h"
#include "macros.hpp"
#include "xml/xml.hpp"

#include <QThread>
#include <QProcess>
#include <KIO/RenameDialog>
#include <klocalizedstring.h>
#include <KLineEdit>
#include <KUrlRequester>

SpeedTask::SpeedTask(const ObjectId &owner, const QString &binId, const QString &destination, int in, int out, std::unordered_map<QString, QVariant> filterParams, QObject* object)
    : AbstractTask(owner, AbstractTask::SPEEDJOB, object)
    , m_binId(binId)
    , m_filterParams(filterParams)
    , m_destination(destination)
{
    m_speed = filterParams.at(QStringLiteral("warp_speed")).toDouble();
    m_inPoint = in > -1 ? qRound(in / m_speed) : -1;
    m_outPoint = out > -1 ? qRound(out / m_speed) : -1;
}

void SpeedTask::start(QObject* object, bool force)
{
    std::vector<QString> binIds = pCore->bin()->selectedClipsIds(true);
    // Show config dialog
    QDialog d(qApp->activeWindow());
    d.setWindowTitle(i18n("Clip Speed"));
    QDialogButtonBox buttonBox(QDialogButtonBox::Cancel | QDialogButtonBox::Save);
    auto *l = new QVBoxLayout;
    d.setLayout(l);
    QLabel labUrl(&d);
    KUrlRequester fileUrl(&d);
    auto binClip = pCore->projectItemModel()->getClipByBinID(binIds.front().section(QLatin1Char('/'), 0, 0));
    QDir folder = QFileInfo(binClip->url()).absoluteDir();
    folder.mkpath(i18n("Speed Change"));
    folder.cd(i18n("Speed Change"));
    if (binIds.size() > 1) {
        labUrl.setText(i18n("Destination Folder"));
        fileUrl.setMode(KFile::Directory);
        fileUrl.setUrl(QUrl::fromLocalFile(folder.absolutePath()));
    } else {
        labUrl.setText(i18n("Destination File"));
        fileUrl.setMode(KFile::File);
        QString filePath = QFileInfo(binClip->url()).fileName().section(QLatin1Char('.'), 0, -2);
        filePath.append(QStringLiteral(".mlt"));
        fileUrl.setUrl(QUrl::fromLocalFile(folder.absoluteFilePath(filePath)));
    }
    QFontMetrics fm = fileUrl.lineEdit()->fontMetrics();
    fileUrl.setMinimumWidth(int(fm.boundingRect(fileUrl.text().left(50)).width() * 1.4));
    QLabel lab(&d);
    lab.setText(i18n("Percentage"));
    QDoubleSpinBox speedInput(&d);
    speedInput.setRange(-100000, 100000);
    speedInput.setValue(100);
    speedInput.setSuffix(QLatin1String("%"));
    speedInput.setFocus();
    speedInput.selectAll();
    QCheckBox cb(i18n("Pitch compensation"), &d);
    cb.setChecked(true);
    l->addWidget(&labUrl);
    l->addWidget(&fileUrl);
    l->addWidget(&lab);
    l->addWidget(&speedInput);
    l->addWidget(&cb);
    l->addWidget(&buttonBox);
    d.connect(&buttonBox, &QDialogButtonBox::rejected, &d, &QDialog::reject);
    d.connect(&buttonBox, &QDialogButtonBox::accepted, &d, &QDialog::accept);
    if (d.exec() != QDialog::Accepted) {
        return;
    }
    double speed = speedInput.value();
    bool warp_pitch = cb.isChecked();
    std::unordered_map<QString, QString> destinations; // keys are binIds, values are path to target files
    std::unordered_map<QString, QVariant> filterParams;
    filterParams[QStringLiteral("warp_speed")] = speed / 100.0;
    if (warp_pitch) {
        filterParams[QStringLiteral("warp_pitch")] = 1;
    }
    for (const auto &binId : binIds) {
        QString mltfile;
        if (binIds.size() == 1) {
            // converting only 1 clip
            mltfile = fileUrl.url().toLocalFile();
        } else {
            QDir dir(fileUrl.url().toLocalFile());
            binClip = pCore->projectItemModel()->getClipByBinID(binId.section(QLatin1Char('/'), 0, 0));
            mltfile = QFileInfo(binClip->url()).fileName().section(QLatin1Char('.'), 0, -2);
            mltfile.append(QString("-%1.mlt").arg(QString::number(int(speed))));
            mltfile = dir.absoluteFilePath(mltfile);
        }
        // Filter several clips, destination points to a folder
        if (QFile::exists(mltfile)) {
            KIO::RenameDialog renameDialog(qApp->activeWindow(), i18n("File already exists"), QUrl::fromLocalFile(mltfile), QUrl::fromLocalFile(mltfile), KIO::RenameDialog_Option::RenameDialog_Overwrite );
            if (renameDialog.exec() != QDialog::Rejected) {
                QUrl final = renameDialog.newDestUrl();
                if (final.isValid()) {
                    mltfile = final.toLocalFile();
                }
            } else {
                return;
            }
        }
        destinations[binId] = mltfile;
    }

    for (auto & id : binIds) {
        SpeedTask* task = nullptr;
        ObjectId owner;
        if (id.contains(QLatin1Char('/'))) {
            QStringList binData = id.split(QLatin1Char('/'));
            if (binData.size() < 3) {
                // Invalid subclip data
                qDebug()<<"=== INVALID SUBCLIP DATA: "<<id;
                continue;
            }
            owner = ObjectId(ObjectType::BinClip, binData.first().toInt());
            auto binClip = pCore->projectItemModel()->getClipByBinID(binData.first());
            if (binClip) {
                task = new SpeedTask(owner, binData.first(), destinations.at(id), binData.at(1).toInt(), binData.at(2).toInt(), filterParams, binClip.get());
            }
        } else {
            // Process full clip
            owner = ObjectId(ObjectType::BinClip, id.toInt());
            auto binClip = pCore->projectItemModel()->getClipByBinID(id);
            if (binClip) {
                task = new SpeedTask(owner, id, destinations.at(id), -1, -1, filterParams, binClip.get());
            }
        }
        if (task) {
            // Otherwise, start a filter thread.
            task->m_isForce = force;
            pCore->taskManager.startTask(owner.second, task);
        }
    }
}

void SpeedTask::run()
{
    if (m_isCanceled) {
        pCore->taskManager.taskDone(m_owner.second, this);
        return;
    }
    m_running = true;
    qDebug()<<" + + + + + + + + STARTING STAB TASK";
    
    QString url;
    auto binClip = pCore->projectItemModel()->getClipByBinID(m_binId);
    QStringList producerArgs = {QStringLiteral("progress=1"),QStringLiteral("-profile"),pCore->getCurrentProfilePath()};
    if (binClip) {
        // Filter applied on a timeline or bin clip
        url = binClip->url();
        if (url.isEmpty()) {
            m_errorMessage.append(i18n("No producer for this clip."));
            pCore->taskManager.taskDone(m_owner.second, this);
            return;
        }
        producerArgs << QString("timewarp:%1:%2").arg(m_speed).arg(url);
        if (m_inPoint > -1) {
           producerArgs << QString("in=%1").arg(m_inPoint);
        }
        if (m_outPoint > -1) {
           producerArgs << QString("out=%1").arg(m_outPoint);
        }
    } else {
        // Filter applied on a track of master producer, leave config to source job
        // We are on master or track, configure producer accordingly
        // TODO
        /*if (m_owner.first == ObjectType::Master) {
            producer = pCore->getMasterProducerInstance();
        } else if (m_owner.first == ObjectType::TimelineTrack) {
            producer = pCore->getTrackProducerInstance(m_owner.second);
        }
        if ((producer == nullptr) || !producer->is_valid()) {
            // Clip was removed or something went wrong, Notify user?
            m_errorMessage.append(i18n("Invalid clip"));
            pCore->taskManager.taskDone(m_owner.second, this);
            return;
        }*/
    }

    // Process filter params
    for (const auto &it : m_filterParams) {
        qDebug()<<". . ."<<it.first<<" = "<<it.second;
        if (it.second.type() == QVariant::Double) {
            producerArgs << QString("%1=%2").arg(it.first).arg(it.second.toDouble());
        } else {
            producerArgs << QString("%1=%2").arg(it.first).arg(it.second.toString());
        }
    }

    // Start the MLT Process
    QProcess filterProcess;
    producerArgs << QStringLiteral("-consumer") << QString("xml:%1").arg(m_destination) << QStringLiteral("terminate_on_pause=1");
    m_jobProcess.reset(new QProcess);
    QMetaObject::invokeMethod(m_object, "updateJobProgress");
    QObject::connect(this, &AbstractTask::jobCanceled, m_jobProcess.get(), &QProcess::kill, Qt::DirectConnection);
    QObject::connect(m_jobProcess.get(), &QProcess::readyReadStandardError, this, &SpeedTask::processLogInfo);
    qDebug()<<"=== STARTING PROCESS: "<<producerArgs;
    m_jobProcess->start(KdenliveSettings::rendererpath(), producerArgs);
    m_jobProcess->waitForFinished(-1);
    qDebug()<<" + + + + + + + + SOURCE FILE PROCESSED: "<<m_jobProcess->exitStatus();
    bool result = m_jobProcess->exitStatus() == QProcess::NormalExit;
    m_progress = 100;
    QMetaObject::invokeMethod(m_object, "updateJobProgress");
    pCore->taskManager.taskDone(m_owner.second, this);
    if (m_isCanceled || !result) {
        return;
    }

    QMetaObject::invokeMethod(pCore->bin(), "addProjectClipInFolder", Qt::QueuedConnection, Q_ARG(const QString&,m_destination), Q_ARG(const QString&,binClip->parent()->clipId()), Q_ARG(const QString&,i18n("Speed Change")));
    return;
}

void SpeedTask::processLogInfo()
{
    const QString buffer = QString::fromUtf8(m_jobProcess->readAllStandardError());
    m_logDetails.append(buffer);
    // Parse MLT output
    if (buffer.contains(QLatin1String("percentage:"))) {
        int progress = buffer.section(QStringLiteral("percentage:"), 1).simplified().section(QLatin1Char(' '), 0, 0).toInt();
        if (progress == m_progress) {
            return;
        }
        m_progress = progress;
        QMetaObject::invokeMethod(m_object, "updateJobProgress");
    }
}
