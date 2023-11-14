/*
    SPDX-FileCopyrightText: 2021 Jean-Baptiste Mardelle <jb@kdenlive.org>

SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "scenesplittask.h"
#include "bin/bin.h"
#include "bin/clipcreator.hpp"
#include "bin/model/markerlistmodel.hpp"
#include "bin/projectclip.h"
#include "bin/projectfolder.h"
#include "bin/projectitemmodel.h"
#include "core.h"
#include "doc/kdenlivedoc.h"
#include "kdenlive_debug.h"
#include "kdenlivesettings.h"
#include "macros.hpp"
#include "mainwindow.h"
#include "ui_scenecutdialog_ui.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPointer>
#include <QProcess>
#include <QTemporaryFile>
#include <QThread>

#include <KLocalizedString>
#include <project/projectmanager.h>

SceneSplitTask::SceneSplitTask(const ObjectId &owner, double threshold, int markersCategory, bool addSubclips, int minDuration, QObject *object)
    : AbstractTask(owner, AbstractTask::ANALYSECLIPJOB, object)
    , m_threshold(threshold)
    , m_jobDuration(0)
    , m_markersType(markersCategory)
    , m_subClips(addSubclips)
    , m_minInterval(minDuration)
    , m_jobProcess(nullptr)
{
    m_description = i18n("Detecting scene change");
    qDebug() << "Threshold is" << threshold << QString::number(threshold);
}

void SceneSplitTask::start(QObject *object, bool force)
{
    Q_UNUSED(object)
    QPointer<QDialog> d = new QDialog;
    Ui::SceneCutDialog_UI view;
    view.setupUi(d);
    view.threshold->setValue(KdenliveSettings::scenesplitthreshold());
    view.add_markers->setChecked(KdenliveSettings::scenesplitmarkers());
    view.cut_scenes->setChecked(KdenliveSettings::scenesplitsubclips());
    // Set  up categories
    view.marker_category->setMarkerModel(pCore->projectManager()->getGuideModel().get());
    d->setWindowTitle(i18nc("@title:window", "Scene Detection"));
    if (d->exec() != QDialog::Accepted) {
        return;
    }
    int threshold = view.threshold->value();
    bool addMarkers = view.add_markers->isChecked();
    bool addSubclips = view.cut_scenes->isChecked();
    int markersCategory = addMarkers ? view.marker_category->currentCategory() : -1;
    int minDuration = view.minDuration->value();
    KdenliveSettings::setScenesplitthreshold(threshold);
    KdenliveSettings::setScenesplitmarkers(view.add_markers->isChecked());
    KdenliveSettings::setScenesplitsubclips(view.cut_scenes->isChecked());

    std::vector<QString> binIds = pCore->bin()->selectedClipsIds(true);
    for (auto &id : binIds) {
        SceneSplitTask *task = nullptr;
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
            task = new SceneSplitTask(owner, threshold / 100., markersCategory, addSubclips, minDuration, binClip.get());

        } else {
            owner = ObjectId(KdenliveObjectType::BinClip, id.toInt(), QUuid());
            auto binClip = pCore->projectItemModel()->getClipByBinID(id);
            task = new SceneSplitTask(owner, threshold / 100., markersCategory, addSubclips, minDuration, binClip.get());
        }
        // See if there is already a task for this MLT service and resource.
        if (task && pCore->taskManager.hasPendingJob(owner, AbstractTask::ANALYSECLIPJOB)) {
            delete task;
            task = nullptr;
        }
        if (task) {
            // Otherwise, start a new audio levels generation thread.
            task->m_isForce = force;
            pCore->taskManager.startTask(owner.itemId, task);
        }
    }
}

void SceneSplitTask::run()
{
    AbstractTaskDone whenFinished(m_owner.itemId, this);
    if (m_isCanceled || pCore->taskManager.isBlocked()) {
        return;
    }
    QMutexLocker lock(&m_runMutex);
    m_running = true;
    auto binClip = pCore->projectItemModel()->getClipByBinID(QString::number(m_owner.itemId));
    const QString source = binClip->url();
    ClipType::ProducerType type = binClip->clipType();
    bool result;
    if (type != ClipType::AV && type != ClipType::Video) {
        // This job can only process video files
        QMetaObject::invokeMethod(pCore.get(), "displayBinMessage", Qt::QueuedConnection, Q_ARG(QString, i18n("Cannot analyse this clip type.")),
                                  Q_ARG(int, int(KMessageWidget::Warning)));
        qDebug() << "=== ABORT 1";
        return;
    }
    if (KdenliveSettings::ffmpegpath().isEmpty()) {
        // FFmpeg not detected, cannot process the Job
        QMetaObject::invokeMethod(pCore.get(), "displayBinMessage", Qt::QueuedConnection,
                                  Q_ARG(QString, i18n("FFmpeg not found, please set path in Kdenlive's settings Environment.")),
                                  Q_ARG(int, int(KMessageWidget::Warning)));
        qDebug() << "=== ABORT 2";
        return;
    }
    m_jobDuration = int(binClip->duration().seconds());
    int producerDuration = binClip->frameDuration();
    // QStringList parameters =
    // {QStringLiteral("-loglevel"),QStringLiteral("info"),QStringLiteral("-i"),source,QStringLiteral("-filter:v"),QString("scdet"),QStringLiteral("-f"),QStringLiteral("null"),QStringLiteral("-")};
    QStringList parameters = {QStringLiteral("-y"),
                              QStringLiteral("-loglevel"),
                              QStringLiteral("info"),
                              QStringLiteral("-i"),
                              source,
                              QStringLiteral("-filter:v"),
                              QString("select='gt(scene,%1)',showinfo").arg(m_threshold),
                              QStringLiteral("-vsync"),
                              QStringLiteral("vfr"),
                              QStringLiteral("-f"),
                              QStringLiteral("null"),
                              QStringLiteral("-")};

    m_jobProcess.reset(new QProcess);
    // m_jobProcess->setStandardErrorFile("/tmp/test_settings.txt");
    m_jobProcess->setProcessChannelMode(QProcess::MergedChannels);
    qDebug() << "=== READY TO START JOB:" << parameters;
    QObject::connect(this, &SceneSplitTask::jobCanceled, m_jobProcess.get(), &QProcess::kill, Qt::DirectConnection);
    QObject::connect(m_jobProcess.get(), &QProcess::readyReadStandardOutput, this, &SceneSplitTask::processLogInfo);
    QObject::connect(m_jobProcess.get(), &QProcess::readyReadStandardError, this, &SceneSplitTask::processLogErr);
    m_jobProcess->start(KdenliveSettings::ffmpegpath(), parameters);
    // m_jobProcess->closeReadChannel(QProcess::StandardError);
    m_jobProcess->waitForStarted();
    // QString data;
    /*while(m_jobProcess->waitForReadyRead()) {
        //data.append(m_jobProcess->readAll());
        qDebug()<<"???? READ: \n"<<m_jobProcess->readAll();
    }*/
    m_jobProcess->waitForFinished(-1);
    result = m_jobProcess->exitStatus() == QProcess::NormalExit;

    // remove temporary playlist if it exists
    m_progress = 100;
    QMetaObject::invokeMethod(m_object, "updateJobProgress");
    if (result && !m_isCanceled) {
        qDebug() << "========================\n\nGOR RESULTS: " << m_results << "\n\n=========";
        auto binClip = pCore->projectItemModel()->getClipByBinID(QString::number(m_owner.itemId));
        if (m_markersType >= 0) {
            // Build json data for markers
            QJsonArray list;
            int ix = 1;
            int lastCut = 0;
            for (auto &marker : m_results) {
                int pos = GenTime(marker).frames(pCore->getCurrentFps());
                if (m_minInterval > 0 && ix > 1 && pos - lastCut < m_minInterval) {
                    continue;
                }
                lastCut = pos;
                QJsonObject currentMarker;
                currentMarker.insert(QLatin1String("pos"), QJsonValue(pos));
                currentMarker.insert(QLatin1String("comment"), QJsonValue(i18n("Scene %1", ix)));
                currentMarker.insert(QLatin1String("type"), QJsonValue(m_markersType));
                list.push_back(currentMarker);
                ix++;
            }
            QJsonDocument json(list);
            QMetaObject::invokeMethod(m_object, "importJsonMarkers", Q_ARG(QString, QString(json.toJson())));
        }
        if (m_subClips) {
            // Create zones
            int ix = 1;
            int lastCut = 0;
            QJsonArray list;
            QJsonDocument json;
            for (double &marker : m_results) {
                int pos = GenTime(marker).frames(pCore->getCurrentFps());
                if (pos <= lastCut + 1 || pos - lastCut < m_minInterval) {
                    continue;
                }
                QJsonObject currentZone;
                currentZone.insert(QLatin1String("name"), QJsonValue(i18n("Scene %1", ix)));
                currentZone.insert(QLatin1String("in"), QJsonValue(lastCut));
                currentZone.insert(QLatin1String("out"), QJsonValue(pos - 1));
                list.push_back(currentZone);
                lastCut = pos;
                ix++;
            }
            if (lastCut < producerDuration) {
                QJsonObject currentZone;
                currentZone.insert(QLatin1String("name"), QJsonValue(i18n("Scene %1", ix)));
                currentZone.insert(QLatin1String("in"), QJsonValue(lastCut));
                currentZone.insert(QLatin1String("out"), QJsonValue(producerDuration));
                list.push_back(currentZone);
            }
            json.setArray(list);
            if (!json.isEmpty()) {
                QString dataMap(json.toJson());
                QMetaObject::invokeMethod(pCore->projectItemModel().get(), "loadSubClips", Q_ARG(QString, QString::number(m_owner.itemId)),
                                          Q_ARG(QString, dataMap), Q_ARG(bool, true));
            }
        }
    } else {
        // Proxy process crashed
        QMetaObject::invokeMethod(pCore.get(), "displayBinLogMessage", Qt::QueuedConnection, Q_ARG(QString, i18n("Failed to analyse clip.")),
                                  Q_ARG(int, int(KMessageWidget::Warning)), Q_ARG(QString, m_logDetails));
    }
}

