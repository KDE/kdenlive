/*
    SPDX-FileCopyrightText: 2021 Jean-Baptiste Mardelle <jb@kdenlive.org>

SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "stabilizetask.h"
#include "assets/model/assetparametermodel.hpp"
#include "bin/bin.h"
#include "bin/projectclip.h"
#include "bin/projectfolder.h"
#include "bin/projectitemmodel.h"
#include "core.h"
#include "kdenlive_debug.h"
#include "kdenlivesettings.h"
#include "macros.hpp"
#include "mainwindow.h"
#include "profiles/profilemodel.hpp"
#include "project/clipstabilize.h"
#include "xml/xml.hpp"

#include <QProcess>
#include <QThread>

#include <KLocalizedString>

StabilizeTask::StabilizeTask(const ObjectId &owner, const QString &binId, const QString &destination, int in, int out,
                             const std::unordered_map<QString, QVariant> &filterParams, QObject *object)
    : AbstractTask(owner, AbstractTask::STABILIZEJOB, object)
    , m_binId(binId)
    , m_inPoint(in)
    , m_outPoint(out)
    , m_filterParams(filterParams)
    , m_destination(destination)
{
    m_description = i18n("Stabilizing");
}

void StabilizeTask::start(QObject *, bool force)
{
    std::vector<QString> binIds = pCore->bin()->selectedClipsIds(true);
    QScopedPointer<ClipStabilize> d(new ClipStabilize(binIds, QStringLiteral("vidstab"), QApplication::activeWindow()));
    if (d->exec() == QDialog::Accepted) {
        std::unordered_map<QString, QVariant> filterParams = d->filterParams();
        std::unordered_map<QString, QString> destinations; // keys are binIds, values are path to target files
        for (const auto &binId : binIds) {
            qDebug() << "==== ANALYSING BINID: " << binId;
            auto binClip = pCore->projectItemModel()->getClipByBinID(binId.section(QLatin1Char('/'), 0, 0));
            QString mltfile = binClip->url() + QStringLiteral(".mlt");
            destinations[binId] = mltfile;
        }
        // Now we have to create the jobs objects. This is trickier than usual, since the parameters are different for each job (each clip has its own
        // destination). We have to construct a lambda that does that.
        for (auto &id : binIds) {
            StabilizeTask *task = nullptr;
            ObjectId owner;
            if (id.contains(QLatin1Char('/'))) {
                QStringList binData = id.split(QLatin1Char('/'));
                if (binData.size() < 3) {
                    // Invalid subclip data
                    qDebug() << "=== INVALID SUBCLIP DATA: " << id;
                    continue;
                }
                owner = ObjectId(KdenliveObjectType::BinClip, binData.first().toInt(), QUuid());
                auto binClip = pCore->projectItemModel()->getClipByBinID(binData.first());
                if (binClip) {
                    task = new StabilizeTask(owner, binData.first(), destinations.at(id), binData.at(1).toInt(), binData.at(2).toInt(), filterParams,
                                             binClip.get());
                }
            } else {
                // Process full clip
                owner = ObjectId(KdenliveObjectType::BinClip, id.toInt(), QUuid());
                auto binClip = pCore->projectItemModel()->getClipByBinID(id);
                if (binClip) {
                    task = new StabilizeTask(owner, id, destinations.at(id), -1, -1, filterParams, binClip.get());
                }
            }
            if (task) {
                // Otherwise, start a filter thread.
                task->m_isForce = force;
                pCore->taskManager.startTask(owner.itemId, task);
            }
        }
    }
}

void StabilizeTask::run()
{
    AbstractTaskDone whenFinished(m_owner.itemId, this);
    if (m_isCanceled || pCore->taskManager.isBlocked()) {
        return;
    }
    QMutexLocker lock(&m_runMutex);
    m_running = true;
    qDebug() << " + + + + + + + + STARTING STAB TASK";

    QString url;
    auto binClip = pCore->projectItemModel()->getClipByBinID(m_binId);
    QString folderId = QLatin1String("-1");
    QStringList producerArgs = {QStringLiteral("progress=1"), QStringLiteral("-profile"), pCore->getCurrentProfilePath()};
    if (binClip) {
        // Filter applied on a timeline or bin clip
        folderId = binClip->parent()->clipId();
        url = binClip->url();
        if (url.isEmpty()) {
            QMetaObject::invokeMethod(pCore.get(), "displayBinMessage", Qt::QueuedConnection, Q_ARG(QString, i18n("No producer for this clip.")),
                                      Q_ARG(int, int(KMessageWidget::Warning)));
            return;
        }
        producerArgs << url;
        producerArgs << binClip->enforcedParams();

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
        QMetaObject::invokeMethod(pCore.get(), "displayBinMessage", Qt::QueuedConnection, Q_ARG(QString, i18n("No producer for this clip.")),
                                  Q_ARG(int, int(KMessageWidget::Warning)));
        return;
        /*if (m_owner.type == KdenliveObjectType::Master) {
            producer = pCore->getMasterProducerInstance();
        } else if (m_owner.type == KdenliveObjectType::TimelineTrack) {
            producer = pCore->getTrackProducerInstance(m_owner.second);
        }
        if ((producer == nullptr) || !producer->is_valid()) {
            // Clip was removed or something went wrong, Notify user?
            m_errorMessage.append(i18n("Invalid clip"));
            return;
        }*/
    }

    producerArgs << QStringLiteral("-attach") << QStringLiteral("vidstab");

    // Process filter params
    qDebug() << " = = = = = CONFIGURING FILTER PARAMS = = = = =  ";
    for (const auto &it : m_filterParams) {
        qDebug() << ". . ." << it.first << " = " << it.second;
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
        if (it.second.type() == QVariant::Double) {
#else
        if (it.second.typeId() == QMetaType::Double) {
#endif
            producerArgs << QString("%1=%2").arg(it.first, QString::number(it.second.toDouble()));
        } else {
            producerArgs << QString("%1=%2").arg(it.first, it.second.toString());
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
    qDebug() << "=== STARTING PROCESS: " << producerArgs;
    m_jobProcess->start(KdenliveSettings::meltpath(), producerArgs);
    m_jobProcess->waitForFinished(-1);
    qDebug() << " + + + + + + + + SOURCE FILE PROCESSED: " << m_jobProcess->exitStatus();
    bool result = m_jobProcess->exitStatus() == QProcess::NormalExit;
    m_progress = 100;
    QMetaObject::invokeMethod(m_object, "updateJobProgress");
    if (m_isCanceled || !result) {
        if (!m_isCanceled) {
            QMetaObject::invokeMethod(pCore.get(), "displayBinLogMessage", Qt::QueuedConnection, Q_ARG(QString, i18n("Failed to stabilize.")),
                                      Q_ARG(int, int(KMessageWidget::Warning)), Q_ARG(QString, m_logDetails));
        }
        return;
    }
    QMetaObject::invokeMethod(pCore->bin(), "addProjectClipInFolder", Qt::QueuedConnection, Q_ARG(QString, m_destination), Q_ARG(QString, m_binId),
                              Q_ARG(QString, folderId), Q_ARG(QString, QStringLiteral("stabilize")));
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
