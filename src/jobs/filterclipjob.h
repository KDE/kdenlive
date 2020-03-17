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

#ifndef JOBS_FILTERCLIPJOB
#define JOBS_FILTERCLIPJOB

#include "meltjob.h"
#include "assets/model/assetparametermodel.hpp"

#include <unordered_map>
#include <unordered_set>


class FilterClipJob : public MeltJob
{
    Q_OBJECT

public:
    FilterClipJob(const QString &binId, const ObjectId &owner, std::weak_ptr<AssetParameterModel> model, const QString &assetId, int in, int out, const QString &filterName, std::unordered_map<QString, QVariant> filterParams, std::unordered_map<QString, QString> filterData, const QStringList consumerArgs);
    const QString getDescription() const override;
    /** @brief This is to be called after the job finished.
    By design, the job should store the result of the computation but not share it with the rest of the code. This happens when we call commitResult */
    bool commitResult(Fun &undo, Fun &redo) override;

protected:
    // @brief create and configure consumer
    void configureConsumer() override;

    // @brief create and configure producer
    void configureProducer() override;

    // @brief create and configure filter
    void configureFilter() override;

protected:
    std::weak_ptr<AssetParameterModel> m_model;
    QString m_filterName;
    int m_timelineClipId;
    QString m_assetId;
    std::unordered_map<QString, QVariant> m_filterParams;
    std::unordered_map<QString, QString> m_filterData;
    QStringList m_consumerArgs;
    ObjectId m_owner;
};

#endif
