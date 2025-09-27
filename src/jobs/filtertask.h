/*
    SPDX-FileCopyrightText: 2021 Jean-Baptiste Mardelle <jb@kdenlive.org>

SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

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
    FilterTask(const ObjectId &owner, const QString &binId, const std::weak_ptr<AssetParameterModel> &model, const QString &assetId, int in, int out,
               const std::unordered_map<QString, QVariant> &filterParams, const std::unordered_map<QString, QString> &filterData,
               const QStringList &consumerArgs, QObject *object);

    static void start(const ObjectId &owner, const QString &binId, const std::weak_ptr<AssetParameterModel> &model, const QString &assetId, int in, int out,
                      const std::unordered_map<QString, QVariant> &filterParams, const std::unordered_map<QString, QString> &filterData,
                      const QStringList &consumerArgs, QObject *object, bool force = false);
    int length;

private Q_SLOTS:
    void processLogInfo();

protected:
    void run() override;

private:
    QString m_binId;
    int m_inPoint;
    int m_outPoint;
    QString m_assetId;
    bool m_onPlaylist{false};
    int m_length{0};
    std::weak_ptr<AssetParameterModel> m_model;
    std::unordered_map<QString, QVariant> m_filterParams;
    std::unordered_map<QString, QString> m_filterData;
    QStringList m_consumerArgs;
    QString m_errorMessage;
    QString m_logDetails;
    QProcess *m_jobProcess;
};
