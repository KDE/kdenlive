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

#include "filtertask.h"
#include "bin/bin.h"
#include "mainwindow.h"
#include "bin/projectclip.h"
#include "bin/projectitemmodel.h"
#include "profiles/profilemodel.hpp"
#include "assets/model/assetparametermodel.hpp" 
#include "core.h"
#include "kdenlive_debug.h"
#include "kdenlivesettings.h"
#include "macros.hpp"

#include <QThread>

#include <klocalizedstring.h>

static void consumer_frame_render(mlt_consumer, FilterTask *self, mlt_frame frame_ptr)
{
    self->updateProgress(int(100 * mlt_frame_get_position(frame_ptr) / self->length));
}

FilterTask::FilterTask(const ObjectId &owner, const QString &binId, std::weak_ptr<AssetParameterModel> model, const QString &assetId, int in, int out, QString filterName, std::unordered_map<QString, QVariant> filterParams, std::unordered_map<QString, QString> filterData, const QStringList consumerArgs, QObject* object)
    : AbstractTask(owner, AbstractTask::FILTERCLIPJOB, object)
    , m_jobDuration(0)
    , m_binId(binId)
    , m_inPoint(in)
    , m_outPoint(out)
    , m_assetId(assetId)
    , m_model(model)
    , m_filterName(filterName)
    , m_filterParams(filterParams)
    , m_filterData(filterData)
    , m_consumerArgs(consumerArgs)
{
}

void FilterTask::start(const ObjectId &owner, const QString &binId, std::weak_ptr<AssetParameterModel> model, const QString &assetId, int in, int out, QString filterName, std::unordered_map<QString, QVariant> filterParams, std::unordered_map<QString, QString> filterData, const QStringList consumerArgs, QObject* object, bool force)
{
    FilterTask* task = new FilterTask(owner, binId, model, assetId, in, out, filterName, filterParams, filterData, consumerArgs, object);
    if (task) {
        // Otherwise, start a filter thread.
        task->m_isForce = force;
        pCore->taskManager.startTask(owner.second, task);
    }
}

void FilterTask::updateProgress(int prog)
{
    if (prog != m_progress) {
        m_progress = prog;
        if (auto ptr = m_model.lock()) {
            QMetaObject::invokeMethod(ptr.get(), "setProgress", Q_ARG(int, prog));
        }
    }
}

void FilterTask::abort()
{
    qDebug()<<"CANCELING FROM THREAD: "<<QThread::currentThreadId()<<"\n::::::::::::::::::::::";
    m_consumer->purge();
    m_consumer->stop();
}

