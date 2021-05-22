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

#ifndef FILTERTASK_H
#define FILTERTASK_H

#include "abstracttask.h"
#include <memory>
#include <unordered_map>
#include <mlt++/MltConsumer.h>

namespace Mlt {
class Profile;
class Producer;
class Consumer;
class Filter;
class Event;
} // namespace Mlt

class AssetParameterModel;
class QProcess;

class FilterTask : public AbstractTask
{
public:
    FilterTask(const ObjectId &owner, const QString &binId, std::weak_ptr<AssetParameterModel> model, const QString &assetId, int in, int out, QString filterName, std::unordered_map<QString, QVariant> filterParams, std::unordered_map<QString, QString> filterData, const QStringList consumerArgs, QObject* object);
    static void start(const ObjectId &owner, const QString &binId, std::weak_ptr<AssetParameterModel> model, const QString &assetId, int in, int out, QString filterName, std::unordered_map<QString, QVariant> filterParams, std::unordered_map<QString, QString> filterData, const QStringList consumerArgs, QObject* object, bool force = false);
    int length;

private slots:
    void processLogInfo();

protected:
    void run() override;

private:
    QString m_binId;
    int m_inPoint;
    int m_outPoint;
    QString m_assetId;
    std::weak_ptr<AssetParameterModel> m_model;
    QString m_filterName;
    std::unordered_map<QString, QVariant> m_filterParams;
    std::unordered_map<QString, QString> m_filterData;
    QStringList m_consumerArgs;
    QString m_errorMessage;
    QString m_logDetails;
    std::unique_ptr<QProcess> m_jobProcess;
};


#endif
