/***************************************************************************
 *                                                                         *
 *   Copyright (C) 2019 by Jean-Baptiste Mardelle (jb@kdenlive.org)        *
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

#include "filterclipjob.h"
#include "assets/model/assetparametermodel.hpp"
#include "bin/projectclip.h"
#include "bin/projectitemmodel.h"
#include "profiles/profilemodel.hpp"
#include "core.h"
#include "kdenlivesettings.h"
#include "macros.hpp"

#include <QThread>

#include <klocalizedstring.h>

FilterClipJob::FilterClipJob(const QString &binId, const ObjectId &owner, std::weak_ptr<AssetParameterModel> model, const QString &assetId, int in, int out, const QString &filterName, std::unordered_map<QString, QVariant> filterParams, std::unordered_map<QString, QString> filterData, const QStringList consumerArgs)
    : MeltJob(binId, FILTERCLIPJOB, false, in, out)
    , m_model(model)
    , m_filterName(filterName)
    , m_assetId(assetId)
    , m_filterParams(std::move(filterParams))
    , m_filterData(std::move(filterData))
    , m_consumerArgs(consumerArgs)
    , m_owner(owner)
{
    m_timelineClipId = -1;
    if (owner.first == ObjectType::TimelineClip) {
        m_timelineClipId = owner.second;
    }
}

void FilterClipJob::configureProducer()
{
    if (m_producer != nullptr) {
        // producer already configured, abort
        return;
    }
    // We are on master or track, configure producer accordingly
    if (m_owner.first == ObjectType::Master) {
        m_profile.reset(&pCore->getCurrentProfile()->profile());
        m_producer = pCore->getMasterProducerInstance();
    } else if (m_owner.first == ObjectType::TimelineTrack) {
        m_profile.reset(&pCore->getCurrentProfile()->profile());
        m_producer = pCore->getTrackProducerInstance(m_owner.second);
    }
    length = m_producer->get_playtime();
    if (length == 0) {
        length = m_producer->get_length();
    }
}

const QString FilterClipJob::getDescription() const
{
    //TODO: add better description after string freeze
    return i18n("Apply Filter on Clip");
}

void FilterClipJob::configureConsumer()
{
    m_consumer = std::make_unique<Mlt::Consumer>(*m_profile.get(), "null");
    for (const QString &param : qAsConst(m_consumerArgs)) {
        if (param.contains(QLatin1Char('='))) {
            m_consumer->set(param.section(QLatin1Char('='), 0, 0).toUtf8().constData(), param.section(QLatin1Char('='), 1).toInt());
        }
    }
}

void FilterClipJob::configureFilter()
{

    m_filter = std::make_unique<Mlt::Filter>(*m_profile.get(), m_filterName.toUtf8().data());
    if ((m_filter == nullptr) || !m_filter->is_valid()) {
        m_errorMessage.append(i18n("Cannot create filter %1", m_filterName));
        return;
    }

    // Process filter params
    qDebug()<<" = = = = = CONFIGURING FILTER PARAMS = = = = =  ";
    for (const auto &it : m_filterParams) {
        qDebug()<<". . ."<<it.first<<" = "<<it.second;
        if (it.second.type() == QVariant::Double) {
            m_filter->set(it.first.toUtf8().constData(), it.second.toDouble());
        } else {
            m_filter->set(it.first.toUtf8().constData(), it.second.toString().toUtf8().constData());
        }
    }
    if (m_filterData.find(QLatin1String("relativeInOut")) != m_filterData.end()) {
        // leave it operate on full clip
    } else {
        m_filter->set_in_and_out(m_producer->get_in(), m_producer->get_out());
    }
}

bool FilterClipJob::commitResult(Fun &undo, Fun &redo)
{
    Q_ASSERT(!m_resultConsumed);
    if (!m_done) {
        qDebug() << "ERROR: Trying to consume invalid results";
        return false;
    }
    m_resultConsumed = true;
    if (!m_successful) {
        m_producer->detach(*m_filter.get());
        m_filter.reset();
        m_producer.reset();
        m_wholeProducer.reset();
        return false;
    }
    QVector<QPair<QString, QVariant>> params;
    QString key("results");
    if (m_filterData.find(QStringLiteral("key")) != m_filterData.end()) {
        key = m_filterData.at(QStringLiteral("key"));
    }
    QString resultData = qstrdup(m_filter->get(key.toUtf8().constData()));
    params.append({key,QVariant(resultData)});
    qDebug()<<"= = = GOT FILTER RESULTS: "<<params;
    if (m_filterData.find(QStringLiteral("storedata")) != m_filterData.end()) {
        // Store a copy of the data in clip analysis
        QString dataName = (m_filterData.find(QStringLiteral("displaydataname")) != m_filterData.end()) ? m_filterData.at(QStringLiteral("displaydataname")) : QStringLiteral("data");
        auto binClip = pCore->projectItemModel()->getClipByBinID(m_clipId);
        binClip->updatedAnalysisData(dataName, resultData, m_in);
    }
    auto operation = [assetModel = m_model, filterParams = std::move(params)]() {
        if (auto ptr = assetModel.lock()) {
            qDebug()<<"===== SETTING FILTER PARAM: "<<filterParams;
            ptr->setParameters(filterParams);
        }
        pCore->setDocumentModified();
        return true;
    };
    auto reverse = [assetModel = m_model, keyName = key]() {
        QVector<QPair<QString, QVariant>> fParams;
        fParams.append({keyName,QVariant()});
        if (auto ptr = assetModel.lock()) {
            ptr->setParameters(fParams);
        }
        pCore->setDocumentModified();
        return true;
    };
    bool ok = operation();
    m_producer->detach(*m_filter.get());
    m_filter.reset();
    m_producer.reset();
    m_wholeProducer.reset();
    if (ok) {
        UPDATE_UNDO_REDO_NOLOCK(operation, reverse, undo, redo);
    }
    return true;
}