void FilterTask::run()
{
    if (m_isCanceled) {
        pCore->taskManager.taskDone(m_owner.second, this);
        return;
    }
    m_running = true;
    
    QString url;
    auto binClip = pCore->projectItemModel()->getClipByBinID(m_binId);
    std::unique_ptr<Mlt::Producer> producer;
    std::unique_ptr<Mlt::Producer> wholeProducer;
    Mlt::Profile profile(pCore->getCurrentProfilePath().toUtf8().constData());
    if (binClip) {
        // Filter applied on a timeline or bin clip
        url = binClip->url();
        if (url.isEmpty()) {
            m_errorMessage.append(i18n("No producer for this clip."));
            pCore->taskManager.taskDone(m_owner.second, this);
            return;
        }
        if (KdenliveSettings::gpu_accel()) {
            producer = binClip->getClone();
            Mlt::Filter converter(profile, "avcolor_space");
            producer->attach(converter);
        } else {
            qDebug()<<"==== BUILDING PRODUCER: "<<url;
            producer = std::make_unique<Mlt::Producer>(profile, url.toUtf8().constData());
        }
        if ((producer == nullptr) || !producer->is_valid()) {
            // Clip was removed or something went wrong, Notify user?
            m_errorMessage.append(i18n("Invalid clip"));
            pCore->taskManager.taskDone(m_owner.second, this);
            return;
        }
        if (m_outPoint == -1) {
            m_outPoint = producer->get_length() - 1;
        }
        if (m_inPoint == -1) {
            m_inPoint = 0;
        }
        if (m_inPoint != 0 || m_outPoint != producer->get_length() - 1) {
            std::swap(wholeProducer, producer);
            producer.reset(wholeProducer->cut(m_inPoint, m_outPoint));
        }
    } else {
        // Filter applied on a track of master producer, leave config to source job
    }

    if (m_owner.first == ObjectType::Master) {
        producer = pCore->getMasterProducerInstance();
    } else if (m_owner.first == ObjectType::TimelineTrack) {
        producer = pCore->getTrackProducerInstance(m_owner.second);
    }
    
    if ((producer == nullptr) || !producer->is_valid()) {
        // Clip was removed or something went wrong, Notify user?
        m_errorMessage.append(i18n("Invalid clip"));
        pCore->taskManager.taskDone(m_owner.second, this);
        return;
    }
    
    length = producer->get_playtime();
    if (length == 0) {
        length = producer->get_length();
    }

    // Build consumer
    //m_consumer.reset(new Mlt::Consumer(profile, "null"));
    qDebug()<<"---- BUILDING CONSUMER ------";
    m_consumer.reset(new Mlt::Consumer(profile, "null"));
    qDebug()<<"---- BUILDING CONSUMER DONE ------";
    for (const QString &param : qAsConst(m_consumerArgs)) {
        if (param.contains(QLatin1Char('='))) {
            m_consumer->set(param.section(QLatin1Char('='), 0, 0).toUtf8().constData(), param.section(QLatin1Char('='), 1).toInt());
        }
    }
    qDebug()<<"---- BUILDING CONSUMER 2------";
    if (!m_consumer->is_valid()) {
        m_errorMessage.append(i18n("Cannot create consumer."));
        pCore->taskManager.taskDone(m_owner.second, this);
        return;
    }
    qDebug()<<"---- BUILDING CONSUMER 3------";

    // Build filter
    Mlt::Filter filter(profile, m_filterName.toUtf8().data());
    if (!filter.is_valid()) {
        m_errorMessage.append(i18n("Cannot create filter %1", m_filterName));
        pCore->taskManager.taskDone(m_owner.second, this);
        return;
    }

    // Process filter params
    qDebug()<<" = = = = = CONFIGURING FILTER PARAMS = = = = =  ";
    for (const auto &it : m_filterParams) {
        qDebug()<<". . ."<<it.first<<" = "<<it.second;
        if (it.second.type() == QVariant::Double) {
            filter.set(it.first.toUtf8().constData(), it.second.toDouble());
        } else {
            filter.set(it.first.toUtf8().constData(), it.second.toString().toUtf8().constData());
        }
    }
    if (m_filterData.find(QLatin1String("relativeInOut")) != m_filterData.end()) {
        // leave it operate on full clip
    } else {
        filter.set_in_and_out(producer->get_in(), producer->get_out());
    }

    Mlt::Tractor tractor(profile);
    tractor.set_track(*producer.get(), 0);
    m_consumer->connect(tractor);
    producer->set_speed(0);
    producer->seek(0);
    producer->attach(filter);

    qDebug()<<"=== FILTER READY TO PROCESS; LENGTH: "<<length;
    m_showFrameEvent.reset(m_consumer->listen("consumer-frame-show", this, mlt_listener(consumer_frame_render)));
    connect(this, &AbstractTask::jobCanceled, [&] () {
        qDebug()<<"CANCELING FROM THREAD: "<<QThread::currentThreadId()<<"\n::::::::::::::::::::::";
        m_showFrameEvent.reset();
        m_consumer->stop();
    });
    //QObject::connect(this, &AbstractTask::jobCanceled, this, &FilterTask::abort);
    /*auto *obj = QAbstractEventDispatcher::instance(thread());
    QObject::connect(this, &AbstractTask::jobCanceled, this, [this, obj]() {
        QMetaObject::invokeMethod(obj, [this]{ abort(); });
    });*/
    m_consumer->run();
    qDebug()<<"===============FILTER PROCESSED\n\n==============0";
    m_progress = 100;
    if (auto ptr = m_model.lock()) {
        qDebug()<<"=== UPDATING PROGRESS TO 100";
        QMetaObject::invokeMethod(ptr.get(), "setProgress", Q_ARG(int, 100));
    }
    m_showFrameEvent.reset();
    qDebug()<<"=== TASK DONE FROM THREAD: "<<QThread::currentThreadId()<<"\n::::::::::::::::::::::";
    pCore->taskManager.taskDone(m_owner.second, this);
    qDebug()<<"=== TASK DONE 2";
    if (m_isCanceled) {
        qDebug()<<"=== EXITING ON CANCEL";
        return;
    }
    
    paramVector params;
    QString key("results");
    if (m_filterData.find(QStringLiteral("key")) != m_filterData.end()) {
        key = m_filterData.at(QStringLiteral("key"));
    }
    QString resultData = qstrdup(filter.get(key.toUtf8().constData()));
    params.append({key,QVariant(resultData)});
    qDebug()<<"= = = GOT FILTER RESULTS: "<<params;
    if (m_filterData.find(QStringLiteral("storedata")) != m_filterData.end()) {
        // Store a copy of the data in clip analysis
        QString dataName = (m_filterData.find(QStringLiteral("displaydataname")) != m_filterData.end()) ? m_filterData.at(QStringLiteral("displaydataname")) : QStringLiteral("data");
        auto binClip = pCore->projectItemModel()->getClipByBinID(m_binId);
        if (binClip) {
            QMetaObject::invokeMethod(binClip.get(), "updatedAnalysisData", Q_ARG(const QString&, dataName), Q_ARG(const QString&, resultData), Q_ARG(int, m_inPoint));
        }
        //binClip->updatedAnalysisData(dataName, resultData, m_inPoint);
    }
    auto operation = [assetModel = m_model, filterParams = std::move(params)]() {
        if (auto ptr = assetModel.lock()) {
            qDebug()<<"===== SETTING FILTER PARAM: "<<filterParams;
            QMetaObject::invokeMethod(ptr.get(), "setParameters", Q_ARG(paramVector, filterParams));
            //ptr->setParameters(filterParams);
        }
        QMetaObject::invokeMethod(pCore.get(), "setDocumentModified");
        return true;
    };
    auto reverse = [assetModel = m_model, keyName = key]() {
        paramVector fParams;
        fParams.append({keyName,QVariant()});
        if (auto ptr = assetModel.lock()) {
            QMetaObject::invokeMethod(ptr.get(), "setParameters", Q_ARG(paramVector, fParams));
            //ptr->setParameters(fParams);
        }
        QMetaObject::invokeMethod(pCore.get(), "setDocumentModified");
        return true;
    };
    bool ok = operation();
    return;
}
