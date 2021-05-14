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
#include "xml/xml.hpp"

#include <QThread>
#include <QProcess>

#include <klocalizedstring.h>

FilterTask::FilterTask(const ObjectId &owner, const QString &binId, std::weak_ptr<AssetParameterModel> model, const QString &assetId, int in, int out, QString filterName, std::unordered_map<QString, QVariant> filterParams, std::unordered_map<QString, QString> filterData, const QStringList consumerArgs, QObject* object)
    : AbstractTask(owner, AbstractTask::FILTERCLIPJOB, object)
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
        // We are on master or track, configure producer accordingly
        if (m_owner.first == ObjectType::Master) {
            producer = pCore->getMasterProducerInstance();
        } else if (m_owner.first == ObjectType::TimelineTrack) {
            producer = pCore->getTrackProducerInstance(m_owner.second);
        }
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
    QTemporaryFile sourceFile(QDir::temp().absoluteFilePath(QStringLiteral("kdenlive-XXXXXX.mlt")));
    if (!sourceFile.open()) {
        // Something went wrong
        return;
    }
    sourceFile.close();
    QTemporaryFile destFile(QDir::temp().absoluteFilePath(QStringLiteral("kdenlive-XXXXXX.mlt")));
    if (!destFile.open()) {
        // Something went wrong
        return;
    }
    destFile.close();
    std::unique_ptr<Mlt::Consumer>consumer(new Mlt::Consumer(profile, "xml", sourceFile.fileName().toUtf8().constData()));
    if (!consumer->is_valid()) {
        m_errorMessage.append(i18n("Cannot create consumer."));
        pCore->taskManager.taskDone(m_owner.second, this);
        return;
    }

    consumer->connect(*producer.get());
    producer->set_speed(0);
    producer->seek(0);
    if (binClip) {
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
        producer->attach(filter);
        filter.set("id", "kdenlive-analysis");
    }

    qDebug()<<"=== FILTER READY TO PROCESS; LENGTH: "<<length;
    consumer->run();
    consumer.reset();
    producer.reset();
    wholeProducer.reset();

    QFile f1(sourceFile.fileName());
    f1.open(QIODevice::ReadOnly);
    QDomDocument dom(sourceFile.fileName());
    dom.setContent(&f1);
    f1.close();

    // add consumer element
    QDomElement consumerNode = dom.createElement("consumer");
    QDomNodeList profiles = dom.elementsByTagName("profile");
    if (profiles.isEmpty())
        dom.documentElement().insertAfter(consumerNode, dom.documentElement());
    else
        dom.documentElement().insertAfter(consumerNode, profiles.at(profiles.length() - 1));
    consumerNode.setAttribute("mlt_service", "xml");
    for (const QString &param : qAsConst(m_consumerArgs)) {
        if (param.contains(QLatin1Char('='))) {
            consumerNode.setAttribute(param.section(QLatin1Char('='), 0, 0), param.section(QLatin1Char('='), 1));
        }
    }
    consumerNode.setAttribute("resource", destFile.fileName());

    f1.open(QIODevice::WriteOnly);
    QTextStream stream(&f1);
    stream << dom.toString();
    f1.close();
    dom.clear();

    // Step 2: process the xml file and save in another .mlt file
    QProcess filterProcess;
    QStringList args({QStringLiteral("progress=1"), sourceFile.fileName()});
    m_jobProcess.reset(new QProcess);
    QObject::connect(this, &AbstractTask::jobCanceled, m_jobProcess.get(), &QProcess::kill, Qt::DirectConnection);
    QObject::connect(m_jobProcess.get(), &QProcess::readyReadStandardError, this, &FilterTask::processLogInfo);
    m_jobProcess->start(KdenliveSettings::rendererpath(), args);
    m_jobProcess->waitForFinished(-1);
    bool result = m_jobProcess->exitStatus() == QProcess::NormalExit;
    m_progress = 100;
    if (auto ptr = m_model.lock()) {
        QMetaObject::invokeMethod(ptr.get(), "setProgress", Q_ARG(int, 100));
    }
    pCore->taskManager.taskDone(m_owner.second, this);
    if (m_isCanceled || !result) {
        return;
    }
    
    paramVector params;
    QString key("results");
    if (m_filterData.find(QStringLiteral("key")) != m_filterData.end()) {
        key = m_filterData.at(QStringLiteral("key"));
    }
    QFile f2(destFile.fileName());
    f2.open(QIODevice::ReadOnly);
    dom.setContent(&f2);
    f2.close();
    QString resultData;
    QDomNodeList filters = dom.elementsByTagName(QLatin1String("filter"));
    for (int i = 0; i < filters.count(); ++i) {
        QDomElement currentParameter = filters.item(i).toElement();
        if (currentParameter.attribute(QLatin1String("id")) == QLatin1String("kdenlive-analysis")) {
            resultData = Xml::getXmlProperty(currentParameter, key);
            break;
        }
    }

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

void FilterTask::processLogInfo()
{
    const QString buffer = QString::fromUtf8(m_jobProcess->readAllStandardError());
    m_logDetails.append(buffer);
    // Parse MLT output
    if (buffer.contains(QLatin1String("percentage:"))) {
        int progress = buffer.section(QStringLiteral("percentage:"), 1).simplified().section(QLatin1Char(' '), 0, 0).toInt();
        if (progress == m_progress) {
            return;
        }
        if (auto ptr = m_model.lock()) {
            m_progress = progress;
            QMetaObject::invokeMethod(ptr.get(), "setProgress", Q_ARG(int, m_progress));
        }
    }
}
