/*

    SPDX-FileCopyrightText: 2021 Jean-Baptiste Mardelle (jb@kdenlive.org)

SPDX-License-Identifier: LicenseRef-KDE-Accepted-GPL
*/

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
