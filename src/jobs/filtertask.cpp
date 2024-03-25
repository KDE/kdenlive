/*
    SPDX-FileCopyrightText: 2011 Jean-Baptiste Mardelle <jb@kdenlive.org>

SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "filtertask.h"
#include "assets/model/assetparametermodel.hpp"
#include "bin/bin.h"
#include "bin/projectclip.h"
#include "bin/projectitemmodel.h"
#include "core.h"
#include "effects/effectstack/model/effectstackmodel.hpp"
#include "kdenlive_debug.h"
#include "kdenlivesettings.h"
#include "macros.hpp"
#include "mainwindow.h"
#include "profiles/profilemodel.hpp"
#include "xml/xml.hpp"

#include <QProcess>
#include <QThread>

#include <KLocalizedString>

FilterTask::FilterTask(const ObjectId &owner, const QString &binId, const std::weak_ptr<AssetParameterModel> &model, const QString &assetId, int in, int out,
                       const QString &filterName, const std::unordered_map<QString, QVariant> &filterParams,
                       const std::unordered_map<QString, QString> &filterData, const QStringList &consumerArgs, QObject *object)
    : AbstractTask(owner, AbstractTask::FILTERCLIPJOB, object)
    , length(0)
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
    m_description = i18n("Processing filter %1", filterName);
}

void FilterTask::start(const ObjectId &owner, const QString &binId, const std::weak_ptr<AssetParameterModel> &model, const QString &assetId, int in, int out,
                       const QString &filterName, const std::unordered_map<QString, QVariant> &filterParams,
                       const std::unordered_map<QString, QString> &filterData, const QStringList &consumerArgs, QObject *object, bool force)
{
    FilterTask *task = new FilterTask(owner, binId, model, assetId, in, out, filterName, filterParams, filterData, consumerArgs, object);
    // Otherwise, start a filter thread.
    task->m_isForce = force;
    pCore->taskManager.startTask(owner.itemId, task);
}

void FilterTask::run()
{
    AbstractTaskDone whenFinished(m_owner.itemId, this);
    if (m_isCanceled || pCore->taskManager.isBlocked()) {
        return;
    }
    QMutexLocker lock(&m_runMutex);
    m_running = true;

    QString url;
    auto binClip = pCore->projectItemModel()->getClipByBinID(m_binId);
    std::unique_ptr<Mlt::Producer> producer = nullptr;
    Mlt::Profile profile(pCore->getCurrentProfilePath().toUtf8().constData());
    if (binClip) {
        // Filter applied on a timeline or bin clip
        url = binClip->url();
        if (url.isEmpty()) {
            QMetaObject::invokeMethod(pCore.get(), "displayBinMessage", Qt::QueuedConnection, Q_ARG(QString, i18n("No producer for this clip.")),
                                      Q_ARG(int, int(KMessageWidget::Warning)));
            return;
        }
        if (KdenliveSettings::gpu_accel()) {
            producer = binClip->getClone();
            if (m_outPoint == -1) {
                m_outPoint = producer->get_length() - 1;
            }
            if (m_inPoint == -1) {
                m_inPoint = 0;
            }
            if (m_inPoint != 0 || m_outPoint != producer->get_length() - 1) {
                producer->set_in_and_out(m_inPoint, m_outPoint);
            }
            Mlt::Filter converter(profile, "avcolor_space");
            producer->attach(converter);
        } else {
            producer = std::make_unique<Mlt::Producer>(profile, url.toUtf8().constData());
            if (!producer || !producer->is_valid()) {
                if (!binClip->isReloading) {
                    QMetaObject::invokeMethod(pCore.get(), "displayBinMessage", Qt::QueuedConnection,
                                              Q_ARG(QString, i18n("Cannot open file %1", binClip->url())), Q_ARG(int, int(KMessageWidget::Warning)));
                } else {
                    QMetaObject::invokeMethod(pCore.get(), "displayBinMessage", Qt::QueuedConnection,
                                              Q_ARG(QString, i18n("Cannot process file %1", binClip->url())), Q_ARG(int, int(KMessageWidget::Warning)));
                }
            }
            if (m_outPoint == -1) {
                m_outPoint = producer->get_length() - 1;
            }
            if (m_inPoint == -1) {
                m_inPoint = 0;
            }
            if (m_inPoint != 0 || m_outPoint != producer->get_length() - 1) {
                producer->set_in_and_out(m_inPoint, m_outPoint);
            }
            // Ensure all user defined properties are passed
            const char *list = ClipController::getPassPropertiesList();
            std::shared_ptr<Mlt::Producer> sourceProducer = binClip->originalProducer();
            Mlt::Properties original(sourceProducer->get_properties());
            Mlt::Properties cloneProps(producer->get_properties());
            cloneProps.pass_list(original, list);
            for (int i = 0; i < sourceProducer->filter_count(); i++) {
                std::shared_ptr<Mlt::Filter> filt(sourceProducer->filter(i));
                if (filt->property_exists("kdenlive_id")) {
                    auto *filter = new Mlt::Filter(*filt.get());
                    producer->attach(*filter);
                }
            }
            if (m_owner.type == KdenliveObjectType::TimelineClip) {
                // Add the timeline clip effects
                std::shared_ptr<EffectStackModel> stack = pCore->getItemEffectStack(pCore->currentTimelineId(), int(m_owner.type), m_owner.itemId);
                stack->passEffects(producer.get(), m_filterName);
            }
        }
        if ((producer == nullptr) || !producer->is_valid()) {
            // Clip was removed or something went wrong
            if (!binClip->isReloading) {
                QMetaObject::invokeMethod(pCore.get(), "displayBinMessage", Qt::QueuedConnection, Q_ARG(QString, i18n("Cannot open file %1", binClip->url())),
                                          Q_ARG(int, int(KMessageWidget::Warning)));
            } else {
                QMetaObject::invokeMethod(pCore.get(), "displayBinMessage", Qt::QueuedConnection,
                                          Q_ARG(QString, i18n("Cannot process file %1", binClip->url())), Q_ARG(int, int(KMessageWidget::Warning)));
            }
            return;
        }
    } else {
        // Filter applied on a track of master producer, leave config to source job
        // We are on master or track, configure producer accordingly
        if (m_owner.type == KdenliveObjectType::Master) {
            producer = pCore->getMasterProducerInstance();
        } else if (m_owner.type == KdenliveObjectType::TimelineTrack) {
            producer = pCore->getTrackProducerInstance(m_owner.itemId);
        }
    }

    if (producer == nullptr || !producer->is_valid()) {
        // Clip was removed or something went wrong, Notify user?
        QMetaObject::invokeMethod(pCore.get(), "displayBinMessage", Qt::QueuedConnection, Q_ARG(QString, i18n("Cannot open source.")),
                                  Q_ARG(int, int(KMessageWidget::Warning)));
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
    QMutexLocker xmlLock(&pCore->xmlMutex);
    std::unique_ptr<Mlt::Consumer> consumer(new Mlt::Consumer(profile, "xml", sourceFile.fileName().toUtf8().constData()));
    if (!consumer->is_valid()) {
        QMetaObject::invokeMethod(pCore.get(), "displayBinMessage", Qt::QueuedConnection, Q_ARG(QString, i18n("Cannot create consumer.")),
                                  Q_ARG(int, int(KMessageWidget::Warning)));
        return;
    }

    consumer->connect(*producer.get());
    producer->set_speed(0);

    if (binClip) {
        // Build filter
        Mlt::Filter filter(profile, m_filterName.toUtf8().data());
        if (!filter.is_valid()) {
            QMetaObject::invokeMethod(pCore.get(), "displayBinMessage", Qt::QueuedConnection, Q_ARG(QString, i18n("Cannot create filter %1", m_filterName)),
                                      Q_ARG(int, int(KMessageWidget::Warning)));
            return;
        }

        // Process filter params
        qDebug() << " = = = = = CONFIGURING FILTER PARAMS = = = = =  ";
        for (const auto &it : m_filterParams) {
            qDebug() << ". . ." << it.first << " = " << it.second;
            if (it.first == QLatin1String("in") || it.first == QLatin1String("out")) {
                continue;
            }
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
            if (it.second.type() == QVariant::Double) {
#else
            if (it.second.typeId() == QMetaType::Double) {
#endif
                filter.set(it.first.toUtf8().constData(), it.second.toDouble());
            } else {
                filter.set(it.first.toUtf8().constData(), it.second.toString().toUtf8().constData());
            }
        }
        if (m_filterData.find(QLatin1String("relativeInOut")) != m_filterData.end()) {
            // leave it operate on full clip
        } else {
            filter.set_in_and_out(m_inPoint, m_outPoint);
        }
        producer->attach(filter);
        filter.set("kdenlive:id", "kdenlive-analysis");
    }

    qDebug() << "=== FILTER READY TO PROCESS; LENGTH: " << length;
    consumer->run();
    consumer.reset();
    producer.reset();
    xmlLock.unlock();
    // wholeProducer.reset();

    QDomDocument dom(sourceFile.fileName());
    Xml::docContentFromFile(dom, sourceFile.fileName(), false);

    // add consumer element
    QDomElement consumerNode = dom.createElement("consumer");
    QDomNodeList profiles = dom.elementsByTagName("profile");
    if (profiles.isEmpty()) {
        dom.documentElement().insertAfter(consumerNode, dom.documentElement());
    } else {
        dom.documentElement().insertAfter(consumerNode, profiles.at(profiles.length() - 1));
    }
    consumerNode.setAttribute("mlt_service", "xml");
    for (const QString &param : qAsConst(m_consumerArgs)) {
        if (param.contains(QLatin1Char('='))) {
            consumerNode.setAttribute(param.section(QLatin1Char('='), 0, 0), param.section(QLatin1Char('='), 1));
        }
    }
    consumerNode.setAttribute("resource", destFile.fileName());
    consumerNode.setAttribute("store", "kdenlive");

    QFile f1(sourceFile.fileName());
    f1.open(QIODevice::WriteOnly);
    QTextStream stream(&f1);
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    stream.setCodec("UTF-8");
#endif
    stream << dom.toString();
    f1.close();
    dom.clear();

    // Step 2: process the xml file and save in another .mlt file
    QStringList args({QStringLiteral("progress=1"), sourceFile.fileName()});
    m_jobProcess.reset(new QProcess);
    QObject::connect(this, &AbstractTask::jobCanceled, m_jobProcess.get(), &QProcess::kill, Qt::DirectConnection);
    QObject::connect(m_jobProcess.get(), &QProcess::readyReadStandardError, this, &FilterTask::processLogInfo);
    m_jobProcess->start(KdenliveSettings::meltpath(), args);
    m_jobProcess->waitForFinished(-1);
    bool result = m_jobProcess->exitStatus() == QProcess::NormalExit;
    m_progress = 100;
    if (auto ptr = m_model.lock()) {
        QMetaObject::invokeMethod(ptr.get(), "setProgress", Q_ARG(int, 100));
    }
    if (m_isCanceled || !result) {
        if (!m_isCanceled) {
            QMetaObject::invokeMethod(pCore.get(), "displayBinLogMessage", Qt::QueuedConnection, Q_ARG(QString, i18n("Failed to filter source.")),
                                      Q_ARG(int, int(KMessageWidget::Warning)), Q_ARG(QString, m_logDetails));
        }
        return;
    }

    paramVector params;
    QString key("results");
    if (m_filterData.find(QStringLiteral("key")) != m_filterData.end()) {
        key = m_filterData.at(QStringLiteral("key"));
    }

    QString resultData;
    if (Xml::docContentFromFile(dom, destFile.fileName(), false)) {
        qDebug() << "AAAA\nGOT DOC\n" << dom.toString();
        QDomNodeList filters = dom.elementsByTagName(QLatin1String("filter"));
        for (int i = 0; i < filters.count(); ++i) {
            QDomElement currentParameter = filters.item(i).toElement();
            if (Xml::getXmlProperty(currentParameter, QLatin1String("mlt_service")) == m_filterName) {
                resultData = Xml::getXmlProperty(currentParameter, key);
            } else if (Xml::getXmlProperty(currentParameter, QLatin1String("kdenlive:id")) == QLatin1String("kdenlive-analysis")) {
                resultData = Xml::getXmlProperty(currentParameter, key);
            }
            if (!resultData.isEmpty()) {
                break;
            }
        }
    }

    if (m_inPoint > 0 && (m_filterData.find(QLatin1String("relativeInOut")) == m_filterData.end())) {
        // Motion tracker keyframes always start at master clip 0, so no need to set in/out points
        params.append({QStringLiteral("in"), m_inPoint});
        params.append({QStringLiteral("out"), m_outPoint});
    }
    params.append({key, QVariant(resultData)});
    if (m_filterData.find(QStringLiteral("storedata")) != m_filterData.end()) {
        // Store a copy of the data in clip analysis
        QString dataName = (m_filterData.find(QStringLiteral("displaydataname")) != m_filterData.end()) ? m_filterData.at(QStringLiteral("displaydataname"))
                                                                                                        : QStringLiteral("data");
        auto binClip = pCore->projectItemModel()->getClipByBinID(m_binId);
        if (binClip) {
            QMetaObject::invokeMethod(binClip.get(), "updatedAnalysisData", Q_ARG(QString, dataName), Q_ARG(QString, resultData), Q_ARG(int, m_inPoint));
        }
        // binClip->updatedAnalysisData(dataName, resultData, m_inPoint);
    }
    if (auto ptr = m_model.lock()) {
        qDebug() << "===== SETTING FILTER PARAM: " << params;
        QMetaObject::invokeMethod(ptr.get(), "setParametersFromTask", Q_ARG(paramVector, std::move(params)));
    }
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