void SceneSplitTask::processLogErr()
{
    const QString buffer = QString::fromUtf8(m_jobProcess->readAllStandardError());
    qDebug() << "ERROR: ----\n" << buffer;
}

void SceneSplitTask::processLogInfo()
{
    const QString buffer = QString::fromUtf8(m_jobProcess->readAllStandardOutput());
    m_logDetails.append(buffer);
    int progress = 0;
    // Parse FFmpeg output
    qDebug() << "-------------\n" << buffer;
    if (buffer.contains(QLatin1String("[Parsed_showinfo"))) {
        QString timeMarker("pts_time:");
        bool ok;
        QStringList output = buffer.split("[Parsed_showinfo");
        output.removeFirst();
        for (const QString &o : qAsConst(output)) {
            if (o.contains(timeMarker)) {
                double res = o.section(timeMarker, 1).section(QLatin1Char(' '), 0, 0).toDouble(&ok);
                if (ok) {
                    m_results << res;
                }
            }
        }
    }
    if (m_jobDuration == 0) {
        qDebug() << "=== NO DURATION!!!";
        if (buffer.contains(QLatin1String("Duration:"))) {
            QString data = buffer.section(QStringLiteral("Duration:"), 1, 1).section(QLatin1Char(','), 0, 0).simplified();
            if (!data.isEmpty()) {
                qDebug() << "==== GOT DURATION:" << data;
                QStringList numbers = data.split(QLatin1Char(':'));
                if (numbers.size() < 3) {
                    return;
                }
                m_jobDuration = numbers.at(0).toInt() * 3600 + numbers.at(1).toInt() * 60 + numbers.at(2).toInt();
            }
        }
    } else if (buffer.contains(QLatin1String("time="))) {
        QString time = buffer.section(QStringLiteral("time="), 1, 1).simplified().section(QLatin1Char(' '), 0, 0);
        qDebug() << "=== GOT PROGRESS TIME: " << time;
        if (!time.isEmpty()) {
            QStringList numbers = time.split(QLatin1Char(':'));
            if (numbers.size() < 3) {
                progress = time.toInt();
                if (progress == 0) {
                    return;
                }
            } else {
                progress = numbers.at(0).toInt() * 3600 + numbers.at(1).toInt() * 60 + qRound(numbers.at(2).toDouble());
            }
        }
        m_progress = 100 * progress / m_jobDuration;
        QMetaObject::invokeMethod(m_object, "updateJobProgress");
        // Q_EMIT jobProgress(int(100.0 * progress / m_jobDuration));
    }
}
