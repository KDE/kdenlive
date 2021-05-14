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

#include "stabilizetask.h"
#include "bin/bin.h"
#include "mainwindow.h"
#include "bin/projectclip.h"
#include "bin/projectfolder.h"
#include "project/clipstabilize.h"
#include "bin/projectitemmodel.h"
#include "profiles/profilemodel.hpp"
#include "assets/model/assetparametermodel.hpp" 
#include "core.h"
#include "kdenlive_debug.h"
#include "kdenlivesettings.h"
#include "macros.hpp"
#include "xml/xml.hpp"

#include <QThread>
#include <QProcess>

#include <klocalizedstring.h>

StabilizeTask::StabilizeTask(const ObjectId &owner, const QString &binId, const QString &destination, int in, int out, std::unordered_map<QString, QVariant> filterParams, QObject* object)
    : AbstractTask(owner, AbstractTask::STABILIZEJOB, object)
    , m_binId(binId)
    , m_inPoint(in)
    , m_outPoint(out)
    , m_filterParams(filterParams)
    , m_destination(destination)
{
}

void StabilizeTask::start(QObject* object, bool force)
{
    std::vector<QString> binIds = pCore->bin()->selectedClipsIds(true);
    QScopedPointer<ClipStabilize> d(new ClipStabilize(binIds, QStringLiteral("vidstab")));
    if (d->exec() == QDialog::Accepted) {
        std::unordered_map<QString, QVariant> filterParams = d->filterParams();
        QString destination = d->destination();
        std::unordered_map<QString, QString> destinations; // keys are binIds, values are path to target files
        for (const auto &binId : binIds) {
            qDebug()<<"==== ANALYSING BINID: "<<binId;
            auto binClip = pCore->projectItemModel()->getClipByBinID(binId.section(QLatin1Char('/'), 0, 0));
            if (binIds.size() == 1) {
                // We only have one clip, destination points to the final url
                destinations[binId] = destination;
            } else {
                // Filter several clips, destination points to a folder
                QString mltfile = destination + QFileInfo(binClip->url()).fileName() + QStringLiteral(".mlt");
                destinations[binId] = mltfile;
            }
        }
        // Now we have to create the jobs objects. This is trickier than usual, since the parameters are different for each job (each clip has its own
        // destination). We have to construct a lambda that does that.
        for (auto & id : binIds) {
            StabilizeTask* task = nullptr;
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
                    task = new StabilizeTask(owner, binData.first(), destinations.at(id), binData.at(1).toInt(), binData.at(2).toInt(), filterParams, binClip.get());
                }
            } else {
                // Process full clip
                owner = ObjectId(ObjectType::BinClip, id.toInt());
                auto binClip = pCore->projectItemModel()->getClipByBinID(id);
                if (binClip) {
                    task = new StabilizeTask(owner, id, destinations.at(id), -1, -1, filterParams, binClip.get());
                }
            }
            if (task) {
                // Otherwise, start a filter thread.
                task->m_isForce = force;
                pCore->taskManager.startTask(owner.second, task);
            }
        }
    }
}

void StabilizeTask::run()
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
        producerArgs << url;
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

    producerArgs << QStringLiteral("-attach") << QStringLiteral("vidstab");

    // Process filter params
    qDebug()<<" = = = = = CONFIGURING FILTER PARAMS = = = = =  ";
    for (const auto &it : m_filterParams) {
        qDebug()<<". . ."<<it.first<<" = "<<it.second;
        if (it.second.type() == QVariant::Double) {
            producerArgs << QString("%1=%2").arg(it.first).arg(it.second.toDouble());
        } else {
            producerArgs << QString("%1=%2").arg(it.first).arg(it.second.toString());
        }
    }
    QString targetFile = m_destination + QStringLiteral(".trf");
    int count = 1;
    while (QFile::exists(targetFile)) {
        targetFile = m_destination + QString("-%1.trf").arg(count);
        count++;
    }
    producerArgs << QString("filename=%1").arg(targetFile);

    // Start the MLT Process
    QProcess filterProcess;
    producerArgs << QStringLiteral("-consumer") << QString("xml:%1").arg(m_destination) << QStringLiteral("all=1") << QStringLiteral("terminate_on_pause=1");
    m_jobProcess.reset(new QProcess);
    QMetaObject::invokeMethod(m_object, "updateJobProgress");
    QObject::connect(this, &AbstractTask::jobCanceled, m_jobProcess.get(), &QProcess::kill, Qt::DirectConnection);
    QObject::connect(m_jobProcess.get(), &QProcess::readyReadStandardError, this, &StabilizeTask::processLogInfo);
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

    QMetaObject::invokeMethod(pCore->bin(), "addProjectClipInFolder", Qt::QueuedConnection, Q_ARG(const QString&,m_destination), Q_ARG(const QString&,binClip->parent()->clipId()), Q_ARG(const QString&,i18n("Stabilized")));
    return;
}

void StabilizeTask::processLogInfo()
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
